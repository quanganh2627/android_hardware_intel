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
#define LOG_TAG "Camera_ColorConverter"

#include <camera/CameraParameters.h>
#include <linux/videodev2.h>
#include "ColorConverter.h"
#include "LogHelper.h"
#include "VAConvertor.h"
namespace android {

inline unsigned char clamp(int x){
    if (x < 0 )
        x = 0;
    else if (x > 255)
        x = 255;
    return (x & 0xFF);
}

void YUYVToNV21(int width, int height, void *src, void *dst)
{
    unsigned char *pSrcY = (unsigned char *) src;
    unsigned char *pSrcU = pSrcY + 1;
    unsigned char *pSrcV = pSrcY + 3;

    unsigned char *pDstY = (unsigned char *) dst;
    unsigned char *pDstUV = pDstY + width * height;

    // YUYV format is: yuyvyuyvyuyv...yuyv
    // NV12 format is: yyyy...yyyyuvuv...uvuvuv
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width / 2; j++) { // 2 y-pixels at a time
            *pDstY++ = *pSrcY;
            pSrcY += 2;
            *pDstY++ = *pSrcY;
            pSrcY += 2;

            // 4:2:2 chroma has 1/2 the horizontal and FULL vertical resolution of full image
            // 4:2:0 chroma has 1/2 the horizontal and 1/2 vertical resolution of full image
            // so skip odd numbered rows
            if ((i % 2) == 0) {
                *pDstUV++ = *pSrcV;
                *pDstUV++ = *pSrcU;
            }
            pSrcU += 4;
            pSrcV += 4;
        }
    }
}

void YUYVToNV12(int width, int height, void *src, void *dst)
{
    unsigned char *pSrcY = (unsigned char *) src;
    unsigned char *pSrcU = pSrcY + 1;
    unsigned char *pSrcV = pSrcY + 3;

    unsigned char *pDstY = (unsigned char *) dst;
    unsigned char *pDstUV = pDstY + width * height;

    // YUYV format is: yuyvyuyvyuyv...yuyv
    // NV12 format is: yyyy...yyyyuvuv...uvuvuv
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width / 2; j++) { // 2 y-pixels at a time
            *pDstY++ = *pSrcY;
            pSrcY += 2;
            *pDstY++ = *pSrcY;
            pSrcY += 2;

            // 4:2:2 chroma has 1/2 the horizontal and FULL vertical resolution of full image
            // 4:2:0 chroma has 1/2 the horizontal and 1/2 vertical resolution of full image
            // so skip odd numbered rows
            if ((i % 2) == 0) {
                *pDstUV++ = *pSrcU;
                *pDstUV++ = *pSrcV;
            }
            pSrcU += 4;
            pSrcV += 4;
        }
    }
}

void YUYVToRGB8888(int width, int height, void *src, void *dst)
{
    int len = width * height * 2;
    int i = 0;
    unsigned char *pYUV = (unsigned char *)src;   //four bytes of two pixels
    unsigned char *pRGB = (unsigned char *)dst;       //6 rgb bytes for two pixels
    int C, D, E;
    len -= len % 4;
    for (i = 0; i < len; i += 4) {
        unsigned char y1 = *(pYUV++);
        unsigned char u = *(pYUV++);
        unsigned char y2 = *(pYUV++);
        unsigned char v = *(pYUV++);
//calculate 1st pixel
        C = y1 - 16;
        D = u - 128;
        E = v -128;
        *(pRGB++) = clamp((C * 298 + E * 409 + 128) >> 8);
        *(pRGB++) = clamp((C * 298 - D * 100 - E * 208 + 128) >> 8);
        *(pRGB++) = clamp((C * 298 + D * 516 + 128) >> 8);
        //alpha
        *(pRGB++) = 0xFF;
//calculate 2nd pixel
        C = y2 -16;
        *(pRGB++) = clamp((C * 298 + E * 409 + 128) >> 8);
        *(pRGB++) = clamp((C * 298 - D * 100 - E * 208 + 128) >> 8);
        *(pRGB++) = clamp((C * 298 + D * 516 + 128) >> 8);
        //alpha
        *(pRGB++) = 0xFF;

    }
}

void YUYVToRGB565(int width, int height, void *src, void *dst)
{

    unsigned char *yuvs = (unsigned char *) src;
    unsigned char *rgbs = (unsigned char *) dst;

    //points to the next luminance value pair
    int lumPtr = 0;
    //points to the next chromiance value pair
    int chrPtr = 1;
    //points to the next byte output pair of RGB565 value
    int outPtr = 0;

    while (true) {

        if (lumPtr == width * height * 2) // our work is done here!
            break;

        //read the luminance
        int Y1 = yuvs[lumPtr] & 0xff;
        lumPtr += 2;
        int Y2 = yuvs[lumPtr] & 0xff;
        lumPtr += 2;

        //read the chroma
        int Cb = (yuvs[chrPtr] & 0xff) - 128;
        chrPtr += 2;
        int Cr = (yuvs[chrPtr] & 0xff) - 128;
        chrPtr += 2;
        int R, G, B;

        //generate first RGB components
        B = clamp(Y1 + ((454 * Cb) >> 8));
        G = clamp(Y1 - ((88 * Cb + 183 * Cr) >> 8));
        R = clamp(Y1 + ((359 * Cr) >> 8));
        //NOTE: this assume little-endian encoding
        rgbs[outPtr++]  = (unsigned char) (((G & 0x3c) << 3) | (B >> 3));
        rgbs[outPtr++]  = (unsigned char) ((R & 0xf8) | (G >> 5));

        //generate second RGB components
        B = clamp(Y2 + ((454 * Cb) >> 8));
        G = clamp(Y2 - ((88 * Cb + 183 * Cr) >> 8));
        R = clamp(Y2 + ((359 * Cr) >> 8));
        //NOTE: this assume little-endian encoding
        rgbs[outPtr++]  = (unsigned char) (((G & 0x3c) << 3) | (B >> 3));
        rgbs[outPtr++]  = (unsigned char) ((R & 0xf8) | (G >> 5));
    }
}

void NV12ToRGB565(int width, int height, void *src, void *dst)
{

    unsigned char *yuvs = (unsigned char *) src;
    unsigned char *rgbs = (unsigned char *) dst;

    //the end of the luminance data
    int lumEnd = width * height;
    //points to the next luminance value pair
    int lumPtr = 0;
    //points to the next chromiance value pair
    int chrPtr = lumEnd;
    //points to the next byte output pair of RGB565 value
    int outPtr = 0;
    //the end of the current luminance scanline
    int lineEnd = width;

    while (true) {
        //skip back to the start of the chromiance values when necessary
        if (lumPtr == lineEnd) {
            if (lumPtr == lumEnd) break; //we've reached the end
            //division here is a bit expensive, but's only done once per scanline
            chrPtr = lumEnd + ((lumPtr  >> 1) / width) * width;
            lineEnd += width;
        }
        //read the luminance and chromiance values
        int Y1 = yuvs[lumPtr++] & 0xff;
        int Y2 = yuvs[lumPtr++] & 0xff;
        int Cb = (yuvs[chrPtr++] & 0xff) - 128;
        int Cr = (yuvs[chrPtr++] & 0xff) - 128;
        int R, G, B;

        //generate first RGB components
        B = Y1 + ((454 * Cb) >> 8);
        if(B < 0) B = 0; else if(B > 255) B = 255;
        G = Y1 - ((88 * Cb + 183 * Cr) >> 8);
        if(G < 0) G = 0; else if(G > 255) G = 255;
        R = Y1 + ((359 * Cr) >> 8);
        if(R < 0) R = 0; else if(R > 255) R = 255;
        //NOTE: this assume little-endian encoding
        rgbs[outPtr++]  = (unsigned char) (((G & 0x3c) << 3) | (B >> 3));
        rgbs[outPtr++]  = (unsigned char) ((R & 0xf8) | (G >> 5));

        //generate second RGB components
        B = Y2 + ((454 * Cb) >> 8);
        if(B < 0) B = 0; else if(B > 255) B = 255;
        G = Y2 - ((88 * Cb + 183 * Cr) >> 8);
        if(G < 0) G = 0; else if(G > 255) G = 255;
        R = Y2 + ((359 * Cr) >> 8);
        if(R < 0) R = 0; else if(R > 255) R = 255;
        //NOTE: this assume little-endian encoding
        rgbs[outPtr++]  = (unsigned char) (((G & 0x3c) << 3) | (B >> 3));
        rgbs[outPtr++]  = (unsigned char) ((R & 0xf8) | (G >> 5));
    }
}

void NV12ToRGB565withStride(int width, int height,int stride,int alignheight, void *src, void *dst)
{

    unsigned char *yuvs = (unsigned char *) src;
    unsigned char *rgbs = (unsigned char *) dst;

    //the end of the luminance data
    int lumEnd = stride * alignheight;
    //points to the next luminance value pair
    int lumPtr = 0;
    //points to the next chromiance value pair
    int chrPtr = lumEnd;
    //points to the next byte output pair of RGB565 value
    int outPtr = 0;
    //the end of the current luminance scanline
    int lineEnd = width;
    int diff = stride-width;
    int actLumEnd = stride*height;
    while (true) {
        //skip back to the start of the chromiance values when necessary
        if (lumPtr == lineEnd) {
            if (lumPtr == (actLumEnd-diff)) break; //we've reached the end
            //division here is a bit expensive, but's only done once per scanline
            lumPtr += diff;
            chrPtr = lumEnd + ((lumPtr  >> 1) / stride) * stride;
            lineEnd += stride;
        }
        //read the luminance and chromiance values
        int Y1 = yuvs[lumPtr++] & 0xff;
        int Y2 = yuvs[lumPtr++] & 0xff;
        int Cb = (yuvs[chrPtr++] & 0xff) - 128;
        int Cr = (yuvs[chrPtr++] & 0xff) - 128;
        int R, G, B;

        //generate first RGB components
        B = Y1 + ((454 * Cb) >> 8);
        if(B < 0) B = 0; else if(B > 255) B = 255;
        G = Y1 - ((88 * Cb + 183 * Cr) >> 8);
        if(G < 0) G = 0; else if(G > 255) G = 255;
        R = Y1 + ((359 * Cr) >> 8);
        if(R < 0) R = 0; else if(R > 255) R = 255;
        //NOTE: this assume little-endian encoding
        rgbs[outPtr++]  = (unsigned char) (((G & 0x3c) << 3) | (B >> 3));
        rgbs[outPtr++]  = (unsigned char) ((R & 0xf8) | (G >> 5));

        //generate second RGB components
        B = Y2 + ((454 * Cb) >> 8);
        if(B < 0) B = 0; else if(B > 255) B = 255;
        G = Y2 - ((88 * Cb + 183 * Cr) >> 8);
        if(G < 0) G = 0; else if(G > 255) G = 255;
        R = Y2 + ((359 * Cr) >> 8);
        if(R < 0) R = 0; else if(R > 255) R = 255;
        //NOTE: this assume little-endian encoding
        rgbs[outPtr++]  = (unsigned char) (((G & 0x3c) << 3) | (B >> 3));
        rgbs[outPtr++]  = (unsigned char) ((R & 0xf8) | (G >> 5));
    }
}
/*
convert YUV422H to NV12, the yuv422h is placed as Y(stride * alignheight),U(stride * alignheight),V(stride * alignheight)
the valid data for U/V is width/2 * alignheight
width: dst image width
height: dst image height
stride: stride for src image
alignheight: aligned height for src image
*/
void YUV422PToNV12withStride(int width, int height,int stride,int alignheight, void *src, void *dst)
{
    int planeSizeY = stride * alignheight;
    int planeSizeU = planeSizeY;
    int i = 0;
    int j = 0;
    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *srcPtrU = (unsigned char *) src + planeSizeY;
    unsigned char *srcPtrV = (unsigned char *) srcPtrU + planeSizeU;
    unsigned char *dstPtr = (unsigned char *) dst;
    // copy the entire Y plane
    if(width == stride)
    {
       memcpy(dstPtr, srcPtr, width*height);
       dstPtr += (width*height);
    }
    else
    {
       for(i=0;i<height;i++)
       {
          memcpy(dstPtr, srcPtr, width);
          dstPtr += width;
          srcPtr += stride;
       }
    }
    // deinterlace the UV data
    int vertical = height/2;
    int horizontal = width / 2;
    for(i = 0; i < vertical; i++) {
        for (j = 0; j < horizontal; j++) {
            *dstPtr++ = srcPtrU[j];
            *dstPtr++ = srcPtrV[j];
        }
        srcPtrV += stride << 1;
        srcPtrU += stride << 1;
    }

}
/*
convert YUV422H to NV21, the yuv422h is placed as Y(stride * alignheight),U(stride * alignheight),V(stride * alignheight)
the valid data for U/V is width/2 * alignheight
width: dst image width
height: dst image height
stride: stride for src image
alignheight: aligned height for src image
*/

void YUV422PToNV21withStride(int width, int height,int stride,int alignheight, void *src, void *dst)
{
    int planeSizeY = stride * alignheight;
    int planeSizeU = planeSizeY;
    int i = 0;
    int j = 0;
    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *srcPtrU = (unsigned char *) src + planeSizeY;
    unsigned char *srcPtrV = (unsigned char *) srcPtrU + planeSizeU;
    unsigned char *dstPtr = (unsigned char *) dst;
    unsigned char *dstPtrVU = (unsigned char *) dst+ width*height;
    // copy the entire Y plane
    if(width == stride)
    {
       memcpy(dstPtr, srcPtr, width*height);
       dstPtr += (width*height);
    }
    else
    {
      for(i = 0;i < height;i++)
      {
         memcpy(dstPtr, srcPtr, width);
         dstPtr += width;
         srcPtr += stride;
      }
   }
    // deinterlace the VU data
    int vertical = height / 2;
    int horizontal = width / 2;
    for(i = 0; i < vertical; i++) {
        for (j = 0; j < horizontal; j++) {
            *dstPtrVU++ = srcPtrV[j];
            *dstPtrVU++ = srcPtrU[j];
        }
        srcPtrV += stride << 1;
        srcPtrU += stride << 1;
    }

}
/*
convert YUV422H to YV12, the yuv422h is placed as Y(stride * alignheight),U(stride * alignheight),V(stride * alignheight)
the valid data for U/V is width/2 * alignheight
yuv422 from graphic buffer should be aligned to 128, so U/V should be aligned to 128, alreay satisfy the requirment of Android's 16 alignment
width: dst image width
height: dst image height
stride: stride for src image
alignheight: aligned height for src image
*/
void YUV422PToYV12withStride(int width, int height,int stride,int alignheight, void *src, void *dst)
{
    int planeSizeY = stride * alignheight;
    int planeSizeU = planeSizeY;
    int i = 0;
    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *srcPtrU = (unsigned char *) src + planeSizeY;
    unsigned char *srcPtrV = (unsigned char *) srcPtrU + planeSizeU;
    unsigned char *dstPtr = (unsigned char *) dst;
    const int dstStride = ALIGN(width >> 1,16);
    unsigned char *dstPtrV = (unsigned char *) dst+ width*height;
    unsigned char *dstPtrU = (unsigned char *) dstPtrV+ dstStride*height/2;
    // copy the entire Y plane
    if(width == stride)
    {
       memcpy(dstPtr, srcPtr, width*height);
       dstPtr += (width*height);
    }
    else
    {
      for(i = 0;i < height;i++)
      {
         memcpy(dstPtr, srcPtr, width);
         dstPtr += width;
         srcPtr += stride;
      }
    }
    // deinterlace the VU data
    int vertical = height / 2;
    int horizontal = width /2;
    for(i = 0; i < vertical; i++) {
        memcpy(dstPtrV,srcPtrV,dstStride);
        srcPtrV += stride << 1;
        dstPtrV += dstStride;
        memcpy(dstPtrU,srcPtrU,dstStride);
        srcPtrU += stride << 1;
        dstPtrU += dstStride;
    }

}
static void YV12ToBGR565(int width, int height, int stride, void *src, void *dst)
{
    unsigned char *yuvs = (unsigned char *)src;
    unsigned char *rgbs = (unsigned char *)dst;

    //the end of the luminance data
    int lumEnd = stride * height;
    //points to the next luminance value pair
    int lumPtr = 0;
    //points to the next chromiance value pair
    int chrPtrU = 0, chrPtrV = 0;

    for (int i = 0; i < height; i += 2) {
        lumPtr = i * stride;
        chrPtrV = i / 2 * stride / 2 + lumEnd;
        chrPtrU = i / 2 * stride / 2 + lumEnd + (stride / 2 * height / 2);
        unsigned char *rgbStart = rgbs;
        for (int j = 0; j < width; j += 2 ) {
            //read the luminance and chromiance values
            int Cb = (yuvs[chrPtrU ++] & 0xff) - 128;
            int Cr = (yuvs[chrPtrV ++] & 0xff) - 128;

            for (int m = 0; m < 2; m ++) { // m
                int lumLine = lumPtr + m * stride;
                unsigned char* pxlrgb = rgbStart + m * width * 2;
                for (int n = 0; n < 2; n ++) { // n
                    int Y = yuvs[lumLine ++] & 0xff;
                    int R, G, B;
                    B = Y + ((454 * Cb) >> 8);
                    if(B < 0) B = 0; else if(B > 255) B = 255;
                    G = Y - ((88 * Cb + 183 * Cr) >> 8);
                    if(G < 0) G = 0; else if(G > 255) G = 255;
                    R = Y + ((359 * Cr) >> 8);
                    if(R < 0) R = 0; else if(R > 255) R = 255;

                    unsigned short *p = (unsigned short *)pxlrgb;
                    *p = (B>>3) | ((G>>2)<<5) | ((R>>3)<<11);
                    pxlrgb += 2;
                } // n
            } // m
            lumPtr += 2;
            rgbStart += 4;
        } // j
        rgbs += 4 * width; // 2 lines
   } // i
}


// covert NV12 (Y plane, interlaced UV bytes) to
// NV21 (Y plane, interlaced VU bytes)
void NV12ToNV21(int width, int height, void *src, void *dst)
{
    int planeSizeY = width * height;
    int planeSizeUV = planeSizeY / 2;
    int i = 0;
    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *dstPtr = (unsigned char *) dst;

    // copy the entire Y plane
    memcpy(dstPtr, src, planeSizeY);

    // byte swap the UV data
    for(i=planeSizeY; i<(planeSizeY+planeSizeUV); i=i+2)
    {
        dstPtr[i] = srcPtr[i + 1];
        dstPtr[i + 1] = srcPtr[i];
    }
}

// P411's Y, U, V are seperated. But the NV12's U and V are interleaved.
void NV12ToP411(int width, int height, void *src, void *dst)
{
    int i, j, p, q;
    unsigned char *pdstU, *pdstV;
    unsigned char *psrcUV;

    // copy Y data
    memcpy(dst, src, width * height);
    // copy U data and V data
    psrcUV = (unsigned char *)src + width * height;
    pdstU = (unsigned char *)dst + width * height;
    pdstV = pdstU + width * height / 4;
    p = q = 0;
    for (i = 0; i < height / 2; i++) {
        for (j = 0; j < width; j++) {
            if (j % 2 == 0) {
                pdstU[p]= (psrcUV[i * width + j] & 0xFF) ;
                p++;
           } else {
                pdstV[q]= (psrcUV[i * width + j] & 0xFF);
                q++;
            }
        }
    }
}

void YU16ToYUYV(int width, int height, void *src, void *dst)
{
    int planeSizeY = width * height;
int planeSizeUV = planeSizeY / 2;

    unsigned char *srcPtrY = (unsigned char *) src;
    unsigned char *srcPtrU = srcPtrY + planeSizeY;
    unsigned char *srcPtrV = srcPtrU + planeSizeUV;
    unsigned char *dstPtr = (unsigned char *) dst;

// interleave: YUYV a macro pixel
    for  (int i = 0; i < planeSizeUV; i ++) {
        * dstPtr ++ = *srcPtrY ++; // Y
        * dstPtr ++ = *srcPtrU ++; // U
        * dstPtr ++ = *srcPtrY ++; // Y
        * dstPtr ++ = *srcPtrV ++; // V
    }
}

void YU16ToYV12(int width, int height, void *src, void *dst)
{
    int planeSizeY = width * height;
    int planeSizeU = planeSizeY / 2;
    int planeSizeV = planeSizeY / 2;
    int newPlaneSizeV = planeSizeY / 4;
    int i = 0;
    int j = 0;
    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *srcPtrU = (unsigned char *) src + planeSizeY;
    unsigned char *srcPtrV = (unsigned char *) src + planeSizeY + planeSizeU;

    unsigned char *dstPtr = (unsigned char *) dst;
    unsigned char *dstPtrV = (unsigned char *) dst + planeSizeY;
    unsigned char *dstPtrU = (unsigned char *) dst + planeSizeY + newPlaneSizeV;

    unsigned char * pTmp;

    // copy the entire Y plane
    memcpy(dstPtr, srcPtr, planeSizeY);

    // handle the V data
    int vertical = height / 2;
    int horizontal = width / 2;
    for(i = 0; i < vertical; i++) {
        pTmp = srcPtrV + 2 * i * horizontal;
        for (j = 0; j < horizontal; j++)
            *dstPtrV++ = (pTmp[j] + (pTmp + horizontal)[j]) / 2;
    }
    // handle the U data
    for(i = 0; i < vertical; i++) {
        pTmp = srcPtrU + 2 * i * horizontal;
        for (j = 0; j < horizontal; j++)
            *dstPtrU++ = (pTmp[j] + (pTmp + horizontal)[j]) / 2;
    }
}

void YU16ToNV12(int width, int height, void *src, void *dst)
{
    int planeSizeY = width * height;
    int planeSizeU = planeSizeY / 2;
    int i = 0;
    int j = 0;
    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *srcPtrU = (unsigned char *) src + planeSizeY;
    unsigned char *srcPtrV = (unsigned char *) srcPtrU + planeSizeU;
    unsigned char *dstPtr = (unsigned char *) dst;

    // copy the entire Y plane
    memcpy(dstPtr, srcPtr, planeSizeY);
    dstPtr += planeSizeY;

    // deinterlace the UV data
    int vertical = height / 2;
    int horizontal = width / 2;
    for(i = 0; i < vertical; i++) {
        for (j = 0; j < horizontal; j++) {
            *dstPtr++ = srcPtrU[2 * i * horizontal + j];
            *dstPtr++ = srcPtrV[2 * i * horizontal + j];
        }
    }
}
void YU16ToNV21(int width, int height, void *src, void *dst)
{
    int planeSizeY = width * height;
    int planeSizeU = planeSizeY / 2;
    int i = 0;
    int j = 0;
    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *srcPtrU = (unsigned char *) src + planeSizeY;
    unsigned char *srcPtrV = (unsigned char *) srcPtrU + planeSizeU;
    unsigned char *dstPtr = (unsigned char *) dst;

    // copy the entire Y plane
    memcpy(dstPtr, srcPtr, planeSizeY);
    dstPtr += planeSizeY;

    // deinterlace the UV data
    int vertical = height / 2;
    int horizontal = width / 2;
    for(i = 0; i < vertical; i++) {
        for (j = 0; j < horizontal; j++) {
            *dstPtr++ = srcPtrV[2 * i * horizontal + j];
            *dstPtr++ = srcPtrU[2 * i * horizontal + j];
        }
    }
}


// covert NV12 (Y plane, interlaced UV bytes) to
// YV12 (Y plane, V plane, U plane)
void NV12ToYV12(int width, int height, void *src, void *dst)
{
    int planeSizeY = width * height;
    int planeSizeUV = planeSizeY / 2;
    int planeUOffset = planeSizeUV / 2;
    int i = 0;
    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *dstPtr = (unsigned char *) dst;
    unsigned char *dstPtrV = (unsigned char *) dst + planeSizeY;
    unsigned char *dstPtrU = (unsigned char *) dst + planeSizeY + planeUOffset;

    // copy the entire Y plane
    memcpy(dstPtr, src, planeSizeY);

    // deinterlace the UV data
    for(i=planeSizeY; i<(planeSizeY+planeSizeUV); i=i+2)
    {
        *dstPtrV++ = srcPtr[i + 1];
        *dstPtrU++ = srcPtr[i];
    }
}

void YV12ToNV12(int width, int height, void *src, void *dst)
{
    int planeSizeY = width * height;
    int planeSizeV = planeSizeY / 4;
    int newPlaneSizeUV = planeSizeY / 2;
    int i = 0;
    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *srcPtrV = (unsigned char *) src + planeSizeY;
    unsigned char *srcPtrU = (unsigned char *) srcPtrV + planeSizeV;
    unsigned char * dstPtr = (unsigned char *) dst;

    // copy the entire Y plane
    memcpy(dstPtr, srcPtr, planeSizeY);
    dstPtr += planeSizeY;

    // deinterlace the UV data
    for(i = 0; i < planeSizeV; i++) {
        *dstPtr++ = srcPtrU[i];
        *dstPtr++ = srcPtrV[i];
    }
}
void YV12ToNV21(int width, int height, void *src, void *dst)
{
    int planeSizeY = width * height;
    int planeSizeV = planeSizeY / 4;
    int newPlaneSizeUV = planeSizeY / 2;
    int i = 0;
    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *srcPtrV = (unsigned char *) src + planeSizeY;
    unsigned char *srcPtrU = (unsigned char *) srcPtrV + planeSizeV;
    unsigned char * dstPtr = (unsigned char *) dst;

    // copy the entire Y plane
    memcpy(dstPtr, srcPtr, planeSizeY);
    dstPtr += planeSizeY;

    // deinterlace the UV data
    for(i = 0; i < planeSizeV; i++) {
        *dstPtr++ = srcPtrV[i];
        *dstPtr++ = srcPtrU[i];
    }
}
/*
convert YV12 to NV21
width: dst image width
height: dst image height
stride: stride for src image
alignheight: aligned height for src image
*/

void YV12ToNV21withStride(int width, int height,int stride,int alignheight, void *src, void *dst)
{
    int planeSizeY = stride * alignheight;
    int planeSizeV = stride * alignheight / 4;
    int newPlaneSizeY = width * height;
    int i = 0;
    int j = 0;
    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *srcPtrV = (unsigned char *) src + planeSizeY;
    unsigned char *srcPtrU = (unsigned char *) srcPtrV + planeSizeV;
    unsigned char * dstPtr = (unsigned char *) dst;
    unsigned char * dstPtrVU = (unsigned char *) dstPtr + newPlaneSizeY;

    // copy the entire Y plane
    if(width == stride)
    {
       memcpy(dstPtr, srcPtr, width * height);
       dstPtr += (width * height);
    }
    else
    {
      for(i = 0;i < height;i++)
      {
         memcpy(dstPtr, srcPtr, width);
         dstPtr += width;
         srcPtr += stride;
      }
   }
    // deinterlace the UV data
    int vertical = height / 2;
    int horizontal = width / 2;
    for(i = 0; i < vertical; i++) {
        for( j = 0; j < horizontal; j ++) {
        *dstPtr++ = srcPtrV[j];
        *dstPtr++ = srcPtrU[j];
       }
        srcPtrV += stride/2;
        srcPtrU += stride/2;
    }
}
void RepaddingYV12(int width, int height, int srcStride, int dstStride,int alignheight, void *src, void *dst,int dstAlignTo16)
{
    // copy the entire Y plane
    if (srcStride == dstStride) {
        memcpy(dst, src, dstStride * height);
    } else {
        unsigned char *srcPtrY = (unsigned char *)src;
        unsigned char *dstPtrY = (unsigned char *)dst;
        for (int i = 0; i < height; i ++) {
            memcpy(dstPtrY, srcPtrY, width);
            srcPtrY += srcStride;
            dstPtrY += dstStride;
        }
    }

    // copy VU plane
    const int scStride = srcStride >> 1;
    int dcStride = 0;
    if(dstAlignTo16 == 1)
    {
       dcStride = ALIGN(dstStride >> 1,16); // Android CTS required: U/V plane needs 16 bytes aligned!
    }
    else
    {
       dcStride = dstStride >> 1;
    }
    if (dcStride == scStride) {
        unsigned char *srcPtrV = (unsigned char *)src + alignheight * srcStride;
        unsigned char *dstPtrV = (unsigned char *)dst + height * dstStride;
        memcpy(dstPtrV, srcPtrV, height/2 * dcStride);
        unsigned char *srcPtrU = srcPtrV + alignheight/2 * scStride;
        unsigned char *dstPtrU = dstPtrV + height/2 * dcStride;
        memcpy(dstPtrU, srcPtrU, height/2 * dcStride);
    } else {
        const int wHalf = width >> 1;
        const int hHalf = height >> 1;
        unsigned char *srcPtrV = (unsigned char *)src + alignheight * srcStride;
        unsigned char *srcPtrU = srcPtrV + scStride * alignheight/2;
        unsigned char *dstPtrV = (unsigned char *)dst + height * dstStride;
        unsigned char *dstPtrU = dstPtrV + dcStride * hHalf;
        for (int i = 0; i < hHalf; i ++) {
            memcpy(dstPtrU, srcPtrU, wHalf);
            memcpy(dstPtrV, srcPtrV, wHalf);
            dstPtrU += dcStride;
            srcPtrU += scStride;
            dstPtrV += dcStride;
            srcPtrV += scStride;
        }
    }
}


static status_t colorConvertYUYV(int dstFormat, int width, int height, void *src, void *dst)
{
    switch (dstFormat) {
    case V4L2_PIX_FMT_NV12:
        YUYVToNV12(width, height, src, dst);
        break;
    case V4L2_PIX_FMT_NV21:
        YUYVToNV21(width, height, src, dst);
        break;
    case V4L2_PIX_FMT_RGB565:
        YUYVToRGB565(width, height, src, dst);
        break;
    case V4L2_PIX_FMT_RGB32:
        YUYVToRGB8888(width, height, src, dst);
        break;
    default:
        ALOGE("Invalid color format (dest)");
        return BAD_VALUE;
    };

    return NO_ERROR;
}

static status_t colorConvertNV12(int dstFormat, int width, int height, void *src, void *dst)
{
    switch (dstFormat) {
    case V4L2_PIX_FMT_NV21:
        NV12ToNV21(width, height, src, dst);
        break;
    case V4L2_PIX_FMT_YUV420:
        NV12ToYV12(width, height, src, dst);
        break;
    case V4L2_PIX_FMT_RGB565:
        NV12ToRGB565(width, height, src, dst);
        break;
    default:
        ALOGE("Invalid color format (dest)");
        return BAD_VALUE;
    };

    return NO_ERROR;
}
static status_t colorConvertYUV420(int dstFormat, int width, int height, void *src, void *dst)
{
    int stride = 0;
    switch (dstFormat) {
    case V4L2_PIX_FMT_NV21:
        YV12ToNV21(width, height, src, dst);
        break;
    case V4L2_PIX_FMT_NV12:
        YV12ToNV12(width, height, src, dst);
        break;
    case V4L2_PIX_FMT_RGB565:
        YV12ToBGR565(width, height,width,src, dst);
        break;
    case V4L2_PIX_FMT_YUV420:
        stride = ALIGN(width,16);
        RepaddingYV12(width, height, width, stride, height, src, dst);
        break;
    default:
        ALOGE("Invalid color format (dest)");
        return BAD_VALUE;
    };

    return NO_ERROR;
}

status_t colorConvert(int srcFormat, int dstFormat, int width, int height, void *src, void *dst)
{
    status_t status = NO_ERROR;
    int size = 0;
    if ((srcFormat == dstFormat) && (srcFormat != V4L2_PIX_FMT_YUV420)) {
        ALOGD("src format is the same as dst format");
        size = frameSize(srcFormat,width,height);
        memcpy(dst,src,size);
        return NO_ERROR;
    }

    switch (srcFormat) {
    case V4L2_PIX_FMT_YUYV:
        return colorConvertYUYV(dstFormat, width, height, src, dst);
    case V4L2_PIX_FMT_NV12:
        return colorConvertNV12(dstFormat, width, height, src, dst);
    case V4L2_PIX_FMT_YUV420:
        return colorConvertYUV420(dstFormat, width, height, src, dst);
    default:
        ALOGE("invalid (source) color format");
        return BAD_VALUE;
    };
}
static status_t colorConvertNV12withStride(int dstFormat, int stride,int width, int alignheight,int height, void *src, void *dst)
{
    switch (dstFormat) {
    case V4L2_PIX_FMT_RGB565:
        NV12ToRGB565withStride(width, height,stride,alignheight, src, dst);
        break;
    default:
        ALOGE("Invalid color format (dest)");
        return BAD_VALUE;
    };

    return NO_ERROR;
}
static status_t colorConvertYUV422PwithStride(int dstFormat, int stride,int width, int alignheight,int height, void *src, void *dst)
{
    switch (dstFormat) {
    case V4L2_PIX_FMT_NV12:
        YUV422PToNV12withStride(width, height,stride,alignheight, src, dst);
        break;
    case V4L2_PIX_FMT_NV21:
        YUV422PToNV21withStride(width, height,stride,alignheight, src, dst);
        break;
    case V4L2_PIX_FMT_YUV420:
        YUV422PToYV12withStride(width, height,stride,alignheight, src, dst);
        break;
    default:
        ALOGE("Invalid color format (dst)");
        return BAD_VALUE;
    };

    return NO_ERROR;
}
static status_t colorConvertYV12withStride(int dstFormat, int stride,int width, int alignheight,int height, void *src, void *dst)
{
    switch (dstFormat) {
        case V4L2_PIX_FMT_NV21:
        YV12ToNV21withStride(width, height,stride,alignheight, src, dst);
        break;
    default:
        ALOGE("Invalid color format (dst)");
        return BAD_VALUE;
    };

    return NO_ERROR;
}

int copyBufWithStride(void *dst, void *src, int width, int height, int srcstride,int dststride,int alignheight,int srcFormat)
{
    char *psrc, *pdst;
    int i, uvstride, uvwidth;
    LOG1("@%s", __FUNCTION__);
    psrc = (char *)src;
    pdst = (char *)dst;
    if (srcFormat == HAL_PIXEL_FORMAT_YV12 ||srcFormat == V4L2_PIX_FMT_YUV420) {
        RepaddingYV12(width,height,srcstride,dststride,alignheight,src,dst);
    }
    else if(srcFormat == HAL_PIXEL_FORMAT_YCrCb_420_SP || srcFormat == V4L2_PIX_FMT_NV21)//nv21
    {
        if (srcstride > width) {
            for (i = 0; i < height; i++) {
                memcpy(pdst, psrc, width);
                pdst += width;
                psrc += srcstride;
            }
            uvwidth = width;
            uvstride = srcstride;
            for (i = 0; i < height/2; i++) {
                memcpy(pdst, psrc, uvwidth);
                pdst += uvwidth;
                psrc +=uvstride;
            }
        } else if (srcstride == width) {
            memcpy(dst, src, width*height*3/2);
        } else {
            ALOGE("@%s, line:%d, wrong, stride:%d < width:%d", __FUNCTION__, __LINE__, srcstride, width);
            return -1;
        }
    }
    else
    {
       ALOGE("@%s, unsupport format", __FUNCTION__);
    }
    return 0;
}

status_t colorConvertwithStride(int srcFormat, int dstFormat, int stride,int width, int alignHeight, int height, void *src, void *dst)
{
    if (srcFormat == dstFormat) {
        return copyBufWithStride(dst,src,width,height,stride,width,alignHeight,srcFormat);
    }

    switch (srcFormat) {
    case V4L2_PIX_FMT_NV12:
    case HAL_PIXEL_FORMAT_NV12_TILED_INTEL:
        return colorConvertNV12withStride(dstFormat,stride, width, alignHeight,height, src, dst);
    case V4L2_PIX_FMT_YUV422P:
    case HAL_PIXEL_FORMAT_YCbCr_422_H_INTEL:
        return colorConvertYUV422PwithStride(dstFormat,stride, width, alignHeight,height, src, dst);
    case V4L2_PIX_FMT_YUV420://yv12
        return colorConvertYV12withStride(dstFormat,stride, width, alignHeight,height, src, dst);
    default:
        ALOGE("invalid (source) color format");
        return BAD_VALUE;
    };
}

const char *cameraParametersFormat(int v4l2Format)
{
    switch (v4l2Format) {
    case V4L2_PIX_FMT_YUV420:
        return CameraParameters::PIXEL_FORMAT_YUV420P;
    case V4L2_PIX_FMT_NV21:
        return CameraParameters::PIXEL_FORMAT_YUV420SP;
    case V4L2_PIX_FMT_YUYV:
        return CameraParameters::PIXEL_FORMAT_YUV422I;
    case V4L2_PIX_FMT_JPEG:
        return CameraParameters::PIXEL_FORMAT_JPEG;
    default:
        ALOGE("failed to map format %x to a PIXEL_FORMAT\n", v4l2Format);
        return NULL;
    };
}

int V4L2Format(const char *cameraParamsFormat)
{
    LOG1("@%s", __FUNCTION__);
    if (!cameraParamsFormat) {
        ALOGE("null cameraParamsFormat");
        return -1;
    }

    int len = strlen(CameraParameters::PIXEL_FORMAT_YUV420SP);
    if (strncmp(cameraParamsFormat, CameraParameters::PIXEL_FORMAT_YUV420SP, len) == 0)
        return V4L2_PIX_FMT_NV21;

    len = strlen(CameraParameters::PIXEL_FORMAT_YUV420P);
    if (strncmp(cameraParamsFormat, CameraParameters::PIXEL_FORMAT_YUV420P, len) == 0)
        return V4L2_PIX_FMT_YUV420;

    len = strlen(CameraParameters::PIXEL_FORMAT_JPEG);
    if (strncmp(cameraParamsFormat, CameraParameters::PIXEL_FORMAT_JPEG, len) == 0)
        return V4L2_PIX_FMT_JPEG;

    len = strlen(CameraParameters::PIXEL_FORMAT_YUV422I);
    if (strncmp(cameraParamsFormat, CameraParameters::PIXEL_FORMAT_YUV422I, len) == 0)
        return V4L2_PIX_FMT_YUYV;

    ALOGE("invalid format %s", cameraParamsFormat);
    return -1;
}
int V4L2ToLumaBitsPerPixel(int format)
{
    switch(format) {
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_YVU420:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_YUV411P:
        case V4L2_PIX_FMT_YUV422P:
           return 1;
        case V4L2_PIX_FMT_YUYV://yuy2
           return 2;
        case V4L2_PIX_FMT_RGB32:
           return 4;
        default:
           return 1;
        }
}
int V4L2FormatToHalPixel(int format)
{
     switch(format) {
        case V4L2_PIX_FMT_YVU420:
            return HAL_PIXEL_FORMAT_YV12;
        case V4L2_PIX_FMT_YUV420:
            return HAL_PIXEL_FORMAT_YV12;
        case V4L2_PIX_FMT_NV12:
            return HAL_PIXEL_FORMAT_NV12_TILED_INTEL;
        case V4L2_PIX_FMT_NV21:
            return HAL_PIXEL_FORMAT_YCrCb_420_SP;
        case V4L2_PIX_FMT_YUV422P:
            return HAL_PIXEL_FORMAT_YCbCr_422_H_INTEL;
        case V4L2_PIX_FMT_YUYV://yuy2
            return HAL_PIXEL_FORMAT_YCbCr_422_I;
        default:
            ALOGE("unkown color format in V4L2FormatToHalPixel");
            return -1;
     }
}
int HalPixelToV4L2Format(int format)
{
     switch(format) {
        case HAL_PIXEL_FORMAT_YV12:
            return V4L2_PIX_FMT_YVU420;
        case HAL_PIXEL_FORMAT_NV12_TILED_INTEL:
            return V4L2_PIX_FMT_NV12;
        case HAL_PIXEL_FORMAT_YCbCr_422_H_INTEL:
            return V4L2_PIX_FMT_YUV422P;
        case HAL_PIXEL_FORMAT_YCbCr_422_I://yuy2
            return V4L2_PIX_FMT_YUYV;
        default:
            ALOGE("unkown color format");
            return -1;
     }
}


} // namespace android
