/*
 *  bd_provisioning.c - bluetooth device provisioning application
 *
 *  Copyright(c) 2009-2011 Intel Corporation. All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#undef LOGV
#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <cutils/properties.h>

#define LOG_TAG "bd_prov"

#define BD_ADDRESS_LEN 6

#define NO_ERR                       0
#define ERR_WRONG_PARAM             -1

#define BD_ADDR_FILE_NAME	"/config/bt/bd_addr.conf"
#define BD_LEN			18

#if (BUILD_WITH_CHAABI_SUPPORT || BUILD_WITH_TXEI_SUPPORT)

#include "umip_access.h"

int get_bd_address(unsigned char **bd_addr_buf){
	return get_customer_data(ACD_BT_MAC_ADDR_FIELD_INDEX,
						(void ** const) bd_addr_buf);
}

#elif BUILD_WITH_TOKEN_SUPPORT

#include "tee_token_if.h"

#define TOKEN_DG_ID		12	// Group ID
#define TOKEN_SG_ID		10	// Subgroup ID
#define TOKEN_ITEM_ID		1	// Item ID

int get_bd_address(unsigned char **bd_addr_buf){
	int ret;
	*bd_addr_buf = malloc(sizeof(char) * BD_ADDRESS_LEN);
	ret = tee_token_item_read(TOKEN_DG_ID, TOKEN_SG_ID, TOKEN_ITEM_ID,
					0, *bd_addr_buf, BD_ADDRESS_LEN, 0);
	if (!ret)
		return BD_ADDRESS_LEN;
	return NULL;
}

#endif /* (BUILD_WITH_CHAABI_SUPPORT || BUILD_WITH_TXEI_SUPPORT) */


int main(int argc, char **argv)
{
	unsigned char *bd_addr_buf = NULL;
	int res = NO_ERR;
	char state[PROPERTY_VALUE_MAX];
	FILE *bd_addr_file = NULL;
	char *bd_addr_file_name = BD_ADDR_FILE_NAME;
	char bd_address_str[BD_LEN] = "00:00:00:00:00:00";

	/* Check parameters */
	if (argc != 1) {
		/* No param expected */
		return ERR_WRONG_PARAM;
	}

#if (BUILD_WITH_CHAABI_SUPPORT || BUILD_WITH_TOKEN_SUPPORT || BUILD_WITH_TXEI_SUPPORT)
	/* Read BD address from Chaabi */
	LOGV("Retrieving BD address...");
	res = get_bd_address(&bd_addr_buf);
	if ((res != BD_ADDRESS_LEN) || !bd_addr_buf) {
		/* chaabi read error OR no chaabi support */
		if (res < 0)
			LOGE("Error retrieving BD address, "
					"error %d", res);
		else
			LOGE("Error retrieving BD address, "
					"wrong length");
		if (bd_addr_buf) {
			free(bd_addr_buf);
			bd_addr_buf = NULL;
		}
	} else {
		LOGV("BD address successfully retrieved: "
				"%02X:%02X:%02X:%02X:%02X:%02X",
				bd_addr_buf[0], bd_addr_buf[1],
				bd_addr_buf[2], bd_addr_buf[3],
				bd_addr_buf[4], bd_addr_buf[5]);
	}
	if (bd_addr_buf) {
		/* write to file */
		LOGD("Open file %s for writing\n", bd_addr_file_name);
		bd_addr_file = fopen(bd_addr_file_name, "w");
		if (bd_addr_file != NULL) {
			snprintf(bd_address_str, sizeof(bd_address_str), "%02X:%02X:%02X:%02X:%02X:%02X",
				bd_addr_buf[0], bd_addr_buf[1],
				bd_addr_buf[2], bd_addr_buf[3],
				bd_addr_buf[4], bd_addr_buf[5]);
			res = fprintf(bd_addr_file, "%s", bd_address_str);
			if (res)
				LOGD("BD address written successfully");
			else
				LOGE("Error %s, failed to write BD address", strerror(errno));
			fflush(bd_addr_file);
			fclose(bd_addr_file);
                        //change BD addr file permission,ensure it can be opened later for reading
                        res=chmod(bd_addr_file_name, 0664);
                        if(res < 0)
                              LOGE("BD addr_file change permission failure");
                        else
                              LOGD("BD addr_file  change permission success");
		}
		else {
			LOGE("Error %s while opening %s\n", strerror(errno), bd_addr_file_name);
			return errno;
		}
		/* deallocate buffer set by get_bd_address() */
		free(bd_addr_buf);
	} else {
		LOGE("No chaabi BD address");
	}
#else
	LOGE("Chaabi not supported, "
			"BD address diversification is not available");
#endif /* (BUILD_WITH_CHAABI_SUPPORT || BUILD_WITH_TOKEN_SUPPORT || BUILD_WITH_TXEI_SUPPORT) */

	return NO_ERR;
}
