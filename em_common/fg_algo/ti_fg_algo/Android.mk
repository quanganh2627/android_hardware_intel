#Android Make file to build fg_algo_iface Executable
LOCAL_PATH := $(call my-dir)
MY_TI_LOCAL_PATH := $(LOCAL_PATH)

ifeq ($(USE_FG_ALGO), true)

include $(CLEAR_VARS)
LOCAL_PATH = $(MY_TI_LOCAL_PATH)
LOCAL_PREBUILT_LIBS := libfg/libfg.so
include $(BUILD_MULTI_PREBUILT)

include $(CLEAR_VARS)
include $(call first-makefiles-under,$(LOCAL_PATH))

include $(CLEAR_VARS)
LOCAL_PATH = $(MY_TI_LOCAL_PATH)

LOCAL_SRC_FILES := fg_config_ti_parse.c
LOCAL_MODULE := libfg_ti_algo
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := libc libcutils libfg
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

endif

