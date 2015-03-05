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

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ExtApp_qa_op_code.h"

#include "chaabi_error_codes.h"
#include "umip_access.h"
#include "tee_if.h"
#include "acd_cmd_structs.h"
#include <fcntl.h>
/* define LOG related macros before including the log header file */
#define LOG_STYLE    LOG_STYLE_PRINTF
#define LOG_TAG      "ACD-lib"
#include "sepdrm-log.h"

#define FAST_BUF_SIZE (8*1024)

#define ACD_MAIN_OPCODE (9876)

//
// SEP message array indices for the Platform Management Database parameters.
//
#define PMDB_SUB_OPCODE_SEP_MSG_INDEX 0
#define PMDB_DATA_SIZE_SEP_MSG_INDEX  1
#define PMDB_WRITE_TYPE_SEP_MSG_INDEX 2

#define PMDB_SEP_MESSAGE_NUM_INDICES 3


//
// Platform Management Database parser sub opcode values.
//
#define PMDB_LOCK_SUB_OPCODE  0
#define PMDB_READ_SUB_OPCODE  1
#define PMDB_WRITE_SUB_OPCODE 2

const GUID guid = {0xafa19346, 0x7459, 0x4f09, {0x9d, 0xad, 0x36, 0x61, 0x1f, 0xe4, 0x28, 0x60}};
int acd_init(const GUID *guid, void **ptrHandle)
{
	int result = tee_init(guid, ptrHandle);
	if (result != 0)
		printf("error in acd_init");
	return result;
}

int acd_deinit(void *handle)
{
	return tee_deinit(handle);
}

int lock_customer_data()
{

        uint32_t                                        ret;
        struct data_buffer                              cmd_data_in[MAX_DATA_BUF_PARAMS];
        struct data_buffer                              cmd_data_out[MAX_DATA_BUF_PARAMS];
        struct acd_lock_cmd_from_host      		params;
        struct acd_lock_cmd_to_host        		resp;
	void *ptrHandle;

	//Initialize ACD by connecting using the GUID
	ret = acd_init(&guid, &ptrHandle);
	if ((ret != EXIT_SUCCESS) || (ptrHandle == NULL))
	{
                LOGERR( "Failed to Initialize ACD: 0x%08x\n", ret );
		return ret;
	}

        // Initialize the parameters to zero
        memset( &params, 0, sizeof( params ) );
        memset( &resp, 0, sizeof( resp ) );
        memset( cmd_data_in, 0, sizeof( cmd_data_in ) );
        memset( cmd_data_out, 0, sizeof( cmd_data_out ) );
        // Populate the parameter data structures
        params.hdr_req.main_opcode = ACD_MAIN_OPCODE;
        params.hdr_req.sub_opcode = OPCODE_IA2CHAABI_ACD_LOCK;
        // Link-up the parameter data structures
        INIT_FROM_HOST_PARAM_BUF( cmd_data_in[FROM_HOST_PARAM_INDEX], &params, sizeof( params ) );
        INIT_TO_HOST_PARAM_BUF( cmd_data_out[TO_HOST_PARAM_INDEX], &resp, sizeof( resp ) );
        // Send the message off to FW
        ret = process_cmd( ptrHandle, DX_SEP_HOST_SEP_PROTOCOL_IA_ACCESS_OP_CODE, cmd_data_in, cmd_data_out, 2 );
        if( ACD_LOCK_SUCCESS != ret )
        {
                LOGERR( "process_cmd failed: error 0x%x\n", ret );
		ptrHandle = NULL;
                return( ret );
        }
#ifdef ENABLE_LATER
	else if ( ACD_LOCK_SUCCESS != bswap_32( resp.hdr_resp.status ))
        {
                LOGERR( "Received failed response: 0x%08x\n", ret );
		mei_print_buffer("lock_customer_data response", (uint8_t *)&resp, sizeof(resp)); 
        }
#endif
        else if( ACD_LOCK_SUCCESS != resp.acd_status)
        {
                LOGERR( "ACD Command failed in FW: 0x%08x\n", resp.acd_status);
		mei_print_buffer("lock_customer_data response", (uint8_t *)&resp, sizeof(resp)); 
        }

	if (ptrHandle != NULL)
	{
		if (acd_deinit(ptrHandle) != 0)
		{
			LOGERR( "Failed to uninitialize ACD");
		}
		ptrHandle = NULL;
	}

        return( ACD_LOCK_SUCCESS - ret );

}       //  lock_customer_data

#ifdef ACD_WIPE_TEST
int wipe_customer_data()
{

        uint32_t                                        ret;
        struct data_buffer                              cmd_data_in[MAX_DATA_BUF_PARAMS];
        struct data_buffer                              cmd_data_out[MAX_DATA_BUF_PARAMS];
        struct acd_generic_cmd_from_host      		params;
        struct acd_generic_cmd_to_host        		resp;

	void *ptrHandle;

	//Initialize ACD by connecting using the GUID
	ret = acd_init(&guid, &ptrHandle);
	if ((ret != EXIT_SUCCESS) || (ptrHandle == NULL))
	{
		LOGERR( "Failed to Initialize ACD: 0x%08x\n", ret );
		return ret;
	}


        // Initialize the parameters to zero
        memset( &params, 0, sizeof( params ) );
        memset( &resp, 0, sizeof( resp ) );
        memset( cmd_data_in, 0, sizeof( cmd_data_in ) );
        memset( cmd_data_out, 0, sizeof( cmd_data_out ) );
        // Populate the parameter data structures
        params.hdr_req.main_opcode = ACD_MAIN_OPCODE;
        params.hdr_req.sub_opcode = OPCODE_IA2CHAABI_ACD_WIPE;
        // Link-up the parameter data structures
        INIT_FROM_HOST_PARAM_BUF( cmd_data_in[FROM_HOST_PARAM_INDEX], &params, sizeof( params ) );
        INIT_TO_HOST_PARAM_BUF( cmd_data_out[TO_HOST_PARAM_INDEX], &resp, sizeof( resp ) );
        // Send the message off to FW
        ret = process_cmd( ptrHandle, DX_SEP_HOST_SEP_PROTOCOL_IA_ACCESS_OP_CODE, cmd_data_in, cmd_data_out, 2 );
        if( ACD_LOCK_SUCCESS != ret )
        {
                LOGERR( "process_cmd failed: error 0x%x\n", ret );
		ptrHandle = NULL;
                return( ret );
        }
#ifdef ENABLE_LATER
	else if (ACD_LOCK_SUCCESS != bswap_32( resp.hdr_resp.status ))
        {
                LOGERR( "Received failed response: 0x%08x\n", ret );
		mei_print_buffer("wipe_customer_data response", (uint8_t *)&resp, sizeof(resp)); 
        }
#endif
        else if(ACD_LOCK_SUCCESS != resp.acd_status)
        {
                LOGERR("ACD Command failed in FW: 0x%08x\n", resp.acd_status);
		mei_print_buffer("wipe_customer_data response", (uint8_t *)&resp, sizeof(resp)); 
        }
	if (ptrHandle != NULL)
	{
		if (acd_deinit(ptrHandle) != 0)
		{
			LOGERR( "Failed to uninitialize ACD");
		}
		ptrHandle = NULL;
	}

        return(ACD_LOCK_SUCCESS - ret );

}       //  wipe_customer_data
#endif

int set_customer_data( const uint8_t uiFieldIndex,
                       uint16_t FieldSize,
                       uint16_t FieldMaxSize,
                       const void * const pvAdcFieldData )
{

        uint32_t                                        ret;
        struct data_buffer                              cmd_data_in[MAX_DATA_BUF_PARAMS];
        struct data_buffer                              cmd_data_out[MAX_DATA_BUF_PARAMS];
        struct acd_write_cmd_from_host       		params;
        struct acd_write_cmd_to_host         		resp;
	void *ptrHandle;

        /*
         *      Sanity check
         */
        if( ( ACD_MIN_FIELD_INDEX > uiFieldIndex ) || ( ACD_MAX_FIELD_INDEX < uiFieldIndex ) )
        {
                LOGERR( "field index %u is illegal.\n", uiFieldIndex );
                return( ACD_WRITE_ERROR_ILLEGAL_PARAMETER );
        }
        if( ( ACD_MIN_DATA_SIZE_IN_BYTES == FieldSize ) || ( ACD_MAX_DATA_SIZE_IN_BYTES < FieldSize ) )
        {
                LOGERR( "field index %u data size %u is illegal.\n", uiFieldIndex, FieldSize );
                return( ACD_WRITE_ERROR_ILLEGAL_PARAMETER );
        }
        if( ( ACD_MIN_DATA_SIZE_IN_BYTES == FieldMaxSize ) || ( ACD_MAX_DATA_SIZE_IN_BYTES < FieldMaxSize ) )
        {
                LOGERR( "field index %u data max size %u bytes is illegal.\n", uiFieldIndex, FieldMaxSize );
                return( ACD_WRITE_ERROR_ILLEGAL_PARAMETER );
        }
        if( FieldSize > FieldMaxSize )
        {
                LOGERR( "field index %u data size %u greater than max size is illegal.\n", uiFieldIndex, FieldSize );
                return( ACD_WRITE_ERROR_DATA_SIZE_TOO_LARGE );
        }
        if( NULL == pvAdcFieldData )
        {
                LOGERR( "NULL data field\n" );
                return( ACD_WRITE_ERROR_ILLEGAL_PARAMETER );
        }

	//Initialize ACD by connecting using the GUID
	ret = acd_init(&guid, &ptrHandle);
	if ((ret != EXIT_SUCCESS) || (ptrHandle == NULL))
	{
		LOGERR( "Failed to Initialize ACD: 0x%08x\n", ret );
		return ret;
	}

        /*
         *      Initialize the parameters to zero
         */
        memset( &params, 0, sizeof( params ) );
        memset( &resp, 0, sizeof( resp ) );
        memset( cmd_data_in, 0, sizeof( cmd_data_in ) );
        memset( cmd_data_out, 0, sizeof( cmd_data_out ) );
        /*
         *      Populate the parameter data structures
         */

        params.hdr_req.main_opcode = ACD_MAIN_OPCODE;
        params.hdr_req.sub_opcode = OPCODE_IA2CHAABI_ACD_WRITE;
        params.index = uiFieldIndex;
        params.actual_size = FieldSize;
        params.max_size = FieldMaxSize;
        memcpy(params.buf, pvAdcFieldData, FieldSize);

        /*
         *      Link-up the parameter data structures
         */
        INIT_FROM_HOST_PARAM_BUF( cmd_data_in[FROM_HOST_PARAM_INDEX], &params, sizeof( params ) );
        INIT_TO_HOST_PARAM_BUF( cmd_data_out[TO_HOST_PARAM_INDEX], &resp, sizeof( resp ) );
        /*
         *      Send the message off to FW
         */
        ret = process_cmd( ptrHandle, DX_SEP_HOST_SEP_PROTOCOL_IA_ACCESS_OP_CODE, cmd_data_in, cmd_data_out, 2 );

        if( ACD_WRITE_SUCCESS != ret )
        {
                LOGERR( "process_cmd failed: error 0x%x\n", ret );
		ptrHandle = NULL;
                return( ACD_WRITE_SUCCESS - ret );
        }
#ifdef ENABLE_LATER
        ret = bswap_32( resp.hdr_resp.status );
        if( ACD_WRITE_SUCCESS != ret )
        {
                LOGERR( "Received failed response: 0x%08x\n", ret );
		mei_print_buffer("set_customer_data response", (uint8_t *)&resp, sizeof(resp));
		ptrHandle = NULL;
                return( ACD_WRITE_SUCCESS - ret );
        }
#endif
        ret = resp.acd_status;
        if( ACD_WRITE_SUCCESS != ret )
        {
                LOGERR( "ACD Command failed in FW: 0x%08x\n", ret );
		mei_print_buffer("set_customer_data response", (uint8_t *)&resp, sizeof(resp)); 
		ptrHandle = NULL;
                return( ACD_WRITE_SUCCESS - ret );
        }
	if (ptrHandle != NULL)
	{
		if (acd_deinit(ptrHandle) != 0)
		{
			LOGERR( "Failed to uninitialize ACD");
		}
		ptrHandle = NULL;
	}

        return( FieldSize );

}       //  set_customer_data

int get_customer_data(const uint8_t uiFieldIndex, void **const pvAdcFieldData )
{

        uint32_t                                        ret = 0xffffffff;
        uint32_t                                        returned_size = 0;
        struct data_buffer                              cmd_data_in[MAX_DATA_BUF_PARAMS];
        struct data_buffer                              cmd_data_out[MAX_DATA_BUF_PARAMS];
        struct acd_read_cmd_from_host		        params;
        struct acd_read_cmd_to_host		        resp;
	void *ptrHandle = NULL;

	//Initialize ACD by connecting using the GUID
	ret = acd_init(&guid, &ptrHandle);
	if (!(ret == EXIT_SUCCESS && ptrHandle))
	{
		LOGERR("Failed to Initialize ACD: 0x%08x\n", ret);
		goto exit;
	}

        /*
         *      Sanity check
         */
        if(!( (ACD_MIN_FIELD_INDEX <= uiFieldIndex) && (ACD_MAX_FIELD_INDEX >= uiFieldIndex )))
        {
                LOGERR( "field index %u is illegal.\n", uiFieldIndex );
                ret = ACD_READ_ERROR_ILLEGAL_INPUT_PARAMETER;
				goto exit;
        }
		if(!(pvAdcFieldData))
        {
                LOGERR( "pvAdcFieldData is invalid\n" );
                ret = ACD_READ_ERROR_ILLEGAL_INPUT_PARAMETER;
				goto exit;
        }
        /*
         *      Initialize the parameters to zero
         */
        memset( &params, 0, sizeof( params ) );
        memset( &resp, 0, sizeof( resp ) );
        memset( cmd_data_in, 0, sizeof( cmd_data_in ) );
        memset( cmd_data_out, 0, sizeof( cmd_data_out ) );
        /*
         *      Populate the parameter data structures
         */
        params.hdr_req.main_opcode = ACD_MAIN_OPCODE;
        params.hdr_req.sub_opcode = OPCODE_IA2CHAABI_ACD_READ;
        params.index = uiFieldIndex;

        /*
         *      Link-up the parameter data structures
         */
        INIT_FROM_HOST_PARAM_BUF( cmd_data_in[FROM_HOST_PARAM_INDEX], &params, sizeof( params ) );
        INIT_TO_HOST_PARAM_BUF( cmd_data_out[TO_HOST_PARAM_INDEX], &resp, sizeof( resp ) );
        /*
         *      Send the message off to FW
         */
        ret = process_cmd( ptrHandle, DX_SEP_HOST_SEP_PROTOCOL_IA_ACCESS_OP_CODE, cmd_data_in, cmd_data_out, 2 );

        if( ACD_READ_SUCCESS != ret )
        {
                LOGERR( "process_cmd failed: error 0x%x\n", ret );
                ret = ACD_READ_SUCCESS - ret;
				goto exit;
        }

        ret = resp.acd_status;

        if( ACD_READ_SUCCESS != ret && ACD_READ_SECURE_DATA_PROVISIONED_AND_WRITE_ONLY != ret)
        {
                LOGERR( "ACD Command failed in FW: 0x%08x\n", ret );
				mei_print_buffer("get_customer_data response", (uint8_t *)&resp, sizeof(resp)); 
               ret = ACD_READ_SUCCESS - ret;
			   goto exit;
        }

	if (ACD_READ_SECURE_DATA_PROVISIONED_AND_WRITE_ONLY == ret)
	{
		LOGDBG( "Read not allowed on this index\n");
	}

        returned_size = resp.bytes_read;

        LOGDBG("read = %d\n", returned_size);

        *pvAdcFieldData = calloc((size_t)returned_size, sizeof(uint8_t));
        if( NULL == *pvAdcFieldData )
        {
                LOGERR( "unable to allocate the read buffer\n" );
                ret = ACD_READ_ERROR_UMIP_READ_FAILURE;
				goto exit;
        }

        memcpy(*pvAdcFieldData, resp.buf, returned_size);
		ret = returned_size;

exit:

	if (ptrHandle != NULL)
	{
		if (acd_deinit(ptrHandle) != 0)
		{
			LOGERR( "Failed to uninitialize ACD");
		}
		ptrHandle = NULL;
	}

        return ret;

}       //  get_customer_data


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
                             void ** const pOutData )
{

        uint32_t                                        ret;
        uint32_t                                        returnDataSizeInBytes;
        struct data_buffer                              cmd_data_in[MAX_DATA_BUF_PARAMS];
        struct data_buffer                              cmd_data_out[MAX_DATA_BUF_PARAMS];
        struct acd_write_cmd_from_host      		params;
        struct acd_read_cmd_to_host		       	resp;
	void *ptrHandle;

        /*
         *      Sanity check.
         */
        if( ( ACD_MIN_DATA_SIZE_IN_BYTES == inDataSize ) || ( ACD_MAX_DATA_SIZE_IN_BYTES < inDataSize ) )
        {
                LOGERR( "ACD provisioning input data size of 0x%X bytes is illegal.\n", inDataSize );
                return( ACD_PROV_ERROR_ILLEGAL_PARAMETER);
        }

        if( NULL == pInData )
        {
                LOGERR( "ACD provisioning input data buffer pointer is NULL.\n" );
                return( ACD_PROV_ERROR_ILLEGAL_PARAMETER );
        }
        if( NULL == pOutDataSize )
        {
                LOGERR( "ACD provisioning output data size pointer is NULL.\n" );
                return( ACD_PROV_ERROR_ILLEGAL_PARAMETER );
        }
        if( ( NULL == pOutData ) && ( 0 == provisionSchema ) )
        {
                LOGERR( "ACD provisioning output data buffer pointer is NULL.\n" );
                return( ACD_PROV_ERROR_ILLEGAL_PARAMETER );
        }

	//Initialize ACD by connecting using the GUID
	ret = acd_init(&guid, &ptrHandle);
	if ((ret != EXIT_SUCCESS) || (ptrHandle == NULL))
	{
		LOGERR( "Failed to Initialize ACD: 0x%08x\n", ret );
		return ret;
	}

        /*
         *      Initialize the parameters to zero
         */
        memset( &params, 0, sizeof( params ) );
        memset( &resp, 0, sizeof( resp ) );
        memset( cmd_data_in, 0, sizeof( cmd_data_in ) );
        memset( cmd_data_out, 0, sizeof( cmd_data_out ) );
        /*
         *      Populate the parameter data structures
         */
        params.hdr_req.main_opcode = ACD_MAIN_OPCODE;
        params.hdr_req.sub_opcode = OPCODE_IA2CHAABI_ACD_PROV;
        /*
         *      Setup the SEP message parameters for ACD provisioning.
         */
        params.index = provisionSchema;
        params.actual_size = inDataSize;
        params.max_size = inDataSize;
        memcpy( params.buf, pInData, inDataSize );
        /*
         *      Link-up the parameter data structures
         */
        INIT_FROM_HOST_PARAM_BUF( cmd_data_in[FROM_HOST_PARAM_INDEX], &params, sizeof( params ) );
        INIT_TO_HOST_PARAM_BUF( cmd_data_out[TO_HOST_PARAM_INDEX], &resp, sizeof( resp ) );
        /*
         *      Send the message off to FW
         */
        ret = process_cmd( ptrHandle, DX_SEP_HOST_SEP_PROTOCOL_IA_ACCESS_OP_CODE, cmd_data_in, cmd_data_out, 2 );
        if( ACD_PROV_SUCCESS != ret )
        {
                LOGERR( "process_cmd failed: error 0x%x\n", ret );
		ptrHandle = NULL;
                return( ACD_PROV_SUCCESS - ret );
        }
#ifdef ENABLE_LATER
        ret = bswap_32( resp.hdr_resp.status );
        if( ACD_PROV_SUCCESS != ret )
        {
                LOGERR( "Received failed response: 0x%08x\n", ret );
		//mei_print_buffer("provision_customer_data response", (uint8_t *)&resp, sizeof(resp)); 
		ptrHandle = NULL;
                return( ACD_PROV_SUCCESS - ret );
        }
#endif
        ret = resp.acd_status;
        if( ACD_PROV_SUCCESS != ret )
        {
                LOGERR( "ACD Command failed in FW: 0x%08x\n", ret );
		//mei_print_buffer("provision_customer_data response", (uint8_t *)&resp, sizeof(resp)); 
		ptrHandle = NULL;
		return ( ACD_PROV_SUCCESS - ret );
        }
        returnDataSizeInBytes = resp.bytes_read;
        if( ACD_MIN_DATA_SIZE_IN_BYTES == returnDataSizeInBytes )
        {
                /*
                 *      There is no output data to return.
                 */
                *pOutDataSize = returnDataSizeInBytes;
                if( NULL != pOutData )
                {
                        *pOutData = NULL;
                }
        }
        else
        {
                /*
                 *      Allocate memory for copying the return data to.
                 */
		if (NULL == pOutData)
		{
			LOGERR( "ACD provisioning output data buffer pointer is NULL.\n" );
			ptrHandle = NULL;
			return( ACD_PROV_ERROR_ILLEGAL_PARAMETER );
		}
                *pOutData = calloc( (size_t)returnDataSizeInBytes, sizeof( uint8_t ) );
                if( NULL == *pOutData )
                {
                        LOGERR( "Could not allocate 0x%08x bytes of memory for output data buffer.\n", returnDataSizeInBytes );
                        *pOutDataSize = ACD_MIN_DATA_SIZE_IN_BYTES;
			ptrHandle = NULL;
                        return( ACD_PROV_ERROR_RETURN_DATA_MEM_ALLOC_FAIL );
                }
                /*
                 *      Copy the returned data byte array to the output buffer.
                 */
//                copySwap( *pOutData, resp.buf, returnDataSizeInBytes, SWAP_PROV );
                memcpy( *pOutData, resp.buf, returnDataSizeInBytes );
                /*
                 *      Return the output buffer size.
                 */
                *pOutDataSize = returnDataSizeInBytes;
        }

	if (ptrHandle != NULL)
	{
		if (acd_deinit(ptrHandle) != 0)
		{
			LOGERR( "Failed to uninitialize ACD");
		}
		ptrHandle = NULL;
	}

        return( ACD_PROV_SUCCESS );

}       //  provision_customer_data


/*
 * @brief Locks to the Platform Management Database write-once area in storage.
 * @return PMDB_SUCCESSFUL Locking the PMDB write-once area in storage succeeded.
 *
 * A secure token must be used to allow writing to the PMDB areas in storage
 * before this API can be used.
 */
pmdb_result_t sep_pmdb_lock( void )
{

#ifdef BAYTRAIL
	return (PMDB_FAIL_UNSUPPORTED);
#else
        DxUint8_t  aSendBuffer[ FAST_BUF_SIZE ], aResponseBuffer[ FAST_BUF_SIZE ];
	DxError_t  status = DX_ERROR;
	DxError_t  sepStatus = DX_ERROR;
	DxUint32_t sepOpcode = 0;
	DxUint32_t sepSramOffset = 0;
	DxUint32_t aSepMsg[ PMDB_SEP_MESSAGE_NUM_INDICES ];
	int        fastcallStatus = 0;


	//
	// Write the PMDB write-only area lock parameters to the SEP message.
	// Locking the PMDB write-once area has a data size of zero.
	//
	(void)memset( aSepMsg, 0, sizeof( aSepMsg ) );

	aSepMsg[ PMDB_SUB_OPCODE_SEP_MSG_INDEX ] = PMDB_LOCK_SUB_OPCODE;

	aSepMsg[ PMDB_DATA_SIZE_SEP_MSG_INDEX ] = 0;

	aSepMsg[ PMDB_WRITE_TYPE_SEP_MSG_INDEX ] = (DxUint32_t)SEP_PMDB_WRITE_ONCE;


	//
	// Initialize the SEP driver.
	//
	status = DX_CC_HostInit();

	if ( DX_SUCCESS != status ) {
		LOGERR( "SEP driver initialization failed; DX_CC_HostInit() return "
		        "error value of 0x%lX.\n", status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Initialize the SEP message.
	//
	sepSramOffset = 0;

	sepOpcode = DX_SEP_HOST_SEP_PROTOCOL_HOST_PMDB_ACCESS_OP_CODE;

	status = SEPDriver_Fastcall_InitHdr( (struct SEP_FastCallHdr *)aSendBuffer,
		(struct SEP_MsgAreaHdr_In *)(aSendBuffer + sizeof(struct SEP_FastCallHdr)),
		0,
		sepOpcode,
		sizeof(aSendBuffer),
		0,
		&sepSramOffset );

	if ( DX_SUCCESS != status ) {
		LOGERR( "Failed to initialize PMDB lock SEP message; "
		        "SEPDriver_Fastcall_InitHdr() returned error value "
		        "0x%lX.\n", status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Write the SEP message parameters to the shared RAM.
	//
	status = SEPDriver_Fastcall_WriteParameter( (DxUint32_t)aSepMsg,
	                                            (DxUint32_t)sizeof( aSepMsg ),
	                                            (DxUint32_t)SEP_MSG_PARAM_MAX_LEN( sizeof( aSepMsg ) ),
	                                            &sepSramOffset,
	                                            DX_FALSE,
	                                            aSendBuffer );

	if ( DX_SUCCESS != status ) {
		LOGERR( "Failed to write PMDB lock parameters to SEP message; "
		        "SEPDriver_Fastcall_WriteParameter() returned error value "
		        "0x%lX.\n", status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Send the SEP message to Chaabi.
	//
	SEPDriver_Fastcall_Endmessage( (struct SEP_FastCallHdr *)aSendBuffer,
	                               (struct SEP_MsgAreaHdr_In *)(aSendBuffer + sizeof(struct SEP_FastCallHdr)),
	                               sepSramOffset );

	fastcallStatus = FVOS_Fastcall_Write( aSendBuffer, sepSramOffset );

	if ( 0 >= fastcallStatus ) {
		LOGERR( "Failed to send SEP message; FVOS_Fastcall_Write() "
		        "returned error value %d.\n", fastcallStatus );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Wait for Chaabi to return SEP message.
	//
	sepSramOffset = 0;

	fastcallStatus = FVOS_Fastcall_Read( aResponseBuffer, sizeof( aResponseBuffer ) );

	if ( 0 >= fastcallStatus ) {
		LOGERR( "Waiting for Chaabi response failed; FVOS_Fastcall_Read() "
		        "returned error value %d.\n", fastcallStatus );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Check the error status from Chaabi.
	//
	status = SEPDriver_Fastcall_Validate( (struct SEP_MsgAreaHdr_Out *)aResponseBuffer,
	                                      sepOpcode,
	                                      CRYS_OK,
	                                      &sepSramOffset );

	if ( DX_SUCCESS != status ) {
		if ( ( DX_FAILED_START_TOKEN_ERR == status ) ||
		     ( DX_WRONG_OPCODE_FROM_SEP_ERR == status ) )
		{
			//
			// Error occured in Fastcall validation.
			//
			LOGERR( "Error with Fastcall validation; "
			        "SEPDriver_Fastcall_Validate() returned error "
			        "value of 0x%lX.\n", status );

			return ( PMDB_FAIL_SEP_DRIVER_OP );
		}
		else
		{
			//
			// Error code is from Chaabi so return it to the caller.
			//
			LOGERR( "Chaabi returned error value of 0x%lX.\n", status );

			return ( (pmdb_result_t)status );
		}
	}


	//
	// Locking the PMDB write-once area succeeded.
	//
#endif  //  BAYTRAIL
        return ( PMDB_SUCCESSFUL );

}       //  sep_pmdb_lock


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
                             uint8_t * const       pData,
                             const uint32_t        dataSizeInBytes )
{

#ifdef BAYTRAIL
	return (PMDB_FAIL_UNSUPPORTED);
#else

	DxUint8_t  aSendBuffer[ FAST_BUF_SIZE ], aResponseBuffer[ FAST_BUF_SIZE ];
	DxError_t  status = DX_ERROR;
	DxError_t  sepStatus = DX_ERROR;
	DxUint32_t sepOpcode = 0;
	DxUint32_t sepSramOffset = 0;
	DxUint32_t aSepMsg[ PMDB_SEP_MESSAGE_NUM_INDICES ];
	DxUint32_t pmdbReadSizeInBytes = 0;
	int        fastcallStatus = 0;


	//
	// Check parameters for illegal values.
	//
	if ( NULL == pData ) {
		//
		// PMDB return buffer pointer is NULL.
		//
		LOGERR( "Pointer to PMDB data return buffer is NULL.\n" );

		return ( PMDB_FAIL_INVALID_PARAMETER );
	}

	if ( dataSizeInBytes > MAX_PMDB_AREA_SIZE_IN_BYTES ) {
		//
		// The amount of data to read to PMDB is too large.
		//
		LOGERR( "Data size of 0x%X bytes is greater than the maximum of "
		        "0x%X bytes.\n", dataSizeInBytes,
		        MAX_PMDB_AREA_SIZE_IN_BYTES );

		return ( PMDB_FAIL_DATA_SIZE_TOO_LARGE_FAILURE );
	}


	//
	// Write the PMDB read parameters to the SEP message. PMDB write type is
	// used to specify which PMDB data area to read.
	//
	(void)memset( aSepMsg, 0, sizeof( aSepMsg ) );

	aSepMsg[ PMDB_SUB_OPCODE_SEP_MSG_INDEX ] = PMDB_READ_SUB_OPCODE;

	aSepMsg[ PMDB_DATA_SIZE_SEP_MSG_INDEX ] = (DxUint32_t)dataSizeInBytes;

	aSepMsg[ PMDB_WRITE_TYPE_SEP_MSG_INDEX ] = (DxUint32_t)pmdbArea;


	//
	// Initialize the SEP driver.
	//
	status = DX_CC_HostInit();

	if ( DX_SUCCESS != status ) {
		LOGERR( "SEP driver initialization failed; DX_CC_HostInit() return "
		        "error value of 0x%lX.\n", status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Initialize the SEP message.
	//
	sepSramOffset = 0;

	sepOpcode = DX_SEP_HOST_SEP_PROTOCOL_HOST_PMDB_ACCESS_OP_CODE;

	status = SEPDriver_Fastcall_InitHdr( (struct SEP_FastCallHdr *)aSendBuffer,
		(struct SEP_MsgAreaHdr_In *)(aSendBuffer + sizeof(struct SEP_FastCallHdr)),
		0,
		sepOpcode,
		sizeof(aSendBuffer),
		0,
		&sepSramOffset );

	if ( DX_SUCCESS != status ) {
		LOGERR( "Failed to initialize PMDB read SEP message 0x%X; "
		        "SEPDriver_Fastcall_InitHdr() returned error value "
		        "0x%lX.\n", (uint32_t)pmdbArea, status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Write the SEP message parameters to the shared RAM.
	//
	status = SEPDriver_Fastcall_WriteParameter( (DxUint32_t)aSepMsg,
	                                            (DxUint32_t)sizeof( aSepMsg ),
	                                            (DxUint32_t)SEP_MSG_PARAM_MAX_LEN( sizeof( aSepMsg ) ),
	                                            &sepSramOffset,
	                                            DX_FALSE,
	                                            aSendBuffer );

	if ( DX_SUCCESS != status ) {
		LOGERR( "Failed to write PMDB lock parameters to SEP message; "
		        "SEPDriver_Fastcall_WriteParameter() returned error value "
		        "0x%lX.\n", status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Send the SEP message to Chaabi.
	//
	SEPDriver_Fastcall_Endmessage( (struct SEP_FastCallHdr *)aSendBuffer,
	                               (struct SEP_MsgAreaHdr_In *)(aSendBuffer + sizeof(struct SEP_FastCallHdr)),
	                               sepSramOffset );

	fastcallStatus = FVOS_Fastcall_Write( aSendBuffer, sepSramOffset );

	if ( 0 >= fastcallStatus ) {
		LOGERR( "Failed to send SEP message; FVOS_Fastcall_Write() "
		        "returned error value %d.\n", fastcallStatus );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Wait for Chaabi to return SEP message.
	//
	sepSramOffset = 0;

	fastcallStatus = FVOS_Fastcall_Read( aResponseBuffer, sizeof( aResponseBuffer ) );

	if ( 0 >= fastcallStatus ) {
		LOGERR( "Waiting for Chaabi response failed; FVOS_Fastcall_Read() "
		        "returned error value %d.\n", fastcallStatus );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Check the error status from Chaabi.
	//
	status = SEPDriver_Fastcall_Validate( (struct SEP_MsgAreaHdr_Out *)aResponseBuffer,
	                                      sepOpcode,
	                                      CRYS_OK,
	                                      &sepSramOffset );

	if ( DX_SUCCESS != status ) {
		if ( ( DX_FAILED_START_TOKEN_ERR == status ) ||
		     ( DX_WRONG_OPCODE_FROM_SEP_ERR == status ) )
		{
			//
			// Error occured in Fastcall validation.
			//
			LOGERR( "Error with Fastcall validation; "
			        "SEPDriver_Fastcall_Validate() returned error "
			        "value of 0x%lX.\n", status );

			return ( PMDB_FAIL_SEP_DRIVER_OP );
		}
		else
		{
			//
			// Error code is from Chaabi so return it to the caller.
			//
			LOGERR( "Chaabi returned error value of 0x%lX.\n", status );

			return ( (pmdb_result_t)status );
		}
	}


	//
	// Get the number of bytes that were read from the PMDB area.
	//
	status = SEPDriver_Fastcall_ReadParameter( (DxUint32_t)&pmdbReadSizeInBytes,
	                                           (DxUint32_t)sizeof( pmdbReadSizeInBytes ),
	                                           (DxUint32_t)SEP_MSG_PARAM_MAX_LEN( sizeof( pmdbReadSizeInBytes ) ),
	                                           &sepSramOffset,
	                                           DX_FALSE,
	                                           aResponseBuffer );

	if ( DX_SUCCESS != status ) {
		LOGERR( "Failed to read PMDB data size from Chaabi; "
		        "SEPDriver_Fastcall_ReadParameter() returned error value "
		        "0x%lX.\n", status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Does the number of bytes read from the PMDB area in Chaabi match the
	// requested number of bytes to read?
	//
	if ( pmdbReadSizeInBytes != dataSizeInBytes ) {
		LOGERR( "0x%lX bytes of 0x%X requested bytes were read from "
		        "PMDB.\n", pmdbReadSizeInBytes, dataSizeInBytes );

			return ( PMDB_FAIL_DATA_SIZE_MISMATCH_FAILURE );
	}


	//
	// Get the PMDB data byte array from the SEP message shared memory.
	//
	status = SEPDriver_Fastcall_ReadParameter( (DxUint32_t)pData,
	                                           (DxUint32_t)dataSizeInBytes,
	                                           (DxUint32_t)SEP_MSG_PARAM_MAX_LEN( dataSizeInBytes ),
	                                           &sepSramOffset,
	                                           DX_TRUE,
	                                           aResponseBuffer );

	if ( DX_SUCCESS != status ) {
		LOGERR( "Failed to read PMDB data from Chaabi; "
		        "SEPDriver_Fastcall_ReadParameter() returned error value "
		        "0x%lX.\n", status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Reading the Platform Management Database data in Chaabi succeeded.
	//
#endif  //  BAYTRAIL
	return ( PMDB_SUCCESSFUL );

}       //  sep_pmdb_read


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
                              const uint32_t        dataSizeInBytes )
{

#ifdef BAYTRAIL
	return (PMDB_FAIL_UNSUPPORTED);
#else

	DxUint8_t  aSendBuffer[ FAST_BUF_SIZE ], aResponseBuffer[ FAST_BUF_SIZE ];
	DxError_t  status = DX_ERROR;
	DxError_t  sepStatus = DX_ERROR;
	DxUint32_t sepOpcode = 0;
	DxUint32_t sepSramOffset = 0;
	DxUint32_t aSepMsg[ PMDB_SEP_MESSAGE_NUM_INDICES ];
	int        fastcallStatus = 0;


	//
	// Check parameters for illegal values.
	//
	if ( NULL == pData ) {
		//
		// PMDB return buffer pointer is NULL.
		//
		LOGERR( "Pointer to PMDB write data buffer is NULL.\n" );

		return ( PMDB_FAIL_INVALID_PARAMETER );
	}

	if ( dataSizeInBytes > MAX_PMDB_AREA_SIZE_IN_BYTES ) {
		//
		// The amount of data to write to PMDB is too large.
		//
		LOGERR( "Data size of 0x%X bytes is greater than the maximum of "
		        "0x%X bytes.\n", dataSizeInBytes,
		        MAX_PMDB_AREA_SIZE_IN_BYTES );

		return ( PMDB_FAIL_DATA_SIZE_TOO_LARGE_FAILURE );
	}


	//
	// Write the PMDB write parameters to the SEP message.
	//
	(void)memset( aSepMsg, 0, sizeof( aSepMsg ) );

	aSepMsg[ PMDB_SUB_OPCODE_SEP_MSG_INDEX ] = PMDB_WRITE_SUB_OPCODE;

	aSepMsg[ PMDB_DATA_SIZE_SEP_MSG_INDEX ] = (DxUint32_t)dataSizeInBytes;

	aSepMsg[ PMDB_WRITE_TYPE_SEP_MSG_INDEX ] = (DxUint32_t)pmdbArea;


	//
	// Initialize the SEP driver.
	//
	status = DX_CC_HostInit();

	if ( DX_SUCCESS != status ) {
		LOGERR( "SEP driver initialization failed; DX_CC_HostInit() return "
		        "error value of 0x%lX.\n", status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Initialize the SEP message.
	//
	sepSramOffset = 0;

	sepOpcode = DX_SEP_HOST_SEP_PROTOCOL_HOST_PMDB_ACCESS_OP_CODE;

	status = SEPDriver_Fastcall_InitHdr( (struct SEP_FastCallHdr *)aSendBuffer,
		(struct SEP_MsgAreaHdr_In *)(aSendBuffer + sizeof(struct SEP_FastCallHdr)),
		0,
		sepOpcode,
		sizeof(aSendBuffer),
		0,
		&sepSramOffset );

	if ( DX_SUCCESS != status ) {
		LOGERR( "Failed to initialize PMDB write SEP message; "
		        "SEPDriver_Fastcall_InitHdr() returned error value "
		        "0x%lX.\n", status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Write the SEP message parameters to the shared RAM.
	//
	status = SEPDriver_Fastcall_WriteParameter( (DxUint32_t)aSepMsg,
	                                            (DxUint32_t)sizeof( aSepMsg ),
	                                            (DxUint32_t)SEP_MSG_PARAM_MAX_LEN( sizeof( aSepMsg ) ),
	                                            &sepSramOffset,
	                                            DX_FALSE,
	                                            aSendBuffer );

	if ( DX_SUCCESS != status ) {
		LOGERR( "Failed to write PMDB write parameters to SEP message; "
		        "SEPDriver_Fastcall_WriteParameter() returned error value "
		        "0x%lX.\n", status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//


	// Write the PMDB write data byte array to the SEP message shared RAM.
	//
	status = SEPDriver_Fastcall_WriteParameter( (DxUint32_t)pData,
	                                            (DxUint32_t)dataSizeInBytes,
	                                            (DxUint32_t)SEP_MSG_PARAM_MAX_LEN( dataSizeInBytes ),
	                                            &sepSramOffset,
	                                            DX_TRUE,
	                                            aSendBuffer );

	if ( DX_SUCCESS != status ) {
		LOGERR( "Failed to write PMDB write data byte array to SEP message; "
		        "SEPDriver_Fastcall_WriteParameter() returned error value "
		        "0x%lX.\n", status );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Send the SEP message to Chaabi.
	//
	SEPDriver_Fastcall_Endmessage( (struct SEP_FastCallHdr *)aSendBuffer,
	                               (struct SEP_MsgAreaHdr_In *)(aSendBuffer + sizeof(struct SEP_FastCallHdr)),
	                               sepSramOffset );

	fastcallStatus = FVOS_Fastcall_Write( aSendBuffer, sepSramOffset );

	if ( 0 >= fastcallStatus ) {
		LOGERR( "Failed to send SEP message; FVOS_Fastcall_Write() "
		        "returned error value %d.\n", fastcallStatus );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Wait for Chaabi to return SEP message.
	//
	sepSramOffset = 0;

	fastcallStatus = FVOS_Fastcall_Read( aResponseBuffer, sizeof( aResponseBuffer ) );

	if ( 0 >= fastcallStatus ) {
		LOGERR( "Waiting for Chaabi response failed; FVOS_Fastcall_Read() "
		        "returned error value %d.\n", fastcallStatus );

		return ( PMDB_FAIL_SEP_DRIVER_OP );
	}


	//
	// Check the error status from Chaabi.
	//
	status = SEPDriver_Fastcall_Validate( (struct SEP_MsgAreaHdr_Out *)aResponseBuffer,
	                                      sepOpcode,
	                                      CRYS_OK,
	                                      &sepSramOffset );

	if ( DX_SUCCESS != status ) {
		if ( ( DX_FAILED_START_TOKEN_ERR == status ) ||
		     ( DX_WRONG_OPCODE_FROM_SEP_ERR == status ) )
		{
			//
			// Error occured in Fastcall validation.
			//
			LOGERR( "Error with Fastcall validation; "
			        "SEPDriver_Fastcall_Validate() returned error "
			        "value of 0x%lX.\n", status );

			return ( PMDB_FAIL_SEP_DRIVER_OP );
		}
		else
		{
			//
			// Error code is from Chaabi so return it to the caller.
			//
			LOGERR( "Chaabi returned error value of 0x%lX.\n", status );

			return ( (pmdb_result_t)status );
		}
	}


	//
	// Reading the Platform Management Database data in Chaabi succeeded.
	//
#endif  //  BAYTRAIL
	return ( PMDB_SUCCESSFUL );

}       //  sep_pmdb_write

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
int sep_rpmb_provision_key(void)
{
#ifdef BAYTRAIL
	return (PMDB_FAIL_UNSUPPORTED);
#else

	DxUint8_t snd_buf[SEP_MESSAGE_MAX_SIZE], res_buf[SEP_MESSAGE_MAX_SIZE];
	CRYSError_t crysStatus = ~CRYS_OK;
	DxError_t sepStatus = DX_ERROR;
	DxUint32_t uiDxSepMessageOpcode;
	DxUint32_t uiDxSepMessageSramOffset;
	DxUint32_t auiSepMessage[ RPMB_MAX_INDICES_OF_SEP_MSG ] = { 0 };

	crysStatus = DX_CC_HostInit();
	if (crysStatus != DX_SUCCESS) {
		LOGERR("DX_CC_HostInit() returned 0x%lX\n", crysStatus);
		return -crysStatus;
	}

	/* Initiate and start the SEP message. */
	uiDxSepMessageOpcode = DX_SEP_HOST_SEP_PROTOCOL_RPMB_ACCESS;
	uiDxSepMessageSramOffset = 0;

	sepStatus = SEPDriver_Fastcall_InitHdr((struct SEP_FastCallHdr *)snd_buf,
					       (struct SEP_MsgAreaHdr_In *)(snd_buf + sizeof(struct SEP_FastCallHdr)),
					       0,
					       uiDxSepMessageOpcode,
					       sizeof(snd_buf),
					       0,
					       &uiDxSepMessageSramOffset);
	if (sepStatus != DX_OK) {
		LOGERR("SEPDriver_Fastcall_InitHdr() returned 0x%lX\n",
		       sepStatus);
		return DX_FASTCALL_WRITE_FAILURE;
	}

	/* Write the message with the Provision Key Opcode */
	auiSepMessage[ RPMB_OPCODE_SEP_MSG_INDEX ] = RPMB_SUBOPCODE_PROVISION_KEY;

	sepStatus = SEPDriver_Fastcall_WriteParameter((DxUint32_t)auiSepMessage,
						      (DxUint32_t)sizeof( auiSepMessage ),
						      (DxUint32_t)SEP_MSG_PARAM_MAX_LEN( sizeof( auiSepMessage ) ),
						      &uiDxSepMessageSramOffset,
						      DX_FALSE, snd_buf);

	if (sepStatus != DX_OK) {
		LOGERR("SEPDriver_Fastcall_WriteParameter() for msg returned 0x%lX\n",
		       sepStatus);
		return DX_FASTCALL_WRITE_FAILURE;
	}

	SEPDriver_Fastcall_Endmessage((struct SEP_FastCallHdr *)snd_buf,
				      (struct SEP_MsgAreaHdr_In *)(snd_buf + sizeof(struct SEP_FastCallHdr)),
				      uiDxSepMessageSramOffset);

	sepStatus = FVOS_Fastcall_Write(snd_buf, uiDxSepMessageSramOffset);
	/* sepStatus contains the number of bytes written, 0 or -1
	 * indicates an error. */
	if (sepStatus <= 0) {
		LOGERR("FVOS_Fastcall_Write returned 0x%lX\n", sepStatus);
		return DX_FASTCALL_WRITE_FAILURE;
	}

	uiDxSepMessageSramOffset = 0;

	sepStatus = FVOS_Fastcall_Read(res_buf, sizeof(res_buf));

	if (sepStatus <= 0) {
		LOGERR("FVOS_Fastcall_Read() returned 0x%lX\n",
		       sepStatus);
		return DX_FASTCALL_READ_FAILURE;
	}

	sepStatus = SEPDriver_Fastcall_Validate((struct SEP_MsgAreaHdr_Out *)res_buf,
						uiDxSepMessageOpcode, CRYS_OK, &uiDxSepMessageSramOffset);

	if (sepStatus != DX_OK) {
		LOGERR("SEPDriver_Fastcall_Validate() returned 0x%lX\n",
		       sepStatus);
		return DX_FASTCALL_VALIDATE_FAILURE;
	}
#endif	//BAYTRAIL
	return RPMB_KEY_PROV_SUCCESS;
}
