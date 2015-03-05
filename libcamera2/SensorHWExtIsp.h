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

#ifndef SENSORHWEXTISP_H
#define SENSORHWEXTISP_H

#include "SensorHW.h"
namespace android {

class SensorHWExtIsp : public SensorHW
{

public:
    SensorHWExtIsp(int cameraId);
    virtual ~SensorHWExtIsp();

    // SensorHW overrides:
    virtual int setAfMode(int mode);
    virtual int getAfMode(int *mode);
    virtual int setAfEnabled(bool enable);
    virtual int setAfWindows(const CameraWindow *windows, int numWindows);

    virtual int setAeFlashMode(int mode);

// Prevent copy constructor and assignment
private:
    SensorHWExtIsp(const SensorHWExtIsp &other);
    SensorHWExtIsp& operator=(const SensorHWExtIsp &other);

// Private functions:
private:
    int extIspIoctl(int id, int &data);

};

} // namespace android

#endif // SENSORHWEXTISP_H
