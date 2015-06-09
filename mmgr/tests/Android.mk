LOCAL_PATH:= $(call my-dir)

include $(call first-makefiles-under,$(LOCAL_PATH))

include $(CLEAR_VARS)
LOCAL_MODULE := mmgr-debug
LOCAL_MODULE_TAGS := optional tests

LOCAL_REQUIRED_MODULES :=\
    mmgr-test\
    MMGR_test\

include $(BUILD_PHONY_PACKAGE)
