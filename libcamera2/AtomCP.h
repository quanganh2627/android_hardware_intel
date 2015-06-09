/*
 * Copyright (C) 2011 The Android Open Source Project
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
 */

#ifndef ANDROID_LIBCAMERA_ATOM_CP
#define ANDROID_LIBCAMERA_ATOM_CP

#include <utils/Errors.h>
#include <utils/threads.h>
#include "ICameraHwControls.h"
#include <ia_cp_types.h>
#include "AtomAIQ.h"

extern "C" {
#include <stdlib.h>
#include <linux/atomisp.h>
}

namespace android {

struct CiUserBuffer {
    ia_frame *ciMainBuf;
    ia_frame *ciPostviewBuf;
    ia_cp_histogram *hist;
    size_t ciBufNum;
};

#ifdef ENABLE_INTEL_EXTRAS
#define STUB
#define STAT_STUB
#else
#define STUB {}
#define STAT_STUB {return INVALID_OPERATION;}
#endif

class AtomCP {

public:
    AtomCP(HWControlGroup &hwcg)STUB;
    ~AtomCP()STUB;
    status_t composeHDR(const CiUserBuffer& inputBuf, const CiUserBuffer& outputBuf, const ia_aiq_gbce_results gbce_results)STAT_STUB;
    status_t initializeHDR(unsigned width, unsigned height, ia_binary_data *aiqb_data)STAT_STUB;
    status_t uninitializeHDR(void)STAT_STUB;
    static status_t setIaFrameFormat(ia_frame *inputBuf, int v4l2Format)STAT_STUB;

    void initIACP(void)STAT_STUB;
    void deinitIACP(void)STAT_STUB;

    ia_cp_context * getIaCpContext(void) const { return mIaCpContext; }

// prevent copy constructor and assignment operator
private:
    AtomCP(const AtomCP& other);
    AtomCP& operator=(const AtomCP& other);

private:
    ia_env mPrintFunctions;
    ia_acceleration mAccAPI;
    IHWIspControl *mISP;
    Mutex mLock;
    ia_cp_context *mIaCpContext;
    ia_cp_hdr *mIaCpHdr;
    ia_cp_hdr_cfg *mIntelHdrCfg;
    bool mInitIACP;
};

};

#endif /* ANDROID_LIBCAMERA_ATOM_CP */
