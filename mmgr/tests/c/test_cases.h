/* Modem Manager (MMGR) test application - tests definition header file
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

#ifndef __MMGR_TEST_CASES_FILE__
#define __MMGR_TEST_CASES_FILE__

#include "test_utils.h"

e_mmgr_errors_t modem_self_reset(test_data_t *events_data);
e_mmgr_errors_t reset_with_cd(test_data_t *events_data);
e_mmgr_errors_t modem_restart(test_data_t *events_data);
e_mmgr_errors_t modem_recovery(test_data_t *events_data);
e_mmgr_errors_t monkey_cd(test_data_t *ctx);
e_mmgr_errors_t full_recovery(test_data_t *events_data);
e_mmgr_errors_t reset_counter(test_data_t *events_data);
e_mmgr_errors_t resource_check(test_data_t *events_data);
e_mmgr_errors_t test_libmmgrcli_api(test_data_t *events_data);
e_mmgr_errors_t resource_acquire(test_data_t *test);
e_mmgr_errors_t resource_release(test_data_t *test);
e_mmgr_errors_t fake_modem_down(test_data_t *test);
e_mmgr_errors_t fake_modem_up(test_data_t *test);
e_mmgr_errors_t fake_modem_shtdwn(test_data_t *test);
e_mmgr_errors_t fake_modem_hs(test_data_t *test);
e_mmgr_errors_t fake_cd(test_data_t *test);
e_mmgr_errors_t fake_cd_complete(test_data_t *test);
e_mmgr_errors_t fake_ap_reset(test_data_t *test);
e_mmgr_errors_t fake_reboot(test_data_t *test);
e_mmgr_errors_t fake_tft_event(test_data_t *test);
e_mmgr_errors_t fake_self_reset(test_data_t *test);
e_mmgr_errors_t start_modem(test_data_t *test);

#endif                          /* __MMGR_TEST_CASES_FILE__ */
