# INTEL CONFIDENTIAL
#
# Copyright (c) 2014 Intel Corporation All Rights Reserved.
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

LOCAL_PATH := $(call my-dir)
include $(OPTIONAL_QUALITY_ENV_SETUP)

# Target build
#######################################################################

include $(CLEAR_VARS)

LOCAL_STATIC_LIBRARIES := \
    libaudio_comms_utilities \
    libaudio_comms_convert \

LOCAL_SRC_FILES := src/KeyValuePairs.cpp \

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wall -Werror -Wextra

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libkeyvaluepairs

include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

# Host build
#######################################################################

include $(CLEAR_VARS)

LOCAL_STATIC_LIBRARIES := \
    libaudio_comms_utilities_host \
    libaudio_comms_convert_host \

LOCAL_SRC_FILES := src/KeyValuePairs.cpp \

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wall -Werror -Wextra
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := libkeyvaluepairs_host
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)

# Functional test
#######################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES += test/KeyValuePairsTest.cpp \

LOCAL_C_INCLUDES := \

LOCAL_STATIC_LIBRARIES += \
    libkeyvaluepairs_host \
    libaudio_comms_utilities_host \
    libaudio_comms_convert_host \

LOCAL_CFLAGS := -Wall -Werror -Wextra

LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := key_value_pairs_test
include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_NATIVE_TEST)


include $(OPTIONAL_QUALITY_RUN_TEST)

#######################################################################

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
