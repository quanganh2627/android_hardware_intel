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

#ifndef _MISC_UTILS_HDR_
#define _MISC_UTILS_HDR_

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
			 uint32_t * const pDataSizeInBytes );

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
			const uint32_t dataSizeInBytes );

#endif //_MISC_UTILS_HDR_
