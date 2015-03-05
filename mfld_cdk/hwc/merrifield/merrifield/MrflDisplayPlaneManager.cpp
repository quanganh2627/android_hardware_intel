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
#include <Log.h>
#include <MrflDisplayPlaneManager.h>
#include <MrflPrimaryPlane.h>
#include <MrflSpritePlane.h>
#include <MrflOverlayPlane.h>

namespace android {
namespace intel {

static Log& log = Log::getInstance();
MrflDisplayPlaneManager::MrflDisplayPlaneManager()
    : DisplayPlaneManager()
{
    log.v("MrflDisplayPlaneManager");
}

MrflDisplayPlaneManager::~MrflDisplayPlaneManager()
{

}

void MrflDisplayPlaneManager::detect()
{
    log.v("MrflDisplayPlaneManager::detect");

    mSpritePlaneCount = 0;
    mPrimaryPlaneCount = 3;
    mOverlayPlaneCount = 1;
    mTotalPlaneCount = mSpritePlaneCount + mPrimaryPlaneCount + mOverlayPlaneCount;

    // Platform dependent, no sprite plane on Medfield
    mFreeSpritePlanes = 0x0;
    // plane A, B & C
    mFreePrimaryPlanes = 0x7;
    // both overlay A & C
    mFreeOverlayPlanes = 0x1;
}

IDisplayPlane* MrflDisplayPlaneManager::allocPlane(int index, int type)
{
    IDisplayPlane *plane = 0;
    log.v("MrflDisplayPlaneManager::allocPlane index %d, type %d", index, type);

    switch (type) {
    case IDisplayPlane::PLANE_PRIMARY:
        plane = new MrflPrimaryPlane(index, index);
        break;
    case IDisplayPlane::PLANE_SPRITE:
        plane = new MrflSpritePlane(index, index);
        break;
    case IDisplayPlane::PLANE_OVERLAY:
        plane = new MrflOverlayPlane(index, index);
        break;
    default:
        log.e("MrflDisplayPlaneManager::allocPlane: unsupported type %d", type);
    }
    return plane;
}

} // namespace intel
} // namespace android

