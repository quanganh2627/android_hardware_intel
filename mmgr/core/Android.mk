LOCAL_PATH:= $(call my-dir)

#############################################
# General rules:
#############################################
MY_MODULE := mmgr
MY_MODULE_TAGS := optional

MY_INCLUDES := \
    $(MMGR_PATH)/inc \
    $(LOCAL_PATH)/link \
    $(TARGET_OUT_HEADERS)/libtcs \
    $(TARGET_OUT_HEADERS)/libmcdr \
    $(call include-path-for, libusb) \
    $(call include-path-for, libpower) \

MY_SRC_FILES := $(call all-c-files-under, .)

# Extract commit id
COMMIT_ID := $(shell git --git-dir=$(MMGR_PATH)/.git \
        --work-tree=$(LOCAL_PATH) log --oneline -n1 \
        | sed -e 's:\s\{1,\}:\\ :g' -e 's:["&{}]::g' \
        -e "s:'::g")

MY_C_FLAGS := -Wall -Werror -Wvla -DLIBUSBHOST \
    -DGIT_COMMIT_ID=\"$(COMMIT_ID)\" -DMODULE_NAME=\"MMGR\"

MY_SHARED_LIBS := libc libcutils libdl libusbhost liblog libpower
MY_LOCAL_IMPORT := libtcs libmmgr_utils libmmgr_cnx
MY_LDLIBS := -lpthread

#############################################
# MODEM MANAGER daemon
#############################################
include $(CLEAR_VARS)
LOCAL_MODULE := $(MY_MODULE)
LOCAL_MODULE_TAGS := $(MY_MODULE_TAGS)

LOCAL_C_INCLUDES := $(MY_INCLUDES)
LOCAL_SRC_FILES :=  $(MY_SRC_FILES)
LOCAL_CFLAGS += $(MY_C_FLAGS)

# libmcdr is not linked. Only import the header
LOCAL_IMPORT_C_INCLUDE_DIRS_FROM_SHARED_LIBRARIES := $(MY_LOCAL_IMPORT) libmcdr
LOCAL_SHARED_LIBRARIES := $(MY_SHARED_LIBS) $(MY_LOCAL_IMPORT)
LOCAL_LDLIBS += $(MY_LDLIBS)
LOCAL_REQUIRED_MODULES := mmgr_xml

# libmcdr is used by mmgr in userdebug and eng builds only
ifneq (, $(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
    LOCAL_REQUIRED_MODULES += libmcdr
endif

ifeq ($(BUILD_WITH_SECURITY_FRAMEWORK), chaabi_token)
LOCAL_REQUIRED_MODULES += libdx_cc7
endif

include $(BUILD_EXECUTABLE)

#############################################
# MODEM MANAGER daemon - GCOV
#############################################
ifeq ($(mmgr_gcov), true)

include $(CLEAR_VARS)
LOCAL_MODULE := $(addsuffix -gcov, $(MY_MODULE))
LOCAL_MODULE_TAGS := $(MY_MODULE_TAGS)

LOCAL_C_INCLUDES := $(MY_INCLUDES)
LOCAL_SRC_FILES :=  $(MY_SRC_FILES)
LOCAL_CFLAGS += $(MY_C_FLAGS) -fprofile-arcs -ftest-coverage -DGOCV_MMGR

MY_LOCAL_GCOV_IMPORT := $(foreach file,$(MY_LOCAL_IMPORT), $(addsuffix -gcov, $(file)))
# libmcdr is not linked. Only import the header
LOCAL_IMPORT_C_INCLUDE_DIRS_FROM_SHARED_LIBRARIES += $(MY_LOCAL_GCOV_IMPORT) libmcdr
LOCAL_LDLIBS += $(MY_LDLIBS)
LOCAL_LDFLAGS += -fprofile-arcs -ftest-coverage -lgcov
LOCAL_SHARED_LIBRARIES := $(MY_SHARED_LIBS) $(MY_LOCAL_GCOV_IMPORT)
include $(BUILD_EXECUTABLE)

endif
