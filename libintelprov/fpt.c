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

#include "fpt.h"
#include "util.h"

static const unsigned int	 TIMEOUT   = 120;
static const char		*TEMP_FILE = "/tmp/temp.bin";
static const char		*LOG_FILE  = "/tmp/fpt.log";

static const char	*FPT_PROGRAM  = "/system/bin/fpttools/FPT";
static const char	*FPT_PASS_STR = "FPT Operation Passed";

#define FPT_START_OPTIONS	NULL

static int call_FPT(char *argv[])
{
	return call_program(FPT_PROGRAM, LOG_FILE, FPT_PASS_STR, TIMEOUT, argv);
}

static int flash_from_data(void *data, unsigned sz,
			   int (*do_it_from_file)(char *filename))
{
	if (file_write(TEMP_FILE, data, sz) != 0)
		return EXIT_FAILURE;

	return do_it_from_file((char *)TEMP_FILE);
}


/* Flash full ifwi.  */
int flash_fpt_file_ifwi(char *filename)
{
	return call_FPT((char *[]){ FPT_START_OPTIONS, "-Y", "-F",
				filename, NULL });
}

int flash_fpt_data_ifwi(void *data, unsigned sz)
{
	return flash_from_data(data, sz, flash_fpt_file_ifwi);
}

/* Flash TXE region only.  */
int flash_fpt_file_txe(char *filename)
{
	return call_FPT((char *[]){ FPT_START_OPTIONS, "-Y", "-F",
				filename, "-TXE", NULL });
}

int flash_fpt_data_txe(void *data, unsigned sz)
{
	return flash_from_data(data, sz, flash_fpt_file_txe);
}

/* Flash PDR region only.  */
int flash_fpt_file_pdr(char *filename)
{
	return call_FPT((char *[]){ FPT_START_OPTIONS, "-Y", "-F",
				filename, "-PDR", NULL });
}

int flash_fpt_data_pdr(void *data, unsigned sz)
{
	return flash_from_data(data, sz, flash_fpt_file_pdr);
}

/* Flash IAFW region only.  */
int flash_fpt_file_bios(char *filename)
{
	return call_FPT((char *[]){ FPT_START_OPTIONS, "-Y", "-F",
				filename, "-BIOS", NULL });
}

int flash_fpt_data_bios(void *data, unsigned sz)
{
	return flash_from_data(data, sz, flash_fpt_file_bios);
}

/* Flash a specific FPF item.  */
int fpt_writeitem(int argc, char **argv)
{
	if (argc != 3) {
		error("Usage: writefpt <name> <value>.");
		return EXIT_FAILURE;
	}
	return call_FPT((char *[]){ FPT_START_OPTIONS, "-WRITEFPF",
				argv[1], "-V", argv[2], NULL });
}

/* Flash the fpfs with values set in FILENAME.  */
int flash_fpt_file_fpfs(char *filename)
{
	return call_FPT((char *[]){ FPT_START_OPTIONS,"-WRITEFPFBATCH", 
				filename, NULL });
}

int flash_fpt_data_fpfs(void *data, unsigned sz)
{
	return flash_from_data(data, sz, flash_fpt_file_fpfs);
}

/* Flash FPF global valid bit, will lock up FPF programming.  */
int fpt_writevalidbit(int argc, char **argv)
{
	if (argc != 1) {
		error("Too many parameters.");
		return EXIT_FAILURE;
	}
	return call_FPT((char *[]){ FPT_START_OPTIONS,
				"-WRITEGLOBAL", NULL});
}

/* Close EOM, lock up SPI access.  */
int fpt_closemnf(int argc, char **argv)
{
	if (argc != 1) {
		error("Too many parameters.");
		return EXIT_FAILURE;
	}
	return call_FPT((char *[]){ FPT_START_OPTIONS,
				"-CLOSEMNF", NULL});
}
