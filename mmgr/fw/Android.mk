# Modem firmwares and TLVs are stored in another repos. That is why, LOCAL_PATH
# is set like that.
LOCAL_PATH := .
IMC_FOLDER := vendor/intel/fw/modem/IMC

# Definition of a recursive wildcard function:
rwildcard=$(foreach d, $(wildcard $1*), $(call rwildcard, $d/, $2) \
    $(filter $(subst *, %, $2), $d))

# Definition of a generic copy file function. Used to copy tlv and fw files
define copy_file
include $(CLEAR_VARS)
LOCAL_MODULE := $(notdir $(1))
LOCAL_SRC_FILES := $(1)
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/firmware/telephony
include $(BUILD_PREBUILT)
endef

# Creation of hash file
include $(CLEAR_VARS)
LOCAL_MODULE := hash
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/firmware/telephony
include $(BUILD_SYSTEM)/base_rules.mk
$(LOCAL_BUILT_MODULE) : $(TLV_FILES) $(FLS_FILES)
	@echo "Building telephony hash file"
	$(hide) rm -fr $(dir $@)
	$(hide) mkdir -p $(dir $@)
	$(hide) cat $(TLV_FILES) $(FLS_FILES) | md5sum | tr -d ' -' > $@

TLV_FILES := $(call rwildcard, $(IMC_FOLDER), *.tlv)
FLS_FILES := $(foreach mdm, $(BOARD_MODEM_LIST), \
    $(wildcard $(IMC_FOLDER)/$(mdm)/FW/*.fls))

$(foreach tlv, $(TLV_FILES), $(eval $(call copy_file, $(tlv))))
$(foreach fls, $(FLS_FILES), $(eval $(call copy_file, $(fls))))

include $(CLEAR_VARS)
LOCAL_MODULE := mdm_fw_pkg
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_REQUIRED_MODULES := $(notdir $(TLV_FILES) $(FLS_FILES)) hash
include $(BUILD_PHONY_PACKAGE)
