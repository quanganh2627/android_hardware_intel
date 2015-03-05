/*
 * Copyright 2013 Intel Corporation
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

#include "oem_partition.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <cgpt.h>
#include <cutils/properties.h>
#include <droidboot_fstab.h>

#include "util.h"
#include "gpt/partlink/partlink.h"

#define K_MAX_LINE_LEN 8192
#define K_MAX_ARGS 256
#define K_MAX_ARG_LEN 256
#define OEM_PARTITION_TIMEOUT 20

static void fake_umount_all(void) {}
static int fake_create_partition(void) { return -1; }

static struct ufdisk ufdisk = {
	.umount_all = fake_umount_all,
	.create_partition = fake_create_partition
};

static char **str_to_array(char *str, int *argc)
{
	char *str1, *token;
	char *saveptr1;
	int j;
	int num_tokens;
	char **tokens;

	tokens = malloc(sizeof(char *) * K_MAX_ARGS);

	if(tokens == NULL)
		return NULL;

	num_tokens = 0;

	for (j = 1, str1 = str; ; j++, str1 = NULL) {
		token = strtok_r(str1, " ", &saveptr1);

		if (token == NULL)
			break;

		tokens[num_tokens] = (char *) malloc(sizeof(char) * K_MAX_ARG_LEN+1);

		if(tokens[num_tokens] == NULL)
			break;

		strncpy(tokens[num_tokens], token, K_MAX_ARG_LEN);
		num_tokens++;

		if (num_tokens == K_MAX_ARGS)
			break;
	}

	*argc = num_tokens;
	return tokens;
}

static int (*indirected_cmd_reload)(int argc, char *argv[]) = cmd_reload;

static int cmd_noop(int argc, char **argv) { return 0; }

void oem_partition_disable_cmd_reload()
{
	indirected_cmd_reload = cmd_noop;
}

static int oem_partition_gpt_sub_command(int argc, char **argv)
{
	unsigned int i;
	char *command = argv[0];
	struct {
		const char *name;
		int (*fp)(int argc, char *argv[]);
	} cmds[] = {
		{"create", cmd_create },
		{"add", cmd_add},
		{"dump", cmd_show},
		{"repair", cmd_repair},
		{"boot", cmd_bootable},
		{"find", cmd_find},
		{"prioritize", cmd_prioritize},
		{"legacy", cmd_legacy},
		{"reload", indirected_cmd_reload},
	};

	optind = 0;
	for (i = 0; command && i < sizeof(cmds)/sizeof(cmds[0]); ++i)
		if (0 == strncmp(cmds[i].name, command, strlen(command)))
			return cmds[i].fp(argc, argv);

	return -1;
}

int is_property_set(char *property, char *expected)
{
	char value[PROPERTY_VALUE_MAX];

	if (property_get(property, value, NULL))
		if (strcmp(value, expected) == 0)
			return true;

	return false;
}

static int oem_partition_gpt_handler(FILE *fp)
{
	int argc = 0;
	int ret = 0;
	int i;
	char buffer[K_MAX_ARG_LEN];
	char **argv = NULL;
	char value[PROPERTY_VALUE_MAX] = {'\0'};

	property_get("sys.partitioning", value, NULL);
	if (strcmp(value, "1")) {
		error("Partitioning is not started\n");
		return -1;
	}

	uuid_generator = uuid_generate;
	while (fgets(buffer, sizeof(buffer), fp)) {
		if (buffer[strlen(buffer)-1] == '\n')
			buffer[strlen(buffer)-1]='\0';
		argv = str_to_array(buffer, &argc);

		if(argv != NULL) {
			ret = oem_partition_gpt_sub_command(argc, argv);

			for(i = 0; i < argc ; i++) {
				if (argv[i]) {
					free(argv[i]);
					argv[i]=NULL;
				}
			}
			free(argv);
			argv=NULL;

			if (ret) {
				error("GPT command failed\n");
				return -1;
			}
		}
		else {
			error("GPT str_to_array error. Malformed string ?\n");
			return -1;
		}
	}

	if (indirected_cmd_reload == cmd_reload)
		partlink_populate();

	return 0;
}

static int oem_partition_mbr_handler(FILE *fp)
{
	print("Using MBR\n");
	return ufdisk.create_partition();
}

static int nuke_volume(const char* volume, long int bufferSize)
{
	Volume* v = volume_for_path(volume);
	int fd, count, count_w, offset;
	char *pbuf = NULL, *pbufRead = NULL;
	long int ret, retval;
	long long size;

	if (v == NULL) {
		error("unknown volume \"%s\"\n", volume);
		return -1;
	}
	if (strcmp(v->fs_type, "ramdisk") == 0) {
		// you can't format the ramdisk.
		error("can't nuke_volume ramdisk volume: \"%s\"\n", volume);
		return -1;
	}
	if (strcmp(v->mount_point, volume) != 0) {
		error("can't give path \"%s\" to nuke_volume\n", volume);
		return -1;
	}

	if (ensure_path_unmounted(volume) != 0) {
		error("nuke_volume failed to unmount \"%s\"\n", v->mount_point);
		return -1;
	}

	fd = open(v->device, O_RDWR);
	if (fd == -1) {
		error("nuke_volume failed to open for writing \"%s\"\n", v->device);
		return -1;
	}

	pbuf = (char *)malloc(bufferSize);
	if (pbuf == NULL){
		error("nuke_volume: malloc pbuf failed\n");
		ret = -1;
		goto end3;
	}

	pbufRead = (char *)malloc(bufferSize);
	if (pbufRead == NULL){
		error("nuke_volume: malloc pbufRead failed\n");
		ret = -1;
		goto end2;
	}

	memset(pbuf, 0xFF, bufferSize*sizeof(char));

	size = lseek64(fd, 0, SEEK_END);

	if(size == -1) {
		error("nuke_volume: lseek64 fd failed\n");
		ret = -1;
		goto end1;
	}

	offset = lseek(fd, 0, SEEK_SET);

	if (offset == -1) {
		error("nuke_volume: lseek fd failed");
		ret = -1;
		goto end1;
	}

	print("erasing volume \"%s\", size=%lld...\n", volume, size);

	//now blast the device with F's until we hit the end.
	count = 0;
	do {
		ret = write(fd, pbuf, bufferSize);

		if (ret == -1) {
			error("nuke_volume: failed to write file");
			goto end1;
		}

		count++;
	} while (ret == bufferSize);

	print("wrote ret %ld, count %d,  \"%s\"\n",ret, count, v->device);

	//now do readback check that data is as expected
	offset = lseek(fd, 0, SEEK_SET);

	if (offset == -1) {
		error("nuke_volume: lseek check data failed");
		goto end1;
	}

	count_w = count;
	count = 0;
	do {
		ret = read(fd, pbufRead, bufferSize);

		if (ret <= 0) {
			error("nuke_volume: failed to read data");
			goto end1;
		}

		retval = memcmp(pbuf, pbufRead,  bufferSize);
		count++;
		if (retval != 0) {
			error("nuke_volume failed read back check!! \"%s\"\n",
				 v->device);
			ret = -1;
			goto end1;
		}
	} while (ret == bufferSize);

	if (count != count_w){
		error("nuke_volume: failed read back check, bad count %d\n", count);
		ret = -1;
		goto end1;
	}

	print("read back ret %ld, count %d \"%s\"\n",ret, count, v->device);
	ret = 0;

end1:
	free(pbufRead);
end2:
	free(pbuf);
end3:
	sync();
	close(fd);
	pbuf = NULL;
	pbufRead = NULL;
	return ret;
}


int oem_partition_start_handler(int argc, char **argv)
{
	int timeout = OEM_PARTITION_TIMEOUT;

	property_set("sys.partitioning", "1");

	while (!is_property_set("sys.partitioning", "1") && --timeout) {
		sleep(1);
	}

	if (!timeout) {
		error("Partitioning is not started\n");
		return -1;
	}

	print("Start partitioning\n");
	ufdisk.umount_all();
	return 0;
}

int oem_partition_stop_handler(int argc, char **argv)
{
	int timeout = OEM_PARTITION_TIMEOUT;


	property_set("sys.partitioning", "0");

	while (!is_property_set("sys.partitioning", "0") && --timeout) {
		sleep(1);
	}

	if (!timeout) {
		error("Partitioning is not stopped\n");
		return -1;
	}

	print("Stop partitioning\n");
	return 0;
}

int oem_partition_cmd_handler(int argc, char **argv)
{
	char buffer[K_MAX_ARG_LEN];
	char partition_type[K_MAX_ARG_LEN];
	FILE *fp;
	int retval = -1;

	memset(buffer, 0, sizeof(buffer));

	if (argc == 2) {
		fp = fopen(argv[1], "r");
		if (!fp) {
			error("Can't open partition file");
			return -1;
		}

		if (!fgets(buffer, sizeof(buffer), fp)) {
			error("partition file is empty");
			return -1;
		}

		buffer[strlen(buffer)-1]='\0';

		if (sscanf(buffer, "%*[^=]=%255s", partition_type) != 1) {
			error("partition file is invalid");
			return -1;
		}

		if (!strncmp("gpt", partition_type, strlen(partition_type)))
			retval = oem_partition_gpt_handler(fp);

		if (!strncmp("mbr", partition_type, strlen(partition_type)))
			retval = oem_partition_mbr_handler(fp);

		fclose(fp);
	}

	return retval;
}

#define MOUNT_POINT_SIZE    50      /* /dev/<whatever> */
#define BUFFER_SIZE         4000000 /* 4Mb */

static int get_mountpoint(char *name, char *mnt_point)
{
	int size;
	if (name[0] == '/') {
		size = snprintf(mnt_point, MOUNT_POINT_SIZE, "%s", name);

		if (size == -1 || size > MOUNT_POINT_SIZE - 1) {
			error("Mount point parameter size exceeds limit");
			return -1;
		}
	} else {
		if (!strcmp(name, "userdata")) {
			strcpy(mnt_point, "/data");
		} else {
			size = snprintf(mnt_point, MOUNT_POINT_SIZE, "/%s", name);

			if (size == -1 || size > MOUNT_POINT_SIZE - 1) {
				error("Mount point size exceeds limit");
				return -1;
			}
		}
	}

	return 0;
}

int oem_erase_partition(int argc, char **argv)
{
	int retval = -1;
	char mnt_point[MOUNT_POINT_SIZE] = "";

	if (argc != 2) {
		/* Should not pass here ! */
                error("oem erase called with wrong parameter!");
		goto end;
	}

	if (get_mountpoint(argv[1], mnt_point))
		goto end;

	print("CMD '%s %s'...\n", argv[0], mnt_point);

	print("ERASE step 1/2...\n");
	retval = nuke_volume(mnt_point, BUFFER_SIZE);
	if (retval != 0) {
		error("format_volume failed: %s\n", mnt_point);
		goto end;
	} else {
		print("format_volume succeeds: %s\n", mnt_point);
	}

	print("ERASE step 2/2...\n");
	retval = format_volume(mnt_point);
	if (retval != 0) {
		error("format_volume failed: %s\n", mnt_point);
	} else {
		print("format_volume succeeds: %s\n", mnt_point);
	}

end:
	return retval;
}

int oem_repart_partition(int argc, char **argv)
{
	int retval = -1;

	if (argc != 1) {
		/* Should not pass here ! */
		error("oem repart does not require argument");
		goto end;
	}

	retval = ufdisk.create_partition();
	if (retval != 0)
		error("cannot write partition");

end:
	return retval;
}

int oem_retrieve_partitions(int argc, char **argv)
{
	int ret;
	char value[PROPERTY_VALUE_MAX];
	char drive[] = STORAGE_BASE_PATH;
	char *boot_argv[3];
	char boot_opt[] = "-p";
	char *reload_argv[2];

	if(argc != 1) {
		error("oem retrieve_partitions does not require argument");
		return -1;
	}

	property_get("sys.partitioning", value, NULL);
	if (strcmp(value, "1")) {
		error("Partitioning is not started\n");
		return -1;
	}

	boot_argv[1] = boot_opt;
	boot_argv[2] = drive;
	printf("boot %s %s\n", boot_argv[1], boot_argv[2]);
	ret = cmd_bootable(3, boot_argv);
	if (ret) {
		error("gpt boot command failed\n");
		return ret;
	}

	reload_argv[1] = drive;
	printf("reload %s\n", reload_argv[1]);
	ret = cmd_reload(2, reload_argv);
	if (ret) {
		error("gpt reload command failed\n");
		return ret;
	}

	return 0;
}

void oem_partition_init(struct ufdisk *new_ufdisk)
{
	if (!new_ufdisk)
		return;

	ufdisk = *new_ufdisk;
	if (!ufdisk.umount_all)
		ufdisk.umount_all = fake_umount_all;
	if (!ufdisk.create_partition)
		ufdisk.create_partition = fake_create_partition;
}

int oem_wipe_partition(int argc, char **argv)
{
	int retval = -1;
	char mnt_point[MOUNT_POINT_SIZE] = "";

	if (argc != 2) {
                error("oem erase called with wrong parameter!");
		goto end;
	}

	if (get_mountpoint(argv[1], mnt_point))
		goto end;

	print("CMD '%s %s'...\n", argv[0], mnt_point);

	retval = nuke_volume(mnt_point, BUFFER_SIZE);
	if (retval != 0)
		error("wipe partition failed: %s\n", mnt_point);

end:
	return retval;
}
