/*****************************************************************************
INTEL CONFIDENTIAL
Copyright 2014 Intel Corporation All Rights Reserved.

The source code contained or described herein and all documents related to the
source code ("Material") are owned by Intel Corporation or its suppliers or
licensors. Title to the Material remains with Intel Corporation or its
suppliers and licensors. The Material may contain trade secrets and
proprietary and confidential information of Intel Corporation and its
suppliers and licensors, and is protected by worldwide copyright and trade
secret laws and treaty provisions. No part of the Material may be used, copied,
reproduced, modified, published, uploaded, posted, transmitted, distributed,
or disclosed in any way without Intel's prior express written permission.

No license under any patent, copyright, trade secret or other intellectual
property right is granted to or conferred upon you by disclosure or delivery of
the Materials, either expressly, by implication, inducement, estoppel or
otherwise. Any license under such intellectual property rights must be express
and approved by Intel in writing

File:fg_algo_iface.c

Description:Fuel Gauge Algorithm interface file

Author: Srinidhi Rao <srinidhi.rao@intel.com>

Date Created: 5 JUNE 2014

*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <poll.h>

#include "fg_algo_iface.h"

#include <cutils/log.h>
#include <cutils/klog.h>

#include <sys/wait.h>
#include <cutils/properties.h>
#include <sys/inotify.h>
#include <sys/epoll.h>

#define DEBUG_TAG "fg_algo_iface"
#define LOG_TAG "EM FG ALGO Iface:"

#define FG_ALGO_KLOG_LEVEL 3
#define KLOGE(x...) do { KLOG_ERROR(DEBUG_TAG, x); } while (0)
#define KLOGI(x...) do { KLOG_INFO(DEBUG_TAG, x); } while (0)
#define KLOGD(x...) do { KLOG_DEBUG(DEBUG_TAG, x); } while (0)

#define FG_SYSPATH "/sys/class/misc/intel_fg_iface"
#define FG_SUBSYS "intel_fg_iface"
#define FG_UEVENT_PATH "/sys/class/misc/intel_fg_iface/uevent"

#define MAX_FG_ATTR 13
#define MAX_PATH_LEN 256

/*PMIC Vendor suggested V_aDJUST Factor*/
#define VADJUST	150

/* Primary XML and Secondary binary config files related defnitions */
#define DC_TI_PRIMARY_FILE "/system/etc/fg_config_ti_swfg.bin"
#define DC_TI_CONFIG_SIZE        312
#define DC_TI_SEC_CONFIG_SIZE    314

#define SECONDARY_FILE "/config/em/battid.dat"
#define POWER_SUPPLY_PATH "/sys/class/power_supply/"
#define MAX_COMMAND_LINE_BUF    1024

#define MODEL_NAME_SIZE 8

/* PATH to get the shutdown notification */
#define INOTIFY_PATH        "/sys/devices/virtual/misc/watchdog"

/* Shutdown notify thread function */
static void *fg_iface_inotify_thread(void);
static bool kill_thread;

/* Battery Config XML related data types */
enum ALGO_TYPE_T {
	INTEL_ALGO = 0,
	TI_ALGO
} ALGO_TYPE;

static struct config_file_params_t {
	char primary_file_name[64];
	char secondary_file_name[64];
	int primary_file_name_len;
	int sec_file_name_len;
	int primary_file_size;
	int sec_file_size;
	int prim_config_data_size;
	int sec_config_data_size;
	char *sec_buf;
	int sec_buf_size;
	enum ALGO_TYPE_T algo_typ;
} cfg_file_params;

/* data types to read FG driver sysfs values */
struct fg_iface_attr_type {
	char *name;
	int id;
	int val;
};

enum {
	VOLT_NOW = 0,
	VOLT_OCV,
	VOLT_BOOT,
	IBAT_BOOT,
	CUR_NOW,
	CUR_AVG,
	BATT_TEMP,
	DELTA_Q,
	CAPACITY,
	NAC,
	FCC,
	CYC_CNT,
	CC_CALIB
} fg_attr;

static struct fg_iface_attr_type fg_att[MAX_FG_ATTR] = {
	{.name = "volt_now",	.id = VOLT_NOW,	.val = 0},
	{.name = "volt_ocv",	.id = VOLT_OCV,	.val = 0},
	{.name = "volt_boot",	.id = VOLT_BOOT, .val = 0},
	{.name = "ibat_boot",	.id = IBAT_BOOT, .val = 0},
	{.name = "cur_now",		.id = CUR_NOW,	.val = 0},
	{.name = "cur_avg",		.id = CUR_AVG,	.val = 0},
	{.name = "batt_temp",	.id = BATT_TEMP, .val = 0},
	{.name = "delta_q",		.id = DELTA_Q,	.val = 0},
	{.name = "capacity",	.id = CAPACITY,	.val = 0},
	{.name = "nac",			.id = NAC,		.val = 0},
	{.name = "fcc",			.id = FCC,		.val = 0},
	{.name = "cyc_cnt",		.id = CYC_CNT,	.val = 0},
	{.name = "cc_calib",	.id = CC_CALIB,	.val = 0},
};

/*
 * read_sysfs_attr: Function to read an attribute from the given syspath
 *	@syspath: path in the sysfs of the attr which has to be read.
 *	@attr: Name of the attribute within the syspath to be read.
 *	@val: Vlaue of the attribute in integer format which has to
 *			returned to the caller.
 *	@Returns: 0 for success, negetive error code on failure.
 */
static int read_sysfs_attr(const char *syspath, const char *attr, int *val)
{
	int ret = 0, fd = 0, count = 0;
	char full_path[PATH_MAX] = {0}, read_buf[8] = {0};

	if (!syspath || !attr) {
		SLOGE("\n Null path or att passed");
		return ret = -EINVAL;
	}

	snprintf(full_path, sizeof(full_path), "%s/%s", syspath, attr);
	fd = open(full_path, O_RDONLY);
	if (fd < 0) {
		SLOGE("\n Failed to open the file at %s", full_path);
		return ret = -EPERM;
	}
	count = read(fd, &read_buf[0], 8);
	*val = atoi(&read_buf[0]);
	close(fd);

	return ret;
}

/*
 * write_sysfs_attr: Function to write an attribute from the given syspath
 *  @syspath: path in the sysfs of the attr which has to be read.
 *  @attr: Name of the attribute within the syspath to be written.
 *  @val: Vlaue of the attribute in integer format which has to
 *          to be written from the caller.
 *  @Returns: 0 for success, negetive error code on failure.
 */
static int write_sysfs_attr(const char *syspath, const char *attr, int val)
{
	int ret = 0, fd = 0, count = 0;
	char full_path[PATH_MAX] = {0}, write_buf[8] = {0};

	if (!syspath || !attr) {
		SLOGE("\n Null path or att passed");
		return ret = -EINVAL;
	}

	snprintf(full_path, sizeof(full_path), "%s/%s", syspath, attr);
	fd = open(full_path, O_WRONLY);
	if (fd < 0) {
		SLOGE("\n Failed to open the file at %s", full_path);
		return ret = -EPERM;
	}
	snprintf(&write_buf[0], 8, "%d", val);
	count = write(fd, &write_buf[0], 8);
	close(fd);

	return ret;
}

/*
 * update_algo_input_params: Function to update input params.
 *  @Returns: Void.
 */
static void update_algo_input_params(void)
{
	fg_in_params.vbatt = fg_att[VOLT_NOW].val;
	fg_in_params.vboot = fg_att[VOLT_BOOT].val;
	fg_in_params.vocv = fg_att[VOLT_OCV].val;
	fg_in_params.ibatt = fg_att[CUR_NOW].val;
	fg_in_params.ibat_boot = fg_att[IBAT_BOOT].val;
	fg_in_params.iavg = fg_att[CUR_AVG].val;
	fg_in_params.bat_temp = fg_att[BATT_TEMP].val;
	fg_in_params.delta_q = fg_att[DELTA_Q].val;
}

/*
 * read_bootup_data: Function to read bootup prams from FG Driver.
 *  @v_bootup: pointer to store the bootup VOCV from the FG drvier
 *	sysfs interface.
 *  @i_bootup: pointer to store the bootup IBAT from the FG drvier's
 *	sysfs interface
 *  @Returns: 0 for success, negetive error code on failure.
 */
static int read_bootup_data(int *v_bootup, int *i_bootup)
{
	int ret = 0, fd = 0;
	char *attr_v_boot = NULL, *att_i_boot = NULL;

	ret = read_sysfs_attr(FG_SYSPATH, fg_att[VOLT_BOOT].name, v_bootup);
	if (ret < 0) {
		SLOGE("\nError in reading attr %s, ret = %d",
				fg_att[VOLT_BOOT].name, ret);
		return ret;
	}

	ret = read_sysfs_attr(FG_SYSPATH, fg_att[IBAT_BOOT].name, i_bootup);
	if (ret < 0) {
		SLOGE("\nError in reading attr %s, ret = %d",
				fg_att[IBAT_BOOT].name, ret);
		return ret;
	}

	return ret;
}

/*
 * populate_fg_input_params: Function to populate FG params from FG Driver.
 *	No input arguments.
 *	This function reads all the sttribute of Fule Gauge interface driver
 *	and populates them into a table which can be passed to the libfg algo
 *  @Returns: 0 for success, negetive error code on failure.
 */
static int populate_fg_input_params(void)
{
	int ret = 0, i = 0;

	for (i = VOLT_NOW; i <= DELTA_Q; i++) {
		ret = read_sysfs_attr(FG_SYSPATH, fg_att[i].name, &fg_att[i].val);
		SLOGD("\n\t %s = %d", fg_att[i].name, fg_att[i].val);
	}

	return ret;
}

/*
 * write_fg_output_params: Function to write libfg output to FG Driver.
 *	No input arguments.
 *  This function writes libfg algorithm output like SoC etc to the
 *	to the Fule Gauge interface driver.
 *  @Returns: 0 for success, negetive error code on failure.
 */
static int write_fg_output_params(void)
{
	int ret = 0, i = 0;

	/*Copy the Algo Output and write it to the sysfs entries*/
	fg_att[CAPACITY].val = fg_out_params.capacity;
	fg_att[NAC].val = fg_out_params.nac;
	fg_att[FCC].val = fg_out_params.fcc;
	fg_att[CYC_CNT].val = fg_out_params.cyc_cnt;
	fg_att[CC_CALIB].val = fg_out_params.cc_calib;

	for (i = CAPACITY; i <= CC_CALIB; i++) {
		ret = write_sysfs_attr(FG_SYSPATH, fg_att[i].name, fg_att[i].val);
		if (ret < 0)
			SLOGE("\nError in writing att %s", fg_att[i].name);
	}

	return ret;
}

/*
 * get_battery_ps_name: Function to get the power supply name of FG driver.
 * input @ps_battery_path: Pointer to the string containing the name of
 * Fuel Gauge path in power supply sysfs.
 * This function reads the sysfs entries exported by power supply class
 * driver in the kernel and reads the Fuel Gauge path.
 * @Returns: 0 for success, negetive error code on failure.
 */
int get_battery_ps_name(char *ps_battery_path)
{
	struct dirent *dir_entry;
	char path[PATH_MAX];
	char buf[20];
	int length;
	int fd;

	DIR *dir = opendir(POWER_SUPPLY_PATH);
	if (dir == NULL)
		return errno;

	while ((dir_entry = readdir(dir))) {
		const char *ps_name = dir_entry->d_name;

		if (ps_name[0] == '.' && (ps_name[1] == 0 || (ps_name[1] == '.' && ps_name[2] == 0)))
			continue;
		snprintf(path, sizeof(path), "%s/%s/type", POWER_SUPPLY_PATH, ps_name);
		fd = open(path, O_RDONLY);

		if (fd < 0) {
			closedir(dir);
			return errno;
		}

		length = read(fd, buf, sizeof(buf));
		close(fd);
		if (length <= 0)
			continue;

		if (buf[length - 1] == '\n')
			buf[length - 1] = 0;
		if (strcmp(buf, "Battery") == 0) {
			snprintf(ps_battery_path, PATH_MAX, "%s", ps_name);
			/* In BYT-CRV-2.2, the Fuel Gauge driver has been
			 * registered with power_supply_class under the name
			 * of intel_fuel_gauge. Though it might mislead, this
			 * name corresponds to TI FG support and for intel FG Algo,
			 * the power supply class has to be registered under different
			 * name.
			 */
			if (strstr(ps_name, "intel_fuel") != NULL) {
				strncpy(&cfg_file_params.primary_file_name[0],
				DC_TI_PRIMARY_FILE, strlen(DC_TI_PRIMARY_FILE));
				strncpy(&cfg_file_params.secondary_file_name[0],
				SECONDARY_FILE, strlen(SECONDARY_FILE));
				cfg_file_params.primary_file_name_len = strlen(DC_TI_PRIMARY_FILE);
				cfg_file_params.sec_file_name_len = strlen(SECONDARY_FILE);
				cfg_file_params.primary_file_name[
				cfg_file_params.primary_file_name_len] = '\0';
				cfg_file_params.secondary_file_name[
				cfg_file_params.sec_file_name_len] = '\0';

				cfg_file_params.prim_config_data_size = DC_TI_CONFIG_SIZE;
				cfg_file_params.sec_config_data_size = DC_TI_SEC_CONFIG_SIZE;
				cfg_file_params.sec_buf = (char *)malloc(DC_TI_SEC_CONFIG_SIZE);
				if (cfg_file_params.sec_buf == NULL) {
					SLOGE("Insufficient memory %s, %d\n", __func__, __LINE__);
					return -ENOMEM;
				}
				cfg_file_params.algo_typ = TI_ALGO;
			} else if (strstr(ps_name, "algo_intel") != NULL) {
				cfg_file_params.algo_typ = INTEL_ALGO;
				/* TODO: Rest of the intel algo related assingments */
			}
			closedir(dir);
			return 0;
		}

		SLOGE("Power Supply type=%.8s name=%s\n", buf, ps_name);
	}
	closedir(dir);
	return -ENOENT;
}

/*
 * get_battid: Function to get the battery ID from power supply class.
 * input @battid: Pointer to the string containing battery ID (8 charecters)
 * This function reads the battery ID from the sysfs entries exported by
 * power supply class driver in the kernel.
 * @Returns: 0 for success, negetive error code on failure.
 */
int get_battid(unsigned char *battid)
{
	int ret;
	int fd;
	char ps_batt_name[PATH_MAX];
	char battid_path[PATH_MAX];

	ret =  get_battery_ps_name(ps_batt_name);
	if (ret) {
		SLOGE("Error(%d) in get_battery_ps_name:%s\n", ret, strerror(ret));
		return ret;
	}

	snprintf(battid_path, sizeof(battid_path), "%s%s/model_name", POWER_SUPPLY_PATH, ps_batt_name);

	SLOGE("Reading battid from %s\n", battid_path);
	fd = open(battid_path, O_RDONLY);

	if (fd < 0)
		return errno;

	ret = read(fd, battid, MODEL_NAME_SIZE);
	battid[MODEL_NAME_SIZE] = '\0';
	if (ret < 0) {
		close(fd);
		return errno;
	}
	SLOGE("\n battid from power supply class = %s\n", battid);
	close(fd);
	return 0;
}

/*
 * checksum: Function to calculate the checksum from saved data.
 * @buf: Pointer to the array containg battery config data.
 * @len: Leght of the array containg battery config data.
 * @Returns: calculated checksum.
 */
unsigned short checksum(void *buf, int len)
{
	short int chksum = 0;
	unsigned char *data = (unsigned char *)buf;
	while (len--)
		chksum += *data++;
	return chksum;
}
/*
 * checksum_primary: Function to calculate the checksum from primary XML.
 * @buf: Pointer to the array containg battery config data.
 * @len: Leght of the array containg battery config data.
 * @tmp: Pointer to return the checksum stored in Primary XML binary file.
 * @Returns: checksum retrived from Primary XML file.
 */
unsigned short checksum_primary(void *buf, int len, unsigned int *tmp)
{
	short int chksum = 0;
	unsigned char *data = (unsigned char *)buf;
	unsigned char fl_array[len+1];
	int fd, ret, i;

	memset(&fl_array[0], 0, len);

	fd = open((const char *)&cfg_file_params.primary_file_name[0], O_RDONLY);
	if (fd < 0)
		return errno;

	ret = read(fd, &fl_array, cfg_file_params.primary_file_size);
	if (ret != len) {
		SLOGE("requested bytes:%d read bytes:%d\n", len, ret);
		close(fd);
		return -ENODATA;
	}
	*tmp = *(unsigned int *)&fl_array[0];
	for (i = 4; i < len; i++)
		chksum += fl_array[i];

	close(fd);

	return chksum;
}

/*
 * read_primary_file: Function to read the binary formt of primary XML.
 * @file_name: String containg full path and name of the primary file.
 * @buf: Pointer to the array where battery config data has to be stored.
 * @battid: Pointer to the string containg the battery ID from
 *          power supply class
 * @size: Size of the config data that has to be read.
 * @Returns: Zero on success, negetive value on failure.
 */
int read_primary_file(char *file_name, unsigned char *buf,
				unsigned char *battid, int size)
{
	int fd, ret, total_size, read_size = 0;
	struct stat st;
	int itr = 0, try_count = 6;
	char batt_string[9] = {0};

	fd = open((const char *)file_name, O_RDONLY);

	if (fd < 0)
		return errno;

	lseek(fd, 4, SEEK_SET);
	SLOGD("Batt ID = %s\n", battid);

	do {
		SLOGE("\n ITERATION = %d\n", itr);
		itr++;
		ret = read(fd, buf, size);
		if (ret != size) {
			SLOGE("requested bytes:%d read bytes:%d\n", size, ret);
			try_count--;
			if (try_count == 1) {
				close(fd);
				return -ENODATA;
			}
		}
		strncpy(&batt_string[0], buf, 8);
		batt_string[8] = '\0';
		read_size += ret;
		SLOGD("\n Batt String = %s\n", (const char *)&batt_string[0]);
	} while (!strstr((const char *)&batt_string[0], (char *)battid) &&
		read_size <= cfg_file_params.primary_file_size);

	close(fd);
	if (read_size > cfg_file_params.primary_file_size) {
		SLOGE("\n Batt ID not found read_size = %d\n", read_size);
		return -ENODATA;
	}
	return 0;
}

/*
 * read_secondary_file: Function to read the secondary binary file.
 * @file_name: String containg full path and name of the secondary file.
 * @buf: Pointer to the array where battery config data has to be stored.
 * @battid: Pointer to the string containg the battery ID from
 *          power supply class
 * @size: Size of the config data that has to be read.
 * @Returns: Zero on success, negetive value on failure.
 */
int read_secondary_file(char *file_name, unsigned char *buf,
				unsigned char *battid, int size)
{
	int fd, ret;
	char batt_string[9] = {0};

	fd = open((const char *)file_name, O_RDONLY);
	if (fd < 0) {
		SLOGE("Failed to open file %s err = %d\n", file_name, errno);
		return errno;
	}
	ret = read(fd, buf, size);
	if (ret != size) {
		SLOGE("file couldn't be read,read_bytes %d size %d\n", ret, size);
		close(fd);
		return -ENODATA;
	}
	close(fd);
	strncpy(&batt_string[0], buf, 8);
	batt_string[8] = '\0';
	SLOGD("Batt string from file = %s\n",(const char *)&batt_string[0]);
	if (!strstr((const char *)&batt_string[0], (const char *)battid)) {
		SLOGE("Batt ID does not match %s\n", battid);
		return -ENODATA;
	}

	return 0;
}

/*
 * write_secondary_file: Function to write the secondary binary file.
 * @file_name: String containg full path and name of the secondary file.
 * @buf: Pointer to the array where battery config data has to be stored.
 * @size: Size of the config data that has to be read.
 * @validity_flag: whether to reset validity flag or not.
 * @Returns: Zero on success, negetive value on failure.
 * This FUnction routine is not dependent on any FG ALGO and can
 * be reused to support any Fuel Gauge Algorithm library.
 */
int write_secondary_file(unsigned char *file_name, unsigned char *buf,
		bool validity_flag)
{
	int fds, ret, cksum;
	unsigned char battid[9];

	fds = open(SECONDARY_FILE, O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
	if (fds < 0) {
		SLOGE("Error(%d) in opening %s:%s\n", errno, SECONDARY_FILE, strerror(errno));
		return errno;
	}
	ret = fg_algo_get_runtime_data(buf, validity_flag);
	if (ret < 0) {
		SLOGE("Failed to get save data from the Algo lib\n");
		return -EAGAIN;
	}
	cksum = checksum(buf, cfg_file_params.sec_config_data_size - 2);
	SLOGD("Write_sec cksum = %d\n", cksum);

	ret = write(fds, buf, cfg_file_params.sec_config_data_size - 2);
	if (ret != cfg_file_params.sec_config_data_size - 2) {
		SLOGE("Error in writing secondary file\n");
		close(fds);
		return -ENODATA;
	}
	lseek(fds, cfg_file_params.sec_config_data_size - 2, SEEK_SET);
	write(fds, &cksum, 2);
	close(fds);
	SLOGD("Wrote %d bytes to %s\n", ret, file_name);
	return 0;
}

/*
 * get_fg_config_table: Function get the batt config table.
 * @parse_cfg: pointer to the config table typecasted to void.
 * @type: Pointer to config_data_type and info pertaining to it.
 * @Returns: Zero on success, negetive value on failure.
 * This FUnction routine is not dependent on any FG ALGO and can
 * be reused to support any Fuel Gauge Algorithm library.
 */
int get_fg_config_table(void *parse_cfg, struct config_file_params_t *type)
{
	unsigned char *pbuf;
	unsigned char *pri_buf;
	unsigned char *sec_buf;
	int is_pcksum_ok = 0;
	int is_scksum_ok = 0;
	int ret;
	int in_cos;
	bool use_sec_file = 0;
	unsigned int pcksum = 0, scksum = 0, tmp = 0;
	unsigned char battid[9] = {0};
	struct stat st;

	ret = get_battid(&battid[0]);
	if (ret) {
		SLOGE("Error(%d) in get_battid:%s\n", ret, strerror(ret));
		return ret;
	}

	stat(&cfg_file_params.primary_file_name[0], &st);
	cfg_file_params.primary_file_size = st.st_size;

	sec_buf = (unsigned char *)malloc(cfg_file_params.sec_config_data_size);
	if (sec_buf == NULL) {
		SLOGE("%s:%d:insufficient memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	pri_buf = (unsigned char *)malloc(
				cfg_file_params.prim_config_data_size);
	if (pri_buf == NULL) {
		SLOGE("%s:%d:insufficient memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	if (read_secondary_file(&cfg_file_params.secondary_file_name[0],
				sec_buf, &battid[0], cfg_file_params.sec_config_data_size))
		SLOGE("Secondary file could not be read");
	else {
		scksum = checksum(sec_buf,
				cfg_file_params.sec_config_data_size - 2);
		tmp = *(unsigned short *)&sec_buf[
				cfg_file_params.sec_config_data_size - 2];
		SLOGD("\nsksum = %d and sec_sum_bin = %d\n", scksum, tmp);
		if (scksum != tmp)
			SLOGE("Secondary checksum mismatch");
		else
			use_sec_file = 1;
	}
	if (read_primary_file(&cfg_file_params.primary_file_name[0],
			pri_buf, &battid[0], cfg_file_params.prim_config_data_size)) {
		SLOGD("Primary file could not be read");
		return -EINVAL;
	} else {
		pcksum = checksum_primary(NULL, cfg_file_params.primary_file_size,
				&tmp);
		SLOGD("\n CHeck-Sum = %d, tmp = %d\n ", pcksum, tmp);
		if (pcksum != tmp) {
			SLOGD("Primary checksum mismatch %x and %x", tmp, pcksum);
			if (!use_sec_file) {
				SLOGE("Cannot get config from either file");
				return -EINVAL;
			}
		}
	}
	if (use_sec_file && scksum
		== *(unsigned short *)&sec_buf[
			cfg_file_params.sec_config_data_size - 2]) {
		SLOGE("Using data from secondary file");
		if (cfg_file_params.algo_typ == TI_ALGO)
			return update_fg_conf(sec_buf, parse_cfg);
		else
			/* TODO: Implement support for Intel algo lib */
			return 0;
	} else {
		SLOGD("Using data from primary file");
		if (cfg_file_params.algo_typ == TI_ALGO)
			return update_fg_conf(pri_buf, parse_cfg);
		else
			/* TODO: Implement support for Intel algo lib */
			return 0;
	}

}
/*
 * main: Entry point of the executable.
 * fg_algo_iface user space execution starts here.
 * Since this needs to run for the entire duration,
 * It will not return.
 */
int main(int argc, char *argv[])
{
	int ret = 0, i = 0;

	int fd = 0, cnt = 0;
	char read_buf[8] = {0};
	struct pollfd fg_poll;

	char *args[2];
	int status;
	void *batt_cfg = NULL;
	pthread_t inotify_thread;

	/*Initialize Klog*/
	klog_init();
	klog_set_level(FG_ALGO_KLOG_LEVEL);

	/*Get a file descriptor for the monitoring FG sysfs entry.
	If the file descriptor is not present then the kernel does
	not support software fuel gauge. Exit the program*/
	fd = open(FG_UEVENT_PATH, O_RDWR);
	if (fd < 0) {
		SLOGE("\nFailed to get File Descriptor");
		exit(1);
	}

	/* Spawn the shutdown event thread */
	if (pthread_create(&inotify_thread, NULL,
		fg_iface_inotify_thread, NULL) < 0) {
		SLOGE("Failed to create the shutdown event thread\n");
		exit(1);
	}

	/*Dummy Read call to clear pending events*/
	cnt = read(fd, &read_buf[0], 8);

	/* Populate all the FG params before init */
	ret =  populate_fg_input_params();
	if (ret < 0) {
		SLOGE("\nFailed to populate fg_input params");
		exit(1);
	}
	update_algo_input_params();

	ret = get_fg_config_table(batt_cfg, &cfg_file_params);
	if (ret < 0) {
		SLOGE("Failed to get the batt config table %d\n", ret);
		exit(1);
	}
	ret = fg_algo_init(batt_cfg, &fg_in_params, &fg_out_params);
	if (ret >= 0) {
		SLOGE("\n\t fg_init, vboot = %d, iboot = %d, soc = %d",
		fg_in_params.vboot, fg_in_params.ibat_boot, fg_out_params.capacity);
	}
	/* Reset the validity flag in secondary file after FG init */
	write_secondary_file(&cfg_file_params.secondary_file_name[0],
		&cfg_file_params.sec_buf, false);

	/*
	 * Wait for the event from the FG kernel driver.
	 * Poll for the intel_fg_uiface driver to send
	 * sysfs_notify to user space, after receiving
	 * the notification, pass the parameters to
	 * libfg. poll is a blocking call.
	*/
	while (1) {
		int timeout_ms = 60000, ret = 0;

		/*Dummy read to clear pending events*/
		cnt = read(fd, &read_buf[0], 8);

		fg_poll.fd = fd;
		fg_poll.events = POLLPRI;
		fg_poll.revents = 0;


		/*Invoke poll. This will block till FG Iface driver sends
			sysfs_notify*/
		ret = poll(&fg_poll, 1, timeout_ms);
		if (ret < 0) {
			SLOGE("\nFailed to poll %d", ret);
			KLOGE("\nFailed to poll %d", ret);
		} else if (ret == 0) {
			SLOGE("\nTimeout occured during poll");
			KLOGE("\nTimeout occured during poll");
		} else if ((fg_poll.revents & POLLPRI)) {

			/*If Poll Event, get all FG values and Invoke functions to process
			fg*/
			ret = populate_fg_input_params();
			if (ret < 0) {
				SLOGE("\nFailed to populate fg_input params");
				exit(1);
			}
			SLOGD("\n\tVBATT = %d, VOCV = %d, IBAT = %d, DQ = %d\n",
			fg_att[VOLT_NOW].val, fg_att[VOLT_OCV].val, fg_att[CUR_NOW].val, fg_att[DELTA_Q].val);
			/* Update the input parameters to be passes to fg_algo_run */
			update_algo_input_params();
			/*Invoke PMIC Vendor provided libfg process API*/
			ret = fg_algo_run(batt_cfg, &fg_in_params, &fg_out_params);
			/* copy the Algo Output and write it to the sysfs entries */
			write_fg_output_params();

			SLOGD("\n\t Capacity = %d, nac = %d, fcc = %d\n",
				fg_att[CAPACITY].val, fg_att[NAC].val, fg_att[FCC].val);

			KLOGE("\n\t Capacity = %d, nac = %d, fcc = %d\n",
				fg_att[CAPACITY].val, fg_att[NAC].val, fg_att[FCC].val);

		} else {
			SLOGE("\n Unknow revent for poll");
			KLOGE("\n Unknow revent for poll");
			sleep(2);
		}
	} /*end of while*/

	/*Should not come here*/
	if (fd)
		close(fd);
	SLOGE("\n Fule Gauge User Space Interface Exiting\n");
	exit(1);
}

/* i-Notify event related function routines */


/*
 * fg_iface_process_inotify_evt: Function to process shut down event.
 * @fd: File descriptor pointing to watchdog timer which indicates whether
 * Shutdown procedure has been started by android or not.
 * @Returns: Zero on success, negetive value on failure.
 * This FUnction routine is not dependent on any FG ALGO and can
 * be reused to support any Fuel Gauge Algorithm library.
 */
static int fg_iface_process_inotify_evt(int fd)
{
#define BUF_LEN        1024
#define INOTIFY_SIZE   (sizeof(struct inotify_event))
#define PROP_SHUTDOWN  "sys.shutdown.requested"

	char buffer[BUF_LEN];
	int length, i = 0;
	int prop_len;
	char prop_shutdown[128];

	length = read(fd, buffer, BUF_LEN);

	if (length < 0) {
		KLOG_ERROR(LOG_TAG, "error reading inotify %d", length);
		return -1;
	}

	while (i < length) {
		struct inotify_event *event = (struct inotify_event *) &buffer[i];
		if (event->len) {
			if (event->mask & IN_MODIFY) {
				prop_len = property_get(PROP_SHUTDOWN, prop_shutdown, "NONE");
				if (strcmp(prop_shutdown, "NONE")) {
					/* shutdown is triggered */
					SLOGE("Shutdown event received\n");
					write_secondary_file(
					&cfg_file_params.secondary_file_name[0],
					&cfg_file_params.sec_buf, true);
					kill_thread = true;
				}
			}
		}
		i += INOTIFY_SIZE + event->len;
	}
	return 0;
}

/*
 * fg_iface_process_inotify_thread: Thread Function to poll for shutdown evt.
 * @Returns: nil.
 * This FUnction routine is not dependent on any FG ALGO and can
 * be reused to support any Fuel Gauge Algorithm library.
 */
static void *fg_iface_inotify_thread(void)
{
	struct epoll_event evt, nevts[1];
	int i, inotify_fd, inotify_wd, epoll_fd;

	epoll_fd = epoll_create(1);
	if (epoll_fd < 0) {
		SLOGE("error epoll %d", epoll_fd);
		return NULL;
	}

	inotify_fd = inotify_init();
	if (inotify_fd < 0) {
		SLOGE("error inotify %d", inotify_fd);
		close(epoll_fd);
		return NULL;
	}

	inotify_wd = inotify_add_watch(inotify_fd, INOTIFY_PATH, IN_MODIFY);

	evt.events = EPOLLIN;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inotify_fd, &evt);

	do {
		epoll_wait(epoll_fd, nevts, 1, -1);
		for (i = 0; i < 1; i++) {
			fg_iface_process_inotify_evt(inotify_fd);
			if (kill_thread)
				break;
		}
	} while (1);

	inotify_rm_watch(inotify_fd, inotify_wd);
	close(inotify_fd);
	close(epoll_fd);
	return NULL;
}
