/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
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
#define LOG_TAG "Camera_ISPObserver"

#include <time.h>
#include "LogHelper.h"
#include "AtomIspObserverManager.h"
#include "IAtomIspObserver.h"
#include <utils/String8.h>

namespace android {

const char *AtomIspObserverManager::State2String[] = { "STATE_STOPPED",
                                                       "STATE_PAUSED",
                                                       "STATE_RUNNING"};
/**
 * Attach observer interface to observe certain operation
 *
 * Attach is done using ObserverThread::attach() when
 * ObserverThread for distinct ObserverOperation exists. For new operations,
 * new ObserverThread gets created and started into paused state.
 *
 * @param observer interface pointer to attach
 * @param op_f identifier for operation passed to IObserverSubject::observe()
 */
status_t
AtomIspObserverManager::attachObserver(IAtomIspObserver *observer, IObserverSubject *s)
{
    LOG1("@%s", __FUNCTION__);
    Mutex::Autolock lock(mLock);
    if (s == NULL)
        return BAD_VALUE;

    ObserverVector::iterator it = mObserverThreads.begin();
    for (;it != mObserverThreads.end(); ++it)
        if (it->key == s) {
            (it->value)->attach(observer);
            return NO_ERROR;
        }
    sp<ObserverThread> newThread = new ObserverThread(s);
    newThread->attach(observer);
    String8 str("CamHAL_");
    str += s->getName();
    newThread->run(str);
    mObserverThreads.push(observer_pair_t(s, newThread));
    return NO_ERROR;
}

/**
 * Detach observer interface
 *
 * Detach is done using ObserverThread::detach(). When last
 * observer gets detached, ObserverThread changes its state
 * to OBSERVER_STATE_STOPPED.
 *
 * @param observer interface pointer to detach
 * @param op_f identifier for operation passed to IObserverSubject::observe()
 */
status_t
AtomIspObserverManager::detachObserver(IAtomIspObserver *observer, IObserverSubject *s)
{
    LOG1("@%s", __FUNCTION__);
    Mutex::Autolock lock(mLock);
    if (s == NULL)
        return BAD_VALUE;
    ObserverVector::iterator it = mObserverThreads.begin();
    for (;it != mObserverThreads.end(); ++it) {
        if (it->key == s) {
            ObserverState state = it->value->detach(observer);
            if (state == OBSERVER_STATE_STOPPED) {
                LOG2("last observer, waiting thread to stop");
                it->value->requestExitAndWait();
                it->value.clear();
                mObserverThreads.erase(it);
            }
            return NO_ERROR;
        }
    }
    return BAD_VALUE;
}

/**
 * Change the observer state
 *
 * Control to states are meant to be used only by IObserverSubject.
 *
 * Optionally by using (bool)synchronous,
 * pause -> stopped and pause -> running sequences can be
 * used for plain synchronisation with the ObserverThread.
 *
 * @param observer interface pointer to detach
 * @param op_f identifier for operation passed to IObserverSubject::observe()
 * @param synchronous whether to block for state to apply
 */
void
AtomIspObserverManager::setState(ObserverState state, IObserverSubject *s, bool synchronous)
{
    LOG1("@%s", __FUNCTION__);
    Mutex::Autolock lock(mLock);
    ObserverVector::iterator it = mObserverThreads.begin();
    for (;it != mObserverThreads.end(); ++it) {
        if (s == NULL || it->key == s) {
            (it->value)->setState(state, synchronous);
        }
    }
}

/**
 * Helper method to retrieve the string representation of a class enum
 * It performs a boundary check
 *
 * @param array pointer to the static table with the strings
 * @param index enum to convert to string
 *
 * @return string describing enum
 */
const char *AtomIspObserverManager
::getLogString(const char *array[], unsigned int index)
{
    if (index > sizeof(*array))
        return "-1";
    return array[index];
}

status_t
AtomIspObserverManager::ObserverThread::setState(ObserverState state, bool sync)
{
    LOG1("@%s:%s", mSubject->getName(), __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_SET_STATE;
    msg.data.state.value = state;
    msg.data.state.synchronous = sync;
    if (state == OBSERVER_STATE_PAUSED && sync) {
        LOG1("pausing %s...", mSubject->getName());
    }
    return mMessageQueue.send(&msg, (sync)?MESSAGE_ID_SET_STATE:(MessageId)-1);
}

status_t
AtomIspObserverManager::ObserverThread::notifyObservers(IAtomIspObserver::Message *msg)
{
    LOG2("@%s:%s", mSubject->getName(), __FUNCTION__);
    bool ret = false;
    List<IAtomIspObserver*>::iterator it = mObservers.begin();
    for (;it != mObservers.end(); ++it)
        ret |= (*it)->atomIspNotify((IAtomIspObserver::Message*)msg, mState);
    return ret;
}

/**
 * Main thread loop of ObserverThread
 *
 * Implements generalized sequence over IObserverSubject's.
 *
 * 1. handle internal messages (attach/detach/setState) until queue
 *    gets empty and we are requested to exit idle (paused) state.
 * 2. call IObserverSubject::observer() with operation identifier and
 *    generic message to be filled by the operation.
 * 3. notify each attached observer with the message and observer state
 *
 * Notify observers also in state changes, with NULL message.
 */
bool
AtomIspObserverManager::ObserverThread::threadLoop()
{
    LOG1("@%s:%s", mSubject->getName(), __FUNCTION__);
    status_t ret = NO_ERROR;
    IAtomIspObserver::Message msg;

    while (mState != OBSERVER_STATE_STOPPED) {
        // prioritise message prosessing, since the sequences are well known
        // and minimal during the running state
        while (!mMessageQueue.isEmpty() || mState == OBSERVER_STATE_PAUSED) {
            ret = waitForAndExecuteMessage();
            if (ret != NO_ERROR) {
                mState = OBSERVER_STATE_STOPPED;
                notifyObservers(NULL);
            }
            LOG2("@%s mState:%s", mSubject->getName(),
                AtomIspObserverManager::getLogString(State2String,mState));
        }
        if (mState == OBSERVER_STATE_STOPPED)
            break;
        ret = mSubject->observe(&msg);

        if (notifyObservers(&msg)) {
            LOG1("%s:Paused by notify request!", mSubject->getName());
            mState = OBSERVER_STATE_PAUSED;
            notifyObservers(NULL);
        }

        if (ret != NO_ERROR) {
            LOG1("%s:Paused by ERROR!", mSubject->getName());
            mState = OBSERVER_STATE_PAUSED;
            ret = NO_ERROR;
            notifyObservers(NULL);
        }
    }

    LOG1("leaving %s threadLoop", mSubject->getName());

    return false;
}

status_t
AtomIspObserverManager::ObserverThread::waitForAndExecuteMessage()
{
    LOG2("@%s:%s", __FUNCTION__, mSubject->getName());
    status_t status = NO_ERROR;
    Message msg;
    mMessageQueue.receive(&msg);

    switch (msg.id) {

        case MESSAGE_ID_ATTACH:
            status = handleMessageAttach(msg.data.observer);
            break;

        case MESSAGE_ID_DETACH:
            status = handleMessageDetach(msg.data.observer);
            break;

        case MESSAGE_ID_SET_STATE:
            status = handleMessageSetState(msg.data.state);
            break;

        default:
            ALOGE("Invalid message");
            status = BAD_VALUE;
            break;
    };

    if (status < NO_ERROR)
        ALOGE("%s: error handling message %d", mSubject->getName(), (int) msg.id);

    return status;
}

status_t
AtomIspObserverManager::ObserverThread::attach(IAtomIspObserver *observer)
{
    LOG1("@%s:%s", mSubject->getName(), __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_ATTACH;
    msg.data.observer.interface = observer;
    return mMessageQueue.send(&msg);
}

ObserverState
AtomIspObserverManager::ObserverThread::detach(IAtomIspObserver *observer)
{
    LOG1("@%s:%s", mSubject->getName(), __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_DETACH;
    msg.data.observer.interface = observer;
    return (ObserverState) mMessageQueue.send(&msg, MESSAGE_ID_DETACH);
}

status_t
AtomIspObserverManager::ObserverThread::handleMessageAttach(MessageObserver &msg)
{
    LOG1("@%s:%s", mSubject->getName(), __FUNCTION__);

    List<IAtomIspObserver*>::iterator it = mObservers.begin();
    for (;it != mObservers.end(); ++it)
        if (*it == msg.interface)
            return ALREADY_EXISTS;

    mObservers.push_back(msg.interface);

    return NO_ERROR;
}

status_t
AtomIspObserverManager::ObserverThread::handleMessageDetach(MessageObserver &msg)
{
    LOG1("@%s:%s", mSubject->getName(), __FUNCTION__);
    status_t status = BAD_VALUE;

    List<IAtomIspObserver*>::iterator it = mObservers.begin();
    for (;it != mObservers.end(); ++it)
        if (*it == msg.interface) {
            mObservers.erase(it);
            if (mObservers.empty()) {
                LOG1("%s: last observer removed, stopping", mSubject->getName());
                mState = OBSERVER_STATE_STOPPED;
            }
            // Returning ObserverState cast to positive status_t
            status = static_cast<status_t>(mState);
            break;
        }

    mMessageQueue.reply(MESSAGE_ID_DETACH, status);
    return NO_ERROR;
}

status_t
AtomIspObserverManager::ObserverThread::handleMessageSetState(MessageState &msg)
{
    LOG1("@%s:%s", mSubject->getName(), __FUNCTION__);
    bool notifyStateChange = (mState != msg.value);
    mState = msg.value;
    if (notifyStateChange)
        notifyObservers(NULL);
    if (msg.synchronous)
        mMessageQueue.reply(MESSAGE_ID_SET_STATE, NO_ERROR);
    return NO_ERROR;
}

AtomIspObserverManager::ObserverThread::ObserverThread(IObserverSubject *s):
    Thread(false)
   ,mState(OBSERVER_STATE_PAUSED)
   ,mSubject(s)
   ,mMessageQueue(s->getName(), (int) MESSAGE_ID_MAX)
{

}

} // namespace android
