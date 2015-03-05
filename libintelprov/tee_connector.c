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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tee_connector.h"
#include "util.h"

/* Internal representation of an item.  */
struct data_items {
	const char *name;
	const uint32_t subgroup_id;
	const uint32_t item_id;
	const size_t size;
};

static const uint32_t PAYLOAD_MAX_SIZE = 32;
static const uint32_t PRINT_BUFFER_SIZE = 1024;

/* Datagroups and item IDs.  */
static const uint32_t SPID_DATAGROUP = 1;
static const uint32_t SERIAL_NUMBER_DATAGROUP = 5;
static const uint32_t FRU_DATAGROUP = 6;

static const struct data_items SPID_ITEMS[] = { { "Customer ID",            11, 1, 2 },
						{ "Vendor ID",              12, 1, 2 },
						{ "Device Manufacturer ID", 12, 2, 2 },
						{ "Platform Family ID",     10, 1, 2 },
						{ "Product Line ID",        10, 2, 2 },
						{ "Hardware Model ID",      10, 3, 2 } };

static const struct data_items ssn_item = { "System Serial Number", 11, 1, 32 };

/* Dummy print function. Should not be used.  */
static void default_print(const char *msg)
{
	fprintf(stdout, "%s\n", msg);
}

/* Dummy error function. Should not be used.  */
static void default_error(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
}

/* Set to output functions to dummy function.  */
void (*print_fun)(const char *msg) = default_print;
void (*error_fun)(const char *msg) = default_error;

#define ERROR_MSG_LENGTH 256

void raise_error(const char *fmt, ...)
{
	char buf[ERROR_MSG_LENGTH];

	va_list argptr;
	va_start(argptr, fmt);
	vsnprintf(buf, sizeof(buf), fmt, argptr);
	va_end(argptr);

	error_fun(buf);
}

/* When set, output will be sent to this file descriptor.  */
static int output_fd = -1;

static void output_data(uint8_t *data, size_t size)
{
	size_t i;

	if (output_fd != -1)
		for (i = 0 ; i < size ; i++)
			write(output_fd, data + i, sizeof(uint8_t));
	else
		hexdump_buffer(data, size, print_fun, size);
}

int set_output_file(const char *path)
{
	output_fd = creat(path, S_IWUSR);
	if (output_fd == -1)
	{
		raise_error("Failed to open output file, error: %s", strerror(errno));
		return EXIT_FAILURE;
	}
	return 0;
}

void close_output_file_when_open()
{
	int ret;

	if (output_fd == -1)
		return;

	ret = close(output_fd);
	if (ret)
		raise_error("Failed to close the output file, error: %s", strerror(errno));

	output_fd = -1;
}

static int parse_datagroup_id(char *arg)
{
	int ret;
	char *end;

	errno = 0;
	ret = strtoul(arg, &end, 10);
	if (*end != '\0' || errno == ERANGE)
	{
		raise_error("Invalid datagroup ID, error: %s", strerror(EINVAL));
		return -1;
	}
	return ret;
}

static int parse_token(int dg, int flags)
{
	uint32_t print_pos = 0;
	uint32_t *sg_list = NULL;
	uint32_t *item_list = NULL;
	size_t sg_count = 0;
	uint8_t print_data[PRINT_BUFFER_SIZE];

	int ret = tee_token_sgids_get(dg, &sg_list, &sg_count, flags);
	if (ret != 0)
		goto end;

	size_t s;
	for (s = 0; s < sg_count; s++)
	{
		size_t it;
		size_t item_count = 0;

		ret = tee_token_itemids_get(dg, sg_list[s], &item_list, &item_count, flags);
		if (ret != 0)
		{
			raise_error("tee_token_itemids_get call failed, return 0x%x\n", ret);
			goto error;
		}

		for (it = 0; it < item_count; it++)
		{
			size_t payload_size = 0;
			uint8_t payload[PAYLOAD_MAX_SIZE];

			memset(payload, 0, PAYLOAD_MAX_SIZE);

			ret = tee_token_item_size_get(dg, sg_list[s], item_list[it], &payload_size, flags);
			if (ret != 0)
			{
				raise_error("tee_token_item_size_get call failed, return 0x%x\n", ret);
				free(item_list);
				goto error;
			}

			if (payload_size > PAYLOAD_MAX_SIZE)
			{
				raise_error("Returned payload size ( %u ) is more than max size ( %u )\n",
					payload_size, PAYLOAD_MAX_SIZE);
				free(item_list);
				ret = 1;
				goto error;
			}

			ret = tee_token_item_read(dg, sg_list[s], item_list[it], 0, payload, payload_size, flags);
			if (ret != 0)
			{
				raise_error("tee_token_item_read call failed, return 0x%x\n", ret);
				free(item_list);
				goto error;
			}

			if (print_pos + payload_size >= PRINT_BUFFER_SIZE)
			{
				raise_error("Print buffer full\n");
				free(item_list);
				ret = 1;
				goto error;
			}

			memcpy(print_data + print_pos, payload, payload_size);
			print_pos += payload_size;
		}

		free(item_list);
	}

error:
	free(sg_list);

end:
	if (print_pos > 0)
		output_data(print_data, print_pos);

	return ret;
}

int get_lifetime(int argc, char **argv)
{
	int ret;
	uint32_t timestamp;
	uint8_t nonce[TOKEN_NONCE_LENGTH];
	uint8_t mac[TOKEN_MAC_LENGTH];

	ret = tee_token_lifetimedata_get(&timestamp, nonce, mac);
	if (ret != 0)
	{
		raise_error("tee_token_lifetimedata_get call failed, return=0x%x", ret);
		return ret;
	}

	output_data((uint8_t *)&timestamp, sizeof(timestamp));
	output_data(nonce, sizeof(nonce));
	output_data(mac, sizeof(mac));

	return ret;
}

int get_ssn(int argc, char **argv)
{
	int ret;
	uint8_t data[ssn_item.size];

	ret = tee_token_item_read(SERIAL_NUMBER_DATAGROUP, ssn_item.subgroup_id,
				ssn_item.item_id, 0, data, ssn_item.size, 0);
	if (ret != 0)
		raise_error("tee_token_item_read() call failed, return=0x%x", ret);
	else
		output_data(data, ssn_item.size);

	return ret;
}

int start_update(int argc, char **argv)
{
	int ret;

	ret = tee_token_update_start(0);
	if (ret != 0)
		raise_error("tee_token_update_start() call failed, return=0x%x", ret);

	return ret;
}

int cancel_update(int argc, char **argv)
{
	int ret;

	ret = tee_token_update_cancel(0);
	if (ret != 0)
		raise_error("tee_token_update_cancel() call failed, return=0x%x", ret);

	return ret;
}

int finalize_update(int argc, char **argv)
{
	int ret;

	ret = tee_token_update_end(0);
	if (ret != 0)
		raise_error("tee_token_update_end() call failed, return=0x%x", ret);

	return ret;
}

int get_spid(int argc, char **argv)
{
	int ret;
	size_t i, payload_size = 0;

	for (i = 0 ; i < sizeof(SPID_ITEMS) / sizeof(struct data_items) ; i++)
		payload_size += SPID_ITEMS[i].size;

	uint8_t *payload = (uint8_t *)malloc(payload_size * sizeof(uint8_t));
	if (!payload)
	{
		raise_error("Failed to allocate payload buffer, error: %s", strerror(ENOMEM));
		return EXIT_FAILURE;
	}
	uint8_t *tmp = payload;

	for (i = 0 ; i < sizeof(SPID_ITEMS) / sizeof(struct data_items) ; i++)
	{
		ret = tee_token_item_read(SPID_DATAGROUP, SPID_ITEMS[i].subgroup_id,
					  SPID_ITEMS[i].item_id, 0, tmp,
					  SPID_ITEMS[i].size, 0);
		if (ret != 0)
		{
			raise_error("tee_token_item_read() call failed, return=0x%x", ret);
			goto exit;
		}

		tmp += SPID_ITEMS[i].size;
	}

	output_data(payload, payload_size);

exit:
	free(payload);
	return ret;
}

int get_fru(int argc, char **argv)
{
	return parse_token(FRU_DATAGROUP, 0);
}

int get_part_id (int argc, char **argv)
{
	int ret;
	uint8_t partid_buf[TOKEN_PSID_LENGTH];

	ret = tee_partid_get(partid_buf);
	if (ret != 0)
		raise_error("tee_partid_get() call failed, return=0x%x", ret);
	else
		output_data(partid_buf, sizeof(partid_buf));

	return ret;
}

int write_token(void *data, size_t size)
{
	int ret;

	ret = tee_token_write(data, size, 0);
	if (ret != 0)
		raise_error("tee_token_write() call failed, return=0x%x", ret);

	return ret;
}

int read_token(int argc, char **argv)
{
	uint8_t *buf;
	int ret, datagroup_id;
	struct tee_token_info info;

	if (argc != 2)
	{
		raise_error("datagroup_id argument is missing");
		return EXIT_FAILURE;
	}

	datagroup_id = parse_datagroup_id(argv[1]);
	if (datagroup_id == -1)
		return EXIT_FAILURE;

	bzero(&info, sizeof(info));
	ret = tee_token_info_get(datagroup_id, &info, 0);
	if (ret != 0)
	{
		raise_error("tee_token_info_get() call failed, return=0x%x", ret);
		return ret;
	}

	if (info.lifetime.token_size == 0)
	{
		raise_error("Failed, token has been provided with the old format...");
		return EXIT_FAILURE;
	}

	buf = (uint8_t *)malloc(info.lifetime.token_size * sizeof(uint32_t));
	if (buf == NULL)
	{
		raise_error("Failed to alloc the buffer to retrieve the token data, error=%s", strerror(ENOMEM));
		return EXIT_FAILURE;
	}

	ret = tee_token_read(datagroup_id, buf, info.lifetime.token_size * sizeof(uint32_t), 0);
	if (ret != 0)
		raise_error("tee_token_read() call failed, return=0x%x", ret);
	else
		output_data(buf, info.lifetime.token_size * sizeof(uint32_t));

	free(buf);

	return EXIT_SUCCESS;
}

int read_token_payload(int argc, char **argv)
{
	int datagroup_id;

	if (argc != 2)
	{
		raise_error("datagroup_id argument is missing");
		return EXIT_FAILURE;
	}

	datagroup_id = parse_datagroup_id(argv[1]);
	if (datagroup_id == -1)
		return EXIT_FAILURE;

	return parse_token(datagroup_id, 0);
}

int remove_token(int argc, char **argv)
{
	int ret, datagroup_id;

	if (argc != 2)
	{
		raise_error("datagroup_id argument is missing");
		return EXIT_FAILURE;
	}

	datagroup_id = parse_datagroup_id(argv[1]);
	if (datagroup_id == -1)
		return EXIT_FAILURE;

	ret = tee_token_remove(datagroup_id, 0);
	if (ret != 0)
		raise_error("tee_token_remove() call failed, return=0x%x", ret);

	return ret;
}

int send_cryptid_request(void *data, size_t size)
{
	int ret;
	size_t response_size;
	uint8_t *response;

	ret = tee_token_cryptid_request(size, data, &response_size, &response);

	if (ret != 0)
		raise_error("tee_token_cryptid_request() call failed, return=0x%x", ret);
	else {
		output_data(response, response_size);
		free(response);
	}

	return ret;
}

int generate_shared_ecc(int argc, char **argv)
{
	int ret;
	size_t pub_key_size;
	uint8_t *pub_key = NULL;

	ret = tee_token_gen_shared_ecc(&pub_key_size, &pub_key);

	if (ret != 0)
		raise_error("tee_token_gen_shared_ecc() call failed, return=0x%x", ret);
	else {
		output_data(pub_key, pub_key_size);
		free(pub_key);
	}

	return ret;
}

int generate_shared_rsa(int argc, char **argv)
{
	int ret;
	size_t pub_key_size, prv_key_size;
	uint8_t *pub_key = NULL;
	uint8_t *prv_key = NULL;

	if (argc != 3) {
		raise_error("file name argument is missing");
		return EXIT_FAILURE;
	}

	ret = tee_token_gen_shared_rsa(&pub_key_size, &pub_key, &prv_key_size, &prv_key);

	if (ret != 0) {
		raise_error("tee_token_gen_shared_rsa() call failed, return=0x%x", ret);
		goto fail_gen;
	}
	else {
		if (file_write(argv[1], pub_key, pub_key_size) < 0) {
			raise_error("Failed to write pub key output file %s, error: %s", argv[1],  strerror(errno));
			ret = EXIT_FAILURE;
			goto fail_file;
		}
		if (file_write(argv[2], prv_key, prv_key_size) < 0) {
			raise_error("Failed to write prv key output file %s, error: %s", argv[2],  strerror(errno));
			ret = EXIT_FAILURE;
			goto fail_file;
		}
	}

fail_file:
	free(pub_key);
	free(prv_key);
fail_gen:

	return ret;
}
