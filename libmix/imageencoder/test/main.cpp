/*
* Copyright (c) 2009-2011 Intel Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <ui/PixelFormat.h>
#include <ui/DisplayInfo.h>
#include <system/window.h>
#include "ImageEncoder.h"

#ifdef GEN_GFX
#include <ufo/graphics.h>
#endif

using namespace android;

inline unsigned long long int current_time(/*bool fixed*/)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (((unsigned long long)tv.tv_usec+(unsigned long long)tv.tv_sec*1000000) * 1000);
}

#define PERF_DEF(counter) unsigned long long int COUNTER_##counter=0;
#define PERF_START(counter, fixed) { COUNTER_##counter = current_time(); }
#define PERF_STOP(counter, fixed) { COUNTER_##counter = current_time() - COUNTER_##counter; }
#define PERF_SET(counter, value) { COUNTER_##counter = value; }
#define PERF_GET(counter) (COUNTER_##counter)

#define MAX_WIDTH 5120
#define MAX_HEIGHT 5120
#define DEFAULT_QUALITY 90
#define DEFAULT_BURST 1
#define YUV420_SAMPLE_SIZE 1.5

static void usage(const char* pname)
{
	fprintf(stderr,
		"\n  USAGE: %s -source [path] -width [value] -height [value] \n"
		"         -output [path] -burst [value] -quality [value] -fix\n\n"
		"  -source: declaring the source's file path.\n"
		"  -width: declaring the source's raw data width (0, 65536].\n"
		"  -height: declaring the source's raw data height (0, 65536].\n"
		"  -output: specifying the output JPEG's file path (.JPG or .jpg).\n"
		"  -surface: specifying the source surface type (0:malloc 1:gralloc).\n"
		"  -burst (optional): enabling continuous encoding times (0, 50].\n"
		"  -quality (optional): setting image quality [0, 100].\n"
		"  -fix (optional): fixing CPU frequency for evaluating performance.\n\n"
		,pname);
}

static bool match_key (char *arg, const char *keyword, int minchars)
{
	register int ca, ck;
	register int nmatched = 0;

	while ((ca = *arg++) != '\0') {
		if ((ck = *keyword++) == '\0')
			return false; /* arg longer than keyword, mismatch */
		if (isupper(ca)) /* force arg to lcase (assume ck is already) */
			ca = tolower(ca);
		if (ca != ck)
			return false; /* mismatch */
		nmatched++; /* count matched characters */
	}

	if (nmatched < minchars)
		return false; /* arg shorter than keyword, mismatch */

	return true; /* Match */
}

int main(int argc, char** argv)
{
	const char *pname = argv[0];
	int argn;
	char *arg;

	/* Parameter variables */
	char *source_name = NULL;
	char *output_name = (char *)"./output.jpg";
	int surface_type = 1;
	int quality = DEFAULT_QUALITY;
	int burst = DEFAULT_BURST;
	int width = 0, height = 0;
	bool fix_cpu_frequency = false;
	unsigned int fourcc_format = 0;

	/* Internal variables*/
	int i, j;
	int stride = 0;
	char final_output_name[128] = "\0";
	int source_fd = -1;
	int output_fd = -1;
	unsigned int source_size = 0, source_buffer_size = 0;
	unsigned int output_size = 0, output_buffer_size = 0;
	unsigned int read_size = 0, write_size = 0;
	GraphicBuffer *gralloc_buffer = NULL;
	void *gralloc_handle = NULL;
	void *source_buffer = NULL, *output_buffer = NULL;
	void *aligned_source_buffer = NULL, *current_position = NULL;
	void *surface_buffer = NULL, *surface_buffer_ptr = NULL;
	int status;
	int image_seq = -1;
	IntelImageEncoder image_encoder;

	/* For CPU frequency fixing */
	FILE *cpu_online_nr_fd = NULL, *cpu_available_max_fd = NULL, *cpu_available_min_fd = NULL;
	FILE *cpu_scaling_max_fd = NULL, *cpu_scaling_min_fd = NULL, *cpu_cur_fd = NULL;
	unsigned int cpu_online_nr = 0, cpu_available_max = 1000000, cpu_available_min = 0, cpu_cur = 0;

	/* For performance logging */
	PERF_DEF(init_driver_time);
	PERF_DEF(create_source_time);
	PERF_DEF(create_context_time);
	PERF_DEF(prepare_encoding_time);
	PERF_DEF(encode_time);
	PERF_DEF(term_driver_time);
	unsigned long long int total_time = 0;
	double compression_rate = 0;

	/* Get the input parameters */
	if (1 >= argc) {
		usage(pname); /* No argument */
		return 1;
	}

	for (argn = 1; argn < argc; argn++) {
		arg = argv[argn];
		if (*arg != '-') {
			/* Every argument should begin with a '-' */
			usage(pname);
			fprintf(stderr, "Every argument should begin with a '-'!\n");
			return 1;
		}
		arg++;

		if (match_key(arg, "width", strlen("width"))) {
			if (++argn >= argc) {
				usage(pname); /* "-width" should be followed by a specified width value*/
				fprintf(stderr, "-width should be followed by a specified width value!\n");
				return 1;
			}

			if ((1 != sscanf(argv[argn], "%d", &width)) || (width <= 0)) {
				usage(pname); /* Invalid width */
				fprintf(stderr, "Invalid width!\n");
				return 1;
			}

			if ((width>MAX_WIDTH) || (width%2)) {
				usage(pname); /* Unsupported width */
				fprintf(stderr, "Unsupported width: %d!\n", width);
				return 1;
			}
		} else if (match_key(arg, "height", strlen("height"))) {
			if (++argn >= argc) {
				usage(pname); /* "-height" should be followed by a specified height value*/
				fprintf(stderr, "-height should be followed by a specified height value!\n");
				return 1;
			}

			if ((1 != sscanf(argv[argn], "%d", &height)) || (height <= 0)) {
				usage(pname); /* Invalid height */
				fprintf(stderr, "Invalid height!\n");
				return 1;
			}

			if ((MAX_HEIGHT<height) || (height%2)) {
				usage(pname); /* Unsupported height */
				fprintf(stderr, "Unsupported height: %d!\n", height);
				return 1;
			}
		} else if (match_key(arg, "source", strlen("source"))) {
			if (++argn >= argc) {
				usage(pname); /* "-source" should be followed by a specified source path */
				fprintf(stderr, "-source should be followed by a specified source path!\n");
				return 1;
			}
			source_name = argv[argn];
		} else if (match_key(arg, "output", strlen("output"))) {
			if (++argn >= argc) {
				usage(pname); /* "-output" should be followed by a specified output file path */
				fprintf(stderr, "-output should be followed by a specified output file path!\n");
				return 1;
			}
			output_name = argv[argn];
			if ((strlen(output_name) <= 4) ||
				(strcmp(output_name+strlen(output_name)-4, ".jpg") &&
				strcmp(output_name+strlen(output_name)-4, ".JPG"))) {
				usage(pname); /* Invalid output file name */
				fprintf(stderr, "Invalid output file name: %s!\n", output_name);
				return 1;
			}
                } else if (match_key(arg, "surface", strlen("surface"))) {
                        if (++argn >= argc) {
                                usage(pname); /* "surface" should be followed by a type */
                                fprintf(stderr, "-sruface should be followed by a type!\n");
                                return 1;
                        }

                        if ((1 != sscanf(argv[argn], "%d", &surface_type)) ||
			(surface_type < 0) || (surface_type > 1)) {
                                usage(pname); /* Invalid surface type */
                                fprintf(stderr, "Invalid surface type!\n");
                                return 1;
                        }
#ifndef GEN_GFX
						surface_type = 1;
#endif
		} else if (match_key(arg, "burst", strlen("burst"))) {
			if (++argn >= argc) {
				usage(pname); /* "burst" should be followed by a quality value */
				fprintf(stderr, "-burst should be followed by a specified encoding times!\n");
				return 1;
			}

			if ((1 != sscanf(argv[argn], "%d", &burst)) || (burst < 0) || (burst > 50)) {
				usage(pname); /* Invalid burst times */
				fprintf(stderr, "Invalid burst times!\n");
				return 1;
			}
		} else if (match_key(arg, "quality", strlen("quality"))) {
			if (++argn >= argc) {
				usage(pname); /* "quality" should be followed by a quality value */
				fprintf(stderr, "-quality should be followed by a specified quality value!\n");
				return 1;
			}

			if ((1 != sscanf(argv[argn], "%d", &quality)) || (quality < 0) || (quality > 100)) {
				usage(pname); /* Invalid quality value */
				fprintf(stderr, "Invalid quality value!\n");
				return 1;
			}
		} else if (match_key(arg, "fix", strlen("fix"))) {
			fix_cpu_frequency = true;
		} else {
			usage(pname); /* Unsupported argument */
			fprintf(stderr, "Unsupported argument: %s!\n", arg);
			return 1;
		}
	}

	/* Validate the input parameters */
	if ((0 == width) || (0 == height)) {
		usage(pname);
		fprintf(stderr, "Width or height unset!\n");
		return 1;
	}

	if (NULL == source_name) {
		usage(pname);
		fprintf(stderr, "Source file path unset!\n");
		return 1;
	}

	/* Get the source image data */
	source_fd = open(source_name, O_RDONLY, 0664);
	if (-1 == source_fd) {
		fprintf(stderr, "Error opening source file: %s (%s)!\n", source_name, strerror(errno));
		return 1;
	}

	source_size = width * height * YUV420_SAMPLE_SIZE;

	if (0 == surface_type) { /* malloc */
		/* TopazHP requires stride must be an integral multiple of 64. */
		stride = (width+0x3f) & (~0x3f);
		source_buffer_size = stride * height * YUV420_SAMPLE_SIZE;

		source_buffer = malloc(source_buffer_size+4096);
		if (NULL == source_buffer) {
			fprintf(stderr, "Fail to allocate source buffer: %d(%s)!\n", errno, strerror(errno));
			close(source_fd);
			return 1;
		}
		memset(source_buffer, 0, source_buffer_size+4096);
		aligned_source_buffer = (void *)((unsigned long)source_buffer -
					((unsigned long)source_buffer)%4096 + 4096);
	} else { /* gralloc */
		/* TopazHP requires stride must be an integral multiple of 64. */
		stride = (width+0x3f) & (~0x3f);

		gralloc_buffer = new GraphicBuffer(stride, height,
#ifdef GEN_GFX
				HAL_PIXEL_FORMAT_NV12_LINEAR_INTEL,
#else
				VA_FOURCC_NV12,
#endif
				GraphicBuffer::USAGE_HW_RENDER);
		if (NULL == gralloc_buffer) {
			fprintf(stderr, "Allocating GraphicBuffer failed!\n");
			close(source_fd);
			return 1;
		}
		stride = (gralloc_buffer->getNativeBuffer())->stride;
		gralloc_handle = (void *)((gralloc_buffer->getNativeBuffer())->handle);

		if(gralloc_buffer->lock(GRALLOC_USAGE_SW_WRITE_RARELY, &aligned_source_buffer)) {
			fprintf(stderr, "Locking GraphicBuffer failed!\n");
			close(source_fd);
			return 1;
		}
	}

	current_position = aligned_source_buffer;
	for (i=0; i<height; ++i) { /* Y Component */
		read_size += read(source_fd, current_position, width);
		current_position = (void *)((unsigned long)current_position + stride);
	}
	for (i=0; i<(height/2); ++i) { /* UV Component */
		read_size += read(source_fd, current_position, width);
		current_position = (void *)((unsigned long)current_position + stride);
	}

	close(source_fd);

	if (1 == surface_type) {
		aligned_source_buffer = NULL;
		gralloc_buffer->unlock();
	}

	if (read_size != source_size) {
		fprintf(stderr, "Incorrect source file size: %d(%s)!\n", read_size, strerror(errno));
		fprintf(stderr, "The correct size should be : %d.\n", source_size);
		if (source_buffer) free(source_buffer);
		return 1;
	}

	/* Try to fix CPUs' frequency to the maximum available */
	if (fix_cpu_frequency) {
		cpu_online_nr_fd = fopen("/sys/devices/system/cpu/online", "r");
		assert(cpu_online_nr_fd != NULL);
		fscanf(cpu_online_nr_fd, "0-%u", &cpu_online_nr);
		assert(cpu_online_nr != 0);
		fclose(cpu_online_nr_fd);

		cpu_available_max_fd  = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
		if (NULL == cpu_available_max_fd) {
			unsigned char one_line[32] = {};
			unsigned char one_segment[32] = {};
			int readed = 0;

			cpu_available_max_fd  = fopen("/proc/cpuinfo", "r");
			assert(cpu_available_max_fd != 0);

			i = strlen("cpu MHz");
			j = -1;  /* Haven't find the cpu MHz line */
			do {
				if (fread(&one_line[0], 1, 1, cpu_available_max_fd) != 1) {
					break;
				} else if ('\n' == one_line[0]) {
					readed = fread(one_line, 1, 24, cpu_available_max_fd);
					if ((24 == readed) &&
					(0 == strncmp((const char *)one_line, (const char *)"cpu MHz", i))) {
						/* Find the cpu MHz line */
						j = 0;
						break;
					} else if (readed > 0) {
						fseek(cpu_available_max_fd, 0-readed, SEEK_CUR);
					}
				}
			} while (1);

			if (0 == j) {
				while (one_line[i] != ':') {
					++i;
				}
				++i; /* The space bewteen ':' and a freq value */
				while (one_line[i] != '.') {
					one_segment[j++] = one_line[i++];
				}
				one_segment[j] = '\0';
				cpu_available_max = atoi((const char *)one_segment) * 1000;
			}
			fclose(cpu_available_max_fd);

			if (0 == cpu_available_max) {
				cpu_available_max = 1000000;
				fprintf(stderr, "\nCan't find CPU frequecency value and we assume 1.0GHz.\n");
			}

			printf("\n%u CPU(s) online, whose unscalable frequency is: %u.\n",
				cpu_online_nr+1, cpu_available_max);
		} else {
			fscanf(cpu_available_max_fd, "%u", &cpu_available_max);
			assert(cpu_available_max != 0);
			fclose(cpu_available_max_fd);

			cpu_available_min_fd  = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq",
							"r");
			assert(cpu_available_min_fd != NULL);
			fscanf(cpu_available_min_fd, "%u", &cpu_available_min);
			assert(cpu_available_min != 0);
			fclose(cpu_available_min_fd);

			printf("\n%u CPU(s) online, whose MAX/MIN available frequency is: %u/%u.\n",
				cpu_online_nr+1, cpu_available_max, cpu_available_min);

			for (i=0; i<=(int)cpu_online_nr; ++i) {
				char fd_name[64];

				sprintf(fd_name, "/sys/devices/system/cpu/cpu%u/cpufreq/scaling_max_freq", i);
				cpu_scaling_max_fd  = fopen(fd_name, "w");
				if (0 == i) {
					assert(cpu_scaling_max_fd != NULL);
				} else if ((i>0) && (NULL==cpu_scaling_max_fd)) {
					fprintf(stderr, "No sysfs attribute to fix cpu%u's frequency!\n", i);
					break;
				}
				fprintf(cpu_scaling_max_fd, "%u", cpu_available_max);
				fclose(cpu_scaling_max_fd);

				sprintf(fd_name, "/sys/devices/system/cpu/cpu%u/cpufreq/scaling_min_freq", i);
				cpu_scaling_min_fd  = fopen(fd_name, "w");
				if (0 == i) {
					assert(cpu_scaling_min_fd != NULL);
				} else if ((i>0) && (NULL== cpu_scaling_min_fd)) {
					fprintf(stderr, "No sysfs attribute to fix cpu%u's frequency!\n", i);
					break;
				}
				fprintf(cpu_scaling_min_fd, "%u", cpu_available_max);
				fclose(cpu_scaling_min_fd);

				sprintf(fd_name, "/sys/devices/system/cpu/cpu%u/cpufreq/scaling_cur_freq", i);
				cpu_cur_fd  = fopen(fd_name, "r");
				assert(cpu_cur_fd != NULL);
				fscanf(cpu_cur_fd, "%u", &cpu_cur);
				assert(cpu_cur == cpu_available_max);
				fclose(cpu_cur_fd);

				printf("cpu%u's frequency is fixed to %u.\n", i, cpu_available_max);

				cpu_scaling_max_fd = cpu_scaling_min_fd = cpu_cur_fd = NULL;
				cpu_cur = 0;
			}
		}
	}
	cpu_available_max = 1000000;

	/* Print encoding settings */
	printf("\n[INPUT]\n");
	printf("Source: %s\n", source_name);
	printf("Width: %d\n", width);
	printf("Height: %d\n", height);
	printf("Output: %s\n", output_name);
	if (0 == surface_type)
		printf("Surface: malloc\n");
	else
		printf("Surface: gralloc\n");
	printf("Burst: %d times\n", burst);
	printf("Quality: %d\n", quality);
	if (true == fix_cpu_frequency)
		printf("Fix CPU frequency: true\n");
	else
		printf("Fix CPU frequency: false\n");
	printf("\n[OUTPUT]\n");

	/* Initialize encoder */
	PERF_START(init_driver_time, fix_cpu_frequency);
	status = image_encoder.initializeEncoder();
	PERF_STOP(init_driver_time, fix_cpu_frequency);
	if (status != 0) {
		fprintf(stderr, "initializeEncoder failed (%d)!\n", status);
		if (source_buffer) free(source_buffer);
		return 1;
	}

	/* Create a source surface*/
	PERF_START(create_source_time, fix_cpu_frequency);
	status = image_encoder.createSourceSurface(surface_type? SURFACE_TYPE_GRALLOC:SURFACE_TYPE_USER_PTR,
							surface_type? gralloc_handle:aligned_source_buffer,
							width, height,
							stride, VA_RT_FORMAT_YUV420,
							&image_seq);
	PERF_STOP(create_source_time, fix_cpu_frequency);
	if (status != 0) {
		fprintf(stderr, "createSourceSurface failed (%d)!\n", status);
		if (source_buffer) free(source_buffer);
		image_encoder.deinitializeEncoder();
		return 1;
	}

	/* Create context*/
	PERF_START(create_context_time, fix_cpu_frequency);
	status = image_encoder.createContext(image_seq, &output_buffer_size);
	PERF_STOP(create_context_time, fix_cpu_frequency);
	if (status != 0) {
		fprintf(stderr, "createContext failed (%d)!\n", status);
		if (source_buffer) free(source_buffer);
		image_encoder.deinitializeEncoder();
		return 1;
	}

	output_buffer = malloc(output_buffer_size);
	if (NULL == output_buffer) {
		fprintf(stderr, "Fail to allocate output buffer: %d(%s)!\n", errno, strerror(errno));
		if (source_buffer) free(source_buffer);
		image_encoder.deinitializeEncoder();
		return 1;
	}

	printf("Init driver: %.3fms\n", (double)PERF_GET(init_driver_time)/cpu_available_max);
	printf("Create source: %.3fms\n", (double)PERF_GET(create_source_time)/cpu_available_max);
	printf("Create context: %.3fms\n", (double)PERF_GET(create_context_time)/cpu_available_max);

	/* Do the encoding */
	for (i=0; i<burst; ++i) {
		PERF_START(prepare_encoding_time, fix_cpu_frequency);
		status = image_encoder.encode(image_seq, quality);
		PERF_STOP(prepare_encoding_time, fix_cpu_frequency);
		if (status != 0) {
			fprintf(stderr, "encode failed (%d)!\n", status);
			if (source_buffer) free(source_buffer);
			free(output_buffer);
			image_encoder.deinitializeEncoder();
			return 1;
		}

		PERF_START(encode_time, fix_cpu_frequency);
		status = image_encoder.getCodedSize(&output_size);
		if (status != 0) {
			fprintf(stderr, "getCodedSize failed (%d)!\n", status);
			if (source_buffer) free(source_buffer);
			free(output_buffer);
			image_encoder.deinitializeEncoder();
			return 1;
		}

		status = image_encoder.getCoded(output_buffer, output_buffer_size);
		if (status != 0) {
			fprintf(stderr, "getCoded failed (%d)!\n", status);
			if (source_buffer) free(source_buffer);
			free(output_buffer);
			image_encoder.deinitializeEncoder();
			return 1;
		}
		PERF_STOP(encode_time, fix_cpu_frequency);

		printf("Prepare encoding: %.3fms\n", (double)PERF_GET(prepare_encoding_time)/cpu_available_max);
		printf("Encode and stitch coded: %.3fms\n", (double)PERF_GET(encode_time)/cpu_available_max);

		if (0 == i) {
			sprintf(final_output_name, "%s", output_name);
		} else {
			sprintf(final_output_name, "%d.%s", i+1, output_name);
		}

		output_fd = open(final_output_name, O_WRONLY | O_CREAT | O_TRUNC, 0664);
		if (-1 == output_fd) {
			fprintf(stderr, "Error opening output file: %s (%s)!\n",
				final_output_name, strerror(errno));
			if (source_buffer) free(source_buffer);
			free(output_buffer);
			image_encoder.deinitializeEncoder();
			return 1;
		}

		write_size = write(output_fd, output_buffer, output_size);
		close(output_fd);
		if (write_size != output_size) {
			fprintf(stderr, "Fail to write coded data to output file: %d(%s)!\n",
				write_size , strerror(errno));
			if (source_buffer) free(source_buffer);
			free(output_buffer);
			image_encoder.deinitializeEncoder();
			return 1;
		}

		output_fd = -1;
	}

	if (source_buffer) free(source_buffer);
	free(output_buffer);

	/* Deinitialize encoder */
	PERF_START(term_driver_time, fix_cpu_frequency);
	status = image_encoder.deinitializeEncoder();
	PERF_STOP(term_driver_time, fix_cpu_frequency);
	if (status != 0) {
		fprintf(stderr, "deinitializeEncoder failed (%d)!\n", status);
		return 1;
	}

	printf("Term driver: %.3fms\n", (double)PERF_GET(term_driver_time)/cpu_available_max);

	/* Print encoding results */
	if (1 == burst) {
		total_time = PERF_GET(init_driver_time) + PERF_GET(create_source_time) +
				PERF_GET(create_context_time) + PERF_GET(prepare_encoding_time) +
				PERF_GET(encode_time) + PERF_GET(term_driver_time);
		printf("[SUM]Total: %.3fms\n", (double)total_time/cpu_available_max);
	}

	compression_rate = ((double)output_size) / ((double)source_size);
	compression_rate = (1 - compression_rate) * 100;
	printf("[SUM]Compression rate: %.2f%% (%d bytes : %d bytes)\n\n",
		compression_rate, output_size, source_size);

	/* Try to restore CPUs' frequency */
	if (fix_cpu_frequency) {
		if (cpu_available_min != 0) {
			for (i=0; i<=(int)cpu_online_nr; ++i) {
				char fd_name[64];

				sprintf(fd_name, "/sys/devices/system/cpu/cpu%u/cpufreq/scaling_min_freq", i);
				cpu_scaling_min_fd  = fopen(fd_name, "w");
				if (0 == i) {
					assert(cpu_scaling_min_fd != NULL);
				} else if ((i>0) && (NULL== cpu_scaling_min_fd)) {
					fprintf(stderr, "No sysfs attribute to restore cpu%u's frequency!\n", i);
					break;
				}

				fprintf(cpu_scaling_min_fd, "%u", cpu_available_min);
				fclose(cpu_scaling_min_fd);

				printf("cpu%u's frequency is restored.\n", i);

				cpu_scaling_min_fd = NULL;
			}
		}
	}

	return 0;
}

