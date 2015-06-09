/*
 * INTEL CONFIDENTIAL
 *
 * Copyright 2013-2014 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to the
 * source code ("Material") are owned by Intel Corporation or its suppliers or
 * licensors. Title to the Material remains with Intel Corporation or its suppliers
 * and licensors. The Material contains trade secrets and proprietary and confidential
 * information of Intel or its suppliers and licensors. The Material is protected by
 * worldwide copyright and trade secret laws and treaty provisions. No part of the
 * Material may be used, copied, reproduced, modified, published, uploaded, posted,
 * transmitted, distributed, or disclosed in any way without Intels prior express
 * written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel
 * or otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */

/**
 * This <ufo/gralloc.h> file contains public extensions provided by UFO GRALLOC HAL.
 */

#ifndef INTEL_UFO_GRALLOC_H
#define INTEL_UFO_GRALLOC_H

#include <hardware/gralloc.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Simple API level control
 * \see INTEL_UFO_GRALLOC_MODULE_VERSION_LATEST
 */
#define INTEL_UFO_GRALLOC_API_LEVEL 0

// Enable for FB reference counting.
#define INTEL_UFO_GRALLOC_HAVE_FB_REF_COUNTING 1
// Enable for PAVP query.
#define INTEL_UFO_GRALLOC_HAVE_QUERY_PAVP_SESSION 1
// Enable for Media query.
#define INTEL_UFO_GRALLOC_HAVE_QUERY_MEDIA_DETAILS 1

/**
 * stage1: media can use gem_datatype with legacy offsets/bits
 * stage2: media can use gem_datatype with new compressed offsets/bits
 * stage3: same as stage2 but additionally gralloc uses private data to store other bits that don't fit into gem_datatype
 * stage4: gralloc uses private data for all bits. only gralloc owns gem_datatype!
 */
#define INTEL_UFO_GRALLOC_MEDIA_API_STAGE 1


/** Operations for the (*perform)() hook
 * \see gralloc_module_t::perform
 */
enum {
    INTEL_UFO_GRALLOC_MODULE_PERFORM_CHECK_VERSION  = 0, // (void)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_GET_DRM_FD     = 1, // (int*)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_DISPLAY    = 2, // (int display, uint32_t width, uint32_t height, uint32_t xdpi, uint32_t ydpi)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_GET_BO_HANDLE  = 3, // (buffer_handle_t, int*)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_GET_BO_NAME    = 4, // (buffer_handle_t, uint32_t*)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_GET_BO_FBID    = 5, // (buffer_handle_t, uint32_t*)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_GET_BO_INFO    = 6, // (buffer_handle_t, intel_ufo_buffer_details_t*)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_GET_BO_STATUS  = 7, // (buffer_handle_t)
#if INTEL_UFO_GRALLOC_HAVE_FB_REF_COUNTING
    INTEL_UFO_GRALLOC_MODULE_PERFORM_FB_ACQUIRE     = 8, // (uint32_t)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_FB_RELEASE     = 9, // (uint32_t)
#endif
    INTEL_UFO_GRALLOC_MODULE_PERFORM_QUERY_PAVP_SESSION    = 10,   // (buffer_handle_t, intel_ufo_buffer_pavp_session_t*)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_QUERY_MEDIA_DETAILS   = 11,   // (buffer_handle_t, intel_ufo_buffer_media_details_t*)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_PAVP_SESSION   = 12,   // (buffer_handle_t, uint32_t session, uint32_t instance, uint32_t is_encrypted)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_COLOR_RANGE    = 13,   // (buffer_handle_t, uint32_t color_range)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_CLIENT_ID      = 14,   // (buffer_handle_t, uint32_t client_id)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_MMC_MODE       = 15,   // (buffer_handle_t, uint32_t mmc_mode)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_KEY_FRAME      = 16,   // (buffer_handle_t, uint32_t is_key_frame)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_CODEC_TYPE     = 17,   // (buffer_handle_t, uint32_t codec, uint32_t is_interlaced)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_DIRTY_RECT     = 18,   // (buffer_handle_t, uint32_t valid, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_QUERY_GMM_PARAMS      = 19,   // (buffer_handle_t, GMM_RESCREATE_PARAMS* params)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_PROBE_BUFFER_GEOMETRY = 20,   // FIXME PLACEHOLDER (intel_ufo_buffer_geometry_t*)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_REGISTER_HWC_PROCS    = 21,   // (const intel_ufo_hwc_procs_t*)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_FRAME_UPDATED  = 22,   // (buffer_handle_t, uint32_t is_updated)
    INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_FRAME_ENCODED  = 23,   // (buffer_handle_t, uint32_t is_encoded)
#if 1 // reserved for internal use only !
    INTEL_UFO_GRALLOC_MODULE_PERFORM_PRIVATE_0      = -1000,
    INTEL_UFO_GRALLOC_MODULE_PERFORM_PRIVATE_1      = -1001,
    INTEL_UFO_GRALLOC_MODULE_PERFORM_PRIVATE_2      = -1002,
    INTEL_UFO_GRALLOC_MODULE_PERFORM_PRIVATE_3      = -1003,
#endif
};


/** Simple version control
 * \see gralloc_module_t::perform
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_CHECK_VERSION
 */
enum {
    INTEL_UFO_GRALLOC_MODULE_VERSION_0 = ANDROID_NATIVE_MAKE_CONSTANT('I','N','T','C'),
    INTEL_UFO_GRALLOC_MODULE_VERSION_LATEST = INTEL_UFO_GRALLOC_MODULE_VERSION_0 + INTEL_UFO_GRALLOC_API_LEVEL,
};


/** Structure with detailed info about allocated buffer.
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_GET_BO_INFO
 */
typedef struct intel_ufo_buffer_details_t
{
    int width;       // \see alloc_device_t::alloc
    int height;      // \see alloc_device_t::alloc
    int format;      // \see alloc_device_t::alloc
    int usage;       // \see alloc_device_t::alloc
    int name;        // flink
    uint32_t fb;        // framebuffer id
    uint32_t fb_format; // framebuffer drm format
    int pitch;       // buffer pitch (in bytes)
    int size;        // buffer size (in bytes)

    int allocWidth;  // allocated buffer width in pixels.
    int allocHeight; // allocated buffer height in lines.
    int allocOffsetX;// horizontal pixel offset to content origin within allocated buffer.
    int allocOffsetY;// vertical line offset to content origin within allocated buffer.
} intel_ufo_buffer_details_t;


/** Structure with additional info about buffer that could be changed after allocation.
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_QUERY_MEDIA_DETAILS
 */
typedef struct intel_ufo_buffer_media_details_t
{
    uint32_t magic;             // [in] Size of this struct
    uint32_t pavp_session_id;   // PAVP Session ID.
    uint32_t pavp_instance_id;  // PAVP Instance.
    uint32_t yuv_color_range;   // YUV Color range.
    uint32_t client_id;         // HWC client ID.
    uint32_t is_updated;        // frame updated flag
    uint32_t is_encoded;        // frame encoded flag
    uint32_t is_encrypted;
    uint32_t is_key_frame;
    uint32_t is_interlaced;
    uint32_t is_mmc_capable;
    uint32_t compression_mode;
    uint32_t codec;
    struct {
        uint32_t is_valid;
        struct {
            uint32_t left;
            uint32_t top;
            uint32_t right;
            uint32_t bottom;
        } rect;
    } dirty;                    // Dirty region hint.
} intel_ufo_buffer_media_details_t;


/**
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_PAVP_SESSION
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_QUERY_MEDIA_DETAILS
 * \see intel_ufo_buffer_media_details_t.pavp_session
 */
enum {
#if (INTEL_UFO_GRALLOC_MEDIA_API_STAGE == 1) || (INTEL_UFO_GRALLOC_MEDIA_API_STAGE == 2) || (INTEL_UFO_GRALLOC_MEDIA_API_STAGE == 3)
    INTEL_UFO_BUFFER_PAVP_SESSION_MAX = 0xF,
#else  /* INTEL_UFO_GRALLOC_MEDIA_API_STAGE */
    INTEL_UFO_BUFFER_PAVP_SESSION_MAX = 0xFFFFFFFF,
#endif /* INTEL_UFO_GRALLOC_MEDIA_API_STAGE */
};


/**
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_PAVP_SESSION
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_QUERY_MEDIA_DETAILS
 * \see intel_ufo_buffer_media_details_t.pavp_instance
 */
enum {
#if (INTEL_UFO_GRALLOC_MEDIA_API_STAGE == 1)
    INTEL_UFO_BUFFER_PAVP_INSTANCE_MAX = 0xFFFF,
#elif (INTEL_UFO_GRALLOC_MEDIA_API_STAGE == 2) || (INTEL_UFO_GRALLOC_MEDIA_API_STAGE == 3)
    INTEL_UFO_BUFFER_PAVP_INSTANCE_MAX = 0xF,
#else  /* INTEL_UFO_GRALLOC_MEDIA_API_STAGE */
    INTEL_UFO_BUFFER_PAVP_INSTANCE_MAX = 0xFFFFFFFF,
#endif /* INTEL_UFO_GRALLOC_MEDIA_API_STAGE */
};


/**
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_SET_BO_COLOR_RANGE
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_QUERY_MEDIA_DETAILS
 * \see intel_ufo_buffer_media_details_t.yuv_color_range
 */
enum {
    INTEL_UFO_BUFFER_COLOR_RANGE_LIMITED = 0u,
    INTEL_UFO_BUFFER_COLOR_RANGE_FULL = 1u,
};


/** Structure with additional info about buffer used by 3D driver.
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_QUERY_3D_RESOLVE_DETAILS
 */
typedef struct intel_ufo_buffer_3d_resolve_details_t
{
    uint32_t magic;             // [in] Size of this struct

    struct {
        uint32_t is_available;
        uint32_t mode;

        union {
            uint32_t value;
            struct {
                uint32_t enable:1;
                uint32_t mode:1;
            };
        } memory_compression;

        struct {
            uint32_t red;
            uint32_t green;
            uint32_t blue;
        } clear_color;

    } aux_surface;

} intel_ufo_buffer_3d_resolve_details_t;


/** Structure with info about buffer PAVP sesssion.
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_QUERY_PAVP_SESSION
 * \deprecated
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_QUERY_MEDIA_DETAILS
 */
typedef struct intel_ufo_buffer_pavp_session_t
{
    uint32_t sessionID; // Session ID.
    uint32_t instance;  // Instance.
} intel_ufo_buffer_pavp_session_t;


#if (INTEL_UFO_GRALLOC_MEDIA_API_STAGE == 1)

/** This structure defines how datatype bits are used.
 */
typedef union intel_ufo_bo_datatype_t {
    uint32_t value;
    struct {
        uint32_t color_range        :2;
        uint32_t is_updated         :1;
        uint32_t is_encoded         :1;
        uint32_t unused1            :5;
        uint32_t is_encrypted       :1;
        uint32_t unused2            :2;
        uint32_t pavp_session_id    :4;
        uint32_t pavp_instance_id   :16;
    };
} intel_ufo_bo_datatype_t;

#elif (INTEL_UFO_GRALLOC_MEDIA_API_STAGE == 2) || (INTEL_UFO_GRALLOC_MEDIA_API_STAGE == 3)

typedef union intel_ufo_bo_datatype_t {
    uint32_t value;
    struct {
        uint32_t unused             :2;
        uint32_t is_updated         :1;
        uint32_t is_encoded         :1;
        uint32_t is_interlaced      :1;
        uint32_t is_mmc_capable     :1; // MMC
        uint32_t compression_mode   :2; // MMC
        uint32_t color_range        :2;
        uint32_t is_key_frame       :1;
        uint32_t pavp_session_id    :8;
        uint32_t is_encrypted       :1;
        uint32_t pavp_instance_id   :4;
        uint32_t client_id          :8;
    };
} intel_ufo_bo_datatype_t;

#else /* INTEL_UFO_GRALLOC_MEDIA_API_STAGE */

/*
 * At this stage intel_ufo_bo_datatype_t is not exposed by gralloc! 
 */

#endif /* INTEL_UFO_GRALLOC_MEDIA_API_STAGE */



/** This structure defines private callback API from GrAlloc to HWC.
 * \see gralloc_module_t::perform
 * \see INTEL_UFO_GRALLOC_MODULE_PERFORM_REGISTER_HWC_PROCS
 */
typedef struct intel_ufo_hwc_procs_t
{
    /* This function will be called by gralloc during processing of alloc() request.
     * It will be called after gralloc initially resolves flexible HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED format.
     * It will be called before gralloc issues any allocation calls into kernel driver.
     * If this function returns an error then gralloc will allocate buffer using default settings.
     * \see alloc_device_t::alloc
     *
     * \param procs pointer to struct that was passed during registration
     * \param width pointer to requested buffer width; HWC may increase it to optimize allocation (cursor/full screen)
     * \param height pointer to requested buffer height; HWC may increase it to optimize allocation (cursor/full screen)
     * \param format pointer to effective buffer format; HWC may modify it only if HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED
     * \param usage pointer to requested buffer usage; HWC may add new usage flags
     * \param fb_format pointer to FB format to be used by gralloc for this buffer; if set to zero then gralloc will not allocate FB for this buffer.
     * \param flags pointer to flags; TBD
     *
     * \return 0 on success or a negative error code on error.
     *
     * \note this function field is REQUIRED.
     */
    int (*pre_buffer_alloc)(const struct intel_ufo_hwc_procs_t* procs, int *width, int *height, int *format, int *usage, uint32_t *fb_format, uint32_t *flags);

    /* This function will be called by gralloc during processing of alloc() request.
     * It will be called after only after succesfull buffer memory allocation.
     * \see alloc_device_t::alloc
     *
     * \param procs pointer to struct that was passed during registration
     * \param buffer handle to just allocated buffer
     * \param details pointer to buffer details
     *
     * \note this function field is REQUIRED.
     */
    void (*post_buffer_alloc)(const struct intel_ufo_hwc_procs_t* procs, buffer_handle_t, const intel_ufo_buffer_details_t *details);

    /* This function will be called by gralloc during processing of free() request.
     * It will be called after only after succesfull buffer memory allocation.
     * \see alloc_device_t::free
     *
     * \param procs pointer to struct that was passed during registration
     * \param buffer handle to buffer that is about to be released by gralloc
     *
     * \note this function field is REQUIRED.
     */
    void (*post_buffer_free)(const struct intel_ufo_hwc_procs_t* procs, buffer_handle_t);

    /* Reserved for future use. Must be NULL. */
    void *reserved[5];
} intel_ufo_hwc_procs_t;


/**
 * \see intel_ufo_hwc_procs_t::pre_buffer_alloc
 * \see intel_ufo_buffer_details_t::flags
 */
enum {
    INTEL_UFO_BUFFER_FLAG_NONE      = 0,
    /**
     * HWC can set the INTEL_UFO_BUFFER_FLAG_LINEAR flag to indicate
     * that gralloc should use linear allocation for this buffer.
     */
    INTEL_UFO_BUFFER_FLAG_LINEAR    = 0x00000001,
    /**
     * HWC can set the INTEL_UFO_BUFFER_FLAG_X_TILED flag to indicate
     * that gralloc should use X tiled allocation for this buffer.
     */
    INTEL_UFO_BUFFER_FLAG_X_TILED   = 0x00000002,
    /**
     * HWC can set the INTEL_UFO_BUFFER_FLAG_Y_TILED flag to indicate
     * that gralloc should use Y tiled allocation for this buffer.
     */
    INTEL_UFO_BUFFER_FLAG_Y_TILED   = 0x00000004,
    /**
     * HWC can set the INTEL_UFO_BUFFER_FLAG_CURSOR flag to indicate
     * that gralloc should treat this buffer as cursor allocation.
     */
    INTEL_UFO_BUFFER_FLAG_CURSOR    = 0x10000000,
};


#ifdef __cplusplus
} // extern "C"
#endif

#endif /* INTEL_UFO_GRALLOC_H */
