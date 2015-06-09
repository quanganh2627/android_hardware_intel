
LOCAL_PATH := .

# Definition of a recursive wildcard function:
rwildcard=$(foreach d, $(wildcard $1*), $(call rwildcard, $d/, $2) \
    $(filter $(subst *, %, $2), $d))

# Definition of a generic copy file function. Used to copy all XML files
define copy_file
include $(CLEAR_VARS)
LOCAL_MODULE := $(notdir $(1))
LOCAL_SRC_FILES := $(1)
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/telephony
include $(BUILD_PREBUILT)
endef

XML_FILES := $(call rwildcard, $(call my-dir), *.xml)

$(foreach xml, $(XML_FILES), $(eval $(call copy_file, $(xml))))

include $(CLEAR_VARS)
LOCAL_MODULE := mmgr_xml
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(notdir $(XML_FILES))
include $(BUILD_PHONY_PACKAGE)
