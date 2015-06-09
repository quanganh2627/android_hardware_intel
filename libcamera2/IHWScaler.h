/*
 * Copyright (c) 2012 Intel Corporation
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

#ifndef IHW_SCALER_H_
#define IHW_SCALER_H_

#include <gui/Surface.h>
namespace android {

/**
 *
 */
class IHWScaler
{
public:
    virtual ~IHWScaler() {};

    virtual void setZoomFactor(float zf) = 0;
    virtual int  processFrame(int inputBufferId, int outputBufferId) = 0;
    virtual int  addOutputBuffer(buffer_handle_t *pBufHandle, int width, int height, int stride, int format) = 0;
    virtual int  addInputBuffer(buffer_handle_t *pBufHandle, int width, int height, int stride, int format) = 0;
    virtual void removeInputBuffer(int bufferId) = 0;
    virtual void removeOutputBuffer(int bufferId) = 0;
};

} //namespace

#endif /* IHW_SCALER_H_ */
