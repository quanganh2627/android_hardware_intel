#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cutils/log.h>
#include <getopt.h>
#include <cutils/properties.h>

#include "update_osip.h"
#include "util.h"
#include "fw_version_check.h"
#include "flash_ifwi.h"
#include "fpt.h"
#include "txemanuf.h"

#ifdef BOARD_HAVE_MODEM
#include "telephony/flashtool.h"
#endif

#define PROP_BUILD_ID	"ro.build.display.id"

enum {
	CMD_NONE,
	CMD_DUMP_FW_VER,
	CMD_WRITE_OSIP,
	CMD_READ_OSIP,
	CMD_DUMP,
	CMD_WRITE_FW,
	CMD_WRITE_3G_FW,
	CMD_WRITE_FPT_IFWI
};

static void usage(void)
{
	printf("\nusage: flashtool <options>\n"
		"Available commands:\n"
		"-i <entry name>       OSIP entry to manipulate, used with -w and -r\n"
		"   Valid entry names are " ANDROID_OS_NAME " " RECOVERY_OS_NAME
		" " FASTBOOT_OS_NAME "\n"
		"-w <osupdate file>    Flash a file to the index, requires -i\n"
		"-r <destination file> Read an osimage index into a file, requires -i\n"
		"-v <firmware file>    Dump the FW versions in an IFWI release\n"
		"-f <firmware file>    Flash an IFWI image\n"
		"-g <firmware file>    Flash 3G firmware\n"
		"-p <firmware file>    Flash IFWI using FPT\n"
		"Run with no arguments to print out some system version information\n"
		"and a dump of the OSIP\n"
		);
}

static void cmd_show_data(void)
{
	struct OSIP_header osip;
	struct firmware_versions cur_fw_ver;
	char build_id[PROPERTY_VALUE_MAX];

	if (read_OSIP(&osip)) {
		fprintf(stderr, "OSIP read failure!\n");
	} else {
		dump_osip_header(&osip);
	}
	if (get_current_fw_rev(&cur_fw_ver)) {
		fprintf(stderr, "FW read failure!\n");
	} else {
		printf("\nCurrent FW versions (NOTE chaabi values are invalid):\n");
		dump_fw_versions(&cur_fw_ver);
	}
	if (property_get(PROP_BUILD_ID, build_id, NULL) < 0) {
		fprintf(stderr, "Android build id read failure!\n");
	} else {
		printf("\nCurrent Android build version:\n");
		printf("%s\n", build_id);
	}
	printf("\nCurrent kernel version:\n");
	system("cat /proc/version");
}

static void cmd_dump_fw(char *filename)
{
	void *data;
	size_t size;
	struct firmware_versions fv;
	if (!filename) {
		fprintf(stderr, "Must specifiy filename!\n");
		exit(1);
	}
	if (file_read(filename, &data, &size)) {
		fprintf(stderr, "failed to read input file!\n");
		exit(1);
	}
	get_image_fw_rev(data, size, &fv);
	free(data);
	printf("Image %s:\n", filename);
	dump_fw_versions(&fv);
}

static void cmd_write_fw(char *filename)
{
	void *data;
	size_t size;
	if (!filename) {
		fprintf(stderr, "Must specifiy filename!\n");
		exit(1);
	}
	if (file_read(filename, &data, &size)) {
		fprintf(stderr, "failed to read input file!\n");
		exit(1);
	}
	if (update_ifwi_image(data, size, 0)) {
		fprintf(stderr, "IFWI update failed!\n");
		exit(1);
	}
	free(data);
}

static void cmd_read_osip(char *entry, char *filename)
{
	void *data;
	size_t size;
	int index;
	if (!entry || !filename) {
		fprintf(stderr, "Must specifiy image index and filename!\n");
		exit(1);
	}
	index = get_named_osii_index(entry);
	if (index < 0) {
		fprintf(stderr, "%s is an invalid entry name\n", entry);
		exit(1);
	}
	if (read_osimage_data(&data, &size, index)) {
		fprintf(stderr, "Failed to read OSIP entry\n");
		exit(1);
	}
	if (file_write(filename, data, size)) {
		fprintf(stderr, "Can't write to output file %s\n", filename);
		exit(1);
	}
}

static void cmd_write_osip(char *entry, char *filename)
{
	int index;
	size_t size;
	void *data;

	if (!entry || !filename) {
		fprintf(stderr, "Must specifiy image entry and filename!\n");
		exit(1);
	}
	index = get_named_osii_index(entry);
	if (index < 0) {
		fprintf(stderr, "%s is an invalid entry name\n", entry);
		exit(1);
	}

	printf("flashing '%s' to '%s' osip[%d]\n", filename, entry, index);
	if (file_read(filename, &data, &size)) {
		fprintf(stderr, "failed to read input file!\n");
		exit(1);
	}
	if (write_stitch_image(data, size, index)) {
		fprintf(stderr, "error writing image\n");
		exit(1);
	}
}

int main(int argc, char ** argv)
{
	int c;
	char *entry = NULL;
	char *filename = NULL;
	int cmd = CMD_NONE;

	while ( (c = getopt(argc, argv, "i:w:r:dv:hf:g:p:") ) != -1 ) {
		switch (c) {
		case 'i':
			entry = strdup(optarg);
			break;
		case 'w':
			filename = strdup(optarg);
			cmd = CMD_WRITE_OSIP;
			break;
		case 'r':
			filename = strdup(optarg);
			cmd = CMD_READ_OSIP;
			break;
		case 'v':
			cmd = CMD_DUMP_FW_VER;
			filename = strdup(optarg);
			break;
		case 'f':
			cmd = CMD_WRITE_FW;
			filename = strdup(optarg);
			break;
		case 'g':
			cmd = CMD_WRITE_3G_FW;
			filename = strdup(optarg);
			break;
		case 'p':
			cmd = CMD_WRITE_FPT_IFWI;
			filename = strdup(optarg);
			break;
		case 'h':
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}

	switch (cmd) {
	case CMD_NONE:
		cmd_show_data();
		break;
	case CMD_DUMP_FW_VER:
		cmd_dump_fw(filename);
		break;
	case CMD_READ_OSIP:
		cmd_read_osip(entry, filename);
		break;
	case CMD_WRITE_OSIP:
		cmd_write_osip(entry, filename);
		break;
	case CMD_WRITE_FW:
		cmd_write_fw(filename);
		break;
#ifdef BOARD_HAVE_MODEM
	case CMD_WRITE_3G_FW:
		cmd_flash_modem_fw(filename);
		break;
#endif
	case CMD_WRITE_FPT_IFWI:
		flash_fpt_file_ifwi(filename);
		break;
	}
	return 0;
}
