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

#######################################################################
# target & host common part

common_test_src_files := \
    test/ResultUnitTest.cpp \
    test/ErrnoResultUnitTest.cpp \

base_name := acresult
common_library_local_module := lib$(base_name)

common_c_includes := \
    $(LOCAL_PATH)/include \

common_c_flags := \
    -Wall \
    -Werror \
    -Wextra

#######################################################################
# target result common library

include $(CLEAR_VARS)
LOCAL_MODULE    := $(common_library_local_module)

LOCAL_C_INCLUDES := \
    $(call include-path-for, stlport) \
    bionic \

LOCAL_CFLAGS := $(common_c_flags)

LOCAL_SHARED_LIBRARIES := libstlport

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

include $(BUILD_STATIC_LIBRARY)

#======================================================================
# host result common library

include $(CLEAR_VARS)
LOCAL_MODULE    := $(common_library_local_module)_host

LOCAL_CFLAGS := $(common_c_flags)

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

include $(BUILD_HOST_STATIC_LIBRARY)

#######################################################################
common_c_flags := $(common_c_flags) -ggdb3 -O0
common_ut_local_module    := $(base_name)UnitTest
#---------------------------------------------------------------------
# host result common test

include $(CLEAR_VARS)

LOCAL_MODULE := $(common_ut_local_module)_host

LOCAL_SRC_FILES := $(common_test_src_files)
LOCAL_C_INCLUDES := $(common_c_includes)

LOCAL_CFLAGS := $(common_c_flags)

LOCAL_LDFLAGS :=

LOCAL_STRIP_MODULE := false

include $(BUILD_HOST_NATIVE_TEST)

#======================================================================
# target unit test

include $(CLEAR_VARS)

LOCAL_MODULE := $(common_ut_local_module)

LOCAL_SRC_FILES := $(common_test_src_files)

LOCAL_C_INCLUDES := \
    $(call include-path-for, stlport) \
    bionic \
    $(common_c_includes)

LOCAL_CFLAGS := $(common_c_flags)

LOCAL_LDFLAGS :=

LOCAL_STRIP_MODULE := false

LOCAL_SHARED_LIBRARIES := libstlport

include $(BUILD_NATIVE_TEST)

