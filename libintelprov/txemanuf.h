/*
 * Copyright 2013 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _TXEMANUF_H_
#define _TXEMANUF_H_

/* Close EOM, lock up SPI access.  */
int fpt_closemnf(int argc, char **argv);

/* Load TXEManuf config file.  */
int flash_txemanuf_file(char *filename);
int flash_txemanuf_data(void *data, unsigned sz);

/* Run EOL test.  */
int txemanuf_eof_test(int argc, char **argv);

/* Run BIST test.  */
int txemanuf_bist_test(int argc, char **argv);

#endif	/* _TXEMANUF_H_ */
