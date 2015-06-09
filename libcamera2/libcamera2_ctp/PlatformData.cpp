/*
 * Copyright (c) 2012 Intel Corporation.
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
namespace android {


static const int spIdLength = 4;

PlatformBase* PlatformData::mInstance = 0;
int PlatformData::mActiveCameraId = -1;

AiqConf PlatformData::AiqConfig;
HalConf PlatformData::HalConfig;

// Max width and height string length, like "1920x1080".
const int MAX_WIDTH_HEIGHT_STRING_LENGTH = 25;

PlatformBase* PlatformData::getInstance(void)
{
    if (mInstance == 0) {
        mInstance = new CameraProfiles();

        // add an extra camera which is copied from the first one as a fake camera
        // for file injection
        mInstance->mCameras.push(mInstance->mCameras[0]);
        mInstance->mFileInject = true;
    }

    return mInstance;
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

SensorType PlatformData::sensorType(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return SENSOR_TYPE_NONE;
    }
    return getInstance()->mCameras[cameraId].sensorType;
}

int PlatformData::cameraFacing(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return -1;
    }
    return getInstance()->mCameras[cameraId].facing;
}

int PlatformData::cameraOrientation(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return -1;
    }
    return getInstance()->mCameras[cameraId].orientation;
}

int PlatformData::sensorFlipping(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return -1;
    }
    return getInstance()->mCameras[cameraId].flipping;
}

void PlatformData::setActiveCameraId(int cameraId)
{
    // Multiple active cameras not supported
    if ((mActiveCameraId >= 0) || (cameraId < 0)) {
        ALOGE("%s: Activating multiple cameras (was %d, now trying %d)", __FUNCTION__, mActiveCameraId, cameraId);
    }
    mActiveCameraId = cameraId;
}

void PlatformData::freeActiveCameraId(int cameraId)
{
    // Multiple active cameras not supported
    if ((mActiveCameraId != cameraId) || (cameraId < 0)) {
        ALOGE("%s: Freeing a wrong camera (was %d, now trying %d)", __FUNCTION__, mActiveCameraId, cameraId);
    }
    mActiveCameraId = -1;
}

int PlatformData::numberOfCameras(void)
{
    return getInstance()->mCameras.size();
}

const char* PlatformData::preferredPreviewSizeForVideo(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::PreviewSizeVideoDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[cameraId].mVideoPreviewSizePref;
}

const char* PlatformData::supportedVideoSizes(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::VideoSizes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[cameraId].supportedVideoSizes;
}

const char* PlatformData::supportedSnapshotSizes(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[cameraId].supportedSnapshotSizes;
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
    return getInstance()->mCameras[cameraId].continuousCapture;
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

bool PlatformData::renderPreviewViaOverlay(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[cameraId].mPreviewViaOverlay;
}

bool PlatformData::resolutionSupportedByVFPP(int cameraId,
                                             int width, int height)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }

    PlatformBase *i = getInstance();
    Vector<Size>::const_iterator it = i->mCameras[cameraId].mVFPPLimitedResolutions.begin();
    for (;it != i->mCameras[cameraId].mVFPPLimitedResolutions.end(); ++it) {
        if (it->width == width && it->height == height) {
            return false;
        }
    }
    return true;
}

bool PlatformData::snapshotResolutionSupportedByZSL(int cameraId,
                                                    int width, int height)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }

    PlatformBase *i = getInstance();
    Vector<Size>::const_iterator it = i->mCameras[cameraId].mZSLUnsupportedSnapshotResolutions.begin();
    for (;it != i->mCameras[cameraId].mZSLUnsupportedSnapshotResolutions.end(); ++it) {
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
    return getInstance()->mCameras[cameraId].overlayRelativeRotation;

}

bool PlatformData::snapshotResolutionSupportedByCVF(int cameraId,
                                                    int width, int height)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }

    PlatformBase *i = getInstance();
    Vector<Size>::const_iterator it = i->mCameras[cameraId].mCVFUnsupportedSnapshotResolutions.begin();
    for (;it != i->mCameras[cameraId].mCVFUnsupportedSnapshotResolutions.end(); ++it) {
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
    return getInstance()->mCameras[cameraId].dvs;
}

const char* PlatformData::supportedBurstFPS(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[cameraId].supportedBurstFPS;
}

const char* PlatformData::supportedBurstLength(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[cameraId].supportedBurstLength;
}

bool PlatformData::supportEV(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    PlatformBase *i = getInstance();
    const char* minEV = i->mCameras[cameraId].minEV;
    const char* maxEV = i->mCameras[cameraId].maxEV;
    if(!strcmp(minEV, "0") && !strcmp(maxEV, "0")) {
        LOG1("@%s: not supported by current camera", __FUNCTION__);
        return false;
    }
    return true;
}

const char* PlatformData::supportedMaxEV(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::EvMax)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].maxEV;
}

const char* PlatformData::supportedMinEV(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::EvMin)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
      return "";
    }
    return getInstance()->mCameras[cameraId].minEV;
}

const char* PlatformData::supportedDefaultEV(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::EvDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultEV;
}

const char* PlatformData::supportedStepEV(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::EvStep)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].stepEV;
}

const char* PlatformData::supportedAeMetering(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::AeModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedAeMetering;
}

const char* PlatformData::defaultAeMetering(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::AeModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultAeMetering;
}

const char* PlatformData::supportedAeLock(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[cameraId].supportedAeLock;
}

const char* PlatformData::supportedMaxSaturation(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::SaturationMax)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].maxSaturation;
}

const char* PlatformData::supportedMinSaturation(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::SaturationMin)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].minSaturation;
}

const char* PlatformData::defaultSaturation(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::SaturationDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultSaturation;
}

const char* PlatformData::supportedSaturation(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::Saturations)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedSaturation;
}

const char* PlatformData::supportedStepSaturation(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::SaturationStep)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].stepSaturation;
}

const char* PlatformData::supportedMaxContrast(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::ContrastMax)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].maxContrast;
}

const char* PlatformData::supportedMinContrast(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::ContrastMin)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].minContrast;
}

const char* PlatformData::defaultContrast(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::ContrastDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultContrast;
}

const char* PlatformData::supportedContrast(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::Contrasts)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedContrast;
}

const char* PlatformData::supportedStepContrast(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::ContrastStep)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].stepContrast;
}

const char* PlatformData::supportedMaxSharpness(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::SharpnessMax)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
       return "";
    }
    return getInstance()->mCameras[cameraId].maxSharpness;
}

const char* PlatformData::supportedMinSharpness(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::SharpnessMin)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].minSharpness;
}

const char* PlatformData::defaultSharpness(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::SharpnessDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultSharpness;
}

const char* PlatformData::supportedSharpness(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::Sharpnesses)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedSharpness;
}

const char* PlatformData::supportedStepSharpness(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::SharpnessStep)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].stepSharpness;
}

const char* PlatformData::supportedFlashModes(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::FlashModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedFlashModes;
}

const char* PlatformData::defaultFlashMode(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::FlashModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultFlashMode;
}

const char* PlatformData::supportedIso(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::IsoModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedIso;
}

const char* PlatformData::defaultIso(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::IsoModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultIso;
}

const char* PlatformData::supportedSceneModes(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::SceneModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedSceneModes;
}

const char* PlatformData::defaultSceneMode(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::SceneModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultSceneMode;
}

const char* PlatformData::supportedEffectModes(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::EffectModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedEffectModes;
}

const char* PlatformData::supportedIntelEffectModes(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::ExtendedEffectModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedIntelEffectModes;
}

const char* PlatformData::defaultEffectMode(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::EffectModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultEffectMode;
}

const char* PlatformData::supportedAwbModes(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::AwbModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedAwbModes;
}

const char* PlatformData::defaultAwbMode(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::AwbModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultAwbMode;
}

const char* PlatformData::supportedPreviewFrameRate(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::PreviewFpss)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedPreviewFrameRate;
}

const char* PlatformData::supportedAwbLock(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return NULL;
    }
    return getInstance()->mCameras[cameraId].supportedAwbLock;
}

const char* PlatformData::supportedPreviewFPSRange(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::PreviewFpsRanges)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedPreviewFPSRange;
}

const char* PlatformData::defaultPreviewFPSRange(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::PreviewFpsRangeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultPreviewFPSRange;
}

const char* PlatformData::supportedPreviewSizes(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::PreviewSizes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedPreviewSizes;
}

const char* PlatformData::supportedPreviewUpdateModes(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedPreviewUpdateModes;
}

const char* PlatformData::defaultPreviewUpdateMode(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultPreviewUpdateMode;
}

bool PlatformData::supportsSlowMotion(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[cameraId].hasSlowMotion;
}

bool PlatformData::supportsFlash(int cameraId)
{
    bool boolean;
    if (!HalConfig.getBool(boolean, CPF::HasFlash)) {
        return boolean;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return false;
    }
    return getInstance()->mCameras[cameraId].hasFlash;
}

const char* PlatformData::supportedHighSpeedResolutionFps(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedHighSpeedResolutionFps;
}

const char* PlatformData::supportedRecordingFramerates(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedRecordingFramerates;
}

const char* PlatformData::maxHighSpeedDvsResolution(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].maxHighSpeedDvsResolution;
}

const char* PlatformData::supportedFocusModes(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::FocusModes)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].supportedFocusModes;
}

const char* PlatformData::defaultFocusMode(int cameraId)
{
    const char *sPtr;
    if (cameraId == mActiveCameraId && !HalConfig.getString(sPtr, CPF::FocusModeDefault)) {
        return sPtr;
    }

    if (!validCameraId(cameraId, __FUNCTION__)) {
        return "";
    }
    return getInstance()->mCameras[cameraId].defaultFocusMode;
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
    return i->mCameras[cameraId].defaultHdr;
}

const char* PlatformData::supportedHdr(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].supportedHdr;

}

const char* PlatformData::defaultUltraLowLight(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].defaultUltraLowLight;
}

const char* PlatformData::supportedUltraLowLight(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].supportedUltraLowLight;
}

const char* PlatformData::defaultFaceRecognition(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].defaultFaceRecognition;
}

const char* PlatformData::supportedFaceRecognition(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].supportedFaceRecognition;
}

const char* PlatformData::defaultSmileShutter(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].defaultSmileShutter;
}

const char* PlatformData::supportedSmileShutter(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].supportedSmileShutter;
}

const char* PlatformData::defaultBlinkShutter(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].defaultBlinkShutter;
}

const char* PlatformData::supportedBlinkShutter(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].supportedBlinkShutter;
}

const char* PlatformData::defaultPanorama(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].defaultPanorama;
}

const char* PlatformData::supportedPanorama(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].supportedPanorama;
}

const char* PlatformData::defaultSceneDetection(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].defaultSceneDetection;
}

const char* PlatformData::supportedSceneDetection(int cameraId)
{
    PlatformBase *i = getInstance();
    if (cameraId < 0 || cameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, cameraId);
      return "";
    }
    return i->mCameras[cameraId].supportedSceneDetection;
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

int PlatformData::getMaxZoomFactor(void)
{
    int value;
    if (!HalConfig.getValue(value, CPF::ZoomDigital, CPF::Max)) {
        return value;
    }

    return getInstance()->mMaxZoomFactor;
}

bool PlatformData::supportVideoSnapshot(void)
{
    return getInstance()->mSupportVideoSnapshot;
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
    return getInstance()->mCameras[cameraId].maxNumYUVBufferForBurst;
}

int PlatformData::getMaxNumYUVBufferForBracket(int cameraId)
{
    if (!validCameraId(cameraId, __FUNCTION__)) {
        return 0;
    }
    return getInstance()->mCameras[cameraId].maxNumYUVBufferForBracket;
}

bool PlatformData::supportDualVideo(void)
{
    return getInstance()->mSupportDualVideo;
}

bool PlatformData::supportPreviewLimitation(void)
{
    return getInstance()->mSupportPreviewLimitation;
}

int PlatformData::getPreviewPixelFormat(void)
{
    return getInstance()->mPreviewFourcc;
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

int PlatformData::getSensorGainLag(void)
{
    int value;
    if (!HalConfig.getValue(value, CPF::Gain, CPF::Lag))
        return value;

    return getInstance()->mSensorGainLag;
}

int PlatformData::getSensorExposureLag(void)
{
    int value;
    if (!HalConfig.getValue(value, CPF::Exposure, CPF::Lag))
        return value;

    return getInstance()->mSensorExposureLag;
}

bool PlatformData::synchronizeExposure(void)
{
    PlatformBase *i = getInstance();
    if (mActiveCameraId < 0 || mActiveCameraId >= static_cast<int>(i->mCameras.size())) {
      ALOGE("%s: Invalid cameraId %d", __FUNCTION__, mActiveCameraId);
      return false;
    }
    return i->mCameras[mActiveCameraId].synchronizeExposure;
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

    const char* strFOV = getInstance()->mCameras[cameraId].verticalFOV;
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

    if (cameraId == mActiveCameraId && !HalConfig.getFloat(retVal, CPF::Fov, CPF::Vertical)) {
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

    const char* strFOV = getInstance()->mCameras[cameraId].horizontalFOV;
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

    if (cameraId == mActiveCameraId && !HalConfig.getFloat(retVal, CPF::Fov, CPF::Horizontal)) {
        return retVal;
    }

    // if don't configure vertical FOV angle, just set one default for it.
    retVal = 54.8;
    return retVal;
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

}; // namespace android
