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

#ifndef __IPT_CMD_STRUCTS_H__
#define __IPT_CMD_STRUCTS_H__

#include "ipt_cmd.h"
#include "tee_types.h"
#include "iha_proto.h"

#define FROM_HOST_PARAM_INDEX           0
#define FROM_HOST_DATA_INDEX            1
#define TO_HOST_PARAM_INDEX             0
#define TO_HOST_DATA_INDEX              1

#define MAX_EPID_MSG_LENGTH_IN_BYTES    (2048)
#define MAX_MSG_FROM_HOST_LENGTH_IN_BYTES \
	MAX(MAX_EPID_MSG_LENGTH_IN_BYTES, DWORD_ALIGNED_SIZE_OF_UNION_MSG_REQ)
#define MAX_MSG_TO_HOST_LENGTH_IN_BYTES \
	MAX(MAX_EPID_MSG_LENGTH_IN_BYTES, DWORD_ALIGNED_SIZE_OF_UNION_MSG_RESP)

struct ipt_header
{
    uint32_t cmd_id;
    uint32_t status;
};

/* As Chaabi core aligns everything to dwords, we have
 * to make sure each individual field in the structs
 * align to dwords as well, in order to avoid unexpected
 * pointer deferenence behaviors.
 */
/*! Data send/recv data structures */
struct ipt_send_msg_cmd_from_host
{
    struct ipt_header header;
    uint32_t in_data_length;
    uint32_t expected_out_data_length;
    uint8_t in_data[MAX_MSG_FROM_HOST_LENGTH_IN_BYTES];
    uint32_t time; // TEMP
};

struct ipt_send_msg_cmd_to_host
{
    struct ipt_header header;
    uint32_t out_data_length;
    uint8_t out_data[MAX_MSG_TO_HOST_LENGTH_IN_BYTES];
};

#endif /* end __IPT_CMD_STRUCTS_H__ */

