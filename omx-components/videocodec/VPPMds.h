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
#include <VPPSetting.h>
#include "PPWorker.h"

using namespace android :: intel;
using namespace android;

class VPPWorker;

class VPPMDSListener : public BnMultiDisplayListener {
private:
    int     mMode;
    bool    mVppState;
    int     mListenerId;
    sp<IMDService> mMds;
    VPPWorker* mVpp;
public:
    VPPMDSListener(VPPWorker*);
    ~VPPMDSListener();

    status_t init();
    status_t deInit();
    status_t onMdsMessage(int msg, void* value, int size);
    int  getMode();
    bool getVppState();
};

#endif //TARGET_HAS_MULTIPLE_DISPLAY
#endif //_VPP_MDS_H_
