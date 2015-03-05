/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef EXIFFIELDS_H_
#define EXIFFIELDS_H_

#include <Exif.h>

namespace android {

class EXIFFields {

// constructor / destructor
public:
    EXIFFields();
    ~EXIFFields();

// public methods
public:
    void reset();

    void setGeneralFields(bool flashEnabled,
                          int pictureWidth,
                          int pictureHeight,
                          int thumbnailWidth,
                          int thumbnailHeight,
                          ExifOrientationType orientation);

    void setGPSFields(long timestamp,
                      float latitude,
                      float longitude,
                      float altitude,
                      const char *processingMethod);

    void setHardwareFields(float focalLength,
                           unsigned int fNumber,                // num/denom
                           CamExifExposureProgramType exposureProgram,
                           CamExifExposureModeType exposureMode,
                           int exposureTime,
                           int aperture,
                           float brightness,
                           float exposureBias,
                           int isoSpeed,
                           CamExifMeteringModeType meteringMode,
                           CamExifWhiteBalanceType wbMode,
                           CamExifSceneCaptureType sceneMode);

    void combineFields(exif_attribute_t *exif);

private:
    void setCommonFields();
    void setUnknownFields();

// private data
private:
    exif_attribute_t mExif;
};

}; // namespace android

#endif // EXIFFIELDS_H_
