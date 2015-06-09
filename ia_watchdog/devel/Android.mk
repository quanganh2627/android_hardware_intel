#
# Copyright 2009 - 2010 (c) Intel Corporation. All rights reserved.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifeq ($(BUILD_WITH_WATCHDOG_DAEMON_SUPPORT),true)

LOCAL_PATH := $(call my-dir)
WDOG_DEV_PATH := $(LOCAL_PATH)
WDOGD_PATH := $(LOCAL_PATH)/../watchdog_daemon

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += vendor/intel/hardware/include
LOCAL_C_INCLUDES += $(WDOGD_PATH)
LOCAL_CFLAGS += -g -Wall
LOCAL_LDFLAGS += -ldl
LOCAL_SRC_FILES:= watchdogd_devel.c
LOCAL_MODULE := libwatchdogd_devel
LOCAL_SHARED_LIBRARIES := libcutils libc
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

endif
