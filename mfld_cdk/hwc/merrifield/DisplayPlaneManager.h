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
#ifndef DISPLAYPLANEMANAGER_H_
#define DISPLAYPLANEMANAGER_H_

#include <Dump.h>
#include <IDisplayPlane.h>
#include <utils/Vector.h>

namespace android {
namespace intel {

class DisplayPlaneManager {
    enum {
        PLANE_ON_RECLAIMED_LIST = 1,
        PLANE_ON_FREE_LIST,
    };
public:
    DisplayPlaneManager();
    virtual ~DisplayPlaneManager();

    bool initCheck() const { return mInitialized; }
    bool initialize();

    // plane allocation & free
    IDisplayPlane* getSpritePlane();
    IDisplayPlane* getPrimaryPlane(int pipe);
    IDisplayPlane* getOverlayPlane();
    void putSpritePlane(IDisplayPlane& plane);
    void putOverlayPlane(IDisplayPlane& plane);

    IDisplayPlane* getDummySpritePlane();
    IDisplayPlane* getDummyPrimaryPlane();
    IDisplayPlane* getDummyOverlayPlane();

    IDisplayPlane* getSpritePlane(int& where);
    IDisplayPlane* getOverlayPlane(int& where);
    void putSpritePlane(IDisplayPlane& plane, int where);
    void putOverlayPlane(IDisplayPlane& plane, int where);

    bool hasFreeSprites();
    bool hasFreeOverlays();
    int getFreeSpriteCount() const;
    int getFreeOverlayCount() const;

    bool hasReclaimedOverlays();
    bool primaryAvailable(int index);

    void reclaimPlane(IDisplayPlane& plane);
    void disableReclaimedPlanes();

    // dump interface
    void dump(Dump& d);
protected:
    int getPlane(uint32_t& mask);
    int getPlane(uint32_t& mask, int index);
    void putPlane(int index, uint32_t& mask);

    // sub-classes need implement follow functions
    virtual void detect();
    virtual IDisplayPlane* allocPlane(int index, int type);
protected:
    int mSpritePlaneCount;
    int mPrimaryPlaneCount;
    int mOverlayPlaneCount;
    int mTotalPlaneCount;
    // maximum plane count for each pipe
    int mMaxPlaneCount;
    Vector<IDisplayPlane*> mSpritePlanes;
    Vector<IDisplayPlane*> mPrimaryPlanes;
    Vector<IDisplayPlane*> mOverlayPlanes;

    // dummy planes
    IDisplayPlane *mDummySpritePlane;
    IDisplayPlane *mDummyPrimaryPlane;
    IDisplayPlane *mDummyOverlayPlane;

    // Bitmap of free planes. Bit0 - plane A, bit 1 - plane B, etc.
    uint32_t mFreeSpritePlanes;
    uint32_t mFreePrimaryPlanes;
    uint32_t mFreeOverlayPlanes;
    uint32_t mReclaimedSpritePlanes;
    uint32_t mReclaimedPrimaryPlanes;
    uint32_t mReclaimedOverlayPlanes;

    bool mInitialized;
};

} // namespace intel
} // namespace android

#endif /* DISPLAYPLANEMANAGER_H_ */
