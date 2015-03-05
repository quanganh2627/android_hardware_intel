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

import android.util.Log;

import com.intel.internal.telephony.Constants;
import com.intel.internal.telephony.mmgr.MedfieldMmgrClient;

import java.io.UnsupportedEncodingException;

import java.nio.ByteBuffer;

public class MmgrRegisterNameRequest extends MmgrBaseRequest {
    private String clientName = "";

    public MmgrRegisterNameRequest(String clientName) {
        super(MedfieldMmgrClient.SET_NAME);
        this.setClientName(clientName);
    }

    public String getClientName() {
        return this.clientName;
    }

    public void setClientName(String clientName) {
        this.clientName = clientName.substring(0,
                                               Math.min(63, clientName.length()));
    }

    @Override
    public String getName() {
        return "RegisterNameRequest";
    }

    @Override
    protected byte[] getPayload() {
        ByteBuffer ret = ByteBuffer.allocate(64);

        byte[] clientNameBytes = null;

        try {
            clientNameBytes = this.clientName.getBytes("US-ASCII");
        } catch (UnsupportedEncodingException e) {
            Log.e(Constants.LOG_TAG, "Ascii encoding not supported");
        }
        if (clientNameBytes != null) {
            ret.put(clientNameBytes, 0, clientNameBytes.length);
        }
        return ret.array();
    }
}
