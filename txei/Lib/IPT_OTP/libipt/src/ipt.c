/*************************************************************************
 ** Copyright (C) 2013 Intel Corporation. All rights reserved.          **
 **                                                                     **
 ** Permission is hereby granted, free of charge, to any person         **
 ** obtaining a copy of this software and associated documentation      **
 ** files (the "Software"), to deal in the Software without             **
 ** restriction, including without limitation the rights to use, copy,  **
 ** modify, merge, publish, distribute, sublicense, and/or sell copies  **
 ** of the Software, and to permit persons to whom the Software is      **
 ** furnished to do so, subject to the following conditions:            **
 **                                                                     **
 ** The above copyright notice and this permission notice shall be      **
 ** included in all copies or substantial portions of the Software.     **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,     **
 ** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF  **
 ** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND               **
 ** NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS **
 ** BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN  **
 ** ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN   **
 ** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE    **
 ** SOFTWARE.                                                           **
 *************************************************************************/

#include <inttypes.h>
#include <string.h>

#include "tee_types.h"
#include <sys/types.h>
#include "tee_if.h"

#include "ipt_tee_interface.h"
#include "ipt_api.h"
#include "ipt_error.h"
#include "ipt_cmd_structs.h"

/* define LOG related macros before including the log header file */
#define LOG_STYLE    LOG_STYLE_LOGDAEMON
#define LOG_TAG      "libipt"
#include "sepdrm-log.h"

//TEMP
#include <time.h>

// {A62E16D1-70BC-47AA-BEA8-7E9E420B7BB3}
const GUID IPT_HECI_CLIENT_GUID = { 0xa62e16d1, 0x70bc, 0x47aa, { 0xbe, 0xa8, 0x7e, 0x9e, 0x42, 0xb, 0x7b, 0xb3 } };

/*!
 * @brief
 */
uint32_t ipt_init()
{
    return IPT_SUCCESS;
}

/*!
 * @brief
 */
uint32_t ipt_deinit()
{
    return IPT_SUCCESS;
}

/*!
 * @brief
 */
uint32_t ipt_connect()
{
    return IPT_SUCCESS;
}

/*!
 * @brief
 */
uint32_t ipt_disconnect()
{
    return IPT_SUCCESS;
}

struct ipt_send_msg_cmd_from_host g_req_param;
struct ipt_send_msg_cmd_to_host g_resp_param;
/*
 * @brief ipt send/receive data
 */
uint32_t ipt_send_message(uint16_t in_data_length,
                          uint8_t *in_data,
                          uint16_t *out_data_length,
                          uint8_t *out_data)
{
    uint32_t ret = IPT_SUCCESS;
    struct data_buffer cmd_data_in;
    struct data_buffer cmd_data_out;
	void *ptrHandle;
    struct timeval t_val; // TEMP

    Enter("");

    if (in_data == NULL ||
        out_data_length == NULL ||
        out_data == NULL)
    {
        LOGERR("Invalid NULL params.\n");
        return IPT_FAIL_NULL_PARAM;
    }

    if (in_data_length == 0 ||
        in_data_length > DWORD_ALIGNED_SIZE_OF_UNION_MSG_REQ)
    {
        LOGERR("Invalid data size.\n");
        return IPT_FAIL_INVALID_PARAM;
    }

	ret = ipt_tee_intf_init(&IPT_HECI_CLIENT_GUID, &ptrHandle);
	if ((ret != IPT_SUCCESS) || (ptrHandle == NULL)) {
		LOGERR("Failed to init IPT TEE interface, err=0x%X\n", ret);
		return ret;
	}

    /*! Initialize API return params */
    memset(out_data, 0, *out_data_length);

    /*! Initialize fw parameters to zero */
    memset(&g_req_param, 0, sizeof(g_req_param));
    memset(&g_resp_param, 0, sizeof(g_resp_param));
    memset(&cmd_data_in, 0, sizeof(cmd_data_in));
    memset(&cmd_data_out, 0, sizeof(cmd_data_out));

	g_req_param.header.cmd_id = IPT_SEND_MSG_CMD_ID;
	g_req_param.header.status = IPT_SUCCESS;

    /* As Chaabi is dword aligned, we have to make sure
     * each data field in the data blob that we send to it
     * is aligned to dwords, in order to avoid unexpected
     * alignment behaviors.
     * With this casting effort, we may isolate libiha
     * from h/w specific requirements on data alignment.
     */
    g_req_param.in_data_length = (uint32_t)in_data_length;
    g_req_param.expected_out_data_length = (uint32_t)(*out_data_length);

    /* Initialize fw parameters to data values */
    memcpy(g_req_param.in_data, in_data, in_data_length);

    //TODO: remove this once SRTC is enabled in the firmware
    gettimeofday(&t_val, NULL);
    g_req_param.time = t_val.tv_sec;

    /*! Initialize TEE parameters */
    INIT_FROM_HOST_PARAM_BUF(cmd_data_in, &g_req_param, sizeof(g_req_param));
    INIT_TO_HOST_PARAM_BUF(cmd_data_out, &g_resp_param, sizeof(g_resp_param));

	/* Currently the second argument cmd_id is NOT being used.
	 * So just pass it with IPT_SEND_MSG_CMD_ID.
	 *
	 * Likely, the last argument num_params is NOT being used.
	 * So just pass it with 1, which is the number of data_buffer
	 * structs that is passed in/out.
	 */
    ret = process_cmd(ptrHandle, IPT_SEND_MSG_CMD_ID, &cmd_data_in, &cmd_data_out, 1);
    if (ret != IPT_SUCCESS) {
        LOGERR("process_cmd failed. error(0x%x)\n", ret);
        goto disconnect_mei;
    }

    /* Only the lower two bytes of out_data_length is used. This
     * is safe when copying out_data_length bytes of data to out_data.
     */
	if (g_resp_param.header.status != IPT_SUCCESS) {
		LOGERR("IPT command failed status=%u\n", g_resp_param.header.status);
		ret = g_resp_param.header.status;
		goto disconnect_mei;
	}

    *out_data_length = (uint16_t)(g_resp_param.out_data_length);
	if (g_resp_param.out_data_length != 0) {
	    memcpy(out_data, g_resp_param.out_data, (uint16_t)(g_resp_param.out_data_length));
	}

disconnect_mei:
	if (ptrHandle != NULL)
	{
		if ((ret = ipt_tee_intf_deinit(ptrHandle)) != IPT_SUCCESS)
		{
			LOGERR("Failed to deinit IPT TEE interface, err=%u\n", ret);
		}
		ptrHandle = NULL;
	}

    return ret;
}
