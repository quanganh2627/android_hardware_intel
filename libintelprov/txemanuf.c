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

#include "txemanuf.h"
#include "util.h"

static const size_t	 TIMEOUT   = 120;
static const char	*TEMP_FILE = "/tmp/temp.bin";
static const char	*LOG_FILE  = "/tmp/txemanuf.log";

static const char	*TXEMANUF_PROGRAM	= "/system/bin/fpttools/TXEManuf";
static const char	*TXEMANUF_OP_PASS_STR	= "TXEManuf Operation Passed";
static const char	*TXEMANUF_TEST_PASS_STR = "TXEManuf Test Passed";

#define TXEMANUF_START_OPTIONS	NULL

static int call_TXEManuf(char *argv[], const char *pass_string)
{
	return call_program(TXEMANUF_PROGRAM, LOG_FILE, pass_string, TIMEOUT, argv);
}

static int flash_from_data(void *data, unsigned sz,
			   int (*do_it_from_file)(char *filename))
{
	if (file_write(TEMP_FILE, data, sz) != 0)
		return EXIT_FAILURE;

	return do_it_from_file((char *)TEMP_FILE);
}

/* Load TXEManuf config file.  */
int flash_txemanuf_file(char *filename)
{
	return call_TXEManuf((char *[]){ TXEMANUF_START_OPTIONS,
				"-F", filename, NULL },
			     TXEMANUF_OP_PASS_STR);
}

int flash_txemanuf_data(void *data, unsigned sz)
{
	return flash_from_data(data, sz, flash_txemanuf_file);
}

/* Run EOL test.  */
int txemanuf_eof_test(int argc, char **argv)
{
	if (argc != 1) {
		error("Too many parameters.");
		return EXIT_FAILURE;
	}
	return call_TXEManuf((char *[]){ TXEMANUF_START_OPTIONS,
				"-EOL", NULL},
			     TXEMANUF_TEST_PASS_STR);
}

/* Run BIST test.  */
int txemanuf_bist_test(int argc, char **argv)
{
	if (argc != 1) {
		error("Too many parameters.");
		return EXIT_FAILURE;
	}
	return call_TXEManuf((char *[]){ TXEMANUF_START_OPTIONS,
				"-TEST", NULL},
			     TXEMANUF_TEST_PASS_STR);
}
