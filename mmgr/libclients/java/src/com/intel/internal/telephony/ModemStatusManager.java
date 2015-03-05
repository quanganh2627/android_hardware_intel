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

package com.intel.internal.telephony;

import java.io.File;

import com.intel.internal.telephony.mmgr.MedfieldMmgrClient;
import com.intel.internal.telephony.stmd.MedfieldStmdClient;

import android.os.AsyncTask;
import android.os.Handler;
import android.os.Handler.Callback;
import android.os.Message;

/**
 * Implements the client side that abstracts communication to the Modem Status
 * Monitor service. This class is singleton, you must instantiate it through
 * getInstance() method. The usage of this API is as follow: - Implement the
 * ModemEventListener interface - Instantiate ModemStatusManager - Pass your
 * implementation of ModemEventListener to ModemStatusManager.subscribeToEvent()
 * method - Also pass the bit mask of events you wish to listen to - Call
 * ModemStatusManager.connect() when your app starts - Call
 * ModemStatusManager.disconnect() when your app stops
 *
 */
public class ModemStatusManager implements Callback {
    private ModemEventListener eventListener = null;

    private ModemStatusMonitor modemStatusMonitor = null;
    private Handler statusEventsHandler = null; // Client <- MMGR
    private Handler requestHandler = null;      // Client -> MMGR

    private static ModemStatusManager instance = null;

    private ModemStatusManager() throws InstantiationException {
        this.statusEventsHandler = new Handler(this);

        if (this.mmgrSocketExists()) {
            this.modemStatusMonitor = new MedfieldMmgrClient(
                this.statusEventsHandler);
        } else if (this.stmdSocketExists()) {
            this.modemStatusMonitor = new MedfieldStmdClient(
                this.statusEventsHandler);
        } else {
            throw new InstantiationException(
                      "Neither STMD nor MMGR are present on this device.");
        }
        this.requestHandler = new Handler(this.modemStatusMonitor);
    }

    private boolean stmdSocketExists() {
        File stmd = new File("/dev/socket/stmd");

        return stmd.exists();
    }

    private boolean mmgrSocketExists() {
        File mmgr = new File("/dev/socket/mmgr");

        return mmgr.exists();
    }

    /**
     * Returns the single instance of ModemStatusManager.
     *
     * @return The single instance of ModemStatusManager
     * @throws InstantiationException
     *             if MMGR or STMD are not present of the device.
     */
    public static ModemStatusManager getInstance()
    throws InstantiationException {
        if (ModemStatusManager.instance == null) {
            ModemStatusManager.instance = new ModemStatusManager();
        }
        return ModemStatusManager.instance;
    }

    /**
     * Requests a modem restart to the Modem Status Monitor service.
     *
     * @throws MmgrClientException
     *             if the service returned an error or if a communication error
     *             occurred between the client and the service.
     */
    public void restartModem() throws MmgrClientException {
        if (this.modemStatusMonitor != null) {
            this.modemStatusMonitor.restartModem();
        }
    }

    /**
     * Requests a modem restart asynchronously (call is not blocking).
     *
     * @param listener The listener to get notified upon operation result.
     */
    public void restartModemAsync(final AsyncOperationResultListener listener) {
        new AsyncOperationTask(AsyncOperationTask.OPERATION_RESTART_MODEM,
                               listener).execute();
    }

    /**
     * Requests a modem recover to the Modem Status Monitor service.
     *
     * @throws MmgrClientException
     *             if the service returned an error or if a communication error
     *             occurred between the client and the service.
     */
    public void recoverModem() throws MmgrClientException {
        recoverModem(null);
    }

    /**
     * Requests a modem recover to the Modem Status Monitor service.
     *
     * @param causes array of string that will be reported to crashlogd
     *
     * @throws MmgrClientException
     *             if the service returned an error or if a communication error
     *             occurred between the client and the service.
     */
    public void recoverModem(String[] causes) throws MmgrClientException {
        if (this.modemStatusMonitor != null) {
            this.modemStatusMonitor.recoverModem(causes);
        }
    }

    /**
     * Requests a modem recover asynchronously (call is not blocking).
     *
     * @param listener The listener to get notified upon operation result.
     */
    public void recoverModemAsync(final AsyncOperationResultListener listener) {
        recoverModemAsync(listener, null);
    }

    /**
     * Requests a modem recover asynchronously (call is not blocking).
     *
     * @param listener The listener to get notified upon operation result.
     * @param causes array of string that will be reported to crashlogd
     */
    public void recoverModemAsync(final AsyncOperationResultListener listener,
                                  String[] causes) {
        new AsyncOperationTask(AsyncOperationTask.OPERATION_RECOVER_MODEM,
                               listener, causes).execute();
    }

    /**
     * Requests a modem shutdown to the Modem Status Monitor service.
     *
     * @throws MmgrClientException
     *             if the service returned an error or if a communication error
     *             occurred between the client and the service.
     */
    public void shutdowModem() throws MmgrClientException {
        if (this.modemStatusMonitor != null) {
            this.modemStatusMonitor.shutdownModem();
        }
    }

    /**
     * Requests a modem shutdown asynchronously (call is not blocking).
     *
     * @param listener The listener to get notified upon operation result.
     */
    public void shutdownModemAsync(final AsyncOperationResultListener listener) {
        new AsyncOperationTask(AsyncOperationTask.OPERATION_SHUTDOWN_MODEM,
                               listener).execute();
    }

    /**
     * Requests a modem lock to the Modem Status Monitor service.
     *
     * @throws MmgrClientException
     *             if the service returned an error or if a communication error
     *             occurred between the client and the service.
     */
    public void acquireModem() throws MmgrClientException {
        if (this.modemStatusMonitor != null) {
            this.modemStatusMonitor.useModem();
        }
    }

    /**
     * Requests a modem acquisition asynchronously (call is not blocking).
     *
     * @param listener The listener to get notified upon operation result.
     */
    public void acquireModemAsync(final AsyncOperationResultListener listener) {
        new AsyncOperationTask(AsyncOperationTask.OPERATION_ACQUIRE_MODEM,
                               listener).execute();
    }

    /**
     * Requests a modem release to the Modem Status Monitor service.
     *
     * @throws MmgrClientException
     *             if the service returned an error or if a communication error
     *             occurred between the client and the service.
     */
    public void releaseModem() throws MmgrClientException {
        if (this.modemStatusMonitor != null) {
            this.modemStatusMonitor.releaseModem();
        }
    }

    /**
     * Requests a modem release asynchronously (call is not blocking).
     *
     * @param listener The listener to get notified upon operation result.
     */
    public void releaseModemAsync(final AsyncOperationResultListener listener) {
        new AsyncOperationTask(AsyncOperationTask.OPERATION_RELEASE_MODEM,
                               listener).execute();
    }

    /**
     * Connects to the Modem Status Monitor service. You must call this method
     * to get your implementation of ModemEventListener called.
     *
     * @param clientName
     * @throws MmgrClientException
     *             if the service returned an error or if a communication error
     *             occurred between the client and the service.
     */
    public void connect(String clientName) throws MmgrClientException {
        if (this.modemStatusMonitor != null) {
            this.modemStatusMonitor.start(clientName);
        }
    }

    /**
     * Requests a connection asynchronously (call is not blocking).
     *
     * @param clientName name of the client
     * @param listener The listener to get notified upon operation result.
     */
    public void connectAsync(String clientName,
                             final AsyncOperationResultListener listener) {
        new AsyncOperationTask(AsyncOperationTask.OPERATION_CONNECT,
                               listener).execute(clientName);
    }

    /**
     * @param status The modem status to wait for
     * @param timeout The maximum amount of time (in milliseconds) to wait
     * @return True if the status was received; otherwise False
     * @throws MmgrClientException On any error
     */
    public boolean waitForModemStatus(ModemStatus status, long timeout)
    throws MmgrClientException {
        if (this.modemStatusMonitor != null) {
            return this.modemStatusMonitor.waitForStatus(status, timeout);
        }
        return false;
    }

    /**
     * @param args The request to send
     * @throws MmgrClientException On any error
     */
    public void sendRequest(ModemRequestArgs args) throws MmgrClientException {
        if (this.modemStatusMonitor != null) {
            this.modemStatusMonitor.sendRequest(args);
        }
    }

    /**
     * Disconnects from the Modem Status Monitor service. After calling this
     * method, the implementation of ModemEventListener will not be notified
     * anymore.
     */
    public void disconnect() {
        try {
            if (this.modemStatusMonitor != null) {
                this.modemStatusMonitor.stop();
            }
        } finally {
            this.eventListener = null;
        }
    }

    /**
     * Requests a disconnection asynchronously (call is not blocking).
     *
     * @param listener The listener to get notified upon operation result.
     */
    public void disconnectAsync(final AsyncOperationResultListener listener) {
        new AsyncOperationTask(AsyncOperationTask.OPERATION_DISCONNECT,
                               listener).execute();
    }

    /**
     * Registers / subscribes an implementation of ModemEventListener to receive
     * modem events.
     *
     * @param listener
     *            The listener to register.
     * @param status
     *            The bit mask of the modem statuses to listen to.
     * @param notifications
     *            The bit mask of the modem notifications to listen to.
     * @return The same instance of ModemStatusManager (this).
     * @throws MmgrClientException
     *             if the service returned an error or if a communication error
     *             occurred between the client and the service.
     */
    public synchronized ModemStatusManager subscribeToEvent(
        ModemEventListener listener, ModemStatus status,
        ModemNotification notifications) throws MmgrClientException {
        this.eventListener = listener;

        if (this.modemStatusMonitor != null) {
            this.modemStatusMonitor.subscribeTo(status, notifications);
        }
        return this;
    }

    /*
     * (non-Javadoc)
     *
     * @see android.os.Handler.Callback#handleMessage(android.os.Message)
     */
    public boolean handleMessage(Message msg) {
        if (msg != null) {
            switch (msg.what) {
            case ModemStatusMonitor.MSG_STATUS:
                ModemStatus status = (ModemStatus)msg.obj;

                this.dispatchStatus(status);
                break;

            case ModemStatusMonitor.MSG_NOTIFICATION:
                ModemNotification notification = (ModemNotification)msg.obj;

                ModemNotificationArgs feedback = this
                                                 .dispatchNotification(notification);

                if (feedback != null) {
                    this.requestHandler.obtainMessage(
                        ModemStatusMonitor.MSG_NOTIFICATION_FEEDBACK,
                        feedback).sendToTarget();
                }
                break;
            }
        }
        return true;
    }

    private void dispatchStatus(ModemStatus status) {
        if (this.eventListener != null) {
            // let's get the list of listeners interested by our event
            synchronized (this.eventListener) {
                switch (status) {
                case UP:
                    this.eventListener.onModemUp();
                    break;
                case DOWN:
                    this.eventListener.onModemDown();
                    break;
                case DEAD:
                    this.eventListener.onModemDead();
                    break;
                default:
                    break;
                }
            }
        }
    }

    private ModemNotificationArgs dispatchNotification(
        ModemNotification notification) {
        ModemNotificationArgs args = null;

        if (this.eventListener != null) {
            // let's get the list of listeners interested by our event
            synchronized (this.eventListener) {
                args = this.modemStatusMonitor.buildNotificationArgs();

                if (args != null) {
                    args.setSender(this);
                    args.setNotification(notification);
                }
                switch (notification) {
                case COLD_RESET:
                    this.eventListener.onModemColdReset(args);
                    break;
                case SHUTDOWN:
                    this.eventListener.onModemShutdown(args);
                    break;
                case PLATFORM_REBOOT:
                    this.eventListener.onPlatformReboot(args);
                    break;
                case CORE_DUMP:
                    this.eventListener.onModemCoreDump(args);
                    break;
                default:
                    break;
                }
            }
        }
        return args;
    }

    private class AsyncOperationTask extends AsyncTask<Object, Void, Exception> {
        private AsyncOperationResultListener listener = null;
        private int requiredOperation = 0;
        private String[] causes = null;

        public static final int OPERATION_ACQUIRE_MODEM = 1;
        public static final int OPERATION_RELEASE_MODEM = 2;
        public static final int OPERATION_RESTART_MODEM = 3;
        public static final int OPERATION_RECOVER_MODEM = 4;
        public static final int OPERATION_CONNECT = 5;
        public static final int OPERATION_DISCONNECT = 6;
        public static final int OPERATION_SHUTDOWN_MODEM = 7;

        public AsyncOperationTask(int requiredOperation,
                                  AsyncOperationResultListener listener) {
            this.listener = listener;
            this.requiredOperation = requiredOperation;
        }

        public AsyncOperationTask(int requiredOperation,
                                  AsyncOperationResultListener listener,
                                  String[] causes) {
            this.listener = listener;
            this.requiredOperation = requiredOperation;
            this.causes = causes;
        }

        @Override
        protected Exception doInBackground(Object ... params) {
            Exception ret = null;

            try {
                switch (this.requiredOperation) {
                case AsyncOperationTask.OPERATION_ACQUIRE_MODEM:
                    ModemStatusManager.this.acquireModem();
                    break;
                case AsyncOperationTask.OPERATION_RELEASE_MODEM:
                    ModemStatusManager.this.releaseModem();
                    break;
                case AsyncOperationTask.OPERATION_RESTART_MODEM:
                    ModemStatusManager.this.restartModem();
                    break;
                case AsyncOperationTask.OPERATION_RECOVER_MODEM:
                    ModemStatusManager.this.recoverModem(causes);
                    break;
                case AsyncOperationTask.OPERATION_CONNECT:
                    if (params != null && params.length > 0) {
                        ModemStatusManager.this.connect((String)(params[0]));
                    }
                    break;
                case AsyncOperationTask.OPERATION_DISCONNECT:
                    ModemStatusManager.this.disconnect();
                    break;
                case AsyncOperationTask.OPERATION_SHUTDOWN_MODEM:
                    ModemStatusManager.this.shutdowModem();
                    break;
                }
            } catch (Exception ex) {
                ret = ex;
            }
            return ret;
        }

        @Override
        protected void onPostExecute(Exception result) {
            super.onPostExecute(result);
            if (listener != null) {
                if (result != null) {
                    listener.onOperationError(result);
                } else {
                    listener.onOperationComplete();
                }
            }
        }
    }
}
