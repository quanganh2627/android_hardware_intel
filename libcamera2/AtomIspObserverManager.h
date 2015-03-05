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

#ifndef ANDROID_LIBCAMERA_ISP_OBSERVER_H
#define ANDROID_LIBCAMERA_ISP_OBSERVER_H

#include <utils/threads.h>
#include <time.h>
#include "IAtomIspObserver.h"

namespace android {

class IObserverSubject
{
public:
    virtual ~IObserverSubject() { };
    virtual const char* getName() = 0;
    virtual status_t observe(IAtomIspObserver::Message *msg) = 0;
};

/**
 * \class AtomIspObserverManager
 *
 * AtomIspObserverManager is a helper class for AtomISP to generalize the
 * way to define operations for polling the device events asynchronously
 * and passing information to multiple observers.
 *
 * AtomIspObserverManager provides a simple interface to attach and detach
 * observers derived from IAtomIspObserver. For each ObserverOperation a
 * new ObserverThread is created, following a generic loop of:
 *
 * IObserverSubject::observer(ObserverOperation, Message)
 * foreach (IAtomIspObserver)
 *      IAtomIspObserver::atomIspNotify(Message)
 */
class AtomIspObserverManager {
public:
    AtomIspObserverManager() { };
    void setState(ObserverState state, IObserverSubject *s, bool synchronous = false);
    status_t attachObserver(IAtomIspObserver *observer, IObserverSubject *s);
    status_t detachObserver(IAtomIspObserver *observer, IObserverSubject *s);
    // private types
private:
    class ObserverThread:public Thread {
        public:
            ObserverThread(IObserverSubject *f);
            status_t attach(IAtomIspObserver *observer);
            ObserverState detach(IAtomIspObserver *observer);

            status_t  setState(ObserverState state, bool sync = false);

            virtual bool threadLoop();

        private:
            enum MessageId {
                MESSAGE_ID_ATTACH,
                MESSAGE_ID_DETACH,
                MESSAGE_ID_SET_STATE,

                MESSAGE_ID_MAX
            };

            struct MessageObserver {
                IAtomIspObserver *interface;
            };

            struct MessageState {
                ObserverState value;
                bool synchronous;
            };

            union MessageData {
                // MESSAGE_ID_ATTACH
                // MESSAGE_ID_DETACH
                MessageObserver observer;
                // MESSAGE_ID_SET_STATE
                MessageState    state;
            };

            struct Message {
                MessageId id;
                MessageData data;
            };

        private:
            status_t waitForAndExecuteMessage();
            status_t notifyObservers(IAtomIspObserver::Message *msg);
            status_t handleMessageAttach(MessageObserver &msg);
            status_t handleMessageDetach(MessageObserver &msg);
            status_t handleMessageSetState(MessageState &msg);

        private:
            List< IAtomIspObserver* > mObservers;
            ObserverState   mState;
            IObserverSubject *mSubject;
            MessageQueue<Message, MessageId> mMessageQueue;
    };

private:
    typedef key_value_pair_t<IObserverSubject*, sp<ObserverThread> > observer_pair_t;
    typedef Vector<observer_pair_t> ObserverVector;
    ObserverVector mObserverThreads;
    mutable Mutex mLock;

    /* PRETTY LOGGING SUPPORT */
    static const char *State2String[];
private:
    static const char *getLogString(const char *array[], unsigned int index);
};
}; // namespace android

#endif // ANDROID_LIBCAMERA_ISP_OBSERVER_H
