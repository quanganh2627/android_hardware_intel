# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifeq ($(ENABLE_SCALABLE_SENSOR_HAL),true)

LOCAL_PATH := $(call my-dir)

ifneq ($(TARGET_SIMULATOR),true)

# HAL module implemenation, not prelinked, and stored in
# hw/<SENSORS_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

LOCAL_MODULE := sensors.$(TARGET_DEVICE)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SRC_FILES := SensorHAL.cpp    \
                   DirectSensor.cpp \
                   PlatformConfig.cpp \
                   PSHSensor.cpp \
                   Sensor.cpp \
                   SensorDevice.cpp \
                   InputEventSensor.cpp \
                   MiscSensor.cpp \
                   PSHCommonSensor.cpp \
                   SensorHubHelper.cpp \
                   PedometerSensor.cpp \
                   PhysicalActivitySensor.cpp \
                   GestureSensor.cpp \
                   utils.cpp \
                   AudioClassifierSensor.cpp

LOCAL_C_INCLUDES := $(COMMON_INCLUDES) \
                    $(call include-path-for, stlport) \
                    $(call include-path-for, stlport)/stl \
                    $(call include-path-for, stlport)/using/h/ \
                    $(call include-path-for, icu4c-common) \
                    $(call include-path-for, libxml2) \
                    $(call include-path-for, libsensorhub) \
                    $(TARGET_OUT_HEADERS)/awarelibs

LOCAL_SHARED_LIBRARIES := liblog libcutils libdl libicuuc libstlport libhardware libutils
LOCAL_STATIC_LIBRARIES := libxml2

include external/stlport/libstlport.mk

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := libsensorcalibration
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DLOG_TAG=\"SensorCalibration\"

LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_STATIC_LIBRARIES := libcompassgenericcalibration

LOCAL_SRC_FILES := sensorcalibration/SensorCalibration.c

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := libcompassgenericcalibration
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DLOG_TAG=\"CompassGenericCalibration\"

LOCAL_SHARED_LIBRARIES := liblog libcutils

LOCAL_SRC_FILES := sensorcalibration/CompassGenericCalibration/CompassGenericCalibration.cpp

include $(BUILD_STATIC_LIBRARY)

endif # !TARGET_SIMULATOR

endif
