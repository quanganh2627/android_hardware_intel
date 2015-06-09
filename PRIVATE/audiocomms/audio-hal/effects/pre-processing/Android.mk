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

effect_pre_proc_src_files :=  \
    src/LpeNs.cpp \
    src/LpeAgc.cpp \
    src/LpeAec.cpp \
    src/LpeBmf.cpp \
    src/LpeWnr.cpp \
    src/AudioEffect.cpp \
    src/AudioEffectSession.cpp \
    src/LpeEffectLibrary.cpp \
    src/LpePreProcessing.cpp

effect_pre_proc_includes_dir := \

effect_pre_proc_includes_common := \
    $(LOCAL_PATH)/include \
    $(call include-path-for, audio-effects)

effect_pre_proc_includes_dir_host := \
    $(foreach inc, $(effect_pre_proc_includes_dir), $(HOST_OUT_HEADERS)/$(inc)) \
    $(call include-path-for, libc-kernel)

effect_pre_proc_includes_dir_target := \
    $(foreach inc, $(effect_pre_proc_includes_dir), $(TARGET_OUT_HEADERS)/$(inc))

effect_pre_proc_static_lib += \
    libaudio_comms_utilities \
    libaudio_comms_convert \

effect_pre_proc_static_lib_host += \
    $(foreach lib, $(effect_pre_proc_static_lib), $(lib)_host) \
    libaudioresample_static_host

effect_pre_proc_static_lib_target += \
    $(effect_pre_proc_static_lib) \
    libmedia_helper \

effect_pre_proc_shared_lib_target += \
    libutils  \
    libcutils \
    libmedia

effect_pre_proc_cflags := -Wall -Werror -Wno-unused-parameter

#######################################################################
# Build for target

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(effect_pre_proc_includes_dir_target) \
    $(effect_pre_proc_includes_common)

LOCAL_SRC_FILES := $(effect_pre_proc_src_files)
LOCAL_CFLAGS := $(effect_pre_proc_cflags)

LOCAL_MODULE := liblpepreprocessing
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := \
    $(effect_pre_proc_static_lib_target)
LOCAL_SHARED_LIBRARIES := \
    $(effect_pre_proc_shared_lib_target)
LOCAL_MODULE_RELATIVE_PATH := soundfx

include external/stlport/libstlport.mk

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_SHARED_LIBRARY)

#######################################################################
# Build for host

include $(CLEAR_VARS)
LOCAL_MODULE := liblpepreprocessing_static_gcov_host
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := $(effect_pre_proc_includes_common)
LOCAL_C_INCLUDES += $(effect_pre_proc_includes_dir_host)
LOCAL_SRC_FILES := $(effect_pre_proc_src_files)
LOCAL_CFLAGS := $(effect_pre_proc_cflags)
LOCAL_STATIC_LIBRARIES := $(effect_pre_proc_static_lib_host)
LOCAL_MODULE_TAGS := tests
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)


# Helper Effect Lib
#######################################################################
# Build for target

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_CFLAGS := $(effect_pre_proc_cflags)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SRC_FILES := src/EffectHelper.cpp
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := liblpepreprocessinghelper

include external/stlport/libstlport.mk
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_STATIC_LIBRARY)

#######################################################################
# Build for host

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := $(effect_pre_proc_cflags)
LOCAL_SRC_FILES := src/EffectHelper.cpp
LOCAL_MODULE_TAGS := tests

LOCAL_MODULE := liblpepreprocessinghelper_host
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)

# Component functional test
#######################################################################

audio_effects_functional_test_static_lib += \
    libgmock_main \
    libgmock \
    libaudio_comms_utilities \

audio_effects_functional_test_src_files := \
    test/AudioEffectsFcct.cpp

audio_effects_functional_test_c_includes := \
    $(call include-path-for, audio-effects) \
    $(LOCAL_PATH)/../../../../external/gmock/include

audio_effects_functional_test_static_lib_target := \
    $(audio_effects_functional_test_static_lib)

audio_effects_functional_test_shared_lib_target := \
    libcutils \
    libbinder \
    libmedia \
    libutils

audio_effects_functional_test_defines += -Wall -Werror -ggdb -O0

###############################
# Functional test target

include $(CLEAR_VARS)

LOCAL_MODULE := audio_effects_functional_test
LOCAL_SRC_FILES := $(audio_effects_functional_test_src_files)
LOCAL_C_INCLUDES := $(audio_effects_functional_test_c_includes)
LOCAL_CFLAGS := $(audio_effects_functional_test_defines)
LOCAL_STATIC_LIBRARIES := $(audio_effects_functional_test_static_lib_target)
LOCAL_SHARED_LIBRARIES := $(audio_effects_functional_test_shared_lib_target)
LOCAL_MODULE_TAGS := tests
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_NATIVE_TEST)

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
