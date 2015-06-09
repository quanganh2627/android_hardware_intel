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

/**
 * An interface to implement to get notified of modem events. The instance
 * implementing this interface must be registered through
 * ModemStatusManager.subscribeToEvent() method.
 */
public interface ModemEventListener {
    /**
     * This method is called prior each modem cold reset planned by the Modem
     * Status Monitor service.
     *
     * @param args
     *            The arguments of the notification. Use
     *            ModemNotificationArgs.setAcknowledge method to return ACK to
     *            the Modem Status Monitor service.
     */
    public void onModemColdReset(ModemNotificationArgs args);

    /**
     * This method is called prior each modem shutdown planned by the Modem
     * Status Monitor service.
     *
     * @param args
     *            The arguments of the notification. Use
     *            ModemNotificationArgs.setAcknowledge method to return ACK to
     *            the Modem Status Monitor service.
     */
    public void onModemShutdown(ModemNotificationArgs args);

    /**
     * This method is called prior each platform reboot planned by the Modem
     * Status Monitor service.
     *
     * @param args
     *            The arguments of the notification. Use
     *            ModemNotificationArgs.setAcknowledge method to return ACK to
     *            the Modem Status Monitor service.
     */
    public void onPlatformReboot(ModemNotificationArgs args);

    /**
     * This method is called prior each platform reboot planned by the Modem
     * Status Monitor service.
     *
     * @param args
     *            The arguments of the notification. Use
     *            ModemNotificationArgs.setAcknowledge method to return ACK to
     *            the Modem Status Monitor service.
     */
    public void onModemCoreDump(ModemNotificationArgs args);

    /**
     * This method is called upon a modem up event detected by the Modem Status
     * Monitor service.
     */
    public void onModemUp();

    /**
     * This method is called upon a modem down event detected by the Modem
     * Status Monitor service.
     */
    public void onModemDown();

    /**
     * This method is called upon a modem out of service event detected by the
     * Modem Status Monitor service.
     */
    public void onModemDead();
}
