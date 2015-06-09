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

#include "AudioRouteManagerObserver.hpp"
#include <AudioCommsAssert.hpp>

namespace intel_audio
{

AudioRouteManagerObserver::AudioRouteManagerObserver()
{
    AUDIOCOMMS_ASSERT(sem_init(&mSyncSem, 0, 0) == 0, "failed to create semaphore");
}

AudioRouteManagerObserver::~AudioRouteManagerObserver()
{
    AUDIOCOMMS_ASSERT(sem_destroy(&mSyncSem) == 0, "failed to destroy semaphore");
}

void AudioRouteManagerObserver::waitNotification()
{
    AUDIOCOMMS_ASSERT(sem_wait(&mSyncSem) == 0, "failed to wait semaphore");
}

void AudioRouteManagerObserver::notify()
{
    AUDIOCOMMS_ASSERT(sem_post(&mSyncSem) == 0, "failed to post semaphore");
}

} // namespace intel_audio
