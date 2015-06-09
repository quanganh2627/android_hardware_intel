/**********************************************************************
 * Copyright (C) 2011 Intel Corporation. All rights reserved.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **********************************************************************/

#ifndef __TEE_ERROR_H__
#define __TEE_ERROR_H__


enum {
	TEE_SUCCESSFUL	= 0,
	TEE_FAILURE = 0xFFFFFFFF,

	/*! TEE PARAM errors */
	TEE_ERR_BASE				= 0x10000000,
	TEE_FAIL_INVALID_PARAM,
	TEE_FAIL_NULL_PARAM,
	TEE_FAIL_BUFFER_TOO_SMALL,
	TEE_FAIL_UNSUPPORTED,
	TEE_FAIL_UNKNOWN_ERROR,
	TEE_FAIL_INVALID_SESSION,
	TEE_FAIL_FW_SESSION,
	TEE_FAIL_AUDIO_DATA_NOT_VALID,
	TEE_FAIL_INVALID_TEE_PARAM,
	TEE_FAIL_KEYBOX_INVALID_BAD_MAGIC,
	TEE_FAIL_KEYBOX_INVALID_BAD_CRC,
	TEE_FAIL_KEYBOX_INVALID_BAD_PROVISIONING,
	TEE_FAIL_KEYBOX_INVALID_NOT_PROVISIONED,
	TEE_FAIL_INPUT_BUFFER_TOO_SHORT,
	TEE_FAIL_AES_DECRYPT,
	TEE_FAIL_AES_FAILURE,
	TEE_FAIL_WV_NO_ASSET_KEY,
	TEE_FAIL_WV_NO_CEK,
	TEE_FAIL_SCHEDULER,
	TEE_FAIL_SESSION_NOT_INITIALIZED,
	TEE_FAIL_REPROVISION_IED_KEY,
	TEE_FAIL_NALU_DATA_EXCEEDS_BUFFER_SIZE,
	TEE_FAIL_WV_SESSION_NALU_PARSE_FAILURE,
	TEE_FAIL_SESSION_KEY_GEN,
	TEE_FAIL_INVALID_PLATFORM_ID,
	TEE_FAIL_INVALID_MAJOR_VERSION,
	TEE_FAIL_GENERATE_RANDOM_NUMBER_FAILURE,
	TEE_FAIL_DX_CCLIB_INIT_FAILURE,

	TEE_ERR_LAST,
	TEE_ERR_NUM_ERRORS			= TEE_ERR_LAST - TEE_ERR_BASE
};


#endif // TEE_error.h 
