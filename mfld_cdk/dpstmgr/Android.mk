ifeq ($(INTEL_DPST),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	dpstmgr.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libdm_dpst
LOCAL_MODULE:= dpstmgr
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
include $(LOCAL_PATH)/lib/Android.mk

endif
