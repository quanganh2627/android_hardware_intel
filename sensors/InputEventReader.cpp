/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "InputEventReader.h"

InputEventCircularReader::InputEventCircularReader(size_t numEvents)
    : mBuffer(new input_event[numEvents * 2]),
      mBufferEnd(mBuffer + numEvents),
      mHead(mBuffer),
      mCurr(mBuffer),
      mFreeSpace(numEvents)
{
	D("%s, numEvents = %d", __func__, numEvents);
}

InputEventCircularReader::~InputEventCircularReader()
{
    delete []mBuffer;
}

ssize_t InputEventCircularReader::fill(int fd)
{

    size_t numEventsRead = 0;
    D("%s, fd = %d, mFreeSpace = %d", __func__, fd, mFreeSpace);
    if (mFreeSpace) {
        const ssize_t nread = read(fd, mHead, mFreeSpace * sizeof(input_event));
        D("%s, nread = %d", __func__, nread);
        if (nread<0 || nread % sizeof(input_event)) {
            /* we got a partial event!! */
            E("%s, error while read input event: nread = %ld", __func__, nread);
            return nread<0 ? -errno : -EINVAL;
        }

        numEventsRead = nread / sizeof(input_event);
        D("%s, numEventsRead = %d", __func__, numEventsRead);
        if (numEventsRead) {
            mHead += numEventsRead;
            mFreeSpace -= numEventsRead;
            if (mHead > mBufferEnd) {
                size_t s = mHead - mBufferEnd;
                memcpy(mBuffer, mBufferEnd, s * sizeof(input_event));
                mHead = mBuffer + s;
            }
        }
    }
    D("%s, return numEventsRead = %d", __func__, numEventsRead);
    return numEventsRead;
}

ssize_t InputEventCircularReader::readEvent(input_event const** events)
{
    *events = mCurr;
    ssize_t available = (mBufferEnd - mBuffer) - mFreeSpace;
    D("%s, return available = %d", __func__, available);
    return available ? 1 : 0;
}

void InputEventCircularReader::next()
{
    mCurr++;
    mFreeSpace++;
    if (mCurr >= mBufferEnd) {
        mCurr = mBuffer;
    }
}
