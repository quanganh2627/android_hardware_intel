LOCAL_PATH := $(call my-dir)

include $(call first-makefiles-under, $(LOCAL_PATH))

include $(CLEAR_VARS)
LOCAL_MODULE := mmgr_xml
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES :=\
    mmgr_6360_xml\
    mmgr_7160_xml\
    mmgr_7260_xml\

include $(BUILD_PHONY_PACKAGE)
