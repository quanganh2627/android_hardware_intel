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



#ifndef __CHAABI_ERROR_CODES_H__
#define __CHAABI_ERROR_CODES_H__



//
// Base Chaabi EXTFW error codes.
// IMPORTANT: These values *must* match the corresponding macro values in the
// Chaabi EXTFW chaabi_error.h header file!
//
#define CHAABI_ERROR_BASE 0xA0B000
#define SECURE_TOKEN_BASE ( CHAABI_ERROR_BASE + 0x300 )
#define PMDB_ERROR_BASE   ( CHAABI_ERROR_BASE + 0x400 )


//
// Secure token error codes (maximum of 256).
//
typedef enum
{
	ST_SUCCESSFUL = 0,
	ST_FAIL_UNSUPPORTED = ( SECURE_TOKEN_BASE + 1 ),                // 0xA0B301
	ST_FAIL_INVALID_PARAMETER,
	ST_FAIL_SEP_DRIVER_OP,
	ST_FAIL_UNIQUE_KEY_CREATION_FAILED,
	ST_FAIL_UNIQUE_KEY_SIZE_MISMATCH,
	ST_FAIL_UNIQUE_KEY_VALIDATION_FAILED,
	ST_FAIL_SECURE_TOKEN_VALIDATION_FAILED,
	ST_FAIL_UNKNOWN_OPERATION_TYPE,
	ST_FAIL_CLEAR_ACD_FAILED,
	ST_FAIL_CUSTOMER_ID_NULL,                                       // 10
	ST_FAIL_UMIP_READ_FAILURE,
	ST_FAIL_UMIP_WRITE_FAILURE,
	ST_FAIL_SECURE_TOKEN_DATA_TABLE_HMAC_FAILURE,
	ST_FAIL_SECURE_TOKEN_DATA_TABLE_IS_FULL,
	ST_FAIL_SECURE_TOKEN_DATA_TABLE_INTEGRITY_FAILURE,
	ST_FAIL_SECURE_TOKEN_NONCE_NOT_IN_DATA_TABLE_FAILURE,
	ST_FAIL_SECURE_TOKEN_NONCE_ALREADY_IN_DATA_TABLE_FAILURE,
	ST_FAIL_PERSISTENT_SECURE_TOKEN_ALREADY_IN_DATA_TABLE_FAILURE,
	ST_FAIL_PERSISTENT_SECURE_TOKEN_NOT_IN_DATA_TABLE_FAILURE,
	ST_FAIL_SECURE_TOKEN_SIZE_MISMATCH_FAILURE,                     // 20
	ST_FAIL_SECURE_TOKEN_SIZE_TOO_SMALL_FAILURE,
	ST_FAIL_SECURE_TOKEN_SIZE_TOO_LARGE_FAILURE,
	ST_FAIL_NO_SECURE_TOKEN_SIGNATURE_FAILURE,
	ST_FAIL_SECURE_REALTIME_CLOCK_FAILURE,
	ST_FAIL_SECURE_REALTIME_CLOCK_RESET_FAILURE,
	ST_FAIL_SECURE_TOKEN_LIFETIME_FAILURE,
	ST_FAIL_SECURE_TOKEN_LIFETIME_EXPIRED_FAILURE,
	ST_FAIL_RPMB_READ_FAILURE,
	ST_FAIL_RPMB_WRITE_FAILURE,
	ST_FAIL_ILLEGAL_SECURE_TOKEN_OPERATION_TYPE_FAILURE,            // 30
} ST_RESULT;


//
// Platform Management Database (PMDB) error codes (maximum of 256).
//
typedef enum
{
	PMDB_SUCCESSFUL = 0,
	PMDB_FAIL_UNSUPPORTED = ( PMDB_ERROR_BASE + 1 ),                // 0xA0B401
	PMDB_FAIL_INVALID_PARAMETER,
	PMDB_FAIL_SEP_DRIVER_OP,
	PMDB_FAIL_DATA_HMAC_FAILURE,
	PMDB_FAIL_STORAGE_READ_FAILURE,
	PMDB_FAIL_STORAGE_WRITE_FAILURE,
	PMDB_FAIL_WRITES_PROHIBITED_FAILURE,
	PMDB_FAIL_DATA_INTEGRITY_FAILURE,
	PMDB_FAIL_WRITE_ONCE_AREA_LOCK_FAILURE,
	PMDB_FAIL_WRITE_ONCE_AREA_LOCKED_FAILURE,                       // 10
	PMDB_FAIL_WRITE_ONCE_AREA_LOCKED_UNKNOWN_FAILURE,
	PMDB_FAIL_WRITE_ONCE_AREA_UNLOCK_FAILURE,
	PMDB_FAIL_DATA_SIZE_TOO_SMALL_FAILURE,
	PMDB_FAIL_DATA_SIZE_TOO_LARGE_FAILURE,
	PMDB_FAIL_DATA_SIZE_MISMATCH_FAILURE,
} pmdb_result_t;


#endif /* __CHAABI_ERROR_CODES_H__ */

