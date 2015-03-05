LOCAL_PATH:= $(call my-dir)


#============================================
# Build the library
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := com.intel.security.lib.sepdrmjni
LOCAL_SRC_FILES := $(call all-java-files-under,.)
include $(BUILD_JAVA_LIBRARY)

#============================================
include $(CLEAR_VARS)
#Copy com.marakana.android.lib.log.xml to /system/etc/permissions/
LOCAL_MODULE:= com.intel.security.lib.sepdrmjni.xml
LOCAL_MODULE_TAGS:=optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/permissions
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

#============================================
# Build JNI library
include $(CLEAR_VARS)

LOCAL_SRC_FILES := jni/com_intel_security_lib_sepdrmjni.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libbinder \
    libandroid_runtime \
    libnativehelper
#    libnativehelper \
#    libsepdrm

LOCAL_MODULE:= libsepdrmjni
LOCAL_MODULE_TAGS:=optional

LOCAL_C_INCLUDES := \
    $(TOP)/frameworks/base/include \
    $(TOP)/vendor/intel/hardware/PRIVATE/chaabi/Lib/inc \
    $(JNI_H_INCLUDE)

include $(BUILD_SHARED_LIBRARY)
