/*
 * Copyright (c) 2012 Intel Corporation.
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

#ifndef ANDROID_LIBCAMERA_ATOM_DVS2
#define ANDROID_LIBCAMERA_ATOM_DVS2

#include <utils/Errors.h>
#include "ICameraHwControls.h"
#include "IAtomIspObserver.h"
#include "IDvs.h"

extern "C" {
#include <stdlib.h>
#include <linux/atomisp.h>
#include <ia_dvs_2.h>
}

namespace android {

class AtomDvs2 : public IDvs {

public:
    AtomDvs2(HWControlGroup &hwcg);
    ~AtomDvs2();

    status_t dvsInit();
    status_t reconfigure();
    bool isDvsValid();
    // overrides from IAtomIspObserver
    bool atomIspNotify(Message *msg, const ObserverState state);

    status_t setZoom(int zoom);

    const static float maxDvs2YUVDSRatio = 1.3f;

// prevent copy constructor and assignment operator
private:
    AtomDvs2(const AtomDvs2& other);
    AtomDvs2& operator=(const AtomDvs2& other);

private:
    status_t reconfigureNoLock();
    status_t run();
    status_t allocateDvs2Statistics(atomisp_dvs_grid_info info);
    status_t allocateDvs2MorphTable();
    void writeBinaryDump(const char *binary_dump_file);
    static void debugPrint(const char *fmt, va_list ap);
    bool isHighSpeedDvsSupported(int width, int height);
    bool isDvsSupportedSize(int width, int height);

private:
    Mutex mLock;

    ia_dvs2_binary_dump_params mDumpParams;
    static bool mDumpLogEnabled;
    ia_env mDvs2Env;
    struct atomisp_dis_statistics mStatistics;
    struct atomisp_dvs2_statistics *mDvs2stats;
    ia_dvs2_state *mState;
    ia_dvs2_configuration mDvs2Config;
    atomisp_dvs_6axis_config *mMorphTable;
    bool mDVSEnabled;
    bool mZoomRatioChanged;
};

};

#endif /* ANDROID_LIBCAMERA_ATOM_DVS2 */
