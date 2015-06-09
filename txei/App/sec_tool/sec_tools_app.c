/**********************************************************************
 * Copyright 2013 (c) Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **********************************************************************/

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>
#include <stdbool.h>

/* define LOG related macros before including the log header file */
#define DEBUG
#define LOG_TAG      "CC6SECUTIL-app"
#include "cutils/log.h"
#include "sepdrm-log.h"
#include "umip_access.h"
#include "misc_utils.h"
#include "txei.h"


/* ====================
 * Constants
 */
#define PMDB_READ_CMD_STR "pmdb-read"
#define PMDB_WRITE_CMD_STR "pmdb-write"
#define PMDB_LOCK_CMD_STR "pmdb-lock"
#define PMDB_WO_AREA_STR  "WO"
#define PMDB_WM_AREA_STR  "WM"
/*
 * gpp_wpgroup - get or set the GPP write protection group
 * gpp_wppart - get or set the GPP partition
 * gpp_wp - get the current write protection status for the wpgroup
 * 	    and wppartition, write sets the GPP write protection to
 * 	    PERMANENT
 *
 */
#define MMC_SYSFS  "/sys/devices/pci0000:00/0000:00:01.0/mmc_host/mmc0/mmc0:0001/"
#define GPP_WPGROUP MMC_SYSFS "gpp_wpgroup"
#define GPP_WPPART MMC_SYSFS "gpp_wppart"
#define GPP_WP MMC_SYSFS "gpp_wp"

#define MAX_STR_SIZE 256
#define FILE_ERROR (-1)
#define NOERROR 0
#define GPP_WPSTATUS_MASK (0x3)

/* ====================
 */

/*
 * Print the usage message
 *
 * @param progname the program name to be printed in the message,
 * 		   normally argv[0].
 */
static void usage( const char * const progname )
{
	static const char *usage_string = {
		"\n"
		"Usage: %s <command> [CMD OPTIONS]\n"
		"       -acd-read  <index> <file name>                              -- read an entry\n"
		"       -acd-write <index> <file name> <data size> <data max size>  -- write an entry\n"
		"       -acd-lock                                                   -- lock ACD table\n"
		"       -acd-prov  <provisioning scheme> <input file> output file   -- key provisioning\n"
		"       -epid-prov  <input file>                                -- perform EPID provisioning\n"
		"       -pmdb-read  [WO | WM] <file>                            -- read PMDB write-once or write-many\n"
		"       -pmdb-write [WO | WM] <file>                            -- write PMDB write-once or write-many\n"
		"       -pmdb-lock                                              -- lock PMDB\n"
		"       -gpp-wp-status                                          -- Get the SF GPP write-protection status\n"
		"       -gpp-wp-lock                                            -- Write protect (lock) the SF GPP\n"
		"       -rpmb-prov                                              -- Provision the RPMB device key\n"
	};
	printf(usage_string, progname);
}

/*
 * Read the GPP write-protection status from the MMC driver sysfs
 * attributes.
 *
 * @return EXIT_FAILURE if an error occurs.
 */
static int gpp_read_wp_status() {
	int result = EXIT_FAILURE;
	int wpgrp_fd = FILE_ERROR;
	int wppart_fd = FILE_ERROR;
	int wp_fd = FILE_ERROR;
	ssize_t retsz = 0;
	const char partition = '1';
	const char group = '0';
	char status_str[ MAX_STR_SIZE ];

	/* One-shot do-while() */
	do {
		wpgrp_fd = open( GPP_WPGROUP, O_RDWR );
		if (wpgrp_fd == FILE_ERROR) {
			LOGERR("GPP WP Group file open failed\n");
			break;
		}
		wppart_fd = open( GPP_WPPART, O_RDWR );
		if (wppart_fd == FILE_ERROR) {
			LOGERR("GPP WP Part file open failed\n");
			break;
		}
		wp_fd = open( GPP_WP, O_RDWR );
		if (wp_fd == FILE_ERROR) {
			LOGERR("GPP WP file open failed\n");
			break;
		}
		/*
		 * Set GPP partition for which the write protection
		 * status
		 */
		LOGDBG("Setting Partition %c\n", partition);
		retsz = write( wppart_fd, &partition, sizeof(partition) );
		if (retsz != sizeof(partition) ) {
			LOGERR("GPP partition %c not supported\n", partition);
			perror("GPP Partition write failed: " );
			break;
		}
		/*
		 * Set the write protect group number. 0 means the first write
		 * protect group
		 */
		LOGDBG("Setting group %c\n", group);
		retsz = write(wpgrp_fd, &group, sizeof(group) );
		if (retsz != sizeof(group) ) {
			LOGERR("GPP group %c not available\n", group);
			perror("GPP Group Write failed: " );
			break;
		}
		/*
		 * Read the Write Protect Status and display it
		 */
		retsz = read( wp_fd, status_str, sizeof(status_str) );
		if (retsz == FILE_ERROR ) {
			LOGERR("GPP WP read failed\n");
			perror("GPP WP Read Failure " );
			break;
		}
		/*
		 * Convert string to uint8 and check to make sure a
		 * conversion was made.
		 */
		errno = NOERROR;
		uint8_t gpp_status = (uint8_t)strtoul(status_str, NULL, 10);
		if (gpp_status > GPP_WPSTATUS_MASK ||
		    (errno != NOERROR && gpp_status == 0)) {
			LOGERR("Invalid GPP WP status (%u)\n", gpp_status);
			break;
		}
		static const char * lock_status[] = { "UNLOCKED (0)",
						      "TEMPORARY (1)",
						      "POWER-ON (2)",
						      "PERMANENT (3)"};
		printf("GPP%c group %c partition write protection is %s\n",
		       partition, group, lock_status[ gpp_status & GPP_WPSTATUS_MASK]);
		result = EXIT_SUCCESS;
	} while (0);

	if ( wpgrp_fd > FILE_ERROR ) {
		close( wpgrp_fd );
	}
	if ( wppart_fd > FILE_ERROR ) {
		close( wppart_fd );
	}
	if ( wp_fd > FILE_ERROR ) {
		close( wp_fd );
	}
	return result;
}

/*
 *  Write-protect the Softfuse GPP partition using the MMC driver sysfs
 *  attributes.
 *
 * @return EXIT_FAILURE if an error occurs.
 */
static int gpp_writeprotect() {
	int result = EXIT_FAILURE;
	int wpgrp_fd = FILE_ERROR;
	int wppart_fd = FILE_ERROR;
	int wp_fd = FILE_ERROR;
	ssize_t retsz = 0;
	const char partition = '1';
	const char group = '0';
	const char wp_permanent = '1';

	/* One-shot do-while() */
	do {
		wpgrp_fd = open( GPP_WPGROUP, O_RDWR );
		if (wpgrp_fd == FILE_ERROR) {
			LOGERR("GPP WP Group file open failed\n");
			break;
		}
		wppart_fd = open( GPP_WPPART, O_RDWR );
		if (wppart_fd == FILE_ERROR) {
			LOGERR("GPP WP Part file open failed\n");
			break;
		}
		wp_fd = open( GPP_WP, O_RDWR );
		if (wp_fd == FILE_ERROR) {
			LOGERR("GPP WP Part file open failed\n");
			break;
		}

		/*
		 * Set GPP partition for which to set the write
		 * protection
		 */
		LOGDBG("Setting Partition %c\n", partition);
		retsz = write( wppart_fd, &partition, sizeof(partition) );
		if (retsz != sizeof(partition) ) {
			LOGERR("GPP partition %c not supported\n", partition);
			perror("GPP Partition: " );
			break;
		}
		/*
		 * Set the write protect group number. 0 means the first write
		 * protect group
		 */
		LOGDBG("Setting group %c\n", group);
		retsz = write(wpgrp_fd, &group, sizeof(group) );
		if (retsz != sizeof(group) ) {
			LOGERR("GPP group %c not available\n", group);
			perror("GPP Group: " );
			break;
		}
		/*
		 * Set the Write Protection to permanent
		 */
		retsz = write( wp_fd, &wp_permanent, sizeof(wp_permanent));
		if (retsz != sizeof(wp_permanent) ) {
			LOGERR("GPP WP Failed\n");
			perror("GPP WP: " );
			break;
		}
		/*
		 * Read the Write Protect Status and display it
		 */
		result = gpp_read_wp_status();

	} while (0);

	if ( wpgrp_fd > FILE_ERROR ) {
		close( wpgrp_fd );
	}
	if ( wppart_fd > FILE_ERROR ) {
		close( wppart_fd );
	}
	if ( wp_fd > FILE_ERROR ) {
		close( wp_fd );
	}

	return result;
}

static int rpmb_provision_key() {
	int result = EXIT_FAILURE;
	result = sep_rpmb_provision_key();
	if (result != RPMB_KEY_PROV_SUCCESS) {
		LOGERR("sep_rpmb_provision_key() returned error (0x%0x)\n", result);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static int pmdb_read( sep_pmdb_area_t which_area, const char * const outputfile )
{
	int status = EXIT_SUCCESS;
	uint8_t data[MAX_PMDB_AREA_SIZE_IN_BYTES];


	//
	// Clear the PMDB read buffer with zeroes.
	//
	(void)memset( data, 0, sizeof( data ) );


	//
	// Read data from the PMDB in Chaabi.
	//
	do {
		pmdb_result_t result = sep_pmdb_read( which_area , data, (uint32_t)sizeof(data));
		if (PMDB_SUCCESSFUL != result) {
			LOGERR("Error reading PMDB (0x%X)\n", result);
			status = EXIT_FAILURE;
			break;
		}

		status = write_data_to_file( outputfile, data, sizeof(data) );

	} while (0);


	return status;
}

static int pmdb_write(sep_pmdb_area_t which_area, const char *const inputfile)
{
	int status = EXIT_SUCCESS;
	uint8_t *pData = NULL;
	uint32_t dataSizeInBytes = 0;


	do {
		//Read data from file
		status = read_data_from_file(inputfile, &pData, &dataSizeInBytes);

		if (status == EXIT_FAILURE) {
			LOGERR( "Unable to read data from file: %s\n", inputfile );
			break;
		}

		if ( 0 == dataSizeInBytes ) {
			LOGERR( "Data size of zero bytes is illegal.\n" );

			status = EXIT_FAILURE;

			//
			// Jump to the exit.
			//
			break;
		}

		if ( dataSizeInBytes > MAX_PMDB_AREA_SIZE_IN_BYTES ) {
			LOGERR( "Data size of %u bytes is greater than the maximum "
				"size of %u bytes.\n", dataSizeInBytes,
				MAX_PMDB_AREA_SIZE_IN_BYTES );

			status = EXIT_FAILURE;

			//
			// Jump to the exit.
			//
			break;
		}


		//
		// Write the data to the PMDB.
		//
		pmdb_result_t result = sep_pmdb_write( which_area, pData, dataSizeInBytes);

		if ( PMDB_SUCCESSFUL != result ) {
			LOGERR( "Error writing PMDB (0x%X)\n", result );

			status = EXIT_FAILURE;

			break;
		}

	} while (0);


	if ( NULL != pData ) {
		//
		// Deallocate the data buffer from memory.
		//
		free( pData );

		pData = NULL;
	}


	return status;
}

static int pmdb_lock()
{
	int status = EXIT_SUCCESS;


	pmdb_result_t result = sep_pmdb_lock();

	if (PMDB_SUCCESSFUL != result) {
		LOGERR( "Error locking PMDB write-once area (0x%X)\n", result );

		status = EXIT_FAILURE;
	}


	return status;
}


/*
 * @brief Provisions the ACD with data when a simple ACD write is insufficient
 * @param provSchema ACD provisioning schema to use
 * @param inputFilename ACD provisioning input data filename
 * @param outputFilename ACD provisioning output data filename
 * @return ACD_PROV_SUCCESS Provisioning the ACD succeeded
 * @return <0 Error occurred
 *
 * Typically ACD provision is used when a more complex provision scheme for the
 * provisioning data is required.
 */
static int provision_android_customer_data( const int provisioningSchema,
					    const char * const inputFilename,
					    const char * const outputFilename )
{
	int status = EXIT_FAILURE;
	uint32_t provInputDataSize = 0;
	uint32_t provReturnDataSizeInBytes = 0;
	uint8_t *pProvInputData = NULL;
	void *pProvOutputData = NULL;

	/*
	 * Check the parameters for illegal values.
	 */
	if ( NULL == inputFilename ) {
		LOGERR("APP: Provisioning input filename parameter does not exist.\n" );
		return EXIT_FAILURE;
	}

	if ( NULL == outputFilename ) {
		LOGERR("APP: Provisioning output filename parameter does not exist.\n" );
		return EXIT_FAILURE;
	}

	/*
	 * Read the data to provision from the input data file.
	 */
	status = read_data_from_file( inputFilename, &pProvInputData, &provInputDataSize );
	if (status == EXIT_FAILURE) {
		LOGERR( "Unable to read data from file: %s\n", inputFilename );
		goto PROVISION_ANDROID_CUSTOMER_DATA_EXIT;
	}

	if ( 0 == provInputDataSize ||
	     (provInputDataSize > ACD_MAX_DATA_SIZE_IN_BYTES) ) {
		LOGERR("Provisioning input file size of %u bytes "
		       "is illegal.\n", provInputDataSize );
		goto PROVISION_ANDROID_CUSTOMER_DATA_EXIT;
	}

	/*
	 * Provision the data into the ACD and get the return data.
	 */
	status = provision_customer_data( (uint32_t)provisioningSchema,
					  provInputDataSize,
					  pProvInputData,
					  &provReturnDataSizeInBytes,
					  &pProvOutputData );

	if ( ACD_PROV_SUCCESS != status ) {
		LOGERR("Provisioning ACD failed (error is %d).\n",
		       status );

		goto PROVISION_ANDROID_CUSTOMER_DATA_EXIT;
	}

	/*
	 * Write the ACD provisioning return data to the output
	 * file.
	 */
	if ( provReturnDataSizeInBytes > 0 ) {

		status = write_data_to_file( outputFilename, pProvOutputData, provReturnDataSizeInBytes);
		if (status == EXIT_FAILURE) {
			LOGERR( "Unable to write data to file: %s\n", inputFilename );
			goto PROVISION_ANDROID_CUSTOMER_DATA_EXIT;
		}
	}
	/*
	 * ACD provisioning was successful.
	 */
	status = ACD_PROV_SUCCESS;


PROVISION_ANDROID_CUSTOMER_DATA_EXIT:
	/*
	 * Close the provisioning input and output files and free the input and
	 * output data memory buffers.
	 */
	if ( NULL != pProvInputData ) {
		free( pProvInputData );
		pProvInputData = NULL;
	}

	if ( NULL != pProvOutputData ) {
		free( pProvOutputData );
		pProvOutputData = NULL;
	}


	return ( status );
} /* end provision_android_customer_data() */


int main(int argc, char **argv)
{
	int result = EXIT_SUCCESS;
	uint8_t acdIndex = 0;
	uint16_t dataSize = 0;
	uint16_t dataMaxSize = 0;
	enum {
		HELP = 1,
		DO_ACD_READ,
		DO_ACD_WRITE,
		DO_ACD_LOCK,
		DO_ACD_PROV,
		DO_EPID_PROV,
		DO_PMDB_READ,
		DO_PMDB_WRITE,
		DO_PMDB_LOCK,
		DO_GPP_WP_STATUS,
		DO_GPP_WP_LOCK,
		DO_RPMB_PROV,
#ifdef ACD_WIPE_TEST
		DO_ACD_WIPE,
#endif
		DO_INVALID
	}  what_to_do = DO_INVALID;
	void *pDataBuffer = NULL;
	int provisioningSchema = ACD_MAX_PROVISIONNG_SCHEMA+1;
	char *inputFilename = NULL;
	char *outputFilename = NULL;
	char *sType = NULL;
	sep_pmdb_area_t which_area = -1;

#define ACD_READ_ARG_COUNT 4
#define ACD_WRITE_ARG_COUNT 6
#define ACD_LOCK_ARG_COUNT 2
#ifdef ACD_WIPE_TEST
#define ACD_WIPE_ARG_COUNT 2
#endif
#define ACD_PROV_ARG_COUNT 5
#define EPID_PROV_ARG_COUNT 3
#define PMDB_READ_ARG_COUNT 4
#define PMDB_WRITE_ARG_COUNT 4
#define PMDB_LOCK_ARG_COUNT 2
#define GPP_WP_STATUS_ARG_COUNT 2
#define GPP_WP_LOCK_ARG_COUNT 2
#define RPMB_PROV_ARG_COUNT 2
#define NO_MORE_OPTIONS -1
#define UNKNOWN_OPTION '?'
#define PROGNAME_IDX 0

	/*
	 * Parse the command line options
	 */
	while (1) {
		int opt_index = 0;
		int c;
		static const struct option long_opts[] = {
			{ "acd-read", required_argument, NULL, DO_ACD_READ },
			{ "acd-write", required_argument, NULL, DO_ACD_WRITE },
			{ "acd-lock", no_argument, NULL, DO_ACD_LOCK },
#ifdef ACD_WIPE_TEST
			{ "acd-wipe", no_argument, NULL, DO_ACD_WIPE },
#endif
			{ "acd-prov", required_argument, NULL, DO_ACD_PROV },
			{ "epid-prov", required_argument, NULL, DO_EPID_PROV },
			{ "pmdb-read", required_argument, NULL, DO_PMDB_READ },
			{ "pmdb-write", required_argument, NULL, DO_PMDB_WRITE },
			{ "pmdb-lock", no_argument, NULL, DO_PMDB_LOCK },
			{ "gpp-wp-status", no_argument, NULL, DO_GPP_WP_STATUS },
			{ "gpp-wp-lock", no_argument, NULL, DO_GPP_WP_LOCK },
			{ "rpmb-prov", no_argument, NULL, DO_RPMB_PROV },
			{ "help", no_argument, NULL, HELP },
			{ NULL, no_argument, NULL, 0 }
		};

		c = getopt_long_only( argc, argv, "", long_opts, &opt_index );
		if (c == NO_MORE_OPTIONS)
			break;

		//LOGDBG("optarg=[%s]\n", optarg);
		//LOGDBG("argc=%d\n", argc);
		switch (c) {
		case HELP:
			usage( argv[ PROGNAME_IDX ] );
			return EXIT_SUCCESS;
			break;

		case DO_ACD_READ:
			if (argc != ACD_READ_ARG_COUNT) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			what_to_do = DO_ACD_READ;
			acdIndex = atoi(optarg);
			/*
			 * ACD read or write field index range check.
			 */
			if ( ( acdIndex < ACD_MIN_FIELD_INDEX ) ||
			     ( acdIndex > ACD_MAX_FIELD_INDEX ) ) {
				LOGERR("index out of range: valid range is %d to %d\n",
				       ACD_MIN_FIELD_INDEX, ACD_MAX_FIELD_INDEX);
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			LOGDBG("filename argv[%d]=%s\n", optind, argv[optind]);
			outputFilename = argv[optind++];
			break;

		case DO_ACD_WRITE:
			if (argc != ACD_WRITE_ARG_COUNT ) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			acdIndex = atoi(optarg);
			/*
			 * ACD read or write field index range check.
			 */
			if ( ( acdIndex < ACD_MIN_FIELD_INDEX ) ||
			     ( acdIndex > ACD_MAX_FIELD_INDEX ) ) {
				LOGERR("index out of range: valid range is %d to %d\n",
				       ACD_MIN_FIELD_INDEX, ACD_MAX_FIELD_INDEX);
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			LOGDBG("filename argv[%d]=%s\n", optind, argv[optind]);
			inputFilename = argv[optind++];
			LOGDBG("dataSize: argv[%d]=%s\n", optind, argv[optind]);
			dataSize = atoi(argv[optind++]);
			if (dataSize > ACD_MAX_DATA_SIZE_IN_BYTES) {
				LOGERR("data size out of range\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			LOGDBG("dataMaxSize: argv[%d]=%s\n", optind, argv[optind]);
			dataMaxSize = atoi(argv[optind++]);
			if (dataMaxSize > ACD_MAX_DATA_SIZE_IN_BYTES) {
				LOGERR("data max size out of range\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			} else if (dataSize > dataMaxSize) {
				LOGERR("data size %d > data max size %d \n", dataSize, dataMaxSize);
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			what_to_do = DO_ACD_WRITE;
			break;

		case DO_ACD_LOCK:
			if (argc != ACD_LOCK_ARG_COUNT) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			what_to_do = DO_ACD_LOCK;
			break;
#ifdef ACD_WIPE_TEST
		case DO_ACD_WIPE:
			if (argc != ACD_WIPE_ARG_COUNT) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			what_to_do = DO_ACD_WIPE;
			break;
#endif
		case DO_ACD_PROV:
			if (argc != ACD_PROV_ARG_COUNT) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			what_to_do = DO_ACD_PROV;

			provisioningSchema = atoi(optarg);
			if ( ( provisioningSchema < ACD_MIN_PROVISIONNG_SCHEMA ) ||
			     ( provisioningSchema > ACD_MAX_PROVISIONNG_SCHEMA ) ) {
				LOGERR("Provisioning schema value of %d is illegal.\n",
				       provisioningSchema );
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			LOGDBG("argv[%d]=%s\n", optind, argv[optind]);
			inputFilename = argv[optind++];

			LOGDBG("argv[%d]=%s\n", optind, argv[optind]);
			outputFilename = argv[optind++];
			break;

		case DO_EPID_PROV:
			LOGDBG("argc=%d\n", argc);
			if ( argc != EPID_PROV_ARG_COUNT) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			what_to_do = DO_EPID_PROV;

			inputFilename = optarg;
			LOGDBG("inputFilename is %s\n", inputFilename);
			break;

		case DO_PMDB_READ:
			if (argc != PMDB_READ_ARG_COUNT) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			sType = optarg;
			outputFilename = argv[optind++];
			if ( strncmp(sType, PMDB_WO_AREA_STR, strlen(PMDB_WO_AREA_STR) ) == 0) {
				which_area = SEP_PMDB_WRITE_ONCE;
			} else if ( strncmp(sType, PMDB_WM_AREA_STR, strlen(PMDB_WM_AREA_STR)) == 0 ) {
				which_area = SEP_PMDB_WRITE_MANY;
			}
			else {
				LOGERR(" Invalid read area (%s != WO | WM)\n", sType);
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			what_to_do = DO_PMDB_READ;
			break;

		case DO_PMDB_WRITE:
			if (argc != PMDB_WRITE_ARG_COUNT) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			sType = optarg;
			if ( strncmp(sType, PMDB_WO_AREA_STR, strlen(PMDB_WO_AREA_STR) ) == 0) {
				which_area = SEP_PMDB_WRITE_ONCE;
			} else if ( strncmp(sType, PMDB_WM_AREA_STR, strlen(PMDB_WM_AREA_STR)) == 0 ) {
				which_area = SEP_PMDB_WRITE_MANY;
			}
			else {
				LOGERR(" Invalid read area (%s != WO | WM)\n", sType);
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			inputFilename = argv[optind++];
			what_to_do = DO_PMDB_WRITE;
			break;

		case DO_PMDB_LOCK:
			if (argc != PMDB_LOCK_ARG_COUNT) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			what_to_do = DO_PMDB_LOCK;
			break;

		case DO_GPP_WP_STATUS:
			if (argc != GPP_WP_STATUS_ARG_COUNT) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			what_to_do = DO_GPP_WP_STATUS;
			break;

		case DO_GPP_WP_LOCK:
			if (argc != GPP_WP_LOCK_ARG_COUNT) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			what_to_do = DO_GPP_WP_LOCK;
			break;

		case DO_RPMB_PROV:
			if (argc != RPMB_PROV_ARG_COUNT) {
				LOGERR("incorrect number of arguments\n");
				usage( argv[ PROGNAME_IDX ] );
				return EXIT_FAILURE;
			}
			what_to_do = DO_RPMB_PROV;
			break;

		default:
			usage( argv[ PROGNAME_IDX ] );
			return EXIT_FAILURE;
			break;
		}
	}

	if (argc > optind) {
		int index = 0;
		for (index = optind; index < argc; index++)
			LOGERR("Invalid argument: %s\n", argv[index]);
		usage( argv[ PROGNAME_IDX ] );
		return EXIT_FAILURE;
	}

	/*
	 * Now that the command line options have been processed do
	 * the operation.
	 */
	switch (what_to_do ) {
	case DO_ACD_PROV:
		printf("Starting ACD provisioning operation\n");
		result = provision_android_customer_data( provisioningSchema,
							  inputFilename,
							  outputFilename );
		break;

	case DO_EPID_PROV:
/* TODO Can the conditional compilation be removed without causing a
 * link error? */
#ifdef EPID_ENABLED
		printf("Starting EPID provisioning operation\n");

		/*
		 * Execute the EPID provisioning for Mediavault then return.
		 */
		result = provision_epid( inputFilename );
#else
		printf("EPID provisioning operation is disabled\n");
#endif
		break;

	case DO_ACD_LOCK:
		printf("Starting ACD lock operation\n");
		result = lock_customer_data();
		if (result < 0) {
			LOGERR("ACD lock operaton failed %d\n", result);
			return EXIT_FAILURE;
		}
		result = EXIT_SUCCESS;
		break;
#ifdef ACD_WIPE_TEST
	case DO_ACD_WIPE:
		printf("Starting ACD wipe operation\n");
		result = wipe_customer_data();
		if (result < 0) {
			LOGERR("ACD wipe operaton failed %d\n", result);
			return EXIT_FAILURE;
		}
		result = EXIT_SUCCESS;
		break;
#endif
	case DO_ACD_READ:
		printf("Starting ACD read operation\n");
		result = get_customer_data( acdIndex, &pDataBuffer );
		if (result < 0) {
			LOGERR("read operaton failed %d\n", result);
			if (pDataBuffer != NULL)
				free(pDataBuffer);
			return EXIT_FAILURE;
		}
		result = write_data_to_file( outputFilename, pDataBuffer, result );
		if (NULL != pDataBuffer) {
			free(pDataBuffer);
			pDataBuffer = NULL;
		}
		result = EXIT_SUCCESS;
		break;

	case DO_ACD_WRITE:
		printf("Starting ACD write operation\n");
		uint32_t dataSizeFromFile = 0;
		result = read_data_from_file( inputFilename, (uint8_t **)&pDataBuffer, &dataSizeFromFile );
		if (result == EXIT_FAILURE) {
			LOGERR("could not read data from file %s\n", inputFilename);
			return EXIT_FAILURE;
		}
		if (dataSizeFromFile != dataSize) {
			LOGERR("%s does not contain the correct data size (%u != %u)\n",
			       inputFilename, dataSize, dataSizeFromFile);
			return EXIT_FAILURE;
		}
		result = set_customer_data( acdIndex, dataSize, dataMaxSize, pDataBuffer);
		if (result < 0) {

			if (result == -ACD_WRITE_ERROR_WEAR_OUT_VIOLATION)
				LOGERR("write operaton blocked, due to wear"
				"level protection: %d\n", result);
			else
				LOGERR("write operaton failed. error "
				"code = %d\n", result);

			if (pDataBuffer != NULL)
				free(pDataBuffer);
			return EXIT_FAILURE;
		}

		if (NULL != pDataBuffer) {
			free(pDataBuffer);
			pDataBuffer = NULL;
		}
		result = EXIT_SUCCESS;
		break;

	case DO_PMDB_READ:
		printf("Starting PMDB read operation\n");
		result = pmdb_read(which_area, outputFilename);
		break;

	case DO_PMDB_WRITE:
		printf("Starting PMDB write operation\n");
		result = pmdb_write(which_area, inputFilename);
		break;

	case DO_PMDB_LOCK:
		printf("Starting PMDB lock operation\n");
		result = pmdb_lock();
		break;

	case DO_GPP_WP_STATUS:
		printf("Starting GPP Write Protection status operation\n");
		result = gpp_read_wp_status();
		break;

	case DO_GPP_WP_LOCK:
		printf("Starting GPP Write Protect (lock) operation\n");
		result = gpp_writeprotect();
		break;

	case DO_RPMB_PROV:
		printf("Starting RPMB Key provisioning\n");
		result = rpmb_provision_key();
		break;

	default:
		usage( argv[ PROGNAME_IDX ] );
	}
	if (result == EXIT_SUCCESS) {
		printf("completed successfully\n");
	}
	return result;
} /* end main() */



