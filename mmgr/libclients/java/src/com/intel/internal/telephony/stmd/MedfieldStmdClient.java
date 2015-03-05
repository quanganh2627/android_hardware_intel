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

package com.intel.internal.telephony.stmd;

import java.io.IOException;
import java.io.InputStream;

import com.intel.internal.telephony.Constants;
import com.intel.internal.telephony.MmgrClientException;
import com.intel.internal.telephony.ModemNotification;
import com.intel.internal.telephony.ModemRequestArgs;
import com.intel.internal.telephony.ModemStatus;
import com.intel.internal.telephony.ModemStatusMonitor;
import com.intel.internal.telephony.ModemNotificationArgs;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

public class MedfieldStmdClient implements ModemStatusMonitor, Runnable {
    private final static byte MODEM_DOWN = 0;
    private final static byte MODEM_UP = 1;
    private final static byte PLATFORM_SHUTDOWN = 2;
    private final static byte MODEM_COLD_RESET = 4;

    protected LocalSocket clientSocket = null;
    protected Handler handler = null;
    protected Thread thread = null;
    protected volatile boolean stopRequested = false;
    protected int connectTimeoutMs = 4000;

    public MedfieldStmdClient(Handler handler) {
        this(handler, 4000);
    }

    public MedfieldStmdClient(Handler handler, int connectTimeoutMs) {
        this.connectTimeoutMs = connectTimeoutMs;
        this.setModemStatusHandler(handler);
    }

    protected LocalSocketAddress getSocketAddress() {
        return new LocalSocketAddress("modem-status",
                                      LocalSocketAddress.Namespace.RESERVED);
    }

    public void start(String clientName) {
        this.stopRequested = false;

        this.clientSocket = new LocalSocket();

        this.thread = new Thread(this);
        this.thread.setName("STMD Client for " + clientName);
        this.thread.start();
    }

    public void stop() {
        this.stopRequested = true;
        this.cleanUp();
        try {
            this.thread.join();
        } catch (InterruptedException ex) {
            Log.e(Constants.LOG_TAG, ex.toString());
        }
    }

    public void setModemStatusHandler(Handler handler) {
        if (handler == null) {
            throw new IllegalArgumentException("handler");
        }
        this.handler = handler;
    }

    public void run() {
        byte[] recvBuffer = new byte[1024]; // should be large enough to contain
                                            // response
        InputStream inputStream = null;
        int readCount = 0;

        try {
            this.clientSocket.connect(this.getSocketAddress());
            inputStream = this.clientSocket.getInputStream();
        } catch (Exception ex) {
            Log.e(Constants.LOG_TAG, ex.toString());

            this.handler.obtainMessage(ModemStatusMonitor.MSG_ERROR, ex)
            .sendToTarget();
            this.cleanUp();
            return;
        }

        while (!this.stopRequested) {
            try {
                readCount = inputStream.read(recvBuffer);

                this.handleResponse(recvBuffer, readCount);
            } catch (IOException ex) {
                Log.e(Constants.LOG_TAG, ex.toString());

                this.handler.obtainMessage(ModemStatusMonitor.MSG_ERROR, ex)
                .sendToTarget();
                this.cleanUp();
                return;
            }
        }
    }

    private void handleResponse(byte[] buffer, int length) {
        ModemNotification notification = ModemNotification.NONE;
        ModemStatus status = ModemStatus.NONE;

        for (int i = 0; i < length; i += 4) {
            switch (buffer[i]) {
            case MedfieldStmdClient.MODEM_DOWN:
                Log.i(Constants.LOG_TAG, "Modem status = MODEM_DOWN");
                status = ModemStatus.DOWN;
                break;
            case MedfieldStmdClient.MODEM_UP:
                Log.i(Constants.LOG_TAG, "Modem status = MODEM_UP");
                status = ModemStatus.UP;
                break;

            case MedfieldStmdClient.MODEM_COLD_RESET:
                Log.i(Constants.LOG_TAG,
                      "Modem notification = MODEM_COLD_RESET");
                notification = ModemNotification.COLD_RESET;
                break;

            case MedfieldStmdClient.PLATFORM_SHUTDOWN:
                Log.i(Constants.LOG_TAG,
                      "Modem notification = PLATFORM_SHUTDOWN");
                notification = ModemNotification.PLATFORM_REBOOT;
                break;
            default:
                Log.i(Constants.LOG_TAG, "Unknown data :" + buffer[i]);
            }
            if (status != ModemStatus.NONE) {
                this.handler.obtainMessage(ModemStatusMonitor.MSG_STATUS,
                                           status).sendToTarget();
            }
            if (notification != ModemNotification.NONE) {
                this.handler.obtainMessage(ModemStatusMonitor.MSG_NOTIFICATION,
                                           notification).sendToTarget();
            }
        }
    }

    protected void cleanUp() {
        if (this.clientSocket != null) {
            try {
                this.clientSocket.shutdownInput();
                this.clientSocket.close();
            } catch (IOException ex) {
                Log.e(Constants.LOG_TAG, ex.toString());
            }
            this.clientSocket = null;
        }
    }

    public ModemNotificationArgs buildNotificationArgs() {
        return new ModemNotificationArgs();
    }

    public void replyToNotification(ModemNotificationArgs args) {
        throw new UnsupportedOperationException("Not supported by STMD");
    }

    @Override
    public void sendRequest(ModemRequestArgs args) {
        throw new UnsupportedOperationException("Not supported yet");
    }

    @Override
    public void useModem() {
        throw new UnsupportedOperationException("Not supported by STMD");
    }

    @Override
    public void releaseModem() {
        throw new UnsupportedOperationException("Not supported by STMD");
    }

    @Override
    public void subscribeTo(ModemStatus status, ModemNotification notifications) {
        throw new UnsupportedOperationException("Not supported by STMD");
    }

    @Override
    public boolean handleMessage(Message msg) {
        // Not supported
        return false;
    }

    @Override
    public void restartModem() throws MmgrClientException {
        throw new UnsupportedOperationException("Not supported yet");
    }

    @Override
    public void recoverModem(String[] causes) throws MmgrClientException {
        throw new UnsupportedOperationException("Not supported yet");
    }

    @Override
    public void shutdownModem() throws MmgrClientException {
        throw new UnsupportedOperationException("Not supported yet");
    }

    @Override
    public boolean waitForStatus(ModemStatus status, long timeout)
    throws MmgrClientException {
        throw new UnsupportedOperationException("Not supported yet");
    }
}
