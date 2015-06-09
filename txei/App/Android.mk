LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := TXEI_TEST
LOCAL_MODULE_TAGS := optional
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_SRC_FILES := txei_test.c
LOCAL_STATIC_LIBRARIES := libcutils libc libtxei
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/inc \
    $(TARGET_OUT_HEADERS)/libtxei
include $(BUILD_EXECUTABLE)


#####################
#  Security tools application (TXEI_SEC_TOOLS)
#
include $(CLEAR_VARS)
LOCAL_MODULE := TXEI_SEC_TOOLS
LOCAL_MODULE_TAGS := optional
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_SRC_FILES := sec_tool/sec_tools_app.c
LOCAL_STATIC_LIBRARIES := \
    CC6_TXEI_UMIP_ACCESS \
    liblog libcutils libc libtxei libtxeimiscutils
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/inc \
    $(TARGET_OUT_HEADERS)/libtxei \
    $(LOCAL_PATH)/../Lib/sec_tool_lib/inc
LOCAL_CFLAGS := -DACD_WIPE_TEST
include $(BUILD_EXECUTABLE)
