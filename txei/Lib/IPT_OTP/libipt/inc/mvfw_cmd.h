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

#ifndef __MVFW_CMD_H__
#define __MVFW_CMD_H__

/*!
 * Enums
 */
enum
{
    /*! MVFW command ids */
    MVFW_BASE_CMD_ID = 0x70000000,
    MVFW_INIT_CMD_ID,
    MVFW_DEINIT_CMD_ID,
    MVFW_CONNECT_CMD_ID,
    MVFW_DISCONNECT_CMD_ID,
    MVFW_SEND_MSG_CMD_ID,
    MVFW_TEST_CMD_ID,
    MVFW_LAST_CMD_ID
};


#endif /* end __MVFW_CMD_H__ */

