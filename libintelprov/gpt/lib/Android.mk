LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:=  $(LOCAL_PATH)/include/
LOCAL_SRC_FILES:= \
	cgpt_add.c \
	cgpt_boot.c \
	cgpt_common.c \
	cgpt_create.c \
	cgpt_find.c \
	cgpt_legacy.c \
	cgptlib_internal.c \
	cgpt_prioritize.c \
	cgpt_repair.c \
	cgpt_show.c \
	crc32.c \
	utility_stub.c \
	cmd_add.c \
	cmd_boot.c \
	cmd_create.c \
	cmd_find.c \
	cmd_legacy.c \
	cmd_prioritize.c \
	cmd_repair.c \
	cmd_reload.c \
	cmd_show.c

LOCAL_MODULE := libcgpt_static
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)
