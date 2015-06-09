/**********************************************************************
 * Copyright 2009 - 2013 (c) Intel Corporation. All rights reserved.
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
/* define LOG related macros before including the log header file */
#define LOG_TAG      "UTILS-lib"

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "chaabi_error_codes.h"

#define LOG_STYLE    LOG_STYLE_PRINTF
#include "sepdrm-log.h"

/*
 * @brief Reads data from an input file and copies it into a memory buffer
 * @param pFilename Input filename to read the data from
 * @param pDataBuffer Pointer to the allocated memory buffer that will store the file data
 * @param pDataSizeInBytes Return value of the size of the data buffer in bytes
 * @return DX_SUCCESS Data from the input file has been copied into the new data buffer
 * @return DX_ERROR Data from the input file was not read
 *
 * This function will allocate a memory buffer that is the same size as the
 * input file and it will then copy the data from the input file into the memory
 * buffer. The size of the buffer in bytes will be returned through the pointer
 * pDataSizeInBytes. If an error occurred then the data buffer pointer value
 * will be NULL and the data buffer size will be zero.
 *
 * NOTE: This function will allocate a memory buffer to store the data read from
 * the input file. It is the caller's responsibility to free the memory buffer
 * that has been allocated by this function. Memory is allocated with the
 * calloc() function, so use the free() function to free the memory.
 */
int read_data_from_file( const char * const pFilename,
			 uint8_t ** const pDataBuffer,
			 uint32_t * const pDataSizeInBytes )
{
	int result = EXIT_FAILURE;
	int status = 0;
	off_t fileSizeInBytes;
	struct stat fileInfo;
	int inputFileHandle = 0;
	ssize_t bytesRead = 0;


	//
	// Check parameters for illegal values.
	//
	if ( NULL == pFilename ) {
		LOGERR( "Input filename does not exist.\n" );

		return ( EXIT_FAILURE );
	}

	if ( NULL == pDataBuffer ) {
		LOGERR( "Data buffer pointer is NULL.\n" );

		return ( EXIT_FAILURE );
	}

	if ( NULL == pDataSizeInBytes ) {
		LOGERR( "Data buffer size pointer is NULL.\n" );

		return ( EXIT_FAILURE );
	}


	//
	// Open the input data file for reading only.
	//
	inputFileHandle = open( pFilename, O_RDONLY );

	if ( inputFileHandle < 0 ) {
		LOGERR( "Unable to open the input file %s.\n", pFilename );

		result = EXIT_FAILURE;

		goto GET_DATA_FROM_FILE_EXIT;
	}


	//
	// Get the size of the input data file.
	//
	memset( &fileInfo, 0, sizeof( fileInfo ) );

	status = fstat( inputFileHandle, &fileInfo );

	if ( status < 0 ) {
		LOGERR( "Unable to get the size of the input file %s.\n",
			pFilename );

		result = EXIT_FAILURE;

		goto GET_DATA_FROM_FILE_EXIT;
	}

	if ( 0 == fileInfo.st_size ) {
		LOGERR( "Input file %s is empty.\n", pFilename );

		result = EXIT_FAILURE;

		goto GET_DATA_FROM_FILE_EXIT;
	}

	*pDataSizeInBytes = (uint32_t)fileInfo.st_size;


	//
	// Allocate the memory buffer for the input file data.
	//
	*pDataBuffer = (uint8_t *)calloc( (size_t)*pDataSizeInBytes, sizeof( uint8_t ) );

	if ( NULL == *pDataBuffer ) {
		LOGERR( "Unable to allocate %u bytes of memory for the input "
			"data.\n", *pDataSizeInBytes );

		result = EXIT_FAILURE;

		goto GET_DATA_FROM_FILE_EXIT;
	}


	//
	// Copy the data from the input file into the memory buffer.
	//
	bytesRead = read( inputFileHandle, *pDataBuffer, (size_t)*pDataSizeInBytes );

	if ( bytesRead != (ssize_t)*pDataSizeInBytes ) {
		LOGERR( "Reading data from the input file %s failed.\n",
			pFilename );

		result = EXIT_FAILURE;

		goto GET_DATA_FROM_FILE_EXIT;
	}


	//
	// Reading data from the input file succeeded.
	//
	result = EXIT_SUCCESS;


GET_DATA_FROM_FILE_EXIT:
	if (  0 <= inputFileHandle ) {
		//
		// Close the open input file.
		//
		close( inputFileHandle );

		inputFileHandle = -1;
	}


	if ( EXIT_SUCCESS != result ) {
		//
		// Reading data from the input file was not successful so the
		// data size is zero.
		//
		*pDataSizeInBytes = 0;


		//
		// Deallocate the memory buffer if it was allocated.
		//
		if ( NULL != *pDataBuffer ) {
			free( *pDataBuffer );

			*pDataBuffer = NULL;
		}
	}


	return ( result );
} // end read_data_from_file()


/*
 * @brief Writes data from a memory data buffer to an output file
 * @param pFilename Output filename to write the data to
 * @param pDataBuffer Pointer to the memory buffer of data to write
 * @param pDataSizeInBytes Number of data bytes to write
 * @return EXIT_SUCCESS Data from the memory buffer has been written to the output file
 * @return EXIT_FAILURE Data from the memory buffer has not been written to the output file
 */
int write_data_to_file( const char * const pFilename,
			const uint8_t * const pDataBuffer,
			const uint32_t dataSizeInBytes )
{
	int result = EXIT_FAILURE;
	int outputFileHandle = 0;
	ssize_t bytesWritten = 0;


	//
	// Check parameters for illegal values.
	//
	if ( NULL == pFilename ) {
		LOGERR( "Output filename does not exist.\n" );

		return ( EXIT_FAILURE );
	}

	if ( NULL == pDataBuffer ) {
		LOGERR( "Data memory buffer pointer is NULL.\n" );

		return ( EXIT_FAILURE );
	}

	if ( 0 == dataSizeInBytes ) {
		LOGERR( "Data size is zero.\n" );

		return ( EXIT_FAILURE );
	}


	//
	// Open the output data file.
	//
	outputFileHandle = open( pFilename, ( O_WRONLY | O_CREAT | O_TRUNC ),
				(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) );

	if ( outputFileHandle < 0 ) {
		LOGERR( "Unable to open the output data file %s.\n", pFilename );

		result = EXIT_FAILURE;

		goto WRITE_DATA_TO_FILE_EXIT;
	}


	//
	// Write the data to the output file.
	//
	bytesWritten = write( outputFileHandle, pDataBuffer, (size_t)dataSizeInBytes );

	if ( bytesWritten != (ssize_t)dataSizeInBytes ) {
		LOGERR( "Writing data to output file %s failed.\n", pFilename );

		result = EXIT_FAILURE;

		goto WRITE_DATA_TO_FILE_EXIT;
	}


	//
	// Writing data to the output file succeeded.
	//
	result = EXIT_SUCCESS;


WRITE_DATA_TO_FILE_EXIT:
	if ( 0 <= outputFileHandle ) {
		//
		// Close the open output data file.
		//
		close( outputFileHandle );

		outputFileHandle = -1;
	}


	return ( result );
} // end write_data_to_file()
