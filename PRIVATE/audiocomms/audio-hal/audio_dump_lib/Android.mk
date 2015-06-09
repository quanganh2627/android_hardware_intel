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

# Common variables
##################

hal_dump_src_files := HalAudioDump.cpp

hal_dump_cflags := -DDEBUG -Wall -Werror

hal_dump_includes_common := \
    $(LOCAL_PATH)/include

hal_dump_includes_dir_host := \
    $(call include-path-for, libc-kernel)

hal_dump_includes_dir_target := \
    $(call include-path-for, bionic)

#######################################################################
# Build for libhalaudiodump for host and target

# Compile macro
define make_hal_dump_lib
$( \
    $(eval LOCAL_EXPORT_C_INCLUDE_DIRS := $(hal_dump_includes_common)) \
    $(eval LOCAL_C_INCLUDES := $(hal_dump_includes_common)) \
    $(eval LOCAL_C_INCLUDES += $(hal_dump_includes_dir_$(1))) \
    $(eval LOCAL_SRC_FILES := $(hal_dump_src_files)) \
    $(eval LOCAL_CFLAGS := $(hal_dump_cflags)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
)
endef

# Build for target
##################
include $(CLEAR_VARS)
LOCAL_MODULE := libhalaudiodump
$(call make_hal_dump_lib,target)
LOCAL_STATIC_LIBRARIES := libaudio_comms_utilities
include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)


# Build for host
################
include $(CLEAR_VARS)
LOCAL_MODULE := libhalaudiodump_host
$(call make_hal_dump_lib,host)
LOCAL_STATIC_LIBRARIES := libaudio_comms_utilities_host
include $(BUILD_HOST_STATIC_LIBRARY)
