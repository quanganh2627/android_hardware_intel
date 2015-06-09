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

#######################################################################
# Common variables

stream_lib_src_files :=  \
    IoStream.cpp \
    TinyAlsaAudioDevice.cpp \
    StreamLib.cpp \
    TinyAlsaIoStream.cpp

stream_lib_includes_common := \
    $(LOCAL_PATH)/include

stream_lib_includes_dir := \
    $(TARGET_OUT_HEADERS)/hw \
    $(TARGET_OUT_HEADERS)/parameter \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils)

stream_lib_includes_dir_host := \
    $(stream_lib_includes_dir)

stream_lib_includes_dir_target := \
    $(stream_lib_includes_dir) \
    $(call include-path-for, bionic)

stream_lib_header_copy_folder_unit_test := \
    stream_lib_unit_test

stream_lib_static_lib += \
    libsamplespec_static \
    libaudio_comms_utilities \
    audio.routemanager.includes

stream_lib_static_lib_host += \
    $(foreach lib, $(stream_lib_static_lib), $(lib)_host)

stream_lib_static_lib_target += \
    $(stream_lib_static_lib) \
    libmedia_helper

stream_lib_include_dirs_from_static_libraries := \
    libproperty_static

stream_lib_include_dirs_from_static_libraries_target := \
    $(stream_lib_include_dirs_from_static_libraries)

stream_lib_include_dirs_from_static_libraries_host := \
    $(foreach lib, $(stream_lib_include_dirs_from_static_libraries), $(lib)_host)

stream_lib_cflags := -Wall -Werror -Wno-unused-parameter


#######################################################################
# Build for test with and without gcov for host and target

# Compile macro
define make_stream_lib_test_lib
$( \
    $(eval LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include) \
    $(eval LOCAL_C_INCLUDES := $(stream_lib_includes_common)) \
    $(eval LOCAL_C_INCLUDES += $(stream_lib_includes_dir_$(1))) \
    $(eval LOCAL_STATIC_LIBRARIES := $(stream_lib_static_lib_$(1)) \
        $(stream_lib_include_dirs_from_static_libraries_$(1)) \
        audio.routemanager.includes )
    $(eval LOCAL_SRC_FILES := $(stream_lib_src_files)) \
    $(eval LOCAL_CFLAGS := $(stream_lib_cflags)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
)
endef

define add_gcov
$( \
    $(eval LOCAL_CFLAGS += -O0 -fprofile-arcs -ftest-coverage) \
    $(eval LOCAL_LDFLAGS += -lgcov) \
)
endef

# Build for host test with gcov
ifeq ($(audiocomms_test_gcov_host),true)

include $(CLEAR_VARS)
$(call make_stream_lib_test_lib,host)
$(call add_gcov)
LOCAL_MODULE := libstream_static_gcov_host
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target test with gcov
ifeq ($(audiocomms_test_gcov_target),true)

include $(CLEAR_VARS)
LOCAL_MODULE := libstream_static_gcov
$(call make_stream_lib_test_lib,target)
$(call add_gcov)
include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

endif

# Build for host test
ifeq ($(audiocomms_test_host),true)

include $(CLEAR_VARS)
$(call make_stream_lib_test_lib,host)
LOCAL_MODULE := libstream_static_host
include $(BUILD_HOST_STATIC_LIBRARY)

endif

# Build for target (Inconditionnal)

include $(CLEAR_VARS)
LOCAL_MODULE := libstream_static
$(call make_stream_lib_test_lib,target)
include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)
