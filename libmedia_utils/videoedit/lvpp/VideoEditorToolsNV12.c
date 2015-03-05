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

#define LOG_NDEBUG 1
#define LOG_TAG "VideoEditorToolsNV12"
#include <utils/Log.h>

#include "VideoEditorToolsNV12.h"
#define M4VIFI_ALLOC_FAILURE 10

static M4VIFI_UInt8 M4VIFI_SemiplanarYUV420toYUV420_X86(void *user_data,
    M4VIFI_ImagePlane *PlaneIn, M4VIFI_ImagePlane *PlaneOut )
{

     M4VIFI_UInt32 i;
     M4VIFI_UInt8 *p_buf_src, *p_buf_dest, *p_buf_src_u, *p_buf_src_v;
     M4VIFI_UInt8 *p_buf_dest_u,*p_buf_dest_v,*p_buf_src_uv;
     M4VIFI_UInt8     return_code = M4VIFI_OK;

     /* the filter is implemented with the assumption that the width is equal to stride */
     if(PlaneIn[0].u_width != PlaneIn[0].u_stride)
        return M4VIFI_INVALID_PARAM;

     /* The input Y Plane is the same as the output Y Plane */
     p_buf_src = &(PlaneIn[0].pac_data[PlaneIn[0].u_topleft]);
     p_buf_dest = &(PlaneOut[0].pac_data[PlaneOut[0].u_topleft]);
     memcpy((void *)p_buf_dest,(void *)p_buf_src ,
         PlaneOut[0].u_width * PlaneOut[0].u_height);

     /* The U and V components are planar. The need to be made interleaved */
     p_buf_src_uv = &(PlaneIn[1].pac_data[PlaneIn[1].u_topleft]);
     p_buf_dest_u  = &(PlaneOut[1].pac_data[PlaneOut[1].u_topleft]);
     p_buf_dest_v  = &(PlaneOut[2].pac_data[PlaneOut[2].u_topleft]);

     for(i = 0; i < PlaneOut[1].u_width*PlaneOut[1].u_height; i++)
     {
        *p_buf_dest_u++ = *p_buf_src_uv++;
        *p_buf_dest_v++ = *p_buf_src_uv++;
     }
     return return_code;
}

/**
 ***********************************************************************************************
 * M4VIFI_UInt8 M4VIFI_ResizeBilinearYUV420toYUV420_X86(void *pUserData, M4VIFI_ImagePlane *pPlaneIn,
 *                                                                  M4VIFI_ImagePlane *pPlaneOut)
 * @author  David Dana (PHILIPS Software)
 * @brief   Resizes YUV420 Planar plane.
 * @note    Basic structure of the function
 *          Loop on each row (step 2)
 *              Loop on each column (step 2)
 *                  Get four Y samples and 1 U & V sample
 *                  Resize the Y with corresponing U and V samples
 *                  Place the YUV in the ouput plane
 *              end loop column
 *          end loop row
 *          For resizing bilinear interpolation linearly interpolates along
 *          each row, and then uses that result in a linear interpolation down each column.
 *          Each estimated pixel in the output image is a weighted
 *          combination of its four neighbours. The ratio of compression
 *          or dilatation is estimated using input and output sizes.
 * @param   pUserData: (IN) User Data
 * @param   pPlaneIn: (IN) Pointer to YUV420 (Planar) plane buffer
 * @param   pPlaneOut: (OUT) Pointer to YUV420 (Planar) plane
 * @return  M4VIFI_OK: there is no error
 * @return  M4VIFI_ILLEGAL_FRAME_HEIGHT: Error in height
 * @return  M4VIFI_ILLEGAL_FRAME_WIDTH:  Error in width
 ***********************************************************************************************
*/

static M4VIFI_UInt8 M4VIFI_ResizeBilinearYUV420toYUV420_X86(void *pUserData,
    M4VIFI_ImagePlane *pPlaneIn, M4VIFI_ImagePlane *pPlaneOut)
{
    M4VIFI_UInt8    *pu8_data_in, *pu8_data_out, *pu8dum;
    M4VIFI_UInt32   u32_plane;
    M4VIFI_UInt32   u32_width_in, u32_width_out, u32_height_in, u32_height_out;
    M4VIFI_UInt32   u32_stride_in, u32_stride_out;
    M4VIFI_UInt32   u32_x_inc, u32_y_inc;
    M4VIFI_UInt32   u32_x_accum, u32_y_accum, u32_x_accum_start;
    M4VIFI_UInt32   u32_width, u32_height;
    M4VIFI_UInt32   u32_y_frac;
    M4VIFI_UInt32   u32_x_frac;
    M4VIFI_UInt32   u32_temp_value;
    M4VIFI_UInt8    *pu8_src_top;
    M4VIFI_UInt8    *pu8_src_bottom;

    M4VIFI_UInt8    u8Wflag = 0;
    M4VIFI_UInt8    u8Hflag = 0;
    M4VIFI_UInt32   loop = 0;

    /*
     If input width is equal to output width and input height equal to
     output height then M4VIFI_YUV420toYUV420 is called.
    */
    if ((pPlaneIn[0].u_height == pPlaneOut[0].u_height) &&
              (pPlaneIn[0].u_width == pPlaneOut[0].u_width))
    {
        return M4VIFI_YUV420toYUV420(pUserData, pPlaneIn, pPlaneOut);
    }

    /* Check for the YUV width and height are even */
    if ((IS_EVEN(pPlaneIn[0].u_height) == FALSE)    ||
        (IS_EVEN(pPlaneOut[0].u_height) == FALSE))
    {
        return M4VIFI_ILLEGAL_FRAME_HEIGHT;
    }

    if ((IS_EVEN(pPlaneIn[0].u_width) == FALSE) ||
        (IS_EVEN(pPlaneOut[0].u_width) == FALSE))
    {
        return M4VIFI_ILLEGAL_FRAME_WIDTH;
    }

    /* Loop on planes */
    for(u32_plane = 0;u32_plane < PLANES;u32_plane++)
    {
        /* Set the working pointers at the beginning of the input/output data field */
        pu8_data_in     = pPlaneIn[u32_plane].pac_data + pPlaneIn[u32_plane].u_topleft;
        pu8_data_out    = pPlaneOut[u32_plane].pac_data + pPlaneOut[u32_plane].u_topleft;

        /* Get the memory jump corresponding to a row jump */
        u32_stride_in   = pPlaneIn[u32_plane].u_stride;
        u32_stride_out  = pPlaneOut[u32_plane].u_stride;

        /* Set the bounds of the active image */
        u32_width_in    = pPlaneIn[u32_plane].u_width;
        u32_height_in   = pPlaneIn[u32_plane].u_height;

        u32_width_out   = pPlaneOut[u32_plane].u_width;
        u32_height_out  = pPlaneOut[u32_plane].u_height;

        /*
        For the case , width_out = width_in , set the flag to avoid
        accessing one column beyond the input width.In this case the last
        column is replicated for processing
        */
        if (u32_width_out == u32_width_in) {
            u32_width_out = u32_width_out-1;
            u8Wflag = 1;
        }

        /* Compute horizontal ratio between src and destination width.*/
        if (u32_width_out >= u32_width_in)
        {
            u32_x_inc   = ((u32_width_in-1) * MAX_SHORT) / (u32_width_out-1);
        }
        else
        {
            u32_x_inc   = (u32_width_in * MAX_SHORT) / (u32_width_out);
        }

        /*
        For the case , height_out = height_in , set the flag to avoid
        accessing one row beyond the input height.In this case the last
        row is replicated for processing
        */
        if (u32_height_out == u32_height_in) {
            u32_height_out = u32_height_out-1;
            u8Hflag = 1;
        }

        /* Compute vertical ratio between src and destination height.*/
        if (u32_height_out >= u32_height_in)
        {
            u32_y_inc   = ((u32_height_in - 1) * MAX_SHORT) / (u32_height_out-1);
        }
        else
        {
            u32_y_inc = (u32_height_in * MAX_SHORT) / (u32_height_out);
        }

        /*
        Calculate initial accumulator value : u32_y_accum_start.
        u32_y_accum_start is coded on 15 bits, and represents a value
        between 0 and 0.5
        */
        if (u32_y_inc >= MAX_SHORT)
        {
        /*
        Keep the fractionnal part, assimung that integer  part is coded
        on the 16 high bits and the fractional on the 15 low bits
        */
            u32_y_accum = u32_y_inc & 0xffff;

            if (!u32_y_accum)
            {
                u32_y_accum = MAX_SHORT;
            }

            u32_y_accum >>= 1;
        }
        else
        {
            u32_y_accum = 0;
        }

        /*
        Calculate initial accumulator value : u32_x_accum_start.
        u32_x_accum_start is coded on 15 bits, and represents a value
        between 0 and 0.5
        */
        if (u32_x_inc >= MAX_SHORT)
        {
            u32_x_accum_start = u32_x_inc & 0xffff;

            if (!u32_x_accum_start)
            {
                u32_x_accum_start = MAX_SHORT;
            }

            u32_x_accum_start >>= 1;
        }
        else
        {
            u32_x_accum_start = 0;
        }

        u32_height = u32_height_out;

        /*
        Bilinear interpolation linearly interpolates along each row, and
        then uses that result in a linear interpolation donw each column.
        Each estimated pixel in the output image is a weighted combination
        of its four neighbours according to the formula:
        F(p',q')=f(p,q)R(-a)R(b)+f(p,q-1)R(-a)R(b-1)+f(p+1,q)R(1-a)R(b)+
        f(p+&,q+1)R(1-a)R(b-1) with  R(x) = / x+1  -1 =< x =< 0 \ 1-x
        0 =< x =< 1 and a (resp. b)weighting coefficient is the distance
        from the nearest neighbor in the p (resp. q) direction
        */

        do { /* Scan all the row */

            /* Vertical weight factor */
            u32_y_frac = (u32_y_accum>>12)&15;

            /* Reinit accumulator */
            u32_x_accum = u32_x_accum_start;

            u32_width = u32_width_out;

            do { /* Scan along each row */
                pu8_src_top = pu8_data_in + (u32_x_accum >> 16);
                pu8_src_bottom = pu8_src_top + u32_stride_in;
                u32_x_frac = (u32_x_accum >> 12)&15; /* Horizontal weight factor */

                /* Weighted combination */
                u32_temp_value = (M4VIFI_UInt8)(((pu8_src_top[0]*(16-u32_x_frac) +
                                                 pu8_src_top[1]*u32_x_frac)*(16-u32_y_frac) +
                                                (pu8_src_bottom[0]*(16-u32_x_frac) +
                                                 pu8_src_bottom[1]*u32_x_frac)*u32_y_frac )>>8);

                *pu8_data_out++ = (M4VIFI_UInt8)u32_temp_value;

                /* Update horizontal accumulator */
                u32_x_accum += u32_x_inc;
            } while(--u32_width);

            /*
               This u8Wflag flag gets in to effect if input and output
               width is same, and height may be different. So previous
               pixel is replicated here
            */
            if (u8Wflag) {
                *pu8_data_out = (M4VIFI_UInt8)u32_temp_value;
            }

            pu8dum = (pu8_data_out-u32_width_out);
            pu8_data_out = pu8_data_out + u32_stride_out - u32_width_out;

            /* Update vertical accumulator */
            u32_y_accum += u32_y_inc;
            if (u32_y_accum>>16) {
                pu8_data_in = pu8_data_in + (u32_y_accum >> 16) * u32_stride_in;
                u32_y_accum &= 0xffff;
            }
        } while(--u32_height);

        /*
        This u8Hflag flag gets in to effect if input and output height
        is same, and width may be different. So previous pixel row is
        replicated here
        */
        if (u8Hflag) {
            for(loop =0; loop < (u32_width_out+u8Wflag); loop++) {
                *pu8_data_out++ = (M4VIFI_UInt8)*pu8dum++;
            }
        }
    }

    return M4VIFI_OK;
}

/**
 *********************************************************************************************
 * M4VIFI_UInt8 M4VIFI_ResizeBilinearYUV420toBGR565_X86(void *pContext, M4VIFI_ImagePlane *pPlaneIn,
 *                                                        M4VIFI_ImagePlane *pPlaneOut)
 * @brief   Resize YUV420 plane and converts to BGR565 with +90 rotation.
 * @note    Basic sturture of the function
 *          Loop on each row (step 2)
 *              Loop on each column (step 2)
 *                  Get four Y samples and 1 u & V sample
 *                  Resize the Y with corresponing U and V samples
 *                  Compute the four corresponding R G B values
 *                  Place the R G B in the ouput plane in rotated fashion
 *              end loop column
 *          end loop row
 *          For resizing bilinear interpolation linearly interpolates along
 *          each row, and then uses that result in a linear interpolation down each column.
 *          Each estimated pixel in the output image is a weighted
 *          combination of its four neighbours. The ratio of compression
 *          or dilatation is estimated using input and output sizes.
 * @param   pPlaneIn: (IN) Pointer to YUV plane buffer
 * @param   pContext: (IN) Context Pointer
 * @param   pPlaneOut: (OUT) Pointer to BGR565 Plane
 * @return  M4VIFI_OK: there is no error
 * @return  M4VIFI_ILLEGAL_FRAME_HEIGHT: YUV Plane height is ODD
 * @return  M4VIFI_ILLEGAL_FRAME_WIDTH:  YUV Plane width is ODD
 *********************************************************************************************
*/
static M4VIFI_UInt8 M4VIFI_ResizeBilinearYUV420toBGR565_X86(void* pContext,
    M4VIFI_ImagePlane *pPlaneIn, M4VIFI_ImagePlane *pPlaneOut)
{
    M4VIFI_UInt8    *pu8_data_in[PLANES], *pu8_data_in1[PLANES],*pu8_data_out;
    M4VIFI_UInt32   *pu32_rgb_data_current, *pu32_rgb_data_next, *pu32_rgb_data_start;

    M4VIFI_UInt32   u32_width_in[PLANES], u32_width_out, u32_height_in[PLANES], u32_height_out;
    M4VIFI_UInt32   u32_stride_in[PLANES];
    M4VIFI_UInt32   u32_stride_out, u32_stride2_out, u32_width2_RGB, u32_height2_RGB;
    M4VIFI_UInt32   u32_x_inc[PLANES], u32_y_inc[PLANES];
    M4VIFI_UInt32   u32_x_accum_Y, u32_x_accum_U, u32_x_accum_start;
    M4VIFI_UInt32   u32_y_accum_Y, u32_y_accum_U;
    M4VIFI_UInt32   u32_x_frac_Y, u32_x_frac_U, u32_y_frac_Y,u32_y_frac_U;
    M4VIFI_Int32    U_32, V_32, Y_32, Yval_32;
    M4VIFI_UInt8    u8_Red, u8_Green, u8_Blue;
    M4VIFI_UInt32   u32_row, u32_col;

    M4VIFI_UInt32   u32_plane;
    M4VIFI_UInt32   u32_rgb_temp1, u32_rgb_temp2;
    M4VIFI_UInt32   u32_rgb_temp3,u32_rgb_temp4;
    M4VIFI_UInt32   u32_check_size;

    M4VIFI_UInt8    *pu8_src_top_Y,*pu8_src_top_U,*pu8_src_top_V ;
    M4VIFI_UInt8    *pu8_src_bottom_Y, *pu8_src_bottom_U, *pu8_src_bottom_V;

    /* Check for the YUV width and height are even */
    u32_check_size = IS_EVEN(pPlaneIn[0].u_height);
    if( u32_check_size == FALSE )
    {
        return M4VIFI_ILLEGAL_FRAME_HEIGHT;
    }
    u32_check_size = IS_EVEN(pPlaneIn[0].u_width);
    if (u32_check_size == FALSE )
    {
        return M4VIFI_ILLEGAL_FRAME_WIDTH;

    }
    /* Make the ouput width and height as even */
    pPlaneOut->u_height = pPlaneOut->u_height & 0xFFFFFFFE;
    pPlaneOut->u_width = pPlaneOut->u_width & 0xFFFFFFFE;
    pPlaneOut->u_stride = pPlaneOut->u_stride & 0xFFFFFFFC;

    /* Assignment of output pointer */
    pu8_data_out    = pPlaneOut->pac_data + pPlaneOut->u_topleft;
    /* Assignment of output width(rotated) */
    u32_width_out   = pPlaneOut->u_width;
    /* Assignment of output height(rotated) */
    u32_height_out  = pPlaneOut->u_height;

    u32_width2_RGB  = pPlaneOut->u_width >> 1;
    u32_height2_RGB = pPlaneOut->u_height >> 1;

    u32_stride_out = pPlaneOut->u_stride >> 1;
    u32_stride2_out = pPlaneOut->u_stride >> 2;

    for(u32_plane = 0; u32_plane < PLANES; u32_plane++)
    {
        /* Set the working pointers at the beginning of the input/output data field */
        pu8_data_in[u32_plane] = pPlaneIn[u32_plane].pac_data + pPlaneIn[u32_plane].u_topleft;

        /* Get the memory jump corresponding to a row jump */
        u32_stride_in[u32_plane] = pPlaneIn[u32_plane].u_stride;

        /* Set the bounds of the active image */
        u32_width_in[u32_plane] = pPlaneIn[u32_plane].u_width;
        u32_height_in[u32_plane] = pPlaneIn[u32_plane].u_height;
    }
    /* Compute horizontal ratio between src and destination width for Y Plane. */
    if (u32_width_out >= u32_width_in[YPlane])
    {
        u32_x_inc[YPlane]   = ((u32_width_in[YPlane]-1) * MAX_SHORT) / (u32_width_out-1);
    }
    else
    {
        u32_x_inc[YPlane]   = (u32_width_in[YPlane] * MAX_SHORT) / (u32_width_out);
    }

    /* Compute vertical ratio between src and destination height for Y Plane.*/
    if (u32_height_out >= u32_height_in[YPlane])
    {
        u32_y_inc[YPlane]   = ((u32_height_in[YPlane]-1) * MAX_SHORT) / (u32_height_out-1);
    }
    else
    {
        u32_y_inc[YPlane] = (u32_height_in[YPlane] * MAX_SHORT) / (u32_height_out);
    }

    /* Compute horizontal ratio between src and destination width for U and V Planes. */
    if (u32_width2_RGB >= u32_width_in[UPlane])
    {
        u32_x_inc[UPlane]   = ((u32_width_in[UPlane]-1) * MAX_SHORT) / (u32_width2_RGB-1);
    }
    else
    {
        u32_x_inc[UPlane]   = (u32_width_in[UPlane] * MAX_SHORT) / (u32_width2_RGB);
    }

    /* Compute vertical ratio between src and destination height for U and V Planes. */

    if (u32_height2_RGB >= u32_height_in[UPlane])
    {
        u32_y_inc[UPlane]   = ((u32_height_in[UPlane]-1) * MAX_SHORT) / (u32_height2_RGB-1);
    }
    else
    {
        u32_y_inc[UPlane]  = (u32_height_in[UPlane] * MAX_SHORT) / (u32_height2_RGB);
    }

    u32_y_inc[VPlane] = u32_y_inc[UPlane];
    u32_x_inc[VPlane] = u32_x_inc[UPlane];

    /*
        Calculate initial accumulator value : u32_y_accum_start.
        u32_y_accum_start is coded on 15 bits,and represents a value between 0 and 0.5
    */
    if (u32_y_inc[YPlane] > MAX_SHORT)
    {
        /*
            Keep the fractionnal part, assimung that integer  part is coded on the 16 high bits,
            and the fractionnal on the 15 low bits
        */
        u32_y_accum_Y = u32_y_inc[YPlane] & 0xffff;
        u32_y_accum_U = u32_y_inc[UPlane] & 0xffff;

        if (!u32_y_accum_Y)
        {
            u32_y_accum_Y = MAX_SHORT;
            u32_y_accum_U = MAX_SHORT;
        }
        u32_y_accum_Y >>= 1;
        u32_y_accum_U >>= 1;
    }
    else
    {
        u32_y_accum_Y = 0;
        u32_y_accum_U = 0;

    }

    /*
        Calculate initial accumulator value : u32_x_accum_start.
        u32_x_accum_start is coded on 15 bits, and represents a value between 0 and 0.5
    */
    if (u32_x_inc[YPlane] > MAX_SHORT)
    {
        u32_x_accum_start = u32_x_inc[YPlane] & 0xffff;

        if (!u32_x_accum_start)
        {
            u32_x_accum_start = MAX_SHORT;
        }

        u32_x_accum_start >>= 1;
    }
    else
    {
        u32_x_accum_start = 0;
    }

    pu32_rgb_data_start = (M4VIFI_UInt32*)pu8_data_out;

    /*
        Bilinear interpolation linearly interpolates along each row, and then uses that
        result in a linear interpolation donw each column. Each estimated pixel in the
        output image is a weighted combination of its four neighbours according to the formula :
        F(p',q')=f(p,q)R(-a)R(b)+f(p,q-1)R(-a)R(b-1)+f(p+1,q)R(1-a)R(b)+f(p+&,q+1)R(1-a)R(b-1)
        with  R(x) = / x+1  -1 =< x =< 0 \ 1-x  0 =< x =< 1 and a (resp. b) weighting coefficient
        is the distance from the nearest neighbor in the p (resp. q) direction
    */
    for (u32_row = u32_height_out; u32_row != 0; u32_row -= 2)
    {
        u32_x_accum_Y = u32_x_accum_start;
        u32_x_accum_U = u32_x_accum_start;

        /* Vertical weight factor */
        u32_y_frac_Y = (u32_y_accum_Y >> 12) & 15;
        u32_y_frac_U = (u32_y_accum_U >> 12) & 15;

        /* RGB current line position pointer */
        pu32_rgb_data_current = pu32_rgb_data_start ;

        /* RGB next line position pointer */
        pu32_rgb_data_next    = pu32_rgb_data_current + (u32_stride2_out);

        /* Y Plane next row pointer */
        pu8_data_in1[YPlane] = pu8_data_in[YPlane];

        u32_rgb_temp3 = u32_y_accum_Y + (u32_y_inc[YPlane]);
        if (u32_rgb_temp3 >> 16)
        {
            pu8_data_in1[YPlane] =  pu8_data_in[YPlane] +
                                                (u32_rgb_temp3 >> 16) * (u32_stride_in[YPlane]);
            u32_rgb_temp3 &= 0xffff;
        }
        u32_rgb_temp4 = (u32_rgb_temp3 >> 12) & 15;

        for (u32_col = u32_width_out; u32_col != 0; u32_col -= 2)
        {

            /* Input Y plane elements */
            pu8_src_top_Y = pu8_data_in[YPlane] + (u32_x_accum_Y >> 16);
            pu8_src_bottom_Y = pu8_src_top_Y + u32_stride_in[YPlane];

            /* Input U Plane elements */
            pu8_src_top_U = pu8_data_in[UPlane] + (u32_x_accum_U >> 16);
            pu8_src_bottom_U = pu8_src_top_U + u32_stride_in[UPlane];

            pu8_src_top_V = pu8_data_in[VPlane] + (u32_x_accum_U >> 16);
            pu8_src_bottom_V = pu8_src_top_V + u32_stride_in[VPlane];

            /* Horizontal weight factor for Y plane */
            u32_x_frac_Y = (u32_x_accum_Y >> 12)&15;
            /* Horizontal weight factor for U and V planes */
            u32_x_frac_U = (u32_x_accum_U >> 12)&15;

            /* Weighted combination */
            U_32 = (((pu8_src_top_U[0]*(16-u32_x_frac_U) + pu8_src_top_U[1]*u32_x_frac_U)
                    *(16-u32_y_frac_U) + (pu8_src_bottom_U[0]*(16-u32_x_frac_U)
                    + pu8_src_bottom_U[1]*u32_x_frac_U)*u32_y_frac_U ) >> 8);

            V_32 = (((pu8_src_top_V[0]*(16-u32_x_frac_U) + pu8_src_top_V[1]*u32_x_frac_U)
                    *(16-u32_y_frac_U)+ (pu8_src_bottom_V[0]*(16-u32_x_frac_U)
                    + pu8_src_bottom_V[1]*u32_x_frac_U)*u32_y_frac_U ) >> 8);

            Y_32 = (((pu8_src_top_Y[0]*(16-u32_x_frac_Y) + pu8_src_top_Y[1]*u32_x_frac_Y)
                    *(16-u32_y_frac_Y) + (pu8_src_bottom_Y[0]*(16-u32_x_frac_Y)
                    + pu8_src_bottom_Y[1]*u32_x_frac_Y)*u32_y_frac_Y ) >> 8);

            u32_x_accum_U += (u32_x_inc[UPlane]);

            /* YUV to RGB */
            #ifdef __RGB_V1__
                    Yval_32 = Y_32*37;
            #else   /* __RGB_V1__v */
                    Yval_32 = Y_32*0x2568;
            #endif /* __RGB_V1__v */

                    DEMATRIX(u8_Red,u8_Green,u8_Blue,Yval_32,U_32,V_32);

            /* Pack 8 bit R,G,B to RGB565 */
            #ifdef  LITTLE_ENDIAN
                    u32_rgb_temp1 = PACK_BGR565(0,u8_Red,u8_Green,u8_Blue);
            #else   /* LITTLE_ENDIAN */
                    u32_rgb_temp1 = PACK_BGR565(16,u8_Red,u8_Green,u8_Blue);
            #endif  /* LITTLE_ENDIAN */


            pu8_src_top_Y = pu8_data_in1[YPlane]+(u32_x_accum_Y >> 16);
            pu8_src_bottom_Y = pu8_src_top_Y + u32_stride_in[YPlane];

            /* Weighted combination */
            Y_32 = (((pu8_src_top_Y[0]*(16-u32_x_frac_Y) + pu8_src_top_Y[1]*u32_x_frac_Y)
                    *(16-u32_rgb_temp4) + (pu8_src_bottom_Y[0]*(16-u32_x_frac_Y)
                    + pu8_src_bottom_Y[1]*u32_x_frac_Y)*u32_rgb_temp4 ) >> 8);

            u32_x_accum_Y += u32_x_inc[YPlane];

            /* Horizontal weight factor */
            u32_x_frac_Y = (u32_x_accum_Y >> 12)&15;

            /* YUV to RGB */
            #ifdef __RGB_V1__
                    Yval_32 = Y_32*37;
            #else   /* __RGB_V1__v */
                    Yval_32 = Y_32*0x2568;
            #endif  /* __RGB_V1__v */

            DEMATRIX(u8_Red,u8_Green,u8_Blue,Yval_32,U_32,V_32);

            /* Pack 8 bit R,G,B to RGB565 */
            #ifdef  LITTLE_ENDIAN
                    u32_rgb_temp2 = PACK_BGR565(0,u8_Red,u8_Green,u8_Blue);
            #else   /* LITTLE_ENDIAN */
                    u32_rgb_temp2 = PACK_BGR565(16,u8_Red,u8_Green,u8_Blue);
            #endif  /* LITTLE_ENDIAN */


            pu8_src_top_Y = pu8_data_in[YPlane] + (u32_x_accum_Y >> 16) ;
            pu8_src_bottom_Y = pu8_src_top_Y + u32_stride_in[YPlane];

            /* Weighted combination */
            Y_32 = (((pu8_src_top_Y[0]*(16-u32_x_frac_Y) + pu8_src_top_Y[1]*u32_x_frac_Y)
                    *(16-u32_y_frac_Y) + (pu8_src_bottom_Y[0]*(16-u32_x_frac_Y)
                    + pu8_src_bottom_Y[1]*u32_x_frac_Y)*u32_y_frac_Y ) >> 8);

            /* YUV to RGB */
            #ifdef __RGB_V1__
                    Yval_32 = Y_32*37;
            #else   /* __RGB_V1__v */
                    Yval_32 = Y_32*0x2568;
            #endif  /* __RGB_V1__v */

            DEMATRIX(u8_Red,u8_Green,u8_Blue,Yval_32,U_32,V_32);

            /* Pack 8 bit R,G,B to RGB565 */
            #ifdef  LITTLE_ENDIAN
                    *(pu32_rgb_data_current)++ = u32_rgb_temp1 |
                                                     PACK_BGR565(16,u8_Red,u8_Green,u8_Blue);
            #else   /* LITTLE_ENDIAN */
                    *(pu32_rgb_data_current)++ = u32_rgb_temp1 |
                                                     PACK_BGR565(0,u8_Red,u8_Green,u8_Blue);
            #endif  /* LITTLE_ENDIAN */


            pu8_src_top_Y = pu8_data_in1[YPlane]+ (u32_x_accum_Y >> 16);
            pu8_src_bottom_Y = pu8_src_top_Y + u32_stride_in[YPlane];

            /* Weighted combination */
            Y_32 = (((pu8_src_top_Y[0]*(16-u32_x_frac_Y) + pu8_src_top_Y[1]*u32_x_frac_Y)
                    *(16-u32_rgb_temp4) + (pu8_src_bottom_Y[0]*(16-u32_x_frac_Y)
                    + pu8_src_bottom_Y[1]*u32_x_frac_Y)*u32_rgb_temp4 )>>8);

            u32_x_accum_Y += u32_x_inc[YPlane];
            /* YUV to RGB */
            #ifdef __RGB_V1__
                    Yval_32=Y_32*37;
            #else   /* __RGB_V1__v */
                    Yval_32=Y_32*0x2568;
            #endif  /* __RGB_V1__v */

            DEMATRIX(u8_Red,u8_Green,u8_Blue,Yval_32,U_32,V_32);

            /* Pack 8 bit R,G,B to RGB565 */
            #ifdef  LITTLE_ENDIAN
                    *(pu32_rgb_data_next)++ = u32_rgb_temp2 |
                                                  PACK_BGR565(16,u8_Red,u8_Green,u8_Blue);
            #else   /* LITTLE_ENDIAN */
                    *(pu32_rgb_data_next)++ = u32_rgb_temp2 |
                                                  PACK_BGR565(0,u8_Red,u8_Green,u8_Blue);
            #endif  /* LITTLE_ENDIAN */

        }   /* End of horizontal scanning */

        u32_y_accum_Y  =  u32_rgb_temp3 + (u32_y_inc[YPlane]);
        u32_y_accum_U += (u32_y_inc[UPlane]);

        /* Y plane row update */
        if (u32_y_accum_Y >> 16)
        {
            pu8_data_in[YPlane] =  pu8_data_in1[YPlane] +
                                       ((u32_y_accum_Y >> 16) * (u32_stride_in[YPlane]));
            u32_y_accum_Y &= 0xffff;
        }
        else
        {
            pu8_data_in[YPlane] = pu8_data_in1[YPlane];
        }
        /* U and V planes row update */
        if (u32_y_accum_U >> 16)
        {
            pu8_data_in[UPlane] =  pu8_data_in[UPlane] +
                                       (u32_y_accum_U >> 16) * (u32_stride_in[UPlane]);
            pu8_data_in[VPlane] =  pu8_data_in[VPlane] +
                                       (u32_y_accum_U >> 16) * (u32_stride_in[VPlane]);
            u32_y_accum_U &= 0xffff;
        }
        /* BGR pointer Update */
        pu32_rgb_data_start += u32_stride_out;

    }   /* End of vertical scanning */
    return M4VIFI_OK;
}

/***************************************************************************
Proto:
M4VIFI_UInt8    M4VIFI_RGB888toNV12(void *pUserData, M4VIFI_ImagePlane *PlaneIn, M4VIFI_ImagePlane PlaneOut[2]);
Author:     Patrice Martinez / Philips Digital Networks - MP4Net
Purpose:    filling of the NV12 plane from a BGR24 plane
Abstract:   Loop on each row ( 2 rows by 2 rows )
                Loop on each column ( 2 col by 2 col )
                    Get 4 BGR samples from input data and build 4 output Y samples and each single U & V data
                end loop on col
            end loop on row

In:         RGB24 plane
InOut:      none
Out:        array of 3 M4VIFI_ImagePlane structures
Modified:   ML: RGB function modified to BGR.
***************************************************************************/
M4VIFI_UInt8 M4VIFI_RGB888toNV12(void *pUserData,
    M4VIFI_ImagePlane *PlaneIn, M4VIFI_ImagePlane *PlaneOut)
{

    M4VIFI_UInt32   u32_width, u32_height;
    M4VIFI_UInt32   u32_stride_Y, u32_stride2_Y, u32_stride_UV, u32_stride_rgb, u32_stride_2rgb;
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

    /* check sizes */
    if( (PlaneIn->u_height != PlaneOut[0].u_height)         ||
        (PlaneOut[0].u_height != (PlaneOut[1].u_height<<1)))
        return M4VIFI_ILLEGAL_FRAME_HEIGHT;

    if( (PlaneIn->u_width != PlaneOut[0].u_width)       ||
        (PlaneOut[0].u_width != PlaneOut[1].u_width))
        return M4VIFI_ILLEGAL_FRAME_WIDTH;


    /* set the pointer to the beginning of the output data buffers */
    pu8_y_data  = PlaneOut[0].pac_data + PlaneOut[0].u_topleft;
    pu8_u_data  = PlaneOut[1].pac_data + PlaneOut[1].u_topleft;
    pu8_v_data  = pu8_u_data + 1;

    /* idem for input buffer */
    pu8_rgbn_data   = PlaneIn->pac_data + PlaneIn->u_topleft;

    /* get the size of the output image */
    u32_width   = PlaneOut[0].u_width;
    u32_height  = PlaneOut[0].u_height;

    /* set the size of the memory jumps corresponding to row jump in each output plane */
    u32_stride_Y = PlaneOut[0].u_stride;
    u32_stride2_Y= u32_stride_Y << 1;
    u32_stride_UV = PlaneOut[1].u_stride;

    /* idem for input plane */
    u32_stride_rgb = PlaneIn->u_stride;
    u32_stride_2rgb = u32_stride_rgb << 1;

    /* loop on each row of the output image, input coordinates are estimated from output ones */
    /* two YUV rows are computed at each pass */
    for (u32_row = u32_height ;u32_row != 0; u32_row -=2)
    {
        /* update working pointers */
        pu8_yn  = pu8_y_data;
        pu8_ys  = pu8_yn + u32_stride_Y;

        pu8_u   = pu8_u_data;
        pu8_v   = pu8_v_data;

        pu8_rgbn= pu8_rgbn_data;

        /* loop on each column of the output image*/
        for (u32_col = u32_width; u32_col != 0 ; u32_col -=2)
        {
            /* get RGB samples of 4 pixels */
            GET_RGB24(i32_r00, i32_g00, i32_b00, pu8_rgbn, 0);
            GET_RGB24(i32_r10, i32_g10, i32_b10, pu8_rgbn, CST_RGB_24_SIZE);
            GET_RGB24(i32_r01, i32_g01, i32_b01, pu8_rgbn, u32_stride_rgb);
            GET_RGB24(i32_r11, i32_g11, i32_b11, pu8_rgbn, u32_stride_rgb + CST_RGB_24_SIZE);

            i32_u00 = U24(i32_r00, i32_g00, i32_b00);
            i32_v00 = V24(i32_r00, i32_g00, i32_b00);
            i32_y00 = Y24(i32_r00, i32_g00, i32_b00);       /* matrix luminance */
            pu8_yn[0]= (M4VIFI_UInt8)i32_y00;

            i32_u10 = U24(i32_r10, i32_g10, i32_b10);
            i32_v10 = V24(i32_r10, i32_g10, i32_b10);
            i32_y10 = Y24(i32_r10, i32_g10, i32_b10);
            pu8_yn[1]= (M4VIFI_UInt8)i32_y10;

            i32_u01 = U24(i32_r01, i32_g01, i32_b01);
            i32_v01 = V24(i32_r01, i32_g01, i32_b01);
            i32_y01 = Y24(i32_r01, i32_g01, i32_b01);
            pu8_ys[0]= (M4VIFI_UInt8)i32_y01;

            i32_u11 = U24(i32_r11, i32_g11, i32_b11);
            i32_v11 = V24(i32_r11, i32_g11, i32_b11);
            i32_y11 = Y24(i32_r11, i32_g11, i32_b11);
            pu8_ys[1] = (M4VIFI_UInt8)i32_y11;

            *pu8_u  = (M4VIFI_UInt8)((i32_u00 + i32_u01 + i32_u10 + i32_u11 + 2) >> 2);
            *pu8_v  = (M4VIFI_UInt8)((i32_v00 + i32_v01 + i32_v10 + i32_v11 + 2) >> 2);

            pu8_rgbn    +=  (CST_RGB_24_SIZE<<1);
            pu8_yn      += 2;
            pu8_ys      += 2;

            pu8_u += 2;
            pu8_v += 2;
        } /* end of horizontal scanning */

        pu8_y_data      += u32_stride2_Y;
        pu8_u_data      += u32_stride_UV;
        pu8_v_data      += u32_stride_UV;
        pu8_rgbn_data   += u32_stride_2rgb;


    } /* End of vertical scanning */

    return M4VIFI_OK;
}

/** NV12 to NV12 */
/**
 *******************************************************************************************
 * M4VIFI_UInt8 M4VIFI_NV12toNV12 (void *pUserData,
 *                                     M4VIFI_ImagePlane *pPlaneIn,
 *                                     M4VIFI_ImagePlane *pPlaneOut)
 * @brief   Transform NV12 image to a NV12 image.
 * @param   pUserData: (IN) User Specific Data (Unused - could be NULL)
 * @param   pPlaneIn: (IN) Pointer to NV12 plane buffer
 * @param   pPlaneOut: (OUT) Pointer to NV12 Plane
 * @return  M4VIFI_OK: there is no error
 * @return  M4VIFI_ILLEGAL_FRAME_HEIGHT: Error in plane height
 * @return  M4VIFI_ILLEGAL_FRAME_WIDTH:  Error in plane width
 *******************************************************************************************
 */

M4VIFI_UInt8 M4VIFI_NV12toNV12(void *user_data,
    M4VIFI_ImagePlane *PlaneIn, M4VIFI_ImagePlane *PlaneOut)
{
    M4VIFI_Int32 plane_number;
    M4VIFI_UInt32 i;
    M4VIFI_UInt8 *p_buf_src, *p_buf_dest;

    for (plane_number = 0; plane_number < 2; plane_number++)
    {
        p_buf_src = &(PlaneIn[plane_number].pac_data[PlaneIn[plane_number].u_topleft]);
        p_buf_dest = &(PlaneOut[plane_number].pac_data[PlaneOut[plane_number].u_topleft]);
        for (i = 0; i < PlaneOut[plane_number].u_height; i++)
        {
            memcpy((void *)p_buf_dest, (void *)p_buf_src ,PlaneOut[plane_number].u_width);
            p_buf_src += PlaneIn[plane_number].u_stride;
            p_buf_dest += PlaneOut[plane_number].u_stride;
        }
    }
    return M4VIFI_OK;
}

/**
 ***********************************************************************************************
 * M4VIFI_UInt8 M4VIFI_ResizeBilinearNV12toNV12(void *pUserData, M4VIFI_ImagePlane *pPlaneIn,
 *                                                                  M4VIFI_ImagePlane *pPlaneOut)
 * @author  David Dana (PHILIPS Software)
 * @brief   Resizes NV12 Planar plane.
 * @note    Basic structure of the function
 *          Loop on each row (step 2)
 *              Loop on each column (step 2)
 *                  Get four Y samples and 1 U & V sample
 *                  Resize the Y with corresponing U and V samples
 *                  Place the NV12 in the ouput plane
 *              end loop column
 *          end loop row
 *          For resizing bilinear interpolation linearly interpolates along
 *          each row, and then uses that result in a linear interpolation down each column.
 *          Each estimated pixel in the output image is a weighted
 *          combination of its four neighbours. The ratio of compression
 *          or dilatation is estimated using input and output sizes.
 * @param   pUserData: (IN) User Data
 * @param   pPlaneIn: (IN) Pointer to NV12 (Planar) plane buffer
 * @param   pPlaneOut: (OUT) Pointer to NV12 (Planar) plane
 * @return  M4VIFI_OK: there is no error
 * @return  M4VIFI_ILLEGAL_FRAME_HEIGHT: Error in height
 * @return  M4VIFI_ILLEGAL_FRAME_WIDTH:  Error in width
 ***********************************************************************************************
*/
M4VIFI_UInt8 M4VIFI_ResizeBilinearNV12toNV12(void *pUserData,
    M4VIFI_ImagePlane *pPlaneIn, M4VIFI_ImagePlane *pPlaneOut)
{
    M4VIFI_UInt8    *pu8_data_in, *pu8_data_out, *pu8dum;
    M4VIFI_UInt32   u32_plane;
    M4VIFI_UInt32   u32_width_in, u32_width_out, u32_height_in, u32_height_out;
    M4VIFI_UInt32   u32_stride_in, u32_stride_out;
    M4VIFI_UInt32   u32_x_inc, u32_y_inc;
    M4VIFI_UInt32   u32_x_accum, u32_y_accum, u32_x_accum_start;
    M4VIFI_UInt32   u32_width, u32_height;
    M4VIFI_UInt32   u32_y_frac;
    M4VIFI_UInt32   u32_x_frac;
    M4VIFI_UInt32   u32_temp_value,u32_temp_value1;
    M4VIFI_UInt8    *pu8_src_top;
    M4VIFI_UInt8    *pu8_src_bottom;

    M4VIFI_UInt8    u8Wflag = 0;
    M4VIFI_UInt8    u8Hflag = 0;
    M4VIFI_UInt32   loop = 0;

    LOGV("M4VIFI_ResizeBilinearNV12toNV12 begin");
    /*
     If input width is equal to output width and input height equal to
     output height then M4VIFI_NV12toNV12 is called.
    */

    LOGV("pPlaneIn[0].u_height = %d, pPlaneIn[0].u_width = %d,\
         pPlaneOut[0].u_height = %d, pPlaneOut[0].u_width = %d",
        pPlaneIn[0].u_height, pPlaneIn[0].u_width,
        pPlaneOut[0].u_height, pPlaneOut[0].u_width
    );
    LOGV("pPlaneIn[1].u_height = %d, pPlaneIn[1].u_width = %d,\
         pPlaneOut[1].u_height = %d, pPlaneOut[1].u_width = %d",
        pPlaneIn[1].u_height, pPlaneIn[1].u_width,
        pPlaneOut[1].u_height, pPlaneOut[1].u_width
    );
    if ((pPlaneIn[0].u_height == pPlaneOut[0].u_height) &&
              (pPlaneIn[0].u_width == pPlaneOut[0].u_width))
    {
        return M4VIFI_NV12toNV12(pUserData, pPlaneIn, pPlaneOut);
    }

    /* Check for the YUV width and height are even */
    if ((IS_EVEN(pPlaneIn[0].u_height) == FALSE)    ||
        (IS_EVEN(pPlaneOut[0].u_height) == FALSE))
    {
        return M4VIFI_ILLEGAL_FRAME_HEIGHT;
    }

    if ((IS_EVEN(pPlaneIn[0].u_width) == FALSE) ||
        (IS_EVEN(pPlaneOut[0].u_width) == FALSE))
    {
        return M4VIFI_ILLEGAL_FRAME_WIDTH;
    }

    /* Loop on planes */
    for(u32_plane = 0;u32_plane < 2;u32_plane++)
    {
        /* Get the memory jump corresponding to a row jump */
        u32_stride_in   = pPlaneIn[u32_plane].u_stride;
        u32_stride_out  = pPlaneOut[u32_plane].u_stride;

        /* Set the bounds of the active image */
        u32_width_in    = pPlaneIn[u32_plane].u_width;
        u32_height_in   = pPlaneIn[u32_plane].u_height;

        u32_width_out   = pPlaneOut[u32_plane].u_width;
        u32_height_out  = pPlaneOut[u32_plane].u_height;

        /*
        For the case , width_out = width_in , set the flag to avoid
        accessing one column beyond the input width.In this case the last
        column is replicated for processing
        */
        if (u32_width_out == u32_width_in) {
            u32_width_out = u32_width_out - 1 - u32_plane;
            u8Wflag = 1;
        }

        /* Compute horizontal ratio between src and destination width.*/
        if (u32_width_out >= u32_width_in)
        {
            u32_x_inc = ((u32_width_in -1 -u32_plane) * MAX_SHORT)/(u32_width_out -1 -u32_plane);
        }
        else
        {
            u32_x_inc = (u32_width_in * MAX_SHORT) / (u32_width_out);
        }

        /*
        For the case , height_out = height_in , set the flag to avoid
        accessing one row beyond the input height.In this case the last
        row is replicated for processing
        */
        if (u32_height_out == u32_height_in) {
            u32_height_out = u32_height_out-1;
            u8Hflag = 1;
        }

        /* Compute vertical ratio between src and destination height.*/
        if (u32_height_out >= u32_height_in)
        {
            u32_y_inc   = ((u32_height_in - 1) * MAX_SHORT) / (u32_height_out - 1);
        }
        else
        {
            u32_y_inc = (u32_height_in * MAX_SHORT) / (u32_height_out);
        }

        /*
        Calculate initial accumulator value : u32_y_accum_start.
        u32_y_accum_start is coded on 15 bits, and represents a value
        between 0 and 0.5
        */
        if (u32_y_inc >= MAX_SHORT)
        {
        /*
        Keep the fractionnal part, assimung that integer  part is coded
        on the 16 high bits and the fractional on the 15 low bits
        */
            u32_y_accum = u32_y_inc & 0xffff;

            if (!u32_y_accum)
            {
                u32_y_accum = MAX_SHORT;
            }

            u32_y_accum >>= 1;
        }
        else
        {
            u32_y_accum = 0;
        }


        /*
        Calculate initial accumulator value : u32_x_accum_start.
        u32_x_accum_start is coded on 15 bits, and represents a value
        between 0 and 0.5
        */
        if (u32_x_inc >= MAX_SHORT)
        {
            u32_x_accum_start = u32_x_inc & 0xffff;

            if (!u32_x_accum_start)
            {
                u32_x_accum_start = MAX_SHORT;
            }

            u32_x_accum_start >>= 1;
        }
        else
        {
            u32_x_accum_start = 0;
        }

        u32_height = u32_height_out;

        /*
        Bilinear interpolation linearly interpolates along each row, and
        then uses that result in a linear interpolation donw each column.
        Each estimated pixel in the output image is a weighted combination
        of its four neighbours according to the formula:
        F(p',q')=f(p,q)R(-a)R(b)+f(p,q-1)R(-a)R(b-1)+f(p+1,q)R(1-a)R(b)+
        f(p+&,q+1)R(1-a)R(b-1) with  R(x) = / x+1  -1 =< x =< 0 \ 1-x
        0 =< x =< 1 and a (resp. b)weighting coefficient is the distance
        from the nearest neighbor in the p (resp. q) direction
        */

        if (u32_plane == 0)
        {
            /* Set the working pointers at the beginning of the input/output data field */
            pu8_data_in = pPlaneIn[u32_plane].pac_data + pPlaneIn[u32_plane].u_topleft;
            pu8_data_out = pPlaneOut[u32_plane].pac_data + pPlaneOut[u32_plane].u_topleft;

            do { /* Scan all the row */

                /* Vertical weight factor */
                u32_y_frac = (u32_y_accum>>12)&15;

                /* Reinit accumulator */
                u32_x_accum = u32_x_accum_start;

                u32_width = u32_width_out;

                do { /* Scan along each row */
                    pu8_src_top = pu8_data_in + (u32_x_accum >> 16);
                    pu8_src_bottom = pu8_src_top + u32_stride_in;
                    u32_x_frac = (u32_x_accum >> 12)&15; /* Horizontal weight factor */

                    /* Weighted combination */
                    u32_temp_value = (M4VIFI_UInt8)(((pu8_src_top[0]*(16-u32_x_frac) +
                                                     pu8_src_top[1]*u32_x_frac)*(16-u32_y_frac) +
                                                    (pu8_src_bottom[0]*(16-u32_x_frac) +
                                                     pu8_src_bottom[1]*u32_x_frac)*u32_y_frac )>>8);

                    *pu8_data_out++ = (M4VIFI_UInt8)u32_temp_value;

                    /* Update horizontal accumulator */
                    u32_x_accum += u32_x_inc;
                } while(--u32_width);

                /*
                   This u8Wflag flag gets in to effect if input and output
                   width is same, and height may be different. So previous
                   pixel is replicated here
                */
                if (u8Wflag) {
                    *pu8_data_out = (M4VIFI_UInt8)u32_temp_value;
                }

                pu8dum = (pu8_data_out-u32_width_out);
                pu8_data_out = pu8_data_out + u32_stride_out - u32_width_out;

                /* Update vertical accumulator */
                u32_y_accum += u32_y_inc;
                if (u32_y_accum>>16) {
                    pu8_data_in = pu8_data_in + (u32_y_accum >> 16) * u32_stride_in;
                    u32_y_accum &= 0xffff;
                }
            } while(--u32_height);

            /*
            This u8Hflag flag gets in to effect if input and output height
            is same, and width may be different. So previous pixel row is
            replicated here
            */
            if (u8Hflag) {
                memcpy((void *)pu8_data_out,(void *)pu8dum,u32_width_out+u8Wflag);
            }
        }
        else
        {
             /* Set the working pointers at the beginning of the input/output data field */
             pu8_data_in = pPlaneIn[u32_plane].pac_data + pPlaneIn[u32_plane].u_topleft;
             pu8_data_out = pPlaneOut[u32_plane].pac_data + pPlaneOut[u32_plane].u_topleft;

             do { /* Scan all the row */

                /* Vertical weight factor */
                u32_y_frac = (u32_y_accum>>12)&15;

                /* Reinit accumulator */
                u32_x_accum = u32_x_accum_start;

                u32_width = u32_width_out;

                do { /* Scan along each row */
                    pu8_src_top = pu8_data_in + ((u32_x_accum >> 16) << 1);
                    pu8_src_bottom = pu8_src_top + u32_stride_in;
                    u32_x_frac = (u32_x_accum >> 12)&15;

                    /* U planar weighted combination */
                    u32_temp_value1 = (M4VIFI_UInt8)(((pu8_src_top[0]*(16-u32_x_frac) +
                                                 pu8_src_top[2]*u32_x_frac)*(16-u32_y_frac) +
                                                (pu8_src_bottom[0]*(16-u32_x_frac) +
                                                 pu8_src_bottom[2]*u32_x_frac)*u32_y_frac )>>8);
                    *pu8_data_out++ = (M4VIFI_UInt8)u32_temp_value1;

                    pu8_src_top = pu8_src_top + 1;
                    pu8_src_bottom = pu8_src_bottom + 1;

                    /* V planar weighted combination */
                    u32_temp_value = (M4VIFI_UInt8)(((pu8_src_top[0]*(16-u32_x_frac) +
                                                 pu8_src_top[2]*u32_x_frac)*(16-u32_y_frac) +
                                                (pu8_src_bottom[0]*(16-u32_x_frac) +
                                                 pu8_src_bottom[2]*u32_x_frac)*u32_y_frac )>>8);
                    *pu8_data_out++ = (M4VIFI_UInt8)u32_temp_value;

                    /* Update horizontal accumulator */
                    u32_x_accum += u32_x_inc;
                    u32_width -= 2;
                } while(u32_width);

                /*
                   This u8Wflag flag gets in to effect if input and output
                   width is same, and height may be different. So previous
                   pixel is replicated here
                */
                if (u8Wflag) {
                    *pu8_data_out = (M4VIFI_UInt8)u32_temp_value1;
                    *(pu8_data_out+1) = (M4VIFI_UInt8)u32_temp_value;
                }

                pu8dum = (pu8_data_out - u32_width_out);
                pu8_data_out = pu8_data_out + u32_stride_out - u32_width_out;

                /* Update vertical accumulator */
                u32_y_accum += u32_y_inc;
                if (u32_y_accum>>16) {
                    pu8_data_in = pu8_data_in + (u32_y_accum >> 16) * u32_stride_in;
                    u32_y_accum &= 0xffff;
                }
            } while(--u32_height);

            /*
            This u8Hflag flag gets in to effect if input and output height
            is same, and width may be different. So previous pixel row is
            replicated here
            */
            if (u8Hflag) {
                memcpy((void *)pu8_data_out,(void *)pu8dum,u32_width_out+u8Wflag+1);
            }
        }
    }
    LOGV("M4VIFI_ResizeBilinearNV12toNV12 end");
    return M4VIFI_OK;
}

M4VIFI_UInt8 M4VIFI_Rotate90LeftNV12toNV12(void* pUserData,
    M4VIFI_ImagePlane *pPlaneIn, M4VIFI_ImagePlane *pPlaneOut)
{
    M4VIFI_Int32 plane_number;
    M4VIFI_UInt32 i,j, u_stride;
    M4VIFI_UInt8 *p_buf_src, *p_buf_dest;

    /**< Loop on Y,U and V planes */
    for (plane_number = 0; plane_number < 2; plane_number++) {
        /**< Get adresses of first valid pixel in input and output buffer */
        /**< As we have a -90. rotation, first needed pixel is the upper-right one */
        if (plane_number == 0) {
            p_buf_src =
                &(pPlaneIn[plane_number].pac_data[pPlaneIn[plane_number].u_topleft]) +
                 pPlaneOut[plane_number].u_height - 1 ;
            p_buf_dest =
                &(pPlaneOut[plane_number].pac_data[pPlaneOut[plane_number].u_topleft]);
            u_stride = pPlaneIn[plane_number].u_stride;
            /**< Loop on output rows */
            for (i = pPlaneOut[plane_number].u_height; i != 0; i--) {
                /**< Loop on all output pixels in a row */
                for (j = pPlaneOut[plane_number].u_width; j != 0; j--) {
                    *p_buf_dest++= *p_buf_src;
                    p_buf_src += u_stride;  /**< Go to the next row */
                }

                /**< Go on next row of the output frame */
                p_buf_dest +=
                    pPlaneOut[plane_number].u_stride - pPlaneOut[plane_number].u_width;
                /**< Go to next pixel in the last row of the input frame*/
                p_buf_src -=
                    pPlaneIn[plane_number].u_stride * pPlaneOut[plane_number].u_width + 1 ;
            }
        } else {
            p_buf_src =
                &(pPlaneIn[plane_number].pac_data[pPlaneIn[plane_number].u_topleft]) +
                 pPlaneIn[plane_number].u_width - 2 ;
            p_buf_dest =
                &(pPlaneOut[plane_number].pac_data[pPlaneOut[plane_number].u_topleft]);
            u_stride = pPlaneIn[plane_number].u_stride;
            /**< Loop on output rows */
            for (i = pPlaneOut[plane_number].u_height; i != 0 ; i--) {
                /**< Loop on all output pixels in a row */
                for (j = (pPlaneOut[plane_number].u_width >> 1); j != 0 ; j--) {
                    *p_buf_dest++= *p_buf_src++;
                    *p_buf_dest++= *p_buf_src--;
                    p_buf_src += u_stride;  /**< Go to the next row */
                }

                /**< Go on next row of the output frame */
                p_buf_dest +=
                    pPlaneOut[plane_number].u_stride - pPlaneOut[plane_number].u_width;
                /**< Go to next pixel in the last row of the input frame*/
                p_buf_src -=
                    pPlaneIn[plane_number].u_stride * pPlaneIn[plane_number].u_height + 2 ;
            }
        }
    }

    return M4VIFI_OK;
}

M4VIFI_UInt8 M4VIFI_Rotate90RightNV12toNV12(void* pUserData,
    M4VIFI_ImagePlane *pPlaneIn, M4VIFI_ImagePlane *pPlaneOut)
{
    M4VIFI_Int32 plane_number;
    M4VIFI_UInt32 i,j, u_stride;
    M4VIFI_UInt8 *p_buf_src, *p_buf_dest;

    /**< Loop on Y,U and V planes */
    for (plane_number = 0; plane_number < 2; plane_number++) {
        /**< Get adresses of first valid pixel in input and output buffer */
        /**< As we have a +90 rotation, first needed pixel is the left-down one */
        p_buf_src =
            &(pPlaneIn[plane_number].pac_data[pPlaneIn[plane_number].u_topleft]) +
             (pPlaneIn[plane_number].u_stride * (pPlaneIn[plane_number].u_height - 1));
        p_buf_dest =
            &(pPlaneOut[plane_number].pac_data[pPlaneOut[plane_number].u_topleft]);
        u_stride = pPlaneIn[plane_number].u_stride;
        if (plane_number == 0) {
           /**< Loop on output rows */
           for (i = pPlaneOut[plane_number].u_height; i != 0 ; i--) {
                /**< Loop on all output pixels in a row */
                for (j = pPlaneOut[plane_number].u_width; j != 0 ; j--) {
                     *p_buf_dest++= *p_buf_src;
                     p_buf_src -= u_stride;  /**< Go to the previous row */
                }

                /**< Go on next row of the output frame */
                p_buf_dest +=
                    pPlaneOut[plane_number].u_stride - pPlaneOut[plane_number].u_width;
                /**< Go to next pixel in the last row of the input frame*/
                p_buf_src +=
                    pPlaneIn[plane_number].u_stride * pPlaneOut[plane_number].u_width + 1 ;
            }
        } else {
            /**< Loop on output rows */
            for (i = pPlaneOut[plane_number].u_height; i != 0 ; i--) {
                /**< Loop on all output pixels in a row */
                for (j = (pPlaneOut[plane_number].u_width >> 1); j != 0 ; j--) {
                     *p_buf_dest++= *p_buf_src++;
                     *p_buf_dest++= *p_buf_src--;
                     p_buf_src -= u_stride;  /**< Go to the previous row */
                }

                /**< Go on next row of the output frame */
                p_buf_dest +=
                    pPlaneOut[plane_number].u_stride - pPlaneOut[plane_number].u_width;
                /**< Go to next pixel in the last row of the input frame*/
                p_buf_src +=
                    pPlaneIn[plane_number].u_stride * pPlaneIn[plane_number].u_height + 2 ;
            }
        }
    }

    return M4VIFI_OK;
}

M4VIFI_UInt8 M4VIFI_Rotate180NV12toNV12(void* pUserData,
    M4VIFI_ImagePlane *pPlaneIn, M4VIFI_ImagePlane *pPlaneOut)
{
    M4VIFI_Int32 plane_number;
    M4VIFI_UInt32 i,j;
    M4VIFI_UInt8 *p_buf_src, *p_buf_dest, temp_pix1;
    M4VIFI_UInt16 *p16_buf_src, *p16_buf_dest, temp_pix2;

    /**< Loop on Y,U and V planes */
    for (plane_number = 0; plane_number < 2; plane_number++) {
        /**< Get adresses of first valid pixel in input and output buffer */
        p_buf_src =
            &(pPlaneIn[plane_number].pac_data[pPlaneIn[plane_number].u_topleft]);
        p_buf_dest =
            &(pPlaneOut[plane_number].pac_data[pPlaneOut[plane_number].u_topleft]);

        if (plane_number == 0) {
            /**< If pPlaneIn = pPlaneOut, the algorithm will be different */
            if (p_buf_src == p_buf_dest) {
                /**< Get Address of last pixel in the last row of the frame */
                p_buf_dest +=
                    pPlaneOut[plane_number].u_stride*(pPlaneOut[plane_number].u_height-1) +
                     pPlaneOut[plane_number].u_width - 1;

                /**< We loop (height/2) times on the rows.
                 * In case u_height is odd, the row at the middle of the frame
                 * has to be processed as must be mirrored */
                for (i = (pPlaneOut[plane_number].u_height>>1); i != 0; i--) {
                    for (j = pPlaneOut[plane_number].u_width; j != 0 ; j--) {
                        temp_pix1= *p_buf_dest;
                        *p_buf_dest--= *p_buf_src;
                        *p_buf_src++ = temp_pix1;
                    }
                    /**< Go on next row in top of frame */
                    p_buf_src +=
                        pPlaneOut[plane_number].u_stride - pPlaneOut[plane_number].u_width;
                    /**< Go to the last pixel in previous row in bottom of frame*/
                    p_buf_dest -=
                        pPlaneOut[plane_number].u_stride - pPlaneOut[plane_number].u_width;
                }

                /**< Mirror middle row in case height is odd */
                if ((pPlaneOut[plane_number].u_height%2)!= 0) {
                    p_buf_src =
                        &(pPlaneOut[plane_number].pac_data[pPlaneIn[plane_number].u_topleft]);
                    p_buf_src +=
                        pPlaneOut[plane_number].u_stride*(pPlaneOut[plane_number].u_height>>1);
                    p_buf_dest =
                        p_buf_src + pPlaneOut[plane_number].u_width;

                    /**< We loop u_width/2 times on this row.
                     *  In case u_width is odd, the pixel at the middle of this row
                     * remains unchanged */
                    for (j = (pPlaneOut[plane_number].u_width>>1); j != 0 ; j--) {
                        temp_pix1= *p_buf_dest;
                        *p_buf_dest--= *p_buf_src;
                        *p_buf_src++ = temp_pix1;
                    }
                }
            } else {
                /**< Get Address of last pixel in the last row of the output frame */
                p_buf_dest +=
                    pPlaneOut[plane_number].u_stride*(pPlaneOut[plane_number].u_height-1) +
                    pPlaneIn[plane_number].u_width - 1;

                /**< Loop on rows */
                for (i = pPlaneOut[plane_number].u_height; i != 0 ; i--) {
                    for (j = pPlaneOut[plane_number].u_width; j != 0 ; j--) {
                        *p_buf_dest--= *p_buf_src++;
                    }

                    /**< Go on next row in top of input frame */
                    p_buf_src +=
                        pPlaneIn[plane_number].u_stride - pPlaneOut[plane_number].u_width;
                    /**< Go to last pixel of previous row in bottom of input frame*/
                    p_buf_dest -=
                        pPlaneOut[plane_number].u_stride - pPlaneOut[plane_number].u_width;
                }
            }
        } else {
            /**< If pPlaneIn = pPlaneOut, the algorithm will be different */
            if (p_buf_src == p_buf_dest) {
                p16_buf_src = (M4VIFI_UInt16 *)p_buf_src;
                p16_buf_dest = (M4VIFI_UInt16 *)p_buf_dest;
                /**< Get Address of last pixel in the last row of the frame */
                p16_buf_dest +=
                    ((pPlaneOut[plane_number].u_stride*(pPlaneOut[plane_number].u_height-1) +
                     pPlaneOut[plane_number].u_width)>>1) - 1;

                /**< We loop (height/2) times on the rows.
                 * In case u_height is odd, the row at the middle of the frame
                 * has to be processed as must be mirrored */
                for (i = (pPlaneOut[plane_number].u_height >> 1); i != 0 ; i--) {
                    for (j = (pPlaneOut[plane_number].u_width >> 1); j != 0 ; j--) {
                        temp_pix2 = *p16_buf_dest;
                        *p16_buf_dest--= *p16_buf_src;
                        *p16_buf_src++ = temp_pix2;
                    }
                    /**< Go on next row in top of frame */
                    p16_buf_src +=
                        ((pPlaneOut[plane_number].u_stride - pPlaneOut[plane_number].u_width)>>1);
                    /**< Go to the last pixel in previous row in bottom of frame*/
                    p16_buf_dest -=
                        ((pPlaneOut[plane_number].u_stride - pPlaneOut[plane_number].u_width)>>1);
                }

                /**< Mirror middle row in case height is odd */
                if ((pPlaneOut[plane_number].u_height%2)!= 0) {
                    p_buf_src =
                        &(pPlaneOut[plane_number].pac_data[pPlaneIn[plane_number].u_topleft]);
                    p_buf_src +=
                        pPlaneOut[plane_number].u_stride*(pPlaneOut[plane_number].u_height>>1);
                    p16_buf_src = (M4VIFI_UInt16 *)p_buf_src;
                    p_buf_dest =
                        p_buf_src + pPlaneOut[plane_number].u_width - 1;
                    p16_buf_dest = (M4VIFI_UInt16 *)p_buf_dest;

                    /**< We loop u_width/2 times on this row.
                     *  In case u_width is odd, the pixel at the middle of this row
                     * remains unchanged */
                    for (j = (pPlaneOut[plane_number].u_width>>2); j != 0 ; j--) {
                        temp_pix2= *p16_buf_dest;
                        *p16_buf_dest--= *p16_buf_src;
                        *p16_buf_src++ = temp_pix2;
                    }
                }
            } else {
                /**< Get Address of last pixel in the last row of the output frame */
                p_buf_dest +=
                    pPlaneOut[plane_number].u_stride*(pPlaneOut[plane_number].u_height-1) +
                    pPlaneIn[plane_number].u_width - 2;
                p16_buf_dest = (M4VIFI_UInt16 *)p_buf_dest;
                p16_buf_src = (M4VIFI_UInt16 *)p_buf_src;

                /**< Loop on rows */
                for (i = pPlaneOut[plane_number].u_height; i != 0 ; i--) {
                    for (j = (pPlaneOut[plane_number].u_width >> 1); j != 0 ; j--) {
                        *p16_buf_dest--= *p16_buf_src++;
                    }

                    /**< Go on next row in top of input frame */
                    p16_buf_src +=
                        ((pPlaneIn[plane_number].u_stride - pPlaneOut[plane_number].u_width)>>1);
                    /**< Go to last pixel of previous row in bottom of input frame*/
                    p16_buf_dest -=
                        ((pPlaneOut[plane_number].u_stride - pPlaneOut[plane_number].u_width)>>1);
                }
            }
        }
    }

    return M4VIFI_OK;
}


M4VIFI_UInt8 M4VIFI_ResizeBilinearNV12toYUV420(void *pUserData,
    M4VIFI_ImagePlane *pPlaneIn, M4VIFI_ImagePlane *pPlaneOut)
{

    LOGV("M4VIFI_ResizeBilinearNV12toYUV420 begin");

    M4VIFI_ImagePlane pPlaneTmp[3];
    M4OSA_UInt32 mVideoWidth, mVideoHeight;
    M4OSA_UInt32 mFrameSize;

    mVideoWidth  = pPlaneIn[0].u_width;
    mVideoHeight = pPlaneIn[0].u_height;
    mFrameSize   = mVideoWidth * mVideoHeight * 3/2;

    M4OSA_UInt8 *pData = (M4OSA_UInt8 *)M4OSA_32bitAlignedMalloc(
                             mFrameSize,
                             12420,
                             (M4OSA_Char*)("M4VIFI_ResizeBilinearNV12toYUV420: tempBuffer")
                         );

    if (NULL == pData)
    {
        LOGE("Error: Fail to allocate tempBuffer!");
        return M4VIFI_ALLOC_FAILURE;
    }

    pPlaneTmp[0].pac_data       = pData;
    pPlaneTmp[0].u_height       = pPlaneIn[0].u_height;
    pPlaneTmp[0].u_width        = pPlaneIn[0].u_width;
    pPlaneTmp[0].u_stride       = pPlaneIn[0].u_stride;
    pPlaneTmp[0].u_topleft      = pPlaneIn[0].u_topleft;

    pPlaneTmp[1].pac_data       = (M4OSA_UInt8 *)(pData + mVideoWidth*mVideoHeight);
    pPlaneTmp[1].u_height       = pPlaneTmp[0].u_height/2;
    pPlaneTmp[1].u_width        = pPlaneTmp[0].u_width/2;
    pPlaneTmp[1].u_stride       = pPlaneTmp[0].u_stride/2;
    pPlaneTmp[1].u_topleft      = pPlaneTmp[0].u_topleft;

    pPlaneTmp[2].pac_data       = (M4OSA_UInt8 *)(pData + mVideoWidth*mVideoHeight*5/4);
    pPlaneTmp[2].u_height       = pPlaneTmp[0].u_height/2;
    pPlaneTmp[2].u_width        = pPlaneTmp[0].u_width/2;
    pPlaneTmp[2].u_stride       = pPlaneTmp[0].u_stride/2;
    pPlaneTmp[2].u_topleft      = pPlaneTmp[0].u_topleft;

    M4VIFI_UInt8 err;
    err = M4VIFI_SemiplanarYUV420toYUV420_X86(pUserData, pPlaneIn,&pPlaneTmp[0]);

    if(err != M4VIFI_OK)
    {
        LOGE("Error: M4VIFI_SemiplanarYUV420toYUV420 fails!");
        free(pData);
        return err;
    }

    err = M4VIFI_ResizeBilinearYUV420toYUV420_X86(pUserData,&pPlaneTmp[0],pPlaneOut);

    free(pData);
    LOGV("M4VIFI_ResizeBilinearNV12toYUV420 end");
    return err;

}

M4VIFI_UInt8 M4VIFI_ResizeBilinearNV12toBGR565(void *pUserData,
    M4VIFI_ImagePlane *pPlaneIn, M4VIFI_ImagePlane *pPlaneOut)
{
    LOGV("M4VIFI_ResizeBilinearNV12toBGR565 begin");

    M4VIFI_ImagePlane pPlaneTmp[3];
    M4OSA_UInt32 mVideoWidth, mVideoHeight;
    M4OSA_UInt32 mFrameSize;

    mVideoWidth  = pPlaneIn[0].u_width;
    mVideoHeight = pPlaneIn[0].u_height;
    mFrameSize   = mVideoWidth * mVideoHeight * 3/2;

    M4OSA_UInt8 *pData = (M4OSA_UInt8 *)M4OSA_32bitAlignedMalloc(
                             mFrameSize,
                             12420,
                             (M4OSA_Char*)("M4VIFI_ResizeBilinearNV12toYUV420:tempBuffer")
                         );
    if (NULL == pData)
    {
        LOGE("Error: Fail to allocate tempBuffer!");
        return M4VIFI_ALLOC_FAILURE;
    }
    pPlaneTmp[0].pac_data   = pData;
    pPlaneTmp[0].u_height   = pPlaneIn[0].u_height;
    pPlaneTmp[0].u_width    = pPlaneIn[0].u_width;
    pPlaneTmp[0].u_stride   = pPlaneIn[0].u_stride;
    pPlaneTmp[0].u_topleft  = pPlaneIn[0].u_topleft;

    pPlaneTmp[1].pac_data   = (M4OSA_UInt8 *)(pData + mVideoWidth*mVideoHeight);
    pPlaneTmp[1].u_height   = pPlaneTmp[0].u_height/2;
    pPlaneTmp[1].u_width    = pPlaneTmp[0].u_width/2;
    pPlaneTmp[1].u_stride   = pPlaneTmp[0].u_stride/2;
    pPlaneTmp[1].u_topleft  = pPlaneTmp[0].u_topleft;

    pPlaneTmp[2].pac_data   = (M4OSA_UInt8 *)(pData + mVideoWidth*mVideoHeight*5/4);
    pPlaneTmp[2].u_height   = pPlaneTmp[0].u_height/2;
    pPlaneTmp[2].u_width    = pPlaneTmp[0].u_width/2;
    pPlaneTmp[2].u_stride   = pPlaneTmp[0].u_stride/2;
    pPlaneTmp[2].u_topleft  = pPlaneTmp[0].u_topleft;

    M4VIFI_UInt8 err;
    err = M4VIFI_SemiplanarYUV420toYUV420_X86(pUserData, pPlaneIn,&pPlaneTmp[0]);

    if(err != M4VIFI_OK)
    {
        LOGE("Error: M4VIFI_SemiplanarYUV420toYUV420 fails!");
        free(pData);
        return err;
    }

    err = M4VIFI_ResizeBilinearYUV420toBGR565_X86(pUserData,&pPlaneTmp[0],pPlaneOut);
    free(pData);

    LOGV("M4VIFI_ResizeBilinearNV12toBGR565 end");
    return err;
}

