/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#define LOG_TAG "AudioConversion"

#include "AudioConversion.hpp"
#include "AudioConverter.hpp"
#include "AudioReformatter.hpp"
#include "AudioRemapper.hpp"
#include "AudioResampler.hpp"
#include "AudioUtils.hpp"
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <media/AudioBufferProvider.h>
#include <stdlib.h>

using audio_comms::utilities::Log;
using namespace android;
using namespace std;

namespace intel_audio
{

const uint32_t AudioConversion::mMaxRate = 92000;

const uint32_t AudioConversion::mMinRate = 8000;

const uint32_t AudioConversion::mAllocBufferMultFactor = 2;

AudioConversion::AudioConversion()
    : mConvOutBufferIndex(0),
      mConvOutFrames(0),
      mConvOutBufferSizeInFrames(0),
      mConvOutBuffer(NULL)
{
    mAudioConverter[ChannelCountSampleSpecItem] = new AudioRemapper(ChannelCountSampleSpecItem);
    mAudioConverter[FormatSampleSpecItem] = new AudioReformatter(FormatSampleSpecItem);
    mAudioConverter[RateSampleSpecItem] = new AudioResampler(RateSampleSpecItem);
}

AudioConversion::~AudioConversion()
{
    for (int i = 0; i < NbSampleSpecItems; i++) {

        delete mAudioConverter[i];
        mAudioConverter[i] = NULL;
    }

    free(mConvOutBuffer);
    mConvOutBuffer = NULL;
}

status_t AudioConversion::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    status_t ret = NO_ERROR;

    emptyConversionChain();

    free(mConvOutBuffer);

    mConvOutBuffer = NULL;
    mConvOutBufferIndex = 0;
    mConvOutFrames = 0;
    mConvOutBufferSizeInFrames = 0;

    mSsSrc = ssSrc;
    mSsDst = ssDst;

    if (ssSrc == ssDst) {
        Log::Debug() << __FUNCTION__ << ": no convertion required";
        return ret;
    }

    SampleSpec tmpSsSrc = ssSrc;

    // Start by adding the remapper, it will add consequently the reformatter and resampler
    // This function may alter the source sample spec
    ret = configureAndAddConverter(ChannelCountSampleSpecItem, &tmpSsSrc, &ssDst);
    if (ret != NO_ERROR) {

        return ret;
    }

    // Assert the temporary sample spec equals the destination sample spec
    AUDIOCOMMS_ASSERT(tmpSsSrc == ssDst, "Could not provide converter");

    return ret;
}

status_t AudioConversion::getConvertedBuffer(void *dst,
                                             const uint32_t outFrames,
                                             AudioBufferProvider *bufferProvider)
{
    AUDIOCOMMS_ASSERT(bufferProvider != NULL, "NULL source buffer");
    AUDIOCOMMS_ASSERT(dst != NULL, "NULL destination buffer");

    status_t status = NO_ERROR;

    if (mActiveAudioConvList.empty()) {
        Log::Error() << __FUNCTION__ << ": conversion called with empty converter list";
        return NO_INIT;
    }

    //
    // Realloc the Output of the conversion if required (with margin of the worst case)
    //
    if (mConvOutBufferSizeInFrames < outFrames) {

        mConvOutBufferSizeInFrames = outFrames +
                                     (mMaxRate / mMinRate) * mAllocBufferMultFactor;
        int16_t *reallocBuffer =
            static_cast<int16_t *>(realloc(mConvOutBuffer,
                                           mSsDst.convertFramesToBytes(
                                               mConvOutBufferSizeInFrames)));
        if (reallocBuffer == NULL) {
            Log::Error() << __FUNCTION__ << ": (frames=" << outFrames << " ): realloc failed";
            return NO_MEMORY;
        }
        mConvOutBuffer = reallocBuffer;
    }

    size_t framesRequested = outFrames;

    //
    // Frames are already available from the ConvOutBuffer, empty it first!
    //
    if (mConvOutFrames) {

        size_t frameToCopy = min(framesRequested, mConvOutFrames);
        framesRequested -= frameToCopy;
        mConvOutBufferIndex += mSsDst.convertFramesToBytes(frameToCopy);
    }

    //
    // Frames still needed? (_pConvOutBuffer emptied!)
    //
    while (framesRequested != 0) {

        //
        // Outputs in the convOutBuffer
        //
        AudioBufferProvider::Buffer &buffer(mConvInBuffer);

        // Calculate the frames we need to get from buffer provider
        // (Runs at ssSrc sample spec)
        // Note that is is rounded up.
        buffer.frameCount = AudioUtils::convertSrcToDstInFrames(framesRequested, mSsDst, mSsSrc);

        //
        // Acquire next buffer from buffer provider
        //
        status = bufferProvider->getNextBuffer(&buffer);
        if (status != NO_ERROR) {

            return status;
        }

        //
        // Convert
        //
        size_t convertedFrames;
        char *convBuf = reinterpret_cast<char *>(mConvOutBuffer) + mConvOutBufferIndex;
        status = convert(buffer.raw, reinterpret_cast<void **>(&convBuf),
                         buffer.frameCount, &convertedFrames);
        if (status != NO_ERROR) {

            bufferProvider->releaseBuffer(&buffer);
            return status;
        }

        mConvOutFrames += convertedFrames;
        mConvOutBufferIndex += mSsDst.convertFramesToBytes(convertedFrames);

        size_t framesToCopy = min(framesRequested, convertedFrames);
        framesRequested -= framesToCopy;

        //
        // Release the buffer
        //
        bufferProvider->releaseBuffer(&buffer);
    }

    //
    // Copy requested outFrames from the output buffer of the conversion.
    // Move the remaining frames to the beginning of the convOut buffer
    //
    memcpy(dst, mConvOutBuffer, mSsDst.convertFramesToBytes(outFrames));
    mConvOutFrames -= outFrames;

    //
    // Move the remaining frames to the beginning of the convOut buffer
    //
    if (mConvOutFrames) {

        memmove(mConvOutBuffer,
                reinterpret_cast<char *>(mConvOutBuffer) + mSsDst.convertFramesToBytes(outFrames),
                mSsDst.convertFramesToBytes(mConvOutFrames));
    }

    // Reset buffer Index
    mConvOutBufferIndex = 0;

    return NO_ERROR;
}

status_t AudioConversion::convert(const void *src,
                                  void **dst,
                                  const uint32_t inFrames,
                                  uint32_t *outFrames)
{
    AUDIOCOMMS_ASSERT(src != NULL, "NULL source buffer");
    const void *srcBuf = src;
    void *dstBuf = NULL;
    size_t srcFrames = inFrames;
    size_t dstFrames = 0;
    status_t status = NO_ERROR;

    if (mActiveAudioConvList.empty()) {

        // Empty converter list -> No need for convertion
        // Copy the input on the ouput if provided by the client
        // or points on the imput buffer
        if (*dst) {

            memcpy(*dst, src, mSsSrc.convertFramesToBytes(inFrames));
            *outFrames = inFrames;
        } else {

            *dst = (void *)src;
            *outFrames = inFrames;
        }
        return NO_ERROR;
    }

    AudioConverterListIterator it;
    for (it = mActiveAudioConvList.begin(); it != mActiveAudioConvList.end(); ++it) {

        AudioConverter *pConv = *it;
        dstBuf = NULL;
        dstFrames = 0;

        if (*dst && (pConv == mActiveAudioConvList.back())) {

            // Last converter must output within the provided buffer (if provided!!!)
            dstBuf = *dst;
        }
        status = pConv->convert(srcBuf, &dstBuf, srcFrames, &dstFrames);
        if (status != NO_ERROR) {

            return status;
        }

        srcBuf = dstBuf;
        srcFrames = dstFrames;
    }

    *dst = dstBuf;
    *outFrames = dstFrames;

    return status;
}

void AudioConversion::emptyConversionChain()
{
    mActiveAudioConvList.clear();
}

status_t AudioConversion::doConfigureAndAddConverter(SampleSpecItem sampleSpecItem,
                                                     SampleSpec *ssSrc,
                                                     const SampleSpec *ssDst)
{
    AUDIOCOMMS_ASSERT(sampleSpecItem < NbSampleSpecItems, "Sample Spec item out of range");

    SampleSpec tmpSsDst = *ssSrc;
    tmpSsDst.setSampleSpecItem(sampleSpecItem, ssDst->getSampleSpecItem(sampleSpecItem));

    if (sampleSpecItem == ChannelCountSampleSpecItem) {

        tmpSsDst.setChannelsPolicy(ssDst->getChannelsPolicy());
    }

    status_t ret = mAudioConverter[sampleSpecItem]->configure(*ssSrc, tmpSsDst);
    if (ret != NO_ERROR) {

        return ret;
    }
    mActiveAudioConvList.push_back(mAudioConverter[sampleSpecItem]);
    *ssSrc = tmpSsDst;

    return NO_ERROR;
}

status_t AudioConversion::configureAndAddConverter(SampleSpecItem sampleSpecItem,
                                                   SampleSpec *ssSrc,
                                                   const SampleSpec *ssDst)
{
    AUDIOCOMMS_ASSERT(sampleSpecItem < NbSampleSpecItems, "Sample Spec item out of range");

    // If the input format size is higher, first perform the reformat
    // then add the resampler
    // and perform the reformat (if not already done)
    if (ssSrc->getSampleSpecItem(sampleSpecItem) > ssDst->getSampleSpecItem(sampleSpecItem)) {

        status_t ret = doConfigureAndAddConverter(sampleSpecItem, ssSrc, ssDst);
        if (ret != NO_ERROR) {

            return ret;
        }
    }

    if ((sampleSpecItem + 1) < NbSampleSpecItems) {
        // Dive
        status_t ret = configureAndAddConverter((SampleSpecItem)(sampleSpecItem + 1), ssSrc,
                                                ssDst);
        if (ret != NO_ERROR) {

            return ret;
        }
    }

    // Handle the case of destination sample spec item is higher than input sample spec
    // or destination and source channels policy are different
    if (!SampleSpec::isSampleSpecItemEqual(sampleSpecItem, *ssSrc, *ssDst)) {

        return doConfigureAndAddConverter(sampleSpecItem, ssSrc, ssDst);
    }
    return NO_ERROR;
}
}  // namespace intel_audio
