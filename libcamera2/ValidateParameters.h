/*
 * Copyright (c) 2014 Intel Corporation.
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

#ifndef ANDROID_LIBCAMERA_VALIDATE_PARAMETERS_H
#define ANDROID_LIBCAMERA_VALIDATE_PARAMETERS_H

#include <camera/CameraParameters.h>
#include "IntelParameters.h"

namespace android {
    status_t validateParameters(const CameraParameters *oldParams, const CameraParameters *params, int cameraId);
    bool validateString(const char* value,  const char* supportList);
}

#endif // ANDROID_LIBCAMERA_VALIDATE_PARAMETERS_H

