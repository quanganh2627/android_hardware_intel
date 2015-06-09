/* Android Modem Status Client API
 *
 * Copyright (C) Intel 2012
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
 *
 */

package com.intel.internal.telephony.mmgr.requests;

import com.intel.internal.telephony.mmgr.MedfieldMmgrClient;

import java.nio.ByteBuffer;

public class MmgrRegisterEventsRequest extends MmgrBaseRequest {
    private int subscribedEvents = 0;

    public MmgrRegisterEventsRequest(int subscribedEvents) {
        super(MedfieldMmgrClient.SET_EVENTS);
        this.setSubscribedEvents(subscribedEvents);
    }

    public int getSubscribedEvents() {
        return this.subscribedEvents;
    }

    public void setSubscribedEvents(int subscribedEvents) {
        this.subscribedEvents = subscribedEvents;
    }

    @Override
    public String getName() {
        return "RegisterEventsRequest";
    }

    @Override
    protected byte[] getPayload() {
        ByteBuffer ret = ByteBuffer.allocate(4);

        ret.putInt(this.subscribedEvents);

        return ret.array();
    }
}
