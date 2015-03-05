LOCAL_PATH := $(call my-dir)

LIBCHAABI := $(TOP)/vendor/intel/hardware/PRIVATE/chaabi
ifeq ($(wildcard $(LIBCHAABI)),)
	external_release := yes
else
	external_release := no
endif

include $(CLEAR_VARS)

common_pmdb_files := \
	pmdb-access-sep.c \
	pmdb.c

ifeq ($(BUILD_WITH_SECURITY_FRAMEWORK),chaabi_token)
token_implementation := \
	tee_connector.c \
	token.c
else
token_implementation := \
	token.c
endif

common_libintelprov_files := \
	update_osip.c \
	fw_version_check.c \
	util.c \
	fpt.c \
	flash_image.c \
	txemanuf.c

ifeq ($(TARGET_BIOS_TYPE), "uefi")
common_libintelprov_files += flash_ifwi_uefi.c
else
common_libintelprov_files += flash_ifwi.c
endif

common_libintelprov_includes := \
	$(call include-path-for, libc-private) \
	$(call include-path-for, mkbootimg) \
	$(TARGET_OUT_HEADERS)/IFX-modem

chaabi_dir := $(TOP)/vendor/intel/hardware/PRIVATE/chaabi
sep_lib_includes := $(chaabi_dir)/SepMW/VOS6/External/Linux/inc/

# Provisionning CC54 tool
ifeq ($(BUILD_WITH_SECURITY_FRAMEWORK),chaabi_token)
include $(CLEAR_VARS)
LOCAL_MODULE := teeprov
LOCAL_SRC_FILES := teeprov.c tee_connector.c util.c
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := $(common_libintelprov_includes) bootable/recovery
LOCAL_CFLAGS := -Wall -Werror -Wno-unused-parameter
LOCAL_STATIC_LIBRARIES := libdx_cc7_static
LOCAL_SHARED_LIBRARIES := libc liblog libifp libkeymaster
include $(BUILD_EXECUTABLE)
endif

# Partitionning library
include $(CLEAR_VARS)
LOCAL_MODULE := liboempartitioning_static
LOCAL_SRC_FILES := oem_partition.c
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := bootable/droidboot $(LOCAL_PATH)/gpt/lib/include
LOCAL_WHOLE_STATIC_LIBRARIES := libpartlink_static libcgpt_static
include $(BUILD_STATIC_LIBRARY)

# Plug-in library for AOSP updater
include $(CLEAR_VARS)
LOCAL_MODULE := libintel_updater
LOCAL_SRC_FILES := updater.c $(common_libintelprov_files)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_C_INCLUDES := $(call include-path-for, recovery) $(common_libintelprov_includes)
LOCAL_CFLAGS := -Wall -Werror -Wno-unused-parameter
LOCAL_WHOLE_STATIC_LIBRARIES := liboempartitioning_static
ifeq ($(BOARD_HAVE_MODEM),true)
LOCAL_SRC_FILES += telephony/updater.c telephony/logs.c
LOCAL_CFLAGS += -DBOARD_HAVE_MODEM
LOCAL_WHOLE_STATIC_LIBRARIES += libxml2 libtcs libmiu
endif
ifeq ($(TARGET_BOARD_PLATFORM),clovertrail)
  LOCAL_CFLAGS += -DCLVT
endif
ifneq ($(filter $(TARGET_BOARD_PLATFORM),merrifield moorefield),)
  LOCAL_CFLAGS += -DMRFLD
endif
ifeq ($(BUILD_WITH_SECURITY_FRAMEWORK),chaabi_token)
LOCAL_SRC_FILES += tee_connector.c
LOCAL_WHOLE_STATIC_LIBRARIES += libdx_cc7_static
LOCAL_SHARED_LIBRARIES += libifp libkeymaster
LOCAL_CFLAGS += -DTEE_FRAMEWORK
endif
include $(BUILD_STATIC_LIBRARY)

# plugin for recovery_ui
include $(CLEAR_VARS)
LOCAL_SRC_FILES := recovery_ui.cpp
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := $(call include-path-for, recovery) $(call include-path-for, libc-private)
LOCAL_MODULE := libintel_recovery_ui
LOCAL_CFLAGS := -Wall -Werror -Wno-unused-parameter
ifeq ($(REF_DEVICE_NAME), mfld_pr2)
LOCAL_CFLAGS += -DMFLD_PRX_KEY_LAYOUT
endif
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_PREBUILT_EXECUTABLES := \
    releasetools.py \
    releasetools/ota_from_target_files \
    releasetools/check_target_files_signatures \
    releasetools/common.py \
    releasetools/edify_generator.py \
    releasetools/lfstk_wrapper.py \
    releasetools/mfld_osimage.py \
    releasetools/sign_target_files_apks \
    releasetools/product_name_mapping.def
include $(BUILD_HOST_PREBUILT)

# if DROIDBOOT is not used, we dont want this...
# allow to transition smoothly
ifeq ($(TARGET_USE_DROIDBOOT),true)

# Plug-in libary for Droidboot
include $(CLEAR_VARS)
LOCAL_MODULE := libintel_droidboot

LOCAL_CFLAGS := -Wall -Werror -Wno-unused-parameter -Wno-unused-but-set-variable
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

LOCAL_C_INCLUDES := bootable/droidboot $(call include-path-for, recovery) $(common_libintelprov_includes)

LOCAL_SRC_FILES := droidboot.c $(common_libintelprov_files)

LOCAL_WHOLE_STATIC_LIBRARIES := liboempartitioning_static
ifeq ($(BOARD_HAVE_MODEM),true)
LOCAL_SRC_FILES += telephony/droidboot.c
LOCAL_WHOLE_STATIC_LIBRARIES += libxml2 libtcs libmiu
LOCAL_CFLAGS += -DBOARD_HAVE_MODEM
endif

ifeq ($(external_release),no)
LOCAL_SRC_FILES += $(common_pmdb_files) $(token_implementation)
LOCAL_C_INCLUDES += $(sep_lib_includes)
LOCAL_WHOLE_STATIC_LIBRARIES += libsecurity_sectoken libcrypto_static CC6_UMIP_ACCESS CC6_ALL_BASIC_LIB
else
LOCAL_CFLAGS += -DEXTERNAL
ifeq ($(BUILD_WITH_SECURITY_FRAMEWORK),chaabi_token)
LOCAL_SRC_FILES += tee_connector.c
endif
endif
ifeq ($(BUILD_WITH_SECURITY_FRAMEWORK),chaabi_token)
LOCAL_CFLAGS += -DTEE_FRAMEWORK
LOCAL_WHOLE_STATIC_LIBRARIES += libdx_cc7_static
LOCAL_SHARED_LIBRARIES += libifp libkeymaster
endif

ifneq ($(DROIDBOOT_NO_GUI),true)
LOCAL_CFLAGS += -DUSE_GUI
endif
ifeq ($(TARGET_BOARD_PLATFORM),clovertrail)
LOCAL_CFLAGS += -DCLVT
else ifneq ($(filter $(TARGET_BOARD_PLATFORM),merrifield moorefield),)
LOCAL_CFLAGS += -DMRFLD
endif
ifeq ($(TARGET_PARTITIONING_SCHEME),"full-gpt")
  LOCAL_CFLAGS += -DFULL_GPT
endif
include $(BUILD_STATIC_LIBRARY)

# a test flashtool for testing the intelprov library
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := flashtool
LOCAL_SHARED_LIBRARIES := liblog libcutils

LOCAL_C_INCLUDES := $(common_libintelprov_includes) $(call include-path-for, recovery)
LOCAL_SRC_FILES := flashtool.c $(common_libintelprov_files)
LOCAL_CFLAGS := -Wall -Werror -Wno-unused-parameter

ifeq ($(BOARD_HAVE_MODEM),true)
LOCAL_CFLAGS += -DBOARD_HAVE_MODEM
LOCAL_SHARED_LIBRARIES += libicuuc
LOCAL_STATIC_LIBRARIES += libmiu libtcs libxml2
LOCAL_SRC_FILES += telephony/flashtool.c telephony/logs.c
endif

ifeq ($(TARGET_BOARD_PLATFORM),clovertrail)
LOCAL_CFLAGS += -DCLVT
else ifneq ($(filter $(TARGET_BOARD_PLATFORM),merrifield moorefield),)
LOCAL_CFLAGS += -DMRFLD
endif

include $(BUILD_EXECUTABLE)

# update_recovery: this binary is updating the recovery from MOS
# because we dont want to update it from itself.
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := update_recovery
LOCAL_SRC_FILES:= update_recovery.c util.c update_osip.c flash_image.c
LOCAL_C_INCLUDES := $(common_libintelprov_includes) $(call include-path-for, recovery)/applypatch $(call include-path-for, recovery) $(call include-path-for, mkbootimg)
LOCAL_CFLAGS := -Wall -Wno-unused-parameter
LOCAL_SHARED_LIBRARIES := liblog libcutils libz
LOCAL_STATIC_LIBRARIES := libmincrypt libapplypatch libbz
include $(BUILD_EXECUTABLE)
endif

include $(call all-makefiles-under,$(LOCAL_PATH))
