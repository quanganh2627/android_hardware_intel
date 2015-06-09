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
#define LOG_TAG "Camera_Profiles"

#include "LogHelper.h"
#include <string.h>
#include <libexpat/expat.h>
#include "PlatformData.h"
#include "CameraProfiles.h"
#include "IntelParameters.h"

namespace android {

CameraProfiles::CameraProfiles()
{
    mCurrentSensor = 0;
    mCurrentDataField = FIELD_INVALID;
    mSensorNum = 0;
    pCurrentCam = NULL;
    LOG1("@%s", __func__);

    getDataFromXmlFile();
//    dump();
}

/**
 * This function will check which field that the parser parses to.
 *
 * The field is set to 4 types.
 * FIELD_INVALID FIELD_SENSOR_BACK FIELD_SENSOR_FRONT and FIELD_COMMON
 *
 * \param profiles: the pointer of the CameraProfiles.
 * \param name: the element's name.
 * \param atts: the element's attribute.
 */
void CameraProfiles::checkField(CameraProfiles *profiles, const char *name, const char **atts)
{
    LOG1("@%s, name:%s", __func__, name);

    if (strcmp(name, "CameraSettings") == 0) {
        profiles->mCurrentDataField = FIELD_INVALID;
        return;
    } else if (strcmp(name, "Profiles") == 0
            && strcmp(atts[0], "cameraId") == 0) {
        profiles->mSensorNum++;
        profiles->mCurrentSensor = atoi(atts[1]);
        if (0 == profiles->mCurrentSensor || 1 == profiles->mCurrentSensor) {
            profiles->pCurrentCam = new CameraInfo;
            if (NULL == profiles->pCurrentCam) {
                ALOGE("@%s, Cannot create CameraInfo!", __func__);
                return;
            }
        }
        if (0 == profiles->mCurrentSensor) {
            profiles->mCurrentDataField = FIELD_SENSOR_BACK;
            return;
        }
        else if (1 == profiles->mCurrentSensor) {
            profiles->mCurrentDataField = FIELD_SENSOR_FRONT;
            return;
        }
    } else if (strcmp(name, "Common") == 0) {
        profiles->mCurrentDataField = FIELD_COMMON;
        return;
    }

    ALOGE("@%s, name:%s, atts[0]:%s, xml format wrong", __func__, name, atts[0]);
    return;
}

/**
 * This function will handle all the common related elements.
 *
 * It will be called in the function startElement
 *
 * \param profiles: the pointer of the CameraProfiles.
 * \param name: the element's name.
 * \param atts: the element's attribute.
 */
void CameraProfiles::handleCommon(CameraProfiles *profiles, const char *name, const char **atts)
{
    LOG1("@%s, name:%s, atts[0]:%s", __func__, name, atts[0]);

    if (strcmp(atts[0], "value") != 0) {
        ALOGE("@%s, name:%s, atts[0]:%s, xml format wrong", __func__, name, atts[0]);
        return;
    }

    if (strcmp(name, "subDevName") == 0) {
        PlatformBase::mSubDevName = atts[1];
    } else if (strcmp(name, "fileInject") == 0) {
        PlatformBase::mFileInject = ((strcmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "productName") == 0) {
        PlatformBase::mProductName = atts[1];
    } else if (strcmp(name, "manufacturerName") == 0) {
        PlatformBase::mManufacturerName = atts[1];
    } else if (strcmp(name, "maxZoomFactor") == 0) {
        PlatformBase::mMaxZoomFactor = atoi(atts[1]);
    } else if (strcmp(name, "supportVideoSnapshot") == 0) {
        PlatformBase::mSupportVideoSnapshot = ((strcmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "numRecordingBuffers") == 0) {
        PlatformBase::mNumRecordingBuffers = atoi(atts[1]);
    } else if (strcmp(name, "numPreviewBuffers") == 0) {
        PlatformBase::mNumPreviewBuffers = atoi(atts[1]);
    } else if (strcmp(name, "maxContinuousRawRingBuffer") == 0) {
        PlatformBase::mMaxContinuousRawRingBuffer = atoi(atts[1]);
    } else if (strcmp(name, "boardName") == 0) {
        PlatformBase::mBoardName = atts[1];
    } else if (strcmp(name, "previewFormat") == 0) {
        if (strcmp(atts[1], "V4L2_PIX_FMT_YVU420") == 0)
            PlatformBase::mPreviewFourcc = V4L2_PIX_FMT_YVU420;
        else if (strcmp(atts[1], "V4L2_PIX_FMT_YUYV") == 0) //Also known as YUY2
            PlatformBase::mPreviewFourcc = V4L2_PIX_FMT_YUYV;
        else
            PlatformBase::mPreviewFourcc = V4L2_PIX_FMT_NV12;
    } else if (strcmp(name, "shutterLagCompensationMs") == 0) {
        PlatformBase::mShutterLagCompensationMs = atoi(atts[1]);
    } else if (strcmp(name, "mPanoramaMaxSnapshotCount") == 0) {
        PlatformBase::mPanoramaMaxSnapshotCount = atoi(atts[1]);
    } else if (strcmp(name, "supportDualVideo") == 0) {
        PlatformBase::mSupportDualVideo = ((strcmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "supportPreviewLimitation") == 0) {
        PlatformBase::mSupportPreviewLimitation
            = ((strcmp(atts[1], "false") == 0) ? false : true);
    } else if (strcmp(name, "useULLImpl") == 0) {
        PlatformBase::mUseIntelULL = ((strcmp(atts[1], "IntelULL") == 0) ? true : false);
    } else if (strcmp(name, "faceCallbackDivider") == 0) {
        PlatformBase::mFaceCallbackDivider = atoi(atts[1]);
    }
}

/**
 * This function will handle all the sensor related elements.
 *
 * It will be called in the function startElement
 *
 * \param profiles: the pointer of the CameraProfiles.
 * \param name: the element's name.
 * \param atts: the element's attribute.
 */
void CameraProfiles::handleSensor(CameraProfiles *profiles, const char *name, const char **atts)
{
    LOG1("@%s, name:%s, atts[0]:%s, profiles->mCurrentSensor:%d", __func__, name, atts[0], profiles->mCurrentSensor);

    if (strcmp(atts[0], "value") != 0) {
        ALOGE("@%s, name:%s, atts[0]:%s, xml format wrong", __func__, name, atts[0]);
        return;
    }

    if (strcmp(name, "maxEV") == 0) {
        pCurrentCam->maxEV = atts[1];
    } else if (strcmp(name, "minEV") == 0) {
        pCurrentCam->minEV = atts[1];
    } else if (strcmp(name, "stepEV") == 0) {
        pCurrentCam->stepEV = atts[1];
    } else if (strcmp(name, "defaultEV") == 0) {
        pCurrentCam->defaultEV = atts[1];
    } else if (strcmp(name, "supportedPreviewSizes") == 0) {
        pCurrentCam->supportedPreviewSizes = atts[1];
    } else if (strcmp(name, "supportedVideoSizes") == 0) {
        pCurrentCam->supportedVideoSizes = atts[1];
    }else if (strcmp(name, "videoPreviewSizePref") == 0) {
        pCurrentCam->mVideoPreviewSizePref = atts[1];
    } else if (strcmp(name, "supportedSceneModes") == 0) {
        pCurrentCam->supportedSceneModes = atts[1];
    } else if (strcmp(name, "defaultSceneMode") == 0) {
        pCurrentCam->defaultSceneMode = atts[1];
    } else if (strcmp(name, "sensorType") == 0) {
        pCurrentCam->sensorType = ((strcmp(atts[1], "SENSOR_TYPE_RAW") == 0) ? SENSOR_TYPE_RAW : SENSOR_TYPE_SOC);
    } else if (strcmp(name, "facing") == 0) {
        pCurrentCam->facing = ((strcmp(atts[1], "CAMERA_FACING_FRONT") == 0) ? CAMERA_FACING_FRONT : CAMERA_FACING_BACK);
    } else if (strcmp(name, "orientation") == 0) {
        pCurrentCam->orientation = atoi(atts[1]);
    } else if (strcmp(name, "dvs") == 0) {
        pCurrentCam->dvs = ((strcmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "flipping") == 0) {
        pCurrentCam->flipping = PlatformData::SENSOR_FLIP_OFF; // reset NA to OFF first
        if (strcmp(atts[0], "value") == 0 && strcmp(atts[1], "SENSOR_FLIP_H") == 0)
            pCurrentCam->flipping |= PlatformData::SENSOR_FLIP_H;
        if (strcmp(atts[2], "value_v") == 0 && strcmp(atts[3], "SENSOR_FLIP_V") == 0)
            pCurrentCam->flipping |= PlatformData::SENSOR_FLIP_V;
    } else if (strcmp(name, "continuousCapture") == 0) {
        pCurrentCam->continuousCapture = ((strcmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "supportedSnapshotSizes") == 0) {
        pCurrentCam->supportedSnapshotSizes = atts[1];
    } else if (strcmp(name, "defaultBurstLength") == 0) {
        pCurrentCam->defaultBurstLength = atts[1];
    } else if (strcmp(name, "supportedBurstLength") == 0) {
        pCurrentCam->supportedBurstLength = atts[1];
        if (!strcmp(atts[1], ""))
            pCurrentCam->supportedBurstLength = "1";
    } else if (strcmp(name, "defaultFlashMode") == 0) {
        pCurrentCam->defaultFlashMode = atts[1];
    } else if (strcmp(name, "supportedFlashModes") == 0) {
        pCurrentCam->supportedFlashModes = atts[1];
    } else if (strcmp(name, "supportedEffectModes") == 0) {
        pCurrentCam->supportedEffectModes = atts[1];
    } else if (strcmp(name, "supportedIntelEffectModes") == 0) {
        pCurrentCam->supportedIntelEffectModes = atts[1];
    } else if (strcmp(name, "supportedAwbModes") == 0) {
        pCurrentCam->supportedAwbModes = atts[1];
    } else if (strcmp(name, "defaultAwbMode") == 0) {
        pCurrentCam->defaultAwbMode = atts[1];
    } else if (strcmp(name, "defaultIso") == 0) {
        pCurrentCam->defaultIso = atts[1];
    } else if (strcmp(name, "supportedIso") == 0) {
        pCurrentCam->supportedIso = atts[1];
    } else if (strcmp(name, "defaultAeMetering") == 0) {
        pCurrentCam->defaultAeMetering = atts[1];
    } else if (strcmp(name, "supportedAeMetering") == 0) {
        pCurrentCam->supportedAeMetering = atts[1];
    } else if (strcmp(name, "defaultFocusMode") == 0) {
        pCurrentCam->defaultFocusMode = atts[1];
    } else if (strcmp(name, "supportedFocusModes") == 0) {
        pCurrentCam->supportedFocusModes = atts[1];
    } else if (strcmp(name, "supportedBurstFPS") == 0) {
        pCurrentCam->supportedBurstFPS = atts[1];
    } else if (strcmp(name, "previewViaOverlay") == 0) {
        pCurrentCam->mPreviewViaOverlay = ((strcmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "VFPPLimitedResolutionList") == 0) {
        IntelCameraParameters::parseResolutionList(atts[1], pCurrentCam->mVFPPLimitedResolutions);
    } else if (strcmp(name, "ZSLUnsupportedSnapshotResolutionList") == 0) {
        IntelCameraParameters::parseResolutionList(atts[1], pCurrentCam->mZSLUnsupportedSnapshotResolutions);
    } else if (strcmp(name, "CVFUnsupportedSnapshotResolutionList") == 0) {
        IntelCameraParameters::parseResolutionList(atts[1], pCurrentCam->mCVFUnsupportedSnapshotResolutions);
    } else if (strcmp(name, "overlayRelativeRotation") == 0) {
        pCurrentCam->overlayRelativeRotation = atoi(atts[1]);
    } else if (strcmp(name, "maxSaturation") == 0) {
        pCurrentCam->maxSaturation = atts[1];
    } else if (strcmp(name, "minSaturation") == 0) {
        pCurrentCam->minSaturation = atts[1];
    } else if (strcmp(name, "stepSaturation") == 0) {
        pCurrentCam->stepSaturation = atts[1];
    } else if (strcmp(name, "defaultSaturation") == 0) {
        pCurrentCam->defaultSaturation = atts[1];
    } else if (strcmp(name, "supportedSaturation") == 0) {
        pCurrentCam->supportedSaturation = atts[1];
    } else if (strcmp(name, "maxContrast") == 0) {
        pCurrentCam->maxContrast = atts[1];
    } else if (strcmp(name, "minContrast") == 0) {
        pCurrentCam->minContrast = atts[1];
    } else if (strcmp(name, "stepContrast") == 0) {
        pCurrentCam->stepContrast = atts[1];
    } else if (strcmp(name, "defaultContrast") == 0) {
        pCurrentCam->defaultContrast = atts[1];
    } else if (strcmp(name, "supportedContrast") == 0) {
        pCurrentCam->supportedContrast = atts[1];
    } else if (strcmp(name, "maxSharpness") == 0) {
        pCurrentCam->maxSharpness = atts[1];
    } else if (strcmp(name, "minSharpness") == 0) {
        pCurrentCam->minSharpness = atts[1];
    } else if (strcmp(name, "stepSharpness") == 0) {
        pCurrentCam->stepSharpness = atts[1];
    } else if (strcmp(name, "defaultSharpness") == 0) {
        pCurrentCam->defaultSharpness = atts[1];
    } else if (strcmp(name, "supportedSharpness") == 0) {
        pCurrentCam->supportedSharpness = atts[1];
    } else if (strcmp(name, "defaultEffectMode") == 0) {
        pCurrentCam->defaultEffectMode = atts[1];
    } else if (strcmp(name, "supportedPreviewFrameRate") == 0) {
        pCurrentCam->supportedPreviewFrameRate = atts[1];
    } else if (strcmp(name, "supportedPreviewFPSRange") == 0) {
        pCurrentCam->supportedPreviewFPSRange = atts[1];
    } else if (strcmp(name, "defaultPreviewFPSRange") == 0) {
        pCurrentCam->defaultPreviewFPSRange = atts[1];
    } else if (strcmp(name, "supportedPreviewUpdateModes") == 0) {
        pCurrentCam->supportedPreviewUpdateModes = atts[1];
    } else if (strcmp(name, "defaultPreviewUpdateMode") == 0) {
        pCurrentCam->defaultPreviewUpdateMode = atts[1];
    } else if (strcmp(name, "hasSlowMotion") == 0) {
        pCurrentCam->hasSlowMotion = ((strcmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "hasFlash") == 0) {
        pCurrentCam->hasFlash = ((strcmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "supportedRecordingFramerates") == 0) {
        pCurrentCam->supportedRecordingFramerates = atts[1];
    } else if (strcmp(name, "supportedHighSpeedResolutionFps") == 0) {
        pCurrentCam->supportedHighSpeedResolutionFps = atts[1];
    } else if (strcmp(name, "maxHighSpeedDvsResolution") == 0) {
        pCurrentCam->maxHighSpeedDvsResolution = atts[1];
    } else if (strcmp(name, "supportedAeLock") == 0) {
        pCurrentCam->supportedAeLock = atts[1];
    } else if (strcmp(name, "supportedAwbLock") == 0) {
        pCurrentCam->supportedAwbLock = atts[1];
    } else if (strcmp(name, "synchronizeExposure") == 0) {
        pCurrentCam->synchronizeExposure = ((strcmp(atts[1], "true") == 0) ? true : false);
    } else if (strcmp(name, "maxNumYUVBufferForBurst") == 0) {
        pCurrentCam->maxNumYUVBufferForBurst = atoi(atts[1]);
    } else if (strcmp(name, "maxNumYUVBufferForBracket") == 0) {
        pCurrentCam->maxNumYUVBufferForBracket = atoi(atts[1]);
    } else if (strcmp(name, "verticalFOV") == 0) {
        pCurrentCam->verticalFOV = atts[1];
    } else if (strcmp(name, "horizontalFOV") == 0) {
        pCurrentCam->horizontalFOV = atts[1];
    }
}

#ifdef ENABLE_INTEL_EXTRAS
void CameraProfiles::handleFeature(CameraProfiles *profiles, const char *name, const char **atts)
{
    LOG1("@%s, name:%s, atts[0]:%s, profiles->mCurrentSensor:%d", __func__, name, atts[0], profiles->mCurrentSensor);

    if (strcmp(atts[0], "value") != 0) {
        ALOGE("@%s, name:%s, atts[0]:%s, xml format wrong", __func__, name, atts[0]);
        return;
    }

    if (strcmp(name, "defaultHdr") == 0) {
        pCurrentCam->defaultHdr = atts[1];
    } else if (strcmp(name, "supportedHdr") == 0) {
        pCurrentCam->supportedHdr = atts[1];
    } else if (strcmp(name, "defaultUltraLowLight") == 0) {
        pCurrentCam->defaultUltraLowLight = atts[1];
    } else if (strcmp(name, "supportedUltraLowLight") == 0) {
        pCurrentCam->supportedUltraLowLight = atts[1];
    } else if (strcmp(name, "defaultFaceRecognition") == 0) {
        pCurrentCam->defaultFaceRecognition = atts[1];
    } else if (strcmp(name, "supportedFaceRecognition") == 0) {
        pCurrentCam->supportedFaceRecognition = atts[1];
    } else if (strcmp(name, "defaultSmileShutter") == 0) {
        pCurrentCam->defaultSmileShutter = atts[1];
    } else if (strcmp(name, "supportedSmileShutter") == 0) {
        pCurrentCam->supportedSmileShutter = atts[1];
    } else if (strcmp(name, "defaultBlinkShutter") == 0) {
        pCurrentCam->defaultBlinkShutter = atts[1];
    } else if (strcmp(name, "supportedBlinkShutter") == 0) {
        pCurrentCam->supportedBlinkShutter = atts[1];
    } else if (strcmp(name, "defaultPanorama") == 0) {
        pCurrentCam->defaultPanorama = atts[1];
    } else if (strcmp(name, "supportedPanorama") == 0) {
        pCurrentCam->supportedPanorama = atts[1];
    } else if (strcmp(name, "defaultSceneDetection") == 0) {
        pCurrentCam->defaultSceneDetection = atts[1];
    } else if (strcmp(name, "supportedSceneDetection") == 0) {
        pCurrentCam->supportedSceneDetection = atts[1];
    }
}
#else
void CameraProfiles::handleFeature(CameraProfiles *profiles, const char *name, const char **atts)
{

    LOG1("@%s, name:%s, atts[0]:%s, profiles->mCurrentSensor:%d", __func__, name, atts[0], profiles->mCurrentSensor);

    pCurrentCam->defaultHdr = "";
    pCurrentCam->supportedHdr = "";
    pCurrentCam->defaultUltraLowLight = "";
    pCurrentCam->supportedUltraLowLight = "";
    pCurrentCam->defaultFaceRecognition = "";
    pCurrentCam->supportedFaceRecognition = "";
    pCurrentCam->defaultSmileShutter = "";
    pCurrentCam->supportedSmileShutter = "";
    pCurrentCam->defaultBlinkShutter = "";
    pCurrentCam->supportedBlinkShutter = "";
    pCurrentCam->defaultPanorama = "";
    pCurrentCam->supportedPanorama = "";
    pCurrentCam->defaultSceneDetection = "";
    pCurrentCam->supportedSceneDetection = "";
}
#endif

/**
 * the callback function of the libexpat for handling of one element start
 *
 * When it comes to the start of one element. This function will be called.
 *
 * \param userData: the pointer we set by the function XML_SetUserData.
 * \param name: the element's name.
 */
void CameraProfiles::startElement(void *userData, const char *name, const char **atts)
{
    CameraProfiles *profiles = (CameraProfiles *)userData;

    if (profiles->mCurrentDataField == FIELD_INVALID) {
        profiles->checkField(profiles, name, atts);
        return;
    }

    switch (profiles->mCurrentDataField) {
        case FIELD_SENSOR_BACK:
        case FIELD_SENSOR_FRONT:
            profiles->handleSensor(profiles, name, atts);
            profiles->handleFeature(profiles, name, atts);
            break;
        case FIELD_COMMON:
            profiles->handleCommon(profiles, name, atts);
            break;
        default:
            ALOGE("@%s, line:%d, go to default handling", __func__, __LINE__);
            break;
    }
}

/**
 * the callback function of the libexpat for handling of one element end
 *
 * When it comes to the end of one element. This function will be called.
 *
 * \param userData: the pointer we set by the function XML_SetUserData.
 * \param name: the element's name.
 */
void CameraProfiles::endElement(void *userData, const char *name)
{
    CameraProfiles *profiles = (CameraProfiles *)userData;

    if (strcmp(name, "Profiles") == 0) {
        profiles->mCurrentDataField = FIELD_INVALID;
        if (profiles->pCurrentCam) {
            profiles->mCameras.push(*(profiles->pCurrentCam));
            delete profiles->pCurrentCam;
            profiles->pCurrentCam = NULL;
        }
    }
    if (strcmp(name, "Common") == 0)
        profiles->mCurrentDataField = FIELD_INVALID;
}

/**
 * Get camera configuration from xml file
 *
 * The function will read the xml configuration file firstly.
 * Then it will parse out the camera settings.
 * The camera setting is stored inside this CameraProfiles class.
 *
 */
void CameraProfiles::getDataFromXmlFile(void)
{
    int done;
    void *pBuf = NULL;
    FILE *fp = NULL;
    LOG1("@%s", __func__);

    static const char *defaultXmlFile = "/etc/camera_profiles.xml";

    fp = ::fopen(defaultXmlFile, "r");
    if (NULL == fp) {
        ALOGE("@%s, line:%d, fp is NULL", __func__, __LINE__);
        return;
    }

    XML_Parser parser = ::XML_ParserCreate(NULL);
    if (NULL == parser) {
        ALOGE("@%s, line:%d, parser is NULL", __func__, __LINE__);
        goto exit;
    }
    ::XML_SetUserData(parser, this);
    ::XML_SetElementHandler(parser, startElement, endElement);

    pBuf = malloc(mBufSize);
    if (NULL == pBuf) {
        ALOGE("@%s, line:%d, pBuf is NULL", __func__, __LINE__);
        goto exit;
    }

    do {
        int len = (int)::fread(pBuf, 1, mBufSize, fp);
        if (!len) {
            if (ferror(fp)) {
                clearerr(fp);
                goto exit;
            }
        }
        done = len < mBufSize;
        if (XML_Parse(parser, (const char *)pBuf, len, done) == XML_STATUS_ERROR) {
            ALOGE("@%s, line:%d, XML_Parse error", __func__, __LINE__);
            goto exit;
        }
    } while (!done);

exit:
    if (parser)
        ::XML_ParserFree(parser);
    if (pBuf)
        free(pBuf);
    if (fp)
    ::fclose(fp);
}

void CameraProfiles::dump(void)
{
    for (unsigned i = 0; i < getSensorNum(); i++) {
        ALOGD("line%d, in DeviceData, start i:%d, sensor number:%d", __LINE__, i, getSensorNum());
        ALOGD("line%d, in DeviceData, pcam->maxEV:%s ", __LINE__, mCameras[i].maxEV.string());
        ALOGD("line%d, in DeviceData, pcam->minEV:%s ", __LINE__, mCameras[i].minEV.string());
        ALOGD("line%d, in DeviceData, pcam->stepEV:%s ", __LINE__, mCameras[i].stepEV.string());
        ALOGD("line%d, in DeviceData, pcam->supportedSceneModes:%s ", __LINE__, mCameras[i].supportedSceneModes.string());
        ALOGD("line%d, in DeviceData, pcam->defaultSceneMode:%s ", __LINE__, mCameras[i].defaultSceneMode.string());
        ALOGD("line%d, in DeviceData, pcam->supportedPreviewSizes:%s ", __LINE__, mCameras[i].supportedPreviewSizes.string());
        ALOGD("line%d, in DeviceData, mSupportedVideoSizes:%s ", __LINE__, mCameras[i].supportedVideoSizes.string());
        ALOGD("line%d, in DeviceData, pcam->mVideoPreviewSizePref:%s ", __LINE__, mCameras[i].mVideoPreviewSizePref.string());
        ALOGD("line%d, in DeviceData, pcam->supportedBurstFPS:%s ", __LINE__, mCameras[i].supportedBurstFPS.string());
        ALOGD("line%d, in DeviceData, pcam->orientation:%d ", __LINE__, mCameras[i].orientation);
        ALOGD("line%d, in DeviceData, pcam->sensorType:%d ", __LINE__, mCameras[i].sensorType);
        ALOGD("line%d, in DeviceData, pcam->dvs:%d ", __LINE__, mCameras[i].dvs);
        ALOGD("line%d, in DeviceData, pcam->supportedSnapshotSizes:%s ", __LINE__, mCameras[i].supportedSnapshotSizes.string());
        ALOGD("line%d, in DeviceData, pcam->flipping:%d ", __LINE__, mCameras[i].flipping);
        ALOGD("line%d, in DeviceData, pcam->continuousCapture:%d ", __LINE__, mCameras[i].continuousCapture);
        ALOGD("line%d, in DeviceData, pcam->mPreviewViaOverlay:%d ", __LINE__, mCameras[i].mPreviewViaOverlay);
        ALOGD("line%d, in DeviceData, pcam->supportedBurstLength:%s ", __LINE__, mCameras[i].supportedBurstLength.string());
        ALOGD("line%d, in DeviceData, pcam->facing:%d ", __LINE__, mCameras[i].facing);
        ALOGD("line%d, in DeviceData, pcam->defaultBurstLength:%s ", __LINE__, mCameras[i].defaultBurstLength.string());
        ALOGD("line%d, in DeviceData, pcam->defaultFlashMode:%s ", __LINE__, mCameras[i].defaultFlashMode.string());
        ALOGD("line%d, in DeviceData, pcam->supportedFlashModes:%s ", __LINE__, mCameras[i].supportedFlashModes.string());
        ALOGD("line%d, in DeviceData, pcam->supportedEffectModes:%s ", __LINE__, mCameras[i].supportedEffectModes.string());
        ALOGD("line%d, in DeviceData, pcam->supportedIntelEffectModes:%s ", __LINE__, mCameras[i].supportedIntelEffectModes.string());
        ALOGD("line%d, in DeviceData, pcam->supportedAwbModes:%s ", __LINE__, mCameras[i].supportedAwbModes.string());
        ALOGD("line%d, in DeviceData, pcam->defaultAwbMode:%s ", __LINE__, mCameras[i].defaultAwbMode.string());
        ALOGD("line%d, in DeviceData, pcam->defaultIso:%s ", __LINE__, mCameras[i].defaultIso.string());
        ALOGD("line%d, in DeviceData, pcam->supportedIso:%s ", __LINE__, mCameras[i].supportedIso.string());
        ALOGD("line%d, in DeviceData, pcam->defaultAeMetering:%s ", __LINE__, mCameras[i].defaultAeMetering.string());
        ALOGD("line%d, in DeviceData, pcam->supportedAeMetering:%s ", __LINE__, mCameras[i].supportedAeMetering.string());
        ALOGD("line%d, in DeviceData, pcam->defaultFocusMode:%s ", __LINE__, mCameras[i].defaultFocusMode.string());
        ALOGD("line%d, in DeviceData, pcam->supportedFocusModes:%s ", __LINE__, mCameras[i].supportedFocusModes.string());
        ALOGD("line%d, in DeviceData, pcam->defaultHdr:%s ", __LINE__, mCameras[i].defaultHdr.string());
        ALOGD("line%d, in DeviceData, pcam->supportedHdr:%s ", __LINE__, mCameras[i].supportedHdr.string());
        ALOGD("line%d, in DeviceData, pcam->defaultUltraLowLight:%s ", __LINE__, mCameras[i].defaultUltraLowLight.string());
        ALOGD("line%d, in DeviceData, pcam->supportedUltraLowLight:%s ", __LINE__, mCameras[i].supportedUltraLowLight.string());
        ALOGD("line%d, in DeviceData, pcam->defaultFaceRecognition:%s ", __LINE__, mCameras[i].defaultFaceRecognition.string());
        ALOGD("line%d, in DeviceData, pcam->supportedFaceRecognition:%s ", __LINE__, mCameras[i].supportedFaceRecognition.string());
        ALOGD("line%d, in DeviceData, pcam->defaultSmileShutter:%s ", __LINE__, mCameras[i].defaultSmileShutter.string());
        ALOGD("line%d, in DeviceData, pcam->supportedSmileShutter:%s ", __LINE__, mCameras[i].supportedSmileShutter.string());
        ALOGD("line%d, in DeviceData, pcam->defaultBlinkShutter:%s ", __LINE__, mCameras[i].defaultBlinkShutter.string());
        ALOGD("line%d, in DeviceData, pcam->supportedBlinkShutter:%s ", __LINE__, mCameras[i].supportedBlinkShutter.string());
        ALOGD("line%d, in DeviceData, pcam->defaultPanorama:%s ", __LINE__, mCameras[i].defaultPanorama.string());
        ALOGD("line%d, in DeviceData, pcam->supportedPanorama:%s ", __LINE__, mCameras[i].supportedPanorama.string());
        ALOGD("line%d, in DeviceData, pcam->defaultSceneDetection:%s ", __LINE__, mCameras[i].defaultSceneDetection.string());
        ALOGD("line%d, in DeviceData, pcam->supportedSceneDetection:%s ", __LINE__, mCameras[i].supportedSceneDetection.string());
        ALOGD("line%d, in DeviceData, pcam->maxNumYUVBufferForBurst:%d ", __LINE__, mCameras[i].maxNumYUVBufferForBurst);
        ALOGD("line%d, in DeviceData, pcam->maxNumYUVBufferForBracket:%d ", __LINE__, mCameras[i].maxNumYUVBufferForBracket);
        ALOGD("line%d, in DeviceData, pcam->maxHighSpeedDvsResolution:%s ",__LINE__, mCameras[i].maxHighSpeedDvsResolution.string());
    }

    ALOGD("line%d, in DeviceData, for common settings ", __LINE__);
    ALOGD("line%d, in DeviceData, mSubDevName:%s ", __LINE__, mSubDevName.string());
    ALOGD("line%d, in DeviceData, mFileInject:%d ", __LINE__, mFileInject);
    ALOGD("line%d, in DeviceData, mProductName:%s ", __LINE__, mProductName.string());
    ALOGD("line%d, in DeviceData, mManufacturerName:%s ", __LINE__, mManufacturerName.string());
    ALOGD("line%d, in DeviceData, mMaxZoomFactor:%d ", __LINE__, mMaxZoomFactor);
    ALOGD("line%d, in DeviceData, mSupportVideoSnapshot:%d ", __LINE__, mSupportVideoSnapshot);
    ALOGD("line%d, in DeviceData, mNumRecordingBuffers:%d ", __LINE__, mNumRecordingBuffers);
    ALOGD("line%d, in DeviceData, mNumPreviewBuffers:%d ", __LINE__, mNumPreviewBuffers);
    ALOGD("line%d, in DeviceData, mMaxContinuousRawRingBuffer:%d ", __LINE__, mMaxContinuousRawRingBuffer);
    ALOGD("line%d, in DeviceData, mBoardName:%s ", __LINE__, mBoardName.string());
    ALOGD("line%d, in DeviceData, mUseIntelULL:%d ", __LINE__, mUseIntelULL);
}

}
