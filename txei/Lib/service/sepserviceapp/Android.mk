LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PACKAGE_NAME := SepService
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(call all-java-files-under,src)
LOCAL_REQUIRED_MODULES := \
    com.intel.security.service.sepmanager \
    com.intel.security.lib.sepdrmjni
LOCAL_JAVA_LIBRARIES := \
    com.intel.security.service.sepmanager \
    com.intel.security.lib.sepdrmjni \
    core-libart \
    framework
LOCAL_SDK_VERSION := current
LOCAL_PROGUARD_ENABLED := disabled
LOCAL_CERTIFICATE := platform
include $(BUILD_PACKAGE)
