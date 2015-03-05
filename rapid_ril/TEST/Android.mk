#
# Copyright 2011 Intrinsyc Software International, Inc.  All rights reserved.
#

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_JAVA_LIBRARIES := telephony-common
LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := RILTest
LOCAL_CERTIFICATE := platform

include $(BUILD_PACKAGE)

# Use the following include to make our test apk.
include $(call all-makefiles-under,$(LOCAL_PATH))
