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
//#define LOG_NDEBUG 0

#define LOG_TAG "VPPMultiDisplay"

#include "VPPMds.h"
#include <utils/Log.h>
#include "VPPWorker.h"

namespace android {

#ifdef TARGET_HAS_MULTIPLE_DISPLAY
VPPMDSListener::VPPMDSListener(VPPProcessorBase* vppprocessor)
    : BnMultiDisplayListener(), mMode(0), mVppState(false),
      mVpp(vppprocessor), mListenerId(-1), mHdmiClient(NULL), mMds(NULL) {
    ALOGI("A new Mds listener is created");
}

status_t VPPMDSListener::init() {
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == NULL) {
        ALOGE("%s: Fail to get service manager", __func__);
        return STATUS_ERROR;
    }

    mMds = interface_cast<IMDService>(sm->getService(String16(INTEL_MDS_SERVICE_NAME)));
    if (mMds == NULL) {
        ALOGE("%s: Failed to get MDS service", __func__);
        return STATUS_ERROR;
    }

    // Get HDMI interfaces to query HDMI device capability
    mHdmiClient = mMds->getHdmiControl();
    if (mHdmiClient == NULL) {
        ALOGE("%s: Failed to get MdsHdmiController service", __func__);
        return STATUS_ERROR;
    }

    sp<IMultiDisplaySinkRegistrar> sinkRegistrar = NULL;
    if ((sinkRegistrar = mMds->getSinkRegistrar()) == NULL)
        return STATUS_ERROR;

    mListenerId = sinkRegistrar->registerListener(this,
            "VPPSetting", MDS_MSG_MODE_CHANGE);
    ALOGV("MDS listener ID %d", mListenerId);

    sp<IMultiDisplayInfoProvider> mdsInfoProvider = mMds->getInfoProvider();
    if (mdsInfoProvider == NULL) {
        ALOGE("%s: Failed to get info provider", __func__);
        return STATUS_ERROR;
    }
    mMode = mdsInfoProvider->getDisplayMode(false);
    ALOGI("%s: The initial display mode is set to %d", __func__, mMode);

    if (mVpp != NULL)
        mVpp->setDisplayMode(mMode);

    return STATUS_OK;
}

void VPPMDSListener::deInit() {
    if ((mListenerId != -1) && mMds != NULL) {
        sp<IMultiDisplaySinkRegistrar> sinkRegistrar = NULL;
        if ((sinkRegistrar = mMds->getSinkRegistrar()) == NULL) {
            ALOGE("Failed to get Mds Sink registrar");
            return;
        }

        sinkRegistrar->unregisterListener(mListenerId);
        ALOGI("A Mds listener Id %d is distroyed", mListenerId);
        mListenerId = -1;
    }

}

VPPMDSListener::~VPPMDSListener() {
    ALOGI("A Mds listener %p is distroyed", this);
    mMode = MDS_MODE_NONE;
    mVppState = false;
    mVpp = NULL;
    mHdmiClient = NULL;
    mMds = NULL;

    if (mListenerId != -1) {
        ALOGW("%s: register a MDS listener function but not unregiser", __func__);
    }

    return;
}

status_t VPPMDSListener::onMdsMessage(int msg, void* value, int size) {
    if ((msg & MDS_MSG_MODE_CHANGE) && (size == sizeof(int))) {
        mMode = *(static_cast<int*>(value));
        ALOGI("Display mode is %d", mMode);
        if (mVpp != NULL)
            mVpp->setDisplayMode(mMode);
    }

    return NO_ERROR;
}

int32_t VPPMDSListener::getMode() {
    //ALOGV("Mds mode 0x%x", mMode);
    return mMode;
}

bool VPPMDSListener::getVppState() {
    ALOGV("MDS Vpp state %d", mVppState);
    return mVppState;
}

/* This function get HDMI devices capability (supported timing list).
 */
status_t VPPMDSListener::getHdmiMetaData(MDSHdmiTiming *curHdmiTiming,
              MDSHdmiTiming *frameRateLst,  int32_t *frameRateLstCount) {

    if ((frameRateLst == NULL) || (curHdmiTiming == NULL) || (frameRateLstCount == NULL)) {
        return STATUS_ERROR;
    }

    memset(curHdmiTiming, 0, sizeof(MDSHdmiTiming));
    *frameRateLstCount = -1;

    if (!(mMode & MDS_HDMI_CONNECTED)) {
        ALOGE("Hdmi is NOT connected. Failed to get HDMI data.");
        return STATUS_ERROR;
    }

    if (mHdmiClient != NULL) {
        //block to qurey display mode
        *frameRateLstCount = mHdmiClient->getHdmiTimingCount();
        int32_t dispayTimingCount = *frameRateLstCount;
        if (dispayTimingCount > 0) {
            ALOGV("HDMI Dispay device TimingCount %d.",dispayTimingCount);
            memset(frameRateLst, 0, dispayTimingCount * sizeof(MDSHdmiTiming));

            MDSHdmiTiming *lst[dispayTimingCount];
            memset(lst, 0, dispayTimingCount * sizeof(MDSHdmiTiming*));
            for (int32_t i = 0; i < dispayTimingCount; i++) {
                lst[i] = frameRateLst + i;
            }

            status_t status = mHdmiClient->getHdmiTimingList(dispayTimingCount, lst);
            if (status != BAD_VALUE) {
                ALOGV("HDMI num of Dispaytime list %d", dispayTimingCount);
                for (int32_t i = 0; i < dispayTimingCount; i++) {
                    ALOGV("HDMI timeing list wxh %dx%d@%dHz",
                              frameRateLst[i].width, frameRateLst[i].height, frameRateLst[i].refresh);
                }

                status = mHdmiClient->getCurrentHdmiTiming(curHdmiTiming);
                if (status != BAD_VALUE) {
                    ALOGV("HDMI curre timeing wxh %dx%d@%dHz",
                              curHdmiTiming->width, curHdmiTiming->height, curHdmiTiming->refresh);
                }

            } else {
                ALOGE("Failed to get HDMI timing list");
            }
        } //end if (dispayTimingCount > 0)
    } else {
        ALOGE("Hdmi service handle is NOT set.");
        return STATUS_ERROR;
    }

    return STATUS_OK;
}
#endif
}; // namespace android
