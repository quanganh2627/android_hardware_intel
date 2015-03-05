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

import android.os.Handler;
import android.os.Message;
import android.os.Handler.Callback;

public interface ModemStatusMonitor extends Callback {
    // service to client messages
    public static final int MSG_ERROR = 1;
    public static final int MSG_STATUS = 2;
    public static final int MSG_NOTIFICATION = 3;

    // client to service messages
    public static final int MSG_NOTIFICATION_FEEDBACK = 1;
    public static final int MSG_REQUEST = 2;

    public void start(String clientName) throws MmgrClientException;

    public void subscribeTo(ModemStatus status, ModemNotification notifications)
    throws MmgrClientException;

    public void stop();

    public void useModem() throws MmgrClientException;

    public void releaseModem() throws MmgrClientException;

    public void sendRequest(ModemRequestArgs args) throws MmgrClientException;

    public void restartModem() throws MmgrClientException;

    public void recoverModem(String[] causes) throws MmgrClientException;

    public boolean waitForStatus(ModemStatus status, long timeout) throws MmgrClientException;

    public void shutdownModem() throws MmgrClientException;

    public ModemNotificationArgs buildNotificationArgs();

    public void setModemStatusHandler(Handler handler);

    public void replyToNotification(ModemNotificationArgs args) throws MmgrClientException;

    @Override
    public boolean handleMessage(Message msg);
}
