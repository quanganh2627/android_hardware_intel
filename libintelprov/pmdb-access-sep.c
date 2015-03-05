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
#include "pmdb.h"
#include "pmdb-access.h"
#include <chaabi/SepMW/VOS6/External/Linux/inc/DxTypes.h>
#include <chaabi/SepMW/VOS6/External/VOS_API/DX_VOS_BaseTypes.h>
#include <chaabi/SepMW/INIT/inc/Init_CC.h>
#include <chaabi/secure_token.h>
#include <chaabi/umip_access.h>
#include <string.h>

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

#define MAX_BUF_SIZE (max(SEP_PMDB_WRITE_ONCE_SIZE, SEP_PMDB_WRITE_MANY_SIZE))

static size_t pmdb_area_size(sep_pmdb_area_t type)
{
	size_t size = 0;
	switch(type) {
		case SEP_PMDB_WRITE_ONCE:
			size = SEP_PMDB_WRITE_ONCE_SIZE;
			break;
		case SEP_PMDB_WRITE_MANY:
			size = SEP_PMDB_WRITE_MANY_SIZE;
			break;
	}
	return size;
}

static int pmdb_read_modify_write(sep_pmdb_area_t type, uint8_t data[],
		size_t size, unsigned int offset)
{
	uint8_t buffer[MAX_BUF_SIZE];
	int ret = -1;

	pmdb_result_t res;
	if(offset + size > pmdb_area_size(type))
		goto done;

	res = sep_pmdb_read(type, buffer, pmdb_area_size(type));
	if (PMDB_SUCCESSFUL != res)
		goto done;

	memcpy(buffer+offset,data,size);
	res = sep_pmdb_write(type,buffer,pmdb_area_size(type));
	if (PMDB_SUCCESSFUL != res)
		goto done;

	ret = 0;
done:
	return ret;
}

int pmdb_access_write(unsigned char *buf, enum pmdb_database db,
		unsigned int offset, size_t size)
{
	return pmdb_read_modify_write((WO == db) ? SEP_PMDB_WRITE_ONCE:
			SEP_PMDB_WRITE_MANY, buf, size, offset);
}
