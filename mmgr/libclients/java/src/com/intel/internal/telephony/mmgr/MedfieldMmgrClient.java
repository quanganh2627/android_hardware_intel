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

package com.intel.internal.telephony.mmgr;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import com.intel.internal.telephony.Constants;
import com.intel.internal.telephony.MmgrClientException;
import com.intel.internal.telephony.ModemNotification;
import com.intel.internal.telephony.ModemRequestArgs;
import com.intel.internal.telephony.ModemStatus;
import com.intel.internal.telephony.ModemStatusMonitor;
import com.intel.internal.telephony.ModemNotificationArgs;
import com.intel.internal.telephony.mmgr.requests.MmgrModemColdResetAckRequest;
import com.intel.internal.telephony.mmgr.requests.MmgrModemLockRequest;
import com.intel.internal.telephony.mmgr.requests.MmgrModemRecoveryRequest;
import com.intel.internal.telephony.mmgr.requests.MmgrModemReleaseRequest;
import com.intel.internal.telephony.mmgr.requests.MmgrModemRestartRequest;
import com.intel.internal.telephony.mmgr.requests.MmgrModemShutdownAckRequest;
import com.intel.internal.telephony.mmgr.requests.MmgrModemShutdownRequest;
import com.intel.internal.telephony.mmgr.requests.MmgrRegisterEventsRequest;
import com.intel.internal.telephony.mmgr.requests.MmgrRegisterNameRequest;
import com.intel.internal.telephony.mmgr.responses.MmgrBaseResponse;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class MedfieldMmgrClient implements ModemStatusMonitor, Runnable {
    /* Clients -> MMGR */
    public final static int SET_NAME = 0;
    public final static int SET_EVENTS = 1;
    public final static int RESOURCE_ACQUIRE = 2;
    public final static int RESOURCE_RELEASE = 3;
    public final static int REQUEST_MODEM_RECOVERY = 4;
    public final static int REQUEST_MODEM_RESTART = 5;
    public final static int REQUEST_FORCE_MODEM_SHUTDOWN = 6;

    public final static int ACK_MODEM_COLD_RESET = 7;
    public final static int ACK_MODEM_SHUTDOWN = 8;

    /* MMGR -> Clients */
    public final static int STATUS_MODEM_DOWN = 0;
    public final static int STATUS_MODEM_UP = 1;
    public final static int STATUS_MODEM_OUT_OF_SERVICE = 2;

    public final static int NOTIFY_MODEM_COLD_RESET = 3;
    public final static int NOTIFY_MODEM_SHUTDOWN = 4;
    public final static int NOTIFY_PLATFORM_REBOOT = 5;
    public final static int NOTIFY_MODEM_CORE_DUMP = 6;

    public final static int NOTIFY_ACK = 7;
    public final static int NOTIFY_NACK = 8;

    public LocalSocket clientSocket = null;
    public Handler handler = null;
    public Thread thread = null;
    public volatile boolean stopRequested = false;
    public int connectTimeoutMs = 2000;
    private int subscribedEvents = 0;

    final Lock ackLock = new ReentrantLock();
    final Condition ackSignal = ackLock.newCondition();
    volatile boolean ackSignaled = false;
    volatile boolean nackSignaled = false;

    final Lock statusLock = new ReentrantLock();
    final Condition statusSignal = statusLock.newCondition();

    private ModemStatus currentStatus = ModemStatus.NONE;
    private ModemStatus waitedStatus = ModemStatus.NONE;

    public MedfieldMmgrClient(Handler handler) {
        this(handler, 4000);
    }

    public MedfieldMmgrClient(Handler handler, int connectTimeoutMs) {
        this.connectTimeoutMs = connectTimeoutMs;
        this.setModemStatusHandler(handler);
    }

    @Override
    public void setModemStatusHandler(Handler handler) {
        if (handler == null) {
            throw new IllegalArgumentException("handler");
        }
        this.handler = handler;
    }

    @Override
    public void start(String clientName) throws MmgrClientException {
        Log.d(Constants.LOG_TAG, "Starting client...");

        if (this.thread != null && this.thread.isAlive()) {
            Log.d(Constants.LOG_TAG, "Client already started.");
            return;
        }

        this.stopRequested = false;

        try {
            this.connectSocket(clientName);
        } catch (MmgrClientException ex) {
            Log.e(Constants.LOG_TAG, ex.toString());

            this.handler.obtainMessage(ModemStatusMonitor.MSG_ERROR, ex)
            .sendToTarget();
            this.cleanUp();
            throw ex;
        }

        this.thread = new Thread(this);
        this.thread.setName("MMGR Client for " + clientName);
        this.thread.start();

        this.sendRequest(new MmgrRegisterNameRequest(clientName));

        if (!this.waitForAck(this.connectTimeoutMs)) {
            this.stop();
            throw new MmgrClientException("MMGR name subscribtion failed.");
        }

        this.sendRequest(new MmgrRegisterEventsRequest(this.subscribedEvents));

        if (!this.waitForAck(this.connectTimeoutMs)) {
            this.stop();
            throw new MmgrClientException("MMGR events subscribtion failed.");
        }
        Log.d(Constants.LOG_TAG, "Client ready.");
    }

    private void connectSocket(String clientName) throws MmgrClientException {
        this.clientSocket = new LocalSocket();

        Log.d(Constants.LOG_TAG, "Connecting to service...");
        try {
            this.clientSocket.connect(this.getSocketAddress());
        } catch (IOException ex) {
            throw new MmgrClientException("Connection to MMGR socket failed.",
                                          ex);
        }
        Log.d(Constants.LOG_TAG, "Connected to service.");
    }

    private boolean waitForAck(long timeout) {
        boolean ret = false;

        this.ackLock.lock();
        try {
            if (!this.ackSignaled) {
                Log.d(Constants.LOG_TAG, "Waiting for ACK");
                ret = this.ackSignal.await(timeout, TimeUnit.MILLISECONDS);
                if (ret) {
                    if (this.nackSignaled) {
                        Log.d(Constants.LOG_TAG, "NACK signaled");
                        ret = false;
                    } else {
                        Log.d(Constants.LOG_TAG, "ACK signaled");
                    }
                } else {
                    Log.d(Constants.LOG_TAG, "ACK timeout");
                }
            } else {
                if (this.nackSignaled) {
                    Log.d(Constants.LOG_TAG, "NACK already signaled");
                } else {
                    Log.d(Constants.LOG_TAG, "ACK already signaled");
                    ret = true;
                }
                this.ackSignaled = false;
                this.nackSignaled = false;
            }
        } catch (InterruptedException ex) {
            Log.d(Constants.LOG_TAG, ex.toString());
        } finally {
            this.ackLock.unlock();
        }
        return ret;
    }

    @Override
    public boolean waitForStatus(ModemStatus status, long timeout) {
        boolean ret = false;

        this.statusLock.lock();

        this.waitedStatus = status;

        try {
            if (status != this.currentStatus) {
                Log.d(Constants.LOG_TAG, "Waiting for status");
                ret = this.statusSignal.await(timeout, TimeUnit.MILLISECONDS);
                if (ret) {
                    Log.d(Constants.LOG_TAG, "Status signaled");
                } else {
                    Log.d(Constants.LOG_TAG, "Status timeout");
                }
            } else {
                ret = true;
                Log.d(Constants.LOG_TAG, "Status already signaled");
            }
        } catch (InterruptedException ex) {
            Log.d(Constants.LOG_TAG, ex.toString());
        } finally {
            this.statusLock.unlock();
        }
        return ret;
    }

    private void signalAck() {
        this.ackLock.lock();
        try {
            Log.d(Constants.LOG_TAG, "Signaling ACK");
            this.nackSignaled = false;
            this.ackSignal.signal();
            this.ackSignaled = true;
        } finally {
            this.ackLock.unlock();
        }
    }

    private void signalNack() {
        this.ackLock.lock();
        try {
            Log.d(Constants.LOG_TAG, "Signaling NACK");
            this.nackSignaled = true;
            this.ackSignal.signal();
        } finally {
            this.ackLock.unlock();
        }
    }

    private void signalStatus() {
        this.statusLock.lock();
        try {
            Log.d(Constants.LOG_TAG, "Signaling status");
            this.statusSignal.signal();
        } finally {
            this.statusLock.unlock();
        }
    }

    protected LocalSocketAddress getSocketAddress() {
        return new LocalSocketAddress("mmgr",
                                      LocalSocketAddress.Namespace.RESERVED);
    }

    public void run() {
        Log.d(Constants.LOG_TAG, "MMGR client thread started");

        byte[] recvBuffer = new byte[1024]; // should be large enough to contain
                                            // response
        InputStream inputStream = null;
        int readCount = 0;

        try {
            inputStream = this.clientSocket.getInputStream();
            Log.d(Constants.LOG_TAG, "Socket output stream open.");
        } catch (IOException ex) {
            Log.e(Constants.LOG_TAG, ex.toString());

            this.handler.obtainMessage(ModemStatusMonitor.MSG_ERROR, ex)
            .sendToTarget();
            this.cleanUp();
            return;
        }

        while (!this.stopRequested) {
            try {
                readCount = inputStream.read(recvBuffer, 0, recvBuffer.length);
                Log.d(Constants.LOG_TAG, String.format(
                          "Received %d bytes from service.", readCount));

                if (readCount > 0) {
                    this.handleResponse(recvBuffer, 0, readCount);
                } else {
                    return;
                }
            } catch (IOException ex) {
                Log.e(Constants.LOG_TAG, ex.toString());

                this.handler.obtainMessage(ModemStatusMonitor.MSG_ERROR, ex)
                .sendToTarget();
                this.cleanUp();
                return;
            }
        }
    }

    private void handleResponse(byte[] buffer, int offset, int length) {
        ModemNotification notification = ModemNotification.NONE;
        ModemStatus status = ModemStatus.NONE;

        /*
         * Uncomment this for debugging purpose only
         *
         * StringBuilder sb = new StringBuilder(); for(int i = 0; i < length;
         * i++) { sb.append(String.format("%02X ", buffer[i])); }
         * Log.v(Constants.LOG_TAG, "Received " + sb.toString());
         */

        List<MmgrBaseResponse> parsedResponses = MmgrBaseResponse
                                                 .parseResponses(buffer, offset, length);

        for (int i = 0; i < parsedResponses.size(); i++) {
            switch (parsedResponses.get(i).getResponseId()) {
            case MedfieldMmgrClient.STATUS_MODEM_DOWN:
                Log.i(Constants.LOG_TAG, "Modem status = MODEM_DOWN");
                status = ModemStatus.DOWN;
                break;

            case MedfieldMmgrClient.STATUS_MODEM_UP:
                Log.i(Constants.LOG_TAG, "Modem status = MODEM_UP");
                status = ModemStatus.UP;
                break;

            case MedfieldMmgrClient.STATUS_MODEM_OUT_OF_SERVICE:
                Log.i(Constants.LOG_TAG, "Modem status = MODEM_DEAD");
                status = ModemStatus.DEAD;
                break;

            case MedfieldMmgrClient.NOTIFY_MODEM_COLD_RESET:
                Log.i(Constants.LOG_TAG,
                      "Modem notification = NOTIFY_MODEM_COLD_RESET");
                notification = ModemNotification.COLD_RESET;
                break;

            case MedfieldMmgrClient.NOTIFY_MODEM_CORE_DUMP:
                Log.i(Constants.LOG_TAG,
                      "Modem notification = NOTIFY_MODEM_CORE_DUMP");
                notification = ModemNotification.CORE_DUMP;
                break;

            case MedfieldMmgrClient.NOTIFY_MODEM_SHUTDOWN:
                Log.i(Constants.LOG_TAG,
                      "Modem notification = NOTIFY_MODEM_SHUTDOWN");
                notification = ModemNotification.SHUTDOWN;
                break;

            case MedfieldMmgrClient.NOTIFY_PLATFORM_REBOOT:
                Log.i(Constants.LOG_TAG,
                      "Modem notification = NOTIFY_PLATFORM_REBOOT");
                notification = ModemNotification.PLATFORM_REBOOT;
                break;

            case MedfieldMmgrClient.NOTIFY_ACK:
                Log.d(Constants.LOG_TAG, "Received ACK");
                this.signalAck();
                break;

            case MedfieldMmgrClient.NOTIFY_NACK:
                Log.d(Constants.LOG_TAG, "Received NACK");
                this.signalNack();
                break;

            default:
                Log.w(Constants.LOG_TAG, "Unknown response ID :"
                      + parsedResponses.get(i).getResponseId());
            }
        }

        this.currentStatus = status;

        if (status == this.waitedStatus) {
            this.signalStatus();
        }
        if (status != ModemStatus.NONE) {
            this.handler.obtainMessage(ModemStatusMonitor.MSG_STATUS, status)
            .sendToTarget();
        }
        if (notification != ModemNotification.NONE) {
            this.handler.obtainMessage(ModemStatusMonitor.MSG_NOTIFICATION,
                                       notification).sendToTarget();
        }
    }

    @Override
    public boolean handleMessage(Message msg) {
        if (msg != null) {
            switch (msg.what) {
            case ModemStatusMonitor.MSG_NOTIFICATION_FEEDBACK:

                ModemNotificationArgs feedback = (ModemNotificationArgs)msg.obj;
                if (feedback != null && feedback.isAcknowledge()) {
                    try {
                        this.replyToNotification(feedback);
                    } catch (MmgrClientException ex) {
                        Log.e(Constants.LOG_TAG, ex.toString(), ex);
                    }
                }
                break;
            case ModemStatusMonitor.MSG_REQUEST:

                ModemRequestArgs request = (ModemRequestArgs)msg.obj;
                try {
                    this.sendRequest(request);
                } catch (MmgrClientException ex) {
                    Log.e(Constants.LOG_TAG, ex.toString(), ex);
                }
                break;
            }
        }
        return true;
    }

    protected void cleanUp() {
        Log.d(Constants.LOG_TAG, "Cleaning up client...");
        if (this.clientSocket != null) {
            try {
                this.clientSocket.shutdownInput();
                this.clientSocket.shutdownOutput();
                this.clientSocket.close();
            } catch (IOException ex) {
                Log.e(Constants.LOG_TAG, ex.toString());
            }
            this.clientSocket = null;
        }
        Log.d(Constants.LOG_TAG, "Cleaning done.");
    }

    @Override
    public void subscribeTo(ModemStatus status, ModemNotification notifications)
    throws MmgrClientException {
        Log.d(Constants.LOG_TAG, "Connecting to service...");

        if (this.thread != null && this.thread.isAlive()) {
            throw new MmgrClientException(
                      "subscribeTo must be called before start.");
        }
        this.subscribedEvents = MmgrHelper.getEventFrom(status, notifications);

        Log.d(Constants.LOG_TAG,
              String.format("Subscribed events: %x", this.subscribedEvents));
    }

    @Override
    public void stop() {
        Log.d(Constants.LOG_TAG, "Stopping client...");
        this.stopRequested = true;
        this.cleanUp();
        if (this.thread != null) {
            try {
                this.thread.join();
                this.thread = null;
            } catch (InterruptedException ex) {
                Log.e(Constants.LOG_TAG, ex.toString());
            }
        }
        Log.d(Constants.LOG_TAG, "Client stopped.");
    }

    @Override
    public void sendRequest(ModemRequestArgs args) throws MmgrClientException {
        if (args != null) {
            byte[] data = args.getFrame();

            if (data != null) {
                OutputStream outputStream = null;
                try {
                    if (this.clientSocket != null) {
                        outputStream = this.clientSocket.getOutputStream();
                        if (outputStream != null) {
                            outputStream.write(data);
                        } else {
                            Log.e(Constants.LOG_TAG,
                                  "Could not write to MMGR socket: outputStream is null.");
                            throw new MmgrClientException(
                                      "Could not write to MMGR socket: outputStream is null.");
                        }
                    } else {
                        Log.e(Constants.LOG_TAG,
                              "Could not write to MMGR socket: clientSocket is null.");
                        throw new MmgrClientException(
                                  "Could not write to MMGR socket: clientSocket is null.");
                    }
                } catch (IOException ex) {
                    throw new MmgrClientException(
                              "Could not write to MMGR socket.", ex);
                }
                Log.d(Constants.LOG_TAG,
                      String.format("%s sent successfully", args.getName()));
            }
        }
    }

    @Override
    public ModemNotificationArgs buildNotificationArgs() {
        return new ModemNotificationArgs();
    }

    @Override
    public void replyToNotification(ModemNotificationArgs args)
    throws MmgrClientException {
        switch (args.getNotification()) {
        case COLD_RESET:
            Log.d(Constants.LOG_TAG,
                  String.format("Replying ACK to cold reset"));
            this.sendRequest(new MmgrModemColdResetAckRequest());
            break;
        case SHUTDOWN:
            Log.d(Constants.LOG_TAG, String.format("Replying ACK to shutdown"));
            this.sendRequest(new MmgrModemShutdownAckRequest());
            break;
        default:
            Log.d(Constants.LOG_TAG,
                  String.format("No possible reply to notification %d",
                                args.getNotification().getValue()));
        }
    }

    @Override
    public void useModem() throws MmgrClientException {
        MmgrModemLockRequest request = new MmgrModemLockRequest();

        this.sendRequest(request);

        if (!this.waitForAck(this.connectTimeoutMs)) {
            throw new MmgrClientException("AcquireModem request failed.");
        }
    }

    @Override
    public void releaseModem() throws MmgrClientException {
        MmgrModemReleaseRequest request = new MmgrModemReleaseRequest();

        this.sendRequest(request);

        if (!this.waitForAck(this.connectTimeoutMs)) {
            throw new MmgrClientException("ReleaseModem request failed.");
        }
    }

    @Override
    public void restartModem() throws MmgrClientException {
        MmgrModemRestartRequest request = new MmgrModemRestartRequest();

        this.sendRequest(request);

        if (!this.waitForAck(this.connectTimeoutMs)) {
            throw new MmgrClientException("RestartModem request failed.");
        }
    }

    @Override
    public void recoverModem(String[] causes) throws MmgrClientException {
        MmgrModemRecoveryRequest request = new MmgrModemRecoveryRequest(causes);

        this.sendRequest(request);

        if (!this.waitForAck(this.connectTimeoutMs)) {
            throw new MmgrClientException("RecoverModem request failed.");
        }
    }

    @Override
    public void shutdownModem() throws MmgrClientException {
        MmgrModemShutdownRequest request = new MmgrModemShutdownRequest();

        this.sendRequest(request);

        if (!this.waitForAck(this.connectTimeoutMs)) {
            throw new MmgrClientException("ShutdownModem request failed.");
        }
    }
}
