/**********************************************************************
* Copyright (C) 2013 Intel Corporation. All rights reserved.

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

#ifndef __IPT_ERROR_H__
#define __IPT_ERROR_H__

enum
{
    IPT_SUCCESS = 0,

    /*! TEE PARAM errors */
    IPT_FAIL_INVALID_PARAM,
    IPT_FAIL_NULL_PARAM,
    IPT_FAIL_INVALID_COMMAND,
    IPT_FAIL_HMAC_VALIDATION,
    IPT_FAIL_CRYPTO_AES,

    IPT_FAILURE = 0xFFFFFFFF,
};


#endif // __IPT_ERROR_H_
