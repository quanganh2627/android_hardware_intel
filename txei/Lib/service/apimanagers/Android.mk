LOCAL_PATH := $(call my-dir)

# Build the library
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := com.intel.security.service.sepmanager
LOCAL_SRC_FILES := $(call all-java-files-under, .)
LOCAL_SRC_FILES += com/intel/security/service/ISEPService.aidl
include $(BUILD_JAVA_LIBRARY)

# Copy com.intel.security.service.log.xml to /system/etc/permissions/
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := com.intel.security.service.sepmanager.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/permissions
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
