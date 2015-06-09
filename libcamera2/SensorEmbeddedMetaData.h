/*
 * Copyright (C) 2014 Intel Corporation
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
#ifndef ANDROID_LIBCAMERA_SENSOREMBEDDEDMETADATA_H
#define ANDROID_LIBCAMERA_SENSOREMBEDDEDMETADATA_H

#include <utils/threads.h>
#include <utils/Vector.h>
#include <ia_types.h>
#include <ia_aiq_types.h>
#include <ia_aiq.h>
#include <ia_emd_decoder.h>

#include "ICameraHwControls.h"

namespace android {

class SensorEmbeddedMetaData {
public:
    SensorEmbeddedMetaData(HWControlGroup &hwcg, int cameraId);
    virtual ~SensorEmbeddedMetaData();

    status_t init();
    status_t getDecodedExposureParams(ia_aiq_exposure_sensor_parameters* sensor_exp_p,
                                      ia_aiq_exposure_parameters* generic_exp_p, unsigned int exp_id = 0);
    status_t getDecodedMiscParams(ia_emd_misc_parameters_t *misc_parameters_p, unsigned int exp_id);
    status_t handleSensorEmbeddedMetaData();
    bool isSensorEmbeddedMetaDataSupported() { return mSensorMetaDataSupported; }

// prevent copy constructor and assignment operator
private:
    SensorEmbeddedMetaData(const SensorEmbeddedMetaData& other);
    SensorEmbeddedMetaData& operator=(const SensorEmbeddedMetaData& other);

//private types
private:
    /*!
     * \brief Common structure for the decoded parameters for Sensor Metadata
     *
     * The structure is a combination of all the sensor metadata needed for AIQ
     * and applications, it should be extended when sensor supports more sensor
     * metadata
     */
    struct decoded_sensor_metadata {
         unsigned int exp_id;
         ia_aiq_exposure_sensor_parameters *sensor_units_p;
         ia_aiq_exposure_parameters *generic_units_p;
         ia_emd_misc_parameters_t *misc_parameters_p;
    };

    enum {
         SENSOR_EXPOSURE_EXIST = 1,
         GENERAL_EXPOSURE_EXIST = 2,
         MISC_PARAMETERS_EXIST = 4
    };

//const data
private:
    static const int MAX_SENSOR_METADATA_QUEUE_SIZE = 7;

    static const int MAX_FRAME_COUNT_ARRAY_SIZE = 3;
    static const int MAX_FRAME_COUNT = 255;
    static const int MIN_FRAME_COUNT = 0;
    static const int FRAME_COUNT_MAX_GAP = 2;
    static const int INIT_FRAME_COUNT = 1;

    struct SbsSensorsFrameCount {
        int index;
        bool hasInit;
        int delta;
        bool hasDelta;
        int firstFrameCount[MAX_FRAME_COUNT_ARRAY_SIZE];
        int secondFrameCount[MAX_FRAME_COUNT_ARRAY_SIZE];
    };

private:
    Mutex mLock;
    IHWIspControl *mISP;
    Vector<decoded_sensor_metadata> mSensorEmbeddedMetaDataStoredQueue;
    //EMBEDDED METADATA
    atomisp_metadata mSensorEmbeddedMetaData;
    ia_emd_decoder_t *mEmbeddedMetaDecoderHandler;
    ia_emd_mode_t mEmbeddedDataMode;
    ia_binary_data mEmbeddedDataBin;
    bool mSensorMetaDataSupported;
    int32_t mSensorMetaDataConfigFlag;
    int mCameraId;
    bool mSbsMetadata;

    SbsSensorsFrameCount mSbsSensorsFrameCount;
private:
    status_t initSensorEmbeddedMetaDataQueue();
    void deinitSensorEmbeddedMetaDataQueue();
    status_t decodeSensorEmbeddedMetaData();
    status_t storeDecodedMetaData();

    status_t decodeSbsSensorsEmbeddedMetaData();

    bool getSbsSensorInitFrameCount(int index, int lastIndex, int &count);
};

} /* namespace android */
#endif /* ANDROID_LIBCAMERA_SENSOREMBEDDEDMETADATA_H */

