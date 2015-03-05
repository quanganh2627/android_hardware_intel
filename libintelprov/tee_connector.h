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

#ifndef TEE_CONNECTOR_H
#define TEE_CONNECTOR_H

#include <unistd.h>
#include "tee_token_if.h"
#include "ifp.h"
#include "sep_keymaster.h"

/* In order to correctly redirect the error and information output,
   you MUST provide the following two functions before using the
   others.  */
extern void (*print_fun)(const char *msg);
extern void (*error_fun)(const char *msg);

extern void raise_error(const char *fmt, ...);

extern int get_spid(int argc, char **argv);
extern int get_fru(int argc, char **argv);
extern int get_part_id(int argc, char **argv);
extern int get_lifetime(int argc, char **argv);
extern int get_ssn(int argc, char **argv);
extern int start_update(int argc, char **argv);
extern int cancel_update(int argc, char **argv);
extern int finalize_update(int argc, char **argv);
extern int write_token(void *data, size_t size);
extern int remove_token(int argc, char **argv);
extern int read_token(int argc, char **argv);
extern int read_token_payload(int argc, char **argv);
extern int send_cryptid_request(void *data, size_t size);
extern int generate_shared_ecc(int argc, char **argv);
extern int generate_shared_rsa(int argc, char **argv);

extern int set_output_file(const char *path);
extern void close_output_file_when_open();

#endif	/* TEE_CONNECTOR_H */
