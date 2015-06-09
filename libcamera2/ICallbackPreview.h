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

#ifndef ICALLBACKPREVIEW_H_
#define ICALLBACKPREVIEW_H_

namespace android {

class AtomBuffer;

/**
 * \class ICallbackPreview
 *
 *  Interface implemented by classes that want to receive preview frames after
 *  PreviewThread has finish with them
 *
 *  split from PreviewThread.h to reduce header dependencies.
 */
class ICallbackPreview {
public:
    /**
     * \enum CallbackType
     * Types of callbacks the PreviewThread accepts
     */
    enum CallbackType {
        INPUT,          /*!< Callback is triggered before being processed by PreviewThread for every frame */
        INPUT_ONCE,     /*!< Callback is triggered only once before frame has been processed by PreviewThread (i.e. rendered) */
        OUTPUT,         /*!< Callback is triggered after PreviewThread has render the frame for every frame */
        OUTPUT_ONCE,    /*!< Callback is triggered once after PreviewThread has render the frame */
        OUTPUT_WITH_DATA/*!< Callback is triggered after PreviewThread has render the frame for every frame.
                             The AtomBuffer associated with this frame is also sent. This is how we pass the
                             preview frame to the PostProcThread */
    };

    ICallbackPreview() {}
    virtual ~ICallbackPreview() {}
    virtual void previewBufferCallback(AtomBuffer *memory, CallbackType t) = 0;
    virtual int getCameraID() = 0;
};

} // namespace android

#endif /* ICALLBACKPREVIEW_H_ */
