/**********************************************************************
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 
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
/*
 * tee_if.h
 *
 *  Created on: Sep 14, 2012
 *      Author: skris14
 */

#ifndef __TEE_IF_H_
#define __TEE_IF_H_

#include "tee_types.h"
#include "txei.h"


typedef enum {
        DO_SWAP,
        DONT_SWAP,
} tee_swap_flag;



/*
 *      These MACRO's define whether or not the endiness of the HOST is the
 *      same as the SEC.
 *
 *      Since the code was origianlly coded for Merrifield where it is assumed
 *      that the SEC is little endian, the __ARE_DIFFERENT, __ARE_THE_SAME and
 *      _HOST_SEC_ENDIAN MACRO's have been created here so as to make selection
 *      based on a platform's configuration more generic.
 */
#define __ARE_DIFFERENT         0
#define __ARE_THE_SAME          1

/*
 *      Swap these defines around to define the endianess relationship between
 *      the HOST and the SEC.
 */
//#define __HOST_SEC_ENDIAN       __ARE_THE_SAME
#define __HOST_SEC_ENDIAN       __ARE_DIFFERENT

#if 0
/*
 *      These three MACRO's are what exist in the original code and which need
 *      values assigned to in order to achieve the desired behaviour of the
 *      code.
 */
#define __BIG_ENDIAN            __ARE_DIFFERENT
#define __LITTLE_ENDIAN         __ARE_THE_SAME
#define __BYTE_ORDER            __HOST_SEC_ENDIAN
#endif

/* Define macros for host to SeP endianess conversion (for host wrappers) */
#if __HOST_SEC_ENDIAN == __ARE_DIFFERENT
#define cpu_to_le16(x) bswap_16(x)
#define le16_to_cpu(x) bswap_16(x)
#define cpu_to_le32(x) bswap_32(x)
#define le32_to_cpu(x) bswap_32(x)
#else   /* __ARE_THE_SAME */
#define cpu_to_le16(x) x
#define le16_to_cpu(x) x
#define cpu_to_le32(x) x
#define le32_to_cpu(x) x
#endif  /* __HOST_SEC_ENDIAN */

/*
 *      This is a configuration table that is used to define which byte-string
 *      data items need swapping and which do not.
 *
 *      Currently, all byte-string data items are configured as DONT_SWAP.
 *      Individual items will be changed to DO_SWAP as it is determined which
 (      if any) will need to be swapped.
 */
#if __HOST_SEC_ENDIAN == __ARE_DIFFERENT
#define SWAP_ENT_KEY    DONT_SWAP       //      Entitlement key
#define SWAP_CW         DONT_SWAP       //      Control word
#define SWAP_KEYBOX     DONT_SWAP       //      Key box
#define SWAP_KEY        DONT_SWAP       //      key data
#define SWAP_DEV_ID     DONT_SWAP       //      Device ID
#define SWAP_CTS        DONT_SWAP       //      Ciphertext Stealing data
#define SWAP_CIPHER     DONT_SWAP       //      Ciphertext
#define SWAP_IV         DONT_SWAP       //      Initialization vector
#define SWAP_PLAIN      DONT_SWAP       //      Plaintext
#define SWAP_NALU       DONT_SWAP       //      NALU headers
#define SWAP_FIELD_DATA DONT_SWAP       //      ACD field data
#define SWAP_PROV       DONT_SWAP       //      Provisioning data
#else   /* __ARE_THE_SAME */
#define SWAP_ENT_KEY    DONT_SWAP       //      Entitlement key
#define SWAP_CW         DONT_SWAP       //      Control word
#define SWAP_KEYBOX     DONT_SWAP       //      Key box
#define SWAP_KEY        DONT_SWAP       //      key data
#define SWAP_DEV_ID     DONT_SWAP       //      Device ID
#define SWAP_CTS        DONT_SWAP       //      Ciphertext Stealing data
#define SWAP_CIPHER     DONT_SWAP       //      Ciphertext
#define SWAP_IV         DONT_SWAP       //      Initialization vector
#define SWAP_PLAIN      DONT_SWAP       //      Plaintext
#define SWAP_NALU       DONT_SWAP       //      NALU headers
#define SWAP_FIELD_DATA DONT_SWAP       //      ACD field data
#define SWAP_PROV       DONT_SWAP       //      Provisioning data
#endif  /* __HOST_SEC_ENDIAN */


#define TEEC_NONE 0
#define TEEC_PARAM_TYPE_SET void
#define WIDEVINE_APP_UUID {0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,\
				0xDA,0xDB,0xDC,0xDD,0xDE,0xDF}
#define MAX_TEE_PARAMS (4)

#define SET_NONE_PARAM_OPERATION(operation, index)			\
	do {								\
		TEEC_PARAM_TYPE_SET(operation.paramTypes, index, TEEC_NONE);\
		operation.params[index].value.a = 0;			\
		operation.params[index].value.b = 0;			\
	} while (0)

#define SET_VALUE_PARAM_OPERATION(operation, index, type, val1, val2)	\
	do {								\
		TEEC_PARAM_TYPE_SET(operation.paramTypes, index, type);	\
		if ((type != TEEC_NONE) && (type != TEEC_VALUE_OUTPUT)) { \
			operation.params[index].value.a = val1;		\
			operation.params[index].value.b = val2;		\
		}							\
	} while (0)


#define SET_MEMREF_PARAM_OPERATION(operation, index, type, object_addr, \
					object_size, object_offset)	\
	do {								\
		TEEC_PARAM_TYPE_SET(operation.paramTypes, index, type);	\
		operation.params[index].memref.parent = object_addr;	\
		if (object_size) {					\
			operation.params[index].memref.size = object_size; \
			operation.params[index].memref.offset = object_offset; \
		}							\
	} while (0)

#define SET_MEM_REF_TYPE(ref_type, buf_type)		\
	do {						\
		if (buf_type & DATA_IN)			\
			ref_type = TEEC_MEM_INPUT;	\
		if (buf_type & DATA_OUT)		\
			ref_type |= TEEC_MEM_OUTPUT;	\
	} while(0)


enum tee_cmd_type {
	TEE_OPEN_SESSION,
	TEE_INVOKE_COMMAND,

};


uint32_t tee_init(const GUID *guid, void **ptrHandle);
uint32_t tee_deinit(void *ptrHandle);

uint32_t process_cmd(
	MEI_HANDLE *ptrHandle,
	uint32_t cmd_id,
	struct data_buffer buf_ptr_in[],
	struct data_buffer buf_ptr_out[],
	uint32_t num_params);

void copySwap( void *vDst, const void *vSrc, const uint32_t length, const tee_swap_flag flag );

#endif /* __TEE_IF_H_ */
