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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>

#include "tee_types.h"
#include <sys/types.h>
#include "tee_if.h"

#include "ipt_cmd_structs.h"
#include "ipt_tee_interface.h"

#include "mvfw_api.h"
#include "mvfw_cmd.h"
#include "mvfw_error.h"

/* define LOG related macros before including the log header file */
#define LOG_STYLE    LOG_STYLE_LOGDAEMON
#define LOG_TAG      "libipt"
#include "sepdrm-log.h"

// {A62E16D1-70BC-47AA-BEA8-7E9E420B7BB3}
extern GUID IPT_HECI_CLIENT_GUID;

extern struct ipt_send_msg_cmd_from_host g_req_param;
extern struct ipt_send_msg_cmd_to_host g_resp_param;

static uint32_t mvfw_cmd(
		const uint32_t cmd_id,
		const uint8_t * const in_data,
		const uint32_t in_data_length,
		uint8_t * const out_data,
		uint32_t * const out_data_length)
{
	uint32_t ret = MVFW_SUCCESS;
	struct data_buffer cmd_data_in;
	struct data_buffer cmd_data_out;
	void *ptrHandle;

	Enter("cmd_id=0x%X\n", cmd_id);

	if (cmd_id == MVFW_SEND_MSG_CMD_ID) {
		if ((in_data == NULL) || (in_data_length == 0) ||
				(in_data_length > MAX_MSG_FROM_HOST_LENGTH_IN_BYTES) ||
				(out_data == NULL) || (out_data_length == NULL)) {
			LOGERR("Invalid input\n");
			return MVFW_FAIL_INVALID_PARAM;
		}
	}

	ret = ipt_tee_intf_init(&IPT_HECI_CLIENT_GUID, &ptrHandle);
	if ((ret != MVFW_SUCCESS) || (ptrHandle == NULL)) {
		LOGERR("Failed to init IPT TEE interface, err=0x%X\n", ret);
		return ret;
	}

	/*! Initialize fw parameters to zero */
	memset(&g_req_param, 0, sizeof(g_req_param));
	memset(&g_resp_param, 0, sizeof(g_resp_param));
	memset(&cmd_data_in, 0, sizeof(cmd_data_in));
	memset(&cmd_data_out, 0, sizeof(cmd_data_out));

	g_req_param.header.cmd_id = cmd_id;
	g_req_param.header.status = MVFW_SUCCESS;

	g_resp_param.header.cmd_id = cmd_id;
	g_resp_param.header.status = MVFW_SUCCESS;

	if (cmd_id == MVFW_SEND_MSG_CMD_ID) {
		/* Initialize fw parameters to data values */
		g_req_param.in_data_length = in_data_length;
		g_req_param.expected_out_data_length = *out_data_length;
		memcpy(g_req_param.in_data, in_data, in_data_length);

		g_resp_param.out_data_length = *out_data_length;
	}

	/*! Initialize TEE parameters */
	INIT_FROM_HOST_PARAM_BUF(cmd_data_in, &g_req_param, sizeof(g_req_param));
	INIT_TO_HOST_PARAM_BUF(cmd_data_out, &g_resp_param, sizeof(g_resp_param));

	/* Currently the second argument cmd_id is NOT being used.
	 * So just pass it with cmd_id.
	 *
	 * Likely, the last argument num_params is NOT being used.
	 * So just pass it with 1, which is the number of data_buffer
	 * structs that is passed in/out.
	 */
	ret = process_cmd(ptrHandle, cmd_id, &cmd_data_in, &cmd_data_out, 1);
	if (ret) {
		LOGERR("tee_process_cmd failed. error(0x%x)\n", ret);
		goto disconnect_mei;
	}

	/* The return status from the firmware is carried back in g_resp_param.header */
	ret = g_resp_param.header.status;

	if (cmd_id == MVFW_SEND_MSG_CMD_ID) {
		if (ret != MVFW_SUCCESS) {
			LOGERR("MVFW command process failed, status=%u\n", g_resp_param.header.status);
			goto disconnect_mei;
		}

		*out_data_length = g_resp_param.out_data_length;
		if (g_resp_param.out_data_length != 0) {
			memcpy(out_data, g_resp_param.out_data, g_resp_param.out_data_length);
		}
	}

disconnect_mei:
	if (ptrHandle != NULL)
	{
		if (ipt_tee_intf_deinit(ptrHandle) != MVFW_SUCCESS)
		{
			LOGERR("Failed to deinit IPT TEE interface\n");
		}
		ptrHandle = NULL;
	}

	LOGDBG("Returned ipt_header.status=0x%X\n", g_resp_param.header.status);

	return ret;
}

uint32_t mvfw_sep_init(void)
{
	/* libiha library, which is calling this API for EPID provisioning
	 * initialization, is expecting a non-zero value as success return.
	 * So we simply return 1 to maintain the back-compatibility of libiha.
	 */
	return 1;
}

uint32_t mvsepfw_initialize(void)
{
	return mvfw_cmd(MVFW_INIT_CMD_ID, NULL, 0, NULL, NULL);

}

uint32_t mvsepfw_deinitialize(void)
{
	return mvfw_cmd(MVFW_DEINIT_CMD_ID, NULL, 0, NULL, NULL);
}

uint32_t mvsepfw_connect(void)
{
	return mvfw_cmd(MVFW_CONNECT_CMD_ID, NULL, 0, NULL, NULL);
}

uint32_t mvsepfw_disconnect(void)
{
	return mvfw_cmd(MVFW_DISCONNECT_CMD_ID, NULL, 0, NULL, NULL);
}

/**
 * mvsepfw_sendmessage
 *
 * Send message to Chaabi FW and receive response.
 *
 * Parameters
 *  @param[in]     in_data - Pointer to message to be sent.
 *  @param[in]     in_data_length - Length of message to be sent.
 *  @param[out]    out_data  - Pointer to buffer to receive response.
 *  @param[in/out] out_data_length - Pointer to length of output buffer.
 *      Updated to response length upon successful return.
 *
 */
uint32_t mvsepfw_sendmessage(const uint8_t *in_data,
							 const uint32_t in_data_length,
							 uint8_t *out_data,
							 uint32_t *out_data_length)
{
	uint32_t ret = MVFW_SUCCESS;

	Enter("");
	/* Check parameters */
	if ((in_data == NULL) || (in_data_length == 0) ||
			(out_data == NULL) || (out_data_length == NULL)) {
		LOGERR("Invalid input\n");
		return MVFW_FAIL_INVALID_PARAM;
	}

	/*! Initialize API return params */
	memset(out_data, 0, *out_data_length);

	ret = mvfw_cmd(MVFW_SEND_MSG_CMD_ID, in_data, in_data_length, out_data, out_data_length);
	if (ret) {
		LOGERR("tee_process_cmd failed. error(0x%x)\n", ret);
		return ret;
	}

	return MVFW_SUCCESS;
}
