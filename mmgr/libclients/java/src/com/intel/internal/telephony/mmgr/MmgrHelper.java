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

import com.intel.internal.telephony.ModemNotification;
import com.intel.internal.telephony.ModemStatus;

public class MmgrHelper {
    public static int getEventFrom(ModemStatus status,
                                   ModemNotification notifications) {
        return getEventFrom(notifications) | getEventFrom(status);
    }

    private static int getEventFrom(ModemStatus status) {
        int ret = 0;

        if ((status.getValue() & ModemStatus.UP.getValue()) != 0) {
            ret |= (1 << MedfieldMmgrClient.STATUS_MODEM_UP);
        }
        if ((status.getValue() & ModemStatus.DOWN.getValue()) != 0) {
            ret |= (1 << MedfieldMmgrClient.STATUS_MODEM_DOWN);
        }
        if ((status.getValue() & ModemStatus.DEAD.getValue()) != 0) {
            ret |= (1 << MedfieldMmgrClient.STATUS_MODEM_OUT_OF_SERVICE);
        }
        return ret;
    }

    private static int getEventFrom(ModemNotification notification) {
        int ret = 0;

        if ((notification.getValue() & ModemNotification.COLD_RESET.getValue()) != 0) {
            ret |= (1 << MedfieldMmgrClient.NOTIFY_MODEM_COLD_RESET);
        }
        if ((notification.getValue() & ModemNotification.CORE_DUMP.getValue()) != 0) {
            ret |= (1 << MedfieldMmgrClient.NOTIFY_MODEM_CORE_DUMP);
        }
        if ((notification.getValue() & ModemNotification.SHUTDOWN.getValue()) != 0) {
            ret |= (1 << MedfieldMmgrClient.NOTIFY_MODEM_SHUTDOWN);
        }
        if ((notification.getValue() & ModemNotification.PLATFORM_REBOOT
             .getValue()) != 0) {
            ret |= (1 << MedfieldMmgrClient.NOTIFY_PLATFORM_REBOOT);
        }
        // by default we want to subscribe to ACK and NACK
        ret |= (1 << MedfieldMmgrClient.NOTIFY_ACK);
        ret |= (1 << MedfieldMmgrClient.NOTIFY_NACK);
        return ret;
    }
}
