/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
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

/**
 *\file CameraProfiles.h
 *
 * parser for the camera xml configuration file
 *
 * This file calls the libexpat ditectly. The libexpat is one xml parser.
 * It will parse the camera configuration out firstly.
 * Then other module can call the methods of it to get the real configuration.
 *
 */

#ifndef ANDROID_LIBCAMERA_CAMERA_PROFILES_H
#define ANDROID_LIBCAMERA_CAMERA_PROFILES_H

#include "AtomCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

namespace android {

/**
 * \class CameraProfiles
 *
 * This class is used to parse the camera configuration file.
 * The configuration file is xml format.
 * This class will use the expat lib to do the xml parser.
 */
class CameraProfiles : public PlatformBase {
public:
    CameraProfiles(const Vector<SensorNameAndPort>& sensorNames = Vector<SensorNameAndPort>());
    ~CameraProfiles(){};

    unsigned getSensorNum(void) {return mSensorNum;};

// prevent copy constructor and assignment operator
private:
    CameraProfiles(const CameraProfiles& other);
    CameraProfiles& operator=(const CameraProfiles& other);

private:
    enum DataField {
        FIELD_INVALID = 0,
        FIELD_SENSOR_BACK,
        FIELD_SENSOR_FRONT,
        FIELD_COMMON
    } mCurrentDataField;
    unsigned mSensorNum;
    unsigned mCurrentSensor;
    bool mCurrentSensorIsExtendedCamera;
    CameraInfo *pCurrentCam;

    Vector<SensorNameAndPort> mSensorNames;

    static const int mBufSize = 4*1024;
    static void startElement(void *userData, const char *name, const char **atts);
    static void endElement(void *userData, const char *name);

    void getDataFromXmlFile(void);
    void checkField(CameraProfiles *profiles, const char *name, const char **atts);

    void handleSensor(CameraProfiles *profiles, const char *name, const char **atts);
    void handleFeature(CameraProfiles *profiles, const char *name, const char **atts);
    void handleCommon(CameraProfiles *profiles, const char *name, const char **atts);

    void dump(void);
};

}; // namespace android

#endif /* ANDROID_LIBCAMERA_CAMERA_PROFILES_H */
