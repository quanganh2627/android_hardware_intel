/* Modem Manager - java intent source file
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

#include <stdio.h>
#include <stdlib.h>
#include "java_intent.h"
#include "logs.h"

static const char const *g_msg_str[] = {
#undef X
#define X(a) #a
    MSG_INTENTS
};

static const char const *g_action_str[] = {
#undef X
#define X(a) #a
    ACTION_INTENTS
};

#define INTENT_MAX_SIZE 256
#define ACTION "am start -a android.intent.action.%s"
#define MSG "am broadcast -a com.intel.action.%s"

/**
 * broadcast an java intent
 *
 * @param [in] mask message format
 * @param [in] action string action
 */
static void broadcast(char *mask, const char *action)
{
    int err;
    char msg[INTENT_MAX_SIZE];

    err = snprintf(msg, INTENT_MAX_SIZE, mask, action);

    if (err > 0) {
        system(msg);
        LOG_DEBUG("(%s) sent", msg);
    }
}

/**
 * broadcast a message intent
 *
 * @param [in] id message intent id
 */
void broadcast_msg(e_intent_msg_t id)
{
    broadcast(MSG, g_msg_str[id]);
}

/**
 * broadcast an action intent
 *
 * @param [in] id action intent id
 */
void broadcast_action(e_intent_action_t id)
{
    broadcast(ACTION, g_action_str[id]);
}
