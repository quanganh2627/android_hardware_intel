/*
 * Copyright 2011-2014 Intel Corporation
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

#include <stdlib.h>
#include <stdio.h>
#include "miu.h"
#include "logs.h"

void cmd_flash_modem_fw(char *filename)
{
	if (miu_initialize(miu_progress_cb, miu_log_cb, filename) != E_MIU_ERR_SUCCESS) {
		fprintf(stderr, "%s initialization has failed\n", __func__);
	} else {
		if (miu_flash_modem_fw(filename, 0) != E_MIU_ERR_SUCCESS) {
			fprintf(stderr, "Failed flashing modem FW!\n");
			miu_dispose();
			exit(1);
		}
		miu_dispose();
	}
}
