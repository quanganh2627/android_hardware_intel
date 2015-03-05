/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#ifndef MRFLPRIMARYPLANE_H_
#define MRFLPRIMARYPLANE_H_

#include <displayclass_interface.h>
#include <SpritePlane.h>

namespace android {
namespace intel {

class MrflPrimaryPlane : public SpritePlane {
public:
    MrflPrimaryPlane(int index, int pipe);
    ~MrflPrimaryPlane();
public:
    // data source
    bool isValidBuffer(uint32_t handle);
    bool setDataBuffer(uint32_t handle);

    // display device
    bool assignToPipe(uint32_t pipe);

    void setZOrderConfig(ZOrderConfig& config);

    // hardware operations
    bool reset();
    bool flip();
    bool enable();
    bool disable();

    void* getContext() const;
private:
    void checkPosition(int& x, int& y, int& w, int& h);
    bool setDataBuffer(IBufferMapper& mapper);
    void setFramebufferTarget(IDataBuffer& buf);
private:
    struct intel_dc_plane_ctx mContext;
    bool mForceBottom;
};

} // namespace intel
} // namespace android

#endif /* MRFLPRIMARYPLANE_H_ */
