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

#include "exif/ExifCreater.h"
#include "I3AControls.h"
#include <camera/CameraParameters.h>

#ifndef EXIFMAKER_H_
#define EXIFMAKER_H_

namespace android {


class EXIFMaker {
private:
    ExifCreater encoder;
    I3AControls *m3AControls;
    exif_attribute_t exifAttributes;
    int thumbWidth;
    int thumbHeight;
    size_t exifSize;
    bool initialized;

    void initializeLocation(const CameraParameters &params);
    void clear();
public:
    EXIFMaker(I3AControls *aaaControls);
    ~EXIFMaker();

    void initialize(const CameraParameters &params, int zoomRatio);
    bool isInitialized() { return initialized; }
    void setMakerNote(const ia_binary_data &aaaMkNoteData);
    void setDriverData(const atomisp_makernote_info &ispData);
    void setSensorAeConfig(const SensorAeConfig &sensorParams);
    void pictureTaken();
    void enableFlash();
    void setThumbnail(unsigned char *data, size_t size);
    bool isThumbnailSet() const;
    size_t makeExif(unsigned char **data);
    void setMaker(const char *data);
    void setModel(const char *data);
    void setSoftware(const char *data);

// prevent copy constructor and assignment operator
private:
    EXIFMaker(const EXIFMaker& other);
    EXIFMaker& operator=(const EXIFMaker& other);
};

}; // namespace android

#endif /* EXIFMAKER_H_ */
