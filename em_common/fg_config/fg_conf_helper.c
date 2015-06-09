#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <cutils/log.h>

typedef enum {FALSE = 0, TRUE} bool;

/* FIXME: Find a generic way to support multiple config structures */
static bool is_xpwr = 0;
#define XPWR_PRIMARY_FILE "/system/etc/fg_config_xpwr.bin"
#define XPWR_CONFIG_SIZE	38
#define XPWR_SEC_CONFIG_SIZE	40
#define INTEL_FG_DEV_FILE	"/dev/intel_fg"

#define PRIMARY_FILE "/system/etc/fg_config.bin"
#define SECONDARY_FILE "/config/em/battid.dat"
#define DEV_FILE "/dev/fg"
#define POWER_SUPPLY_PATH "/sys/class/power_supply/"
#define MAX_COMMAND_LINE_BUF	1024

#define TRUE 1
#define FALSE 0

struct __attribute__((__packed__))primary_header {
		unsigned short  rev;
		unsigned short file_size;
		unsigned short cksum;
		unsigned short res;
};

#define MAX_TABLE_NAME_SIZE 8
#define MAX_BATTID_SIZE 8
#define MAX_FG_CONFIG_SIZE 138
#define MODEL_NAME_SIZE 2
#define MAX_SERIAL_NUM_SIZE 12

struct __attribute__((__packed__)) table_body {
	unsigned char table_type;
	unsigned short size;
	unsigned short rev;
	char table_name[MAX_TABLE_NAME_SIZE];
	char battid[MAX_BATTID_SIZE];
	unsigned char config_init;
	unsigned char fg_config[MAX_FG_CONFIG_SIZE];
	unsigned short cksum;
};

struct __attribute__((__packed__)) sec_file_body {
	struct table_body tbl;
	unsigned short pcksum;
	char serial_num[MAX_SERIAL_NUM_SIZE + 1];
};

#define MAX_FG_TABLES	15
#define MAX_PRIM_FILE_SIZE (sizeof(struct primary_header) +\
	MAX_FG_TABLES * sizeof(struct table_body))


#define LOG_TAG "EM FG User Space:"

int read_primary_header(struct primary_header *pheader)
{
	int ret;
	int fd = open(PRIMARY_FILE, O_RDONLY);
	if (fd < 0)
		return errno;
	ret = read(fd, pheader, sizeof(*pheader));
	if (ret !=  sizeof(*pheader)) {
		close(fd);
		return ENODATA;
	}
	close(fd);

	if (pheader->file_size > MAX_PRIM_FILE_SIZE)
		return EINVAL;
	return 0;
}


int read_primary_file(unsigned char *buf, int size)
{
	int fd;
	int ret;
	fd = open(PRIMARY_FILE, O_RDONLY);
	if (fd < 0)
		return errno;

	ret = read(fd, buf, size);
	if (ret != size) {
		ALOGI("requested bytes:%d read bytes:%d\n", size, ret);
		close(fd);
		return ENODATA;
	}
	close(fd);
	return 0;
}

int read_xpwr_primary_file(unsigned char *buf, int size)
{
	int fd, ret;
	fd = open(XPWR_PRIMARY_FILE, O_RDONLY);
	if (fd < 0)
		return errno;

	ret = read(fd, buf, size);
	if (ret != size) {
		ALOGI("requested bytes:%d read bytes:%d\n", size, ret);
		close(fd);
		return ENODATA;
	}
	close(fd);
	return 0;
}

int read_secondary_file(struct sec_file_body *sbuf)
{
	int fd;
	int ret;
	fd = open(SECONDARY_FILE, O_RDONLY);
	if (fd < 0)
		return errno;
	ret = read(fd, sbuf, sizeof(*sbuf));
	if (ret !=  sizeof(*sbuf)) {
		close(fd);
		return ENODATA;
	}
	close(fd);
	return 0;
}

int read_xpwr_secondary_file(unsigned char *buf)
{
	int fd, ret;
	fd = open(SECONDARY_FILE, O_RDONLY);
	if (fd < 0)
		return errno;
	ret = read(fd, buf, XPWR_SEC_CONFIG_SIZE);
	if (ret != XPWR_SEC_CONFIG_SIZE) {
		close(fd);
		return ENODATA;
	}
	close(fd);
	return 0;
}

unsigned short checksum(void *buf, int len)
{
	short int chksum = 0;
	unsigned char *data = (unsigned char *)buf;
	while (len--)
		chksum += *data++;
	return chksum;
}

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
		if (length > 0) {
			if (buf[length - 1] == '\n')
				buf[length - 1] = 0;
			if (strcmp(buf, "Battery") == 0) {
				snprintf(ps_battery_path, PATH_MAX, "%s", ps_name);
				if (strstr(ps_name, "dollar") != NULL)
					is_xpwr = 1;
				closedir(dir);
				return 0;
			}

			ALOGI("Power Supply type=%.8s name=%s\n", buf, ps_name);
		}
	}
	closedir(dir);
	return ENOENT;
}

int get_battid(char *battid)
{
	int ret;
	int fd;
	char ps_batt_name[PATH_MAX];
	char battid_path[PATH_MAX];
	ret =  get_battery_ps_name(ps_batt_name);
	if (ret) {
		ALOGE("Error(%d) in get_battery_ps_name:%s\n", ret, strerror(ret));
		return ret;
	}

	snprintf(battid_path, sizeof(battid_path), "%s/%s/model_name", POWER_SUPPLY_PATH, ps_batt_name);

	ALOGI("Reading battid from %s\n", battid_path);
	fd = open(battid_path, O_RDONLY);

	if (fd < 0)
		return errno;

	ret = read(fd, battid, MODEL_NAME_SIZE);
	if (ret < 0) {
		close(fd);
		return errno;
	}
	close(fd);
	return 0;
}

int get_serial_num(char *serial_num)
{
	int ret;
	int fd;
	char ps_batt_name[PATH_MAX];
	char serial_num_path[PATH_MAX];
	ret =  get_battery_ps_name(ps_batt_name);
	if (ret) {
		ALOGE("Error(%d) in get_battery_ps_name:%s\n", ret, strerror(ret));
		return ret;
	}

	snprintf(serial_num_path, sizeof(serial_num_path), "%s/%s/serial_number", POWER_SUPPLY_PATH, ps_batt_name);

	ALOGI("Reading serial number from %s\n", serial_num_path);

	fd = open(serial_num_path, O_RDONLY);

	if (fd < 0)
		return errno;

	ret = read(fd, serial_num, MAX_SERIAL_NUM_SIZE);
	if (ret < 0) {
		close(fd);
		return errno;
	}
	close(fd);

	serial_num[ret] = '\0';
	return 0;
}

bool is_prim_cksum_ok(unsigned char *buf, int size)
{

	struct primary_header *pheader = (struct primary_header *)buf;
	unsigned short pcksum = pheader->cksum;
	pheader->cksum = 0x00;

	if (checksum(buf, size) != pcksum) {
		ALOGE("Primary checksum failed:%x:%x\n",
				checksum(buf, size), pcksum);
		pheader->cksum = pcksum;
		return FALSE;
	}

	pheader->cksum = pcksum;
	return TRUE;
}

bool is_sec_cksum_ok(struct sec_file_body *sbuf, int len)
{
	if (checksum(sbuf, len - 2) != sbuf->tbl.cksum) {
		ALOGE("Secondary checksum failed:%x:%x\n", checksum(sbuf, len - 2), sbuf->tbl.cksum);
		return FALSE;
	} else
		return TRUE;
}

int write_to_intel_fg_dev(unsigned char *buf)
{
	int ret;
	int fd = open(INTEL_FG_DEV_FILE, O_WRONLY, S_IRWXU|S_IRWXG|S_IRWXO);
	if (fd < 0)
		return errno;

	ret = write(fd, buf, XPWR_CONFIG_SIZE - 2);
	if (ret != XPWR_CONFIG_SIZE - 2) {
		close(fd);
		if (ret < 0)
			return errno;
		else
			return EIO;
	}
	close(fd);
	return 0;
}

int write_to_dev(struct table_body *fg_tbl)
{
	int ret;
	int fd = open(DEV_FILE, O_WRONLY, S_IRWXU|S_IRWXG|S_IRWXO);
	if (fd < 0)
		return errno;

	/* write fg config table to device file */
	ret = write(fd, fg_tbl, sizeof(*fg_tbl) - sizeof(fg_tbl->cksum));
	if (ret != (sizeof(*fg_tbl) - sizeof(fg_tbl->cksum))) {
		close(fd);
		if (ret < 0)
			return errno;
		else
			return EIO;
	}

	close(fd);
	return 0;
}

int get_primary_fg_config(unsigned char *pbuf,
		struct table_body *tbl, char *battid)
{
	int size;
	struct primary_header *pheader;
	struct table_body *temp_tbl;
	unsigned char *ptr;
	int i = 1;
	char ps_batt_name[PATH_MAX];
	int ret;

	ret = get_battery_ps_name(ps_batt_name);
	if (ret) {
		ALOGE("Error(%d) in get_battery_ps_name:%s\n", ret, strerror(ret));
		return ret;
	}

	pheader = (struct primary_header *)pbuf;
	size = sizeof(struct primary_header);
	ptr  = (unsigned char *)pbuf;
	ptr += size;

	ALOGI("pheader->file_size =%d\n", pheader->file_size);

	while (size < pheader->file_size) {

		temp_tbl = (struct table_body *)ptr;
		ALOGI("Table[%d].name:%.8s FG.name=%s\n", i, temp_tbl->table_name, ps_batt_name);
		ALOGI("Table[%d].Battid:%.8s FG.Battid:%.8s\n",
				i, temp_tbl->battid, battid);
		/* FIXME: Read Table name from power_supply sysfs */
		if (!strncmp(temp_tbl->table_name, ps_batt_name,
					MAX_TABLE_NAME_SIZE)) {
			if (!strncmp(temp_tbl->battid, battid,
						MODEL_NAME_SIZE)) {
				ALOGI("Matching table_name and battid:%.8s:%.8s\n", temp_tbl->table_name, temp_tbl->battid);
				memcpy(tbl, temp_tbl, sizeof(*tbl));
				return 0;
			}

		}
		size += sizeof(*temp_tbl);
		ptr += sizeof(*temp_tbl);
		i++;
	}
	return EINVAL;
}

static int isCOS()
{
	int cos = 0;
	char cmdline_buf[MAX_COMMAND_LINE_BUF];
	char *ptr;

	int fd, size;

	fd = open("/proc/cmdline", O_RDONLY);
	if (fd < 0) {
		ALOGI("%s:Unable to read commandline\n", __func__);
	} else {
		size = read(fd, cmdline_buf, MAX_COMMAND_LINE_BUF);
		cmdline_buf[MAX_COMMAND_LINE_BUF - 1] = '\0';
		if ( size <= 0) {
			ALOGI("%s:error to read commandline\n", __func__);
		} else {
			ptr = strstr(cmdline_buf, "androidboot.mode=");
			if (ptr != NULL) {
				ptr += strlen("androidboot.mode=");
				if (strncmp(ptr, "main", 4) == 0)
					cos = 0;
				else if (strncmp(ptr, "charger", 7) == 0)
					cos = 1;
			}
		}
		close(fd);
	}
	return cos;
}

int get_fg_config_table(struct table_body *sec_tbl)
{
	char battid[MAX_BATTID_SIZE];
	char serial_num[MAX_SERIAL_NUM_SIZE + 1] = {'\0'};
	unsigned char *pbuf;
	unsigned char *xpwr_pbuf;
	unsigned char *xpwr_sbuf;
	int is_pcksum_ok = 0;
	int is_scksum_ok = 0;
	struct primary_header pheader;
	struct sec_file_body sbuf;
	int ret;
	int inCOS;
	bool use_sec_file = 0;
	unsigned short pcksum, scksum, tmp;

	/* get battid */
	ret = get_battid(battid);
	if (ret) {
		ALOGE("Error(%d) in get_battid:%s\n", ret, strerror(ret));
		return ret;
	}

	/* FIXME: Find a generic way to support multiple config structures */
	if (is_xpwr) {
		xpwr_sbuf = malloc(XPWR_SEC_CONFIG_SIZE);
		if (xpwr_sbuf == NULL) {
			ALOGE("%s:%d:insufficient memory\n", __func__, __LINE__);
			return ENOMEM;
		}
		xpwr_pbuf = malloc(XPWR_CONFIG_SIZE);
		if (xpwr_pbuf == NULL) {
			free(xpwr_sbuf);
			ALOGE("%s:%d:insufficient memory\n", __func__, __LINE__);
			return ENOMEM;
		}
		if (read_xpwr_secondary_file(xpwr_sbuf))
			ALOGE("Secondary file could not be read");
		else {
			scksum = checksum(xpwr_sbuf, XPWR_SEC_CONFIG_SIZE - 2);
			tmp = *(unsigned short *)&xpwr_sbuf[XPWR_SEC_CONFIG_SIZE - 2];
			if (scksum != tmp)
				ALOGE("Secondary checksum mismatch");
			else
				use_sec_file = 1;
		}
		if (read_xpwr_primary_file(xpwr_pbuf, XPWR_CONFIG_SIZE)) {
			ALOGE("Primary file could not be read");
			free(xpwr_pbuf);
			free(xpwr_sbuf);
			return EINVAL;
		} else {
			pcksum = checksum(xpwr_pbuf, XPWR_CONFIG_SIZE - 2);
			tmp = *(unsigned short *)&xpwr_pbuf[XPWR_CONFIG_SIZE - 2];
			if (pcksum != tmp) {
				ALOGE("Primary checksum mismatch");
				if (!use_sec_file) {
					ALOGE("Cannot get config from either file");
					free(xpwr_pbuf);
					free(xpwr_sbuf);
					return EINVAL;
				}
			}
		}
		if (use_sec_file && pcksum
			== *(unsigned short *)&xpwr_sbuf[XPWR_SEC_CONFIG_SIZE - 4]) {
			ALOGI("Using data from secondary file");
			ret = write_to_intel_fg_dev(xpwr_sbuf);
		} else {
			ALOGI("Using data from primary file");
			ret = write_to_intel_fg_dev(xpwr_pbuf);
		}
		free(xpwr_pbuf);
		free(xpwr_sbuf);
		return ret;
	}

	/* get serial number */
	ret = get_serial_num(serial_num);
	if (ret) {
		ALOGE("Error(%d) in get_serial_num:%s\n", ret, strerror(ret));
		return ret;
	}

	/* read primary header */
	ret = read_primary_header(&pheader);
	if (ret) {
		ALOGE("Error(%d) in read_primary_header:%s\n", ret, strerror(ret));
		return ret;
	}

	pbuf = malloc(pheader.file_size);

	if (pbuf == NULL) {
		ALOGE("%s:%d:insufficient memory\n", __func__, __LINE__);
		return ENOMEM;
	}

	/*read primary file including header */
	ret = read_primary_file(pbuf, pheader.file_size);
	if (ret) {
		ALOGE("Error(%d) in read_primary:%s\n", ret, strerror(ret));
		free(pbuf);
		return ret;
	}

	ALOGI("pheader.size=%d\n", pheader.file_size);

	is_pcksum_ok = is_prim_cksum_ok(pbuf, pheader.file_size);

	/* read secondary */
	ret = read_secondary_file(&sbuf);
	if (ret) {
		ALOGE("Error(%d) in read_secondary_file:%s\n", ret, strerror(ret));
		goto read_pri_config;
	} else
		is_scksum_ok = is_sec_cksum_ok(&sbuf, sizeof(sbuf.tbl));

	/* If primary checksum mismatch return error */
	if (!(is_pcksum_ok) && !(is_scksum_ok)) {
		ALOGE("primary and secondary checksum failed.\n");
		free(pbuf);
		return EINVAL;
	}

	/* Read primary config if secondary battid doesn't match or
	secondary checksum is not ok or primary file has a new config */
	if (!is_scksum_ok)
		ALOGE("Secondary checksum failed\n");
	else if (strncmp(sbuf.tbl.battid, battid, MODEL_NAME_SIZE))
		ALOGE("Secondary Battid doesn't match %s:%s\n", sbuf.tbl.battid, battid);
	else if ((strlen(sbuf.serial_num) !=
		strnlen(serial_num, MAX_SERIAL_NUM_SIZE+1))
		|| (strncmp(sbuf.serial_num, serial_num,
		strnlen(serial_num, MAX_SERIAL_NUM_SIZE+1))))
		ALOGE("Secondary serial number doesn't match %s:%s\n", sbuf.serial_num, serial_num);
	else if (pheader.cksum != sbuf.pcksum)
		ALOGE("Secondary.primary_checksum mismatch. New primary file detected\n");
	else {
		ALOGI("Using FG data from %s\n", SECONDARY_FILE);
		memcpy(sec_tbl, &sbuf.tbl, sizeof(*sec_tbl));
		free(pbuf);
		return 0;
	}

read_pri_config:
	if (!is_pcksum_ok) {
		free(pbuf);
		return -EINVAL;
	}

	ALOGI("Using FG data from %s\n", PRIMARY_FILE);
	ret = get_primary_fg_config(pbuf, &sbuf.tbl, battid);
	if (ret) {
		ALOGE("Error(%d) in get_primary_fg_config:%s\n", ret, strerror(ret));
		free(pbuf);
		return ret;
	}
	/* WA to  set fg_config.config_init = 1 in COS */
	inCOS = isCOS();
	ALOGI("inCOS=%d\n", inCOS);
	if(inCOS)
		sbuf.tbl.config_init = 1;
	/* copy the secondary table */
	memcpy(sec_tbl, &sbuf.tbl, sizeof(*sec_tbl));
	free(pbuf);
	return 0;
}

int write_xpwr_sec(unsigned char *buf)
{
	int fds, ret, cksum;
	unsigned char tmp[XPWR_SEC_CONFIG_SIZE];
	fds = open(SECONDARY_FILE, O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
	if (fds < 0) {
		ALOGE("Error(%d) in opening %s:%s\n", errno, SECONDARY_FILE, strerror(errno));
		return errno;
	}
	if (!read_xpwr_primary_file(tmp, XPWR_CONFIG_SIZE))
		memcpy(buf + XPWR_SEC_CONFIG_SIZE - 4, tmp + XPWR_CONFIG_SIZE - 2, 2);
	cksum = checksum(buf, XPWR_SEC_CONFIG_SIZE - 2);
	memcpy(buf + XPWR_SEC_CONFIG_SIZE - 2, &cksum, 2);
	ret = write(fds, buf, XPWR_SEC_CONFIG_SIZE);
	if (ret != XPWR_SEC_CONFIG_SIZE) {
		ALOGE("Error in writing secondary file\n");
		close(fds);
		return ENODATA;
        }
        close(fds);
        ALOGI("Wrote %d bytes to %s Size of table=%d\n", ret, SECONDARY_FILE, XPWR_SEC_CONFIG_SIZE);
        return 0;
}

int write_sec_config(struct sec_file_body *sbuf)
{
	int fds, ret;
	char serial_num[MAX_SERIAL_NUM_SIZE + 1] = {'\0'};
	struct primary_header pheader;
	/* open secondary file in write mode */
	fds = open(SECONDARY_FILE, O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
	if (fds < 0) {
		ALOGE("Error(%d) in opening %s:%s\n", errno, SECONDARY_FILE, strerror(errno));
		return errno;
	}

	/* Read primary header to get primary checksum. This is to detect
	changes in the primary config, so that when configure FG, config data
	can be read from primary, instead of reading from secondary.*/
	ret = read_primary_header(&pheader);
	if (ret) {
		ALOGE("Error(%d) in read_primary_header:%s\n", ret, strerror(ret));
		close(fds);
		return ENODATA;
	}

	/* set primary checksum in secondary. This field will be used to check
	if the primary is modified or not */
	sbuf->pcksum = pheader.cksum;

	/*set secondary checksum */
	sbuf->tbl.cksum = checksum(&sbuf->tbl,
				sizeof(sbuf->tbl) - sizeof(sbuf->tbl.cksum));

	/* Set serial number of the battery. Next time when the same battery
	 * is plugged in, we know that the data is valid for it.
	 */
	 ret = get_serial_num(serial_num);
	 if (ret) {
		ALOGE("Error(%d) in get_serial_num%s\n", ret, strerror(ret));
		close(fds);
		return ENODATA;
	}
	snprintf(sbuf->serial_num,
			 strnlen(serial_num, MAX_SERIAL_NUM_SIZE+1) + 1,
			"%s", serial_num);

	/* write secondary file */
	ret = write(fds, sbuf, sizeof(*sbuf));
	if (ret != sizeof(*sbuf)) {
		ALOGE("Error in writing secondary file\n");
		close(fds);
		return ENODATA;
	}
	close(fds);
	ALOGI("Wrote %d bytes to %s Size of table=%d\n", ret, SECONDARY_FILE, sizeof(sbuf->tbl));
	return 0;
}

int write_fgdev_config(void)
{
	struct sec_file_body sbuf;
	int ret;

	/* get FG config from secondary/primary */
	ret = get_fg_config_table(&sbuf.tbl);
	if (ret)
		return ret;
	if (is_xpwr)
		return 0;

	/* write to device file */
	return write_to_dev(&sbuf.tbl);
}

int  read_fg_write_sec()
{
	int fdd, ret;
	unsigned char fg_config[MAX_FG_CONFIG_SIZE];
	struct sec_file_body sbuf;
	unsigned char xpwr_sbuf[XPWR_SEC_CONFIG_SIZE];
	char path[PATH_MAX];

	if (!get_battery_ps_name(path) && is_xpwr) {
		fdd = open(INTEL_FG_DEV_FILE, O_RDONLY);
		if (fdd <0) {
			ALOGE("Error(%d) in opening %s:%s\n", errno,
				INTEL_FG_DEV_FILE, strerror(errno));
			return errno;
		}
		ret = read(fdd, xpwr_sbuf, XPWR_CONFIG_SIZE - 2);
		if (ret != XPWR_CONFIG_SIZE - 2) {
			close(fdd);
			ALOGE("Error(%d) in reading %s:%s requested=%d read=%d\n", errno,
							INTEL_FG_DEV_FILE, strerror(errno),
							XPWR_CONFIG_SIZE - 2, ret);
			if (ret < 0)
				return errno;
			else
				return ENODATA;
		}
		ret = write_xpwr_sec(xpwr_sbuf);
		if (ret)
			ALOGE("Eror in writing to secondary file");
		close(fdd);
		return ret;
	}

	/*open device file */
	fdd = open(DEV_FILE, O_RDONLY);
	if (fdd < 0) {
		ALOGE("Error(%d) in opening %s:%s\n", errno, DEV_FILE, strerror(errno));
		return errno;
	}

	/*read from device file except checksum*/
	ret = read(fdd, &sbuf.tbl, (sizeof(sbuf.tbl) - sizeof(sbuf.tbl.cksum)));
	if (ret != (sizeof(sbuf.tbl) - sizeof(sbuf.tbl.cksum))) {
		close(fdd);

		ALOGE("Error(%d) in reading %s:%s requested=%d read=%d\n", errno, DEV_FILE, strerror(errno),
				(int)(sizeof(sbuf.tbl) - sizeof(sbuf.tbl.cksum)), ret);
		if (ret < 0)
			return errno;
		else
			return ENODATA;
	}

	ret = write_sec_config(&sbuf);
	if (ret) {
		ALOGE("Error(%d) in saving secondary:%s\n", ret, strerror(ret));
	}

	close(fdd);

	return 0;
}

char *cfg_name[] = {
	"rcomp0",
	"tempCo",
	"kempty0",
	"full_cap",
	"cycles",
	"full_capnom",
	"soc_empty",
	"ichgt_term",
	"design_cap",
	"etc",
	"rsense",
	"cfg",
	"learn_cfg",
	"filter_cfg",
	"relax_cfg",
	"qrtbl00",
	"qrtbl10",
	"qrtbl20",
	"qrtbl30",
	"full_soc_thr",
	"vempty",
	NULL
};

int dump_config(struct table_body *config)
{
	unsigned short  *data_ptr =  (unsigned short *) config->fg_config;
	int i = 0;
	int char_table_size;


	ALOGI("table_type:0x%x\n", (unsigned int)config->table_type);
	ALOGI("size:0x%x\n", config->size);
	ALOGI("revision:0x%x\n", (int)config->rev);
	ALOGI("table_name:%.8s\n", config->table_name);
	ALOGI("battid:%.8s\n", config->battid);
	ALOGI("reserved:0x%x\n", (unsigned int)config->config_init);

	while (cfg_name[i] != NULL)
		ALOGI("%s:0x%x\n", cfg_name[i++], *data_ptr++);

	data_ptr--;

	char_table_size = MAX_FG_CONFIG_SIZE / sizeof(unsigned short);

	for (i = (i - 1); i < char_table_size; ++i)
		ALOGI("cell_char_tbl[%d]:0x%x\n", i, *data_ptr++);

	ALOGI("checksum:0x%x\n", config->cksum);
	return 0;
}
void dump_primary_header(struct primary_header pheader)
{
	ALOGI("Primary Header\n===================\n");
	ALOGI("revision=0x%x\n", (unsigned short) pheader.rev);
	ALOGI("file_size=0x%x\n", (unsigned short) pheader.file_size);
	ALOGI("checksum=0x%x\n", (unsigned short) pheader.cksum);
	ALOGI("reserved=0x%x\n", (unsigned short) pheader.res);

}

int dump_fg_config(char sel)
{
	int ret;
	struct sec_file_body sbuf;
	char battid[MAX_BATTID_SIZE];
	struct primary_header pheader;
	unsigned char *pbuf;
	unsigned long offset = 0;
	int i = 1;

	/* get FG config from secondary/primary */
	if (sel == 'p') {
		/* read primary header */
		ret = read_primary_header(&pheader);
		if (ret) {
			ALOGE("Error(%d) in read_primary_header:%s\n", ret, strerror(ret));
			return ret;
		}

		dump_primary_header(pheader);

		pbuf = malloc(pheader.file_size);

		if (pbuf == NULL) {
			ALOGE("%s:%d:Error: Insufficient memory\n",
					__func__, __LINE__);
			return ENOMEM;
		}

		/*read primary file including header */
		ret = read_primary_file(pbuf, pheader.file_size);
		if (ret) {
			ALOGE("Error(%d) in read_primary:%s\n", ret, strerror(ret));
			free(pbuf);
			return ret;
		}

		offset += sizeof(pheader);
		while (offset < pheader.file_size) {
			ALOGI("Table:%d\n=========================\n", i++);
			ret = dump_config((struct table_body *)(pbuf + offset));
			if (ret) {
				ALOGE("Error(%d) in dump_config:%s\n", ret, strerror(ret));
				free(pbuf);
				return ret;
			}
			offset += sizeof(struct table_body);

		}
		free(pbuf);
		return ret;
	}
	else if (sel == 's')
		ret = read_secondary_file(&sbuf);
	else if (sel == '\0')
		ret = get_fg_config_table(&sbuf.tbl);
	else {
		ALOGE("Invalid argument with -d\n");
		return EINVAL;
	}

	if (ret) {
		ALOGE("Error(%d) in reading config:%s\n", ret, strerror(ret));
		return ret;
	}
	return dump_config(&sbuf.tbl);
}

void print_help(void)
{
	ALOGI("\nfg_conf [-w/-r/-d/-ds/-dp]\n");
	ALOGI("-w - Read config and write to fg interface\n");
	ALOGI("-r - Read config from fg and save in secondary\n");
	ALOGI("-d - Dump the matching table from secondary/primary\n");
	ALOGI("-ds - Dump secondaty file\n");
	ALOGI("-dp -  Dump primary file\n");

}

int main(int argc, char *argv[])
{
	int ret = 0;
	if (argc != 2) {
		ALOGE("Invalid argument list\n");
		return -EINVAL;
	}

	if ((argc == 2) && (argv[1][0] == '-'))	{
		switch (argv[1][1]) {

		case 'w':
			ALOGI("Invoked with -w\n");
			ret = write_fgdev_config();
			if (ret) {
				ALOGE("Error(%d) in write_fgdev_config\n", ret);
				return ret;
			}
			ALOGI("Restored FG data successfully\n");

			break;
		case 'r':
			ALOGI("Invoked with -r\n");
			ret = read_fg_write_sec();
			if (ret) {
				ALOGE("Error(%d) in read_fg_write_sec\n", ret);
				return ret;
			}
			ALOGI("Saved FG data successfully in %s\n", SECONDARY_FILE);
			break;
		case 'd':
			ALOGI("Invoked with -d\n");
			ret =  dump_fg_config(argv[1][2]);
			if (ret) {
				ALOGE("Error(%d) in dumping FG Config:%s\n", ret, strerror(ret));
				return ret;
			}
			break;
		case 'h':
			ALOGI("Invoked with -h\n");
			print_help();
			break;
		default:
			ALOGE("Invoked with invalid argument\n");
			return EINVAL;
		}
	} else  {
		ALOGE("Invoked with invalid argument\n");
		return EINVAL;

	}
	return ret;
}
