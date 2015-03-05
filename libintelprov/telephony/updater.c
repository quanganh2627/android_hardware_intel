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


#include <stdio.h>
#include <edify/expr.h>
#include <updater/updater.h>
#include "logs.h"
#include "miu.h"

#define MODEM_PATH "/tmp/radio_firmware.bin"
#define MODEM_NAME "radio_firmware"

static Value *FlashModemFn(const char *name, State * state, int argc, Expr * argv[])
{
	Value *ret = NULL;
	int err;
	ZipArchive modem_za;
	char *filename = NULL;
	e_miu_flash_options_t flash_options = 0;

	if (ReadArgs(state, argv, 1, &filename) < 0)
		return NULL;

	if (filename == NULL || strlen(filename) == 0) {
		ErrorAbort(state, "filename argument to %s can't be empty", name);
		goto done;
	}

	err = mzOpenZipArchive(filename, &modem_za);
	if (err) {
		printf("Failed to open zip archive %s\n", filename);
		ret = StringValue(strdup(""));
		goto done;
	}
	printf("miu using archive %s\n", filename);
	mzCloseZipArchive(&modem_za);


	if (miu_initialize(miu_progress_cb, miu_log_cb, filename) != E_MIU_ERR_SUCCESS) {
		printf("%s failed at %s\n", __func__, "miu_initialize failed");
	} else {
		if (miu_flash_modem_fw(filename, flash_options) !=
			E_MIU_ERR_SUCCESS) {
			printf("error during 3G Modem flashing!\n");
		}
		miu_dispose();
	}

	ret = StringValue(strdup(""));

done:
	if (filename)
		free(filename);

	return ret;
}

static Value *FlashNvmFn(const char *name, State * state, int argc, Expr * argv[])
{
	Value *ret = NULL;
	char *filename = NULL;

	if (ReadArgs(state, argv, 1, &filename) < 0)
		return NULL;

	if (filename == NULL || strlen(filename) == 0) {
		ErrorAbort(state, "filename argument to %s can't be empty", name);
		goto done;
	}

	if (miu_initialize(miu_progress_cb, miu_log_cb, filename) != E_MIU_ERR_SUCCESS) {
		printf("%s failed at %s\n", __func__, "miu_initialize failed");
	} else {
		if (miu_flash_modem_nvm(filename, NULL) != E_MIU_ERR_SUCCESS)
			printf("error during 3G Modem NVM config!\n");
		miu_dispose();
	}

	ret = StringValue(strdup(""));

done:
	if (filename)
		free(filename);

	return ret;
}

void RegisterTelephonyFunctions(void)
{
	RegisterFunction("flash_modem", FlashModemFn);
	RegisterFunction("flash_nvm", FlashNvmFn);
}
