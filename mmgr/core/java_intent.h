/* Modem Manager - java intent header file
**
** Copyright (C) Intel 2012
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*/

#ifndef __MMGR_JAVA_INTENT_HEADER__
#define __MMGR_JAVA_INTENT_HEADER__

#define MSG_INTENTS \
    X(CORE_DUMP_WARNING), \
    X(CORE_DUMP_COMPLETE), \
    X(MODEM_OUT_OF_SERVICE), \
    X(PLATFORM_REBOOT), \
    X(REBOOT), \
    X(MODEM_COLD_RESET), \
    X(MODEM_UNSOLICITED_RESET), \
    X(MODEM_NOT_RESPONSIVE), \
    /* flashless specific errors */ \
    X(MODEM_FW_BAD_FAMILY), \
    X(MODEM_FW_OUTDATED), \
    X(MODEM_FW_SECURITY_CORRUPTED), \
    X(MODEM_FW_RESTRICTED), \
    X(MODEM_FW_UPDATE_FAILURE), \
    X(MSG_NUM)

#define ACTION_INTENTS \
    X(REBOOT), \
    X(ACTION_NUM)

typedef enum e_intent_msg {
#undef X
#define X(a) E_MSG_INTENT_ ## a
    MSG_INTENTS
} e_intent_msg_t;

typedef enum e_intent_action {
#undef X
#define X(a) E_ACTION_INTENT_ ## a
    ACTION_INTENTS
} e_intent_action_t;

void broadcast_msg(e_intent_msg_t id);
void broadcast_action(e_intent_action_t id);

#endif                          /* __MMGR_JAVA_INTENT_HEADER__ */
