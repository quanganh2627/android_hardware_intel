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
#include <stdlib.h>
#include <utils/Log.h>

#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IBinder.h>

#include <utils/String16.h>

static const char StatusOn[][5] = {"1frc", "1vpp"};


namespace android {

VPPSetting::VPPSetting() {}
VPPSetting::~VPPSetting() {}

#ifdef TARGET_VPP_USE_GEN
bool VPPSetting::detectUserId(int* id)
{
    sp<IBinder> binder;
    //get service manager
    sp<IServiceManager> sm = defaultServiceManager();
    binder = sm->getService(String16("activity"));
    if(binder == 0){
        ALOGD("%s: get service failed", __func__);
        return false;
    }

    Parcel data, reply;
    data.writeInterfaceToken(String16("android.app.IActivityManager"));

    status_t ret = binder->transact(IBinder::FIRST_CALL_TRANSACTION + 144, data, &reply);

    if (ret == NO_ERROR ) {
        int exceptionCode    =  reply.readExceptionCode();
        if (!exceptionCode) {
            *id         = reply.readInt32();
            int retNum =0;

            size_t len;
            const char16_t* name = reply.readString16Inplace(&len);
            String8 sAddress(String16(name, len));

            const char16_t* icopath = reply.readString16Inplace(&len);
            String8 sicopath(String16(icopath, len));

            int flags         = reply.readInt32();
            int serialNum     = reply.readInt32();
            long createtime   = reply.readInt64();
            long lastLoggedTime  = reply.readInt64();
            int partial   = reply.readInt32();

            return true;
        } else {
            // An exception was thrown back; fall through to return failure
            ALOGD("detect user id caught exception %d\n", exceptionCode);
            return false;
        }
    }
    return false;
}
#endif

bool VPPSetting::isVppOn(unsigned int *status)
{
    char path[80];
    int userId = 0;
#ifdef TARGET_VPP_USE_GEN
    if (!detectUserId(&userId) || userId < 0) {
        ALOGD("%s: detect user Id error userId = %d", __func__, userId);
        return false;
    }
#endif

    snprintf(path, 80, "/data/user/%d/com.intel.vpp/shared_prefs/vpp_settings.xml", userId);
    ALOGI("%s: %s",__func__, path);
    FILE *handle = fopen(path, "r");
    if(handle == NULL)
        return false;

    const int MAXLEN = 1024;
    char buf[MAXLEN] = {0};
    memset(buf, 0 ,MAXLEN);
    if(fread(buf, 1, MAXLEN, handle) <= 0) {
        ALOGE("%s: failed to read vpp config file %d", __func__, userId);
        fclose(handle);
        return false;
    }
    buf[MAXLEN - 1] = '\0';

    *status = 0;
    if(strstr(buf, StatusOn[0]) != NULL)
        *status |= VPP_FRC_ON;

    if(strstr(buf, StatusOn[1]) != NULL)
        *status |= VPP_COMMON_ON;

    fclose(handle);
    return (*status != 0);
}

} //namespace android

