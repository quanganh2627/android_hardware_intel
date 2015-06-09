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

#define LOG_TAG "Camera_V4L2Subdev"

#include "AtomCommon.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "v4l2device.h"

namespace android {


////////////////////////////////////////////////////////////////////
//                          PUBLIC METHODS
////////////////////////////////////////////////////////////////////


V4L2Subdevice::V4L2Subdevice(const char *name, int anId): V4L2DeviceBase(name,anId)
{
    LOG1("@%s", __FUNCTION__);
}

V4L2Subdevice::~V4L2Subdevice()
{
    LOG1("@%s", __FUNCTION__);

}


////////////////////////////////////////////////////////////////////
//                          PRIVATE METHODS
////////////////////////////////////////////////////////////////////


}; // namespace android



