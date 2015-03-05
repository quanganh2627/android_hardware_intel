/**********************************************************************
 * Copyright 2014 (c) Intel Corporation. All rights reserved.

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

#ifndef INTELKEYMASTER_FIRMWARE_API_H
#define INTELKEYMASTER_FIRMWARE_API_H

#include <stdint.h>

// for now, only KEY_TYPE_RSA is supported
typedef enum {
	KEY_TYPE_RSA = 1,
	KEY_TYPE_DSA,
	KEY_TYPE_EC
} intel_keymaster_key_type_t;

typedef struct {
	uint32_t modulus_size;	// in bits, example 2048
	uint32_t reserved;
	// the public exponent is passed in little endian format from the keymaster HAL
	// and needs to be converted to big endian for RSA mathematics in firmware
	// the leading zeros can be discarded
	uint64_t public_exponent;	// 0x10001
} intel_keymaster_rsa_keygen_params_t;

typedef enum {
	KEYMASTER_CMD_GENERATE_KEYPAIR = 1,
	KEYMASTER_CMD_IMPORT_KEYPAIR,
	KEYMASTER_CMD_GET_KEYPAIR_PUBLIC,
	KEYMASTER_CMD_DELETE_KEYPAIR,
	KEYMASTER_CMD_DELETE_ALL,
	KEYMASTER_CMD_SIGN_DATA,
	KEYMASTER_CMD_VERIFY_DATA,
	KEYMASTER_RSP_FLAG = 0x80000000	// responses are flagged with cmd_id with msb changed to 1
} intel_keymaster_cmd_t;

// this is the input for KEYMASTER_CMD_GENERATE_KEYPAIR
typedef struct {
	intel_keymaster_key_type_t key_type;
	uint8_t key_params[];	// cast to intel_keymaster_rsa_keygen_params_t if key_type == KEY_TYPE_RSA
} intel_keymaster_keygen_params_t;

// this is the output of KEYMASTER_CMD_GENERATE_KEYPAIR and KEYMASTER_CMD_IMPORT_KEYPAIR
// and input for KEYMASTER_CMD_GET_KEYPAIR_PUBLIC and KEYMASTER_CMD_DELETE_KEYPAIR
typedef struct {
	uint32_t key_blob_length;
	uint8_t key_blob[];	// opaque structure intepreted by firmware only, private key is wrapped with device specific key
} intel_keymaster_key_blob_t;

// the big numbers in the bytes[] array in the following structure are in big endian format
// private key is unencrypted
typedef struct {
	uint32_t modulus_length;	// in bytes
	uint32_t public_exponent_length;	// in bytes
	uint32_t private_exponent_length;	// in bytes
	uint8_t bytes[];	// concatenation of modulus, public exponent and private exponent
} intel_keymaster_rsa_key_t;

// this is the input for KEYMASTER_CMD_IMPORT_KEYPAIR
typedef struct {
	intel_keymaster_key_type_t key_type;
	uint8_t key_data[];	// cast to intel_keymaster_rsa_key_t if key_type == KEY_TYPE_RSA
} intel_keymaster_import_key_t;

// the big numbers in the bytes[] array in the following structure are in big endian format
typedef struct {
	uint32_t modulus_length;	// in bytes
	uint32_t public_exponent_length;	// in bytes
	uint8_t bytes[];	// concatenation of modulus and public exponent
} intel_keymaster_rsa_public_key_t;

// this is the output of KEYMASTER_CMD_GET_KEYPAIR_PUBLIC
typedef struct {
	intel_keymaster_key_type_t key_type;
	uint8_t public_key_data[];	// cast to intel_keymaster_rsa_public_key_t if key_type == KEY_TYPE_RSA
} intel_keymaster_public_key_t;

// this is the input for KEYMASTER_CMD_SIGN_DATA
typedef struct {
	uint32_t key_blob_length;
	uint32_t data_length;	// because of no hashing and no padding, data size must be equal to modulus size
	uint8_t buffer[];	// concatenation of key blob and data block to be signed
} intel_keymaster_signing_cmd_data_t;

// this is the output of KEYMASTER_CMD_SIGN_DATA
typedef struct {
	uint32_t signature_length;
	uint8_t signature[];
} intel_keymaster_signature_t;

// this is the input for KEYMASTER_CMD_VERIFY_DATA
typedef struct {
	uint32_t key_blob_length;
	uint32_t data_length;	// because of no hashing and no padding, data size must be equal to modulus size
	uint32_t signature_length;	// because of no hashing and no padding, signature size must be equal to modulus size
	uint8_t buffer[];	// concatenation of key blob, data block and signature block
} intel_keymaster_verification_data_t;

typedef enum {
	KEYMASTER_RESULT_SUCCESS = 0,
	KEYMASTER_RESULT_INVALID_INPUT = 1,
	KEYMASTER_RESULT_NOT_SUPPORTED = 2,
	KEYMASTER_RESULT_FAILURE = 0xFFFFFFFF
} intel_keymaster_result_t;

// this is the command structure, it follows any platform specific message header(s) if exist
typedef struct {
	intel_keymaster_cmd_t cmd_id;
	uint32_t cmd_data_length;	// this does not include the size of cmd_id and cmd_data_length itself
	uint8_t cmd_data[];
} intel_keymaster_firmware_cmd_t;

// this is the command response, it follows any platform specific message header(s) if exist
typedef struct {
	intel_keymaster_cmd_t rsp_id;	// cmd_id | KEYMASTER_RSP_FLAG
	uint32_t rsp_result_and_data_length;	// this does not include the size of rsp_id and rsp_result_and_data_length itself, but it does include the size of result field
	intel_keymaster_result_t result;
	uint8_t rsp_data[];
} intel_keymaster_firmware_rsp_t;

#define INTEL_KEYMASTER_MAX_MESSAGE_SIZE    4000	// less platform specific headers

#endif				/* INTELKEYMASTER_FIRMWARE_API_H */
