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

#define LOG_TAG "Camera_EXIFFields"

#include "EXIFFields.h"
#include "LogHelper.h"

namespace android {

#define DEFAULT_ISO_SPEED 100

EXIFFields::EXIFFields()
{
    LOG1("@%s", __FUNCTION__);
    reset();
}

EXIFFields::~EXIFFields()
{
    LOG1("@%s", __FUNCTION__);
}

void EXIFFields::reset()
{
    setCommonFields();
    setUnknownFields();
}

void EXIFFields::setGeneralFields(bool flashEnabled,
                                  int pictureWidth,
                                  int pictureHeight,
                                  int thumbnailWidth,
                                  int thumbnailHeight,
                                  ExifOrientationType orientation)
{
    // bit 0: flash fired; bit 1 to 2: flash return; bit 3 to 4: flash mode;
    // bit 5: flash function; bit 6: red-eye mode;
    mExif.flash = EXIF_FLASH_ON;

    // pic width and height
    mExif.width = pictureWidth;
    mExif.height = pictureHeight;

    // thum width and height
    if (thumbnailWidth == 0 || thumbnailHeight == 0) {
        mExif.enableThumb = false;
    } else {
        mExif.enableThumb = true;
        mExif.widthThumb = thumbnailWidth;
        mExif.heightThumb = thumbnailHeight;
    }

    mExif.orientation = orientation;
}

void EXIFFields::setGPSFields(long timestamp,
                              float latitude,
                              float longitude,
                              float altitude,
                              const char *processingMethod)
{
    struct tm time;

    mExif.enableGps = true;

    // the version is given as 2.2.0.0, it is mandatory when GPSInfo tag is present
    mExif.gps_version_id[0] = 0x02;
    mExif.gps_version_id[1] = 0x02;
    mExif.gps_version_id[2] = 0x00;
    mExif.gps_version_id[3] = 0x00;

    // latitude, for example, 39.904214 degrees, N
    if (latitude > 0)
        memcpy(mExif.gps_latitude_ref, "N", sizeof(mExif.gps_latitude_ref));
    else
        memcpy(mExif.gps_latitude_ref, "S", sizeof(mExif.gps_latitude_ref));
    latitude = fabs(latitude);
    mExif.gps_latitude[0].num = (uint32_t)latitude;
    mExif.gps_latitude[0].den = 1;
    mExif.gps_latitude[1].num = (uint32_t)((latitude - mExif.gps_latitude[0].num) * 60);
    mExif.gps_latitude[1].den = 1;
    mExif.gps_latitude[2].num = (uint32_t)(((latitude - mExif.gps_latitude[0].num) * 60 -
                mExif.gps_latitude[1].num) * 60 * 100);
    mExif.gps_latitude[2].den = 100;
    LOG1("EXIF: latitude, ref:%s, dd:%d, mm:%d, ss:%d",
            mExif.gps_latitude_ref, mExif.gps_latitude[0].num,
            mExif.gps_latitude[1].num, mExif.gps_latitude[2].num);

    // longitude, for example, 116.407413 degrees, E
    if (longitude > 0)
        memcpy(mExif.gps_longitude_ref, "E", sizeof(mExif.gps_longitude_ref));
    else
        memcpy(mExif.gps_longitude_ref, "W", sizeof(mExif.gps_longitude_ref));
    longitude = fabs(longitude);
    mExif.gps_longitude[0].num = (uint32_t)longitude;
    mExif.gps_longitude[0].den = 1;
    mExif.gps_longitude[1].num = (uint32_t)((longitude - mExif.gps_longitude[0].num) * 60);
    mExif.gps_longitude[1].den = 1;
    mExif.gps_longitude[2].num = (uint32_t)(((longitude - mExif.gps_longitude[0].num) * 60 -
                mExif.gps_longitude[1].num) * 60 * 100);
    mExif.gps_longitude[2].den = 100;
    LOG1("EXIF: longitude, ref:%s, dd:%d, mm:%d, ss:%d",
            mExif.gps_longitude_ref, mExif.gps_longitude[0].num,
            mExif.gps_longitude[1].num, mExif.gps_longitude[2].num);

    // altitude, sea level or above sea level, set it to 0; below sea level, set it to 1
    mExif.gps_altitude_ref = ((altitude > 0) ? 0 : 1);
    altitude = fabs(altitude);
    mExif.gps_altitude.num = (uint32_t)altitude;
    mExif.gps_altitude.den = 1;
    LOG1("EXIF: altitude, ref:%d, height:%d",
            mExif.gps_altitude_ref, mExif.gps_altitude.num);

    // timestamp
    gmtime_r(&timestamp, &time);
    time.tm_year += 1900;
    time.tm_mon += 1;
    mExif.gps_timestamp[0].num = time.tm_hour;
    mExif.gps_timestamp[0].den = 1;
    mExif.gps_timestamp[1].num = time.tm_min;
    mExif.gps_timestamp[1].den = 1;
    mExif.gps_timestamp[2].num = time.tm_sec;
    mExif.gps_timestamp[2].den = 1;
    snprintf((char *)mExif.gps_datestamp, sizeof(mExif.gps_datestamp), "%04d:%02d:%02d",
            time.tm_year, time.tm_mon, time.tm_mday);
    LOG1("EXIF: timestamp, year:%d,mon:%d,day:%d,hour:%d,min:%d,sec:%d",
            time.tm_year, time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);

    // processing method
    if (processingMethod != NULL) {
        strncpy((char*) mExif.gps_processing_method,
                processingMethod,
                sizeof(mExif.gps_processing_method));
        LOG1("EXIF: GPS processing method:%s", mExif.gps_processing_method);
    }
}

void EXIFFields::setHardwareFields(float focalLength,            // num/denom
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
                                   CamExifSceneCaptureType sceneMode)
{
    // f number
    if (fNumber != 0) {
        mExif.fnumber.num = fNumber >> 16;
        mExif.fnumber.den = fNumber & 0xffff;
    } else {
        mExif.fnumber.num = EXIF_DEF_FNUMBER_NUM;
        mExif.fnumber.den = EXIF_DEF_FNUMBER_DEN;
    }

    LOG1("EXIF: fnumber=%u (num=%d, den=%d)", fNumber, mExif.fnumber.num, mExif.fnumber.den);

    mExif.max_aperture.num = mExif.fnumber.num;
    mExif.max_aperture.den = mExif.fnumber.den;

    // exposure time
    mExif.exposure_time.num = exposureTime * 10000;
    mExif.exposure_time.den = mExif.exposure_time.num ? 10000 : 0;
    LOG1("EXIF: exposure time=%u", exposureTime);

    // shutter speed, = -log2(exposure time)
    mExif.shutter_speed.den = 0;
    if (mExif.exposure_time.den) {
        float exp_t = (float)(exposureTime / 10000.0);
        float shutter = -1.0 * (log10(exp_t) / log10(2.0));
        mExif.shutter_speed.num = (shutter * 10000);
        mExif.shutter_speed.den = 10000;
        LOG1("EXIF: shutter speed=%.2f", shutter);
    }

    // flash
    mExif.flash = EXIF_DEF_FLASH;

    // aperture
    mExif.aperture.num = 100*(int)((1.0*mExif.fnumber.num/mExif.fnumber.den) * sqrt(100.0/aperture));
    mExif.aperture.den = 100;
    LOG1("EXIF: aperture=%u", aperture);

    // brightness, -99.99 to 99.99. FFFFFFFF.H means unknown.
    mExif.brightness.num = (int)(brightness * 100);
    mExif.brightness.den = 100;
    LOG1("EXIF: brightness = %.2f", brightness);

    // exposure bias. unit is APEX value. -99.99 to 99.99
    mExif.exposure_bias.num = (int)(exposureBias * 100);
    mExif.exposure_bias.den = 100;
    LOG1("EXIF: Ev = %.2f", exposureBias);

    // set the exposure program mode
    mExif.exposure_program = exposureProgram;
    mExif.exposure_mode = exposureMode;

    // indicates the ISO speed of the camera
    mExif.iso_speed_rating = isoSpeed;
    LOG1("EXIF: ISO=%d", isoSpeed);

    // the metering mode.
    mExif.metering_mode = meteringMode;

    // white balance mode
    mExif.white_balance = wbMode;

    // scene mode
    mExif.scene_capture_type = sceneMode;

    // the actual focal length of the lens, in mm.
    mExif.focal_length.num = (int) (focalLength * 100);
    mExif.focal_length.den = 100;
    LOG1("EXIF: focal length=%f (num=%d, den=%d)",
            focalLength, mExif.focal_length.num, mExif.focal_length.den);
}

void EXIFFields::combineFields(exif_attribute_t *exif)
{
    if (exif)
        *exif = mExif;
}

void EXIFFields::setCommonFields()
{
    // Reset all the attributes
    memset(&mExif, 0, sizeof(mExif));

    // Initialize the common values
    mExif.enableThumb = false;
    strncpy((char*)mExif.image_description, EXIF_DEF_IMAGE_DESCRIPTION, sizeof(mExif.image_description));
    strncpy((char*)mExif.maker, EXIF_DEF_MAKER, sizeof(mExif.maker));
    strncpy((char*)mExif.model, EXIF_DEF_MODEL, sizeof(mExif.model));
    strncpy((char*)mExif.software, EXIF_DEF_SOFTWARE, sizeof(mExif.software));

    memcpy(mExif.exif_version, EXIF_DEF_EXIF_VERSION, sizeof(mExif.exif_version));
    memcpy(mExif.flashpix_version, EXIF_DEF_FLASHPIXVERSION, sizeof(mExif.flashpix_version));

    // initially, set default flash
    mExif.flash = EXIF_DEF_FLASH;

    // normally it is sRGB, 1 means sRGB. FFFF.H means uncalibrated
    mExif.color_space = EXIF_DEF_COLOR_SPACE;

    // the number of pixels per ResolutionUnit in the w or h direction
    // 72 means the image resolution is unknown
    mExif.x_resolution.num = EXIF_DEF_RESOLUTION_NUM;
    mExif.x_resolution.den = EXIF_DEF_RESOLUTION_DEN;
    mExif.y_resolution.num = mExif.x_resolution.num;
    mExif.y_resolution.den = mExif.x_resolution.den;
    // resolution unit, 2 means inch
    mExif.resolution_unit = EXIF_DEF_RESOLUTION_UNIT;
    // when thumbnail uses JPEG compression, this tag 103H's value is set to 6
    mExif.compression_scheme = EXIF_DEF_COMPRESSION;

    // the TIFF default is 1 (centered)
    mExif.ycbcr_positioning = EXIF_DEF_YCBCR_POSITIONING;
}

// TODO: This method currently sets data uknown to the platform.
//       Perhaps in the future we can fill these fields with real data
void EXIFFields::setUnknownFields()
{
    // time information
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime((char *)mExif.date_time, sizeof(mExif.date_time), "%Y:%m:%d %H:%M:%S", timeinfo);

    // conponents configuration. 0 means does not exist
    // 1 = Y; 2 = Cb; 3 = Cr; 4 = R; 5 = G; 6 = B; other = reserved
    memset(mExif.components_configuration, 0, sizeof(mExif.components_configuration));

    // subject distance,    0 means distance unknown; (~0) means infinity.
    mExif.subject_distance.num = EXIF_DEF_SUBJECT_DISTANCE_UNKNOWN;
    mExif.subject_distance.den = 1;

    // light source, 0 means light source unknown
    mExif.light_source = 0;

    // gain control, 0 = none;
    // 1 = low gain up; 2 = high gain up; 3 = low gain down; 4 = high gain down
    mExif.gain_control = 0;

    // sharpness, 0 = normal; 1 = soft; 2 = hard; other = reserved
    mExif.sharpness = 0;
}

}; // namespace android
