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
/**
 * @file    tee_if.c
 * @brief
 *
 * @History:
 *-----------------------------------------------------------------------------
 * Date		Author		Description
   09-10-2012	Sudha		Added process_cmd api to tee send commands to 
   				firmware.
   10-17-2012	Sudha		Modularised process_cmd, to diffrentiate 
   				open_session and invoking command.
 *-----------------------------------------------------------------------------
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef MSVS
#include <string.h>
#endif /* MSVS */
#include "tee_types.h"
#include "tee_if.h"
#include "tee_error.h"
#include "sepdrm-log.h"

#define RELEASE_SHARED_MEMORY(reg_shm, cnt)	\
	do {					\
		while (cnt) {			\
			TEEC_ReleaseSharedMemory(&reg_shm[cnt - 1]);\
			cnt--;			\
		}				\
	} while(0)


static uint32_t validate_data_buffer_params(
	struct data_buffer buf_ptr_in[],
	struct data_buffer buf_ptr_out[],
	uint32_t num_params)
{
	uint32_t cnt_in;
	uint32_t cnt_out;

	if (!buf_ptr_in || !buf_ptr_out)
		return TEE_FAIL_INVALID_PARAM;
	return TEE_SUCCESSFUL;

}

static uint32_t send_cmd(
	MEI_HANDLE *ptrHandle,
	uint32_t cmd_id,
	struct data_buffer buf_ptr[],
	uint32_t num_params)
{

	uint32_t cnt;
	uint32_t ret_origin = 0;
	int ret;
	uint32_t totalsize = 0;

	if( ( NULL == buf_ptr ) || ( NULL == buf_ptr[0].buffer ) )
		return TEE_FAIL_INVALID_PARAM;

//        mei_print_buffer( "buf_ptr sent", buf_ptr[0].buffer, buf_ptr[0].size );
        ret = mei_sndmsg( ptrHandle, buf_ptr[0].buffer, buf_ptr[0].size);
	if (ret <= 0)
	{
		LOGERR("failed to send message to HECI");
		return ret;
	}
	return TEE_SUCCESSFUL;
}

static uint32_t recv_cmd(
	MEI_HANDLE *ptrHandle,
	uint32_t cmd_id,
	struct data_buffer buf_ptr[],
	uint32_t num_params)
{
	uint32_t cnt;
	int ret;
	uint32_t totalsize = 0;

	if( ( NULL == buf_ptr ) || ( NULL == buf_ptr[0].buffer ) )
		return TEE_FAIL_INVALID_PARAM;

        ret = mei_rcvmsg( ptrHandle, buf_ptr[0].buffer, buf_ptr[0].size);
	if (ret <= 0)
	{
		LOGERR("failed to send message to HECI");
		return ret;
	}

//        mei_print_buffer("buf_ptr_recvd", buf_ptr[0].buffer, buf_ptr[0].size);

	return TEE_SUCCESSFUL;
}


/*!
 * Functions
 */

uint32_t tee_init(const GUID *guid, void **ptrHandle)
{
	*ptrHandle = (void *)mei_connect(guid);
	if (*ptrHandle == NULL)
	{
		printf("ptrHandle error");
		return TEE_FAILURE;
	}
	return TEE_SUCCESSFUL;
}

uint32_t tee_deinit(void *ptrHandle)
{
	mei_disconnect(ptrHandle);
	return TEE_SUCCESSFUL;
}

uint32_t process_cmd(
	MEI_HANDLE *ptrHandle,
	uint32_t cmd_id,
	struct data_buffer buf_ptr_in[],
	struct data_buffer buf_ptr_out[],
	uint32_t num_params)
{
	uint32_t status = TEE_SUCCESSFUL;
	int ret = 0;
	if (!buf_ptr_in || !buf_ptr_out || !num_params)
		return TEE_FAIL_INVALID_PARAM;

	ret = validate_data_buffer_params(buf_ptr_in, buf_ptr_out, num_params);
	if (ret) {
		LOGERR("Data buffer params invalid, ret=0x%x", ret);
		return TEE_FAIL_INVALID_PARAM;
	}

	ret = send_cmd(ptrHandle, cmd_id, buf_ptr_in, num_params);
	if (ret) {
		LOGERR("Send Command, error=0x%08x, cmd-id=0x%08x\n", ret, cmd_id);
	}

	ret = recv_cmd(ptrHandle, cmd_id, buf_ptr_out, num_params);
	if (ret) {
		LOGERR("Receive Command, error=0x%08x, cmd-id=0x%08x\n", ret, cmd_id);
	}

final_exit:

	return ret;

}



void copySwap( void *vDst, const void *vSrc, const uint32_t length, const tee_swap_flag flag )
{

#define QUICK_SWAP_SIZE         8       //  This must always be a power of 2, greater than 0

        uint8_t         *pDst = (uint8_t *)vDst;
        uint8_t         *pSrc = (uint8_t *)vSrc + length;
        uint32_t         quickSwap = length & ( QUICK_SWAP_SIZE - 1 );

        if( ( NULL == vDst ) || ( NULL == vSrc ) )
        {
                return;
        }
        if( flag == DONT_SWAP )
        {
                memcpy( vDst, vSrc, length );
                return;
        }
        while( pSrc != (uint8_t *)vSrc )
        {
                switch( quickSwap )
                {
                        /*
                         *      Because pSrc starts out pointing at one byte
                         *      past the end of the source data, it must be
                         *      decremented BEFORE a byte is copied from it.
                         */
                        case 0:
                                *pDst++ = *( --pSrc );
                        case 7:
                                *pDst++ = *( --pSrc );
                        case 6:
                                *pDst++ = *( --pSrc );
                        case 5:
                                *pDst++ = *( --pSrc );
                        case 4:
                                *pDst++ = *( --pSrc );
                        case 3:
                                *pDst++ = *( --pSrc );
                        case 2:
                                *pDst++ = *( --pSrc );
                        default:
                                *pDst++ = *( --pSrc );
                }
                quickSwap = 0;
        }

}
