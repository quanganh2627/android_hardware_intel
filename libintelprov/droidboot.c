/*
 * Copyright 2011 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <droidboot.h>
#include <droidboot_plugin.h>
#include <droidboot_util.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <cutils/properties.h>
#include <cutils/android_reboot.h>
#include <unistd.h>
#include <charger/charger.h>
#include <linux/ioctl.h>
#include <sys/mount.h>

#include "volumeutils/ufdisk.h"
#include "update_osip.h"
#include "util.h"
#include "fw_version_check.h"
#include "flash_ifwi.h"
#include "flash_image.h"
#include "fpt.h"
#include "txemanuf.h"
#include "fastboot.h"
#include "droidboot_ui.h"
#include "gpt/partlink/partlink.h"
#include "oem_partition.h"

#ifdef TEE_FRAMEWORK
#include "tee_connector.h"
#endif	/* TEE_FRAMEWORK */

#ifndef EXTERNAL
#include "pmdb.h"
#include "token.h"
#endif

#ifdef BOARD_HAVE_MODEM
#include "telephony/droidboot.h"
#endif

static int oem_write_osip_header(int argc, char **argv);

static int flash_testos(void *data, unsigned sz)
{
	oem_write_osip_header(0,0);
	return write_stitch_image_ex(data, sz, 0, 1);
}

#ifdef MRFLD

static int flash_dnx(void *data, unsigned sz)
{
    return 0;
}

static int flash_ifwi(void *data, unsigned size)
{
	int ret;

	ret = check_ifwi_file(data, size);
	if (ret > 0)
		return update_ifwi_file(data, size);

	return ret;
}

static int flash_token_umip(void *data, unsigned size)
{
	return write_token_umip(data, size);
}

#else

#define BIN_DNX  "/tmp/__dnx.bin"
#define BIN_IFWI "/tmp/__ifwi.bin"

static int flash_dnx(void *data, unsigned sz)
{
	if (file_write(BIN_DNX, data, sz)) {
		pr_error("Couldn't write dnx file to %s\n", BIN_DNX);
		return -1;
	}

	return 0;
}

static int flash_ifwi(void *data, unsigned sz)
{
	struct firmware_versions img_fw_rev;


	if (access(BIN_DNX, F_OK)) {
		pr_error("dnx binary must be flashed to board first\n");
		return -1;
	}

	if (get_image_fw_rev(data, sz, &img_fw_rev)) {
		pr_error("Coudn't extract FW version data from image");
		return -1;
	}

	printf("Image FW versions:\n");
	dump_fw_versions(&img_fw_rev);

	if (file_write(BIN_IFWI, data, sz)) {
		pr_error("Couldn't write ifwi file to %s\n", BIN_IFWI);
		return -1;
	}

	if (update_ifwi_file(BIN_DNX, BIN_IFWI)) {
		pr_error("IFWI flashing failed!");
		return -1;
	}
	return 0;
}

#endif

#define DNX_TIMEOUT_CHANGE  "dnx_timeout"
#define DNX_TIMEOUT_GET	    "--get"
#define DNX_TIMEOUT_SET	    "--set"
#define SYS_CURRENT_TIMEOUT "/sys/devices/platform/intel_mid_umip/current_timeout"
#define TIMEOUT_SIZE	    20
#define OPTION_SIZE	    6

static int oem_dnx_timeout(int argc, char **argv)
{
	int retval = -1;
	int count, offset, bytes, size;
	int fd;
	char option[OPTION_SIZE] = "";
	char timeout[TIMEOUT_SIZE] = "";
	char check[TIMEOUT_SIZE] = "";

	if (argc < 1 || argc > 3) {
		/* Should not pass here ! */
		fastboot_fail("oem dnx_timeout requires one or two arguments");
		goto end2;
	}

	size = snprintf(option, OPTION_SIZE, "%s", argv[1]);

	if (size == -1 || size > OPTION_SIZE-1) {
	    fastboot_fail("Parameter size exceeds limit");
	    goto end2;
	}

	fd = open(SYS_CURRENT_TIMEOUT, O_RDWR);

	if (fd == -1) {
		pr_error("Can't open %s\n", SYS_CURRENT_TIMEOUT);
		goto end2;
	}

	if (!strcmp(option, DNX_TIMEOUT_GET)) {
		/* Get current timeout */
		count = read(fd, check, TIMEOUT_SIZE);

		if (count <= 0) {
			fastboot_fail("Failed to read");
			goto end1;
		}

		fastboot_info(check);

	} else { if (!strcmp(option, DNX_TIMEOUT_SET)) {
		    /* Set new timeout */

		    if (argc != 3) {
			/* Should not pass here ! */
			fastboot_fail("oem dnx_timeout --set not enough arguments");
			goto end1;
		    }

		    // Get timeout value to set
		    size = snprintf(timeout, TIMEOUT_SIZE, "%s", argv[2]);

		    if (size == -1 || size > TIMEOUT_SIZE-1) {
			fastboot_fail("Timeout value size exceeds limit");
			goto end1;
		    }

		    bytes = write(fd, timeout, size);
		    if (bytes != size) {
			fastboot_fail("oem dnx_timeout failed to write file");
			goto end1;
		    }

		    offset = lseek(fd, 0, SEEK_SET);
		    if (offset == -1) {
			fastboot_fail("oem dnx_timeout failed to set offset");
			goto end1;
		    }

		    memset(check, 0, TIMEOUT_SIZE);

		    count = read(fd, check, TIMEOUT_SIZE);
		    if (count <= 0) {
			fastboot_fail("Failed to check");
		    	goto end1;
		    }

		    // terminate string unconditionally to avoid buffer overflow
		    check[TIMEOUT_SIZE-1] = '\0';
		    if (check[strlen(check)-1] == '\n')
			check[strlen(check)-1]= '\0';
		    if (strcmp(check, timeout)) {
			fastboot_fail("oem dnx_timeout called with wrong parameter");
			goto end1;
		    }
		} else {
		    fastboot_fail("Unknown command. Use fastboot oem dnx_timeout [--get/--set] command\n");
		    goto end1;
		}
	}

	retval = 0;
	fastboot_okay("");

end1:
	close(fd);
end2:
	return retval;
}

#define K_MAX_LINE_LEN 8192
#define K_MAX_ARGS 256
#define K_MAX_ARG_LEN 256

static int oem_write_osip_header(int argc, char **argv)
{
	static struct OSIP_header default_osip = {
		.sig = OSIP_SIG,
		.intel_reserved = 0,
		.header_rev_minor = 0,
		.header_rev_major = 1,
		.header_checksum = 0,
		.num_pointers = 1,
		.num_images = 1,
		.header_size = 0
	};

	ui_print("Write OSIP header\n");
	default_osip.header_checksum = get_osip_crc(&default_osip);
	write_OSIP(&default_osip);
	restore_osii("boot");
	restore_osii("recovery");
	restore_osii("fastboot");
	return 0;
}

#ifndef EXTERNAL
static int wait_property(char *prop, char *value, int timeout_sec)
{
	int i;
	char v[PROPERTY_VALUE_MAX];

	for(i = 0; i < timeout_sec; i++) {
		property_get(prop, v, NULL);
		if(!strcmp(v, value))
			return 0;
		sleep(1);
	}
	return -1;
}

static int oem_backup_factory(int argc, char **argv)
{
	int len;
	char value[PROPERTY_VALUE_MAX];

	len = property_get("sys.backup_factory", value, NULL);
	if (strcmp(value, "done") && len) {
		fastboot_fail("Factory partition backing up failed!\n");
		return -1;
	}

	property_set("sys.backup_factory", "backup");
	ui_print("Backing up factory partition...\n");
	if(wait_property("sys.backup_factory", "done", 60)) {
		fastboot_fail("Factory partition backing up timeout!\n");
		return -1;
	}

	return 0;
}

static int oem_restore_factory(int argc, char **argv)
{
	char value[PROPERTY_VALUE_MAX];

	property_get("sys.backup_factory", value, NULL);
	if (strcmp(value, "done")) {
		fastboot_fail("Factory partition restoration failed!\n");
		return -1;
	}

	property_set("sys.backup_factory", "restore");
	ui_print("Restoring factory partition...\n");
	if(wait_property("sys.backup_factory", "done", 60)) {
		fastboot_fail("Factory partition restore timeout!\n");
		return -1;
	}

	return 0;
}
#endif	/* EXTERNAL */

static int oem_get_batt_info_handler(int argc, char **argv)
{
	char msg_buf[] = " level: 000";
	int batt_level = 0;

	batt_level = get_battery_level();
	if (batt_level == -1) {
		fastboot_fail("Could not get battery level");
		return -1;
	}
	// Prepare the message sent to the host
	snprintf(msg_buf, sizeof(msg_buf), "\nlevel: %d", batt_level);
	// Push the value to the host
	fastboot_info(msg_buf);
	// Display the result on the UI
	ui_print("Battery level at %d%%\n", batt_level);

	return 0;
}

#ifndef EXTERNAL
static int oem_fru_handler(int argc, char **argv)
{
	int ret = -1;
	char *str;
	char tmp[3];
	char fru[PMDB_FRU_SIZE];
	int i;

	if (argc != 3) {
		fastboot_fail("oem fru must be called with \"set\" subcommand\n");
		goto out;
	}

	if (strcmp(argv[1], "set")) {
		fastboot_fail("unknown oem fru subcommand\n");
		goto out;
	}

	str = argv[2];
	if (strlen(str) != (PMDB_FRU_SIZE * 2)) {
		fastboot_fail("fru value must be 20 4-bits nibbles in hexa format. Ex: 123456...\n");
		goto out;
	}

	tmp[2] = 0;
	for (i = 0; i < PMDB_FRU_SIZE; i++) {
		/* FRU is passed by 4bits nibbles. Need to reorder them into hex values. */
		tmp[0] = str[2*i+1];
		tmp[1] = str[2*i];
		if (!is_hex(tmp[0]) || ! is_hex(tmp[1]))
			fastboot_fail("fru value have non hexadecimal characters\n");
		sscanf(tmp, "%2hhx", &fru[i]);
	}
	ret = pmdb_write_fru(fru, PMDB_FRU_SIZE);

out:
	return ret;
}

static int oem_fastboot2adb(int argc, char **argv)
{
	char value[PROPERTY_VALUE_MAX];
	int len = 0;
	int ret = -1;

	len = property_get("ro.debuggable", value, NULL);
	if ((len != 0) && (strcmp(value, "1") == 0)) {
		fastboot_okay("");
		ret = property_set("sys.adb.config", "adb");
	} else {
		fastboot_fail("property ro.debuggable must be set to activate adb.");
	}
	return ret;
}
#endif	/* EXTERNAL */

static int oem_reboot(int argc, char **argv)
{
	char *target_os;

	switch (argc) {
	case 1:
		target_os = "android";
		break;
	case 2:
		target_os = argv[1];
		break;
	default:
		LOGE("reboot command take zero on one argument only\n");
		fastboot_fail("Usage: reboot [target_os]");
		return -EINVAL;
	}

	fastboot_okay("");
	sync();

	ui_print("REBOOT in %s...\n", target_os);
	pr_info("Rebooting in %s !\n", target_os);
	return android_reboot(ANDROID_RB_RESTART2, 0, target_os);
}

#ifdef USE_GUI
#define PROP_FILE					"/default.prop"
#define SERIAL_NUM_FILE			"/sys/class/android_usb/android0/iSerial"
#define PRODUCT_NAME_ATTR		"ro.product.name"
#define MAX_NAME_SIZE			128
#define BUF_SIZE					256

static char* strupr(char *str)
{
	char *p = str;
	while (*p != '\0') {
		*p = toupper(*p);
		p++;
	}
	return str;
}

static int read_from_file(char* file, char *attr, char *value)
{
	char *p;
	char buf[BUF_SIZE];
	FILE *f;

	if ((f = fopen(file, "r")) == NULL) {
		LOGE("open %s error!\n", file);
		return -1;
	}
	while(fgets(buf, BUF_SIZE, f)) {
		if ((p = strstr(buf, attr)) != NULL) {
			p += strlen(attr)+1;
			strncpy(value, p, MAX_NAME_SIZE);
			value[MAX_NAME_SIZE-1] = '\0';
			strupr(value);
			break;
		}
	}

	fclose(f);
	return 0;
}

static int get_system_info(int type, char *info, unsigned sz)
{
	int ret = -1;
	char pro_name[MAX_NAME_SIZE];
	FILE *f;
	char value[PROPERTY_VALUE_MAX];

	switch (type) {
		case IFWI_VERSION:
			property_get("sys.ifwi.version", value, "");
			snprintf(info, sz, "%s", value);
			ret = 0;
			break;
		case PRODUCT_NAME:
			if ((ret = read_from_file(PROP_FILE, PRODUCT_NAME_ATTR, pro_name)) < 0)
				break;
			snprintf(info, sz, "%s", pro_name);
			ret = 0;
			break;
		case SERIAL_NUM:
			if ((f = fopen(SERIAL_NUM_FILE, "r")) == NULL)
				break;
			if (fgets(info, sz, f) == NULL) {
				fclose(f);
				break;
			}
			fclose(f);
			ret = 0;
			break;
		default:
			break;
	}

	return ret;
}
#endif

static void cmd_intel_reboot(const char *arg, void *data, unsigned sz)
{
	fastboot_okay("");
	// This will cause a property trigger in init.rc to cold boot
	property_set("sys.forcecoldboot", "yes");
	sync();
	ui_print("REBOOT...\n");
	pr_info("Rebooting!\n");
	android_reboot(ANDROID_RB_RESTART2, 0, "android");
	pr_error("Reboot failed");
}

static void cmd_intel_reboot_bootloader(const char *arg, void *data, unsigned sz)
{
	fastboot_okay("");
	// No cold boot as it would not allow to reboot in bootloader
	sync();
	ui_print("REBOOT in BOOTLOADER...\n");
	pr_info("Rebooting in BOOTLOADER !\n");
	android_reboot(ANDROID_RB_RESTART2, 0, "bootloader");
	pr_error("Reboot failed");
}

static void cmd_intel_boot(const char *arg, void *data, unsigned sz)
{
	ui_print("boot command stubbed on this platform!\n");
	pr_info("boot command stubbed on this platform!\n");
	fastboot_okay("");
}

struct property_format {
	const char *name;
	const char *msg;
	const char *error_msg;
};

static struct property_format properties_format[] = {
	{ "sys.ifwi.version",     "         ifwi:", NULL },
	{ NULL,                   "---- components ----" , NULL },
	{ "sys.scu.version",      "          scu:", NULL },
	{ "sys.punit.version",    "        punit:", NULL },
	{ "sys.valhooks.version", "    hooks/oem:", NULL },
	{ "sys.ia32.version",     "         ia32:", NULL },
	{ "sys.suppia32.version", "     suppia32:", NULL },
	{ "sys.mia.version",      "          mIA:", NULL },
	{ "sys.chaabi.version",   "       chaabi:", "CHAABI versions unreadable at runtime" },
};

static void dump_system_versions()
{
	char property[PROPERTY_VALUE_MAX];
	unsigned int i;

	for (i = 0 ; i < sizeof(properties_format)/ sizeof((properties_format)[0]) ; i++)
	{
		struct property_format *fmt = &properties_format[i];

		if (!fmt->name) {
			printf("%s\n", fmt->msg);
			continue;
		}

		property_get(fmt->name, property, "");
		if (strcmp(property, ""))
			printf("%s %s\n", fmt->msg, property);
		else if (fmt->error_msg)
			printf("%s\n", fmt->error_msg);
	}
}

void libintel_droidboot_init(void)
{
	int ret = 0;
	struct ufdisk ufdisk = {
		.umount_all = ufdisk_umount_all,
		.create_partition = ufdisk_create_partition
	};

	oem_partition_init(&ufdisk);
	util_init(fastboot_fail, fastboot_info);

	ret |= aboot_register_flash_cmd(TEST_OS_NAME, flash_testos);
	ret |= aboot_register_flash_cmd(ANDROID_OS_NAME, flash_android_kernel);
	ret |= aboot_register_flash_cmd(RECOVERY_OS_NAME, flash_recovery_kernel);
	ret |= aboot_register_flash_cmd(FASTBOOT_OS_NAME, flash_fastboot_kernel);
	ret |= aboot_register_flash_cmd(ESP_PART_NAME, flash_esp);
	ret |= aboot_register_flash_cmd("splashscreen", flash_splashscreen_image);
	ret |= aboot_register_flash_cmd("dnx", flash_dnx);
	ret |= aboot_register_flash_cmd("ifwi", flash_ifwi);
#ifdef MRFLD
	ret |= aboot_register_flash_cmd("token_umip", flash_token_umip);
#endif
	ret |= aboot_register_flash_cmd("capsule", flash_capsule);
	ret |= aboot_register_flash_cmd("ulpmc", flash_ulpmc);

	ret |= aboot_register_oem_cmd(DNX_TIMEOUT_CHANGE, oem_dnx_timeout);
	ret |= aboot_register_oem_cmd("erase", oem_erase_partition);
	ret |= aboot_register_oem_cmd("repart", oem_repart_partition);

	ret |= aboot_register_oem_cmd("write_osip_header", oem_write_osip_header);
	ret |= aboot_register_oem_cmd("start_partitioning", oem_partition_start_handler);
	ret |= aboot_register_oem_cmd("partition", oem_partition_cmd_handler);
	ret |= aboot_register_oem_cmd("retrieve_partitions", oem_retrieve_partitions);
	ret |= aboot_register_oem_cmd("stop_partitioning", oem_partition_stop_handler);
	ret |= aboot_register_oem_cmd("get_batt_info", oem_get_batt_info_handler);
	ret |= aboot_register_oem_cmd("reboot", oem_reboot);
	ret |= aboot_register_oem_cmd("wipe", oem_wipe_partition);

#ifdef TEE_FRAMEWORK
	print_fun = fastboot_info;
	error_fun = fastboot_fail;

	ret |= aboot_register_oem_cmd("get-spid", get_spid);
	ret |= aboot_register_oem_cmd("get-fru", get_fru);
	ret |= aboot_register_oem_cmd("get-part-id", get_part_id);
	ret |= aboot_register_oem_cmd("get-lifetime", get_lifetime);
	ret |= aboot_register_oem_cmd("start-update", start_update);
	ret |= aboot_register_oem_cmd("cancel-update", cancel_update);
	ret |= aboot_register_oem_cmd("finalize-update", finalize_update);
	ret |= aboot_register_oem_cmd("remove-token", remove_token);
	ret |= aboot_register_flash_cmd("token", write_token);
#endif

#ifndef EXTERNAL
	char build_type_prop[PROPERTY_VALUE_MAX] = { '\0', };
	char platform_prop[PROPERTY_VALUE_MAX] = { '\0', };
	ret |= aboot_register_oem_cmd("fru", oem_fru_handler);
	ret |= libintel_droidboot_token_init();

	ret |= aboot_register_oem_cmd("backup_factory", oem_backup_factory);
	ret |= aboot_register_oem_cmd("restore_factory", oem_restore_factory);
	ret |= aboot_register_oem_cmd("fastboot2adb", oem_fastboot2adb);

	if (property_get("ro.board.platform", platform_prop, '\0') &&
	    property_get("ro.build.type", build_type_prop, '\0')) {
		if ((strcmp(platform_prop, "baytrail") == 0) &&
		    (strcmp(build_type_prop, "eng") == 0)) {
			aboot_register_flash_cmd("fpt_ifwi", flash_fpt_data_ifwi);
			aboot_register_flash_cmd("fpt_txe", flash_fpt_data_txe);
			aboot_register_flash_cmd("fpt_pdr", flash_fpt_data_pdr);
			aboot_register_flash_cmd("fpt_bios", flash_fpt_data_bios);
			aboot_register_flash_cmd("fpt_fpfs", flash_fpt_data_fpfs);
			aboot_register_flash_cmd("txemanuf", flash_txemanuf_data);

			aboot_register_oem_cmd("fpt_writeitem", fpt_writeitem);
			aboot_register_oem_cmd("fpt_writevalidbit", fpt_writevalidbit);
			aboot_register_oem_cmd("fpt_closemnf", fpt_closemnf);
			aboot_register_oem_cmd("txemanuf_eof_test", txemanuf_eof_test);
			aboot_register_oem_cmd("txemanuf_bist_test", txemanuf_bist_test);
		}
	}
#endif

#ifdef BOARD_HAVE_MODEM
	ret |= aboot_register_telephony_functions();
#endif

	fastboot_register("continue", cmd_intel_reboot);
	fastboot_register("reboot", cmd_intel_reboot);
	fastboot_register("reboot-bootloader", cmd_intel_reboot_bootloader);
	fastboot_register("boot", cmd_intel_boot);

#ifdef USE_GUI
	ret |= aboot_register_ui_cmd(UI_GET_SYSTEM_INFO, get_system_info);
#endif

	if (ret)
		die();

	if (full_gpt()) {
		struct OSIP_header osip;
		/* Dump the OSIP to serial to assist debugging */
		if (read_OSIP(&osip)) {
			printf("OSIP read failure!\n");
		} else {
			dump_osip_header(&osip);
		}
	}

	dump_system_versions();
}
