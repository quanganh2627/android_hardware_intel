/*
 * INTEL CONFIDENTIAL
 * Copyright © 2011 Intel
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
 *
 */

class NaiveTokenizer {
public:
    /** tokenize a space-separated string, handling quotes
     *
     * The input is tokenized, using " " (space) as the tokenizer; multiple
     * spaces are regarded as a single separator.  If the input begins with a
     * quote (either single (') or double (")), the next token will be all the
     * characters (including spaces) until the next identical quote.
     * Warning: This function modifies its argument in-place !
     *
     * It aims at reproducing the parsing of a shell.
     *
     * @param[inout] line The string to be tokenized. Warning: modified in-place
     * @return Pointer to the next token (which is actually the original value of 'line'
     */
    static char* getNextToken(char** line);
};
