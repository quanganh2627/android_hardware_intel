LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= fg_conf_helper.c
LOCAL_MODULE:= fg_conf
LOCAL_MODULE_TAGS:= optional
LOCAL_SHARED_LIBRARIES:= libcutils
include $(BUILD_EXECUTABLE)

