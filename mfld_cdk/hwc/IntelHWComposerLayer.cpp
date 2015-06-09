/*
 * Copyright (c) 2008-2012, Intel Corporation. All rights reserved.
 *
 * Redistribution.
 * Redistribution and use in binary form, without modification, are
 * permitted provided that the following conditions are met:
 *  * Redistributions must reproduce the above copyright notice and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its
 * suppliers may be used to endorse or promote products derived from
 * this software without specific  prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this
 * software is permitted.
 *
 * Limited patent license.
 * Intel Corporation grants a world-wide, royalty-free, non-exclusive
 * license under patents it now or hereafter owns or controls to make,
 * have made, use, import, offer to sell and sell ("Utilize") this
 * software, but solely to the extent that any such patent is necessary
 * to Utilize the software alone, or in combination with an operating
 * system licensed under an approved Open Source license as listed by
 * the Open Source Initiative at http://opensource.org/licenses.
 * The patent license shall not apply to any other combinations which
 * include this software. No hardware per se is licensed hereunder.
 *
 * DISCLAIMER.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <IntelHWComposerLayer.h>
#include <IntelHWComposerCfg.h>

IntelHWComposerLayer::IntelHWComposerLayer()
    : mHWCLayer(0), mPlane(0), mFlags(0)
{

}

IntelHWComposerLayer::IntelHWComposerLayer(hwc_layer_1_t *layer,
                                           IntelDisplayPlane *plane,
                                           int flags)
    : mHWCLayer(layer), mPlane(plane), mFlags(flags), mForceOverlay(false),
      mLayerType(0), mFormat(0), mIsProtected(false)
{

}

IntelHWComposerLayer::~IntelHWComposerLayer()
{

}

IntelHWComposerLayerList::IntelHWComposerLayerList(IntelDisplayPlaneManager *pm)
    : mLayerList(0),
      mPlaneManager(pm),
      mNumLayers(0),
      mNumRGBLayers(0),
      mNumYUVLayers(0),
      mAttachedSpritePlanes(0),
      mAttachedOverlayPlanes(0),
      mNumAttachedPlanes(0)
{
    if (!mPlaneManager)
        mInitialized = false;
    else
        mInitialized = true;
}

IntelHWComposerLayerList::~IntelHWComposerLayerList()
{
    if (!initCheck())
        return;

    // delete list
    mPlaneManager = 0;
    delete[] mLayerList;
    mNumLayers = 0;
    mNumRGBLayers = 0;
    mNumYUVLayers= 0;
    mAttachedSpritePlanes = 0;
    mAttachedOverlayPlanes = 0;
    mInitialized = false;
}

void IntelHWComposerLayerList::updateLayerList(hwc_display_contents_1_t *layerList)
{
    int numLayers;
    int numRGBLayers = 0;
    int numYUVLayers = 0;

    if (!layerList) {
        mNumLayers = 0;
        mNumRGBLayers = 0;
        mNumYUVLayers = 0;
        mAttachedSpritePlanes = 0;
        mAttachedOverlayPlanes = 0;
        mNumAttachedPlanes = 0;

        delete [] mLayerList;
        mLayerList = 0;
        return;
    }

    numLayers = layerList->numHwLayers;
    // reset numLayers minus one if HWC supports FRAMEBUFFER_TARGET
    // For FRAMEBUFFER_TARGET we handle it separately
    if (layerList->hwLayers[numLayers-1].compositionType == HWC_FRAMEBUFFER_TARGET)
        numLayers = layerList->numHwLayers - 1;

    if (numLayers <= 0 || !initCheck()) {
        mNumLayers = 0;
        mNumRGBLayers = 0;
        mNumYUVLayers = 0;
        mAttachedSpritePlanes = 0;
        mAttachedOverlayPlanes = 0;
        mNumAttachedPlanes = 0;
        return;
    }

    if (mNumLayers < numLayers) {
        delete [] mLayerList;
        mLayerList = new IntelHWComposerLayer[numLayers];
        if (!mLayerList) {
            ALOGE("%s: failed to create layer list\n", __func__);
            return;
        }
    }

    for (int i = 0; i < numLayers; i++) {
        mLayerList[i].mHWCLayer = &layerList->hwLayers[i];
        mLayerList[i].mPlane = 0;
        mLayerList[i].mFlags = 0;
        mLayerList[i].mForceOverlay = false;
        mLayerList[i].mNeedClearup = false;
        mLayerList[i].mLayerType = IntelHWComposerLayer::LAYER_TYPE_INVALID;
        mLayerList[i].mFormat = 0;
        mLayerList[i].mIsProtected = false;

        // update layer format
        IMG_native_handle_t *grallocHandle =
            (IMG_native_handle_t*)layerList->hwLayers[i].handle;

        if (!grallocHandle)
            continue;

        mLayerList[i].mFormat = grallocHandle->iFormat;

        if (grallocHandle->iFormat == HAL_PIXEL_FORMAT_YV12 ||
            grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12 ||
            grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12_VED ||
            grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_YUY2 ||
            grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_UYVY ||
            grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_I420 ||
            grallocHandle->iFormat == HAL_PIXEL_FORMAT_INTEL_HWC_NV12_TILE) {
            mLayerList[i].mLayerType = IntelHWComposerLayer::LAYER_TYPE_YUV;
            numYUVLayers++;
        } else if (grallocHandle->iFormat == HAL_PIXEL_FORMAT_RGB_565 ||
            grallocHandle->iFormat == HAL_PIXEL_FORMAT_BGRA_8888 ||
            grallocHandle->iFormat == HAL_PIXEL_FORMAT_BGRX_8888 ||
            grallocHandle->iFormat == HAL_PIXEL_FORMAT_RGBX_8888 ||
            grallocHandle->iFormat == HAL_PIXEL_FORMAT_RGBA_8888) {
            mLayerList[i].mLayerType = IntelHWComposerLayer::LAYER_TYPE_RGB;
            numRGBLayers++;
        } else
            ALOGW("updateLayerList: unknown format 0x%x", grallocHandle->iFormat);

        // check if a protected layer
        if (grallocHandle->usage & GRALLOC_USAGE_PROTECTED)
            mLayerList[i].mIsProtected = true;
    }

    mNumLayers = numLayers;
    mNumRGBLayers = numRGBLayers;
    mNumYUVLayers = numYUVLayers;
    mNumAttachedPlanes = 0;
}

bool IntelHWComposerLayerList::invalidatePlanes()
{
    if (!initCheck())
        return false;

    for (int i = 0; i < mNumLayers; i++) {
        if (mLayerList[i].mPlane) {
            mPlaneManager->reclaimPlane(mLayerList[i].mPlane);
            mLayerList[i].mPlane = 0;
        }
    }

    mAttachedSpritePlanes = 0;
    mAttachedOverlayPlanes = 0;
    mNumAttachedPlanes = 0;
    return true;
}

void IntelHWComposerLayerList::attachPlane(int index,
                                           IntelDisplayPlane *plane,
                                           int flags)
{
    if (index < 0 || index >= mNumLayers || !plane) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return;
    }

    if (initCheck()) {
        int type = plane->getPlaneType();

        mLayerList[index].mPlane = plane;
        mLayerList[index].mFlags = flags;
        if (type == IntelDisplayPlane::DISPLAY_PLANE_SPRITE)
            mAttachedSpritePlanes++;
        else if (type == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY ||
                 type == IntelDisplayPlane::DISPLAY_PLANE_RGB_OVERLAY)
            mAttachedOverlayPlanes++;
        mNumAttachedPlanes++;
    }
}

void IntelHWComposerLayerList::detachPlane(int index, IntelDisplayPlane *plane)
{
    if (index < 0 || index >= mNumLayers || !plane) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return;
    }

    if (initCheck()) {
        int type = plane->getPlaneType();

        mPlaneManager->reclaimPlane(plane);
        mLayerList[index].mPlane = 0;
        mLayerList[index].mFlags = 0;
        if (type == IntelDisplayPlane::DISPLAY_PLANE_SPRITE)
            mAttachedSpritePlanes--;
        else if (type == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY ||
                 type == IntelDisplayPlane::DISPLAY_PLANE_RGB_OVERLAY)
            mAttachedOverlayPlanes--;
        mNumAttachedPlanes--;
    }
}

IntelDisplayPlane* IntelHWComposerLayerList::getPlane(int index)
{
    if (index < 0 || index >= mNumLayers) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return 0;
    }

    if (initCheck())
        return mLayerList[index].mPlane;

    return 0;
}

void IntelHWComposerLayerList::setFlags(int index, int flags)
{
    if (index < 0 || index >= mNumLayers) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return;
    }

    if (initCheck())
        mLayerList[index].mFlags = flags;
}

int IntelHWComposerLayerList::getFlags(int index)
{
    if (index < 0 || index >= mNumLayers) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return 0;
    }

    if (initCheck())
        return mLayerList[index].mFlags;

    return 0;
}

void IntelHWComposerLayerList::setForceOverlay(int index, bool isForceOverlay)
{
    if (index < 0 || index >= mNumLayers) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return;
    }

    if (initCheck())
        mLayerList[index].mForceOverlay = isForceOverlay;
}

bool IntelHWComposerLayerList::getForceOverlay(int index)
{
    if (index < 0 || index >= mNumLayers) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return false;
    }

    if (initCheck())
        return mLayerList[index].mForceOverlay;

    return false;
}

void IntelHWComposerLayerList::setNeedClearup(int index, bool needClearup)
{
    if (index < 0 || index >= mNumLayers) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return;
    }

    if (initCheck())
        mLayerList[index].mNeedClearup = needClearup;
}

bool IntelHWComposerLayerList::getNeedClearup(int index)
{
    if (index < 0 || index >= mNumLayers) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return false;
    }

    if (initCheck())
        return mLayerList[index].mNeedClearup;

    return false;
}

int IntelHWComposerLayerList::getLayerType(int index) const
{
    if (!initCheck() || index < 0 || index >= mNumLayers) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return IntelHWComposerLayer::LAYER_TYPE_INVALID;
    }

    return mLayerList[index].mLayerType;
}

int IntelHWComposerLayerList::getLayerFormat(int index) const
{
    if (!initCheck() || index < 0 || index >= mNumLayers) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return 0;
    }

    return mLayerList[index].mFormat;
}

bool IntelHWComposerLayerList::isProtectedLayer(int index) const
{
    if (!initCheck() || index < 0 || index >= mNumLayers) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return false;
    }

    return mLayerList[index].mIsProtected;
}

int IntelHWComposerLayerList::getRGBLayerCount() const
{
    if (!initCheck()) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return 0;
    }

    return mNumRGBLayers;
}

int IntelHWComposerLayerList::getYUVLayerCount() const
{
    if (!initCheck()) {
        ALOGE("%s: Invalid parameters\n", __func__);
        return 0;
    }

    return mNumYUVLayers;
}
