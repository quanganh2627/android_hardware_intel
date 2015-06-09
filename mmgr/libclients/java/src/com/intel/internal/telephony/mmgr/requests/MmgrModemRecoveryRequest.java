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

public class MmgrModemRecoveryRequest extends MmgrBaseRequest {
    private String[] causes;

    public MmgrModemRecoveryRequest(String[] causes) {
        super(MedfieldMmgrClient.REQUEST_MODEM_RECOVERY);
        this.causes = causes;
    }

    @Override
    public String getName() {
        return "ModemRecoveryRequest";
    }

    @Override
    protected byte[] getPayload() {
        if (causes == null) {
            return new byte[0];
        } else {
            int size = 4;
            for (int i = 0; i < causes.length; i++) {
                size += 4;
                size += causes[i].length();
            }
            ByteBuffer ret = ByteBuffer.allocate(size);
            ret.putInt(causes.length);
            for (int i = 0; i < causes.length; i++) {
                ret.putInt(causes[i].length());
                ret.put(causes[i].getBytes());
            }
            return ret.array();
        }
    }
}
