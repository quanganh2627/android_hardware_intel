#
# Copyright 2014 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

###########################
# convert static lib target

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_MODULE := libaudio_comms_signal_processing

include $(BUILD_STATIC_LIBRARY)

#########################
# convert static lib host

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_MODULE := libaudio_comms_signal_processing_host

LOCAL_CFLAGS = -O0 --coverage

LOCAL_LDFLAGS = --coverage

LOCAL_MODULE_TAGS := tests

include $(BUILD_HOST_STATIC_LIBRARY)

#########################
# host common test

include $(CLEAR_VARS)

LOCAL_MODULE := libaudio_comms_signal_processing_unit_test_host

LOCAL_SRC_FILES := test/SignalProcessingUnitTest.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_CFLAGS := -Wall -Werror -Wextra -ggdb3 -O0

LOCAL_STATIC_LIBRARIES := \
    libacresult_host \
    libaudio_comms_utilities_host

LOCAL_STRIP_MODULE := false

LOCAL_MODULE_TAGS := tests

include $(BUILD_HOST_NATIVE_TEST)

#########################
# target unit test

include $(CLEAR_VARS)

LOCAL_MODULE := libaudio_comms_signal_processing_unit_test

LOCAL_SRC_FILES := test/SignalProcessingUnitTest.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_CFLAGS := -Wall -Werror -Wextra -ggdb3 -O0

LOCAL_STRIP_MODULE := false

LOCAL_SHARED_LIBRARIES := libstlport

LOCAL_STATIC_LIBRARIES := \
    libacresult \
    libaudio_comms_utilities

include external/stlport/libstlport.mk
include $(BUILD_NATIVE_TEST)

