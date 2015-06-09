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

#define LOG_TAG "Camera_EXIFMaker"

#include "EXIFMaker.h"
#include "LogHelper.h"
#include "ICameraHwControls.h"
#include "PlatformData.h"
#include <camera.h>


namespace android {

#define DEFAULT_ISO_SPEED 100

EXIFMaker::EXIFMaker(I3AControls *aaaControls) :
    m3AControls(aaaControls)
    ,thumbWidth(-1)
    ,thumbHeight(-1)
    ,exifSize(-1)
    ,initialized(false)
{
    LOG1("@%s", __FUNCTION__);
}

EXIFMaker::~EXIFMaker()
{
    LOG1("@%s", __FUNCTION__);
}

/**
 * Sets MakerNote field data.
 */
void EXIFMaker::setMakerNote(const ia_binary_data &aaaMkNoteData)
{
    LOG1("@%s: %d bytes", __FUNCTION__, aaaMkNoteData.size);

    if (aaaMkNoteData.data) {
        exifAttributes.makerNoteDataSize = aaaMkNoteData.size;
        exifAttributes.makerNoteData = (unsigned char*) aaaMkNoteData.data;
    }
}

/**
 * Sets picture meta data retrieved from atomisp kernel driver.
 *
 * @param ispData struct retrieved with ATOMISP_IOC_ISP_MAKERNOTE
 *                kernel ioctl
 */
void EXIFMaker::setDriverData(const atomisp_makernote_info &ispData)
{
    LOG1("@%s", __FUNCTION__);

    // f number
    if (ispData.f_number_curr > 0) {
        exifAttributes.fnumber.num = ispData.f_number_curr >> 16;
        exifAttributes.fnumber.den = ispData.f_number_curr & 0xffff;
        exifAttributes.max_aperture.num = exifAttributes.fnumber.num;
        exifAttributes.max_aperture.den = exifAttributes.fnumber.den;
    } else {
        ALOGW("Invalid fnumber %u from driver", ispData.f_number_curr);
    }

    LOG1("EXIF: fnumber=%u (num=%d, den=%d)", ispData.f_number_curr,
         exifAttributes.fnumber.num, exifAttributes.fnumber.den);

    // the actual focal length of the lens, in mm.
    // there is no API for lens position.
    if (ispData.focal_length > 0) {
        exifAttributes.focal_length.num = ispData.focal_length >> 16;
        exifAttributes.focal_length.den = ispData.focal_length & 0xffff;
    } else {
        ALOGW("Invalid focal length %u from driver", ispData.focal_length);
    }

    LOG1("EXIF: focal length=%u (num=%d, den=%d)", ispData.focal_length,
         exifAttributes.focal_length.num, exifAttributes.focal_length.den);
}

/**
 * Fills EXIF data after a picture has been taken to
 * record the active sensor, 3A and ISP state to EXIF metadata.
 *
 * This function is intented to set EXIF tags belonging
 * to the EXIF "Per Picture Camera Setting" group.
 *
 * @arg params active Android HAL parameters
 */
void EXIFMaker::pictureTaken(void)
{
    LOG1("@%s", __FUNCTION__);

    // TODO: The calls to mAAA should ideally be done at an earlier
    //       step. The basic problem is that this function is called
    //       from picture encoding thread, and in theory the ISP
    //       pipeline (and 3A) could already be processing a new
    //       image. In that case the values we get here would not be
    //       valid anymore for the images the EXIF tags describe.
    //       Currently as preview is not started until compressed
    //       images have been delivered, this is a not a problem, but
    //       this may change in the future.

    // brightness, -99.99 to 99.99. FFFFFFFF.H means unknown.
    float brightness;
    // TODO: The check for getAeManualBrightness of 3A should be moved
    //       to MetaData class, because the metadata collection happen
    //       at capture time
    if (m3AControls->getAeManualBrightness(&brightness) == NO_ERROR) {
        exifAttributes.brightness.num = (int)(brightness * 100);
        exifAttributes.brightness.den = 100;
        LOG1("EXIF: brightness = %.2f", brightness);
    } else {
        ALOGW("EXIF: Could not query brightness!");
    }

    // set the exposure program mode
    AeMode aeMode = m3AControls->getAeMode();
    switch (aeMode) {
    case CAM_AE_MODE_MANUAL:
        exifAttributes.exposure_program = EXIF_EXPOSURE_PROGRAM_MANUAL;
        LOG1("EXIF: Exposure Program = Manual");
        exifAttributes.exposure_mode = EXIF_EXPOSURE_MANUAL;
        LOG1("EXIF: Exposure Mode = Manual");
        break;
    case CAM_AE_MODE_SHUTTER_PRIORITY:
        exifAttributes.exposure_program = EXIF_EXPOSURE_PROGRAM_SHUTTER_PRIORITY;
        LOG1("EXIF: Exposure Program = Shutter Priority");
        break;
    case CAM_AE_MODE_APERTURE_PRIORITY:
        exifAttributes.exposure_program = EXIF_EXPOSURE_PROGRAM_APERTURE_PRIORITY;
        LOG1("EXIF: Exposure Program = Aperture Priority");
        break;
    case CAM_AE_MODE_AUTO:
    default:
        exifAttributes.exposure_program = EXIF_EXPOSURE_PROGRAM_NORMAL;
        LOG1("EXIF: Exposure Program = Normal");
        exifAttributes.exposure_mode = EXIF_EXPOSURE_AUTO;
        LOG1("EXIF: Exposure Mode = Auto");
        break;
    }

    // indicates the ISO speed of the camera
    int isoSpeed(DEFAULT_ISO_SPEED);
    if (m3AControls->getManualIso(&isoSpeed) == NO_ERROR) {
        exifAttributes.iso_speed_rating = isoSpeed;
    } else {
        ALOGW("EXIF: Could not query ISO speed!");
        exifAttributes.iso_speed_rating = DEFAULT_ISO_SPEED;
    }
    LOG1("EXIF: ISO=%d", isoSpeed);

    // the metering mode.
    MeteringMode meteringMode  = m3AControls->getAeMeteringMode();
    switch (meteringMode) {
    case CAM_AE_METERING_MODE_AUTO:
        exifAttributes.metering_mode = EXIF_METERING_AVERAGE;
        LOG1("EXIF: Metering Mode = Average");
        break;
    case CAM_AE_METERING_MODE_SPOT:
        exifAttributes.metering_mode = EXIF_METERING_SPOT;
        LOG1("EXIF: Metering Mode = Spot");
        break;
    case CAM_AE_METERING_MODE_CENTER:
        exifAttributes.metering_mode = EXIF_METERING_CENTER;
        LOG1("EXIF: Metering Mode = Center");
        break;
    case CAM_AE_METERING_MODE_CUSTOMIZED:
    default:
        exifAttributes.metering_mode = EXIF_METERING_OTHER;
        LOG1("EXIF: Metering Mode = Other");
        break;
    }

    // white balance mode. 0: auto; 1: manual
    AwbMode awbMode = m3AControls->getAwbMode();
    switch (awbMode) {
    case CAM_AWB_MODE_AUTO:
    case CAM_AWB_MODE_NOT_SET:
        exifAttributes.white_balance = EXIF_WB_AUTO;
        LOG1("EXIF: Whitebalance = Auto");
        break;
    default:
        exifAttributes.white_balance = EXIF_WB_MANUAL;
        LOG1("EXIF: Whitebalance = Manual");
        break;
    }

    // light source type. Refer to EXIF V2.3
    // TBD. Now light source is only set to UNKNOWN, when WB is auto mode.
    if (CAM_AWB_MODE_AUTO == awbMode) {
        exifAttributes.light_source = EXIF_LIGHT_SOURCE_UNKNOWN;
    }
    else {
        AwbMode lightSource = m3AControls->getLightSource();
        switch (lightSource) {
        case CAM_AWB_MODE_MANUAL_INPUT:
        case CAM_AWB_MODE_AUTO:
        case CAM_AWB_MODE_NOT_SET:
            exifAttributes.light_source  = EXIF_LIGHT_SOURCE_OTHER_LIGHT_SOURCE;
            break;
        case CAM_AWB_MODE_SUNSET:
            exifAttributes.light_source  = EXIF_LIGHT_SOURCE_TUNGSTEN;
            break;
        case CAM_AWB_MODE_DAYLIGHT:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_FINE_WEATHER;
            break;
        case CAM_AWB_MODE_CLOUDY:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_CLOUDY_WEATHER;
            break;
        case CAM_AWB_MODE_SHADOW:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_SHADE;
            break;
        case CAM_AWB_MODE_TUNGSTEN:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_TUNGSTEN;
            break;
        case CAM_AWB_MODE_WARM_FLUORESCENT:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_WARM_WHITE_FLUORESCENT;
            break;
        case CAM_AWB_MODE_FLUORESCENT:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_FLUORESCENT;
            break;
        case CAM_AWB_MODE_WARM_INCANDESCENT:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_TUNGSTEN;
            break;
        default:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_OTHER_LIGHT_SOURCE;
            break;
        }
    }

    // scene mode
    SceneMode sceneMode = m3AControls->getAeSceneMode();
    switch (sceneMode) {
    case CAM_AE_SCENE_MODE_PORTRAIT:
        exifAttributes.scene_capture_type = EXIF_SCENE_PORTRAIT;
        LOG1("EXIF: Scene Mode = Portrait");
        break;
    case CAM_AE_SCENE_MODE_LANDSCAPE:
        exifAttributes.scene_capture_type = EXIF_SCENE_LANDSCAPE;
        LOG1("EXIF: Scene Mode = Landscape");
        break;
    case CAM_AE_SCENE_MODE_NIGHT:
    case CAM_AE_SCENE_MODE_NIGHT_PORTRAIT:
        exifAttributes.scene_capture_type = EXIF_SCENE_NIGHT;
        LOG1("EXIF: Scene Mode = Night");
        break;
    case CAM_AE_SCENE_MODE_FIREWORKS:
        LOG1("EXIF: Scene Mode = Standard");
        break;
    default:
        exifAttributes.scene_capture_type = EXIF_SCENE_STANDARD;
        LOG1("EXIF: Scene Mode = Standard");
        break;
    }

}

/**
 * Called when the the camera static configuration is known.
 *
 * @arg params active Android HAL parameters
 */
    void EXIFMaker::initialize(const CameraParameters &params, int zoomRatio)
{
    LOG1("@%s: params = %p", __FUNCTION__, &params);
    const char *p = NULL;
    /* We clear the exif attributes, so we won't be using some old values
     * from a previous EXIF generation.
     */
    clear();

    // Initialize the exifAttributes with specific values
    // time information
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    if (timeinfo) {
        strftime((char *)exifAttributes.date_time, sizeof(exifAttributes.date_time), "%Y:%m:%d %H:%M:%S", timeinfo);
        // fields: tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst, tm_gmtoff, tm_zone
    } else {
        ALOGW("NULL timeinfo from localtime(), using defaults...");
        struct tm tmpTime = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "UTC"};
        strftime((char *)exifAttributes.date_time, sizeof(exifAttributes.date_time), "%Y:%m:%d %H:%M:%S", &tmpTime);
    }

    // conponents configuration.
    // Default = 4 5 6 0(if RGB uncompressed), 1 2 3 0(other cases)
    // 0 = does not exist; 1 = Y; 2 = Cb; 3 = Cr; 4 = R; 5 = G; 6 = B; other = reserved
    exifAttributes.components_configuration[0] = 1;
    exifAttributes.components_configuration[1] = 2;
    exifAttributes.components_configuration[2] = 3;
    exifAttributes.components_configuration[3] = 0;

    // set default values for fnumber and focal length
    // (see EXIFMaker::setDriverData() how to override these)
    exifAttributes.fnumber.num = EXIF_DEF_FNUMBER_NUM;
    exifAttributes.fnumber.den = EXIF_DEF_FNUMBER_DEN;
    exifAttributes.focal_length.num = EXIF_DEF_FOCAL_LEN_NUM;
    exifAttributes.focal_length.den = EXIF_DEF_FOCAL_LEN_DEN;

    // TODO: should ISO be omitted if the value cannot be trusted?
    exifAttributes.iso_speed_rating = DEFAULT_ISO_SPEED;

    // max aperture. the smallest F number of the lens. unit is APEX value.
    // TBD, should get from driver
    exifAttributes.max_aperture.num = exifAttributes.aperture.num;
    exifAttributes.max_aperture.den = exifAttributes.aperture.den;

    // subject distance,    0 means distance unknown; (~0) means infinity.
    exifAttributes.subject_distance.num = EXIF_DEF_SUBJECT_DISTANCE_UNKNOWN;
    exifAttributes.subject_distance.den = 1;

    // light source, 0 means light source unknown
    exifAttributes.light_source = 0;
    p = params.get(CameraParameters::KEY_WHITE_BALANCE);
    String8 whiteBalance(p, (p == NULL ? 0 : strlen(p))); // String8 segfaults if given a NULL
    if (!whiteBalance.isEmpty()) {
        if(whiteBalance == CameraParameters::WHITE_BALANCE_AUTO) {
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_UNKNOWN;
        } else if(whiteBalance == CameraParameters::WHITE_BALANCE_INCANDESCENT) {
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_TUNGSTEN;
        } else if(whiteBalance == CameraParameters::WHITE_BALANCE_FLUORESCENT) {
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_FLUORESCENT;
        } else if(whiteBalance == CameraParameters::WHITE_BALANCE_DAYLIGHT) {
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_DAYLIGHT;
        } else if(whiteBalance == CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT) {
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_CLOUDY_WEATHER;
        } else if(whiteBalance == CameraParameters::WHITE_BALANCE_SHADE) {
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_SHADE;
        }
    }

    // gain control, 0 = none;
    // 1 = low gain up; 2 = high gain up; 3 = low gain down; 4 = high gain down
    exifAttributes.gain_control = 0;

    // contrast, 0 = normal; 1 = soft; 2 = hard; other = reserved
    const char *contrast = params.get(IntelCameraParameters::KEY_CONTRAST_MODE);

    exifAttributes.contrast = EXIF_CONTRAST_NORMAL;
    if (contrast) {
        if (!strcmp(contrast, IntelCameraParameters::CONTRAST_MODE_SOFT))
            exifAttributes.contrast = EXIF_CONTRAST_SOFT;
        else if (!strcmp(contrast, IntelCameraParameters::CONTRAST_MODE_HARD))
            exifAttributes.contrast = EXIF_CONTRAST_HARD;
    }

    // saturation, 0 = normal; 1 = Low saturation; 2 = High saturation; other = reserved
    const char *saturation = params.get(IntelCameraParameters::KEY_SATURATION_MODE);
    exifAttributes.saturation = EXIF_SATURATION_NORMAL;
    if (saturation) {
        if (!strcmp(saturation, IntelCameraParameters::SATURATION_MODE_LOW))
            exifAttributes.saturation = EXIF_SATURATION_LOW;
        else if (!strcmp(saturation, IntelCameraParameters::SATURATION_MODE_HIGH))
            exifAttributes.saturation = EXIF_SATURATION_HIGH;
    }

    // sharpness, 0 = normal; 1 = soft; 2 = hard; other = reserved
    const char *sharpness = params.get(IntelCameraParameters::KEY_SHARPNESS_MODE);
    exifAttributes.sharpness = EXIF_SHARPNESS_NORMAL;
    if (sharpness) {
        if (!strcmp(sharpness, IntelCameraParameters::SHARPNESS_MODE_SOFT))
            exifAttributes.sharpness = EXIF_SHARPNESS_SOFT;
        else if (!strcmp(sharpness, IntelCameraParameters::SHARPNESS_MODE_HARD))
            exifAttributes.sharpness = EXIF_SHARPNESS_HARD;
    }

    // the picture's width and height
    params.getPictureSize((int*)&exifAttributes.width, (int*)&exifAttributes.height);

    thumbWidth = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    thumbHeight = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);

    int rotation = params.getInt(CameraParameters::KEY_ROTATION);
    exifAttributes.orientation = 1;
    if (0 == rotation)
        exifAttributes.orientation = 1;
    else if (90 == rotation)
        exifAttributes.orientation = 6;
    else if (180 == rotation)
        exifAttributes.orientation = 3;
    else if (270 == rotation)
        exifAttributes.orientation = 8;
    LOG1("EXIF: rotation value:%d degrees, orientation value:%d",
            rotation, exifAttributes.orientation);

    exifAttributes.zoom_ratio.num = zoomRatio;
    exifAttributes.zoom_ratio.den = 100;
    LOG1("EXIF: zoom=%u/%u", exifAttributes.zoom_ratio.num, exifAttributes.zoom_ratio.den);

    // metering mode, 0 = normal; 1 = soft; 2 = hard; other = reserved
    exifAttributes.metering_mode = EXIF_METERING_UNKNOWN;
    p = params.get(IntelCameraParameters::KEY_AE_METERING_MODE);
    String8 meteringMode(p, (p == NULL ? 0 : strlen(p))); // String8 segfaults if given a NULL
    if (!meteringMode.isEmpty()) {
        if (meteringMode == IntelCameraParameters::AE_METERING_MODE_AUTO) {
            exifAttributes.metering_mode = EXIF_METERING_AVERAGE;
        } else if (meteringMode == IntelCameraParameters::AE_METERING_MODE_CENTER) {
            exifAttributes.metering_mode = EXIF_METERING_CENTER;
        } else if (meteringMode == IntelCameraParameters::AE_METERING_MODE_SPOT) {
            exifAttributes.metering_mode = EXIF_METERING_SPOT;
        }
    }

    initializeLocation(params);

    initialized = true;
}

void EXIFMaker::initializeLocation(const CameraParameters &params)
{
    LOG1("@%s", __FUNCTION__);
    // GIS information
    bool gpsEnabled = false;
    const char *platitude = params.get(CameraParameters::KEY_GPS_LATITUDE);
    const char *plongitude = params.get(CameraParameters::KEY_GPS_LONGITUDE);
    const char *paltitude = params.get(CameraParameters::KEY_GPS_ALTITUDE);
    const char *ptimestamp = params.get(CameraParameters::KEY_GPS_TIMESTAMP);
    const char *pprocmethod = params.get(CameraParameters::KEY_GPS_PROCESSING_METHOD);
    const char *pimgdirection = params.get(IntelCameraParameters::KEY_GPS_IMG_DIRECTION);
    const char *pimgdirectionref = params.get(IntelCameraParameters::KEY_GPS_IMG_DIRECTION_REF);

    double imgdirection = 0.0;
    if (pimgdirection) {
        imgdirection = atof(pimgdirection);
        if ((pimgdirectionref == NULL)
            ||((strcmp(pimgdirectionref, IntelCameraParameters::GPS_IMG_DIRECTION_REF_TRUE) != 0)
            && (strcmp(pimgdirectionref, IntelCameraParameters::GPS_IMG_DIRECTION_REF_MAGNETIC) != 0))
            || (imgdirection < 0 || imgdirection > 359.99))
            pimgdirection = NULL;
    }

    // check whether the GIS Information is valid
    if((NULL != platitude) || (NULL != plongitude) || (NULL != paltitude)
       || (NULL != ptimestamp) || (NULL != pprocmethod) || (NULL != pimgdirection))
        gpsEnabled = true;

    exifAttributes.enableGps = 0;
    LOG1("EXIF: gpsEnabled: %d", gpsEnabled);

    // the version is given as 2.2.0.0, it is mandatory when GPSInfo tag is present
    if(gpsEnabled) {
        const unsigned char gpsversion[4] = {0x02, 0x02, 0x00, 0x00};
        memcpy(exifAttributes.gps_version_id, gpsversion, sizeof(gpsversion));
    } else {
        return;
    }

    // latitude, for example, 39.904214 degrees, N
    if (platitude) {
        double latitude = atof(platitude);
        if(latitude > 0)
            memcpy(exifAttributes.gps_latitude_ref, "N", sizeof(exifAttributes.gps_latitude_ref));
        else
            memcpy(exifAttributes.gps_latitude_ref, "S", sizeof(exifAttributes.gps_latitude_ref));
        latitude = fabs(latitude);
        exifAttributes.gps_latitude[0].num = (uint32_t)latitude;
        exifAttributes.gps_latitude[0].den = 1;
        exifAttributes.gps_latitude[1].num = (uint32_t)((latitude - exifAttributes.gps_latitude[0].num) * 60);
        exifAttributes.gps_latitude[1].den = 1;
        exifAttributes.gps_latitude[2].num = (uint32_t)(((latitude - exifAttributes.gps_latitude[0].num) * 60 - exifAttributes.gps_latitude[1].num) * 60 * 100);
        exifAttributes.gps_latitude[2].den = 100;
        exifAttributes.enableGps |= EXIF_GPS_LATITUDE;
        LOG1("EXIF: latitude, ref:%s, dd:%d, mm:%d, ss:%d",
                exifAttributes.gps_latitude_ref, exifAttributes.gps_latitude[0].num,
                exifAttributes.gps_latitude[1].num, exifAttributes.gps_latitude[2].num);
    }

    // longitude, for example, 116.407413 degrees, E
    if (plongitude) {
        double longitude = atof(plongitude);
        if(longitude > 0)
            memcpy(exifAttributes.gps_longitude_ref, "E", sizeof(exifAttributes.gps_longitude_ref));
        else
            memcpy(exifAttributes.gps_longitude_ref, "W", sizeof(exifAttributes.gps_longitude_ref));
        longitude = fabs(longitude);
        exifAttributes.gps_longitude[0].num = (uint32_t)longitude;
        exifAttributes.gps_longitude[0].den = 1;
        exifAttributes.gps_longitude[1].num = (uint32_t)((longitude - exifAttributes.gps_longitude[0].num) * 60);
        exifAttributes.gps_longitude[1].den = 1;
        exifAttributes.gps_longitude[2].num = (uint32_t)(((longitude - exifAttributes.gps_longitude[0].num) * 60 - exifAttributes.gps_longitude[1].num) * 60 * 100);
        exifAttributes.gps_longitude[2].den = 100;
        exifAttributes.enableGps |= EXIF_GPS_LONGITUDE;
        LOG1("EXIF: longitude, ref:%s, dd:%d, mm:%d, ss:%d",
                exifAttributes.gps_longitude_ref, exifAttributes.gps_longitude[0].num,
                exifAttributes.gps_longitude[1].num, exifAttributes.gps_longitude[2].num);
    }

    // altitude
    if (paltitude) {
        // altitude, sea level or above sea level, set it to 0; below sea level, set it to 1
        float altitude = atof(paltitude);
        exifAttributes.gps_altitude_ref = ((altitude > 0) ? 0 : 1);
        altitude = fabs(altitude);
        exifAttributes.gps_altitude.num = (uint32_t)altitude;
        exifAttributes.gps_altitude.den = 1;
        exifAttributes.enableGps |= EXIF_GPS_ALTITUDE;
        LOG1("EXIF: altitude, ref:%d, height:%d",
                exifAttributes.gps_altitude_ref, exifAttributes.gps_altitude.num);
    }

    // timestamp
    if (ptimestamp != NULL) {
        long timestamp = atol(ptimestamp);
        struct tm time;
        if (timestamp >= LONG_MAX || timestamp <= LONG_MIN)
        {
            timestamp = 0;
            ALOGW("invalid timestamp was provided, defaulting to 0 (i.e. 1970)");
        }
        gmtime_r(&timestamp, &time);
        time.tm_year += 1900;
        time.tm_mon += 1;
        exifAttributes.gps_timestamp[0].num = time.tm_hour;
        exifAttributes.gps_timestamp[0].den = 1;
        exifAttributes.gps_timestamp[1].num = time.tm_min;
        exifAttributes.gps_timestamp[1].den = 1;
        exifAttributes.gps_timestamp[2].num = time.tm_sec;
        exifAttributes.gps_timestamp[2].den = 1;
        exifAttributes.enableGps |= EXIF_GPS_TIMESTAMP;
        snprintf((char *)exifAttributes.gps_datestamp, sizeof(exifAttributes.gps_datestamp), "%04d:%02d:%02d",
                time.tm_year, time.tm_mon, time.tm_mday);
        LOG1("EXIF: timestamp, year:%d,mon:%d,day:%d,hour:%d,min:%d,sec:%d",
                time.tm_year, time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
    }

    // processing method
    if (pprocmethod) {
        unsigned len;
        if(strlen(pprocmethod) + 1 >= sizeof(exifAttributes.gps_processing_method))
            len = sizeof(exifAttributes.gps_processing_method);
        else
            len = strlen(pprocmethod) + 1;
        memcpy(exifAttributes.gps_processing_method, pprocmethod, len);
        exifAttributes.enableGps |= EXIF_GPS_PROCMETHOD;
        LOG1("EXIF: GPS processing method:%s", exifAttributes.gps_processing_method);
    }

    if (pimgdirection && pimgdirectionref)  {
        if (strcmp(pimgdirectionref, IntelCameraParameters::GPS_IMG_DIRECTION_REF_TRUE) == 0)
            memcpy(exifAttributes.gps_img_direction_ref, "T", sizeof(exifAttributes.gps_img_direction_ref));
        else if (strcmp(pimgdirectionref, IntelCameraParameters::GPS_IMG_DIRECTION_REF_MAGNETIC) == 0)
            memcpy(exifAttributes.gps_img_direction_ref, "M", sizeof(exifAttributes.gps_img_direction_ref));
        exifAttributes.gps_img_direction.num = (uint32_t)(imgdirection*100);
        exifAttributes.gps_img_direction.den = 100;
        exifAttributes.enableGps |= EXIF_GPS_IMG_DIRECTION;
        LOG1("EXIF: GPS img direction ref:%s, img direction num:%d, den:%d",
            exifAttributes.gps_img_direction_ref,
            exifAttributes.gps_img_direction.num,
            exifAttributes.gps_img_direction.den);
    }
}

void EXIFMaker::setSensorAeConfig(const SensorAeConfig& aeConfig)
{
    LOG1("@%s", __FUNCTION__);

    if (m3AControls->isIntel3A()) {
        // overwrite the value that we get from the sensor with the value from AEC
        LOG1("EXIF: Intel 3A used, setting exposure information from AEC");

        // conversion formula taken directly from libcamera1
        int expTime = (int) (pow(2.0, -aeConfig.aecApexTv * (1.52587890625e-005)) * 10000);

        // Exposure time
        exifAttributes.exposure_time.num = expTime;
        exifAttributes.exposure_time.den = 10000;

        // APEX shutter speed
        exifAttributes.shutter_speed.num = aeConfig.aecApexTv;
        exifAttributes.shutter_speed.den = 65536;

        // APEX aperture value
        exifAttributes.aperture.num = aeConfig.aecApexAv;
        exifAttributes.aperture.den = 65536;

        // exposure bias. unit is APEX value. -99.99 to 99.99
        if (aeConfig.evBias > EV_LOWER_BOUND && aeConfig.evBias < EV_UPPER_BOUND) {
            exifAttributes.exposure_bias.num = (int)(aeConfig.evBias * 100);
            exifAttributes.exposure_bias.den = 100;
            LOG1("EXIF: Ev = %.2f", aeConfig.evBias);
        } else {
            ALOGW("EXIF: Invalid Ev!");
        }
    } else {
        if (aeConfig.expTime > 0) {
            // exposure time - from aeConfig
            exifAttributes.exposure_time.num = aeConfig.expTime;
            exifAttributes.exposure_time.den = 10000;
            // APEX shutter speed = -log2(exposure time) - from aeConfig
            double exp_t = (double)aeConfig.expTime / (double)10000;
            double shutter = APEX_EXPOSURE_TO_SHUTTER(exp_t);
            exifAttributes.shutter_speed.num = (uint32_t)(shutter * 10000);
            exifAttributes.shutter_speed.den = 10000;
        } else {
            exifAttributes.exposure_time.num = exifAttributes.exposure_time.den = 0;
            exifAttributes.shutter_speed.num = exifAttributes.shutter_speed.den = 0;
        }

        // APEX aperture value = 2 x log2(F number) - from TOMISP_IOC_ISP_MAKERNOTE
        // fnumber.den never is 0, for initialize() setting to default
        double f_number = (double)exifAttributes.fnumber.num / (double)exifAttributes.fnumber.den;
        exifAttributes.aperture.num = (uint32_t)(10000 * APEX_FNUM_TO_APERTURE(f_number));
        exifAttributes.aperture.den = 10000;

        // exposure bias - hardcode, set to default 0
        // TODO: exposure bias is N/A for SoC sensors now
        exifAttributes.exposure_bias.num = 0;
        exifAttributes.exposure_bias.den = 100;
    }

    LOG1("EXIF: shutter speed=%u/%u", exifAttributes.shutter_speed.num,
         exifAttributes.shutter_speed.den);
    LOG1("EXIF: exposure time=%u/%u", exifAttributes.exposure_time.num,
         exifAttributes.exposure_time.den);
    LOG1("EXIF: aperture=%u/%u", exifAttributes.aperture.num,
         exifAttributes.aperture.den);
}

void EXIFMaker::clear()
{
    LOG1("@%s", __FUNCTION__);
    // Reset all the attributes
    memset(&exifAttributes, 0, sizeof(exifAttributes));

    // Initialize the common values
    exifAttributes.enableThumb = false;
    strncpy((char*)exifAttributes.image_description, EXIF_DEF_IMAGE_DESCRIPTION, sizeof(exifAttributes.image_description));
    strncpy((char*)exifAttributes.maker, PlatformData::manufacturerName(), sizeof(exifAttributes.maker));
    strncpy((char*)exifAttributes.model, PlatformData::productName(), sizeof(exifAttributes.model));
    strncpy((char*)exifAttributes.software, EXIF_DEF_SOFTWARE, sizeof(exifAttributes.software));

    memcpy(exifAttributes.exif_version, EXIF_DEF_EXIF_VERSION, sizeof(exifAttributes.exif_version));
    memcpy(exifAttributes.flashpix_version, EXIF_DEF_FLASHPIXVERSION, sizeof(exifAttributes.flashpix_version));

    // initially, set default flash
    exifAttributes.flash = EXIF_DEF_FLASH;

    // normally it is sRGB, 1 means sRGB. FFFF.H means uncalibrated
    exifAttributes.color_space = EXIF_DEF_COLOR_SPACE;

    // the number of pixels per ResolutionUnit in the w or h direction
    // 72 means the image resolution is unknown
    exifAttributes.x_resolution.num = EXIF_DEF_RESOLUTION_NUM;
    exifAttributes.x_resolution.den = EXIF_DEF_RESOLUTION_DEN;
    exifAttributes.y_resolution.num = exifAttributes.x_resolution.num;
    exifAttributes.y_resolution.den = exifAttributes.x_resolution.den;
    // resolution unit, 2 means inch
    exifAttributes.resolution_unit = EXIF_DEF_RESOLUTION_UNIT;
    // when thumbnail uses JPEG compression, this tag 103H's value is set to 6
    exifAttributes.compression_scheme = EXIF_DEF_COMPRESSION;

    // the TIFF default is 1 (centered)
    exifAttributes.ycbcr_positioning = EXIF_DEF_YCBCR_POSITIONING;

    initialized = false;
}

void EXIFMaker::enableFlash()
{
    LOG1("@%s", __FUNCTION__);
    // bit 0: flash fired; bit 1 to 2: flash return; bit 3 to 4: flash mode;
    // bit 5: flash function; bit 6: red-eye mode;
    exifAttributes.flash = EXIF_FLASH_ON;
    exifAttributes.light_source = EXIF_LIGHT_SOURCE_FLASH;
}

void EXIFMaker::setThumbnail(unsigned char *data, size_t size)
{
    LOG1("@%s: data = %p, size = %u", __FUNCTION__, data, size);
    exifAttributes.enableThumb = true;
    exifAttributes.widthThumb = thumbWidth;
    exifAttributes.heightThumb = thumbHeight;
    if (encoder.setThumbData(data, size) != EXIF_SUCCESS) {
        ALOGE("Error in setting EXIF thumbnail");
    }
}

bool EXIFMaker::isThumbnailSet() const {
    LOG1("@%s", __FUNCTION__);
    return encoder.isThumbDataSet();
}

size_t EXIFMaker::makeExif(unsigned char **data)
{
    LOG1("@%s", __FUNCTION__);
    if (*data == NULL) {
        ALOGE("NULL pointer passed for EXIF. Cannot generate EXIF!");
        return 0;
    }
    if (encoder.makeExif(*data, &exifAttributes, &exifSize) == EXIF_SUCCESS) {
        LOG1("Generated EXIF (@%p) of size: %u", *data, exifSize);
        return exifSize;
    }
    return 0;
}

void EXIFMaker::setMaker(const char *data)
{
    LOG1("@%s: data = %s", __FUNCTION__, data);
    size_t len(sizeof(exifAttributes.maker));
    strncpy((char*)exifAttributes.maker, data, len-1);
    exifAttributes.maker[len-1] = '\0';
}

void EXIFMaker::setModel(const char *data)
{
    LOG1("@%s: data = %s", __FUNCTION__, data);
    size_t len(sizeof(exifAttributes.model));
    strncpy((char*)exifAttributes.model, data, len-1);
    exifAttributes.model[len-1] = '\0';
}

void EXIFMaker::setSoftware(const char *data)
{
    LOG1("@%s: data = %s", __FUNCTION__, data);
    size_t len(sizeof(exifAttributes.software));
    strncpy((char*)exifAttributes.software, data, len-1);
    exifAttributes.software[len-1] = '\0';
}

}; // namespace android
