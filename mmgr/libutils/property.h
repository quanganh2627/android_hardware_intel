/* Modem Manager - Android property header file
**
** Copyright (C) Intel 2012
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*/

#ifndef __MMGR_PROPERTY_HEADER_FILE__
#define __MMGR_PROPERTY_HEADER_FILE__

#include <cutils/properties.h>
#include "errors.h"

e_mmgr_errors_t property_set_int(const char *key, int value);
e_mmgr_errors_t property_get_int(const char *key, int *value);
e_mmgr_errors_t property_get_string(const char *key, char *value);

#endif                          /* __MMGR_PROPERTY_HEADER_FILE__ */
