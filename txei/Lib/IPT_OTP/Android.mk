# Build the components in this subtree
# only for platforms with trusted execution engine
ifeq ($(filter $(TARGET_BOARD_PLATFORM),baytrail cherrytrail braswell),$(TARGET_BOARD_PLATFORM))
include $(call all-subdir-makefiles)
endif
