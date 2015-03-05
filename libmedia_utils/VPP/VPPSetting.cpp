/*
 * Copyright (C) 2012 Intel Corporation.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "VPPSetting.h"
#include <stdio.h>
#include <string.h>

static const char StatusOn[][5] = {"1frc", "1vpp"};
static const char VPP_STATUS_STORAGE[] = "/data/data/com.intel.vpp/shared_prefs/vpp_settings.xml";

namespace android {

VPPSetting::VPPSetting() {}
VPPSetting::~VPPSetting() {}

bool VPPSetting::FRCStatus = false;
bool VPPSetting::VPPStatus = false;

bool VPPSetting::isVppOn()
{
    FILE *handle = fopen(VPP_STATUS_STORAGE, "r");
    if(handle == NULL)
        return false;

    const int MAXLEN = 1024;
    char buf[MAXLEN] = {0};
    memset(buf, 0 ,MAXLEN);
    if(fread(buf, 1, MAXLEN, handle) <= 0) {
        fclose(handle);
        return false;
    }
    buf[MAXLEN - 1] = '\0';

    if(strstr(buf, StatusOn[0]) != NULL) {
        FRCStatus = true;
    } else {
        FRCStatus = false;
    }
    if(strstr(buf, StatusOn[1]) != NULL) {
        VPPStatus = true;
    } else {
        VPPStatus = false;
    }

    fclose(handle);
    return (FRCStatus || VPPStatus);
}

} //namespace android

