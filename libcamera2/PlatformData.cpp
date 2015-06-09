/*
 * Copyright (c) 2012-2014 Intel Corporation
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

//#define LOG_NDEBUG 0
#define LOG_TAG "Camera_PlatformData"
#include "LogHelper.h"

#include <assert.h>
#include <camera.h>
#include <camera/CameraParameters.h>
#include "PlatformData.h"
#include "CameraProfiles.h"
#include <utils/Log.h>
#include "v4l2device.h"
#include <sys/sysinfo.h>

namespace android {

static const int spIdLength = 4;
static unsigned long sTotalRam = 0;

PlatformBase* PlatformData::mInstance = 0;

AiqConf PlatformData::AiqConfig[MAX_CAMERAS];
HalConf PlatformData::HalConfig[MAX_CAMERAS];

// Max width and height string length, like "1920x1080".
const int MAX_WIDTH_HEIGHT_STRING_LENGTH = 25;

__attribute__((constructor)) void initializeLoadTimePlatformData(void)
{
    struct sysinfo sysInfo;
    sysinfo(&sysInfo);
    sTotalRam = sysInfo.totalram;
}

status_t PlatformBase::getSensorInfo(Vector<SensorNameAndPort>& sensorInfo)
{
    LOG2("@%s", __FUNCTION__);

    status_t ret = NO_ERROR;
    int i = 0;
    SensorNameAndPort sensor;
    ssize_t idx;
    struct v4l2_input input;
    atomisp_camera_port ispPort;

    sp<V4L2VideoNode> mainDevice = new V4L2VideoNode("/dev/video0", 0);
    if (mainDevice->open() != NO_ERROR) {
        ALOGE("@%s, Failed to open first device!", __FUNCTION__);
        return NO_INIT;
    }

    while (ret == NO_ERROR) {
        idx = -1;
        CLEAR(input);
        input.index = i;

        if (NO_ERROR == (ret = mainDevice->enumerateInputs(&input))) {
            idx = String8((const char*)input.name).find(" ");
            sensor.name.clear();
            if (idx == -1)
                sensor.name.append((const char*)input.name);
            else
                sensor.name.append((const char*)input.name, idx);

            // Need to save ISP port. This is used for matching CameraId.
            ispPort = (atomisp_camera_port)input.reserved[1];
            switch (ispPort) {
            case ATOMISP_CAMERA_PORT_PRIMARY:
                sensor.ispPort = SensorNameAndPort::PRIMARY;
                break;
            case ATOMISP_CAMERA_PORT_SECONDARY:
                sensor.ispPort = SensorNameAndPort::SECONDARY;
                break;
            case ATOMISP_CAMERA_PORT_TERTIARY:
                sensor.ispPort = SensorNameAndPort::TERTIARY;
                break;
            default:
                sensor.ispPort = SensorNameAndPort::UNKNOWN_PORT;
                break;
            }

            sensorInfo.push(sensor);
            LOG1("@%s: %s at index %d (port %d)", __FUNCTION__, sensor.name.string(), i, sensor.ispPort);
            i++;
        }
    }
    if (ret == BAD_INDEX) {
        // Not an error: all devices were enumerated.
        ret = NO_ERROR;
    } else {
        ALOGE("@%s: Device input enumeration failed for sensor index %d (err = %d)", __FUNCTION__, i,  ret);
    }

    mainDevice->close();
    return ret;
}

PlatformBase* PlatformData::getInstance(void)
{
    if (mInstance == 0) {
        // Get the sensor names from driver
        Vector<SensorNameAndPort> sensorInfo;
        if (NO_ERROR != PlatformBase::getSensorInfo(sensorInfo))
            mInstance = new CameraProfiles();
        else
            mInstance = new CameraProfiles(sensorInfo);

        // add an extra camera which is copied from the first one as a fake camera
        // for file injection
        mInstance->mCameras.push(mInstance->mCameras[0]);
        mInstance->mFileInject = true;
    }

    return mInstance;
}

unsigned long PlatformData::getTotalRam()
{
    return sTotalRam;
}

void PlatformData::setIntelligentMode(int cameraId, bool val)
{
    getInstance()->mCameras.editItemAt(cameraId).mIntelligentMode = val;
}

bool PlatformData::getIntelligentMode(int cameraId)
{
    return getInstance()->mCameras[cameraId].mIntelligentMode;
}

bool PlatformData::isDisable3A(int cameraId)
{
    // the intelligent mode needs 3A to be disabled
    if (getInstance()->mCameras[cameraId].mIntelligentMode)
        return true;
    return getInstance()->mCameras[cameraId].disable3A;
}

status_t PlatformData::readSpId(String8& spIdName, int& spIdValue)
{
        FILE *file;
        status_t ret = OK;
        String8 sysfsSpIdPath = String8("/sys/spid/");
        String8 fullPath;

        fullPath = sysfsSpIdPath;
        fullPath.append(spIdName);

        file = fopen(fullPath, "rb");
        if (!file) {
            ALOGE("ERROR in opening file %s", fullPath.string());
            return NAME_NOT_FOUND;
        }
        ret = fscanf(file, "%x", &spIdValue);
        if (ret < 0) {
            ALOGE("ERROR in reading %s", fullPath.string());
            spIdValue = 0;
            fclose(file);
            return UNKNOWN_ERROR;
        }
        fclose(file);
        return ret;
}

bool PlatformData::validCameraId(int cameraId, const char* functionName)
{
    if (cameraId < 0 || cameraId >= static_cast<int>(getInstance()->mCameras.size())) {
        ALOGE("%s: Invalid cameraId %d", functionName, cameraId);
        return false;
    }
    else {
        return true;
    }
}

int PlatformData::getActiveCamIdx(int cameraId)
{
    int id;
    PlatformBase *i = getInstance();
    if (i->mUseExtendedCamera
        && i->mHasExtendedCamera
        && cameraId == i->mExtendedCameraId)
        id = i->mExtendedCameraIndex;
    else
        id = cameraId;

    return id;
}

const char* PlatformData::sensorName(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].sensorName;
}

SensorType PlatformData::sensorType(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return SENSOR_TYPE_NONE;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].sensorType;
}

int PlatformData::cameraFacing(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return -1;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].facing;
}

int PlatformData::cameraOrientation(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return -1;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].orientation;
}

int PlatformData::sensorFlipping(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return -1;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].flipping;
}

int PlatformData::numberOfCameras(void)
{
    return getInstance()->mCameras.size();
}

const char* PlatformData::preferredPreviewSizeForVideo(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::PreviewSizeVideoDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].mVideoPreviewSizePref;
}

const char* PlatformData::defaultPreviewSize(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }

    return getInstance()->mCameras[cameraId].defaultPreviewSize;
}

const char* PlatformData::defaultVideoSize(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }

    return getInstance()->mCameras[cameraId].defaultVideoSize;
}

const char* PlatformData::supportedVideoSizes(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::VideoSizes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedVideoSizes;
}

const char* PlatformData::supportedSnapshotSizes(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedSnapshotSizes;
}

int PlatformData::defaultJpegQuality(int cameraId) {
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return -1;
    }
    return getInstance()->mCameras[cameraId].defaultJpegQuality;
}

int PlatformData::defaultJpegThumbnailQuality(int cameraId) {
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return -1;
    }
    return getInstance()->mCameras[cameraId].defaultJpegThumbnailQuality;
}

bool PlatformData::supportsFileInject(void)
{
    return getInstance()->mFileInject;
}

bool PlatformData::supportsContinuousCapture(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].continuousCapture;
}

bool PlatformData::supportsContinuousJpegCapture(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].continuousJpegCapture;
}

int PlatformData::maxContinuousRawRingBufferSize(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__) || PlatformData::supportsContinuousCapture(cameraId) == false) {
        return 0;
    }
    return getInstance()->mMaxContinuousRawRingBuffer;
}

int PlatformData::shutterLagCompensationMs(void)
{
    return getInstance()->mShutterLagCompensationMs;
}

int PlatformData::getMaxISPTimeoutCount(void)
{
    return getInstance()->mMaxISPTimeoutCount;
}

bool PlatformData::extendedMakernote(void)
{
    return getInstance()->mExtendedMakernote;
}


bool PlatformData::renderPreviewViaOverlay(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].mPreviewViaOverlay;
}

bool PlatformData::snapshotResolutionSupportedByZSL(int cameraId,
                                                    int width, int height)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }

    PlatformBase *i = getInstance();
    Vector<Size>::const_iterator it = i->mCameras[getActiveCamIdx(cameraId)].mZSLUnsupportedSnapshotResolutions.begin();
    for (;it != i->mCameras[getActiveCamIdx(cameraId)].mZSLUnsupportedSnapshotResolutions.end(); ++it) {
        if (it->width == width && it->height == height) {
            return false;
        }
    }
    return true;
}

int PlatformData::overlayRotation(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].overlayRelativeRotation;

}

bool PlatformData::snapshotResolutionSupportedByCVF(int cameraId,
                                                    int width, int height)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }

    PlatformBase *i = getInstance();
    Vector<Size>::const_iterator it = i->mCameras[getActiveCamIdx(cameraId)].mCVFUnsupportedSnapshotResolutions.begin();
    for (;it != i->mCameras[getActiveCamIdx(cameraId)].mCVFUnsupportedSnapshotResolutions.end(); ++it) {
        if (it->width == width && it->height == height) {
            return false;
        }
    }
    return true;
}

bool PlatformData::supportsDVS(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].dvs;
}

bool PlatformData::supportsNarrowGamma(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].narrowGamma;
}

const char* PlatformData::supportedBurstFPS(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedBurstFPS;
}

const char* PlatformData::supportedBurstLength(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedBurstLength;
}

bool PlatformData::supportEV(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    PlatformBase *i = getInstance();
    const char* minEV = i->mCameras[getActiveCamIdx(cameraId)].minEV;
    const char* maxEV = i->mCameras[getActiveCamIdx(cameraId)].maxEV;
    if(!strcmp(minEV, "0") && !strcmp(maxEV, "0")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return false;
    }
    return true;
}

const char* PlatformData::supportedMaxEV(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::EvMax)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].maxEV;
}

const char* PlatformData::supportedMinEV(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::EvMin)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
      return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].minEV;
}

const char* PlatformData::supportedDefaultEV(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::EvDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultEV;
}

const char* PlatformData::supportedStepEV(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::EvStep)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].stepEV;
}

const char* PlatformData::supportedAeMetering(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::AeModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedAeMetering;
}

const char* PlatformData::defaultAeMetering(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::AeModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultAeMetering;
}

const char* PlatformData::supportedAeLock(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedAeLock;
}

const char* PlatformData::supportedMaxSaturation(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::SaturationMax)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].maxSaturation;
}

const char* PlatformData::supportedMinSaturation(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::SaturationMin)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].minSaturation;
}

const char* PlatformData::defaultSaturation(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::SaturationDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultSaturation;
}

const char* PlatformData::supportedSaturation(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::Saturations)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedSaturation;
}

const char* PlatformData::supportedStepSaturation(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::SaturationStep)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].stepSaturation;
}

int PlatformData::lowSaturation(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].lowSaturation;
}

int PlatformData::highSaturation(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].highSaturation;
}

const char* PlatformData::supportedMaxContrast(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::ContrastMax)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].maxContrast;
}

const char* PlatformData::supportedMinContrast(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::ContrastMin)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].minContrast;
}

const char* PlatformData::defaultContrast(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::ContrastDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultContrast;
}

const char* PlatformData::supportedContrast(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::Contrasts)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedContrast;
}

const char* PlatformData::supportedStepContrast(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::ContrastStep)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].stepContrast;
}

int PlatformData::softContrast(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].softContrast;
}

int PlatformData::hardContrast(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].hardContrast;
}

const char* PlatformData::supportedMaxSharpness(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::SharpnessMax)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
       return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].maxSharpness;
}

const char* PlatformData::supportedMinSharpness(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::SharpnessMin)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].minSharpness;
}

const char* PlatformData::defaultSharpness(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::SharpnessDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultSharpness;
}

const char* PlatformData::supportedSharpness(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::Sharpnesses)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedSharpness;
}

const char* PlatformData::supportedStepSharpness(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::SharpnessStep)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].stepSharpness;
}

int PlatformData::softSharpness(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].softSharpness;
}

int PlatformData::hardSharpness(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].hardSharpness;
}

const char* PlatformData::supportedFlashModes(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::FlashModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedFlashModes;
}

const char* PlatformData::defaultFlashMode(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::FlashModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultFlashMode;
}

const char* PlatformData::supportedIso(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::IsoModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedIso;
}

const char* PlatformData::defaultIso(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::IsoModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultIso;
}

const char* PlatformData::supportedSceneModes(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::SceneModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedSceneModes;
}

const char* PlatformData::defaultSceneMode(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::SceneModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultSceneMode;
}

const char* PlatformData::supportedEffectModes(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::EffectModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedEffectModes;
}

const char* PlatformData::supportedIntelEffectModes(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::ExtendedEffectModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedIntelEffectModes;
}

const char* PlatformData::defaultEffectMode(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::EffectModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultEffectMode;
}

const char* PlatformData::supportedAwbModes(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::AwbModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedAwbModes;
}

const char* PlatformData::defaultAwbMode(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::AwbModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultAwbMode;
}

const char* PlatformData::supportedPreviewFrameRate(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::PreviewFpss)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedPreviewFrameRate;
}

const char* PlatformData::supportedAwbLock(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedAwbLock;
}

const char* PlatformData::supportedPreviewFPSRange(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::PreviewFpsRanges)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedPreviewFPSRange;
}

const char* PlatformData::defaultPreviewFPSRange(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::PreviewFpsRangeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultPreviewFPSRange;
}

const char* PlatformData::supportedPreviewSizes(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::PreviewSizes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedPreviewSizes;
}

const char* PlatformData::supportedPreviewUpdateModes(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedPreviewUpdateModes;
}

const char* PlatformData::defaultPreviewUpdateMode(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultPreviewUpdateMode;
}

bool PlatformData::supportsSlowMotion(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].hasSlowMotion;
}

bool PlatformData::supportsFlash(int cameraId)
{
    bool boolean;
    if (!HalConfig[cameraId].getBool(boolean, CPF::HasFlash)) {
        return boolean;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].hasFlash;
}

const char* PlatformData::supportedHighSpeedResolutionFps(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedHighSpeedResolutionFps;
}

const char* PlatformData::supportedRecordingFramerates(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedRecordingFramerates;
}

const char* PlatformData::maxHighSpeedDvsResolution(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].maxHighSpeedDvsResolution;
}

bool PlatformData::isFullResSdvSupported(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return (strlen(supportedSdvSizes(cameraId)) != 0);
}

const char* PlatformData::supportedSdvSizes(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedSdvSizes;
}

const char* PlatformData::supportedFocusModes(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::FocusModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedFocusModes;
}

const char* PlatformData::defaultFocusMode(int cameraId)
{
    const char *sPtr;
    if (!HalConfig[cameraId].getString(sPtr, CPF::FocusModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultFocusMode;
}

size_t PlatformData::getMaxNumFocusAreas(int cameraId)
{

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[cameraId].maxNumFocusAreas;
}

int PlatformData::getMaxDepthPreviewBufferQueueSize(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].maxDepthPreviewBufferQueueSize;
}

bool PlatformData::isFixedFocusCamera(int cameraId)
{
    if (strcmp(PlatformData::defaultFocusMode(cameraId), "fixed") == 0) {
        return true;
    } else {
        return false;
    }
}

const char* PlatformData::defaultHdr(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].defaultHdr;
}

const char* PlatformData::supportedHdr(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].supportedHdr;

}

const char* PlatformData::defaultUltraLowLight(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].defaultUltraLowLight;
}

const char* PlatformData::supportedUltraLowLight(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].supportedUltraLowLight;
}

const char* PlatformData::defaultFaceRecognition(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].defaultFaceRecognition;
}

const char* PlatformData::supportedFaceRecognition(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].supportedFaceRecognition;
}

const char* PlatformData::defaultSmileShutter(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].defaultSmileShutter;
}

const char* PlatformData::supportedSmileShutter(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].supportedSmileShutter;
}

const char* PlatformData::defaultBlinkShutter(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].defaultBlinkShutter;
}

const char* PlatformData::supportedBlinkShutter(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].supportedBlinkShutter;
}

const char* PlatformData::defaultPanorama(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].defaultPanorama;
}

const char* PlatformData::supportedPanorama(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].supportedPanorama;
}

const char* PlatformData::defaultSceneDetection(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].defaultSceneDetection;
}

const char* PlatformData::supportedSceneDetection(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[getActiveCamIdx(cameraId)].supportedSceneDetection;
}

int PlatformData::cacheLineSize()
{
    return getInstance()->mCacheLineSize;
}

const char* PlatformData::productName(void)
{
    return getInstance()->mProductName;
}

int PlatformData::getMaxPanoramaSnapshotCount()
{
    return getInstance()->mPanoramaMaxSnapshotCount;
}

const char* PlatformData::manufacturerName(void)
{
    return getInstance()->mManufacturerName;
}

const char* PlatformData::getISPSubDeviceName(void)
{
    return getInstance()->mSubDevName;
}

int PlatformData::getMaxZoomFactor(int cameraId)
{
    int value;
    if (!HalConfig[cameraId].getValue(value, CPF::ZoomDigital, CPF::Max)) {
        return value;
    }

    return getInstance()->mMaxZoomFactor;
}

bool PlatformData::supportVideoSnapshot(void)
{
    return getInstance()->mSupportVideoSnapshot;
}

bool PlatformData::supportsOfflineBurst(void)
{
    return getInstance()->mSupportsOfflineBurst;
}

bool PlatformData::supportsOfflineBracket(void)
{
    return getInstance()->mSupportsOfflineBracket;
}

bool PlatformData::supportsOfflineHdr(void)
{
    return getInstance()->mSupportsOfflineHdr;
}

int PlatformData::getRecordingBufNum(void)
{
    return getInstance()->mNumRecordingBuffers;
}

int PlatformData::getPreviewBufNum(void)
{
    return getInstance()->mNumPreviewBuffers;
}

int PlatformData::getMaxNumYUVBufferForBurst(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].maxNumYUVBufferForBurst;
}

int PlatformData::getMaxNumYUVBufferForBracket(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].maxNumYUVBufferForBracket;
}

bool PlatformData::supportDualMode(void)
{
    return getInstance()->mSupportDualMode;
}

bool PlatformData::supportPreviewLimitation(void)
{
    return getInstance()->mSupportPreviewLimitation;
}

int PlatformData::getPreviewPixelFormat(int cameraId)
{
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].mPreviewFourcc;
}

const char* PlatformData::getBoardName(void)
{
    return getInstance()->mBoardName;
}

status_t PlatformData::createVendorPlatformProductName(String8& name)
{
    int vendorIdValue;
    int platformFamilyIdValue;
    int productLineIdValue;

    name = "";

    String8 vendorIdName = String8("vendor_id");
    String8 platformFamilyIdName = String8("platform_family_id");
    String8 productLineIdName = String8("product_line_id");

    if (readSpId(vendorIdName, vendorIdValue) < 0) {
        ALOGE("%s could not be read from sysfs", vendorIdName.string());
        return UNKNOWN_ERROR;
    }
    if (readSpId(platformFamilyIdName, platformFamilyIdValue) < 0) {
        ALOGE("%s could not be read from sysfs", platformFamilyIdName.string());
        return UNKNOWN_ERROR;
    }
    if (readSpId(productLineIdName, productLineIdValue) < 0){
        ALOGE("%s could not be read from sysfs", productLineIdName.string());
        return UNKNOWN_ERROR;
    }

    char vendorIdValueStr[spIdLength];
    char platformFamilyIdValueStr[spIdLength];
    char productLineIdValueStr[spIdLength];

    snprintf(vendorIdValueStr, spIdLength, "%#x", vendorIdValue);
    snprintf(platformFamilyIdValueStr, spIdLength, "%#x", platformFamilyIdValue);
    snprintf(productLineIdValueStr, spIdLength, "%#x", productLineIdValue);

    name = vendorIdValueStr;
    name += String8("-");
    name += platformFamilyIdValueStr;
    name += String8("-");
    name += productLineIdValueStr;

    return OK;
}

int PlatformData::getSensorGainLag(int cameraId)
{
    int value;
    if (!HalConfig[cameraId].getValue(value, CPF::Gain, CPF::Lag))
        return value;

    return getInstance()->mSensorGainLag;
}

int PlatformData::getSensorExposureLag(int cameraId)
{
    int value;
    if (!HalConfig[cameraId].getValue(value, CPF::Exposure, CPF::Lag))
        return value;

    return getInstance()->mSensorExposureLag;
}

bool PlatformData::synchronizeExposure(int cameraId)
{
    PlatformBase *i = getInstance();
    return i->mCameras[cameraId].synchronizeExposure;
}

bool PlatformData::useIntelULL(void)
{
    PlatformBase *i = getInstance();
    return i->mUseIntelULL;
}

float PlatformData::verticalFOV(int cameraId, int width, int height)
{
    float retVal = 0.0f;

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return retVal;
    }

    const char* strFOV = getInstance()->mCameras[getActiveCamIdx(cameraId)].verticalFOV;
    if (strFOV) {
        char str[MAX_WIDTH_HEIGHT_STRING_LENGTH];
        snprintf(str, sizeof(str), "%dx%d", width, height);
        char* ptr = strstr(strFOV, str);
        if (ptr) {
            ptr = ptr + strlen(str);
            if ((ptr != NULL) && (*ptr == '@')) {
                retVal = strtod(ptr+1, NULL);
                return retVal;
            }
        }
    }

    if (!HalConfig[cameraId].getFloat(retVal, CPF::Fov, CPF::Vertical)) {
        return retVal;
    }

    // if don't configure vertical FOV angle, just set one default for it.
    retVal = 42.5;
    return retVal;
}

float PlatformData::horizontalFOV(int cameraId, int width, int height)
{
    float retVal = 0.0f;

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return retVal;
    }

    const char* strFOV = getInstance()->mCameras[getActiveCamIdx(cameraId)].horizontalFOV;
    if (strFOV) {
        char str[MAX_WIDTH_HEIGHT_STRING_LENGTH];
        snprintf(str, sizeof(str), "%dx%d", width, height);
        char* ptr = strstr(strFOV, str);
        if (ptr) {
            ptr = ptr + strlen(str);
            if ((ptr != NULL) && (*ptr == '@')) {
                retVal = strtod(ptr+1, NULL);
                return retVal;
            }
        }
    }

    if (!HalConfig[cameraId].getFloat(retVal, CPF::Fov, CPF::Horizontal)) {
        return retVal;
    }

    // if don't configure vertical FOV angle, just set one default for it.
    retVal = 54.8;
    return retVal;
}

int PlatformData::defaultDepthFocalLength(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].defaultDepthFocalLength;
}

bool PlatformData::supportsPostviewOutput(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return true;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].mSupportsPostviewOutput;
}

bool PlatformData::ispSupportContinuousCaptureMode(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return true;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].mISPSupportContinuousCaptureMode;
}

bool PlatformData::supportsColorBarPreview(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return true;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].mSupportsColorBarPreview;
}

const char* PlatformData::supportedDvsSizes(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedDvsSizes;
}

bool PlatformData::supportedSensorMetadata(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[getActiveCamIdx(cameraId)].supportedSensorMetadata;
}

bool PlatformData::isGraphicGen(void)
{
#ifdef GRAPHIC_IS_GEN
    return true;
#else
    return false;
#endif
}

int PlatformData::faceCallbackDivider()
{

    PlatformBase *i = getInstance();
    int retVal = i->mFaceCallbackDivider;

    // Always return a positive integer:
    return (retVal > 0) ? retVal : 1;
}

unsigned int PlatformData::getNumOfCPUCores()
{
    LOG1("@%s, line:%d", __FUNCTION__, __LINE__);
    unsigned int cpuCores = 1;

    char buf[20];
    FILE *cpuOnline = fopen("/sys/devices/system/cpu/online", "r");
    if (cpuOnline) {
        memset(buf, 0, sizeof(buf));
        fread(buf, 1, sizeof(buf), cpuOnline);
        buf[sizeof(buf) - 1] = '\0';
        cpuCores = 1 + atoi(strstr(buf, "-") + 1);
        fclose(cpuOnline);
    }
    LOG1("@%s, line:%d, cpu core number:%d", __FUNCTION__, __LINE__, cpuCores);
    return cpuCores;
}

/**
 * \brief For checking whether we enable HAL-video stabilization specific
 * functionality.
 *
 * NOTE: this mode requires customers to integrate their own video stabilization
 * algorithm
 */
bool PlatformData::useHALVS(int cameraId)
{

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }

    PlatformBase *i = getInstance();

    return i->mCameras[getActiveCamIdx(cameraId)].useHALVS;
}

int PlatformData::getNumOfCaptureWarmUpFrames(int cameraId)
{
    int retVal = 0;

    if (!validCameraId(cameraId, __FUNCTION__)) {
       return retVal;
    }

    PlatformBase *i = getInstance();
    retVal = i->mCameras[getActiveCamIdx(cameraId)].captureWarmUpFrames;

    // Always return a non-negative integer:
    return (retVal > 0) ? retVal : 0;
}

bool PlatformData::useMultiStreamsForSoC(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }

    PlatformBase *i = getInstance();
    if (i->mCameras[getActiveCamIdx(cameraId)].sensorType == SENSOR_TYPE_RAW) {
        return false;
    }

    return i->mCameras[getActiveCamIdx(cameraId)].useMultiStreamsForSoC ? true : false;
}

void PlatformData::useExtendedCamera(bool val)
{
    LOG1("@useExtendedCamera val = %d", val);
    if (supportExtendedCamera())
        getInstance()->mUseExtendedCamera = val;
}

bool PlatformData::isExtendedCameras(void)
{
    return getInstance()->mUseExtendedCamera;
}

bool PlatformData::isExtendedCamera(int cameraId)
{
    PlatformBase *i = getInstance();
    if (i->mUseExtendedCamera
        && i->mHasExtendedCamera
        && cameraId == i->mExtendedCameraId)
        return true;
    else
        return false;
}

bool PlatformData::supportExtendedCamera(void)
{
    PlatformBase *i = getInstance();
    return i->mHasExtendedCamera ? true : false;
}

const char* PlatformData::supportedIntelligentMode(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[cameraId].supportedIntelligentMode;
}

}; // namespace android
