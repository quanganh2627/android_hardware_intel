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

#ifndef __VPP_SETTING_H
#define __VPP_SETTING_H

namespace android {

typedef enum {
    VPP_COMMON_ON   = 1,        // VPP is on
    VPP_FRC_ON      = 1 << 1,   // FRC is on
} VPP_SETTING_STATUS;

class VPPSetting{
public:
    VPPSetting();
    ~VPPSetting();
    static bool isVppOn(unsigned int* status);

private:
    friend class VPPWorker;
    friend class VPPProcessor;
    friend class NuPlayerVPPProcessor;

private:
    VPPSetting(const VPPSetting &);
    VPPSetting &operator=(const VPPSetting &);
#ifdef TARGET_VPP_USE_GEN
    static bool detectUserId(int* id);
#endif
};
} /* namespace android */

#endif /* __VPP_SETTING_H */
