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

define named-subdir-makefiles
$(wildcard $(addsuffix /Android.mk, $(addprefix $(LOCAL_PATH)/,$(1))))
endef

SUBDIRS := audio_conversion \
           audio_dump_lib \
           audio_policy \
           audio_route_manager \
           audio_stream_manager \
           effects \
           parameter_mgr_helper \
           sample_specifications \
           stream_lib \
           audio_platform_state \
           hardware_device \
           utilities/active_value_set \
           utilities/parameter \

# Call sub-folders' Android.mk
include $(call named-subdir-makefiles, $(SUBDIRS))

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
endif #ifeq ($(BOARD_USES_AUDIO_HAL_XML),true)

