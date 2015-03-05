/*
 * Copyright (C) 2013 Intel Corporation
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

#ifndef ANDROID_LIBCAMERA_IORIENTATION_LISTENER_H
#define ANDROID_LIBCAMERA_IORIENTATION_LISTENER_H

namespace android {

class IOrientationListener {

public:

    virtual ~IOrientationListener() {};

    /**
     * Callback function that is called when listener is registered
     *
     * \param orientation current orientation value 0, 90, 180 or 270
     *
     * \return none
     */
    virtual void orientationChanged(int orientation) = 0;
};

}; // namespace android

#endif // ANDROID_LIBCAMERA_IORIENTATION_LISTENER_H
