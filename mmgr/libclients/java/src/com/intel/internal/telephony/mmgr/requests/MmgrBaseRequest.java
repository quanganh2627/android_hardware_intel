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

import com.intel.internal.telephony.ModemRequestArgs;

import java.nio.ByteBuffer;

public abstract class MmgrBaseRequest extends ModemRequestArgs {
    protected int requestId = -1;

    protected MmgrBaseRequest(int requestId) {
        if (requestId < 0) {
            throw new IllegalArgumentException("requestId");
        }
        this.requestId = requestId;
    }

    protected abstract byte[] getPayload();

    @Override
    public byte[] getFrame() {
        byte[] payload = this.getPayload();
        int payloadSize = (payload != null ? payload.length : 0);

        /*
         * 4 bytes request ID 4 bytes timestamp 4 bytes payload size N payload
         * size
         */
        ByteBuffer ret = ByteBuffer.allocate(4 + 4 + 4 + payloadSize);

        if (ret != null) {
            ret.putInt(this.requestId);
            ret.putInt(super.getTimestamp());
            ret.putInt(payloadSize);
            if (payload != null) {
                ret.put(payload);
            }
            return ret.array();
        }
        return null;
    }
}
