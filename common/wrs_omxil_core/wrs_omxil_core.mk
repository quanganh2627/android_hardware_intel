# Include wrs_omxil_components to the image

# omx enable
BOARD_USES_WRS_OMXIL_CORE := true
PRODUCT_PACKAGES += \
        10_wrs_omxil_core.cfg \
        libstagefrighthw \
        libwrs_omxil_core_pvwrapped \
        libwrs_omxil_common \
        libwrs_omxil_utils

