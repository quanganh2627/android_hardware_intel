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

package com.intel.internal.telephony.mmgr.responses;

import android.util.Log;

import com.intel.internal.telephony.Constants;
import com.intel.internal.telephony.mmgr.MedfieldMmgrClient;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

public abstract class MmgrBaseResponse {
    protected int responseId = -1;
    protected int timestamp = -1;
    protected byte[] payload = new byte[0];

    /*
     * 4 bytes request ID 4 bytes timestamp 4 bytes payload size N payload size
     */
    public static final int HEADER_SIZE = 4 + 4 + 4;

    protected MmgrBaseResponse() {
    }

    protected MmgrBaseResponse(int responseId, int timestamp) {
        this.setResponseId(responseId);
        this.setTimestamp(timestamp);
    }

    public int getResponseId() {
        return this.responseId;
    }

    public void setResponseId(int responseId) {
        if (responseId < 0) {
            throw new IllegalArgumentException("responseId");
        }
        this.responseId = responseId;
    }

    public int getTimestamp() {
        return this.timestamp;
    }

    public void setTimestamp(int timestamp) {
        if (timestamp < 0) {
            throw new IllegalArgumentException("timestamp");
        }
        this.timestamp = timestamp;
    }

    public byte[] getPayload() {
        return this.payload;
    }

    public void setPayload(byte[] payload) {
        this.payload = payload;
    }

    public void parseRawFrame(byte[] fullFrame, int offset, int size) {
        if (fullFrame == null || fullFrame.length < (offset + size)
            || size < MmgrBaseResponse.HEADER_SIZE) {
            throw new IllegalArgumentException("fullFrame");
        }
        ByteBuffer buffer = ByteBuffer.wrap(fullFrame, offset, size);
        this.setResponseId(buffer.getInt());
        this.setTimestamp(buffer.getInt());
        int payloadSize = buffer.getInt();

        this.payload = new byte[Math.min(payloadSize, buffer.remaining())];
        buffer.get(this.payload, 0, payloadSize);
    }

    public byte[] getRawFrame() {
        byte[] payload = this.getPayload();
        int payloadSize = (payload != null ? payload.length : 0);

        ByteBuffer ret = ByteBuffer.allocate(MmgrBaseResponse.HEADER_SIZE
                                             + payloadSize);

        if (ret != null) {
            ret.putInt(this.responseId);
            ret.putInt(this.getTimestamp());
            ret.putInt(payloadSize);
            if (payload != null) {
                ret.put(payload);
            }
            return ret.array();
        }
        return null;
    }

    public int getRawFrameLength() {
        byte[] frame = this.getRawFrame();

        return frame == null ? 0 : frame.length;
    }

    public static MmgrBaseResponse parseResponse(byte[] data, int offset,
                                                 int size) {
        MmgrBaseResponse header = new MmgrBaseResponse() {
        };

        /* if length not long enough to contain header */
        if (size < MmgrBaseResponse.HEADER_SIZE) {
            return null;
        }

        header.parseRawFrame(data, offset, size);

        /*
         * Handle here any response ID and create appropriate response object.
         * If later implementation need to handle a specific payload in a
         * response object, this object has to be defined in a class that parses
         * the payload
         */
        switch (header.getResponseId()) {
        case MedfieldMmgrClient.NOTIFY_ACK:
            header = new MmgrAckResponse(header.getResponseId(),
                                         header.getTimestamp(), true);
            break;
        case MedfieldMmgrClient.NOTIFY_NACK:
            header = new MmgrAckResponse(header.getResponseId(),
                                         header.getTimestamp(), false);
            break;
        }

        return header;
    }

    public static List<MmgrBaseResponse> parseResponses(byte[] data,
                                                        int offset, int size) {
        ArrayList<MmgrBaseResponse> ret = new ArrayList<MmgrBaseResponse>();

        MmgrBaseResponse parsedResponse = null;
        int currentOffset = offset;
        int currentLength = size;

        do {
            parsedResponse = MmgrBaseResponse.parseResponse(data,
                                                            currentOffset, currentLength);

            if (parsedResponse != null) {
                Log.d(Constants.LOG_TAG, "Parsed response ID : "
                      + parsedResponse.getResponseId());
                ret.add(parsedResponse);
                int currentFrameLength = parsedResponse.getRawFrameLength();
                currentOffset += currentFrameLength;
                currentLength -= currentFrameLength;
            }
        } while (parsedResponse != null && currentOffset < size);

        return ret;
    }
}
