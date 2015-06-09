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

LOCAL_PATH := $(call my-dir)

# Build for target
##################

include $(CLEAR_VARS)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_SRC_FILES := EventThread.cpp
LOCAL_CFLAGS := -Wall -Werror -Wextra
LOCAL_SHARED_LIBRARIES := libstlport libcutils
LOCAL_STATIC_LIBRARIES := libaudio_comms_utilities

LOCAL_MODULE := libevent-listener
LOCAL_MODULE_TAGS := optional

include external/stlport/libstlport.mk

include $(BUILD_SHARED_LIBRARY)

# Build static lib
###########################

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_SRC_FILES := EventThread.cpp
LOCAL_STATIC_LIBRARIES := libaudio_comms_utilities

LOCAL_MODULE := libevent-listener_static
LOCAL_MODULE_TAGS := optional

include external/stlport/libstlport.mk

include $(BUILD_STATIC_LIBRARY)

# Build for host test
#####################

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
LOCAL_SRC_FILES := EventThread.cpp
LOCAL_STATIC_LIBRARIES := libaudio_comms_utilities_host

LOCAL_MODULE := libevent-listener_static_host
LOCAL_MODULE_TAGS := tests

include $(BUILD_HOST_STATIC_LIBRARY)


