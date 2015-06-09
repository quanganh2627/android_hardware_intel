/**********************************************************************
 * Copyright 2009 - 2012 (c) Intel Corporation. All rights reserved.

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


#ifndef __UMIP_ACCESS_H__
#define __UMIP_ACCESS_H__

#include <inttypes.h>
#include "acd_reference.h"
#include "chaabi_error_codes.h"
#include "txei.h"

#ifndef LEGACY_ACD_CODE
#include "acds_module_error.h"
#endif

/*
 * Android Customer Data Widevine keybox provisioning schema values.
 */
#define ACD_PROV_WV_KEYBOX_0 0


/*
 * Size of AES-128 key in bytes
 */
#define AES_128_KEY_LEN_BYTES 16


/*
 * Widevine input/output structures and definitions
 * (for provision_keys())
 */
#define KEYBOX_DEVID_SIZE	(32) 	// 32 bytes
#define KEYBOX_DEVKEY_SIZE	(16) 	// 16 bytes
#define KEYBOX_KEYDATA_SIZE 	(72)	// 72 bytes
#define KEYBOX_MAGIC_SIZE	(4) 	// 4 bytes
#define KEYBOX_CRC_SIZE		(4) 	// 4 bytes
#define WIDEVINE_KEYBOX_SIZE	(128)	// 128 bytes
#define WIDEVINE_PROVISION_SIGNATURE_SIZE (20)	// 20 bytes
#define WIDEVINE_PROVISION_AES_KEYSIZE (16)	// 16 bytes

#define CUSTID_SIZE	16
#define PUBKEY_SIZE	256
#define IMEI_SIZE	15
#define CHALLENGE_SIZE	4
#define STAGE_1_RESP_SIZE PUBKEY_SIZE
#define RESP_SIZE	(AES_128_KEY_LEN_BYTES + CHALLENGE_SIZE + KEYBOX_DEVID_SIZE)
#define KEYBLOB_SIZE	(WIDEVINE_KEYBOX_SIZE + WIDEVINE_PROVISION_SIGNATURE_SIZE)


/*
 * RSA public key exponent size in bytes.
 */
#define RSA_PUBLIC_KEY_EXPONENT_SIZE_IN_BYTES 4


/*
 * RSA public key modulus size in bytes.
 */
#define RSA_PUBLIC_KEY_MODULUS_SIZE_IN_BYTES 256


/*
 * Maximum RSA public key encryption plaintext input buffer size in bytes.
 */
#define MAX_RSA_PLAINTEXT_INPUT_SIZE_IN_BYTES ( RSA_PUBLIC_KEY_MODULUS_SIZE_IN_BYTES - 11 )


/*
 * Minimum RSA public key encryption ciphertext output buffer size in bytes.
 */
#define MIN_RSA_CIPHERTEXT_OUTPUT_SIZE_IN_BYTES RSA_PUBLIC_KEY_MODULUS_SIZE_IN_BYTES




typedef struct {
	uint32_t prov_stage;
	union {
		struct {
			uint8_t  customer_id[CUSTID_SIZE];
			uint8_t  rsa_wv_mod[RSA_PUBLIC_KEY_MODULUS_SIZE_IN_BYTES];
			uint8_t  rsa_wv_exp[RSA_PUBLIC_KEY_EXPONENT_SIZE_IN_BYTES];
			uint32_t challenge;
			uint8_t  device_imei[IMEI_SIZE];
			uint8_t  pad;
		} stage_1;
		struct {
			uint8_t key_blob[KEYBLOB_SIZE];
			uint8_t iv[AES_128_KEY_LEN_BYTES];
		} stage_2;
	} prov_data;
} acd_prov_wv_indata_t;


 /*
  * Output data type for Android Customer Data provisioning of Widevine keybox
  * for a device.
  *
  * The keybox_deviceID member is the Widevine keybox Device ID C-style
  * character string, including the terminating null character.
  */
typedef struct {
	union {
		struct {
			uint8_t response[STAGE_1_RESP_SIZE];
			uint8_t iv[AES_128_KEY_LEN_BYTES];
		} stage_1;
	} prov_data;
} acd_prov_wv_outdata_t;

#define acd_prov_wv_outdata_t_INIT { { { { 0 }, { 0 } } } }

typedef enum {
	WV_PROV_STAGE1 = 1,
	WV_PROV_STAGE2,
} acd_prov_wv_stage_t;


//
// Maximum number of bytes that is stored in each Platform Management Database
// (PMDB) area in storage.
//
#define MAX_PMDB_AREA_SIZE_IN_BYTES 256


//
// Size in bytes of the PMDB write-many and write-once areas.
//
#define SEP_PMDB_WRITE_MANY_SIZE ( MAX_PMDB_AREA_SIZE_IN_BYTES )
#define SEP_PMDB_WRITE_ONCE_SIZE ( MAX_PMDB_AREA_SIZE_IN_BYTES )


//
// Platform Management Database (PMDB) areas in storage. This data type matches
// the the one used in the Chaabi EXTFW.
//
typedef enum
{
	SEP_PMDB_WRITE_ONCE  = 0,
	SEP_PMDB_WRITE_MANY,
} sep_pmdb_area_t;

int acd_init(const GUID *guid, void **ptrHandle);
int acd_deinit(void *handle);

/**
 * lock_customer_data
 * @returns: 0 on success; negative value on failure
 */
int lock_customer_data();

#ifdef ACD_WIPE_TEST
/**
 * wipe_customer_data
 * @returns: 0 on success; negative value on failure
 */
int wipe_customer_data();
#endif

/**
 * get_customer_data
 * @uiFieldIndex: Field index data to read
 * @pvAdcFieldData: Pointer to the return field index data
 * @return Number of bytes of field index data read
 * @return <0 Error occurred (negative value)
 *
 * If the field index has data to return then this function will allocate memory
 * and write the field index data into it as a byte array. The pvAdcFieldData
 * pointer will point to the memory buffer. It is the caller's responsibility to
 * free the memory after use.
 *
 * Returns the number of bytes read, which is also the size of the memory buffer
 * pointed to by pvAdcFieldData, or a negative value if an error occurred.
 */
int get_customer_data(const uint8_t uiFieldIndex, void ** const pvAdcFieldData);

/**
 * set_customer_data
 * @uiFieldIndex: which field you are referring to
 * @FieldSize: size of data field
 * @FieldMaxSize: maximum size of data field
 * @pvAdcFieldData: pointer to array for data (this must
 *	be size appropriate for field (please refer to
 *	size table below)
 * @returns: 0 on success; negative value on failure
 */
int set_customer_data(
	const uint8_t uiFieldIndex,
	const uint16_t FieldSize,
	const uint16_t FieldMaxSize,
	const void * const pvAdcFieldData);


/**
 * @brief Provisions the ACD with a key
 * @param provisionSchema Provisioning schema to use
 * @param inDataSize Input data size in bytes
 * @param pInData Pointer to a buffer for the input data
 * @param pOutDataSize Pointer to a buffer for output data size in bytes
 * @param pOutData Pointer to a buffer for the output data
 * @return 0 Provisioning was successful
 * @return <0 Provisioning was not successful
 *
 * NOTE: This function will allocate memory for the output data buffer pOutData
 * of pOutDataSize bytes. If pOutDataSize is zero then pOutData will be a NULL
 * pointer. It is the caller's responsibility to deallocate the buffer.
 */
int provision_customer_data( const uint32_t provisionSchema,
                             const uint32_t inDataSize,
                             const void * const pInData,
                             uint32_t * const pOutDataSize,
                             void ** const pOutData );


/*
 * @brief Locks to the Platform Management Database write-once area in storage.
 * @return PMDB_SUCCESSFUL Locking the PMDB write-once area in storage succeeded.
 *
 * A secure token must be used to allow writing to the PMDB areas in storage
 * before this API can be used.
 */
pmdb_result_t sep_pmdb_lock( void );


/*
 * @brief Reads data from the specified Platform Management Database area in
 *        storage and copies it to a buffer.
 * @param[in] pmdbArea The PMDB area in storage to read the data from.
 * @param[out] pData Pointer to a buffer to copy the data to.
 * @param[in] dataSizeInBytes Number of bytes to read.
 * @return PMDB_SUCCESSFUL Reading data from the PMDB area in storage succeeded.
 *
 * The return data buffer must be at least dataSizeInBytes bytes in size. The
 * range of this parameter is:
 *
 *   0 < dataSizeInBytes <= MAX_PMDB_AREA_SIZE_IN_BYTES
 */
pmdb_result_t sep_pmdb_read( const sep_pmdb_area_t pmdbArea,
                             uint8_t * const pData,
                             const uint32_t dataSizeInBytes );


/*
 * @brief Writes data from a buffer to the specified Platform Management
 *        Database area in storage.
 * @param[in] pmdbArea The PMDB area in storage to write the data to.
 * @param[in] pData Pointer to a buffer with data to write to the PMDB area.
 * @param[in] dataSizeInBytes Size of the data buffer in bytes.
 * @return PMDB_SUCCESSFUL Writing data to the PMDB area in storage succeeded.
 *
 * The data buffer must be at least dataSizeInBytes bytes in size. The range of
 * this parameter is:
 *
 *   0 < dataSizeInBytes <= MAX_PMDB_AREA_SIZE_IN_BYTES
 *
 * If the data size is less than MAX_PMDB_AREA_SIZE_IN_BYTES then the data
 * written to the PMDB will be padded with trailing zeroes.
 *
 * A secure token must be used to allow writing to the PMDB areas in storage
 * before this API can be used.
 */
pmdb_result_t sep_pmdb_write( const sep_pmdb_area_t pmdbArea,
                              const uint8_t * const pData,
                              const uint32_t dataSizeInBytes );


#ifdef EPID_ENABLED
extern int provision_epid( const char *const epidConfigFile);
#endif
/* Android Customer Data EXTFW opcodes */
/*
#define OPCODE_IA2CHAABI_ACD_READ             1
#define OPCODE_IA2CHAABI_ACD_WRITE            2
#define OPCODE_IA2CHAABI_ACD_LOCK             4
#define OPCODE_IA2CHAABI_ACD_PROV             5
*/

#define OPCODE_IA2CHAABI_ACD_READ             1
#define OPCODE_IA2CHAABI_ACD_WRITE            2
#define OPCODE_IA2CHAABI_OS_VRL               3
#define OPCODE_IA2CHAABI_ACD_LOCK             4
#define OPCODE_IA2CHAABI_ACD_PROV             5
#define OPCODE_IA2CHAABI_ACD_WIPE             6

#define ACD_LOCK_SUCCESS			0
#define ACD_PROV_SUCCESS			0
#define ACD_READ_SUCCESS			0
#define ACD_WRITE_SUCCESS			0

#ifdef LEGACY_ACD_CODE
/*
 * Android Customer Data get_customer_data(), lock_customer_data(),
 * provision_customer_data(), and set_customer_data() error code return values.
 * The host library returns the EXTFW unsigned error values as negative. If
 * there is a ACD write error during the ACD provisioning then the error code
 * return value will be ACD write error plus one hundred (e.g., error of -104 is
 * because the data being provisioned is too large to be written) These macros
 * will have to be kept synchronized with the same EXTFW macros.
 */
#define ACD_LOCK_ERROR_NO_CUSTOMER_DATA             -1
#define ACD_LOCK_ERROR_CUSTOMER_DATA_ALREADY_LOCKED -2
#define ACD_LOCK_ERROR_UMIP_READ_WRITE_FAILURE      -3
#define ACD_LOCK_ERROR_HMAC_FAILURE                 -4
#define ACD_LOCK_ERROR_FIELD_TABLE_CACHE_ERROR      -5

#define ACD_PROV_ERROR_ILLEGAL_PARAMETER               -1
#define ACD_PROV_ERROR_SEP_OPERATION                   -2
#define ACD_PROV_ERROR_RETURN_DATA_MEM_ALLOC_FAIL      -3
#define ACD_PROV_PROVISION_INVALID_SCHEME              -4
#define ACD_PROV_WV_KEYBOX_BAD_MAGIC                   -5
#define ACD_PROV_WV_KEYBOX_GEN_CRC_FAILED              -6
#define ACD_PROV_WV_KEYBOX_BAD_CRC                     -7
#define ACD_PROV_WV_KEYBOX_NO_KEYDATA                  -8
#define ACD_PROV_WV_KEYBOX_NO_DEVICEID                 -9
#define ACD_PROV_WV_KEYBOX_NO_DEVICE_KEY               -10
#define ACD_PROV_WV_KEYBOX_BAD_IMEI                    -11
#define ACD_PROV_WV_PROVISION_INPUT_BUFFER_LEN_INVALID -12
#define ACD_PROV_WV_PROVISION_GEN_DPSK_FAILURE         -13
#define ACD_PROV_WV_PROVISION_DPSK_ENCRYPT_FAILURE     -14
#define ACD_PROV_WV_PROVISION_PUBKEY_ENCRYPT_FAILURE   -15
#define ACD_PROV_WV_GEN_AESKEY_FAILURE                 -16
#define ACD_PROV_WV_PROVISION_AESKEY_DECRYPT_FAILURE   -17
#define ACD_PROV_WV_CHALLENGE_RESPONSE_FAILURE         -18
#define ACD_PROV_WV_GEN_HMAC_FAILURE                   -19
#define ACD_PROV_WV_GEN_IMEIHASH_FAILURE               -20
#define ACD_PROV_WV_HMAC_MISMATCH                      -21
#define ACD_PROV_WV_DEVICE_ID_MISMATCH                 -22
#define ACD_PROV_WV_ACD_WRITE_FAILURE                  -23
#define ACD_PROV_WV_ACD_IS_LOCKED                      -24
#define ACD_PROV_WV_PROVISION_GET_IMEI_FAILURE         -25
#define ACD_PROV_WV_PROVISION_GET_DEVID_FAILURE        -26
#define ACD_PROV_WV_INVALID_STAGE		       -27
#define ACD_PROV_WV_GEN_HASH_FAILURE		       -28
#define ACD_PROV_WV_HASH_MISMATCH		       -29


#define ACD_READ_ERROR_ILLEGAL_INPUT_PARAMETER            -1
#define ACD_READ_ERROR_HMAC_VERIFICATION_FAILURE          -2
#define ACD_READ_ERROR_TABLE_HMAC_FAILURE                 -3
#define ACD_READ_ERROR_UMIP_READ_FAILURE                  -4
#define ACD_READ_ERROR_FIELD_NOT_FOUND                    -5
#define ACD_READ_ERROR_FIELD_TABLE_CACHE_ERROR            -6
#define ACD_READ_SECURE_DATA_PROVISIONED_AND_WRITE_ONLY   -7

#define ACD_WRITE_ERROR_ILLEGAL_PARAMETER           -1
#define ACD_WRITE_ERROR_CUSTOMER_DATA_LOCKED        -2
#define ACD_WRITE_ERROR_MAX_SIZE_TOO_LARGE          -3
#define ACD_WRITE_ERROR_DATA_SIZE_TOO_LARGE         -4
#define ACD_WRITE_ERROR_CANNOT_CHANGE_MAX_SIZE      -5
#define ACD_WRITE_ERROR_UMIP_READ_WRITE_FAILURE     -6
#define ACD_WRITE_ERROR_CANNOT_ADD_NEW_FIELD        -7
#define ACD_WRITE_ERROR_HMAC_FAILURE                -8
#define ACD_WRITE_ERROR_FIELD_TABLE_CACHE_ERROR     -9
#define ACD_WRITE_ERROR_SECURE_DATA_LOCKED          -10
#define ACD_WRITE_ERROR_SECURE_DATA_FAILURE         -11

#endif



//
// Maximum and minimum Android Customer Data field index indices. These indices
// are ones-based counting.
//
#define ACD_MIN_FIELD_INDEX 1
#define ACD_MAX_FIELD_INDEX 63


//
// Maximum and minimum Android Customer Data provisioning schema values.
//
#ifdef EPID_ENABLED
#define ACD_MAX_PROVISIONNG_SCHEMA 2
#define ACD_PROVISIONING_SCHEMA_EPID 2
#else
#define ACD_MAX_PROVISIONNG_SCHEMA 1
#endif
#define ACD_MIN_PROVISIONNG_SCHEMA 0


//
// Android Customer Data provisioning schemas. These values are used with the
// user space program to select the type of ACD provisioning.
//
#define ACD_PROVISIONING_SCHEMA_WV_KEYBOX_0 0


//
// The maximum size in bytes for the Android Customer Data provisioning schemas
// input data.
//
#define ACD_PROVISIONING_SCHEMA_WV_KEYBOX_0_MAX_SIZE ( sizeof( acd_prov_wv_indata_t ) )


//
// The input data size in bytes for the Widevine keybox provisioning schema zero.
//
#define ACD_PROVISIONING_SCHEMA_WV_KEYBOX_0_INPUT_SIZE ACD_PROVISIONING_SCHEMA_WV_KEYBOX_0_MAX_SIZE


//
// The output data size in bytes for the Widevine keybox provisioning schema zero.
//
#define ACD_PROVISIONING_SCHEMA_WV_KEYBOX_0_OUTPUT_SIZE ( sizeof( acd_prov_wv_outdata_t ) )


//
// Macro for converting a SEP message parameter length in bytes so that the
// value is aligned to a four byte boundary. The maximum SEP message parameter
// length in bytes is required to be four-byte aligned.
//
#define BYTES_TO_4BYTE_BLOCKS(x) ( ( (x) + 3 ) / 4 )
#define SEP_MSG_PARAM_MAX_LEN(x) (BYTES_TO_4BYTE_BLOCKS(x) * 4)


//
// Indices for the Android Customer Data SEP message. These indices are for the
// SEP message that is used to send the Android Customer Data field parameters
// to the EXTFW but not the field data.
//
#define ACD_OPCODE_SEP_MSG_INDEX             0
#define ACD_FIELD_INDEX_SEP_MSG_INDEX        1
#define ACD_DATA_ACTUAL_SIZE_SEP_MSG_INDEX   2
#define ACD_DATA_MAX_SIZE_SEP_MSG_INDEX      3

#define ACD_NUM_SEP_MSG_INDICES 4


//
// Indices for the Android Customer Data return SEP message. These indices are
// for the SEP message that is returned by the EXTFW.
//
#define ACD_NUM_BYTES_SEP_RETURN_MSG_INDEX 1


//
// Maximum Android Customer Data field data size in bytes.
//
#define ACD_MAX_DATA_SIZE_IN_BYTES 512
#define ACD_MIN_DATA_SIZE_IN_BYTES 0

#define RPMB_MAX_INDICES_OF_SEP_MSG		6

#define RPMB_OPCODE_SEP_MSG_INDEX			0
/* Subopcodes used for the communication between rpmb users and rpmb */
#define RPMB_SUBOPCODE_CREATE_ENTRY  1
#define RPMB_SUBOPCODE_UPDATE_ENTRY  2
#define RPMB_SUBOPCODE_READ_ENTRY    3
#define RPMB_SUBOPCODE_READ_RAW      4
#define RPMB_SUBOPCODE_WRITE_RAW     5
#define RPMB_SUBOPCODE_DELETE_ALL    6
#define RPMB_SUBOPCODE_READ_IMR      7
#define RPMB_SUBOPCODE_PROVISION_KEY 8

#define RPMB_KEY_PROV_SUCCESS 	0
/*
 * This function sends a message to the EXTFW to initate the
 * provisioning of the RPMB key.  This function is called during
 * manufacturing.
 *
 * @return RPMB_KEY_PROV_SUCCESS when the RPMB Key provision message
 * was successfully sent to the EXTFW
 * @return DX_FASTCALL_WRITE_FAILURE, DX_FASTCALL_READ_FAILURE, or
 * DX_FASTCALL_VALIDATE_FAILURE if any of the fastcall function fail
 */
int sep_rpmb_provision_key(void);

#endif  /* end __UMIP_ACCESS_H__ */
