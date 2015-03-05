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

#ifndef HALVIDEOSTABILIZATION_H_
#define HALVIDEOSTABILIZATION_H_

#include "AtomCommon.h"

namespace android {

class HALVideoStabilization : public RefBase {
public:
    explicit HALVideoStabilization() {}
    virtual ~HALVideoStabilization() {}

    static void getEnvelopeSize(int previewWidth, int previewHeight, int &envelopeWidth, int &envelopeHeight, int &bpl);

    void process(const AtomBuffer *inBuf, AtomBuffer *outBuf);
// prevent copy constructor and assignment operator
private:
    HALVideoStabilization(const HALVideoStabilization& other);
    HALVideoStabilization& operator=(const HALVideoStabilization& other);

    const static int ENVELOPE_MULTIPLIER = 6;
    const static int ENVELOPE_DIVIDER = 5;
};

}

#endif /* HALVIDEOSTABILIZATION_H_ */
