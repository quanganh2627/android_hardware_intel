/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ANDROID_LIBCAMERA_COLOR_CONVERTER_H
#define ANDROID_LIBCAMERA_COLOR_CONVERTER_H

#include <utils/Errors.h>

namespace android {

void YUV420ToRGB565(int width, int height, void *src, void *dst);

void trimConvertNV12ToRGB565(int width, int height, int srcBpl, void *src, void *dst);

void convertYV12ToNV21(int width, int height, int srcBpl, int dstBpl, void *src, void *dst);
void copyYV12ToYV12(int width, int height, int srcBpl, int dstBpl, void *src, void *dst);

void trimConvertNV12ToNV21(int width, int height, int srcBpl, void *src, void *dst);

void align16ConvertNV12ToYV12(int width, int height, int srcBpl, void *src, void *dst);

void NV12ToP411(int width, int height, void *src, void *dst);
void NV12ToP411Separate(int width, int height, void *srcY, void *srcUV, void *dst);

void YUY2ToP411(int width, int height, void *src, void *dst);

void convertYUYVToYV12(int width, int height, int srcBpl, int dstBpl, void *src, void *dst);

void convertYUYVToNV21(int width, int height, int srcBpl, void *src, void *dst);

void convertBuftoYV12(int fourcc, int width, int height, int srcBpl, int
                      dstBpl, void *src, void *dst);

void convertBuftoNV21(int fourcc, int width, int height, int srcBpl, int
                      dstBpl, void *src, void *dst);

void repadYUV420(int width, int height, int srcBpl, int dstBpl, void *src, void *dst);

const char *cameraParametersFormat(int v4l2Format);
int V4L2Format(const char *cameraParamsFormat);

}; // namespace android

#endif // ANDROID_LIBCAMERA_COLOR_CONVERTER_H
