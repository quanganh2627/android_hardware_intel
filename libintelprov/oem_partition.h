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

#ifndef _OEM_PARTITION_H_
#define _OEM_PARTITION_H_

int oem_partition_start_handler(int argc, char **argv);
int oem_partition_stop_handler(int argc, char **argv);
int oem_partition_cmd_handler(int argc, char **argv);
int oem_erase_partition(int argc, char **argv);
int oem_repart_partition(int argc, char **argv);
int oem_retrieve_partitions(int argc, char **argv);
int oem_wipe_partition(int argc, char **argv);
void oem_partition_disable_cmd_reload();

struct ufdisk {
	void (*umount_all)(void);
	int (*create_partition)(void);
};

void oem_partition_init(struct ufdisk *ufdisk);

#endif	/* _OEM_PARTITION_H_ */
