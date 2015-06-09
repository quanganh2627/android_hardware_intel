# INTEL CONFIDENTIAL
#
# Copyright (c) 2013-2014 Intel Corporation All Rights Reserved.
#
# The source code contained or described herein and all documents related to
# the source code ("Material") are owned by Intel Corporation or its suppliers
# or licensors.
#
# Title to the Material remains with Intel Corporation or its suppliers and
# licensors. The Material contains trade secrets and proprietary and
# confidential information of Intel or its suppliers and licensors. The
# Material is protected by worldwide copyright and trade secret laws and treaty
# provisions. No part of the Material may be used, copied, reproduced,
# modified, published, uploaded, posted, transmitted, distributed, or disclosed
# in any way without Intel's prior express written permission.
#
# No license under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or delivery
# of the Materials, either expressly, by implication, inducement, estoppel or
# otherwise. Any license under such intellectual property rights must be
# express and approved by Intel in writing.

ifeq ($(BOARD_USES_AUDIO_HAL_XML),true)

LOCAL_PATH := $(call my-dir)
include $(OPTIONAL_QUALITY_ENV_SETUP)

# Target build
#######################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := libaudioplatformstate
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    external/tinyalsa/include \
    frameworks/av/include/media \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \
    $(TARGET_OUT_HEADERS)/mamgr-interface \
    $(TARGET_OUT_HEADERS)/mamgr-core \

LOCAL_SRC_FILES := \
    src/Parameter.cpp \
    src/CriterionParameter.cpp \
    src/AudioPlatformState.cpp \
    src/VolumeKeys.cpp \
    src/ModemProxy.cpp \
    src/ParameterAdapter.cpp \

LOCAL_STATIC_LIBRARIES := \
    libsamplespec_static \
    libstream_static \
    libparametermgr_static \
    libaudio_comms_utilities \
    libaudio_comms_convert \
    liblpepreprocessinghelper \
    libaudiocomms_naive_tokenizer \
    libinterface-provider-lib_static \
    libproperty_static \
    audio.routemanager.includes \
    libmedia_helper \
    libactive_value_set \
    libkeyvaluepairs \
    libevent-listener_static \

LOCAL_CFLAGS := -Wall -Werror -Wextra -Wno-unused-parameter
LOCAL_MODULE_TAGS := optional
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

# Build for configuration file
#######################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := route_criteria.conf
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_SRC_FILES := config/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

#######################################################################

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)

endif #ifeq ($(BOARD_USES_AUDIO_HAL_XML),true)
