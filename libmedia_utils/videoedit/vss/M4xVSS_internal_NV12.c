/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "M4OSA_Debug.h"
#include "M4OSA_CharStar.h"

#include "NXPSW_CompilerSwitches.h"

#include "M4VSS3GPP_API.h"
#include "M4VSS3GPP_ErrorCodes.h"

#include "M4xVSS_API.h"
#include "M4xVSS_Internal.h"


/*for rgb16 color effect*/
#include "M4VIFI_Defines.h"
#include "M4VIFI_Clip.h"

/**
 * component includes */
#include "M4VFL_transition.h"            /**< video effects */

/* Internal header file of VSS is included because of MMS use case */
#include "M4VSS3GPP_InternalTypes.h"

/*Exif header files to add image rendering support (cropping, black borders)*/
#include "M4EXIFC_CommonAPI.h"
// StageFright encoders require %16 resolution
#include "M4ENCODER_common.h"

#include "M4AIR_API_NV12.h"
#include "VideoEditorToolsNV12.h"

#define TRANSPARENT_COLOR 0x7E0
#define LUM_FACTOR_MAX 10

/**
 ******************************************************************************
 * M4VIFI_UInt8 M4VIFI_RGB565toNV12 (void *pUserData,
 *                                   M4VIFI_ImagePlane *pPlaneIn,
 *                                   M4VIFI_ImagePlane *pPlaneOut)
 * @brief   transform RGB565 image to a NV12 image.
 * @note    Convert RGB565 to NV12,
 *          Loop on each row ( 2 rows by 2 rows )
 *              Loop on each column ( 2 col by 2 col )
 *                  Get 4 RGB samples from input data and build 4 output Y samples
 *                  and each single U & V data
 *              end loop on col
 *          end loop on row
 * @param   pUserData: (IN) User Specific Data
 * @param   pPlaneIn: (IN) Pointer to RGB565 Plane
 * @param   pPlaneOut: (OUT) Pointer to  NV12 buffer Plane
 * @return  M4VIFI_OK: there is no error
 * @return  M4VIFI_ILLEGAL_FRAME_HEIGHT: YUV Plane height is ODD
 * @return  M4VIFI_ILLEGAL_FRAME_WIDTH:  YUV Plane width is ODD
 ******************************************************************************
*/

M4VIFI_UInt8 M4VIFI_RGB565toNV12(void *pUserData, M4VIFI_ImagePlane *pPlaneIn,
    M4VIFI_ImagePlane *pPlaneOut)
{
    M4VIFI_UInt32   u32_width, u32_height;
    M4VIFI_UInt32   u32_stride_Y, u32_stride2_Y, u32_stride_UV;
    M4VIFI_UInt32   u32_stride_rgb, u32_stride_2rgb;
    M4VIFI_UInt32   u32_col, u32_row;

    M4VIFI_Int32    i32_r00, i32_r01, i32_r10, i32_r11;
    M4VIFI_Int32    i32_g00, i32_g01, i32_g10, i32_g11;
    M4VIFI_Int32    i32_b00, i32_b01, i32_b10, i32_b11;
    M4VIFI_Int32    i32_y00, i32_y01, i32_y10, i32_y11;
    M4VIFI_Int32    i32_u00, i32_u01, i32_u10, i32_u11;
    M4VIFI_Int32    i32_v00, i32_v01, i32_v10, i32_v11;
    M4VIFI_UInt8    *pu8_yn, *pu8_ys, *pu8_u, *pu8_v;
    M4VIFI_UInt8    *pu8_y_data, *pu8_u_data, *pu8_v_data;
    M4VIFI_UInt8    *pu8_rgbn_data, *pu8_rgbn;
    M4VIFI_UInt16   u16_pix1, u16_pix2, u16_pix3, u16_pix4;

    /* Check planes height are appropriate */
    if ((pPlaneIn->u_height != pPlaneOut[0].u_height)           ||
        (pPlaneOut[0].u_height != (pPlaneOut[1].u_height<<1)))
    {
        return M4VIFI_ILLEGAL_FRAME_HEIGHT;
    }

    /* Check planes width are appropriate */
    if ((pPlaneIn->u_width != pPlaneOut[0].u_width)         ||
        (pPlaneOut[0].u_width != pPlaneOut[1].u_width))
    {
        return M4VIFI_ILLEGAL_FRAME_WIDTH;
    }

    /* Set the pointer to the beginning of the output data buffers */
    pu8_y_data = pPlaneOut[0].pac_data + pPlaneOut[0].u_topleft;
    pu8_u_data = pPlaneOut[1].pac_data + pPlaneOut[1].u_topleft;
    pu8_v_data = pu8_u_data + 1;

    /* Set the pointer to the beginning of the input data buffers */
    pu8_rgbn_data   = pPlaneIn->pac_data + pPlaneIn->u_topleft;

    /* Get the size of the output image */
    u32_width = pPlaneOut[0].u_width;
    u32_height = pPlaneOut[0].u_height;

    /* Set the size of the memory jumps corresponding to row jump in each output plane */
    u32_stride_Y = pPlaneOut[0].u_stride;
    u32_stride2_Y = u32_stride_Y << 1;
    u32_stride_UV = pPlaneOut[1].u_stride;

    /* Set the size of the memory jumps corresponding to row jump in input plane */
    u32_stride_rgb = pPlaneIn->u_stride;
    u32_stride_2rgb = u32_stride_rgb << 1;


    /* Loop on each row of the output image, input coordinates are estimated from output ones */
    /* Two YUV rows are computed at each pass */

    for (u32_row = u32_height ;u32_row != 0; u32_row -=2)
    {
        /* Current Y plane row pointers */
        pu8_yn = pu8_y_data;
        /* Next Y plane row pointers */
        pu8_ys = pu8_yn + u32_stride_Y;
        /* Current U plane row pointer */
        pu8_u = pu8_u_data;
        /* Current V plane row pointer */
        pu8_v = pu8_v_data;

        pu8_rgbn = pu8_rgbn_data;

        /* Loop on each column of the output image */
        for (u32_col = u32_width; u32_col != 0 ; u32_col -=2)
        {
            /* Get four RGB 565 samples from input data */
            u16_pix1 = *( (M4VIFI_UInt16 *) pu8_rgbn);
            u16_pix2 = *( (M4VIFI_UInt16 *) (pu8_rgbn + CST_RGB_16_SIZE));
            u16_pix3 = *( (M4VIFI_UInt16 *) (pu8_rgbn + u32_stride_rgb));
            u16_pix4 = *( (M4VIFI_UInt16 *) (pu8_rgbn + u32_stride_rgb + CST_RGB_16_SIZE));

            /* Unpack RGB565 to 8bit R, G, B */
            /* (x,y) */
            GET_RGB565(i32_r00,i32_g00,i32_b00,u16_pix1);
            /* (x+1,y) */
            GET_RGB565(i32_r10,i32_g10,i32_b10,u16_pix2);
            /* (x,y+1) */
            GET_RGB565(i32_r01,i32_g01,i32_b01,u16_pix3);
            /* (x+1,y+1) */
            GET_RGB565(i32_r11,i32_g11,i32_b11,u16_pix4);

            /* Convert RGB value to YUV */
            i32_u00 = U16(i32_r00, i32_g00, i32_b00);
            i32_v00 = V16(i32_r00, i32_g00, i32_b00);
            /* luminance value */
            i32_y00 = Y16(i32_r00, i32_g00, i32_b00);

            i32_u10 = U16(i32_r10, i32_g10, i32_b10);
            i32_v10 = V16(i32_r10, i32_g10, i32_b10);
            /* luminance value */
            i32_y10 = Y16(i32_r10, i32_g10, i32_b10);

            i32_u01 = U16(i32_r01, i32_g01, i32_b01);
            i32_v01 = V16(i32_r01, i32_g01, i32_b01);
            /* luminance value */
            i32_y01 = Y16(i32_r01, i32_g01, i32_b01);

            i32_u11 = U16(i32_r11, i32_g11, i32_b11);
            i32_v11 = V16(i32_r11, i32_g11, i32_b11);
            /* luminance value */
            i32_y11 = Y16(i32_r11, i32_g11, i32_b11);

            /* Store luminance data */
            pu8_yn[0] = (M4VIFI_UInt8)i32_y00;
            pu8_yn[1] = (M4VIFI_UInt8)i32_y10;
            pu8_ys[0] = (M4VIFI_UInt8)i32_y01;
            pu8_ys[1] = (M4VIFI_UInt8)i32_y11;

            /* Store chroma data */
            *pu8_u = (M4VIFI_UInt8)((i32_u00 + i32_u01 + i32_u10 + i32_u11 + 2) >> 2);
            *pu8_v = (M4VIFI_UInt8)((i32_v00 + i32_v01 + i32_v10 + i32_v11 + 2) >> 2);

            /* Prepare for next column */
            pu8_rgbn += (CST_RGB_16_SIZE<<1);
            /* Update current Y plane line pointer*/
            pu8_yn += 2;
            /* Update next Y plane line pointer*/
            pu8_ys += 2;
            /* Update U plane line pointer*/
            pu8_u += 2;
            /* Update V plane line pointer*/
            pu8_v += 2;
        } /* End of horizontal scanning */

        /* Prepare pointers for the next row */
        pu8_y_data += u32_stride2_Y;
        pu8_u_data += u32_stride_UV;
        pu8_v_data += u32_stride_UV;
        pu8_rgbn_data += u32_stride_2rgb;


    } /* End of vertical scanning */

    return M4VIFI_OK;
}


unsigned char M4VFL_modifyLumaWithScale_NV12(M4ViComImagePlane *plane_in,
    M4ViComImagePlane *plane_out, unsigned long lum_factor,
    void *user_data)
{
    unsigned short *p_src, *p_dest, *p_src_line, *p_dest_line;
    unsigned char *p_csrc, *p_cdest, *p_csrc_line, *p_cdest_line;
    unsigned long pix_src;
    unsigned long u_outpx, u_outpx2;
    unsigned long u_width, u_stride, u_stride_out,u_height, pix;
    long i, j;

    /* copy or filter chroma */
    u_width = plane_in[1].u_width;
    u_height = plane_in[1].u_height;
    u_stride = plane_in[1].u_stride;
    u_stride_out = plane_out[1].u_stride;
    p_cdest_line = (unsigned char *) &plane_out[1].pac_data[plane_out[1].u_topleft];
    p_csrc_line = (unsigned char *) &plane_in[1].pac_data[plane_in[1].u_topleft];

    if (lum_factor > 256)
    {
        /* copy chroma */
        for (j = u_height; j != 0; j--)
        {
            memcpy((void *)p_cdest_line, (void *)p_csrc_line, u_width);
            p_cdest_line += u_stride_out;
            p_csrc_line += u_stride;
        }
    }
    else
    {
        /* filter chroma */
        pix = (1024 - lum_factor) << 7;
        for (j = u_height; j != 0; j--)
        {
            p_cdest = p_cdest_line;
            p_csrc = p_csrc_line;
            for (i = u_width; i != 0; i--)
            {
                *p_cdest++ = ((pix + (*p_csrc++ & 0xFF) * lum_factor) >> LUM_FACTOR_MAX);
            }
            p_cdest_line += u_stride_out;
            p_csrc_line += u_stride;
        }
    }

    /* apply luma factor */
    u_width = plane_in[0].u_width;
    u_height = plane_in[0].u_height;
    u_stride = (plane_in[0].u_stride >> 1);
    u_stride_out = (plane_out[0].u_stride >> 1);
    p_dest = (unsigned short *) &plane_out[0].pac_data[plane_out[0].u_topleft];
    p_src = (unsigned short *) &plane_in[0].pac_data[plane_in[0].u_topleft];
    p_dest_line = p_dest;
    p_src_line = p_src;

    for (j = u_height; j != 0; j--)
    {
        p_dest = p_dest_line;
        p_src = p_src_line;
        for (i = (u_width >> 1); i != 0; i--)
        {
            pix_src = (unsigned long) *p_src++;
            pix = pix_src & 0xFF;
            u_outpx = ((pix * lum_factor) >> LUM_FACTOR_MAX);
            pix = ((pix_src & 0xFF00) >> 8);
            u_outpx2 = (((pix * lum_factor) >> LUM_FACTOR_MAX)<< 8) ;
            *p_dest++ = (unsigned short) (u_outpx2 | u_outpx);
        }
        p_dest_line += u_stride_out;
        p_src_line += u_stride;
    }
    return 0;
}

unsigned char M4VIFI_ImageBlendingonNV12 (void *pUserData,
    M4ViComImagePlane *pPlaneIn1, M4ViComImagePlane *pPlaneIn2,
    M4ViComImagePlane *pPlaneOut, UInt32 Progress)
{
    UInt8    *pu8_data_Y_start1, *pu8_data_Y_start2, *pu8_data_Y_start3;
    UInt8    *pu8_data_UV_start1, *pu8_data_UV_start2, *pu8_data_UV_start3;
    UInt8    *pu8_data_Y_current1, *pu8_data_Y_next1;
    UInt8    *pu8_data_Y_current2, *pu8_data_Y_next2;
    UInt8    *pu8_data_Y_current3, *pu8_data_Y_next3;
    UInt8    *pu8_data_UV1, *pu8_data_UV2, *pu8_data_UV3;
    UInt32   u32_stride_Y1, u32_stride2_Y1, u32_stride_UV1;
    UInt32   u32_stride_Y2, u32_stride2_Y2, u32_stride_UV2;
    UInt32   u32_stride_Y3, u32_stride2_Y3, u32_stride_UV3;
    UInt32   u32_height,  u32_width;
    UInt32   u32_blendfactor, u32_startA, u32_endA, u32_blend_inc, u32_x_accum;
    UInt32   u32_col, u32_row, u32_rangeA, u32_progress;
    UInt32   u32_U1,u32_V1,u32_U2,u32_V2, u32_Y1, u32_Y2;

    /* Check the Y plane height is EVEN and image plane heights are same */
    if( (IS_EVEN(pPlaneIn1[0].u_height) == FALSE)                ||
        (IS_EVEN(pPlaneIn2[0].u_height) == FALSE)                ||
        (IS_EVEN(pPlaneOut[0].u_height) == FALSE)                ||
        (pPlaneIn1[0].u_height != pPlaneOut[0].u_height)         ||
        (pPlaneIn2[0].u_height != pPlaneOut[0].u_height) )
    {
        return M4VIFI_ILLEGAL_FRAME_HEIGHT;
    }

    /* Check the Y plane width is EVEN and image plane widths are same */
    if( (IS_EVEN(pPlaneIn1[0].u_width) == FALSE)                 ||
        (IS_EVEN(pPlaneIn2[0].u_width) == FALSE)                 ||
        (IS_EVEN(pPlaneOut[0].u_width) == FALSE)                 ||
        (pPlaneIn1[0].u_width  != pPlaneOut[0].u_width)          ||
        (pPlaneIn2[0].u_width  != pPlaneOut[0].u_width)  )
    {
        return M4VIFI_ILLEGAL_FRAME_WIDTH;
    }
    /* Set the pointer to the beginning of the input1 NV12 image planes */
    pu8_data_Y_start1 = pPlaneIn1[0].pac_data + pPlaneIn1[0].u_topleft;
    pu8_data_UV_start1 = pPlaneIn1[1].pac_data + pPlaneIn1[1].u_topleft;

    /* Set the pointer to the beginning of the input2 NV12 image planes */
    pu8_data_Y_start2 = pPlaneIn2[0].pac_data + pPlaneIn2[0].u_topleft;
    pu8_data_UV_start2 = pPlaneIn2[1].pac_data + pPlaneIn2[1].u_topleft;

    /* Set the pointer to the beginning of the output NV12 image planes */
    pu8_data_Y_start3 = pPlaneOut[0].pac_data + pPlaneOut[0].u_topleft;
    pu8_data_UV_start3 = pPlaneOut[1].pac_data + pPlaneOut[1].u_topleft;

    /* Set the stride for the next row in each input1 NV12 plane */
    u32_stride_Y1 = pPlaneIn1[0].u_stride;
    u32_stride_UV1 = pPlaneIn1[1].u_stride;

    /* Set the stride for the next row in each input2 NV12 plane */
    u32_stride_Y2 = pPlaneIn2[0].u_stride;
    u32_stride_UV2 = pPlaneIn2[1].u_stride;

    /* Set the stride for the next row in each output NV12 plane */
    u32_stride_Y3 = pPlaneOut[0].u_stride;
    u32_stride_UV3 = pPlaneOut[1].u_stride;

    u32_stride2_Y1   = u32_stride_Y1 << 1;
    u32_stride2_Y2   = u32_stride_Y2 << 1;
    u32_stride2_Y3   = u32_stride_Y3 << 1;

    /* Get the size of the output image */
    u32_height = pPlaneOut[0].u_height;
    u32_width  = pPlaneOut[0].u_width;

    /* User Specified Progress value */
    u32_progress = Progress;

    /* Map Progress value from (0 - 1000) to (0 - 1024) -> for optimisation */
    if(u32_progress < 1000)
        u32_progress = ((u32_progress << 10) / 1000);
    else
        u32_progress = 1024;

    /* Set the range of blendingfactor */
    if(u32_progress <= 512)
    {
        u32_startA = 0;
        u32_endA   = (u32_progress << 1);
    }
    else /* u32_progress > 512 */
    {
        u32_startA = (u32_progress - 512) << 1;
        u32_endA   =  1024;
    }
    u32_rangeA = u32_endA - u32_startA;

    /* Set the increment of blendingfactor for each element in the image row */
    if ((u32_width >= u32_rangeA) && (u32_rangeA > 0) )
    {
        u32_blend_inc   = ((u32_rangeA-1) * MAX_SHORT) / (u32_width - 1);
    }
    else /* (u32_width < u32_rangeA) || (u32_rangeA < 0) */
    {
        u32_blend_inc   = (u32_rangeA * MAX_SHORT) / (u32_width);
    }
    /* Two YUV420 rows are computed at each pass */
    for (u32_row = u32_height; u32_row != 0; u32_row -=2)
    {
        /* Set pointers to the beginning of the row for each input image1 plane */
        pu8_data_Y_current1 = pu8_data_Y_start1;
        pu8_data_UV1 = pu8_data_UV_start1;

        /* Set pointers to the beginning of the row for each input image2 plane */
        pu8_data_Y_current2 = pu8_data_Y_start2;
        pu8_data_UV2 = pu8_data_UV_start2;

        /* Set pointers to the beginning of the row for each output image plane */
        pu8_data_Y_current3 = pu8_data_Y_start3;
        pu8_data_UV3 = pu8_data_UV_start3;

        /* Set pointers to the beginning of the next row for image luma plane */
        pu8_data_Y_next1 = pu8_data_Y_current1 + u32_stride_Y1;
        pu8_data_Y_next2 = pu8_data_Y_current2 + u32_stride_Y2;
        pu8_data_Y_next3 = pu8_data_Y_current3 + u32_stride_Y3;

        /* Initialise blendfactor */
        u32_blendfactor   = u32_startA;
        /* Blendfactor Increment accumulator */
        u32_x_accum = 0;

        /* Loop on each column of the output image */
        for (u32_col = u32_width; u32_col != 0 ; u32_col -=2)
        {
            /* Update the blending factor */
            u32_blendfactor = u32_startA + (u32_x_accum >> 16);

            /* Get Luma value (x,y) of input Image1 */
            u32_Y1 = *pu8_data_Y_current1++;

            /* Get chrominance2 value */
            u32_U1 = *pu8_data_UV1++;
            u32_V1 = *pu8_data_UV1++;

            /* Get Luma value (x,y) of input Image2 */
            u32_Y2 = *pu8_data_Y_current2++;

            /* Get chrominance2 value */
            u32_U2 = *pu8_data_UV2++;
            u32_V2 = *pu8_data_UV2++;

            /* Compute Luma value (x,y) of Output image */
            *pu8_data_Y_current3++  = (UInt8)((u32_blendfactor * u32_Y2 +
                                        (1024 - u32_blendfactor)*u32_Y1) >> 10);
            /* Compute chroma(U) value of Output image */
            *pu8_data_UV3++         = (UInt8)((u32_blendfactor * u32_U2 +
                                        (1024 - u32_blendfactor)*u32_U1) >> 10);
            /* Compute chroma(V) value of Output image */
            *pu8_data_UV3++         = (UInt8)((u32_blendfactor * u32_V2 +
                                        (1024 - u32_blendfactor)*u32_V1) >> 10);

            /* Get Luma value (x,y+1) of input Image1 */
            u32_Y1 = *pu8_data_Y_next1++;

             /* Get Luma value (x,y+1) of input Image2 */
            u32_Y2 = *pu8_data_Y_next2++;

            /* Compute Luma value (x,y+1) of Output image*/
            *pu8_data_Y_next3++ = (UInt8)((u32_blendfactor * u32_Y2 +
                                    (1024 - u32_blendfactor)*u32_Y1) >> 10);
            /* Update accumulator */
            u32_x_accum += u32_blend_inc;

            /* Update the blending factor */
            u32_blendfactor = u32_startA + (u32_x_accum >> 16);

            /* Get Luma value (x+1,y) of input Image1 */
            u32_Y1 = *pu8_data_Y_current1++;

            /* Get Luma value (x+1,y) of input Image2 */
            u32_Y2 = *pu8_data_Y_current2++;

            /* Compute Luma value (x+1,y) of Output image*/
            *pu8_data_Y_current3++ = (UInt8)((u32_blendfactor * u32_Y2 +
                                       (1024 - u32_blendfactor)*u32_Y1) >> 10);

            /* Get Luma value (x+1,y+1) of input Image1 */
            u32_Y1 = *pu8_data_Y_next1++;

            /* Get Luma value (x+1,y+1) of input Image2 */
            u32_Y2 = *pu8_data_Y_next2++;

            /* Compute Luma value (x+1,y+1) of Output image*/
            *pu8_data_Y_next3++ = (UInt8)((u32_blendfactor * u32_Y2 +
                                    (1024 - u32_blendfactor)*u32_Y1) >> 10);
            /* Update accumulator */
            u32_x_accum += u32_blend_inc;

            /* Working pointers are incremented just after each storage */

        }/* End of row scanning */

        /* Update working pointer of input image1 for next row */
        pu8_data_Y_start1 += u32_stride2_Y1;
        pu8_data_UV_start1 += u32_stride_UV1;

        /* Update working pointer of input image2 for next row */
        pu8_data_Y_start2 += u32_stride2_Y2;
        pu8_data_UV_start2 += u32_stride_UV2;

        /* Update working pointer of output image for next row */
        pu8_data_Y_start3 += u32_stride2_Y3;
        pu8_data_UV_start3 += u32_stride_UV3;

    }/* End of column scanning */

    return M4VIFI_OK;
}


/**
 ******************************************************************************
 * M4VIFI_UInt8 M4VIFI_RGB565toNV12 (void *pUserData,
 *                                     M4VIFI_ImagePlane *pPlaneIn,
 *                                   M4VIFI_ImagePlane *pPlaneOut)
 * @brief   transform RGB565 image to a NV12 image.
 * @note    Convert RGB565 to NV12,
 *          Loop on each row ( 2 rows by 2 rows )
 *              Loop on each column ( 2 col by 2 col )
 *                  Get 4 RGB samples from input data and build 4 output Y samples
 *                  and each single U & V data
 *              end loop on col
 *          end loop on row
 * @param   pUserData: (IN) User Specific Data
 * @param   pPlaneIn: (IN) Pointer to RGB565 Plane
 * @param   pPlaneOut: (OUT) Pointer to  NV12 buffer Plane
 * @return  M4VIFI_OK: there is no error
 * @return  M4VIFI_ILLEGAL_FRAME_HEIGHT: YUV Plane height is ODD
 * @return  M4VIFI_ILLEGAL_FRAME_WIDTH:  YUV Plane width is ODD
 ******************************************************************************
*/
M4VIFI_UInt8    M4VIFI_xVSS_RGB565toNV12(void *pUserData, M4VIFI_ImagePlane *pPlaneIn,
                                                      M4VIFI_ImagePlane *pPlaneOut)
{
    M4VIFI_UInt32   u32_width, u32_height;
    M4VIFI_UInt32   u32_stride_Y, u32_stride2_Y, u32_stride_UV;
    M4VIFI_UInt32   u32_stride_rgb, u32_stride_2rgb;
    M4VIFI_UInt32   u32_col, u32_row;

    M4VIFI_Int32    i32_r00, i32_r01, i32_r10, i32_r11;
    M4VIFI_Int32    i32_g00, i32_g01, i32_g10, i32_g11;
    M4VIFI_Int32    i32_b00, i32_b01, i32_b10, i32_b11;
    M4VIFI_Int32    i32_y00, i32_y01, i32_y10, i32_y11;
    M4VIFI_Int32    i32_u00, i32_u01, i32_u10, i32_u11;
    M4VIFI_Int32    i32_v00, i32_v01, i32_v10, i32_v11;
    M4VIFI_UInt8    *pu8_yn, *pu8_ys, *pu8_u, *pu8_v;
    M4VIFI_UInt8    *pu8_y_data, *pu8_u_data, *pu8_v_data;
    M4VIFI_UInt8    *pu8_rgbn_data, *pu8_rgbn;
    M4VIFI_UInt16   u16_pix1, u16_pix2, u16_pix3, u16_pix4;
    M4VIFI_UInt8    count_null=0;

    /* Check planes height are appropriate */
    if( (pPlaneIn->u_height != pPlaneOut[0].u_height)           ||
        (pPlaneOut[0].u_height != (pPlaneOut[1].u_height<<1)))
    {
        return M4VIFI_ILLEGAL_FRAME_HEIGHT;
    }

    /* Check planes width are appropriate */
    if( (pPlaneIn->u_width != pPlaneOut[0].u_width)         ||
        (pPlaneOut[0].u_width != pPlaneOut[1].u_width))
    {
        return M4VIFI_ILLEGAL_FRAME_WIDTH;
    }

    /* Set the pointer to the beginning of the output data buffers */
    pu8_y_data = pPlaneOut[0].pac_data + pPlaneOut[0].u_topleft;
    pu8_u_data = pPlaneOut[1].pac_data + pPlaneOut[1].u_topleft;
    pu8_v_data = pu8_u_data + 1;

    /* Set the pointer to the beginning of the input data buffers */
    pu8_rgbn_data   = pPlaneIn->pac_data + pPlaneIn->u_topleft;

    /* Get the size of the output image */
    u32_width = pPlaneOut[0].u_width;
    u32_height = pPlaneOut[0].u_height;

    /* Set the size of the memory jumps corresponding to row jump in each output plane */
    u32_stride_Y = pPlaneOut[0].u_stride;
    u32_stride2_Y = u32_stride_Y << 1;
    u32_stride_UV = pPlaneOut[1].u_stride;

    /* Set the size of the memory jumps corresponding to row jump in input plane */
    u32_stride_rgb = pPlaneIn->u_stride;
    u32_stride_2rgb = u32_stride_rgb << 1;

    /* Loop on each row of the output image, input coordinates are estimated from output ones */
    /* Two YUV rows are computed at each pass */
    for (u32_row = u32_height ;u32_row != 0; u32_row -=2)
    {
        /* Current Y plane row pointers */
        pu8_yn = pu8_y_data;
        /* Next Y plane row pointers */
        pu8_ys = pu8_yn + u32_stride_Y;
        /* Current U plane row pointer */
        pu8_u = pu8_u_data;
        /* Current V plane row pointer */
        pu8_v = pu8_v_data;

        pu8_rgbn = pu8_rgbn_data;

        /* Loop on each column of the output image */
        for (u32_col = u32_width; u32_col != 0 ; u32_col -=2)
        {
            /* Get four RGB 565 samples from input data */
            u16_pix1 = *( (M4VIFI_UInt16 *) pu8_rgbn);
            u16_pix2 = *( (M4VIFI_UInt16 *) (pu8_rgbn + CST_RGB_16_SIZE));
            u16_pix3 = *( (M4VIFI_UInt16 *) (pu8_rgbn + u32_stride_rgb));
            u16_pix4 = *( (M4VIFI_UInt16 *) (pu8_rgbn + u32_stride_rgb + CST_RGB_16_SIZE));

            /* Unpack RGB565 to 8bit R, G, B */
            /* (x,y) */
            GET_RGB565(i32_b00,i32_g00,i32_r00,u16_pix1);
            /* (x+1,y) */
            GET_RGB565(i32_b10,i32_g10,i32_r10,u16_pix2);
            /* (x,y+1) */
            GET_RGB565(i32_b01,i32_g01,i32_r01,u16_pix3);
            /* (x+1,y+1) */
            GET_RGB565(i32_b11,i32_g11,i32_r11,u16_pix4);
            /* If RGB is transparent color (0, 63, 0), we transform it to white (31,63,31) */
            if(i32_b00 == 0 && i32_g00 == 63 && i32_r00 == 0)
            {
                i32_b00 = 31;
                i32_r00 = 31;
            }
            if(i32_b10 == 0 && i32_g10 == 63 && i32_r10 == 0)
            {
                i32_b10 = 31;
                i32_r10 = 31;
            }
            if(i32_b01 == 0 && i32_g01 == 63 && i32_r01 == 0)
            {
                i32_b01 = 31;
                i32_r01 = 31;
            }
            if(i32_b11 == 0 && i32_g11 == 63 && i32_r11 == 0)
            {
                i32_b11 = 31;
                i32_r11 = 31;
            }
            /* Convert RGB value to YUV */
            i32_u00 = U16(i32_r00, i32_g00, i32_b00);
            i32_v00 = V16(i32_r00, i32_g00, i32_b00);
            /* luminance value */
            i32_y00 = Y16(i32_r00, i32_g00, i32_b00);

            i32_u10 = U16(i32_r10, i32_g10, i32_b10);
            i32_v10 = V16(i32_r10, i32_g10, i32_b10);
            /* luminance value */
            i32_y10 = Y16(i32_r10, i32_g10, i32_b10);

            i32_u01 = U16(i32_r01, i32_g01, i32_b01);
            i32_v01 = V16(i32_r01, i32_g01, i32_b01);
            /* luminance value */
            i32_y01 = Y16(i32_r01, i32_g01, i32_b01);

            i32_u11 = U16(i32_r11, i32_g11, i32_b11);
            i32_v11 = V16(i32_r11, i32_g11, i32_b11);
            /* luminance value */
            i32_y11 = Y16(i32_r11, i32_g11, i32_b11);
            /* Store luminance data */
            pu8_yn[0] = (M4VIFI_UInt8)i32_y00;
            pu8_yn[1] = (M4VIFI_UInt8)i32_y10;
            pu8_ys[0] = (M4VIFI_UInt8)i32_y01;
            pu8_ys[1] = (M4VIFI_UInt8)i32_y11;
            *pu8_u = (M4VIFI_UInt8)((i32_u00 + i32_u01 + i32_u10 + i32_u11 + 2) >> 2);
            *pu8_v = (M4VIFI_UInt8)((i32_v00 + i32_v01 + i32_v10 + i32_v11 + 2) >> 2);
            /* Prepare for next column */
            pu8_rgbn += (CST_RGB_16_SIZE<<1);
            /* Update current Y plane line pointer*/
            pu8_yn += 2;
            /* Update next Y plane line pointer*/
            pu8_ys += 2;
            /* Update U plane line pointer*/
            pu8_u += 2;
            /* Update V plane line pointer*/
            pu8_v += 2;
        } /* End of horizontal scanning */

        /* Prepare pointers for the next row */
        pu8_y_data += u32_stride2_Y;
        pu8_u_data += u32_stride_UV;
        pu8_v_data += u32_stride_UV;
        pu8_rgbn_data += u32_stride_2rgb;


    } /* End of vertical scanning */

    return M4VIFI_OK;
}


/**
 ******************************************************************************
 * M4OSA_ERR M4xVSS_internalConvertAndResizeARGB8888toNV12(M4OSA_Void* pFileIn,
 *                                             M4OSA_FileReadPointer* pFileReadPtr,
 *                                                M4VIFI_ImagePlane* pImagePlanes,
 *                                                 M4OSA_UInt32 width,
 *                                                M4OSA_UInt32 height);
 * @brief    It Coverts and resizes a ARGB8888 image to NV12
 * @note
 * @param    pFileIn            (IN) The Image input file
 * @param    pFileReadPtr    (IN) Pointer on filesystem functions
 * @param    pImagePlanes    (IN/OUT) Pointer on NV12 output planes allocated by the user
 *                            ARGB8888 image  will be converted and resized  to output
 *                             NV12 plane size
 *@param    width        (IN) width of the ARGB8888
 *@param    height            (IN) height of the ARGB8888
 * @return    M4NO_ERROR:    No error
 * @return    M4ERR_ALLOC: memory error
 * @return    M4ERR_PARAMETER: At least one of the function parameters is null
 ******************************************************************************
 */

M4OSA_ERR M4xVSS_internalConvertAndResizeARGB8888toNV12(M4OSA_Void* pFileIn,
    M4OSA_FileReadPointer* pFileReadPtr, M4VIFI_ImagePlane* pImagePlanes,
    M4OSA_UInt32 width,M4OSA_UInt32 height)
{
    M4OSA_Context pARGBIn;
    M4VIFI_ImagePlane rgbPlane1 ,rgbPlane2;
    M4OSA_UInt32 frameSize_argb=(width * height * 4);
    M4OSA_UInt32 frameSize = (width * height * 3); //Size of RGB888 data.
    M4OSA_UInt32 i = 0,j= 0;
    M4OSA_ERR err=M4NO_ERROR;


    M4OSA_UInt8 *pTmpData = (M4OSA_UInt8*) M4OSA_32bitAlignedMalloc(frameSize_argb,
         M4VS, (M4OSA_Char*)"Image argb data");
        M4OSA_TRACE1_0("M4xVSS_internalConvertAndResizeARGB8888toNV12 Entering :");
    if(pTmpData == M4OSA_NULL) {
        M4OSA_TRACE1_0("M4xVSS_internalConvertAndResizeARGB8888toNV12 :\
            Failed to allocate memory for Image clip");
        return M4ERR_ALLOC;
    }

    M4OSA_TRACE1_2("M4xVSS_internalConvertAndResizeARGB8888toNV12 :width and height %d %d",
        width ,height);
    /* Get file size (mandatory for chunk decoding) */
    err = pFileReadPtr->openRead(&pARGBIn, pFileIn, M4OSA_kFileRead);
    if(err != M4NO_ERROR)
    {
        M4OSA_TRACE1_2("M4xVSS_internalConvertAndResizeARGB8888toNV12 :\
            Can't open input ARGB8888 file %s, error: 0x%x\n",pFileIn, err);
        free(pTmpData);
        pTmpData = M4OSA_NULL;
        goto cleanup;
    }

    err = pFileReadPtr->readData(pARGBIn,(M4OSA_MemAddr8)pTmpData, &frameSize_argb);
    if(err != M4NO_ERROR)
    {
        M4OSA_TRACE1_2("M4xVSS_internalConvertAndResizeARGB8888toNV12 Can't close ARGB8888\
             file %s, error: 0x%x\n",pFileIn, err);
        pFileReadPtr->closeRead(pARGBIn);
        free(pTmpData);
        pTmpData = M4OSA_NULL;
        goto cleanup;
    }

    err = pFileReadPtr->closeRead(pARGBIn);
    if(err != M4NO_ERROR)
    {
        M4OSA_TRACE1_2("M4xVSS_internalConvertAndResizeARGB8888toNV12 Can't close ARGB8888 \
             file %s, error: 0x%x\n",pFileIn, err);
        free(pTmpData);
        pTmpData = M4OSA_NULL;
        goto cleanup;
    }

    rgbPlane1.pac_data = (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(frameSize, M4VS,
         (M4OSA_Char*)"Image clip RGB888 data");
    if(rgbPlane1.pac_data == M4OSA_NULL)
    {
        M4OSA_TRACE1_0("M4xVSS_internalConvertAndResizeARGB8888toNV12 \
            Failed to allocate memory for Image clip");
        free(pTmpData);
        return M4ERR_ALLOC;
    }

        rgbPlane1.u_height = height;
        rgbPlane1.u_width = width;
        rgbPlane1.u_stride = width*3;
        rgbPlane1.u_topleft = 0;


    /** Remove the alpha channel */
    for (i=0, j = 0; i < frameSize_argb; i++) {
        if ((i % 4) == 0) continue;
        rgbPlane1.pac_data[j] = pTmpData[i];
        j++;
    }
    free(pTmpData);
    pTmpData = M4OSA_NULL;

    /* To Check if resizing is required with color conversion */
    if(width != pImagePlanes->u_width || height != pImagePlanes->u_height)
    {
        M4OSA_TRACE1_0("M4xVSS_internalConvertAndResizeARGB8888toNV12 Resizing :");
        frameSize =  ( pImagePlanes->u_width * pImagePlanes->u_height * 3);
        rgbPlane2.pac_data = (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(frameSize, M4VS,
             (M4OSA_Char*)"Image clip RGB888 data");
        if(rgbPlane2.pac_data == M4OSA_NULL)
        {
            M4OSA_TRACE1_0("Failed to allocate memory for Image clip");
            return M4ERR_ALLOC;
        }

        rgbPlane2.u_height =  pImagePlanes->u_height;
        rgbPlane2.u_width = pImagePlanes->u_width;
        rgbPlane2.u_stride = pImagePlanes->u_width*3;
        rgbPlane2.u_topleft = 0;

        /* Resizing RGB888 to RGB888 */
        err = M4VIFI_ResizeBilinearRGB888toRGB888(M4OSA_NULL, &rgbPlane1, &rgbPlane2);
        if(err != M4NO_ERROR)
        {
            M4OSA_TRACE1_1("error when converting from Resize RGB888 to RGB888: 0x%x\n", err);
            free(rgbPlane2.pac_data);
            free(rgbPlane1.pac_data);
            return err;
        }
        /*Converting Resized RGB888 to YUV420 */
        err = M4VIFI_RGB888toNV12(M4OSA_NULL, &rgbPlane2, pImagePlanes);
        if(err != M4NO_ERROR)
        {
            M4OSA_TRACE1_1("error when converting from RGB888 to NV12: 0x%x\n", err);
            free(rgbPlane2.pac_data);
            free(rgbPlane1.pac_data);
            return err;
        }
            free(rgbPlane2.pac_data);
            free(rgbPlane1.pac_data);

            M4OSA_TRACE1_0("RGB to YUV done");


    }
    else
    {
        M4OSA_TRACE1_0("M4xVSS_internalConvertAndResizeARGB8888toNV12 NO  Resizing :");
        err = M4VIFI_RGB888toNV12(M4OSA_NULL, &rgbPlane1, pImagePlanes);
        if(err != M4NO_ERROR)
        {
            M4OSA_TRACE1_1("error when converting from RGB to YUV: 0x%x\n", err);
        }
            free(rgbPlane1.pac_data);

            M4OSA_TRACE1_0("RGB to YUV done");
    }
cleanup:
    M4OSA_TRACE1_0("M4xVSS_internalConvertAndResizeARGB8888toNV12 leaving :");
    return err;
}


/**
 ******************************************************************************
 * M4OSA_ERR M4xVSS_internalConvertARGB8888toNV12(M4OSA_Void* pFileIn,
 *                                             M4OSA_FileReadPointer* pFileReadPtr,
 *                                                M4VIFI_ImagePlane* pImagePlanes,
 *                                                 M4OSA_UInt32 width,
 *                                                M4OSA_UInt32 height);
 * @brief    It Coverts a ARGB8888 image to NV12
 * @note
 * @param    pFileIn            (IN) The Image input file
 * @param    pFileReadPtr    (IN) Pointer on filesystem functions
 * @param    pImagePlanes    (IN/OUT) Pointer on NV12 output planes allocated by the user
 *                            ARGB8888 image  will be converted and resized  to output
 *                            NV12 plane size
 * @param    width        (IN) width of the ARGB8888
 * @param    height            (IN) height of the ARGB8888
 * @return    M4NO_ERROR:    No error
 * @return    M4ERR_ALLOC: memory error
 * @return    M4ERR_PARAMETER: At least one of the function parameters is null
 ******************************************************************************
 */

M4OSA_ERR M4xVSS_internalConvertARGB8888toNV12(M4OSA_Void* pFileIn,
    M4OSA_FileReadPointer* pFileReadPtr, M4VIFI_ImagePlane** pImagePlanes,
    M4OSA_UInt32 width, M4OSA_UInt32 height)
{
    M4OSA_ERR err = M4NO_ERROR;
    M4VIFI_ImagePlane *yuvPlane = M4OSA_NULL;

    yuvPlane = (M4VIFI_ImagePlane*)M4OSA_32bitAlignedMalloc(2*sizeof(M4VIFI_ImagePlane),
                M4VS, (M4OSA_Char*)"M4xVSS_internalConvertRGBtoNV12: Output plane NV12");
    if(yuvPlane == M4OSA_NULL) {
        M4OSA_TRACE1_0("M4xVSS_internalConvertARGB8888toNV12:\
            Failed to allocate memory for Image clip");
        return M4ERR_ALLOC;
    }
    yuvPlane[0].u_height = height;
    yuvPlane[0].u_width = width;
    yuvPlane[0].u_stride = width;
    yuvPlane[0].u_topleft = 0;
    yuvPlane[0].pac_data = (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(yuvPlane[0].u_height \
        * yuvPlane[0].u_width * 1.5, M4VS, (M4OSA_Char*)"imageClip YUV data");

    if(yuvPlane[0].pac_data == M4OSA_NULL) {
        M4OSA_TRACE1_0("M4xVSS_internalConvertARGB8888toNV12 \
            Failed to allocate memory for Image clip");
        free(yuvPlane);
        return M4ERR_ALLOC;
    }

    yuvPlane[1].u_height = yuvPlane[0].u_height >>1;
    yuvPlane[1].u_width = yuvPlane[0].u_width;
    yuvPlane[1].u_stride = yuvPlane[1].u_width;
    yuvPlane[1].u_topleft = 0;
    yuvPlane[1].pac_data = (M4VIFI_UInt8*)(yuvPlane[0].pac_data + yuvPlane[0].u_height \
        * yuvPlane[0].u_width);

    err = M4xVSS_internalConvertAndResizeARGB8888toNV12(pFileIn,pFileReadPtr,
                                                          yuvPlane, width, height);
    if(err != M4NO_ERROR)
    {
        M4OSA_TRACE1_1("M4xVSS_internalConvertAndResizeARGB8888toNV12 return error: 0x%x\n", err);
        free(yuvPlane);
        return err;
    }

    *pImagePlanes = yuvPlane;

    M4OSA_TRACE1_0("M4xVSS_internalConvertARGB8888toNV12: Leaving");
    return err;

}

/**
 ******************************************************************************
 * M4OSA_ERR M4xVSS_PictureCallbackFct_NV12 (M4OSA_Void* pPictureCtxt,
 *                                        M4VIFI_ImagePlane* pImagePlanes,
 *                                        M4OSA_UInt32* pPictureDuration);
 * @brief    It feeds the PTO3GPP with NV12 pictures.
 * @note    This function is given to the PTO3GPP in the M4PTO3GPP_Params structure
 * @param    pContext    (IN) The integrator own context
 * @param    pImagePlanes(IN/OUT) Pointer to an array of three valid image planes
 * @param    pPictureDuration(OUT) Duration of the returned picture
 *
 * @return    M4NO_ERROR:    No error
 * @return    M4PTO3GPP_WAR_LAST_PICTURE: The returned image is the last one
 * @return    M4ERR_PARAMETER: At least one of the function parameters is null
 ******************************************************************************
 */
M4OSA_ERR M4xVSS_PictureCallbackFct_NV12(M4OSA_Void* pPictureCtxt,
    M4VIFI_ImagePlane* pImagePlanes, M4OSA_Double*  pPictureDuration)
{
    M4OSA_ERR err = M4NO_ERROR;
    M4OSA_UInt8    last_frame_flag = 0;
    M4xVSS_PictureCallbackCtxt* pC = (M4xVSS_PictureCallbackCtxt*) (pPictureCtxt);

    /*Used for pan&zoom*/
    M4OSA_UInt8 tempPanzoomXa = 0;
    M4OSA_UInt8 tempPanzoomXb = 0;
    M4AIR_Params Params;
    /**/

    /*Used for cropping and black borders*/
    M4OSA_Context    pPictureContext = M4OSA_NULL;
    M4OSA_FilePosition    pictureSize = 0 ;
    M4OSA_UInt8*    pictureBuffer = M4OSA_NULL;
    //M4EXIFC_Context pExifContext = M4OSA_NULL;
    M4EXIFC_BasicTags pBasicTags;
    M4VIFI_ImagePlane pImagePlanes1 = pImagePlanes[0];
    M4VIFI_ImagePlane pImagePlanes2 = pImagePlanes[1];

    /**/

    /**
     * Check input parameters */
    M4OSA_DEBUG_IF2((M4OSA_NULL==pPictureCtxt),        M4ERR_PARAMETER,
         "M4xVSS_PictureCallbackFct_NV12: pPictureCtxt is M4OSA_NULL");
    M4OSA_DEBUG_IF2((M4OSA_NULL==pImagePlanes),        M4ERR_PARAMETER,
         "M4xVSS_PictureCallbackFct_NV12: pImagePlanes is M4OSA_NULL");
    M4OSA_DEBUG_IF2((M4OSA_NULL==pPictureDuration), M4ERR_PARAMETER,
         "M4xVSS_PictureCallbackFct_NV12: pPictureDuration is M4OSA_NULL");
    M4OSA_TRACE1_0("M4xVSS_PictureCallbackFct :Entering");
    /*PR P4ME00003181 In case the image number is 0, pan&zoom can not be used*/
    if(M4OSA_TRUE == pC->m_pPto3GPPparams->isPanZoom && pC->m_NbImage == 0)
    {
        pC->m_pPto3GPPparams->isPanZoom = M4OSA_FALSE;
    }

    /*If no cropping/black borders or pan&zoom, just decode and resize the picture*/
    if(pC->m_mediaRendering == M4xVSS_kResizing && M4OSA_FALSE == pC->m_pPto3GPPparams->isPanZoom)
    {
        /**
         * Convert and resize input ARGB8888 file to NV12 */
        /*To support ARGB8888 : */
        M4OSA_TRACE1_2("M4xVSS_PictureCallbackFct_NV12 1: width and height %d %d",
            pC->m_pPto3GPPparams->width,pC->m_pPto3GPPparams->height);
        err = M4xVSS_internalConvertAndResizeARGB8888toNV12(pC->m_FileIn,
             pC->m_pFileReadPtr, pImagePlanes,pC->m_pPto3GPPparams->width,
                pC->m_pPto3GPPparams->height);
        if(err != M4NO_ERROR)
        {
            M4OSA_TRACE1_1("M4xVSS_PictureCallbackFct_NV12: Error when decoding JPEG: 0x%x\n", err);
            return err;
        }
    }
    /*In case of cropping, black borders or pan&zoom, call the EXIF reader and the AIR*/
    else
    {
        /**
         * Computes ratios */
        if(pC->m_pDecodedPlane == M4OSA_NULL)
        {
            /**
             * Convert input ARGB8888 file to NV12 */
             M4OSA_TRACE1_2("M4xVSS_PictureCallbackFct_NV12 2: width and height %d %d",
                pC->m_pPto3GPPparams->width,pC->m_pPto3GPPparams->height);
            err = M4xVSS_internalConvertARGB8888toNV12(pC->m_FileIn, pC->m_pFileReadPtr,
                &(pC->m_pDecodedPlane),pC->m_pPto3GPPparams->width,pC->m_pPto3GPPparams->height);
            if(err != M4NO_ERROR)
            {
                M4OSA_TRACE1_1("M4xVSS_PictureCallbackFct_NV12: Error when decoding JPEG: 0x%x\n", err);
                if(pC->m_pDecodedPlane != M4OSA_NULL)
                {
                    /* NV12 planar is returned but allocation is made only once
                        (contigous planes in memory) */
                    if(pC->m_pDecodedPlane->pac_data != M4OSA_NULL)
                    {
                        free(pC->m_pDecodedPlane->pac_data);
                    }
                    free(pC->m_pDecodedPlane);
                    pC->m_pDecodedPlane = M4OSA_NULL;
                }
                return err;
            }
        }

        /*Initialize AIR Params*/
        Params.m_inputCoord.m_x = 0;
        Params.m_inputCoord.m_y = 0;
        Params.m_inputSize.m_height = pC->m_pDecodedPlane->u_height;
        Params.m_inputSize.m_width = pC->m_pDecodedPlane->u_width;
        Params.m_outputSize.m_width = pImagePlanes->u_width;
        Params.m_outputSize.m_height = pImagePlanes->u_height;
        Params.m_bOutputStripe = M4OSA_FALSE;
        Params.m_outputOrientation = M4COMMON_kOrientationTopLeft;

        /*Initialize Exif params structure*/
        pBasicTags.orientation = M4COMMON_kOrientationUnknown;

        /**
        Pan&zoom params*/
        if(M4OSA_TRUE == pC->m_pPto3GPPparams->isPanZoom)
        {
            /*Save ratio values, they can be reused if the new ratios are 0*/
            tempPanzoomXa = (M4OSA_UInt8)pC->m_pPto3GPPparams->PanZoomXa;
            tempPanzoomXb = (M4OSA_UInt8)pC->m_pPto3GPPparams->PanZoomXb;

            /*Check that the ratio is not 0*/
            /*Check (a) parameters*/
            if(pC->m_pPto3GPPparams->PanZoomXa == 0)
            {
                M4OSA_UInt8 maxRatio = 0;
                if(pC->m_pPto3GPPparams->PanZoomTopleftXa >=
                     pC->m_pPto3GPPparams->PanZoomTopleftYa)
                {
                    /*The ratio is 0, that means the area of the picture defined with (a)
                    parameters is bigger than the image size*/
                    if(pC->m_pPto3GPPparams->PanZoomTopleftXa + tempPanzoomXa > 1000)
                    {
                        /*The oversize is maxRatio*/
                        maxRatio = pC->m_pPto3GPPparams->PanZoomTopleftXa + tempPanzoomXa - 1000;
                    }
                }
                else
                {
                    /*The ratio is 0, that means the area of the picture defined with (a)
                     parameters is bigger than the image size*/
                    if(pC->m_pPto3GPPparams->PanZoomTopleftYa + tempPanzoomXa > 1000)
                    {
                        /*The oversize is maxRatio*/
                        maxRatio = pC->m_pPto3GPPparams->PanZoomTopleftYa + tempPanzoomXa - 1000;
                    }
                }
                /*Modify the (a) parameters:*/
                if(pC->m_pPto3GPPparams->PanZoomTopleftXa >= maxRatio)
                {
                    /*The (a) topleft parameters can be moved to keep the same area size*/
                    pC->m_pPto3GPPparams->PanZoomTopleftXa -= maxRatio;
                }
                else
                {
                    /*Move the (a) topleft parameter to 0 but the ratio will be also further
                    modified to match the image size*/
                    pC->m_pPto3GPPparams->PanZoomTopleftXa = 0;
                }
                if(pC->m_pPto3GPPparams->PanZoomTopleftYa >= maxRatio)
                {
                    /*The (a) topleft parameters can be moved to keep the same area size*/
                    pC->m_pPto3GPPparams->PanZoomTopleftYa -= maxRatio;
                }
                else
                {
                    /*Move the (a) topleft parameter to 0 but the ratio will be also further
                     modified to match the image size*/
                    pC->m_pPto3GPPparams->PanZoomTopleftYa = 0;
                }
                /*The new ratio is the original one*/
                pC->m_pPto3GPPparams->PanZoomXa = tempPanzoomXa;
                if(pC->m_pPto3GPPparams->PanZoomXa + pC->m_pPto3GPPparams->PanZoomTopleftXa > 1000)
                {
                    /*Change the ratio if the area of the picture defined with (a) parameters is
                    bigger than the image size*/
                    pC->m_pPto3GPPparams->PanZoomXa = 1000 - pC->m_pPto3GPPparams->PanZoomTopleftXa;
                }
                if(pC->m_pPto3GPPparams->PanZoomXa + pC->m_pPto3GPPparams->PanZoomTopleftYa > 1000)
                {
                    /*Change the ratio if the area of the picture defined with (a) parameters is
                    bigger than the image size*/
                    pC->m_pPto3GPPparams->PanZoomXa = 1000 - pC->m_pPto3GPPparams->PanZoomTopleftYa;
                }
            }
            /*Check (b) parameters*/
            if(pC->m_pPto3GPPparams->PanZoomXb == 0)
            {
                M4OSA_UInt8 maxRatio = 0;
                if(pC->m_pPto3GPPparams->PanZoomTopleftXb >=
                     pC->m_pPto3GPPparams->PanZoomTopleftYb)
                {
                    /*The ratio is 0, that means the area of the picture defined with (b)
                     parameters is bigger than the image size*/
                    if(pC->m_pPto3GPPparams->PanZoomTopleftXb + tempPanzoomXb > 1000)
                    {
                        /*The oversize is maxRatio*/
                        maxRatio = pC->m_pPto3GPPparams->PanZoomTopleftXb + tempPanzoomXb - 1000;
                    }
                }
                else
                {
                    /*The ratio is 0, that means the area of the picture defined with (b)
                     parameters is bigger than the image size*/
                    if(pC->m_pPto3GPPparams->PanZoomTopleftYb + tempPanzoomXb > 1000)
                    {
                        /*The oversize is maxRatio*/
                        maxRatio = pC->m_pPto3GPPparams->PanZoomTopleftYb + tempPanzoomXb - 1000;
                    }
                }
                /*Modify the (b) parameters:*/
                if(pC->m_pPto3GPPparams->PanZoomTopleftXb >= maxRatio)
                {
                    /*The (b) topleft parameters can be moved to keep the same area size*/
                    pC->m_pPto3GPPparams->PanZoomTopleftXb -= maxRatio;
                }
                else
                {
                    /*Move the (b) topleft parameter to 0 but the ratio will be also further
                     modified to match the image size*/
                    pC->m_pPto3GPPparams->PanZoomTopleftXb = 0;
                }
                if(pC->m_pPto3GPPparams->PanZoomTopleftYb >= maxRatio)
                {
                    /*The (b) topleft parameters can be moved to keep the same area size*/
                    pC->m_pPto3GPPparams->PanZoomTopleftYb -= maxRatio;
                }
                else
                {
                    /*Move the (b) topleft parameter to 0 but the ratio will be also further
                    modified to match the image size*/
                    pC->m_pPto3GPPparams->PanZoomTopleftYb = 0;
                }
                /*The new ratio is the original one*/
                pC->m_pPto3GPPparams->PanZoomXb = tempPanzoomXb;
                if(pC->m_pPto3GPPparams->PanZoomXb + pC->m_pPto3GPPparams->PanZoomTopleftXb > 1000)
                {
                    /*Change the ratio if the area of the picture defined with (b) parameters is
                    bigger than the image size*/
                    pC->m_pPto3GPPparams->PanZoomXb = 1000 - pC->m_pPto3GPPparams->PanZoomTopleftXb;
                }
                if(pC->m_pPto3GPPparams->PanZoomXb + pC->m_pPto3GPPparams->PanZoomTopleftYb > 1000)
                {
                    /*Change the ratio if the area of the picture defined with (b) parameters is
                    bigger than the image size*/
                    pC->m_pPto3GPPparams->PanZoomXb = 1000 - pC->m_pPto3GPPparams->PanZoomTopleftYb;
                }
            }

            /**
             * Computes AIR parameters */
/*        Params.m_inputCoord.m_x = (M4OSA_UInt32)(pC->m_pDecodedPlane->u_width *
            (pC->m_pPto3GPPparams->PanZoomTopleftXa +
            (M4OSA_Int16)((pC->m_pPto3GPPparams->PanZoomTopleftXb \
                - pC->m_pPto3GPPparams->PanZoomTopleftXa) *
            pC->m_ImageCounter) / (M4OSA_Double)pC->m_NbImage)) / 100;
        Params.m_inputCoord.m_y = (M4OSA_UInt32)(pC->m_pDecodedPlane->u_height *
            (pC->m_pPto3GPPparams->PanZoomTopleftYa +
            (M4OSA_Int16)((pC->m_pPto3GPPparams->PanZoomTopleftYb\
                 - pC->m_pPto3GPPparams->PanZoomTopleftYa) *
            pC->m_ImageCounter) / (M4OSA_Double)pC->m_NbImage)) / 100;

        Params.m_inputSize.m_width = (M4OSA_UInt32)(pC->m_pDecodedPlane->u_width *
            (pC->m_pPto3GPPparams->PanZoomXa +
            (M4OSA_Int16)((pC->m_pPto3GPPparams->PanZoomXb - pC->m_pPto3GPPparams->PanZoomXa) *
            pC->m_ImageCounter) / (M4OSA_Double)pC->m_NbImage)) / 100;

        Params.m_inputSize.m_height =  (M4OSA_UInt32)(pC->m_pDecodedPlane->u_height *
            (pC->m_pPto3GPPparams->PanZoomXa +
            (M4OSA_Int16)((pC->m_pPto3GPPparams->PanZoomXb - pC->m_pPto3GPPparams->PanZoomXa) *
            pC->m_ImageCounter) / (M4OSA_Double)pC->m_NbImage)) / 100;
 */
            // Instead of using pC->m_NbImage we have to use (pC->m_NbImage-1) as pC->m_ImageCounter
            // will be x-1 max for x no. of frames
            Params.m_inputCoord.m_x = (M4OSA_UInt32)((((M4OSA_Double)pC->m_pDecodedPlane->u_width *
                (pC->m_pPto3GPPparams->PanZoomTopleftXa +
                (M4OSA_Double)((M4OSA_Double)(pC->m_pPto3GPPparams->PanZoomTopleftXb\
                     - pC->m_pPto3GPPparams->PanZoomTopleftXa) *
                pC->m_ImageCounter) / ((M4OSA_Double)pC->m_NbImage-1))) / 1000));
            Params.m_inputCoord.m_y =
                 (M4OSA_UInt32)((((M4OSA_Double)pC->m_pDecodedPlane->u_height *
                (pC->m_pPto3GPPparams->PanZoomTopleftYa +
                (M4OSA_Double)((M4OSA_Double)(pC->m_pPto3GPPparams->PanZoomTopleftYb\
                     - pC->m_pPto3GPPparams->PanZoomTopleftYa) *
                pC->m_ImageCounter) / ((M4OSA_Double)pC->m_NbImage-1))) / 1000));

            Params.m_inputSize.m_width =
                 (M4OSA_UInt32)((((M4OSA_Double)pC->m_pDecodedPlane->u_width *
                (pC->m_pPto3GPPparams->PanZoomXa +
                (M4OSA_Double)((M4OSA_Double)(pC->m_pPto3GPPparams->PanZoomXb\
                     - pC->m_pPto3GPPparams->PanZoomXa) *
                pC->m_ImageCounter) / (M4OSA_Double)pC->m_NbImage-1)) / 1000));

            Params.m_inputSize.m_height =
                 (M4OSA_UInt32)((((M4OSA_Double)pC->m_pDecodedPlane->u_height *
                (pC->m_pPto3GPPparams->PanZoomXa +
                (M4OSA_Double)((M4OSA_Double)(pC->m_pPto3GPPparams->PanZoomXb \
                    - pC->m_pPto3GPPparams->PanZoomXa) *
                pC->m_ImageCounter) / (M4OSA_Double)pC->m_NbImage-1)) / 1000));


            if((Params.m_inputSize.m_width + Params.m_inputCoord.m_x)\
                 > pC->m_pDecodedPlane->u_width)
            {
                Params.m_inputSize.m_width = pC->m_pDecodedPlane->u_width \
                    - Params.m_inputCoord.m_x;
            }

            if((Params.m_inputSize.m_height + Params.m_inputCoord.m_y)\
                 > pC->m_pDecodedPlane->u_height)
            {
                Params.m_inputSize.m_height = pC->m_pDecodedPlane->u_height\
                     - Params.m_inputCoord.m_y;
            }

            Params.m_inputSize.m_width = (Params.m_inputSize.m_width>>1)<<1;
            Params.m_inputSize.m_height = (Params.m_inputSize.m_height>>1)<<1;
        }



    /**
        Picture rendering: Black borders*/

        if(pC->m_mediaRendering == M4xVSS_kBlackBorders)
        {
            memset((void *)pImagePlanes[0].pac_data,Y_PLANE_BORDER_VALUE,
                (pImagePlanes[0].u_height*pImagePlanes[0].u_stride));
            memset((void *)pImagePlanes[1].pac_data,U_PLANE_BORDER_VALUE,
                (pImagePlanes[1].u_height*pImagePlanes[1].u_stride));
            /**
            First without pan&zoom*/
            M4OSA_TRACE1_0("M4xVSS_PictureCallbackFct_NV12: Black borders");
            if(M4OSA_FALSE == pC->m_pPto3GPPparams->isPanZoom)
            {
                M4OSA_TRACE1_0("M4xVSS_PictureCallbackFct_NV12: Black borders without panzoom");

                switch(pBasicTags.orientation)
                {
                default:
                case M4COMMON_kOrientationUnknown:
                    Params.m_outputOrientation = M4COMMON_kOrientationTopLeft;
                case M4COMMON_kOrientationTopLeft:
                case M4COMMON_kOrientationTopRight:
                case M4COMMON_kOrientationBottomRight:
                case M4COMMON_kOrientationBottomLeft:
                    if((M4OSA_UInt32)((pC->m_pDecodedPlane->u_height * pImagePlanes->u_width)\
                         /pC->m_pDecodedPlane->u_width) <= pImagePlanes->u_height)
                         //Params.m_inputSize.m_height < Params.m_inputSize.m_width)
                    {
                        /*it is height so black borders will be on the top and on the bottom side*/
                        Params.m_outputSize.m_width = pImagePlanes->u_width;
                        Params.m_outputSize.m_height =
                             (M4OSA_UInt32)((pC->m_pDecodedPlane->u_height \
                                * pImagePlanes->u_width) /pC->m_pDecodedPlane->u_width);
                        /*number of lines at the top*/
                        pImagePlanes[0].u_topleft =
                            (M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[0].u_height\
                                -Params.m_outputSize.m_height)>>1))*pImagePlanes[0].u_stride;
                        pImagePlanes[0].u_height = Params.m_outputSize.m_height;
                        pImagePlanes[1].u_topleft =
                             (M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[1].u_height\
                                -(Params.m_outputSize.m_height>>1)))>>1)*pImagePlanes[1].u_stride;
                        pImagePlanes[1].u_height = Params.m_outputSize.m_height>>1;

                    }
                    else
                    {
                        /*it is width so black borders will be on the left and right side*/
                        Params.m_outputSize.m_height = pImagePlanes->u_height;
                        Params.m_outputSize.m_width =
                             (M4OSA_UInt32)((pC->m_pDecodedPlane->u_width \
                                * pImagePlanes->u_height) /pC->m_pDecodedPlane->u_height);

                        pImagePlanes[0].u_topleft =
                            (M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[0].u_width\
                                -Params.m_outputSize.m_width))>>1);
                        pImagePlanes[0].u_width = Params.m_outputSize.m_width;
                        pImagePlanes[1].u_topleft =
                             (M4xVSS_ABS((M4OSA_Int32)((pImagePlanes[1].u_width\
                                -Params.m_outputSize.m_width)>>1)));
                        pImagePlanes[1].u_width = Params.m_outputSize.m_width;

                    }
                    break;
                case M4COMMON_kOrientationLeftTop:
                case M4COMMON_kOrientationLeftBottom:
                case M4COMMON_kOrientationRightTop:
                case M4COMMON_kOrientationRightBottom:
                        if((M4OSA_UInt32)((pC->m_pDecodedPlane->u_width * pImagePlanes->u_width)\
                             /pC->m_pDecodedPlane->u_height) < pImagePlanes->u_height)
                             //Params.m_inputSize.m_height > Params.m_inputSize.m_width)
                        {
                            /*it is height so black borders will be on the top and on
                             the bottom side*/
                            Params.m_outputSize.m_height = pImagePlanes->u_width;
                            Params.m_outputSize.m_width =
                                 (M4OSA_UInt32)((pC->m_pDecodedPlane->u_width \
                                    * pImagePlanes->u_width) /pC->m_pDecodedPlane->u_height);
                            /*number of lines at the top*/
                            pImagePlanes[0].u_topleft =
                                ((M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[0].u_height\
                                    -Params.m_outputSize.m_width))>>1)*pImagePlanes[0].u_stride)+1;
                            pImagePlanes[0].u_height = Params.m_outputSize.m_width;
                            pImagePlanes[1].u_topleft =
                                ((M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[1].u_height\
                                    -(Params.m_outputSize.m_width>>1)))>>1)\
                                        *pImagePlanes[1].u_stride)+1;
                            pImagePlanes[1].u_height = Params.m_outputSize.m_width>>1;

                        }
                        else
                        {
                            /*it is width so black borders will be on the left and right side*/
                            Params.m_outputSize.m_width = pImagePlanes->u_height;
                            Params.m_outputSize.m_height =
                                 (M4OSA_UInt32)((pC->m_pDecodedPlane->u_height\
                                     * pImagePlanes->u_height) /pC->m_pDecodedPlane->u_width);

                            pImagePlanes[0].u_topleft =
                                 ((M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[0].u_width\
                                    -Params.m_outputSize.m_height))>>1))+1;
                            pImagePlanes[0].u_width = Params.m_outputSize.m_height;
                            pImagePlanes[1].u_topleft =
                                 ((M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[1].u_width\
                                    -(Params.m_outputSize.m_height>>1)))))+1;
                            pImagePlanes[1].u_width = Params.m_outputSize.m_height;

                        }
                    break;
                }
            }

            /**
            Secondly with pan&zoom*/
            else
            {
                M4OSA_TRACE1_0("M4xVSS_PictureCallbackFct_NV12: Black borders with panzoom");
                switch(pBasicTags.orientation)
                {
                default:
                case M4COMMON_kOrientationUnknown:
                    Params.m_outputOrientation = M4COMMON_kOrientationTopLeft;
                case M4COMMON_kOrientationTopLeft:
                case M4COMMON_kOrientationTopRight:
                case M4COMMON_kOrientationBottomRight:
                case M4COMMON_kOrientationBottomLeft:
                    /*NO ROTATION*/
                    if((M4OSA_UInt32)((pC->m_pDecodedPlane->u_height * pImagePlanes->u_width)\
                         /pC->m_pDecodedPlane->u_width) <= pImagePlanes->u_height)
                            //Params.m_inputSize.m_height < Params.m_inputSize.m_width)
                    {
                        /*Black borders will be on the top and bottom of the output video*/
                        /*Maximum output height if the input image aspect ratio is kept and if
                        the output width is the screen width*/
                        M4OSA_UInt32 tempOutputSizeHeight =
                            (M4OSA_UInt32)((pC->m_pDecodedPlane->u_height\
                                 * pImagePlanes->u_width) /pC->m_pDecodedPlane->u_width);
                        M4OSA_UInt32 tempInputSizeHeightMax = 0;
                        M4OSA_UInt32 tempFinalInputHeight = 0;
                        /*The output width is the screen width*/
                        Params.m_outputSize.m_width = pImagePlanes->u_width;
                        tempOutputSizeHeight = (tempOutputSizeHeight>>1)<<1;

                        /*Maximum input height according to the maximum output height
                        (proportional to the maximum output height)*/
                        tempInputSizeHeightMax = (pImagePlanes->u_height\
                            *Params.m_inputSize.m_height)/tempOutputSizeHeight;
                        tempInputSizeHeightMax = (tempInputSizeHeightMax>>1)<<1;

                        /*Check if the maximum possible input height is contained into the
                        input image height*/
                        if(tempInputSizeHeightMax <= pC->m_pDecodedPlane->u_height)
                        {
                            /*The maximum possible input height is contained in the input
                            image height,
                            that means no black borders, the input pan zoom area will be extended
                            so that the input AIR height will be the maximum possible*/
                            if(((tempInputSizeHeightMax - Params.m_inputSize.m_height)>>1)\
                                 <= Params.m_inputCoord.m_y
                                && ((tempInputSizeHeightMax - Params.m_inputSize.m_height)>>1)\
                                     <= pC->m_pDecodedPlane->u_height -(Params.m_inputCoord.m_y\
                                         + Params.m_inputSize.m_height))
                            {
                                /*The input pan zoom area can be extended symmetrically on the
                                top and bottom side*/
                                Params.m_inputCoord.m_y -= ((tempInputSizeHeightMax \
                                    - Params.m_inputSize.m_height)>>1);
                            }
                            else if(Params.m_inputCoord.m_y < pC->m_pDecodedPlane->u_height\
                                -(Params.m_inputCoord.m_y + Params.m_inputSize.m_height))
                            {
                                /*There is not enough place above the input pan zoom area to
                                extend it symmetrically,
                                so extend it to the maximum on the top*/
                                Params.m_inputCoord.m_y = 0;
                            }
                            else
                            {
                                /*There is not enough place below the input pan zoom area to
                                extend it symmetrically,
                                so extend it to the maximum on the bottom*/
                                Params.m_inputCoord.m_y = pC->m_pDecodedPlane->u_height \
                                    - tempInputSizeHeightMax;
                            }
                            /*The input height of the AIR is the maximum possible height*/
                            Params.m_inputSize.m_height = tempInputSizeHeightMax;
                        }
                        else
                        {
                            /*The maximum possible input height is greater than the input
                            image height,
                            that means black borders are necessary to keep aspect ratio
                            The input height of the AIR is all the input image height*/
                            Params.m_outputSize.m_height =
                                (tempOutputSizeHeight*pC->m_pDecodedPlane->u_height)\
                                    /Params.m_inputSize.m_height;
                            Params.m_outputSize.m_height = (Params.m_outputSize.m_height>>1)<<1;
                            Params.m_inputCoord.m_y = 0;
                            Params.m_inputSize.m_height = pC->m_pDecodedPlane->u_height;
                            pImagePlanes[0].u_topleft =
                                 (M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[0].u_height\
                                    -Params.m_outputSize.m_height)>>1))*pImagePlanes[0].u_stride;
                            pImagePlanes[0].u_height = Params.m_outputSize.m_height;
                            pImagePlanes[1].u_topleft =
                                ((M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[1].u_height\
                                    -(Params.m_outputSize.m_height>>1)))>>1)\
                                        *pImagePlanes[1].u_stride);
                            pImagePlanes[1].u_height = Params.m_outputSize.m_height>>1;

                        }
                    }
                    else
                    {
                        /*Black borders will be on the left and right side of the output video*/
                        /*Maximum output width if the input image aspect ratio is kept and if the
                         output height is the screen height*/
                        M4OSA_UInt32 tempOutputSizeWidth =
                             (M4OSA_UInt32)((pC->m_pDecodedPlane->u_width \
                                * pImagePlanes->u_height) /pC->m_pDecodedPlane->u_height);
                        M4OSA_UInt32 tempInputSizeWidthMax = 0;
                        M4OSA_UInt32 tempFinalInputWidth = 0;
                        /*The output height is the screen height*/
                        Params.m_outputSize.m_height = pImagePlanes->u_height;
                        tempOutputSizeWidth = (tempOutputSizeWidth>>1)<<1;

                        /*Maximum input width according to the maximum output width
                        (proportional to the maximum output width)*/
                        tempInputSizeWidthMax =
                             (pImagePlanes->u_width*Params.m_inputSize.m_width)\
                                /tempOutputSizeWidth;
                        tempInputSizeWidthMax = (tempInputSizeWidthMax>>1)<<1;

                        /*Check if the maximum possible input width is contained into the input
                         image width*/
                        if(tempInputSizeWidthMax <= pC->m_pDecodedPlane->u_width)
                        {
                            /*The maximum possible input width is contained in the input
                            image width,
                            that means no black borders, the input pan zoom area will be extended
                            so that the input AIR width will be the maximum possible*/
                            if(((tempInputSizeWidthMax - Params.m_inputSize.m_width)>>1) \
                                <= Params.m_inputCoord.m_x
                                && ((tempInputSizeWidthMax - Params.m_inputSize.m_width)>>1)\
                                     <= pC->m_pDecodedPlane->u_width -(Params.m_inputCoord.m_x \
                                        + Params.m_inputSize.m_width))
                            {
                                /*The input pan zoom area can be extended symmetrically on the
                                     right and left side*/
                                Params.m_inputCoord.m_x -= ((tempInputSizeWidthMax\
                                     - Params.m_inputSize.m_width)>>1);
                            }
                            else if(Params.m_inputCoord.m_x < pC->m_pDecodedPlane->u_width\
                                -(Params.m_inputCoord.m_x + Params.m_inputSize.m_width))
                            {
                                /*There is not enough place above the input pan zoom area to
                                    extend it symmetrically,
                                so extend it to the maximum on the left*/
                                Params.m_inputCoord.m_x = 0;
                            }
                            else
                            {
                                /*There is not enough place below the input pan zoom area
                                    to extend it symmetrically,
                                so extend it to the maximum on the right*/
                                Params.m_inputCoord.m_x = pC->m_pDecodedPlane->u_width \
                                    - tempInputSizeWidthMax;
                            }
                            /*The input width of the AIR is the maximum possible width*/
                            Params.m_inputSize.m_width = tempInputSizeWidthMax;
                        }
                        else
                        {
                            /*The maximum possible input width is greater than the input
                            image width,
                            that means black borders are necessary to keep aspect ratio
                            The input width of the AIR is all the input image width*/
                            Params.m_outputSize.m_width =\
                                 (tempOutputSizeWidth*pC->m_pDecodedPlane->u_width)\
                                    /Params.m_inputSize.m_width;
                            Params.m_outputSize.m_width = (Params.m_outputSize.m_width>>1)<<1;
                            Params.m_inputCoord.m_x = 0;
                            Params.m_inputSize.m_width = pC->m_pDecodedPlane->u_width;
                            pImagePlanes[0].u_topleft =
                                 (M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[0].u_width\
                                    -Params.m_outputSize.m_width))>>1);
                            pImagePlanes[0].u_width = Params.m_outputSize.m_width;
                            pImagePlanes[1].u_topleft =
                                 (M4xVSS_ABS((M4OSA_Int32)((pImagePlanes[1].u_width\
                                    -Params.m_outputSize.m_width)>>1)));
                            pImagePlanes[1].u_width = Params.m_outputSize.m_width;

                        }
                    }
                    break;
                case M4COMMON_kOrientationLeftTop:
                case M4COMMON_kOrientationLeftBottom:
                case M4COMMON_kOrientationRightTop:
                case M4COMMON_kOrientationRightBottom:
                    /*ROTATION*/
                    if((M4OSA_UInt32)((pC->m_pDecodedPlane->u_width * pImagePlanes->u_width)\
                         /pC->m_pDecodedPlane->u_height) < pImagePlanes->u_height)
                         //Params.m_inputSize.m_height > Params.m_inputSize.m_width)
                    {
                        /*Black borders will be on the left and right side of the output video*/
                        /*Maximum output height if the input image aspect ratio is kept and if
                        the output height is the screen width*/
                        M4OSA_UInt32 tempOutputSizeHeight =
                        (M4OSA_UInt32)((pC->m_pDecodedPlane->u_width * pImagePlanes->u_width)\
                             /pC->m_pDecodedPlane->u_height);
                        M4OSA_UInt32 tempInputSizeHeightMax = 0;
                        M4OSA_UInt32 tempFinalInputHeight = 0;
                        /*The output width is the screen height*/
                        Params.m_outputSize.m_height = pImagePlanes->u_width;
                        Params.m_outputSize.m_width= pImagePlanes->u_height;
                        tempOutputSizeHeight = (tempOutputSizeHeight>>1)<<1;

                        /*Maximum input height according to the maximum output height
                             (proportional to the maximum output height)*/
                        tempInputSizeHeightMax =
                            (pImagePlanes->u_height*Params.m_inputSize.m_width)\
                                /tempOutputSizeHeight;
                        tempInputSizeHeightMax = (tempInputSizeHeightMax>>1)<<1;

                        /*Check if the maximum possible input height is contained into the
                             input image width (rotation included)*/
                        if(tempInputSizeHeightMax <= pC->m_pDecodedPlane->u_width)
                        {
                            /*The maximum possible input height is contained in the input
                            image width (rotation included),
                            that means no black borders, the input pan zoom area will be extended
                            so that the input AIR width will be the maximum possible*/
                            if(((tempInputSizeHeightMax - Params.m_inputSize.m_width)>>1) \
                                <= Params.m_inputCoord.m_x
                                && ((tempInputSizeHeightMax - Params.m_inputSize.m_width)>>1)\
                                     <= pC->m_pDecodedPlane->u_width -(Params.m_inputCoord.m_x \
                                        + Params.m_inputSize.m_width))
                            {
                                /*The input pan zoom area can be extended symmetrically on the
                                 right and left side*/
                                Params.m_inputCoord.m_x -= ((tempInputSizeHeightMax \
                                    - Params.m_inputSize.m_width)>>1);
                            }
                            else if(Params.m_inputCoord.m_x < pC->m_pDecodedPlane->u_width\
                                -(Params.m_inputCoord.m_x + Params.m_inputSize.m_width))
                            {
                                /*There is not enough place on the left of the input pan
                                zoom area to extend it symmetrically,
                                so extend it to the maximum on the left*/
                                Params.m_inputCoord.m_x = 0;
                            }
                            else
                            {
                                /*There is not enough place on the right of the input pan zoom
                                 area to extend it symmetrically,
                                so extend it to the maximum on the right*/
                                Params.m_inputCoord.m_x =
                                     pC->m_pDecodedPlane->u_width - tempInputSizeHeightMax;
                            }
                            /*The input width of the AIR is the maximum possible width*/
                            Params.m_inputSize.m_width = tempInputSizeHeightMax;
                        }
                        else
                        {
                            /*The maximum possible input height is greater than the input
                            image width (rotation included),
                            that means black borders are necessary to keep aspect ratio
                            The input width of the AIR is all the input image width*/
                            Params.m_outputSize.m_width =
                            (tempOutputSizeHeight*pC->m_pDecodedPlane->u_width)\
                                /Params.m_inputSize.m_width;
                            Params.m_outputSize.m_width = (Params.m_outputSize.m_width>>1)<<1;
                            Params.m_inputCoord.m_x = 0;
                            Params.m_inputSize.m_width = pC->m_pDecodedPlane->u_width;
                            pImagePlanes[0].u_topleft =
                                ((M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[0].u_height\
                                    -Params.m_outputSize.m_width))>>1)*pImagePlanes[0].u_stride)+1;
                            pImagePlanes[0].u_height = Params.m_outputSize.m_width;
                            pImagePlanes[1].u_topleft =
                            ((M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[1].u_height\
                                -(Params.m_outputSize.m_width>>1)))>>1)\
                                    *pImagePlanes[1].u_stride)+1;
                            pImagePlanes[1].u_height = Params.m_outputSize.m_width>>1;

                        }
                    }
                    else
                    {
                        /*Black borders will be on the top and bottom of the output video*/
                        /*Maximum output width if the input image aspect ratio is kept and if
                         the output width is the screen height*/
                        M4OSA_UInt32 tempOutputSizeWidth =
                        (M4OSA_UInt32)((pC->m_pDecodedPlane->u_height * pImagePlanes->u_height)\
                             /pC->m_pDecodedPlane->u_width);
                        M4OSA_UInt32 tempInputSizeWidthMax = 0;
                        M4OSA_UInt32 tempFinalInputWidth = 0, tempFinalOutputWidth = 0;
                        /*The output height is the screen width*/
                        Params.m_outputSize.m_width = pImagePlanes->u_height;
                        Params.m_outputSize.m_height= pImagePlanes->u_width;
                        tempOutputSizeWidth = (tempOutputSizeWidth>>1)<<1;

                        /*Maximum input width according to the maximum output width
                         (proportional to the maximum output width)*/
                        tempInputSizeWidthMax =
                        (pImagePlanes->u_width*Params.m_inputSize.m_height)/tempOutputSizeWidth;
                        tempInputSizeWidthMax = (tempInputSizeWidthMax>>1)<<1;

                        /*Check if the maximum possible input width is contained into the input
                         image height (rotation included)*/
                        if(tempInputSizeWidthMax <= pC->m_pDecodedPlane->u_height)
                        {
                            /*The maximum possible input width is contained in the input
                             image height (rotation included),
                            that means no black borders, the input pan zoom area will be extended
                            so that the input AIR height will be the maximum possible*/
                            if(((tempInputSizeWidthMax - Params.m_inputSize.m_height)>>1) \
                                <= Params.m_inputCoord.m_y
                                && ((tempInputSizeWidthMax - Params.m_inputSize.m_height)>>1)\
                                     <= pC->m_pDecodedPlane->u_height -(Params.m_inputCoord.m_y \
                                        + Params.m_inputSize.m_height))
                            {
                                /*The input pan zoom area can be extended symmetrically on
                                the right and left side*/
                                Params.m_inputCoord.m_y -= ((tempInputSizeWidthMax \
                                    - Params.m_inputSize.m_height)>>1);
                            }
                            else if(Params.m_inputCoord.m_y < pC->m_pDecodedPlane->u_height\
                                -(Params.m_inputCoord.m_y + Params.m_inputSize.m_height))
                            {
                                /*There is not enough place on the top of the input pan zoom
                                area to extend it symmetrically,
                                so extend it to the maximum on the top*/
                                Params.m_inputCoord.m_y = 0;
                            }
                            else
                            {
                                /*There is not enough place on the bottom of the input pan zoom
                                 area to extend it symmetrically,
                                so extend it to the maximum on the bottom*/
                                Params.m_inputCoord.m_y = pC->m_pDecodedPlane->u_height\
                                     - tempInputSizeWidthMax;
                            }
                            /*The input height of the AIR is the maximum possible height*/
                            Params.m_inputSize.m_height = tempInputSizeWidthMax;
                        }
                        else
                        {
                            /*The maximum possible input width is greater than the input\
                             image height (rotation included),
                            that means black borders are necessary to keep aspect ratio
                            The input height of the AIR is all the input image height*/
                            Params.m_outputSize.m_height =
                                (tempOutputSizeWidth*pC->m_pDecodedPlane->u_height)\
                                    /Params.m_inputSize.m_height;
                            Params.m_outputSize.m_height = (Params.m_outputSize.m_height>>1)<<1;
                            Params.m_inputCoord.m_y = 0;
                            Params.m_inputSize.m_height = pC->m_pDecodedPlane->u_height;
                            pImagePlanes[0].u_topleft =
                                ((M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[0].u_width\
                                    -Params.m_outputSize.m_height))>>1))+1;
                            pImagePlanes[0].u_width = Params.m_outputSize.m_height;
                            pImagePlanes[1].u_topleft =
                                ((M4xVSS_ABS((M4OSA_Int32)(pImagePlanes[1].u_width\
                                    -(Params.m_outputSize.m_height>>1)))))+1;
                            pImagePlanes[1].u_width = Params.m_outputSize.m_height;

                        }
                    }
                    break;
                }
            }

            /*Width and height have to be even*/
            Params.m_outputSize.m_width = (Params.m_outputSize.m_width>>1)<<1;
            Params.m_outputSize.m_height = (Params.m_outputSize.m_height>>1)<<1;
            Params.m_inputSize.m_width = (Params.m_inputSize.m_width>>1)<<1;
            Params.m_inputSize.m_height = (Params.m_inputSize.m_height>>1)<<1;
            pImagePlanes[0].u_width = (pImagePlanes[0].u_width>>1)<<1;
            pImagePlanes[1].u_width = (pImagePlanes[1].u_width>>1)<<1;

            pImagePlanes[0].u_height = (pImagePlanes[0].u_height>>1)<<1;
            pImagePlanes[1].u_height = (pImagePlanes[1].u_height>>1)<<1;


            /*Check that values are coherent*/
            if(Params.m_inputSize.m_height == Params.m_outputSize.m_height)
            {
                Params.m_inputSize.m_width = Params.m_outputSize.m_width;
            }
            else if(Params.m_inputSize.m_width == Params.m_outputSize.m_width)
            {
                Params.m_inputSize.m_height = Params.m_outputSize.m_height;
            }
        }

        /**
        Picture rendering: Resizing and Cropping*/
        if(pC->m_mediaRendering != M4xVSS_kBlackBorders)
        {

            switch(pBasicTags.orientation)
            {
            default:
            case M4COMMON_kOrientationUnknown:
                Params.m_outputOrientation = M4COMMON_kOrientationTopLeft;
            case M4COMMON_kOrientationTopLeft:
            case M4COMMON_kOrientationTopRight:
            case M4COMMON_kOrientationBottomRight:
            case M4COMMON_kOrientationBottomLeft:
                Params.m_outputSize.m_height = pImagePlanes->u_height;
                Params.m_outputSize.m_width = pImagePlanes->u_width;
                break;
            case M4COMMON_kOrientationLeftTop:
            case M4COMMON_kOrientationLeftBottom:
            case M4COMMON_kOrientationRightTop:
            case M4COMMON_kOrientationRightBottom:
                Params.m_outputSize.m_height = pImagePlanes->u_width;
                Params.m_outputSize.m_width = pImagePlanes->u_height;
                break;
            }
        }

        /**
        Picture rendering: Cropping*/
        if(pC->m_mediaRendering == M4xVSS_kCropping)
        {
            M4OSA_TRACE1_0("M4xVSS_PictureCallbackFct_NV12: Cropping");
            if((Params.m_outputSize.m_height * Params.m_inputSize.m_width)\
                 /Params.m_outputSize.m_width<Params.m_inputSize.m_height)
            {
                M4OSA_UInt32 tempHeight = Params.m_inputSize.m_height;
                /*height will be cropped*/
                Params.m_inputSize.m_height =  (M4OSA_UInt32)((Params.m_outputSize.m_height \
                    * Params.m_inputSize.m_width) /Params.m_outputSize.m_width);
                Params.m_inputSize.m_height =  (Params.m_inputSize.m_height>>1)<<1;
                if(M4OSA_FALSE == pC->m_pPto3GPPparams->isPanZoom)
                {
                    Params.m_inputCoord.m_y = (M4OSA_Int32)((M4OSA_Int32)\
                        ((pC->m_pDecodedPlane->u_height - Params.m_inputSize.m_height))>>1);
                }
                else
                {
                    Params.m_inputCoord.m_y += (M4OSA_Int32)((M4OSA_Int32)\
                        ((tempHeight - Params.m_inputSize.m_height))>>1);
                }
            }
            else
            {
                M4OSA_UInt32 tempWidth= Params.m_inputSize.m_width;
                /*width will be cropped*/
                Params.m_inputSize.m_width =  (M4OSA_UInt32)((Params.m_outputSize.m_width \
                    * Params.m_inputSize.m_height) /Params.m_outputSize.m_height);
                Params.m_inputSize.m_width =  (Params.m_inputSize.m_width>>1)<<1;
                if(M4OSA_FALSE == pC->m_pPto3GPPparams->isPanZoom)
                {
                    Params.m_inputCoord.m_x = (M4OSA_Int32)((M4OSA_Int32)\
                        ((pC->m_pDecodedPlane->u_width - Params.m_inputSize.m_width))>>1);
                }
                else
                {
                    Params.m_inputCoord.m_x += (M4OSA_Int32)\
                        (((M4OSA_Int32)(tempWidth - Params.m_inputSize.m_width))>>1);
                }
            }
        }

        M4OSA_TRACE1_0("M4xVSS_PictureCallbackFct_NV12: Before AIR functions");

        /**
         * Call AIR functions */
        if(M4OSA_NULL == pC->m_air_context)
        {
            err = M4AIR_create_NV12(&pC->m_air_context, M4AIR_kNV12P);

            M4OSA_TRACE1_0("M4xVSS_PictureCallbackFct_NV12: After M4AIR_create_NV12");

            if(err != M4NO_ERROR)
            {
                free(pC->m_pDecodedPlane[0].pac_data);
                free(pC->m_pDecodedPlane);
                pC->m_pDecodedPlane = M4OSA_NULL;
                M4OSA_TRACE1_1("M4xVSS_PictureCallbackFct_NV12:\
                     Error when initializing AIR: 0x%x", err);
                return err;
            }
        }

        err = M4AIR_configure_NV12(pC->m_air_context, &Params);

        M4OSA_TRACE1_0("M4xVSS_PictureCallbackFct_NV12: After M4AIR_configure_NV12");

        if(err != M4NO_ERROR)
        {
            M4OSA_TRACE1_1("M4xVSS_PictureCallbackFct_NV12:\
                 Error when configuring AIR: 0x%x", err);
            M4AIR_cleanUp_NV12(pC->m_air_context);
            free(pC->m_pDecodedPlane[0].pac_data);
            free(pC->m_pDecodedPlane);
            pC->m_pDecodedPlane = M4OSA_NULL;
            return err;
        }

        err = M4AIR_get_NV12(pC->m_air_context, pC->m_pDecodedPlane, pImagePlanes);
        if(err != M4NO_ERROR)
        {
            M4OSA_TRACE1_1("M4xVSS_PictureCallbackFct_NV12: Error when getting AIR plane: 0x%x", err);
            M4AIR_cleanUp_NV12(pC->m_air_context);
            free(pC->m_pDecodedPlane[0].pac_data);
            free(pC->m_pDecodedPlane);
            pC->m_pDecodedPlane = M4OSA_NULL;
            return err;
        }
        pImagePlanes[0] = pImagePlanes1;
        pImagePlanes[1] = pImagePlanes2;

    }


    /**
     * Increment the image counter */
    pC->m_ImageCounter++;

    /**
     * Check end of sequence */
    last_frame_flag = (pC->m_ImageCounter >= pC->m_NbImage);

    /**
     * Keep the picture duration */
    *pPictureDuration = pC->m_timeDuration;

    if (1 == last_frame_flag)
    {
        if(M4OSA_NULL != pC->m_air_context)
        {
            err = M4AIR_cleanUp(pC->m_air_context);
            if(err != M4NO_ERROR)
            {
                M4OSA_TRACE1_1("M4xVSS_PictureCallbackFct_NV12: Error when cleaning AIR: 0x%x", err);
                return err;
            }
        }
        if(M4OSA_NULL != pC->m_pDecodedPlane)
        {
            free(pC->m_pDecodedPlane[0].pac_data);
            free(pC->m_pDecodedPlane);
            pC->m_pDecodedPlane = M4OSA_NULL;
        }
        return M4PTO3GPP_WAR_LAST_PICTURE;
    }

    M4OSA_TRACE1_0("M4xVSS_PictureCallbackFct_NV12: Leaving ");
    return M4NO_ERROR;
}

/**
 ******************************************************************************
 * prototype    M4OSA_ERR M4xVSS_internalConvertRGBtoNV12(M4xVSS_FramingStruct* framingCtx)
 * @brief    This function converts an RGB565 plane to NV12 planar
 * @note    It is used only for framing effect
 *            It allocates output YUV planes
 * @param    framingCtx    (IN) The framing struct containing input RGB565 plane
 *
 * @return    M4NO_ERROR:    No error
 * @return    M4ERR_PARAMETER: At least one of the function parameters is null
 * @return    M4ERR_ALLOC: Allocation error (no more memory)
 ******************************************************************************
 */
M4OSA_ERR M4xVSS_internalConvertRGBtoNV12(M4xVSS_FramingStruct* framingCtx)
{
    M4OSA_ERR err;

    /**
     * Allocate output NV12 planes */
    framingCtx->FramingYuv = (M4VIFI_ImagePlane*)M4OSA_32bitAlignedMalloc(2*sizeof(M4VIFI_ImagePlane),
        M4VS, (M4OSA_Char *)"M4xVSS_internalConvertRGBtoNV12: Output plane NV12");
    if(framingCtx->FramingYuv == M4OSA_NULL)
    {
        M4OSA_TRACE1_0("Allocation error in M4xVSS_internalConvertRGBtoNV12");
        return M4ERR_ALLOC;
    }
    framingCtx->FramingYuv[0].u_width = framingCtx->FramingRgb->u_width;
    framingCtx->FramingYuv[0].u_height = framingCtx->FramingRgb->u_height;
    framingCtx->FramingYuv[0].u_topleft = 0;
    framingCtx->FramingYuv[0].u_stride = framingCtx->FramingRgb->u_width;
    framingCtx->FramingYuv[0].pac_data =
        (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc((framingCtx->FramingYuv[0].u_width\
        *framingCtx->FramingYuv[0].u_height*3)>>1, M4VS, (M4OSA_Char *)\
         "Alloc for the Convertion output YUV");

    if(framingCtx->FramingYuv[0].pac_data == M4OSA_NULL)
    {
        M4OSA_TRACE1_0("Allocation error in M4xVSS_internalConvertRGBtoNV12");
        return M4ERR_ALLOC;
    }
    framingCtx->FramingYuv[1].u_width = framingCtx->FramingRgb->u_width;
    framingCtx->FramingYuv[1].u_height = (framingCtx->FramingRgb->u_height)>>1;
    framingCtx->FramingYuv[1].u_topleft = 0;
    framingCtx->FramingYuv[1].u_stride = framingCtx->FramingRgb->u_width;
    framingCtx->FramingYuv[1].pac_data = framingCtx->FramingYuv[0].pac_data \
    + framingCtx->FramingYuv[0].u_width * framingCtx->FramingYuv[0].u_height;

    /**
     * Convert input RGB 565 to NV12 to be able to merge it with output video in framing
      effect */
    err = M4VIFI_xVSS_RGB565toNV12(M4OSA_NULL, framingCtx->FramingRgb, framingCtx->FramingYuv);
    if(err != M4NO_ERROR)
    {
        M4OSA_TRACE1_1("M4xVSS_internalConvertRGBtoNV12:\
             error when converting from RGB to NV12: 0x%x\n", err);
    }

    framingCtx->duration = 0;
    framingCtx->previousClipTime = -1;
    framingCtx->previewOffsetClipTime = -1;

    /**
     * Only one element in the chained list (no animated image with RGB buffer...) */
    framingCtx->pCurrent = framingCtx;
    framingCtx->pNext = framingCtx;

    return M4NO_ERROR;
}


static M4OSA_ERR M4xVSS_internalProbeFramingBoundaryNV12(M4xVSS_FramingStruct *framingCtx)
{
    M4OSA_ERR err = M4NO_ERROR;
    M4OSA_Int32   topleft_x, topleft_y, botright_x, botright_y;
    M4OSA_UInt32   isTopLeftFound, isBottomRightFound;
    M4OSA_UInt32   u32_width, u32_height;
    M4OSA_UInt32   u32_stride_rgb, u32_stride_2rgb;
    M4OSA_UInt32   u32_col, u32_row;
    M4OSA_UInt8    *pu8_rgbn_data, *pu8_rgbn;
    M4OSA_UInt16   u16_pix1, u16_pix2, u16_pix3, u16_pix4;

    M4OSA_TRACE1_0("M4xVSS_internalProbeFramingBoundary starts!");

    if (!framingCtx->exportmode) {
        M4OSA_TRACE1_0("Err: not in export mode!");
        return err;
    }
    topleft_x           = 0;
    topleft_y           = 0;
    botright_x          = 0;
    botright_y          = 0;
    isTopLeftFound      = 0;
    isBottomRightFound  = 0;
    framingCtx->framing_topleft_x = 0;
    framingCtx->framing_topleft_y = 0;
    framingCtx->framing_bottomright_x = 0;
    framingCtx->framing_bottomright_y = 0;


    /* Set the pointer to the beginning of the input data buffers */
    pu8_rgbn_data   = framingCtx->FramingRgb->pac_data + framingCtx->FramingRgb->u_topleft;

    u32_width = framingCtx->FramingRgb->u_width;
    u32_height = framingCtx->FramingRgb->u_height;

    /* Set the size of the memory jumps corresponding to row jump in input plane */
    u32_stride_rgb = framingCtx->FramingRgb->u_stride;
    u32_stride_2rgb = u32_stride_rgb << 1;


    /* Loop on each row of the output image, input coordinates are estimated from output ones */
    /* Two YUV rows are computed at each pass */
    for (u32_row = u32_height ;u32_row != 0; u32_row -=2)
    {
        pu8_rgbn = pu8_rgbn_data;

        /* Loop on each column of the output image */
        for (u32_col = u32_width; u32_col != 0 ; u32_col -=2)
        {
            /* Get four RGB 565 samples from input data */
            u16_pix1 = *( (M4VIFI_UInt16 *) pu8_rgbn);
            u16_pix2 = *( (M4VIFI_UInt16 *) (pu8_rgbn + CST_RGB_16_SIZE));
            u16_pix3 = *( (M4VIFI_UInt16 *) (pu8_rgbn + u32_stride_rgb));
            u16_pix4 = *( (M4VIFI_UInt16 *) (pu8_rgbn + u32_stride_rgb + CST_RGB_16_SIZE));
            M4OSA_TRACE1_4("u16_pix1 = 0x%x, u16_pix2 = 0x%x, u16_pix3 = 0x%x, u16_pix4 = 0x%x",
                u16_pix1, u16_pix2, u16_pix3, u16_pix4);
            if (u16_pix1 != 0xE007 && u16_pix2 != 0xE007 &&
                u16_pix3 != 0xE007 && u16_pix4 != 0xE007 && !isTopLeftFound)
            {
                topleft_x = u32_width - (u32_col+1);
                topleft_y = u32_height - (u32_row+1);
                isTopLeftFound = 1;
            }
            if (u16_pix1 != 0xE007 && u16_pix2 != 0xE007 &&
                u16_pix3 != 0xE007 && u16_pix4 != 0xE007)
            {
                botright_x = u32_width  - (u32_col+1);
                botright_y = u32_height - (u32_row+1);
                isBottomRightFound = 1;
            }

            /* Prepare for next column */
            pu8_rgbn += (CST_RGB_16_SIZE<<1);
        } /* End of horizontal scanning */

        /* Prepare pointers for the next row */
        pu8_rgbn_data += u32_stride_2rgb;

    }
    M4OSA_TRACE1_2("isTopLeftFound = %d, isBottomRightFound = %d", isTopLeftFound, isBottomRightFound);
    if (isTopLeftFound && isTopLeftFound)
    {
        if ((topleft_x < botright_x) && (topleft_y < botright_y))
        {
            framingCtx->framing_topleft_x       = (((topleft_x + 1)>>1)<<1) + 2;
            framingCtx->framing_topleft_y       = (((topleft_y + 1)>>1)<<1) + 2;
            framingCtx->framing_bottomright_x   = (((botright_x- 1)>>1)<<1) - 1;
            framingCtx->framing_bottomright_y   = (((botright_y- 1)>>1)<<1) - 1;
            M4OSA_TRACE1_2("framingCtx->framing_topleft_x = %d, framingCtx->framing_topleft_y = %d",
                framingCtx->framing_topleft_x, framingCtx->framing_topleft_y);
            M4OSA_TRACE1_2("framingCtx->framing_bottomright_x = %d, framingCtx->framing_bottomright_y = %d",
                framingCtx->framing_bottomright_x, framingCtx->framing_bottomright_y);
        }
        else
        {
            M4OSA_TRACE1_0("Err: invalid topleft and bottomright!");
        }
    }
    else
    {
        M4OSA_TRACE1_0("Err: fail to find framing boundaries!");
    }
    return M4NO_ERROR;
}


/**
 ******************************************************************************
 * prototype M4OSA_ERR M4xVSS_internalConvertARBG888toNV12_FrammingEffect(M4OSA_Context pContext,
 *                                                M4VSS3GPP_EffectSettings* pEffect,
 *                                                M4xVSS_FramingStruct* framingCtx,
                                                  M4VIDEOEDITING_VideoFrameSize OutputVideoResolution)
 *
 * @brief    This function converts ARGB8888 input file  to NV12 when used for framming effect
 * @note    The input ARGB8888 file path is contained in the pEffect structure
 *            If the ARGB8888 must be resized to fit output video size, this function
 *            will do it.
 * @param    pContext    (IN) The integrator own context
 * @param    pEffect        (IN) The effect structure containing all informations on
 *                        the file to decode, resizing ...
 * @param    framingCtx    (IN/OUT) Structure in which the output RGB will be stored
 *
 * @return    M4NO_ERROR:    No error
 * @return    M4ERR_PARAMETER: At least one of the function parameters is null
 * @return    M4ERR_ALLOC: Allocation error (no more memory)
 * @return    M4ERR_FILE_NOT_FOUND: File not found.
 ******************************************************************************
 */

M4OSA_ERR M4xVSS_internalConvertARGB888toNV12_FrammingEffect(M4OSA_Context pContext,
    M4VSS3GPP_EffectSettings* pEffect, M4xVSS_FramingStruct* framingCtx,
    M4VIDEOEDITING_VideoFrameSize OutputVideoResolution)
{
    M4OSA_ERR err = M4NO_ERROR;
    M4OSA_Context pARGBIn;
    M4OSA_UInt32 file_size;
    M4xVSS_Context* xVSS_context = (M4xVSS_Context*)pContext;
    M4OSA_UInt32 width, height, width_out, height_out;
    M4OSA_Void* pFile = pEffect->xVSS.pFramingFilePath;
    M4OSA_UInt8 transparent1 = (M4OSA_UInt8)((TRANSPARENT_COLOR & 0xFF00)>>8);
    M4OSA_UInt8 transparent2 = (M4OSA_UInt8)TRANSPARENT_COLOR;
    /*UTF conversion support*/
    M4OSA_Char* pDecodedPath = M4OSA_NULL;
    M4OSA_UInt32 i = 0,j = 0;
    M4VIFI_ImagePlane rgbPlane;
    M4OSA_UInt32 frameSize_argb=(framingCtx->width * framingCtx->height * 4);
    M4OSA_UInt32 frameSize;
    M4OSA_UInt32 tempAlphaPercent = 0;
    M4VIFI_UInt8* TempPacData = M4OSA_NULL;
    M4OSA_UInt16 *ptr = M4OSA_NULL;
    M4OSA_UInt32 z = 0;

    M4OSA_TRACE3_0("M4xVSS_internalConvertARGB888toNV12_FrammingEffect: Entering ");

    M4OSA_TRACE1_2("M4xVSS_internalConvertARGB888toNV12_FrammingEffect width and height %d %d ",
    framingCtx->width,framingCtx->height);

    M4OSA_UInt8 *pTmpData = (M4OSA_UInt8*) M4OSA_32bitAlignedMalloc(frameSize_argb, M4VS, (M4OSA_Char*)\
    "Image argb data");
    if(pTmpData == M4OSA_NULL) {
        M4OSA_TRACE1_0("Failed to allocate memory for Image clip");
        return M4ERR_ALLOC;
    }
    /**
     * UTF conversion: convert the file path into the customer format*/
    pDecodedPath = pFile;

    if(xVSS_context->UTFConversionContext.pConvFromUTF8Fct != M4OSA_NULL
    && xVSS_context->UTFConversionContext.pTempOutConversionBuffer != M4OSA_NULL)
    {
        M4OSA_UInt32 length = 0;
        err = M4xVSS_internalConvertFromUTF8(xVSS_context, (M4OSA_Void*) pFile,
        (M4OSA_Void*) xVSS_context->UTFConversionContext.pTempOutConversionBuffer, &length);
        if(err != M4NO_ERROR)
        {
            M4OSA_TRACE1_1("M4xVSS_internalDecodePNG:\
                 M4xVSS_internalConvertFromUTF8 returns err: 0x%x",err);
            free(pTmpData);
            pTmpData = M4OSA_NULL;
            return err;
        }
        pDecodedPath = xVSS_context->UTFConversionContext.pTempOutConversionBuffer;
    }

    /**
    * End of the conversion, now use the decoded path*/

    /* Open input ARGB8888 file and store it into memory */
    err = xVSS_context->pFileReadPtr->openRead(&pARGBIn, pDecodedPath, M4OSA_kFileRead);

    if(err != M4NO_ERROR)
    {
        M4OSA_TRACE1_2("Can't open input ARGB8888 file %s, error: 0x%x\n",pFile, err);
        free(pTmpData);
        pTmpData = M4OSA_NULL;
        return err;
    }

    err = xVSS_context->pFileReadPtr->readData(pARGBIn,(M4OSA_MemAddr8)pTmpData, &frameSize_argb);
    if(err != M4NO_ERROR)
    {
        xVSS_context->pFileReadPtr->closeRead(pARGBIn);
        free(pTmpData);
        pTmpData = M4OSA_NULL;
        return err;
    }


    err =  xVSS_context->pFileReadPtr->closeRead(pARGBIn);
    if(err != M4NO_ERROR)
    {
        M4OSA_TRACE1_2("Can't close input png file %s, error: 0x%x\n",pFile, err);
        free(pTmpData);
        pTmpData = M4OSA_NULL;
        return err;
    }


    rgbPlane.u_height = framingCtx->height;
    rgbPlane.u_width = framingCtx->width;
    rgbPlane.u_stride = rgbPlane.u_width*3;
    rgbPlane.u_topleft = 0;

    frameSize = (rgbPlane.u_width * rgbPlane.u_height * 3); //Size of RGB888 data
    rgbPlane.pac_data = (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(((frameSize)+ (2 * framingCtx->width)),
    M4VS, (M4OSA_Char*)"Image clip RGB888 data");
    if(rgbPlane.pac_data == M4OSA_NULL)
    {
        M4OSA_TRACE1_0("Failed to allocate memory for Image clip");
        free(pTmpData);
        return M4ERR_ALLOC;
    }

    M4OSA_TRACE1_0("M4xVSS_internalConvertARGB888toNV12_FrammingEffect:\
          Remove the alpha channel  ");

    /* premultiplied alpha % on RGB */
    for (i=0, j = 0; i < frameSize_argb; i += 4) {
        /* this is alpha value */
        if ((i % 4) == 0)
        {
            tempAlphaPercent = pTmpData[i];
        }

        /* R */
        rgbPlane.pac_data[j] = pTmpData[i+1];
        j++;

        /* G */
        if (tempAlphaPercent > 0) {
            rgbPlane.pac_data[j] = pTmpData[i+2];
            j++;
        } else {/* In case of alpha value 0, make GREEN to 255 */
            rgbPlane.pac_data[j] = 255; //pTmpData[i+2];
            j++;
        }

        /* B */
        rgbPlane.pac_data[j] = pTmpData[i+3];
        j++;
    }

    free(pTmpData);
    pTmpData = M4OSA_NULL;

    /* convert RGB888 to RGB565 */

    /* allocate temp RGB 565 buffer */
    TempPacData = (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(frameSize +
    (4 * (framingCtx->width + framingCtx->height + 1)),
    M4VS, (M4OSA_Char*)"Image clip RGB565 data");
    if (TempPacData == M4OSA_NULL) {
        M4OSA_TRACE1_0("Failed to allocate memory for Image clip RGB565 data");
        free(rgbPlane.pac_data);
        return M4ERR_ALLOC;
    }

    ptr = (M4OSA_UInt16 *)TempPacData;
    z = 0;

    for (i = 0; i < j ; i += 3)
    {
        ptr[z++] = PACK_RGB565(0,   rgbPlane.pac_data[i],
        rgbPlane.pac_data[i+1],
        rgbPlane.pac_data[i+2]);
    }

    /* free the RBG888 and assign RGB565 */
    free(rgbPlane.pac_data);
    rgbPlane.pac_data = TempPacData;

    /**
     * Check if output sizes are odd */
    if(rgbPlane.u_height % 2 != 0)
    {
        M4VIFI_UInt8* output_pac_data = rgbPlane.pac_data;
        M4OSA_UInt32 i;
        M4OSA_TRACE1_0("M4xVSS_internalConvertARGB888toNV12_FrammingEffect:\
             output height is odd  ");
        output_pac_data +=rgbPlane.u_width * rgbPlane.u_height*2;

        for(i=0; i<rgbPlane.u_width; i++)
        {
            *output_pac_data++ = transparent1;
            *output_pac_data++ = transparent2;
        }

        /**
         * We just add a white line to the PNG that will be transparent */
        rgbPlane.u_height++;
    }
    if(rgbPlane.u_width % 2 != 0)
    {
        /**
         * We add a new column of white (=transparent), but we need to parse all RGB lines ... */
        M4OSA_UInt32 i;
        M4VIFI_UInt8* newRGBpac_data;
        M4VIFI_UInt8* output_pac_data, *input_pac_data;

        rgbPlane.u_width++;
        M4OSA_TRACE1_0("M4xVSS_internalConvertARGB888toYUV420_FrammingEffect: \
             output width is odd  ");
        /**
         * We need to allocate a new RGB output buffer in which all decoded data
          + white line will be copied */
        newRGBpac_data = (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(rgbPlane.u_height*rgbPlane.u_width*2\
        *sizeof(M4VIFI_UInt8), M4VS, (M4OSA_Char *)"New Framing GIF Output pac_data RGB");

        if(newRGBpac_data == M4OSA_NULL)
        {
            M4OSA_TRACE1_0("Allocation error in \
                M4xVSS_internalConvertARGB888toYUV420_FrammingEffect");
            free(rgbPlane.pac_data);
            return M4ERR_ALLOC;
        }

        output_pac_data= newRGBpac_data;
        input_pac_data = rgbPlane.pac_data;

        for(i=0; i<rgbPlane.u_height; i++)
        {
            memcpy((void *)output_pac_data, (void *)input_pac_data,
            (rgbPlane.u_width-1)*2);

            output_pac_data += ((rgbPlane.u_width-1)*2);
            /* Put the pixel to transparency color */
            *output_pac_data++ = transparent1;
            *output_pac_data++ = transparent2;

            input_pac_data += ((rgbPlane.u_width-1)*2);
        }
        free(rgbPlane.pac_data);
        rgbPlane.pac_data = newRGBpac_data;
    }

    /* reset stride */
    rgbPlane.u_stride = rgbPlane.u_width*2;

    /**
     * Initialize chained list parameters */
    framingCtx->duration = 0;
    framingCtx->previousClipTime = -1;
    framingCtx->previewOffsetClipTime = -1;

    /**
     * Only one element in the chained list (no animated image ...) */
    framingCtx->pCurrent = framingCtx;
    framingCtx->pNext = framingCtx;

    /**
     * Get output width/height */
    switch(OutputVideoResolution)
        //switch(xVSS_context->pSettings->xVSS.outputVideoSize)
    {
    case M4VIDEOEDITING_kSQCIF:
        width_out = 128;
        height_out = 96;
        break;
    case M4VIDEOEDITING_kQQVGA:
        width_out = 160;
        height_out = 120;
        break;
    case M4VIDEOEDITING_kQCIF:
        width_out = 176;
        height_out = 144;
        break;
    case M4VIDEOEDITING_kQVGA:
        width_out = 320;
        height_out = 240;
        break;
    case M4VIDEOEDITING_kCIF:
        width_out = 352;
        height_out = 288;
        break;
    case M4VIDEOEDITING_kVGA:
        width_out = 640;
        height_out = 480;
        break;
    case M4VIDEOEDITING_kWVGA:
        width_out = 800;
        height_out = 480;
        break;
    case M4VIDEOEDITING_kNTSC:
        width_out = 720;
        height_out = 480;
        break;
    case M4VIDEOEDITING_k640_360:
        width_out = 640;
        height_out = 360;
        break;
    case M4VIDEOEDITING_k854_480:
        // StageFright encoders require %16 resolution
        width_out = M4ENCODER_854_480_Width;
        height_out = 480;
        break;
    case M4VIDEOEDITING_k1280_720:
        width_out = 1280;
        height_out = 720;
        break;
    case M4VIDEOEDITING_k1080_720:
        // StageFright encoders require %16 resolution
        width_out = M4ENCODER_1080_720_Width;
        height_out = 720;
        break;
    case M4VIDEOEDITING_k960_720:
        width_out = 960;
        height_out = 720;
        break;
    case M4VIDEOEDITING_k1920_1080:
        width_out = 1920;
        height_out = M4ENCODER_1920_1080_Height;
        break;
        /**
         * If output video size is not given, we take QCIF size,
         * should not happen, because already done in M4xVSS_sendCommand */
    default:
        width_out = 176;
        height_out = 144;
        break;
    }

    /**
     * Allocate output planes structures */
    framingCtx->FramingRgb = (M4VIFI_ImagePlane*)M4OSA_32bitAlignedMalloc(sizeof(M4VIFI_ImagePlane), M4VS,
    (M4OSA_Char *)"Framing Output plane RGB");
    if(framingCtx->FramingRgb == M4OSA_NULL)
    {
        M4OSA_TRACE1_0("Allocation error in M4xVSS_internalConvertARGB888toNV12_FrammingEffect");
        return M4ERR_ALLOC;
    }
    /**
     * Resize RGB if needed */
    if((pEffect->xVSS.bResize) &&
    (rgbPlane.u_width != width_out || rgbPlane.u_height != height_out))
    {
        width = width_out;
        height = height_out;

        M4OSA_TRACE1_2("M4xVSS_internalConvertARGB888toNV12_FrammingEffect: \
             New Width and height %d %d  ",width,height);

        framingCtx->FramingRgb->u_height = height_out;
        framingCtx->FramingRgb->u_width = width_out;
        framingCtx->FramingRgb->u_stride = framingCtx->FramingRgb->u_width*2;
        framingCtx->FramingRgb->u_topleft = 0;

        framingCtx->FramingRgb->pac_data =
        (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(framingCtx->FramingRgb->u_height*framingCtx->\
        FramingRgb->u_width*2*sizeof(M4VIFI_UInt8), M4VS,
        (M4OSA_Char *)"Framing Output pac_data RGB");

        if(framingCtx->FramingRgb->pac_data == M4OSA_NULL)
        {
            M4OSA_TRACE1_0("Allocation error in \
                M4xVSS_internalConvertARGB888toNV12_FrammingEffect");
            free(framingCtx->FramingRgb);
            free(rgbPlane.pac_data);
            return M4ERR_ALLOC;
        }

        M4OSA_TRACE1_0("M4xVSS_internalConvertARGB888toNV12_FrammingEffect:  Resizing Needed ");
        M4OSA_TRACE1_2("M4xVSS_internalConvertARGB888toNV12_FrammingEffect:\
              rgbPlane.u_height & rgbPlane.u_width %d %d",rgbPlane.u_height,rgbPlane.u_width);

        err = M4VIFI_ResizeBilinearRGB565toRGB565(M4OSA_NULL, &rgbPlane,framingCtx->FramingRgb);

        if(err != M4NO_ERROR)
        {
            M4OSA_TRACE1_1("M4xVSS_internalConvertARGB888toNV12_FrammingEffect :\
                when resizing RGB plane: 0x%x\n", err);
            return err;
        }

        if(rgbPlane.pac_data != M4OSA_NULL)
        {
            free(rgbPlane.pac_data);
            rgbPlane.pac_data = M4OSA_NULL;
        }
    }
    else
    {

        M4OSA_TRACE1_0("M4xVSS_internalConvertARGB888toNV12_FrammingEffect:\
              Resizing Not Needed ");

        width = rgbPlane.u_width;
        height = rgbPlane.u_height;
        framingCtx->FramingRgb->u_height = height;
        framingCtx->FramingRgb->u_width = width;
        framingCtx->FramingRgb->u_stride = framingCtx->FramingRgb->u_width*2;
        framingCtx->FramingRgb->u_topleft = 0;
        framingCtx->FramingRgb->pac_data = rgbPlane.pac_data;
    }


    if(pEffect->xVSS.bResize)
    {
        /**
         * Force topleft to 0 for pure framing effect */
        framingCtx->topleft_x = 0;
        framingCtx->topleft_y = 0;
    }


    /**
     * Convert  RGB output to NV12 to be able to merge it with output video in framing
     effect */
    framingCtx->FramingYuv = (M4VIFI_ImagePlane*)M4OSA_32bitAlignedMalloc(2*sizeof(M4VIFI_ImagePlane), M4VS,
    (M4OSA_Char *)"Framing Output plane NV12");
    if(framingCtx->FramingYuv == M4OSA_NULL)
    {
        M4OSA_TRACE1_0("Allocation error in M4xVSS_internalConvertARGB888toNV12_FrammingEffect");
        free(framingCtx->FramingRgb->pac_data);
        return M4ERR_ALLOC;
    }

    // Alloc for Y, U and V planes
    framingCtx->FramingYuv[0].u_width = ((width+1)>>1)<<1;
    framingCtx->FramingYuv[0].u_height = ((height+1)>>1)<<1;
    framingCtx->FramingYuv[0].u_topleft = 0;
    framingCtx->FramingYuv[0].u_stride = ((width+1)>>1)<<1;
    framingCtx->FramingYuv[0].pac_data = (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc
    ((framingCtx->FramingYuv[0].u_width*framingCtx->FramingYuv[0].u_height), M4VS,
    (M4OSA_Char *)"Alloc for the output Y");
    if(framingCtx->FramingYuv[0].pac_data == M4OSA_NULL)
    {
        M4OSA_TRACE1_0("Allocation error in M4xVSS_internalConvertARGB888toNV12_FrammingEffect");
        free(framingCtx->FramingYuv);
        free(framingCtx->FramingRgb->pac_data);
        return M4ERR_ALLOC;
    }
    framingCtx->FramingYuv[1].u_width = framingCtx->FramingYuv[0].u_width;
    framingCtx->FramingYuv[1].u_height = (framingCtx->FramingYuv[0].u_height)>>1;
    framingCtx->FramingYuv[1].u_topleft = 0;
    framingCtx->FramingYuv[1].u_stride = framingCtx->FramingYuv[1].u_width;

    framingCtx->FramingYuv[1].pac_data = (M4VIFI_UInt8*)M4OSA_32bitAlignedMalloc(
        framingCtx->FramingYuv[1].u_width * framingCtx->FramingYuv[1].u_height, M4VS,
        (M4OSA_Char *)"Alloc for the output U&V");
    if (framingCtx->FramingYuv[1].pac_data == M4OSA_NULL) {
        free(framingCtx->FramingYuv[0].pac_data);
        free(framingCtx->FramingYuv);
        free(framingCtx->FramingRgb->pac_data);
        return M4ERR_ALLOC;
    }

    M4OSA_TRACE3_0("M4xVSS_internalConvertARGB888toNV12_FrammingEffect:\
        convert RGB to YUV ");

    err = M4VIFI_RGB565toNV12(M4OSA_NULL, framingCtx->FramingRgb,  framingCtx->FramingYuv);

    if (framingCtx->exportmode) {
        M4xVSS_internalProbeFramingBoundaryNV12(framingCtx);
    }

    if (err != M4NO_ERROR)
    {
        M4OSA_TRACE1_1("SPS png: error when converting from RGB to NV12: 0x%x\n", err);
    }
    M4OSA_TRACE3_0("M4xVSS_internalConvertARGB888toNV12_FrammingEffect:  Leaving ");
    return err;
}

/**
 ******************************************************************************
 * prototype    M4VSS3GPP_externalVideoEffectColor_NV12(M4OSA_Void *pFunctionContext,
 *                                                    M4VIFI_ImagePlane *PlaneIn,
 *                                                    M4VIFI_ImagePlane *PlaneOut,
 *                                                    M4VSS3GPP_ExternalProgress *pProgress,
 *                                                    M4OSA_UInt32 uiEffectKind)
 *
 * @brief    This function apply a color effect on an input NV12 planar frame
 * @note
 * @param    pFunctionContext(IN) Contains which color to apply (not very clean ...)
 * @param    PlaneIn            (IN) Input NV12 planar
 * @param    PlaneOut        (IN/OUT) Output NV12 planar
 * @param    pProgress        (IN/OUT) Progress indication (0-100)
 * @param    uiEffectKind    (IN) Unused
 *
 * @return    M4VIFI_OK:    No error
 ******************************************************************************
 */
M4OSA_ERR M4VSS3GPP_externalVideoEffectColor_NV12(M4OSA_Void *pFunctionContext,
    M4VIFI_ImagePlane *PlaneIn, M4VIFI_ImagePlane *PlaneOut,
    M4VSS3GPP_ExternalProgress *pProgress, M4OSA_UInt32 uiEffectKind)
{
    M4VIFI_Int32 plane_number;
    M4VIFI_UInt32 i,j,wStep;
    M4VIFI_UInt8 *p_buf_src, *p_buf_dest,*p_buf_dest_uv;
    M4xVSS_ColorStruct* ColorContext = (M4xVSS_ColorStruct*)pFunctionContext;
    M4VIFI_UInt32 uvTmp,uvTmp1,uvTmp2,u_wTmp,u_wTmp1;
    M4VIFI_UInt32 *p_buf_dest_uv32;
    uvTmp = uvTmp1 = uvTmp2 = 0;


    for (plane_number = 0; plane_number < 2; plane_number++)
    {
        p_buf_src = &(PlaneIn[plane_number].pac_data[PlaneIn[plane_number].u_topleft]);
        p_buf_dest = &(PlaneOut[plane_number].pac_data[PlaneOut[plane_number].u_topleft]);

        for (i = 0; i < PlaneOut[plane_number].u_height; i++)
        {
            /**
             * Chrominance */
            if(plane_number==1)
            {
                p_buf_dest_uv32 = (M4VIFI_UInt32*)p_buf_dest;
                //switch ((M4OSA_UInt32)pFunctionContext)
                // commented because a structure for the effects context exist
                switch (ColorContext->colorEffectType)
                {
                case M4xVSS_kVideoEffectType_BlackAndWhite:
                    memset((void *)p_buf_dest,128,
                    PlaneIn[plane_number].u_width);
                    break;
                case M4xVSS_kVideoEffectType_Pink:
                    memset((void *)p_buf_dest,255,
                    PlaneIn[plane_number].u_width);
                    break;
                case M4xVSS_kVideoEffectType_Green:
                    memset((void *)p_buf_dest,0,
                    PlaneIn[plane_number].u_width);
                    break;
                case M4xVSS_kVideoEffectType_Sepia:
                {
                    uvTmp1 = 139;
                    uvTmp2 = 117 | (uvTmp1 << 8);
                    uvTmp = uvTmp2 | (uvTmp2 << 16);

                    u_wTmp = PlaneIn[plane_number].u_width;

                    u_wTmp1 = u_wTmp >> 4;
                    for(wStep = 0; wStep < u_wTmp1; wStep++)
                    {
                        *p_buf_dest_uv32++ = uvTmp;
                        *p_buf_dest_uv32++ = uvTmp;
                        *p_buf_dest_uv32++ = uvTmp;
                        *p_buf_dest_uv32++ = uvTmp;
                    }
                    u_wTmp1 = u_wTmp - ((u_wTmp>>4)<<4); // equal to u_wTmp % 16
                    p_buf_dest_uv = (M4VIFI_UInt8*)p_buf_dest_uv32;
                    for(j=0; j< u_wTmp1; j++)
                    {
                        if (j%2 == 0)
                        {
                            *p_buf_dest_uv++ = 117;
                        }
                        else
                        {
                            *p_buf_dest_uv++ = 139;
                        }
                    }
                    break;
                }
                case M4xVSS_kVideoEffectType_Negative:
                    memcpy((void *)p_buf_dest,
                    (void *)p_buf_src ,PlaneOut[plane_number].u_width);
                    break;

                case M4xVSS_kVideoEffectType_ColorRGB16:
                {
                    M4OSA_UInt16 r = 0,g = 0,b = 0,y = 0,u = 0,v = 0;

                    /*first get the r, g, b*/
                    b = (ColorContext->rgb16ColorData &  0x001f);
                    g = (ColorContext->rgb16ColorData &  0x07e0)>>5;
                    r = (ColorContext->rgb16ColorData &  0xf800)>>11;

                    /*keep y, but replace u and v*/
                    u = U16(r, g, b);
                    v = V16(r, g, b);
                    uvTmp1 = (M4OSA_UInt8)v;
                    uvTmp2 = ((M4OSA_UInt8)u) | (uvTmp1 << 8);
                    uvTmp = uvTmp2 | (uvTmp2 << 16);

                    u_wTmp = PlaneIn[plane_number].u_width;

                    u_wTmp1 = u_wTmp >> 2;
                    for(wStep = 0; wStep < u_wTmp1; wStep++)
                    {
                        *p_buf_dest_uv32++ = uvTmp;
                    }
                    u_wTmp1 = u_wTmp - ((u_wTmp>>2)<<2); // equal to u_wTmp % 4
                    p_buf_dest_uv = (M4VIFI_UInt8*)p_buf_dest_uv32;
                    if(u_wTmp1 == 0) {
                        break;
                    } else if(u_wTmp1 == 1) {
                        *p_buf_dest_uv = (M4OSA_UInt8)u;
                    } else if(u_wTmp1 == 2) {
                        *p_buf_dest_uv++ = (M4OSA_UInt8)u;
                        *p_buf_dest_uv = (M4OSA_UInt8)v;
                    } else {
                        *p_buf_dest_uv++ = (M4OSA_UInt8)u;
                        *p_buf_dest_uv++ = (M4OSA_UInt8)v;
                        *p_buf_dest_uv = (M4OSA_UInt8)u;
                    }
                    break;
                }
                case M4xVSS_kVideoEffectType_Gradient:
                {
                    M4OSA_UInt16 r = 0,g = 0,b = 0,y = 0,u = 0,v = 0;

                    /*first get the r, g, b*/
                    b = (ColorContext->rgb16ColorData &  0x001f);
                    g = (ColorContext->rgb16ColorData &  0x07e0)>>5;
                    r = (ColorContext->rgb16ColorData &  0xf800)>>11;

                    /*for color gradation*/
                    b = (M4OSA_UInt16)( b - ((b*i)/PlaneIn[plane_number].u_height));
                    g = (M4OSA_UInt16)(g - ((g*i)/PlaneIn[plane_number].u_height));
                    r = (M4OSA_UInt16)(r - ((r*i)/PlaneIn[plane_number].u_height));

                    /*keep y, but replace u and v*/
                    u = U16(r, g, b);
                    v = V16(r, g, b);
                    uvTmp1 = (M4OSA_UInt8)v;
                    uvTmp2 = ((M4OSA_UInt8)u) | (uvTmp1 << 8);
                    uvTmp = uvTmp2 | (uvTmp2 << 16);

                    u_wTmp = PlaneIn[plane_number].u_width;

                    u_wTmp1 = u_wTmp >> 2;
                    for(wStep = 0; wStep < u_wTmp1; wStep++)
                    {
                        *p_buf_dest_uv32++ = uvTmp;
                    }
                    u_wTmp1 = u_wTmp - ((u_wTmp>>2)<<2); // equal to u_wTmp % 4
                    p_buf_dest_uv = (M4VIFI_UInt8*)p_buf_dest_uv32;
                    if(u_wTmp1 == 0) {
                        break;
                    } else if(u_wTmp1 == 1) {
                        *p_buf_dest_uv = (M4OSA_UInt8)u;
                    } else if(u_wTmp1 == 2) {
                        *p_buf_dest_uv++ = (M4OSA_UInt8)u;
                        *p_buf_dest_uv = (M4OSA_UInt8)v;
                    } else {
                        *p_buf_dest_uv++ = (M4OSA_UInt8)u;
                        *p_buf_dest_uv++ = (M4OSA_UInt8)v;
                        *p_buf_dest_uv = (M4OSA_UInt8)u;
                    }
                    break;
                }
                default:
                    break;
                }
            }
            /**
             * Luminance */
            else
            {
                //switch ((M4OSA_UInt32)pFunctionContext)
                // commented because a structure for the effects context exist
                switch (ColorContext->colorEffectType)
                {
                case M4xVSS_kVideoEffectType_Negative:
                    for(j=0; j<PlaneOut[plane_number].u_width; j++)
                    {
                        p_buf_dest[j] = 255 - p_buf_src[j];
                    }
                    break;
                default:
                    memcpy((void *)p_buf_dest,
                    (void *)p_buf_src ,PlaneOut[plane_number].u_width);
                    break;
                }
            }
            p_buf_src += PlaneIn[plane_number].u_stride;
            p_buf_dest += PlaneOut[plane_number].u_stride;
        }
    }

    return M4VIFI_OK;
}

/**
 ******************************************************************************
 * prototype    M4VSS3GPP_externalVideoEffectFraming_NV12(M4OSA_Void *pFunctionContext,
 *                                                    M4VIFI_ImagePlane *PlaneIn,
 *                                                    M4VIFI_ImagePlane *PlaneOut,
 *                                                    M4VSS3GPP_ExternalProgress *pProgress,
 *                                                    M4OSA_UInt32 uiEffectKind)
 *
 * @brief    This function add a fixed or animated image on an input NV12 planar frame
 * @note
 * @param    pFunctionContext(IN) Contains which color to apply (not very clean ...)
 * @param    PlaneIn            (IN) Input NV12 planar
 * @param    PlaneOut        (IN/OUT) Output NV12 planar
 * @param    pProgress        (IN/OUT) Progress indication (0-100)
 * @param    uiEffectKind    (IN) Unused
 *
 * @return    M4VIFI_OK:    No error
 ******************************************************************************
 */
M4OSA_ERR M4VSS3GPP_externalVideoEffectFraming_NV12(M4OSA_Void *userData,
    M4VIFI_ImagePlane *PlaneIn, M4VIFI_ImagePlane *PlaneOut,
    M4VSS3GPP_ExternalProgress *pProgress, M4OSA_UInt32 uiEffectKind)
{
    M4VIFI_UInt32 x,y;

    M4VIFI_UInt8 *p_in_Y = PlaneIn[0].pac_data;
    M4VIFI_UInt8 *p_in_UV = PlaneIn[1].pac_data;

    M4xVSS_FramingStruct* Framing = M4OSA_NULL;
    M4xVSS_FramingStruct* currentFraming = M4OSA_NULL;
    M4VIFI_UInt8 *FramingRGB = M4OSA_NULL;

    M4VIFI_UInt8 *p_out0, *p_out1;

    M4VIFI_UInt32 topleft[2];
    M4VIFI_UInt32 topleft_x, topleft_y, botright_x, botright_y;
    M4OSA_UInt8 transparent1 = (M4OSA_UInt8)((TRANSPARENT_COLOR & 0xFF00)>>8);
    M4OSA_UInt8 transparent2 = (M4OSA_UInt8)TRANSPARENT_COLOR;

#ifndef DECODE_GIF_ON_SAVING
    Framing = (M4xVSS_FramingStruct *)userData;
    currentFraming = (M4xVSS_FramingStruct *)Framing->pCurrent;
    FramingRGB = Framing->FramingRgb->pac_data;
#endif /*DECODE_GIF_ON_SAVING*/

    /*FB*/
#ifdef DECODE_GIF_ON_SAVING
    M4OSA_ERR err;
    Framing = (M4xVSS_FramingStruct *)((M4xVSS_FramingContext*)userData)->aFramingCtx;
    currentFraming = (M4xVSS_FramingStruct *)Framing;
    FramingRGB = Framing->FramingRgb->pac_data;
#endif /*DECODE_GIF_ON_SAVING*/
    /*end FB*/

    /**
     * Initialize input / output plane pointers */
    p_in_Y += PlaneIn[0].u_topleft;
    p_in_UV += PlaneIn[1].u_topleft;

    p_out0 = PlaneOut[0].pac_data;
    p_out1 = PlaneOut[1].pac_data;

    /**
     * Depending on time, initialize Framing frame to use */
    if(Framing->previousClipTime == -1)
    {
        Framing->previousClipTime = pProgress->uiOutputTime;
    }

    /**
     * If the current clip time has reach the duration of one frame of the framing picture
     * we need to step to next framing picture */

    Framing->previousClipTime = pProgress->uiOutputTime;
    FramingRGB = currentFraming->FramingRgb->pac_data;
    topleft[0] = currentFraming->topleft_x;
    topleft[1] = currentFraming->topleft_y;

    M4OSA_TRACE1_2("currentFraming->topleft_x = %d, currentFraming->topleft_y = %d",
        currentFraming->topleft_x,currentFraming->topleft_y);

    topleft_x = currentFraming->framing_topleft_x;
    topleft_y = currentFraming->framing_topleft_y;
    botright_x = currentFraming->framing_bottomright_x;
    botright_y = currentFraming->framing_bottomright_y;

    M4OSA_TRACE1_4("topleft_x = %d, topleft_y = %d, botright_x = %d, botright_y = %d",
        topleft_x,topleft_y, botright_x, botright_y);

    /*Alpha blending support*/
    M4OSA_Float alphaBlending = 1;

    M4xVSS_internalEffectsAlphaBlending*  alphaBlendingStruct =\
    (M4xVSS_internalEffectsAlphaBlending*)\
    ((M4xVSS_FramingContext*)userData)->alphaBlendingStruct;

    if(alphaBlendingStruct != M4OSA_NULL)
    {
        if(pProgress->uiProgress \
        < (M4OSA_UInt32)(alphaBlendingStruct->m_fadeInTime*10))
        {
            if(alphaBlendingStruct->m_fadeInTime == 0) {
                alphaBlending = alphaBlendingStruct->m_start / 100;
            } else {
                alphaBlending = ((M4OSA_Float)(alphaBlendingStruct->m_middle\
                - alphaBlendingStruct->m_start)\
                *pProgress->uiProgress/(alphaBlendingStruct->m_fadeInTime*10));
                alphaBlending += alphaBlendingStruct->m_start;
                alphaBlending /= 100;
            }
        }
        else if(pProgress->uiProgress >= (M4OSA_UInt32)(alphaBlendingStruct->\
        m_fadeInTime*10) && pProgress->uiProgress < 1000\
        - (M4OSA_UInt32)(alphaBlendingStruct->m_fadeOutTime*10))
        {
            alphaBlending = (M4OSA_Float)\
            ((M4OSA_Float)alphaBlendingStruct->m_middle/100);
        }
        else if(pProgress->uiProgress >= 1000 - (M4OSA_UInt32)\
        (alphaBlendingStruct->m_fadeOutTime*10))
        {
            if(alphaBlendingStruct->m_fadeOutTime == 0) {
                alphaBlending = alphaBlendingStruct->m_end / 100;
            } else {
                alphaBlending = ((M4OSA_Float)(alphaBlendingStruct->m_middle \
                - alphaBlendingStruct->m_end))*(1000 - pProgress->uiProgress)\
                /(alphaBlendingStruct->m_fadeOutTime*10);
                alphaBlending += alphaBlendingStruct->m_end;
                alphaBlending /= 100;
            }
        }
    }

    M4VIFI_UInt8 alphaBlending_int8 = (M4VIFI_UInt8)(alphaBlending * 255);

    for( x=0 ; x < PlaneIn[0].u_height ; x++)
    {
        if((x<topleft[1]+topleft_y) || (x> topleft[1] + botright_y))
        {
            if(x&0x01)
            {
                memcpy(p_out0+x*PlaneOut[0].u_stride,
                        p_in_Y+x*PlaneIn[0].u_stride,
                        PlaneOut[0].u_width);
            }
            else
            {
                memcpy(p_out0+x*PlaneOut[0].u_stride,
                        p_in_Y+x*PlaneIn[0].u_stride,
                        PlaneOut[0].u_width);
                memcpy(p_out1+(x>>1)*PlaneOut[1].u_stride,
                        p_in_UV+(x>>1)*PlaneIn[1].u_stride,
                        PlaneOut[1].u_width);
            }
        }
        else
        {
            if(x&0x01)
            {
                for(y=0; y < PlaneIn[0].u_width ; y++)
                {
                    if((y>=topleft[0]+topleft_x) && (y<=topleft[0]+botright_x))
                    {
                        *( p_out0+y+x*PlaneOut[0].u_stride)=
                        (M4VIFI_UInt8)(((*(currentFraming->FramingYuv[0].pac_data+(y-topleft[0])\
                        +(x-topleft[1])*currentFraming->FramingYuv[0].u_stride))*alphaBlending_int8
                        + (*(p_in_Y+y+x*PlaneIn[0].u_stride))*(255-alphaBlending_int8))>>8);
                    }
                    else
                    {
                        *( p_out0+y+x*PlaneOut[0].u_stride)=*(p_in_Y+y+x*PlaneIn[0].u_stride);
                    }
                }
            }
            else
            {
                for(y=0 ; y < PlaneIn[0].u_width ; y++)
                {
                    if((y>=topleft[0]+topleft_x) && (y<=topleft[0]+botright_x))
                    {
                        *( p_out0+y+x*PlaneOut[0].u_stride)=
                        (M4VIFI_UInt8)(((*(currentFraming->FramingYuv[0].pac_data+(y-topleft[0])\
                        +(x-topleft[1])*currentFraming->FramingYuv[0].u_stride))*alphaBlending_int8
                        +(*(p_in_Y+y+x*PlaneIn[0].u_stride))*(255-alphaBlending_int8))>>8);

                        *( p_out1+y+(x>>1)*PlaneOut[1].u_stride)=
                        (M4VIFI_UInt8)(((*(currentFraming->FramingYuv[1].pac_data+(y-topleft[0])\
                        +((x-topleft[1])>>1)*currentFraming->FramingYuv[1].u_stride))\
                        *alphaBlending_int8 +
                        *(p_in_UV+y+(x>>1)*PlaneIn[1].u_stride)*(255-alphaBlending_int8))>>8);
                    }
                    else
                    {
                        *( p_out0+y+x*PlaneOut[0].u_stride)=*(p_in_Y+y+x*PlaneIn[0].u_stride);
                        *( p_out1+y+(x>>1)*PlaneOut[1].u_stride)= *(p_in_UV+y+(x>>1)*PlaneIn[1].u_stride);
                    }
                }
            }
        }
    }
    return M4VIFI_OK;
}

/**
 ******************************************************************************
 * prototype    M4xVSS_AlphaMagic_NV12( M4OSA_Void *userData,
 *                                    M4VIFI_ImagePlane PlaneIn1[2],
 *                                    M4VIFI_ImagePlane PlaneIn2[2],
 *                                    M4VIFI_ImagePlane *PlaneOut,
 *                                    M4VSS3GPP_ExternalProgress *pProgress,
 *                                    M4OSA_UInt32 uiTransitionKind)
 *
 * @brief    This function apply a color effect on an input NV12 planar frame
 * @note
 * @param    userData        (IN) Contains a pointer on a settings structure
 * @param    PlaneIn1        (IN) Input NV12 planar from video 1
 * @param    PlaneIn2        (IN) Input NV12 planar from video 2
 * @param    PlaneOut        (IN/OUT) Output NV12 planar
 * @param    pProgress        (IN/OUT) Progress indication (0-100)
 * @param    uiTransitionKind(IN) Unused
 *
 * @return    M4VIFI_OK:    No error
 ******************************************************************************
 */
M4OSA_ERR M4xVSS_AlphaMagic_NV12( M4OSA_Void *userData, M4VIFI_ImagePlane *PlaneIn1,
    M4VIFI_ImagePlane *PlaneIn2, M4VIFI_ImagePlane *PlaneOut,
    M4VSS3GPP_ExternalProgress *pProgress, M4OSA_UInt32 uiTransitionKind)
{
    M4OSA_ERR err;
    M4xVSS_internal_AlphaMagicSettings* alphaContext;
    M4VIFI_Int32 alphaProgressLevel;

    M4VIFI_ImagePlane* planeswap;
    M4VIFI_UInt32 x,y;

    M4VIFI_UInt8 *p_out0;
    M4VIFI_UInt8 *p_out1;

    M4VIFI_UInt8 *alphaMask;
    /* "Old image" */
    M4VIFI_UInt8 *p_in1_Y;
    M4VIFI_UInt8 *p_in1_UV;
    /* "New image" */
    M4VIFI_UInt8 *p_in2_Y;
    M4VIFI_UInt8 *p_in2_UV;

    M4OSA_TRACE1_0("M4xVSS_AlphaMagic_NV12 begin");

    err = M4NO_ERROR;

    alphaContext = (M4xVSS_internal_AlphaMagicSettings*)userData;

    alphaProgressLevel = (pProgress->uiProgress * 128)/1000;

    if( alphaContext->isreverse != M4OSA_FALSE)
    {
        alphaProgressLevel = 128 - alphaProgressLevel;
        planeswap = PlaneIn1;
        PlaneIn1 = PlaneIn2;
        PlaneIn2 = planeswap;
    }

    p_out0 = PlaneOut[0].pac_data;
    p_out1 = PlaneOut[1].pac_data;

    alphaMask = alphaContext->pPlane->pac_data;

    /* "Old image" */
    p_in1_Y = PlaneIn1[0].pac_data;
    p_in1_UV = PlaneIn1[1].pac_data;

    /* "New image" */
    p_in2_Y = PlaneIn2[0].pac_data;
    p_in2_UV = PlaneIn2[1].pac_data;

     /**
     * For each column ... */
    for( y=0; y<PlaneOut->u_height; y++ )
    {
        /**
         * ... and each row of the alpha mask */
        for( x=0; x<PlaneOut->u_width; x++ )
        {
            /**
             * If the value of the current pixel of the alpha mask is > to the current time
             * ( current time is normalized on [0-255] ) */
            if( alphaProgressLevel < alphaMask[x+y*PlaneOut->u_width] )
            {
                /* We keep "old image" in output plane */
                *( p_out0+x+y*PlaneOut[0].u_stride)=*(p_in1_Y+x+y*PlaneIn1[0].u_stride);
                *( p_out1+x+(y>>1)*PlaneOut[1].u_stride)=
                    *(p_in1_UV+x+(y>>1)*PlaneIn1[1].u_stride);
            }
            else
            {
                /* We take "new image" in output plane */
                *( p_out0+x+y*PlaneOut[0].u_stride)=*(p_in2_Y+x+y*PlaneIn2[0].u_stride);
                *( p_out1+x+(y>>1)*PlaneOut[1].u_stride)=
                    *(p_in2_UV+x+(y>>1)*PlaneIn2[1].u_stride);
            }
        }
    }

    M4OSA_TRACE1_0("M4xVSS_AlphaMagic_NV12 end");

    return(err);
}

/**
 ******************************************************************************
 * prototype    M4xVSS_AlphaMagicBlending_NV12( M4OSA_Void *userData,
 *                                    M4VIFI_ImagePlane PlaneIn1[2],
 *                                    M4VIFI_ImagePlane PlaneIn2[2],
 *                                    M4VIFI_ImagePlane *PlaneOut,
 *                                    M4VSS3GPP_ExternalProgress *pProgress,
 *                                    M4OSA_UInt32 uiTransitionKind)
 *
 * @brief    This function apply a color effect on an input NV12 planar frame
 * @note
 * @param    userData        (IN) Contains a pointer on a settings structure
 * @param    PlaneIn1        (IN) Input NV12 planar from video 1
 * @param    PlaneIn2        (IN) Input NV12 planar from video 2
 * @param    PlaneOut        (IN/OUT) Output NV12 planar
 * @param    pProgress        (IN/OUT) Progress indication (0-100)
 * @param    uiTransitionKind(IN) Unused
 *
 * @return    M4VIFI_OK:    No error
 ******************************************************************************
 */
M4OSA_ERR M4xVSS_AlphaMagicBlending_NV12(M4OSA_Void *userData,
    M4VIFI_ImagePlane *PlaneIn1, M4VIFI_ImagePlane *PlaneIn2,
    M4VIFI_ImagePlane *PlaneOut, M4VSS3GPP_ExternalProgress *pProgress,
    M4OSA_UInt32 uiTransitionKind)
{
    M4OSA_ERR err;

    M4xVSS_internal_AlphaMagicSettings* alphaContext;
    M4VIFI_Int32 alphaProgressLevel;
    M4VIFI_Int32 alphaBlendLevelMin;
    M4VIFI_Int32 alphaBlendLevelMax;
    M4VIFI_Int32 alphaBlendRange;

    M4VIFI_ImagePlane* planeswap;
    M4VIFI_UInt32 x,y;
    M4VIFI_Int32 alphaMaskValue;

    M4VIFI_UInt8 *p_out0;
    M4VIFI_UInt8 *p_out1;
    M4VIFI_UInt8 *alphaMask;

    /* "Old image" */
    M4VIFI_UInt8 *p_in1_Y;
    M4VIFI_UInt8 *p_in1_UV;

    /* "New image" */
    M4VIFI_UInt8 *p_in2_Y;
    M4VIFI_UInt8 *p_in2_UV;

    M4OSA_TRACE1_0("M4xVSS_AlphaMagicBlending_NV12 begin");

    err = M4NO_ERROR;

    alphaContext = (M4xVSS_internal_AlphaMagicSettings*)userData;

    alphaProgressLevel = (pProgress->uiProgress * 128)/1000;

    if( alphaContext->isreverse != M4OSA_FALSE)
    {
        alphaProgressLevel = 128 - alphaProgressLevel;
        planeswap = PlaneIn1;
        PlaneIn1 = PlaneIn2;
        PlaneIn2 = planeswap;
    }

    alphaBlendLevelMin = alphaProgressLevel-alphaContext->blendingthreshold;

    alphaBlendLevelMax = alphaProgressLevel+alphaContext->blendingthreshold;

    alphaBlendRange = (alphaContext->blendingthreshold)*2;

    p_out0 = PlaneOut[0].pac_data;
    p_out1 = PlaneOut[1].pac_data;


    alphaMask = alphaContext->pPlane->pac_data;

    /* "Old image" */
    p_in1_Y = PlaneIn1[0].pac_data;
    p_in1_UV = PlaneIn1[1].pac_data;

    /* "New image" */
    p_in2_Y = PlaneIn2[0].pac_data;
    p_in2_UV = PlaneIn2[1].pac_data;


    /* apply Alpha Magic on each pixel */
    for( y=0; y<PlaneOut->u_height; y++ )
    {
        if (y%2 == 0)
        {
            for( x=0; x<PlaneOut->u_width; x++ )
            {
                alphaMaskValue = alphaMask[x+y*PlaneOut->u_width];
                if( alphaBlendLevelMax < alphaMaskValue )
                {
                    /* We keep "old image" in output plane */
                    *( p_out0+x+y*PlaneOut[0].u_stride)=*(p_in1_Y+x+y*PlaneIn1[0].u_stride);
                    *( p_out1+x+(y>>1)*PlaneOut[1].u_stride)=
                        *(p_in1_UV+x+(y>>1)*PlaneIn1[1].u_stride);
                }
                else if( (alphaBlendLevelMin < alphaMaskValue)&&
                        (alphaMaskValue <= alphaBlendLevelMax ) )
                {
                    /* We blend "old and new image" in output plane */
                    *( p_out0+x+y*PlaneOut[0].u_stride)=(M4VIFI_UInt8)
                        (( (alphaMaskValue-alphaBlendLevelMin)*( *(p_in1_Y+x+y*PlaneIn1[0].u_stride))
                            +(alphaBlendLevelMax-alphaMaskValue)\
                                *( *(p_in2_Y+x+y*PlaneIn2[0].u_stride)) )/alphaBlendRange );

                    *( p_out1+x+(y>>1)*PlaneOut[1].u_stride)=(M4VIFI_UInt8)\
                        (( (alphaMaskValue-alphaBlendLevelMin)*( *(p_in1_UV+x+(y>>1)\
                            *PlaneIn1[1].u_stride))
                                +(alphaBlendLevelMax-alphaMaskValue)*( *(p_in2_UV+x+(y>>1)\
                                    *PlaneIn2[1].u_stride)) )/alphaBlendRange );
                }
                else
                {
                    /* We take "new image" in output plane */
                    *( p_out0+x+y*PlaneOut[0].u_stride)=*(p_in2_Y+x+y*PlaneIn2[0].u_stride);
                    *( p_out1+x+(y>>1)*PlaneOut[1].u_stride)=
                        *(p_in2_UV+x+(y>>1)*PlaneIn2[1].u_stride);

                }
            }
        }
        else
        {
            for( x=0; x<PlaneOut->u_width; x++ )
            {
                alphaMaskValue = alphaMask[x+y*PlaneOut->u_width];
                if( alphaBlendLevelMax < alphaMaskValue )
                {
                    /* We keep "old image" in output plane */
                    *( p_out0+x+y*PlaneOut[0].u_stride)=*(p_in1_Y+x+y*PlaneIn1[0].u_stride);
                }
                else if( (alphaBlendLevelMin < alphaMaskValue)&&
                        (alphaMaskValue <= alphaBlendLevelMax ) )
                {
                    /* We blend "old and new image" in output plane */
                    *( p_out0+x+y*PlaneOut[0].u_stride)=(M4VIFI_UInt8)
                        (( (alphaMaskValue-alphaBlendLevelMin)*( *(p_in1_Y+x+y*PlaneIn1[0].u_stride))
                            +(alphaBlendLevelMax-alphaMaskValue)\
                                *( *(p_in2_Y+x+y*PlaneIn2[0].u_stride)) )/alphaBlendRange );
                }
                else
                {
                    /* We take "new image" in output plane */
                    *( p_out0+x+y*PlaneOut[0].u_stride)=*(p_in2_Y+x+y*PlaneIn2[0].u_stride);
                }
            }
        }
    }

    M4OSA_TRACE1_0("M4xVSS_AlphaMagicBlending_NV12 end");

    return(err);
}


#define M4XXX_SampleAddress_X86(plane, x, y)  ( (plane).pac_data + (plane).u_topleft + (y)\
     * (plane).u_stride + (x) )

static void M4XXX_CopyPlane_X86(M4VIFI_ImagePlane* dest, M4VIFI_ImagePlane* source)
{
    M4OSA_UInt32    height, width, sourceStride, destStride, y;
    M4OSA_MemAddr8    sourceWalk, destWalk;

    /* cache the vars used in the loop so as to avoid them being repeatedly fetched and
     recomputed from memory. */
    height = dest->u_height;
    width = dest->u_width;

    sourceWalk = (M4OSA_MemAddr8)M4XXX_SampleAddress_X86(*source, 0, 0);
    sourceStride = source->u_stride;

    destWalk = (M4OSA_MemAddr8)M4XXX_SampleAddress_X86(*dest, 0, 0);
    destStride = dest->u_stride;

    for (y=0; y<height; y++)
    {
        memcpy((void *)destWalk, (void *)sourceWalk, width);
        destWalk += destStride;
        sourceWalk += sourceStride;
    }
}

static M4OSA_ERR M4xVSS_VerticalSlideTransition_NV12(M4VIFI_ImagePlane* topPlane,
    M4VIFI_ImagePlane* bottomPlane, M4VIFI_ImagePlane *PlaneOut,
    M4OSA_UInt32 shiftUV)
{
    M4OSA_UInt32 i;
    M4OSA_TRACE1_0("M4xVSS_VerticalSlideTransition_NV12 begin");

    /* Do three loops, one for each plane type, in order to avoid having too many buffers
    "hot" at the same time (better for cache). */
    for (i=0; i<2; i++)
    {
        M4OSA_UInt32    topPartHeight, bottomPartHeight, width, sourceStride, destStride, y;
        M4OSA_MemAddr8  sourceWalk, destWalk;

        /* cache the vars used in the loop so as to avoid them being repeatedly fetched and
         recomputed from memory. */

        if (0 == i) /* Y plane */
        {
            bottomPartHeight = 2*shiftUV;
        }
        else /* U and V planes */
        {
            bottomPartHeight = shiftUV;
        }
        topPartHeight = PlaneOut[i].u_height - bottomPartHeight;
        width = PlaneOut[i].u_width;

        sourceWalk = (M4OSA_MemAddr8)M4XXX_SampleAddress_X86(topPlane[i], 0, bottomPartHeight);
        sourceStride = topPlane[i].u_stride;

        destWalk = (M4OSA_MemAddr8)M4XXX_SampleAddress_X86(PlaneOut[i], 0, 0);
        destStride = PlaneOut[i].u_stride;

        /* First the part from the top source clip frame. */
        for (y=0; y<topPartHeight; y++)
        {
            memcpy((void *)destWalk, (void *)sourceWalk, width);
            destWalk += destStride;
            sourceWalk += sourceStride;
        }

        /* and now change the vars to copy the part from the bottom source clip frame. */
        sourceWalk = (M4OSA_MemAddr8)M4XXX_SampleAddress_X86(bottomPlane[i], 0, 0);
        sourceStride = bottomPlane[i].u_stride;

        /* destWalk is already at M4XXX_SampleAddress(PlaneOut[i], 0, topPartHeight) */

        for (y=0; y<bottomPartHeight; y++)
        {
            memcpy((void *)destWalk, (void *)sourceWalk, width);
            destWalk += destStride;
            sourceWalk += sourceStride;
        }
    }

    M4OSA_TRACE1_0("M4xVSS_VerticalSlideTransition_NV12 end");
    return M4NO_ERROR;
}

static M4OSA_ERR M4xVSS_HorizontalSlideTransition_NV12(M4VIFI_ImagePlane* leftPlane,
    M4VIFI_ImagePlane* rightPlane, M4VIFI_ImagePlane *PlaneOut,
    M4OSA_UInt32 shiftUV)
{
    M4OSA_UInt32 i, y;
    /* If we shifted by exactly 0, or by the width of the target image, then we would get the left
    frame or the right frame, respectively. These cases aren't handled too well by the general
    handling, since they result in 0-size memcopies, so might as well particularize them. */

    M4OSA_TRACE1_0("M4xVSS_HorizontalSlideTransition_NV12 begin");

    if (0 == shiftUV)    /* output left frame */
    {
        for (i = 0; i<2; i++) /* for each YUV plane */
        {
            M4XXX_CopyPlane_X86(&(PlaneOut[i]), &(leftPlane[i]));
        }

        return M4NO_ERROR;
    }

    if (PlaneOut[1].u_width == shiftUV) /* output right frame */
    {
        for (i = 0; i<2; i++) /* for each YUV plane */
        {
            M4XXX_CopyPlane_X86(&(PlaneOut[i]), &(rightPlane[i]));
        }

        return M4NO_ERROR;
    }

    /* Do three loops, one for each plane type, in order to avoid having too many buffers
    "hot" at the same time (better for cache). */
    for (i=0; i<2; i++)
    {
        M4OSA_UInt32    height, leftPartWidth, rightPartWidth;
        M4OSA_UInt32    leftStride, rightStride, destStride;
        M4OSA_MemAddr8  leftWalk, rightWalk, destWalkLeft, destWalkRight;

        /* cache the vars used in the loop so as to avoid them being repeatedly fetched
        and recomputed from memory. */
        height = PlaneOut[i].u_height;

        rightPartWidth = 2*shiftUV;
        leftPartWidth = PlaneOut[i].u_width - rightPartWidth;

        leftWalk = (M4OSA_MemAddr8)M4XXX_SampleAddress_X86(leftPlane[i], rightPartWidth, 0);
        leftStride = leftPlane[i].u_stride;

        rightWalk = (M4OSA_MemAddr8)M4XXX_SampleAddress_X86(rightPlane[i], 0, 0);
        rightStride = rightPlane[i].u_stride;

        destWalkLeft = (M4OSA_MemAddr8)M4XXX_SampleAddress_X86(PlaneOut[i], 0, 0);
        destWalkRight = (M4OSA_MemAddr8)M4XXX_SampleAddress_X86(PlaneOut[i], leftPartWidth, 0);
        destStride = PlaneOut[i].u_stride;

        for (y=0; y<height; y++)
        {
            memcpy((void *)destWalkLeft, (void *)leftWalk, leftPartWidth);
            leftWalk += leftStride;

            memcpy((void *)destWalkRight, (void *)rightWalk, rightPartWidth);
            rightWalk += rightStride;

            destWalkLeft += destStride;
            destWalkRight += destStride;
        }
    }

    M4OSA_TRACE1_0("M4xVSS_HorizontalSlideTransition_NV12 end");

    return M4NO_ERROR;
}


M4OSA_ERR M4xVSS_SlideTransition_NV12(M4OSA_Void *userData,
    M4VIFI_ImagePlane *PlaneIn1, M4VIFI_ImagePlane *PlaneIn2,
    M4VIFI_ImagePlane *PlaneOut, M4VSS3GPP_ExternalProgress *pProgress,
    M4OSA_UInt32 uiTransitionKind)
{
    M4xVSS_internal_SlideTransitionSettings* settings =
         (M4xVSS_internal_SlideTransitionSettings*)userData;
    M4OSA_UInt32    shiftUV;

    M4OSA_TRACE1_0("inside M4xVSS_SlideTransition_NV12");
    if ((M4xVSS_SlideTransition_RightOutLeftIn == settings->direction)
        || (M4xVSS_SlideTransition_LeftOutRightIn == settings->direction) )
    {
        /* horizontal slide */
        shiftUV = ((PlaneOut[1]).u_width/2 * pProgress->uiProgress)/1000;
        M4OSA_TRACE1_2("M4xVSS_SlideTransition_NV12 upper: shiftUV = %d,progress = %d",
            shiftUV,pProgress->uiProgress );
        if (M4xVSS_SlideTransition_RightOutLeftIn == settings->direction)
        {
            /* Put the previous clip frame right, the next clip frame left, and reverse shiftUV
            (since it's a shift from the left frame) so that we start out on the right
            (i.e. not left) frame, it
            being from the previous clip. */
            return M4xVSS_HorizontalSlideTransition_NV12(PlaneIn2, PlaneIn1, PlaneOut,
                 (PlaneOut[1]).u_width/2 - shiftUV);
        }
        else /* Left out, right in*/
        {
            return M4xVSS_HorizontalSlideTransition_NV12(PlaneIn1, PlaneIn2, PlaneOut, shiftUV);
        }
    }
    else
    {
        /* vertical slide */
        shiftUV = ((PlaneOut[1]).u_height * pProgress->uiProgress)/1000;
        M4OSA_TRACE1_2("M4xVSS_SlideTransition_NV12 bottom: shiftUV = %d,progress = %d",shiftUV,
            pProgress->uiProgress );
        if (M4xVSS_SlideTransition_TopOutBottomIn == settings->direction)
        {
            /* Put the previous clip frame top, the next clip frame bottom. */
            return M4xVSS_VerticalSlideTransition_NV12(PlaneIn1, PlaneIn2, PlaneOut, shiftUV);
        }
        else /* Bottom out, top in */
        {
            return M4xVSS_VerticalSlideTransition_NV12(PlaneIn2, PlaneIn1, PlaneOut,
                (PlaneOut[1]).u_height - shiftUV);
        }
    }
}

/**
 ******************************************************************************
 * prototype    M4xVSS_FadeBlackTransition_NV12(M4OSA_Void *pFunctionContext,
 *                                                    M4VIFI_ImagePlane *PlaneIn,
 *                                                    M4VIFI_ImagePlane *PlaneOut,
 *                                                    M4VSS3GPP_ExternalProgress *pProgress,
 *                                                    M4OSA_UInt32 uiEffectKind)
 *
 * @brief    This function apply a fade to black and then a fade from black
 * @note
 * @param    pFunctionContext(IN) Contains which color to apply (not very clean ...)
 * @param    PlaneIn            (IN) Input NV12 planar
 * @param    PlaneOut        (IN/OUT) Output NV12 planar
 * @param    pProgress        (IN/OUT) Progress indication (0-100)
 * @param    uiEffectKind    (IN) Unused
 *
 * @return    M4VIFI_OK:    No error
 ******************************************************************************
 */
M4OSA_ERR M4xVSS_FadeBlackTransition_NV12(M4OSA_Void *userData,
    M4VIFI_ImagePlane *PlaneIn1, M4VIFI_ImagePlane *PlaneIn2,
    M4VIFI_ImagePlane *PlaneOut, M4VSS3GPP_ExternalProgress *pProgress,
    M4OSA_UInt32 uiTransitionKind)
{
    M4OSA_Int32 tmp = 0;
    M4OSA_ERR err = M4NO_ERROR;

    if((pProgress->uiProgress) < 500)
    {
        /**
         * Compute where we are in the effect (scale is 0->1024) */
        tmp = (M4OSA_Int32)((1.0 - ((M4OSA_Float)(pProgress->uiProgress*2)/1000)) * 1024 );

        /**
         * Apply the darkening effect */
        err = M4VFL_modifyLumaWithScale_NV12((M4ViComImagePlane*)PlaneIn1,
             (M4ViComImagePlane*)PlaneOut, tmp, M4OSA_NULL);
        if (M4NO_ERROR != err)
        {
            M4OSA_TRACE1_1("M4xVSS_FadeBlackTransition_NV12: M4VFL_modifyLumaWithScale_NV12 returns\
                 error 0x%x, returning M4VSS3GPP_ERR_LUMA_FILTER_ERROR", err);
            return M4VSS3GPP_ERR_LUMA_FILTER_ERROR;
        }
    }
    else
    {
        /**
         * Compute where we are in the effect (scale is 0->1024). */
        tmp = (M4OSA_Int32)((((M4OSA_Float)(((pProgress->uiProgress-500)*2))/1000)) * 1024);

        /**
         * Apply the darkening effect */
        err = M4VFL_modifyLumaWithScale_NV12((M4ViComImagePlane*)PlaneIn2,
             (M4ViComImagePlane*)PlaneOut, tmp, M4OSA_NULL);
        if (M4NO_ERROR != err)
        {
            M4OSA_TRACE1_1("M4xVSS_FadeBlackTransition_NV12:\
                 M4VFL_modifyLumaWithScale_NV12 returns error 0x%x,\
                     returning M4VSS3GPP_ERR_LUMA_FILTER_ERROR", err);
            return M4VSS3GPP_ERR_LUMA_FILTER_ERROR;
        }
    }

    return M4VIFI_OK;
}

