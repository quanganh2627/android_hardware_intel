LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
 
LOCAL_SRC_FILES:= \
	intel_aidltest_jni_JNIClient.c \
	socketclient_interface.c
LOCAL_C_INCLUDES := \
    $(JNI_H_INCLUDE)
 
LOCAL_SHARED_LIBRARIES := \
    libutils
 
LOCAL_PRELINK_MODULE := false
 
LOCAL_MODULE := libUnipoint
LOCAL_MODULE_TAGS := optional
 
include $(BUILD_SHARED_LIBRARY)
