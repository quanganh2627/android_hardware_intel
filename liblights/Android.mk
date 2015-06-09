LOCAL_PATH:= $(call my-dir)
# HAL module implemenation, not prelinked and stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.board.platform>.so
include $(CLEAR_VARS)

LOCAL_SRC_FILES := lights.c

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_SHARED_LIBRARIES := liblog

LOCAL_MODULE := lights.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional

ifeq ($(BOARD_GRAPHIC_IS_GEN),true)
LOCAL_CFLAGS += -DGRAPHIC_IS_GEN
endif

include $(BUILD_SHARED_LIBRARY)
