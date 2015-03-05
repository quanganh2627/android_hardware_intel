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

#ifndef _FPT_H_
#define _FPT_H_

/* Flash full ifwi.  */
int flash_fpt_file_ifwi(char *filename);
int flash_fpt_data_ifwi(void *data, unsigned sz);

/* Flash TXE region only.  */
int flash_fpt_file_txe(char *filename);
int flash_fpt_data_txe(void *data, unsigned sz);

/* Flash PDR region only.  */
int flash_fpt_file_pdr(char *filename);
int flash_fpt_data_pdr(void *data, unsigned sz);

/* Flash IAFW region only.  */
int flash_fpt_file_bios(char *filename);
int flash_fpt_data_bios(void *data, unsigned sz);

/* Flash a specific FPF item.  */
int fpt_writeitem(int argc, char **argv);

/* Flash the fpfs with values set in FILENAME.  */
int flash_fpt_file_fpfs(char *filename);
int flash_fpt_data_fpfs(void *data, unsigned sz);

/* Flash FPF global valid bit, will lock up FPF programming.  */
int fpt_writevalidbit(int argc, char **argv);

#endif	/* _FPT_H_ */
