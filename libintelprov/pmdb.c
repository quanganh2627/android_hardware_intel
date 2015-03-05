/*
 * Copyright 2011 Intel Corporation
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
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "pmdb.h"
#include "pmdb-access.h"
#include "util.h"
#include "droidboot_ui.h"
#ifdef TEE_FRAMEWORK
#include <tee_token_error.h>
#include <tee_token_if.h>
#endif	/* TEE_FRAMEWORK */

#define min(a,b) (((a) < (b)) ? (a) : (b))

#define PMDB_FRU_OFFSET		0x00
#define CHECKSUM_SIZE		4

#ifndef TEE_FRAMEWORK

struct pmdb_field_s {
	const char *name;
	enum pmdb_database db;
	unsigned int offset;
	size_t size;
};

static struct pmdb_field_s pmdb_fields[] = {
	{ "fru",	WM,	PMDB_FRU_OFFSET,	PMDB_FRU_SIZE },
	{ "fru+cs",	WM,	PMDB_FRU_OFFSET,	PMDB_FRU_SIZE + CHECKSUM_SIZE },
};

static struct pmdb_field_s *pmdb_field(const char *name)
{
	unsigned int i;

	for (i = 0; i < sizeof(pmdb_fields) / sizeof(*pmdb_fields); i++) {
		if (!strcmp(pmdb_fields[i].name,name))
			return &pmdb_fields[i];
	}
	return 0;
}

static int pmdb_write_field(const char *name, unsigned char *buf, size_t size)
{
	int ret = -4;

	struct pmdb_field_s *field = pmdb_field(name);
	if (!field)
		goto done;


	if (size != field->size) {
		pr_error("invalid %s size", field->name);
		ret = -3;
		goto done;
	}

	ret = pmdb_access_write(buf, field->db, field->offset, size);

done:
	return ret;
}
#endif	/* TEE_FRAMEWORK */

int pmdb_write_fru(void *buf, unsigned size)
{
#ifdef TEE_FRAMEWORK
	int ret;
	struct FRU_RAW field_rep_unit = { FRU_RAW_TAG, { 0 } };
	memcpy(field_rep_unit.fru_value,buf,min(size,sizeof(field_rep_unit.fru_value)));

	ret = tee_token_write_tmp((uint8_t *)&field_rep_unit, sizeof(struct FRU_RAW), 0);
	fprintf(stderr, "Calling token_write -- ret = %d\n", ret);

	return (0 != ret);
#else
	unsigned char with_cs[PMDB_FRU_SIZE + CHECKSUM_SIZE];

	memset(with_cs, 0, sizeof(with_cs));
	memcpy(with_cs, buf, min(size, PMDB_FRU_SIZE));
	twoscomplement(&with_cs[PMDB_FRU_SIZE], with_cs, PMDB_FRU_SIZE);

	return pmdb_write_field("fru+cs", with_cs, sizeof(with_cs));
#endif	/* TEE_FRAMEWORK */
}
