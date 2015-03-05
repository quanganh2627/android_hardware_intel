/*
 * Copyright (c) 2013 Intel Corporation
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

#ifndef __LIBCAMERA2_IDVS_H__
#define __LIBCAMERA2_IDVS_H__

#include "ICameraHwControls.h"
#include "IAtomIspObserver.h"

namespace android {
class IDvs : public IAtomIspObserver {

public:
    /* **********************************************************
     * Constructor/Destructor
     */
    IDvs(HWControlGroup &hwcg): mIsp(hwcg.mIspCI), mSensorCI(hwcg.mSensorCI) {};
    virtual ~IDvs() {};
    static IDvs* createAtomDvs(HWControlGroup &hwcg);
    virtual status_t dvsInit() = 0;
    virtual status_t reconfigure() = 0;
    virtual bool isDvsValid() = 0;
    virtual status_t setZoom(int zoom) = 0;

protected:
    IHWIspControl *mIsp;
    IHWSensorControl *mSensorCI;

};

};
#endif /* LIBCAMERA2_IDVS_H */
