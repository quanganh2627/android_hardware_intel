/*
 * Copyright (c) 2014 Intel Corporation
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

#define LOG_TAG "Camera_HAL_VS"

#include "HALVideoStabilization.h"
#include "LogHelper.h"
#include "ImageScaler.h"
#include "assert.h"
#include "JpegCapture.h"

namespace android {

void HALVideoStabilization::getEnvelopeSize(int previewWidth, int previewHeight, int &envelopeWidth, int &envelopeHeight, int &bpl)
{
    LOG1("@%s", __FUNCTION__);
    if (previewWidth == 176 && previewHeight == 144) {
        // qcif support currently through halvs only, using memcpy, so make
        // the envelope equal preview size
        envelopeWidth = 176;
        envelopeHeight = 144;
    } else {
        envelopeWidth = (previewWidth * ENVELOPE_MULTIPLIER) / ENVELOPE_DIVIDER;
        envelopeHeight = (previewHeight * ENVELOPE_MULTIPLIER) / ENVELOPE_DIVIDER;
    }

    bpl = ALIGN64(envelopeWidth);

    LOG1("@%s: selected envelope size %dx%d for preview %dx%d bpl %d", __FUNCTION__,
         envelopeWidth, envelopeHeight, previewWidth, previewHeight, bpl);
}

void HALVideoStabilization::process(const AtomBuffer *inBuf, AtomBuffer *outBuf)
{
    LOG2("@%s", __FUNCTION__);
    assert(inBuf && outBuf && inBuf->width >= outBuf->width);
    assert(inBuf->auxBuf);
    unsigned char *nv12meta = ((unsigned char*)inBuf->auxBuf->dataPtr) + NV12_META_START;

    if (inBuf->width == outBuf->width) {
        memcpy((char *)outBuf->dataPtr, (const char*)inBuf->dataPtr, outBuf->size);
        return;
    }

    uint16_t leftCrop(getU16fromFrame(nv12meta, NV12_META_LEFT_OFFSET_ADDR));
    uint16_t topCrop(getU16fromFrame(nv12meta, NV12_META_TOP_OFFSET_ADDR));

    int rightCrop = inBuf->bpl - outBuf->bpl - leftCrop;
    int bottomCrop = inBuf->height - outBuf->height - topCrop;
    ImageScaler::cropNV12orNV21Image(inBuf, outBuf, leftCrop, rightCrop, topCrop, bottomCrop);
}

}

