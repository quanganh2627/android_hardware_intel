#
# INTEL CONFIDENTIAL
# Copyright (c) 2013-2014 Intel
# Corporation All Rights Reserved.
#
# The source code contained or described herein and all documents related to
# the source code ("Material") are owned by Intel Corporation or its suppliers
# or licensors. Title to the Material remains with Intel Corporation or its
# suppliers and licensors. The Material contains trade secrets and proprietary
# and confidential information of Intel or its suppliers and licensors. The
# Material is protected by worldwide copyright and trade secret laws and
# treaty provisions. No part of the Material may be used, copied, reproduced,
# modified, published, uploaded, posted, transmitted, distributed, or
# disclosed in any way without Intel's prior express written permission.
#
# No license under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or delivery
# of the Materials, either expressly, by implication, inducement, estoppel or
# otherwise. Any license under such intellectual property rights must be
# express and approved by Intel in writing.
#

LOCAL_PATH := $(call my-dir)
include $(OPTIONAL_QUALITY_ENV_SETUP)

# Component build
#######################################################################
# Common variables

audio_route_manager_src_files :=  \
    RoutingElement.cpp \
    AudioPort.cpp \
    AudioPortGroup.cpp \
    AudioRoute.cpp \
    AudioStreamRoute.cpp \
    AudioRouteManager.cpp \
    AudioRouteManagerObserver.cpp \
    RouteManagerInstance.cpp \
    EntryPoint.cpp

audio_route_manager_includes_common := \
    $(LOCAL_PATH)/includes \
    $(LOCAL_PATH)/interface

audio_route_manager_includes_dir := \
    $(TARGET_OUT_HEADERS)/libaudioresample \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils)

audio_route_manager_includes_dir_host := \
    $(audio_route_manager_includes_dir)

audio_route_manager_includes_dir_target := \
    $(audio_route_manager_includes_dir) \
    $(call include-path-for, bionic)

audio_route_manager_header_copy_folder_unit_test := \
    audio_route_manager_unit_test

audio_route_manager_static_lib += \
    libstream_static \
    libsamplespec_static \
    libparametermgr_static \
    libaudio_comms_utilities \
    liblpepreprocessinghelper

audio_route_manager_static_lib_host += \
    $(foreach lib, $(audio_route_manager_static_lib), $(lib)_host)

audio_route_manager_static_lib_target += \
    $(audio_route_manager_static_lib) \
    libmedia_helper

audio_route_manager_shared_lib_target += \
    libtinyalsa \
    libcutils \
    libutils \
    libparameter \
    libhardware_legacy \
    libicuuc \
    libevent-listener \
    libaudioutils \
    libproperty \
    libinterface-provider \
    libinterface-provider-lib

audio_route_manager_include_dirs_from_static_libraries := \
    libevent-listener_static \
    libinterface-provider-lib_static \
    libproperty_static

audio_route_manager_include_dirs_from_static_libraries_target := \
    $(audio_route_manager_include_dirs_from_static_libraries)

audio_route_manager_include_dirs_from_static_libraries_host := \
    $(foreach lib, $(audio_route_manager_include_dirs_from_static_libraries), $(lib)_host)

audio_route_manager_cflags := -Wall -Werror

#######################################################################
# Build for target

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(audio_route_manager_includes_common)

LOCAL_C_INCLUDES := \
    $(audio_route_manager_includes_common) \
    $(audio_route_manager_includes_dir_target)

LOCAL_SRC_FILES := $(audio_route_manager_src_files)
LOCAL_CFLAGS := $(audio_route_manager_cflags)

LOCAL_MODULE := audio.routemanager
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := \
    $(audio_route_manager_static_lib_target)

LOCAL_SHARED_LIBRARIES := \
    $(audio_route_manager_shared_lib_target)

include external/stlport/libstlport.mk

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_SHARED_LIBRARY)

#######################################################################
# Build for host

ifeq ($(audiocomms_test_gcov_host),true)

include $(CLEAR_VARS)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include)
LOCAL_C_INCLUDES := $(audio_route_manager_includes_common)
LOCAL_C_INCLUDES += $(audio_route_manager_includes_dir_host)
LOCAL_STATIC_LIBRARIES := $(audio_route_manager_static_lib_host)
# libraries included for their headers
LOCAL_STATIC_LIBRARIES += \
    $(audio_route_manager_include_dirs_from_static_libraries_host)
LOCAL_SRC_FILES := $(audio_route_manager_src_files)
LOCAL_CFLAGS := $(audio_route_manager_cflags)
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := libaudio_route_manager_static_gcov_host

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)

endif


#######################################################################
# Build for target to export headers

include $(CLEAR_VARS)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(audio_route_manager_includes_common)
LOCAL_MODULE := audio.routemanager.includes
include $(BUILD_STATIC_LIBRARY)


include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
