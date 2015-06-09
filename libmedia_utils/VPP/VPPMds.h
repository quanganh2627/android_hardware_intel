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

#ifndef _VPP_MDS_H_
#define _VPP_MDS_H_

#ifdef TARGET_HAS_MULTIPLE_DISPLAY
#include <display/MultiDisplayService.h>
#include <display/IMultiDisplayListener.h>
#include <display/MultiDisplayType.h>
#endif

#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <VPPSetting.h>
#include "VPPProcessorBase.h"

#ifdef TARGET_HAS_MULTIPLE_DISPLAY
using namespace android :: intel;

namespace android {
class VPPProcessorBase;

class VPPMDSListener : public BnMultiDisplayListener {
private:
    int32_t     mMode;
    bool    mVppState;
    VPPProcessorBase* mVpp;
    int32_t     mListenerId;
    sp<IMultiDisplayHdmiControl> mHdmiClient;
    sp<IMDService> mMds;

public:
    VPPMDSListener(VPPProcessorBase*);
    ~VPPMDSListener();

    status_t init();
    void deInit();
    status_t onMdsMessage(int msg, void* value, int size);
    int32_t  getMode();
    bool getVppState();
    status_t getHdmiMetaData(MDSHdmiTiming *curHdmiTiming,
              MDSHdmiTiming *frameRateLst,  int32_t *dispayTimingCount);
};
};
#else

/** @brief HDMI timing structure */
#define MDS_HDMI_CONNECTED (1<<1)
typedef struct {
    int         width;
    int         height;
    int         refresh;    /**< refresh rate */
    int         interlace;  /**< 1:interlaced 0:progressive */
    int         ratio;      /**< aspect ratio */
    int         flags;      /**< expended flag */
} MDSHdmiTiming;

namespace android {

class VPPProcessorBase;
//namespace intel {

class VPPMDSListener : public RefBase {
private:

public:
    VPPMDSListener(VPPProcessorBase*){};
    ~VPPMDSListener(){};

    status_t init(){ return 0; };
    void deInit(){};
    //status_t onMdsMessage(int msg, void* value, int size);
    //int32_t  getMode();
    //bool getVppState();
    status_t getHdmiMetaData(MDSHdmiTiming *curHdmiTiming,
              MDSHdmiTiming *frameRateLst,  int32_t *dispayTimingCount){ return 0; };
};
//}; // namespace android
};
#endif //TARGET_HAS_MULTIPLE_DISPLAY
#endif //_VPP_MDS_H_
