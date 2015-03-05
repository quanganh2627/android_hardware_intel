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
#ifndef __INTEL_EXTERNAL_DISPLAY_MONITOR_H__
#define __INTEL_EXTERNAL_DISPLAY_MONITOR_H__

#include <utils/threads.h>

#include "display/IExtendDisplayListener.h"
#include "display/IMultiDisplayComposer.h"
#include "display/MultiDisplayClient.h"
#include "display/MultiDisplayType.h"
#include "display/MultiDisplayService.h"

class IntelHWComposer;

class IntelExternalDisplayMonitor :
    public BnExtendDisplayListener,
    public android::IBinder::DeathRecipient,
    protected android::Thread
{
public:
    enum {
        MSG_TYPE_MDS_UNDEFINED = -1,
        MSG_TYPE_UEVENT = 0,
        MSG_TYPE_MDS,
        MSG_TYPE_MDS_HOTPLUG_IN,
        MSG_TYPE_MDS_HOTPLUG_OUT,
        MSG_TYPE_MDS_TIMING_DYNAMIC_SETTING,
    };

    enum {
        UEVENT_MSG_LEN = 4096,
    };

public:
    IntelExternalDisplayMonitor(IntelHWComposer *hwc);
    virtual ~IntelExternalDisplayMonitor();
    void initialize();
public:
    // onMdsMessage() interface
    int onMdsMessage(int msg, void *data, int size);
public:
    int getDisplayMode();
    bool isVideoPlaying();
    bool isVideoPrepared();
    bool isOverlayOff();
    bool isHdmiConnected();
    bool notifyWidi(bool);
    bool notifyMipi(bool);
    bool getVideoInfo(int *displayW, int *displayH, int *fps, int *isinterlace);
private:
    //DeathReipient interface
    virtual void binderDied(const android::wp<android::IBinder>& who);
private:
    virtual bool threadLoop();
    virtual android::status_t readyToRun();
    virtual void onFirstRef();

    inline bool checkMode(int value, int bit) {
        if ((value & bit) == bit)
           return true;
        return false;
    }
private:
    MultiDisplayClient* mMDClient;
    android::Mutex mLock;
    android::Condition mModeChanged;
    int mActiveDisplayMode;
    int mVideoPrepareState;
    bool mWidiOn;
    bool mInitialized;
    IntelHWComposer *mComposer;
    char mUeventMessage[UEVENT_MSG_LEN];
    int mUeventFd;
    int mLastMsg;
}; // IntelExternalDisplayMonitor

#endif // __INTEL_EXTERNAL_DISPLAY_MONITOR_H__

