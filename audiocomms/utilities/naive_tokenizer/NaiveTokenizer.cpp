/*
 * INTEL CONFIDENTIAL
 * Copyright © 2013 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */
#include "NaiveTokenizer.h"
#include <cstring>

char* NaiveTokenizer::getNextToken(char** line)
{
    const char *quotes = "'\""; // single or double quotes
    char separator[2] = " ";
    char first[2];

    if (*line == NULL || (*line)[0] == '\0') {
        return NULL;
    }

    // Copy the first character into its own new string
    first[0] = (*line)[0];
    first[1] = '\0';

    // Check if the first character is a quote
    if (strstr(quotes, first) != NULL) {
        // If so, move forward and set the separator to that quote
        (*line)++;
        strncpy(separator, first, sizeof(separator));
    }
    // If it is not, get the next space-delimited token
    // First, move the cursor forward if the first character is a space
    // This effectively ignores multiple spaces in a row
    else if (strstr(separator, first) != NULL) {
        (*line)++;
        return NaiveTokenizer::getNextToken(line);
    }

    return strsep(line, separator);
}
