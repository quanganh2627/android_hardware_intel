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
#define LOG_TAG "Camera_SensorEmbeddedMetaData"

#include <utils/Errors.h>
#include "LogHelper.h"
#include "ia_cmc_parser.h"
#include "SensorEmbeddedMetaData.h"
#include "PlatformData.h"

namespace android {

SensorEmbeddedMetaData::SensorEmbeddedMetaData(HWControlGroup &hwcg, int cameraId)
            :mISP(hwcg.mIspCI)
            ,mEmbeddedMetaDecoderHandler(NULL)
            ,mSensorMetaDataSupported(false)
            ,mSensorMetaDataConfigFlag(0)
            ,mCameraId(cameraId)
            ,mSbsMetadata(false)
{
    LOG2("@%s", __FUNCTION__);
    CLEAR(mEmbeddedDataBin);
    CLEAR(mSensorEmbeddedMetaData);
    CLEAR(mEmbeddedDataMode);
    mSbsSensorsFrameCount.index = 0;
    mSbsSensorsFrameCount.hasInit = false;
    mSbsSensorsFrameCount.delta = 0;
    mSbsSensorsFrameCount.hasDelta = false;
    memset(mSbsSensorsFrameCount.firstFrameCount, -1, sizeof(mSbsSensorsFrameCount.firstFrameCount));
    memset(mSbsSensorsFrameCount.secondFrameCount, -1, sizeof(mSbsSensorsFrameCount.secondFrameCount));
}

SensorEmbeddedMetaData::~SensorEmbeddedMetaData()
{
    LOG2("@%s", __FUNCTION__);
    deinitSensorEmbeddedMetaDataQueue();
    free(mSensorEmbeddedMetaData.data);
    free(mSensorEmbeddedMetaData.effective_width);
    ia_emd_decoder_deinit(mEmbeddedMetaDecoderHandler);
}

status_t SensorEmbeddedMetaData::init()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if(!PlatformData::supportedSensorMetadata(mCameraId))
        return NO_ERROR;

    ia_binary_data cpfData;
    CLEAR(cpfData);
    if (PlatformData::sensorType(mCameraId) == SENSOR_TYPE_SOC) {
        int size = 0;
        const char *data = NULL;
        if (PlatformData::HalConfig[mCameraId].getValue(size, CPF::EmdHeadFile, CPF::Size))
            return NO_ERROR;
        if (PlatformData::HalConfig[mCameraId].getBool(mSbsMetadata, CPF::EmdHeadFile, CPF::SbsMetadata))
            return NO_ERROR;
        data = PlatformData::HalConfig[mCameraId].getString(CPF::EmdHeadFile, CPF::Data);
        if (data != NULL) {
            unsigned char *tmp = (unsigned char*)malloc(sizeof(unsigned char) * size);
            char *endptr = NULL;
            if (tmp != NULL) {
                for (int i = 0; i < size; ++i) {
                    tmp[i] = strtol(data, &endptr, 16);
                    data = endptr + 1;
                }
                cpfData.data = (void*)tmp;
                cpfData.size = size;
                mEmbeddedMetaDecoderHandler = ia_emd_decoder_init(&cpfData);
                free(tmp);
                tmp = NULL;
            }
        }
    } else {
        if (PlatformData::AiqConfig[mCameraId]) {
            cpfData.data = PlatformData::AiqConfig[mCameraId].ptr();
            cpfData.size = PlatformData::AiqConfig[mCameraId].size();
            mEmbeddedMetaDecoderHandler = ia_emd_decoder_init(&cpfData);
        }
    }

    if (mEmbeddedMetaDecoderHandler == NULL)
        return UNKNOWN_ERROR;

    //get embedded metadata buffer size
    struct atomisp_parm isp_params;
    mISP->getIspParameters(&isp_params);
    int height = isp_params.metadata_config.metadata_height;
    int width = isp_params.metadata_config.metadata_stride;
    int size = height * width;

    if (size > 0) {
        mSensorEmbeddedMetaData.data = (void*) malloc (size);
        if (mSensorEmbeddedMetaData.data == NULL) {
            status = NO_MEMORY;
            goto errorFree;
        }

        // effective_width is an array recording the effect data size for each line.
        mSensorEmbeddedMetaData.effective_width = (uint32_t*) malloc (height * sizeof(uint32_t));
        if (mSensorEmbeddedMetaData.effective_width == NULL) {
            status = NO_MEMORY;
            goto errorFree;
        }

        mEmbeddedDataBin.data = mSensorEmbeddedMetaData.data;
        mEmbeddedDataBin.size = size;
        status = initSensorEmbeddedMetaDataQueue();
        if (status == NO_ERROR)
            mSensorMetaDataSupported = true;

    } else {
        // sensor doesn't support sensor embedded metadata
        return UNKNOWN_ERROR;
    }

    return status;

errorFree:
    if (mSensorEmbeddedMetaData.data) {
        free(mSensorEmbeddedMetaData.data);
        mSensorEmbeddedMetaData.data = NULL;
    }
    if (mSensorEmbeddedMetaData.effective_width) {
        free(mSensorEmbeddedMetaData.effective_width);
        mSensorEmbeddedMetaData.effective_width = NULL;
    }
    return status;
}

status_t SensorEmbeddedMetaData::initSensorEmbeddedMetaDataQueue()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mSensorEmbeddedMetaDataStoredQueue.clear();
    mSensorEmbeddedMetaDataStoredQueue.setCapacity(MAX_SENSOR_METADATA_QUEUE_SIZE);
    decoded_sensor_metadata metadata;
    for(unsigned int i = 0; i < mSensorEmbeddedMetaDataStoredQueue.capacity(); i++) {
        CLEAR(metadata);
        metadata.sensor_units_p = (ia_aiq_exposure_sensor_parameters*) malloc (sizeof(ia_aiq_exposure_sensor_parameters));
        if (metadata.sensor_units_p == NULL) {
            status = NO_MEMORY;
            goto errorFree;
        }
        memset(metadata.sensor_units_p, 0, sizeof(ia_aiq_exposure_sensor_parameters));
        metadata.generic_units_p = (ia_aiq_exposure_parameters*) malloc (sizeof(ia_aiq_exposure_parameters));
        if (metadata.generic_units_p == NULL) {
            status = NO_MEMORY;
            goto errorFree;
        }
        memset(metadata.generic_units_p, 0, sizeof(ia_aiq_exposure_parameters));
        metadata.misc_parameters_p = (ia_emd_misc_parameters_t*) malloc (sizeof(ia_emd_misc_parameters_t));
        if (metadata.misc_parameters_p == NULL) {
            status = NO_MEMORY;
            goto errorFree;
        }
        memset(metadata.misc_parameters_p, 0, sizeof(ia_emd_misc_parameters_t));
        metadata.exp_id = 0;
        mSensorEmbeddedMetaDataStoredQueue.push_front(metadata);
    }
    return status;

errorFree:
    for(size_t i = 0; i < mSensorEmbeddedMetaDataStoredQueue.size(); i++) {
        if (mSensorEmbeddedMetaDataStoredQueue.itemAt(i).sensor_units_p) {
            free(metadata.sensor_units_p);
            metadata.sensor_units_p = NULL;
        }

        if (mSensorEmbeddedMetaDataStoredQueue.itemAt(i).generic_units_p) {
            free(metadata.generic_units_p);
            metadata.generic_units_p = NULL;
        }

        if (mSensorEmbeddedMetaDataStoredQueue.itemAt(i).misc_parameters_p) {
            free(metadata.misc_parameters_p);
            metadata.misc_parameters_p = NULL;
        }
    }
    return status;
}

void SensorEmbeddedMetaData::deinitSensorEmbeddedMetaDataQueue()
{
    LOG2("@%s", __FUNCTION__);

    for (Vector<decoded_sensor_metadata>::iterator it = mSensorEmbeddedMetaDataStoredQueue.begin(); it != mSensorEmbeddedMetaDataStoredQueue.end(); ++it) {
        if (it->sensor_units_p) {
            free(it->sensor_units_p);
            it->sensor_units_p = NULL;
        }

        if (it->generic_units_p) {
            free(it->generic_units_p);
            it->generic_units_p = NULL;
        }

        if (it->misc_parameters_p) {
            free(it->misc_parameters_p);
            it->misc_parameters_p = NULL;
        }
    }
    mSensorEmbeddedMetaDataStoredQueue.clear();
}

/**
  * New sensor metadata available
  * the sensor metadata buffer is from ISP, after parsing by decoder,
  * the decoded results should be stored in Vector.
  */
status_t SensorEmbeddedMetaData::handleSensorEmbeddedMetaData()
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (!mSensorMetaDataSupported)
        return UNKNOWN_ERROR;

    //deque the embedded metadata
    if (mISP && mSensorEmbeddedMetaData.data && mSensorEmbeddedMetaData.effective_width) {
        mISP->getSensorEmbeddedMetaData(&mSensorEmbeddedMetaData);
        mEmbeddedDataMode.exp_id = mSensorEmbeddedMetaData.exp_id;
        mEmbeddedDataMode.stride = mSensorEmbeddedMetaData.stride;
        mEmbeddedDataMode.height = mSensorEmbeddedMetaData.height;
        mEmbeddedDataMode.effective_width = (int32_t*)mSensorEmbeddedMetaData.effective_width;
        LOG2("exp_id=%d, stride=%d, height=%d", mEmbeddedDataMode.exp_id, mEmbeddedDataMode.stride,
              mEmbeddedDataMode.height);
    }

    if (mSbsMetadata)
        status = decodeSbsSensorsEmbeddedMetaData();
    else
        status = decodeSensorEmbeddedMetaData();

    if (status == NO_ERROR)
        status = storeDecodedMetaData();

    return status;
}

/**
 * pop the sensor metadata from the stored queue according to the exp_id
 * if the value of "exp_id" is equal to 0, means don't need to sync sensor metadata with exp_id,
 * just use the oldest one
 */
 status_t SensorEmbeddedMetaData::getDecodedExposureParams(ia_aiq_exposure_sensor_parameters* sensor_exp_p
                       , ia_aiq_exposure_parameters* generic_exp_p, unsigned int exp_id)
{
    Mutex::Autolock lock(mLock);
    LOG2("@%s exp_id:%u", __FUNCTION__, exp_id);
    status_t status = UNKNOWN_ERROR;

    if (!mSensorMetaDataSupported || sensor_exp_p == NULL || generic_exp_p == NULL)
        return status;

    Vector<decoded_sensor_metadata>::iterator it = mSensorEmbeddedMetaDataStoredQueue.begin();
    for (;it != mSensorEmbeddedMetaDataStoredQueue.end(); ++it) {
        if (it->exp_id == exp_id || (exp_id == 0 && it->exp_id != 0)) {
            if ((mSensorMetaDataConfigFlag & SENSOR_EXPOSURE_EXIST) && it->sensor_units_p != NULL) {
                memcpy(sensor_exp_p, it->sensor_units_p, sizeof(ia_aiq_exposure_sensor_parameters));
                LOG2("get metadata: expid: %u, sensor_exposure_params fine_integration: %d, coarse_integration:%d, ag:%d, dg:%d",
                     exp_id,
                     sensor_exp_p->fine_integration_time,
                     sensor_exp_p->coarse_integration_time,
                     sensor_exp_p->analog_gain_code_global,
                     sensor_exp_p->digital_gain_global);
            }
            if ((mSensorMetaDataConfigFlag & GENERAL_EXPOSURE_EXIST) && it->generic_units_p != NULL) {
                memcpy(generic_exp_p, it->generic_units_p, sizeof(ia_aiq_exposure_parameters));
            }

            return NO_ERROR;
        }
    }
    return status;
}

status_t SensorEmbeddedMetaData::getDecodedMiscParams(ia_emd_misc_parameters_t* misc_parameters_p, unsigned int exp_id)
{
    Mutex::Autolock lock(mLock);
    LOG2("@%s exp_id:%u", __FUNCTION__, exp_id);
    status_t status = UNKNOWN_ERROR;

    if (!mSensorMetaDataSupported || misc_parameters_p == NULL || !(mSensorMetaDataConfigFlag & MISC_PARAMETERS_EXIST))
        return status;

    Vector<decoded_sensor_metadata>::iterator it = mSensorEmbeddedMetaDataStoredQueue.begin();
    for (;it != mSensorEmbeddedMetaDataStoredQueue.end(); ++it) {
        if ((it->exp_id == exp_id) && (it->misc_parameters_p != NULL)) {
            memcpy(misc_parameters_p, it->misc_parameters_p, sizeof(ia_emd_misc_parameters_t));
            LOG2("it->exp_id=%d, misc_parameters_p->frame_counter=%d", it->exp_id, misc_parameters_p->frame_counter);
            return NO_ERROR;
        }
    }
    LOG2("No sensor metadata exp_id = %d in current queue.", exp_id);
    return status;
}

/**
  * decode sensor metadata buffer by iq_tool
  */
status_t SensorEmbeddedMetaData::decodeSensorEmbeddedMetaData()
{
    LOG2("@%s", __FUNCTION__);
    ia_err err = ia_err_none;
    status_t ret = NO_ERROR;

    err = ia_emd_decoder_run(&mEmbeddedDataBin, &mEmbeddedDataMode, mEmbeddedMetaDecoderHandler);
    if (err !=  ia_err_none) {
        LOGW("decoder error ret:%d", err);
        ret = UNKNOWN_ERROR;
    }
    if (mSensorMetaDataConfigFlag == 0) {
        if ((mEmbeddedMetaDecoderHandler->decoded_data).sensor_units_p) {
            mSensorMetaDataConfigFlag |= SENSOR_EXPOSURE_EXIST;
            LOG2("decoded metadata: sensor_exposure_params fine_integration: %d, coarse_integration:%d, ag:%d, dg:%d",
                 (mEmbeddedMetaDecoderHandler->decoded_data).sensor_units_p->fine_integration_time,
                 (mEmbeddedMetaDecoderHandler->decoded_data).sensor_units_p->coarse_integration_time,
                 (mEmbeddedMetaDecoderHandler->decoded_data).sensor_units_p->analog_gain_code_global,
                 (mEmbeddedMetaDecoderHandler->decoded_data).sensor_units_p->digital_gain_global);
        }

        if ((mEmbeddedMetaDecoderHandler->decoded_data).generic_units_p) {
            mSensorMetaDataConfigFlag |= GENERAL_EXPOSURE_EXIST;
        }

        if ((mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p) {
            mSensorMetaDataConfigFlag |= MISC_PARAMETERS_EXIST;
            LOG2("decoded metadata: misc_parameters frame count: %d",
                 (mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p->frame_counter);
        }
    }

    return ret;
}

/**
  * store the sensor metadata into FIFO queque.
  */
status_t SensorEmbeddedMetaData::storeDecodedMetaData()
{
    Mutex::Autolock lock(mLock);
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    decoded_sensor_metadata new_stored_element;

    // Stored Q is full, pop the oldest buffer to fill the latest data.
    new_stored_element = mSensorEmbeddedMetaDataStoredQueue.top();
    mSensorEmbeddedMetaDataStoredQueue.pop();

    if (mSensorMetaDataConfigFlag & SENSOR_EXPOSURE_EXIST) {
        ia_aiq_exposure_sensor_parameters *sensor_exposure_params =
                                            new_stored_element.sensor_units_p;
        memcpy(sensor_exposure_params,
               (mEmbeddedMetaDecoderHandler->decoded_data).sensor_units_p,
                   sizeof(ia_aiq_exposure_sensor_parameters));
    }

    if (mSensorMetaDataConfigFlag & GENERAL_EXPOSURE_EXIST) {
        ia_aiq_exposure_parameters *generic_exposure_params =
                                            new_stored_element.generic_units_p;
        memcpy(generic_exposure_params,
               (mEmbeddedMetaDecoderHandler->decoded_data).generic_units_p,
                   sizeof(ia_aiq_exposure_parameters));
    }

    if (mSensorMetaDataConfigFlag & MISC_PARAMETERS_EXIST) {
        ia_emd_misc_parameters_t *misc_parameters = new_stored_element.misc_parameters_p;
        memcpy(misc_parameters, (mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p,
               sizeof(ia_emd_misc_parameters_t));
        LOG2("frame count=%d", misc_parameters->frame_counter);
    }

    new_stored_element.exp_id = mSensorEmbeddedMetaData.exp_id;
    LOG2("stored metadata exposure id: %u", new_stored_element.exp_id);
    mSensorEmbeddedMetaDataStoredQueue.push_front(new_stored_element);

    return status;
}

/**
 * For Sbs (side by side) sensors, one metadata includes two sensors information.
 * They use the same sensor controller and have same metadata information.
 * For Kevlar, we should let ov8858 control ov680 to output first frame at same time.
 * When we try to let main back sensor sync with Sbs sensors which are used for depth
 * mode capture, it will cause metadata information in chaos and disorder between Sbs sensors.
 * Frame count will be different in two Sbs sensors or same but larger than the
 * last one too much. It is the first frame from main back sensor and frame count
 * is 2. So we should make a workaround to change frame count of Sbs sensor to initial value
 * and let them sync with main back sensor. Then recalculate the frame count for
 * next frame always. The range of frame count is [1~255].
 * Here are the cases we handle:
 * ov680 frame count value(m, n), m indicate first frame count value, n indicate second
 * frame count value.
 * 1. (m != n): second frame is outputted by ov8858 sensor;
 * 2. (m, 0) or (0, n): one metadata is missing;
 * 3. (m == n), (m, n) --> (m+3, m+3): they are equal, but the frame count interval between
 *    two sensor metadatas is larger than 2, second frame is outputted by ov8858 sensor.
 */
status_t SensorEmbeddedMetaData::decodeSbsSensorsEmbeddedMetaData()
{
    LOG2("@%s", __FUNCTION__);
    ia_err err = ia_err_none;
    status_t ret = NO_ERROR;

    ia_binary_data embeddedDataBin;
    embeddedDataBin.data = mEmbeddedDataBin.data;
    embeddedDataBin.size = mEmbeddedDataBin.size / 2;
    err = ia_emd_decoder_run(&embeddedDataBin, &mEmbeddedDataMode, mEmbeddedMetaDecoderHandler);
    if (err !=  ia_err_none) {
        LOGW("decoder error ret:%d", err);
        ret = UNKNOWN_ERROR;
    }

    // Sbs sensor is SOC sensor, so we just need frame count to sync frames between sensors.
    if (mSensorMetaDataConfigFlag == 0) {
        if ((mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p) {
            mSensorMetaDataConfigFlag |= MISC_PARAMETERS_EXIST;
            LOG2("decoded metadata: misc_parameters frame count: %d",
                 (mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p->frame_counter);
        }
    }

    // Below is workarond for Kevlar special.
    if (mSensorMetaDataConfigFlag & MISC_PARAMETERS_EXIST) {
        int index = mSbsSensorsFrameCount.index;
        int arraySize = MAX_FRAME_COUNT_ARRAY_SIZE;
        int gap = FRAME_COUNT_MAX_GAP;
        int init = INIT_FRAME_COUNT;
        int max = MAX_FRAME_COUNT;

        if ((mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p) {
            mSbsSensorsFrameCount.firstFrameCount[index]
                    = (mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p->frame_counter;
        }

        embeddedDataBin.data = (char*)mEmbeddedDataBin.data + mEmbeddedDataMode.stride;
        embeddedDataBin.size = mEmbeddedDataBin.size / 2;
        err = ia_emd_decoder_run(&embeddedDataBin, &mEmbeddedDataMode, mEmbeddedMetaDecoderHandler);
        if (err !=  ia_err_none) {
            LOGW("decoder error ret:%d", err);
            ret = UNKNOWN_ERROR;
        }

        if ((mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p) {
            mSbsSensorsFrameCount.secondFrameCount[index]
                    = (mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p->frame_counter;

            LOG2("msbs: frame count = [%d, %d]", mSbsSensorsFrameCount.firstFrameCount[index], mSbsSensorsFrameCount.secondFrameCount[index]);

            // Sometimes one metadata of them may be missing.
            if (mSbsSensorsFrameCount.firstFrameCount[index] != 0
                && mSbsSensorsFrameCount.secondFrameCount[index] == 0) {
                mSbsSensorsFrameCount.secondFrameCount[index] = mSbsSensorsFrameCount.firstFrameCount[index];
            } else if (mSbsSensorsFrameCount.firstFrameCount[index] == 0
                && mSbsSensorsFrameCount.secondFrameCount[index] != 0) {
                mSbsSensorsFrameCount.firstFrameCount[index] = mSbsSensorsFrameCount.secondFrameCount[index];
            }

            if ((!mSbsSensorsFrameCount.hasInit) && (mSbsSensorsFrameCount.firstFrameCount[index]
                    != mSbsSensorsFrameCount.secondFrameCount[index]
                || (mSbsSensorsFrameCount.firstFrameCount[(index+arraySize-1) % arraySize] >= 0
                    && (mSbsSensorsFrameCount.firstFrameCount[index]
                        > (mSbsSensorsFrameCount.firstFrameCount[(index+arraySize-1) % arraySize] + gap)))
                || (mSbsSensorsFrameCount.secondFrameCount[(index+arraySize-1) % arraySize] >= 0
                    && (mSbsSensorsFrameCount.secondFrameCount[index]
                        > (mSbsSensorsFrameCount.secondFrameCount[(index+arraySize-1) % arraySize] + gap))))) {
                // reinitialize frame counter
                (mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p->frame_counter = init;
                mSbsSensorsFrameCount.hasInit = true;
            } else {
                if ((!mSbsSensorsFrameCount.hasDelta)
                    && ((mSbsSensorsFrameCount.firstFrameCount[(index+arraySize-1) % arraySize] >= 0
                        && mSbsSensorsFrameCount.firstFrameCount[(index+arraySize-1) % arraySize]
                            != mSbsSensorsFrameCount.secondFrameCount[(index+arraySize-1) % arraySize])
                      || (mSbsSensorsFrameCount.firstFrameCount[(index+arraySize-2) % arraySize] >= 0
                        && (mSbsSensorsFrameCount.firstFrameCount[(index+arraySize-1) % arraySize]
                            > (mSbsSensorsFrameCount.firstFrameCount[(index+arraySize-2) % arraySize] + gap)))
                      || (mSbsSensorsFrameCount.secondFrameCount[(index+arraySize-2) % arraySize] >= 0
                        &&(mSbsSensorsFrameCount.secondFrameCount[(index+arraySize-1) % arraySize]
                            > (mSbsSensorsFrameCount.secondFrameCount[(index+arraySize-2) % arraySize] + gap))))) {
                    // calculate the delta between real frame count and recalculated frame count
                    mSbsSensorsFrameCount.delta = mSbsSensorsFrameCount.firstFrameCount[index] - (init + 1);
                    mSbsSensorsFrameCount.hasDelta = true;
                }

                // recalculate frame count
                if (mSbsSensorsFrameCount.firstFrameCount[index] >= mSbsSensorsFrameCount.delta)
                    (mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p->frame_counter
                        = mSbsSensorsFrameCount.firstFrameCount[index] - mSbsSensorsFrameCount.delta;
                else {
                    (mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p->frame_counter
                        = mSbsSensorsFrameCount.firstFrameCount[index] + max + 1 - mSbsSensorsFrameCount.delta;
                }
            }

            LOG2("msbs: index=%d, delta=%d, frame count=%d"
                , mSbsSensorsFrameCount.index
                , mSbsSensorsFrameCount.delta
                , (mEmbeddedMetaDecoderHandler->decoded_data).misc_parameters_p->frame_counter);
            ++mSbsSensorsFrameCount.index;
            mSbsSensorsFrameCount.index %= arraySize;
        }
    }
    return ret;
}

} /* namespace android */

