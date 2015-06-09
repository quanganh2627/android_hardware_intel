# Copyright 2014 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)
include $(OPTIONAL_QUALITY_ENV_SETUP)

# Common variables
##################

remote_param_common_src_files := \
    RemoteParameterConnector.cpp

remote_param_common_includes_dir_host := \
    bionic/libc/kernel/common

remote_param_common_includes_dir_target := \
    external/stlport/stlport \
    bionic

remote_param_common_shared_lib_target += \
    libstlport \
    libcutils

remote_param_common_static_lib += \
    libaudio_comms_utilities

remote_param_common_static_lib_host += \
    $(foreach lib, $(remote_param_common_static_lib), $(lib)_host)

remote_param_common_static_lib_target += \
    $(remote_param_common_static_lib)

remote_param_common_cflags := -Wall -Werror -Wno-unused-parameter

#######################################################################
# Compile macro

define make_remote_param_common_lib
$( \
    $(eval LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)) \
    $(eval LOCAL_C_INCLUDES += $(remote_param_common_includes_dir_$(1))) \
    $(eval LOCAL_STATIC_LIBRARIES := $(remote_param_common_static_lib_$(1))) \
    $(eval LOCAL_SHARED_LIBRARIES := $(remote_param_common_shared_lib_$(1))) \
    $(eval LOCAL_SRC_FILES := $(remote_param_common_src_files)) \
    $(eval LOCAL_CFLAGS += $(remote_param_common_cflags)) \
)
endef

# Build for target
##################################
include $(CLEAR_VARS)
LOCAL_MODULE := libremote-parameter-common
$(call make_remote_param_common_lib,target)
LOCAL_MODULE_TAGS := optional

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_STATIC_LIBRARY)

# Build for host
##################################
include $(CLEAR_VARS)
LOCAL_MODULE := libremote-parameter-common_host
$(call make_remote_param_common_lib,host)
LOCAL_MODULE_TAGS := tests

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)
include $(BUILD_HOST_STATIC_LIBRARY)

include $(OPTIONAL_QUALITY_ENV_TEARDOWN)
