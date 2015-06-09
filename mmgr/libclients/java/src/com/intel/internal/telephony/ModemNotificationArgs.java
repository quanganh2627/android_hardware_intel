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

public class ModemNotificationArgs {
    private ModemStatusManager sender = null;
    private ModemNotification notification;
    private boolean acknowledge = true;

    public ModemStatusManager getSender() {
        return this.sender;
    }

    public void setSender(ModemStatusManager sender) {
        this.sender = sender;
    }

    public ModemNotification getNotification() {
        return this.notification;
    }

    public void setNotification(ModemNotification notification) {
        this.notification = notification;
    }

    public boolean isAcknowledge() {
        return this.acknowledge;
    }

    public void setAcknowledge(boolean acknowledge) {
        this.acknowledge = acknowledge;
    }
}
