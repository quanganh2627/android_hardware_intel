# INTEL CONFIDENTIAL
#
# Copyright 2013 Intel Corporation All Rights Reserved.
# The source code contained or described herein and all documents
# related to the source code ("Material") are owned by Intel
# Corporation or its suppliers or licensors. Title to the Material
# remains with Intel Corporation or its suppliers and licensors. The
# Material contains trade secrets and proprietary and confidential
# information of Intel or its suppliers and licensors. The Material is
# protected by worldwide copyright and trade secret laws and treaty
# provisions. No part of the Material may be used, copied, reproduced,
# modified, published, uploaded, posted, transmitted, distributed, or
# disclosed in any way without Intel's prior express written permission.
#
# No license under any patent, copyright, trade secret or other
# intellectual property right is granted to or conferred upon you by
# disclosure or delivery of the Materials, either expressly, by
# implication, inducement, estoppel or otherwise. Any license under
# such intellectual property rights must be express and approved by
# Intel in writing.

LOCAL_PATH := $(call my-dir)

naive_tokenizer_src_files := NaiveTokenizer.cpp
naive_tokenizer_cflags := \
    -Wall \
    -Werror \
    -Wextra \

# target
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(naive_tokenizer_src_files)
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(naive_tokenizer_cflags)
LOCAL_C_INCLUDES := $(call include-path-for, stlport)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_MODULE := libaudiocomms_naive_tokenizer
include $(BUILD_STATIC_LIBRARY)

# host
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(naive_tokenizer_src_files)
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(naive_tokenizer_cflags)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_MODULE := libaudiocomms_naive_tokenizer
include $(BUILD_HOST_STATIC_LIBRARY)
