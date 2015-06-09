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

ifeq ($(USE_LEGACY_AUDIO_POLICY), 1)

# uncomment this variable to execute audio policy test
#AUDIO_POLICY_TEST := true

LOCAL_PATH := $(call my-dir)

# This is the ALSA audio policy manager

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    AudioPolicyManagerALSA.cpp

ifeq ($(AUDIO_POLICY_TEST),true)
  LOCAL_CFLAGS += -DAUDIO_POLICY_TEST
endif

LOCAL_C_INCLUDES += \
    $(call include-path-for, bionic)

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libproperty

LOCAL_STATIC_LIBRARIES := \
    libmedia_helper \
    libaudio_comms_utilities

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libaudiopolicy_legacy

LOCAL_MODULE := audio_policy.$(TARGET_DEVICE)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

include external/stlport/libstlport.mk

include $(BUILD_SHARED_LIBRARY)

endif # ifeq ($(USE_LEGACY_AUDIO_POLICY), 1)
