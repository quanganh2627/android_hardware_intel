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

ifeq ($(BOARD_USES_AUDIO_HAL_XML),true)

LOCAL_PATH := $(call my-dir)
include $(OPTIONAL_QUALITY_ENV_SETUP)

# Component build
#######################################################################
# Common variables

audio_stream_manager_src_files :=  \
    src/Stream.cpp \
    src/Device.cpp \
    src/StreamIn.cpp \
    src/StreamOut.cpp \
    src/AudioParameterHandler.cpp

audio_stream_manager_includes_dir := \
    $(TARGET_OUT_HEADERS)/libaudioresample \
    $(TARGET_OUT_HEADERS)/hal_audio_dump \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils) \
    $(call include-path-for, audio-effects)

audio_stream_manager_includes_dir_host := \
    $(audio_stream_manager_includes_dir)

audio_stream_manager_includes_dir_target := \
    $(audio_stream_manager_includes_dir) \
    $(call include-path-for, bionic)

audio_stream_manager_static_lib += \
    libsamplespec_static \
    libaudioconversion_static \
    libstream_static \
    libaudioplatformstate \
    libactive_value_set \
    libkeyvaluepairs \
    libparametermgr_static \
    libaudio_comms_utilities \
    libaudio_comms_convert \
    libhalaudiodump \
    liblpepreprocessinghelper \
    libaudiocomms_naive_tokenizer

audio_stream_manager_static_lib_host += \
    $(foreach lib, $(audio_stream_manager_static_lib), $(lib)_host)

audio_stream_manager_static_lib_target += \
    $(audio_stream_manager_static_lib) \
    libmedia_helper \
    audio.routemanager.includes

audio_stream_manager_shared_lib_target += \
    libtinyalsa \
    libcutils \
    libutils \
    libmedia \
    libhardware \
    libhardware_legacy \
    libparameter \
    libicuuc \
    libevent-listener \
    libaudioresample \
    libaudioutils \
    libproperty \
    libinterface-provider-lib \
    libmodem-audio-collection \

audio_stream_manager_include_dirs_from_static_libraries := \
    libevent-listener_static \
    libinterface-provider-lib_static \
    libproperty_static

audio_stream_manager_include_dirs_from_static_libraries_target := \
    $(audio_stream_manager_include_dirs_from_static_libraries)

audio_stream_manager_whole_static_lib := \
    libaudiohw_intel

audio_stream_manager_include_dirs_from_static_libraries_host := \
    $(foreach lib, $(audio_stream_manager_include_dirs_from_static_libraries), $(lib)_host)

audio_stream_manager_cflags := -Wall -Werror -Wextra

#######################################################################
# Phony package definition

include $(CLEAR_VARS)
LOCAL_MODULE := audio_hal_configurable
LOCAL_MODULE_TAGS := optional

LOCAL_REQUIRED_MODULES := \
    audio.primary.$(TARGET_DEVICE) \
    audio.routemanager \
    liblpepreprocessing \
    route_criteria.conf

include $(BUILD_PHONY_PACKAGE)

#######################################################################
# Build for target

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(audio_stream_manager_includes_dir_target)

LOCAL_SRC_FILES := $(audio_stream_manager_src_files)
LOCAL_CFLAGS := $(audio_stream_manager_cflags)

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

LOCAL_MODULE := audio.primary.$(TARGET_DEVICE)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := \
    $(audio_stream_manager_static_lib_target)
LOCAL_WHOLE_STATIC_LIBRARIES := \
    $(audio_stream_manager_whole_static_lib)
LOCAL_SHARED_LIBRARIES := \
    $(audio_stream_manager_shared_lib_target)

include external/stlport/libstlport.mk

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_SHARED_LIBRARY)

#######################################################################
# Build for host

ifeq ($(audiocomms_test_host),true)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(audio_stream_manager_includes_dir_host)
LOCAL_STATIC_LIBRARIES := $(audio_stream_manager_static_lib_host)
# libraries included for their headers
LOCAL_STATIC_LIBRARIES += \
    $(audio_stream_manager_include_dirs_from_static_libraries_host)
LOCAL_WHOLE_STATIC_LIBRARIES := $(audio_stream_manager_whole_static_lib)
LOCAL_SRC_FILES := $(audio_stream_manager_src_files)
LOCAL_CFLAGS := $(audio_stream_manager_cflags)
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := libaudio_stream_manager_static_host
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Component functional test
#######################################################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= test/FunctionalTest.cpp

LOCAL_C_INCLUDES := \
        $(TARGET_OUT_HEADERS)/hw \
        $(TARGET_OUT_HEADERS)/parameter \
        frameworks/av/include \
        system/media/audio_utils/include \
        system/media/audio_effects/include \

LOCAL_STATIC_LIBRARIES := \
        libkeyvaluepairs \
        libaudio_comms_utilities \
        libaudio_comms_convert \
        libmedia_helper \

LOCAL_SHARED_LIBRARIES := \
        libhardware \
        liblog \

LOCAL_MODULE := audio-hal-functional_test
LOCAL_MODULE_TAGS := tests

LOCAL_CFLAGS := -Wall -Werror -Wextra

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_NATIVE_TEST)


include $(OPTIONAL_QUALITY_ENV_TEARDOWN)

endif #ifeq ($(BOARD_USES_AUDIO_HAL_XML),true)
