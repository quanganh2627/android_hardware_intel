/*
 * Copyright 2011-2013 Intel Corporation
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
#include <droidboot.h>
#include <droidboot_plugin.h>
#include <droidboot_util.h>
#include <chaabi/SepMW/VOS6/External/Linux/inc/DxTypes.h>
#include <chaabi/SepMW/VOS6/External/VOS_API/DX_VOS_BaseTypes.h>
#include <chaabi/SepMW/INIT/inc/Init_CC.h>
#include <chaabi/secure_token.h>
#include "token.h"
#include <unistd.h>
#include "util.h"
#include "fastboot.h"
#ifdef TEE_FRAMEWORK
#include "tee_connector.h"
#endif	/* TEE_FRAMEWORK */

#ifndef TEE_FRAMEWORK
static int oem_uniqueid_handler(int argc, char **argv)
{
	int retval = -1;
	ST_RESULT result;
	uint8_t uniqueKey[ SECURE_TOKEN_UNIQUE_KEY_SIZE_IN_BYTES ];
	char hexuniqueKey[ SECURE_TOKEN_UNIQUE_KEY_SIZE_IN_BYTES * 3 + 2 ];

	DX_CC_HostInit();
	result = sep_sectoken_request_token(uniqueKey);
	pr_info("sep_sectoken_request_token() == 0x%x\n", result);
	if(ST_FAIL_SEP_DRIVER_OP == result)
		pr_info("sep_sectoken_request_token() =="
				"ST_FAIL_SEP_DRIVER_OP\n");
	retval = ( result != ST_SUCCESSFUL );

	if (retval != 0)
		fastboot_fail("cannot get uniqueid");
	else {
		snhexdump(hexuniqueKey, sizeof(hexuniqueKey)-1, uniqueKey,
				SECURE_TOKEN_UNIQUE_KEY_SIZE_IN_BYTES);
		pr_info("%s\n",hexuniqueKey);
		hexdump_buffer(uniqueKey,
				SECURE_TOKEN_UNIQUE_KEY_SIZE_IN_BYTES,
				fastboot_info,16);
	}
	fastboot_okay("");
	DX_CC_HostFinish();
	return retval;
}

static int flash_token(void *data, unsigned sz)
{
	int retval = -1;
	ST_RESULT result;

	DX_CC_HostInit();

	result = sep_sectoken_consume_token(data,sz);
	pr_info("sep_sectoken_consume_token() == 0x%x\n",result);

	/* if the file is too small, it assumes that this is the
		dummy file sent by the Phone Flash Tool.
		So, it returns a success result to continue with the
		fastboot command that sends the FRU */
	if(result == ST_FAIL_SECURE_TOKEN_SIZE_TOO_SMALL_FAILURE)
	{
		pr_info("Token is too small. Assuming it is a dummy token "
				"for development, so return successful status.\n");
		result = ST_SUCCESSFUL;
	}

	retval = ( result != ST_SUCCESSFUL );
	DX_CC_HostFinish();
	return retval;
}
#endif	/* TEE_FRAMEWORK */

int libintel_droidboot_token_init(void)
{
	int ret = 0;

#ifndef TEE_FRAMEWORK
	ret |= aboot_register_flash_cmd("token", flash_token);
	ret |= aboot_register_oem_cmd("uniqueid", oem_uniqueid_handler);
#endif	/* TEE_FRAMEWORK */

	return ret;
}
