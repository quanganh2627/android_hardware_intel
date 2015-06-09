ifneq ($(BOARD_HAVE_GEN_GFX_SRC),true)

LOCAL_PATH:= $(call my-dir)

UFO_PROJECT_PATH := $(LOCAL_PATH)

# ------------------------------------------------------------------------
## include ufo_headers.mk for needed header exports
include $(LOCAL_PATH)/ufo_headers.mk
# ------------------------------------------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE := ufo
LOCAL_MODULE_TAGS := optional

# Required static libraries or export_includes
# libastl - From external/asti, required for building these packages.
LOCAL_STATIC_LIBRARIES := libastl

# Required libraries or export_includes
LOCAL_SHARED_LIBRARIES :=
LOCAL_SHARED_LIBRARIES += libva
LOCAL_SHARED_LIBRARIES += libva-android
ifeq ($(strip $(INTEL_WIDI)),true)
ifeq ($(strip $(INTEL_WIDI_BAYTRAIL)),true)
LOCAL_SHARED_LIBRARIES += libhwcwidi
endif
endif

# Replace $(BUILD_PHONY_PACKAGE) with a kludge that will generate
# pre-requisites for the package, particularly $(LOCAL_STATIC_LIBRARIES).
# Note: Variable "all_libraries" is a build system internal variable.

## include $(BUILD_PHONY_PACKAGE)

# ------------------------------------------------------------------------
LOCAL_MODULE_CLASS := FAKE
LOCAL_MODULE_SUFFIX := -timestamp

include $(BUILD_SYSTEM)/binary.mk

$(LOCAL_BUILT_MODULE): $(all_libraries)

$(LOCAL_BUILT_MODULE):
	$(hide) echo "Fake: $@"
	$(hide) mkdir -p $(dir $@)
	$(hide) touch $@

# ------------------------------------------------------------------------

$(LOCAL_INSTALLED_MODULE): $(LOCAL_MODULE)_always_extract

.PHONY: $(LOCAL_MODULE)_always_extract

$(LOCAL_MODULE)_always_extract: PRIVATE_PATH := $(LOCAL_PATH)

$(LOCAL_MODULE)_always_extract:
	@ echo "$(UFO_PROJECT_PATH): GEN GFX: $@"
	$(hide) mkdir -p $(dir $@)

#for user build, strip all symbols, but keep them otherwise
ifeq ($(TARGET_BUILD_VARIANT),user)
	$(hide) $(PRIVATE_PATH)/dist_install.sh --strip-symbols $(PRIVATE_PATH)/dist $(PRODUCT_OUT)
else
	$(hide) $(PRIVATE_PATH)/dist_install.sh $(PRIVATE_PATH)/dist $(PRODUCT_OUT)
endif

ifeq ($(TARGET_BOARD_PLATFORM),bigcore)
	$(hide) $(PRIVATE_PATH)/dist_install.sh $(PRIVATE_PATH)/dist_bigcore $(PRODUCT_OUT)
endif

# ------------------------------------------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE := libivp
LOCAL_MODULE_TAGS := optional

LOCAL_EXPORT_C_INCLUDE_DIRS += $(UFO_PROJECT_PATH)/include

# Can't use shared_library.mk because it defines target $(linked_module)
# to be built as a shared library, instead of copied.
# Can't use prebuilt.mk because it has a dummy export_includes.

# Set things that would be set by shared_library.mk if really building a
# shared library.

LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so

# Expected: TARGET_OUT_INTERMEDIATE_LIBRARIES = ${ANDROID_PRODUCT_OUT}/obj/lib
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)

# Do not strip, as debug section creation will fail because already stripped.
LOCAL_STRIP_MODULE := false

include $(BUILD_SYSTEM)/dynamic_binary.mk

# Inclusion of all_copied_headers invokes code for LOCAL_COPY_HEADERS.

$(linked_module): $(UFO_PROJECT_PATH)/dist/system/lib/libivp.so.xz all_copied_headers
	mkdir -p $(dir $@)
	xz -d -c $< >$@
	chmod --reference=$< $@

# ------------------------------------------------------------------------

include $(CLEAR_VARS)

LOCAL_MODULE := libpavp
LOCAL_MODULE_TAGS := optional

LOCAL_EXPORT_C_INCLUDE_DIRS += $(UFO_PROJECT_PATH)/include

# Can't use shared_library.mk because it defines target $(linked_module)
# to be built as a shared library, instead of copied.
# Can't use prebuilt.mk because it has a dummy export_includes.

# Set things that would be set by shared_library.mk if really building a
# shared library.

LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so

# Expected: TARGET_OUT_INTERMEDIATE_LIBRARIES = ${ANDROID_PRODUCT_OUT}/obj/lib
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)

# Do not strip, as debug section creation will fail because already stripped.
LOCAL_STRIP_MODULE := false

include $(BUILD_SYSTEM)/dynamic_binary.mk

# Inclusion of all_copied_headers invokes code for LOCAL_COPY_HEADERS.

$(linked_module): $(UFO_PROJECT_PATH)/dist/system/lib/libpavp.so.xz all_copied_headers
	mkdir -p $(dir $@)
	xz -d -c $< >$@
	chmod --reference=$< $@

endif # ifneq ($(BOARD_HAVE_GEN_GFX_SRC),true)
