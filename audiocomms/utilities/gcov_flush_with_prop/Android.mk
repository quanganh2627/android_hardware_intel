#
# Copyright 2013-2014 Intel Corporation
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

LOCAL_PATH := $(call my-dir)

#######################################################################
# Build for target with flush from property
include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_SRC_FILES := GcovFlushWithProp.cpp
LOCAL_C_INCLUDES := \
    bionic

LOCAL_MODULE := gcov_flush_with_prop
LOCAL_MODULE_TAGS := tests

# library included for its header
LOCAL_STATIC_LIBRARIES := libproperty_includes

include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

#######################################################################
# Build for target

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_SRC_FILES := GcovFlush.cpp
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    bionic

LOCAL_MODULE := gcov_flush
LOCAL_MODULE_TAGS := optional

include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

#######################################################################
# Build for host

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_SRC_FILES := GcovFlush.cpp
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    bionic/libc/kernel/common

LOCAL_MODULE := gcov_flush
LOCAL_MODULE_TAGS := tests


include $(BUILD_HOST_STATIC_LIBRARY)


