################################################################################
#
# Copyright 2013-2014 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################


LOCAL_PATH := $(call my-dir)

#-----------------------------------------------------------------------
# target & host common part

common_library_src_files := \

common_library_test_src_files := \
    test/ConvertToStringUnitTest.cpp \
    test/HelpersUnitTest.cpp \

common_base_name := acserializer
common_library_local_module := lib$(common_base_name)

common_static_libs := \
    libacresult \

common_static_libs_host := \
    libacresult_host \

common_shared_libs := \
    libstlport

common_shared_libs_host :=

common_library_c_includes := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/src \

common_header_lib := \
    libaudio_comms_convert \
    libaudio_comms_utilities

common_header_lib_host := \
    libaudio_comms_convert_host \
    libaudio_comms_utilities_host

common_c_flags := \
    -Wall \
    -Werror \
    -Wextra

#######################################################################
# target cme serializer library

include $(CLEAR_VARS)
LOCAL_MODULE    := $(common_library_local_module)
LOCAL_SRC_FILES := $(common_library_src_files)

LOCAL_C_INCLUDES := \
    bionic \
    $(common_library_c_includes) \

LOCAL_CFLAGS := $(common_c_flags)

LOCAL_STATIC_LIBRARIES := $(common_static_libs)

# libraries included for their headers
LOCAL_STATIC_LIBRARIES += $(common_header_lib)

LOCAL_SHARED_LIBRARIES := $(common_shared_libs)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

include external/stlport/libstlport.mk
include $(BUILD_STATIC_LIBRARY)

#======================================================================
# host cme serializer library

include $(CLEAR_VARS)
LOCAL_MODULE    := $(common_library_local_module)_host
LOCAL_SRC_FILES := $(common_library_src_files)

LOCAL_C_INCLUDES := $(common_library_c_includes)

LOCAL_CFLAGS := $(common_c_flags)

LOCAL_STATIC_LIBRARIES := $(common_static_libs_host)

# libraries included for their headers
LOCAL_STATIC_LIBRARIES += $(common_header_lib_host)

LOCAL_SHARED_LIBRARIES := $(common_shared_libs_host)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

include $(BUILD_HOST_STATIC_LIBRARY)

#######################################################################
#Unit test
common_c_flags := $(common_c_flags) -O0 --coverage

#=====================================================================
# target cme api test

include $(CLEAR_VARS)

LOCAL_MODULE := $(common_base_name)_unit_test

LOCAL_SRC_FILES := \
    $(common_library_test_src_files)

LOCAL_C_INCLUDES := $(common_library_c_includes)

LOCAL_CFLAGS := $(common_c_flags)

LOCAL_LDFLAGS := --coverage

LOCAL_STRIP_MODULE := false

LOCAL_STATIC_LIBRARIES := \
    $(common_library_local_module) \
    $(common_static_libs)

# libraries included for their headers
LOCAL_STATIC_LIBRARIES += $(common_header_lib)

LOCAL_SHARED_LIBRARIES := \
    $(common_shared_libs) \


include $(BUILD_NATIVE_TEST)

#=====================================================================
# host cme api test

include $(CLEAR_VARS)

LOCAL_MODULE := $(common_base_name)_unit_test_host

LOCAL_SRC_FILES := \
    $(common_library_test_src_files)

LOCAL_C_INCLUDES := $(common_library_c_includes)

LOCAL_CFLAGS := $(common_c_flags)

LOCAL_LDFLAGS := --coverage

LOCAL_STRIP_MODULE := false

LOCAL_STATIC_LIBRARIES := \
    $(common_library_local_module)_host \
    $(common_static_libs_host)

# libraries included for their headers
LOCAL_STATIC_LIBRARIES += $(common_header_lib_host)

LOCAL_SHARED_LIBRARIES := \
    $(common_shared_libs_host) \


include $(BUILD_HOST_NATIVE_TEST)

