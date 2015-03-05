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

#include "ipt_internal.h"

#ifndef _IPTUTIL_H_
#define _IPTUTIL_H_

#ifdef __cplusplus
extern "C"
{
#endif

// Sigma defines
#define MAX(a, b)                  (((a) > (b)) ? (a) : (b))

#define OTP_MAX_S1_LEN            (200)
#define OTP_MAX_M1_LEN            (SIZE_OF_SVC_HDR + SIZE_OF_TLV_HDR + OTP_MAX_S1_LEN)
#define OTP_MAX_S2_LEN            (5000)
#define OTP_MAX_M3_SIZE           (2500)
#define OTP_MAX_GET_SVP_SIZE      (MAX(OTP_MAX_M1_LEN, OTP_MAX_M3_SIZE))

#define OTP_MAX_PROCESS_SVP_SIZE  (OTP_MAX_S2_LEN)

#ifdef __cplusplus
} // extern "C"
#endif

#endif //_IPUTIL_H_

