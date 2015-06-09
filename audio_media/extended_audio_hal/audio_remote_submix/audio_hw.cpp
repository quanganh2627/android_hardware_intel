/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "r_submix_intel"
//#define LOG_NDEBUG 0

//#define VERY_VERBOSE_LOGGING
#ifdef VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <media/nbaio/MonoPipe.h>
#include <media/nbaio/MonoPipeReader.h>
#include <media/AudioBufferProvider.h>
#include <media/nbaio/roundup.h>

#include <utils/String8.h>
#include <media/AudioParameter.h>

extern "C" {

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>
namespace android {

#define MAX_PIPE_DEPTH_IN_FRAMES    (1024*32)
// The duration of MAX_READ_ATTEMPTS * READ_ATTEMPT_SLEEP_MS must be strictly inferior to
//   the duration of a record buffer at the current record sample rate (of the device, not of
//   the recording itself). Here we have:
//      3 * 5ms = 15ms < 1024 frames * 1000 / 48000 = 21.333ms
#define MAX_READ_ATTEMPTS           3
#define READ_ATTEMPT_SLEEP_MS       5 // 5ms between two read attempts when pipe is empty
#define DEFAULT_RATE_HZ             48000 // default sample rate

#ifdef SURROUND_SUBMIX
  #define DEFAULT_PERIOD_SIZE       1024
  #define DEFAULT_PERIOD_COUNT      4
  #define MAX_SURROUND_BUFFER_SIZE  16384
  // this is intentional
  #define MAX_NORMAL_BUFFER_SIZE    16384
#else
  #define DEFAULT_PERIOD_SIZE       1024
  #define DEFAULT_PERIOD_COUNT      4
#endif

struct submix_config {
    audio_format_t format;
    audio_channel_mask_t channel_mask;
    unsigned int rate; // sample rate for the device
    unsigned int period_size; // size of the audio pipe is period_size * period_count in frames
    unsigned int period_count;
};

struct submix_audio_device {
    struct audio_hw_device device;
    bool output_standby;
    bool input_standby;
    submix_config config;
    submix_config config_out;
    // Pipe variables: they handle the ring buffer that "pipes" audio:
    //  - from the submix virtual audio output == what needs to be played
    //    remotely, seen as an output for AudioFlinger
    //  - to the virtual audio source == what is captured by the component
    //    which "records" the submix / virtual audio source, and handles it as needed.
    // A usecase example is one where the component capturing the audio is then sending it over
    // Wifi for presentation on a remote Wifi Display device (e.g. a dongle attached to a TV, or a
    // TV with Wifi Display capabilities), or to a wireless audio player.
    sp<MonoPipe>       rsxSink;
    sp<MonoPipeReader> rsxSource;

    // device lock, also used to protect access to the audio pipe
    pthread_mutex_t lock;
    // remote bgm state - true, false
    char *bgmstate;
    //bgm player session
    char* bgmsession;

    //counter to keep track of the number of active streams (mixer/direct..)
    int num_of_streams;
    //mixer elements to mix direct and mixer outputs
    char* mixbuffer_surround;
    char* mixbuffer_normal;
    int mixbuffer_surround_bytes;
    int mixbuffer_normal_bytes;
    char* mixbuffer;
    bool submixThreadStart;
    //submix thread member to write to the monopipe in a different thread context
    // other than audio flinger
    pthread_t sbm_thread;
    // flag to check whether the remote sink supports multichannel or not
    bool surround_sink_connected;
};

struct submix_stream_out {
    struct audio_stream_out stream;
    struct submix_audio_device *dev;
    submix_config config_out;
};

struct submix_stream_in {
    struct audio_stream_in stream;
    struct submix_audio_device *dev;
    bool output_standby; // output standby state as seen from record thread

    // wall clock when recording starts
    struct timespec record_start_time;
    // how many frames have been requested to be read
    int64_t read_counter_frames;
};


/*keep track of audio availability in AV playback with BGM*/
static char* gbgm_audio = "true";

//-------------submix thread start-----------

#define SUBMIX_THREAD_NAME "submixThread"

static int write_to_pipe(submix_audio_device *rsxadev, const void* buffer,
                         size_t bytes)
{
    ssize_t written_frames = 0;
    size_t frame_size = 0;
    size_t frames = 0;

    pthread_mutex_lock(&rsxadev->lock);

    frame_size = popcount(rsxadev->config.channel_mask) * sizeof(int16_t);
    //JIC
    if(!frame_size)
       frame_size = 4;//stereo*samplesize
    frames = bytes / frame_size;

    ALOGVV("write_to_pipe:frame_size = %d frames = %d bytes = %d",frame_size,frames,bytes);

    sp<MonoPipe> sink = rsxadev->rsxSink.get();
    if (sink != 0) {
        if (sink->isShutdown()) {
            sink.clear();
            pthread_mutex_unlock(&rsxadev->lock);
            // the pipe has already been shutdown, this buffer will be lost but we must
            //   simulate timing so we don't drain the output faster than realtime
            int rate = (rsxadev->config.rate ? rsxadev->config.rate : DEFAULT_RATE_HZ);
            usleep(frames * 1000000 / rate);
            return bytes;
        }
    } else {
        pthread_mutex_unlock(&rsxadev->lock);
        ALOGE("write_to_pipe without a pipe!");
        ALOG_ASSERT("write_to_pipe without a pipe!");
        return 0;
    }

    pthread_mutex_unlock(&rsxadev->lock);

    written_frames = sink->write(buffer, frames);

    if (written_frames < 0) {
        if (written_frames == (ssize_t)NEGOTIATE) {
            ALOGE("write_to_pipe() write to pipe returned NEGOTIATE");

            pthread_mutex_lock(&rsxadev->lock);
            sink.clear();
            pthread_mutex_unlock(&rsxadev->lock);

            written_frames = 0;
            return 0;
        } else {
            // write() returned UNDERRUN or WOULD_BLOCK, retry
            ALOGE("write_to_pipe() write to pipe returned unexpected %d", written_frames);
            written_frames = sink->write(buffer, frames);
        }
    }
#ifdef _DUMP
   {
     FILE* fp =0;
     fp = fopen("/data/writedump.pcm","ab+");
     if(fp){
       int dumpbytes = fwrite(buffer,1,bytes,fp);
       ALOGD("dump data bytes = %d",dumpbytes);
       fclose(fp);
       fp =0;
     }
   }
#endif

    pthread_mutex_lock(&rsxadev->lock);
    sink.clear();
    pthread_mutex_unlock(&rsxadev->lock);

    if (written_frames < 0) {
        ALOGE("write_to_pipe() failed writing to pipe with %d", written_frames);
        return 0;
    } else {
        ALOGV("write_to_pipe() wrote %u bytes)", written_frames * frame_size);
        return written_frames * frame_size;
    }
}

static void mix_buffer(char* outbuf, char *mbuf1, char* mbuf2, int bytes)
{
   int i = 0;
   for(i = 0; i < MAX_SURROUND_BUFFER_SIZE; i++) {
       //todo : update a proper mixer logic
       outbuf[i] = (mbuf1[i]+mbuf2[i])/2;
   }
   return;
}

static void *submix_loop(void *param)
{
   int bytes = 0;
   struct submix_audio_device *rsxadev = (struct submix_audio_device *)param;
   rsxadev->sbm_thread = pthread_self();
   while(1) {
     //ALOGV("inside submix_loop dev = %x thread = %x",rsxadev,rsxadev->sbm_thread);

     pthread_mutex_lock(&rsxadev->lock);

     if(!rsxadev->submixThreadStart) {
        ALOGV("submix streams closed - exit submix thread");
        pthread_mutex_unlock(&rsxadev->lock);
        break;
     }

#ifdef MIX_BUFFERS
     //todo: apply multichannel SRC and add mixing support
     bytes = (rsxadev->mixbuffer_surround_bytes > rsxadev->mixbuffer_normal_bytes)
               ? rsxadev->mixbuffer_surround_bytes : rsxadev->mixbuffer_normal_bytes;
     mix_buffer(rsxadev->mixbuffer,
                rsxadev->mixbuffer_surround,
                rsxadev->mixbuffer_normal,
                bytes);
#else

     char *cpbuf = (popcount(rsxadev->config_out.channel_mask) > 2) ?
                        rsxadev->mixbuffer_surround : rsxadev->mixbuffer_normal;
     bytes = (popcount(rsxadev->config_out.channel_mask) > 2) ?
                    rsxadev->mixbuffer_surround_bytes : rsxadev->mixbuffer_normal_bytes;
     memcpy(rsxadev->mixbuffer,cpbuf,bytes);
#endif

     pthread_mutex_unlock(&rsxadev->lock);

     if(bytes)
       write_to_pipe(rsxadev,rsxadev->mixbuffer,bytes);

   }

   return NULL;
}

int init_submix_thread(void* param)
{
    int ret;
    pthread_t submix_thread;
    ALOGI("Initializing submix Thread");
    ret = pthread_create(&submix_thread, NULL, submix_loop, (void*) param);
    if (ret) {
        ALOGE("%s: failed to create %s: %s", __FUNCTION__,
              SUBMIX_THREAD_NAME, strerror(ret));
        return -ENOSYS;
    }
    return NO_ERROR;
}

//-------------submix thread end-----------

/* audio HAL functions */

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    const struct submix_stream_out *out =
            reinterpret_cast<const struct submix_stream_out *>(stream);
    uint32_t out_rate = out->dev->config.rate;
    //ALOGV("out_get_sample_rate() returns %u", out_rate);
    return out_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    if (rate != DEFAULT_RATE_HZ) {
        ALOGE("out_set_sample_rate(rate=%u) rate unsupported", rate);
        return -ENOSYS;
    }
    struct submix_stream_out *out = reinterpret_cast<struct submix_stream_out *>(stream);
    pthread_mutex_lock(&out->dev->lock);
    //ALOGV("out_set_sample_rate(rate=%u)", rate);
    out->dev->config.rate = rate;
    pthread_mutex_unlock(&out->dev->lock);
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    const struct submix_stream_out *out =
            reinterpret_cast<const struct submix_stream_out *>(stream);
    const struct submix_config& config_out = out->dev->config;
    struct submix_audio_device *rsxadev = (struct submix_audio_device *)out->dev;
    size_t buffer_size;

    if (rsxadev->surround_sink_connected) {
       buffer_size = config_out.period_size * popcount(out->config_out.channel_mask)
                            * sizeof(int16_t); // only PCM 16bit
    } else {
       buffer_size = config_out.period_size * popcount(config_out.channel_mask)
                            * sizeof(int16_t); // only PCM 16bit
    }
    ALOGV("out_get_buffer_size() returns %u, period size=%u",
            buffer_size, config_out.period_size);
    return buffer_size;
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    const struct submix_stream_out *out =
            reinterpret_cast<const struct submix_stream_out *>(stream);
    struct submix_audio_device *rsxadev = (struct submix_audio_device *)out->dev;
    uint32_t channels;
    if (rsxadev->surround_sink_connected) {
       channels = out->config_out.channel_mask;
    } else {
       channels = out->dev->config.channel_mask;
    }
    ALOGV("out_get_channels() returns %08x", channels);
    return channels;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    if (format != AUDIO_FORMAT_PCM_16_BIT) {
        return -ENOSYS;
    } else {
        return 0;
    }
}

static int out_standby(struct audio_stream *stream)
{
    ALOGI("out_standby()");

    const struct submix_stream_out *out = reinterpret_cast<const struct submix_stream_out *>(stream);

    pthread_mutex_lock(&out->dev->lock);

    out->dev->output_standby = true;

    pthread_mutex_unlock(&out->dev->lock);

    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    int exiting = -1;

    const struct submix_stream_out *out =
            reinterpret_cast<const struct submix_stream_out *>(stream);
    const struct submix_config * config_out = &(out->dev->config);
    struct submix_audio_device *rsxadev = (struct submix_audio_device *)out->dev;

    AudioParameter parms = AudioParameter(String8(kvpairs));
    // FIXME this is using hard-coded strings but in the future, this functionality will be
    //       converted to use audio HAL extensions required to support tunneling
    if ((parms.getInt(String8("exiting"), exiting) == NO_ERROR) && (exiting > 0)) {
        const struct submix_stream_out *out =
                reinterpret_cast<const struct submix_stream_out *>(stream);

        pthread_mutex_lock(&out->dev->lock);

        {
            sp<MonoPipe> sink = out->dev->rsxSink.get();
            if (sink == 0) {
                pthread_mutex_unlock(&out->dev->lock);
                return 0;
            }

            if(rsxadev->surround_sink_connected) {
                ALOGD("output stream exiting , active streams = %d",out->dev->num_of_streams);
                if(!out->dev->num_of_streams) {
                   ALOGI("shutdown");
                   sink->shutdown(true);
                }
            } else {
                ALOGI("shutdown");
                sink->shutdown(true);
            }
        } // done using the sink

        pthread_mutex_unlock(&out->dev->lock);
    }

    return 0;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    const struct submix_stream_out *out =
            reinterpret_cast<const struct submix_stream_out *>(stream);
    const struct submix_config * config_out = &(out->dev->config);
    uint32_t latency = (MAX_PIPE_DEPTH_IN_FRAMES * 1000) / config_out->rate;
    ALOGV("out_get_latency() returns %u", latency);
    return latency;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    return -ENOSYS;
}

static int convert_stereo_to_surround(char* out_buf,char * in_buf, size_t in_bytes, int in_channels)
{
    size_t out_bytes = 0, i = 0, j = 0;
    switch(in_channels) {
     case 6:
         for(i = 0,j=0; i < in_bytes; i+=2,j+=6) {
             out_buf[j+0] = in_buf[i+0];
             out_buf[j+1] = in_buf[i+1];
             out_buf[j+2] = 0;
             out_buf[j+3] = 0;
             out_buf[j+4] = 0;
             out_buf[j+5] = 0;
         }
         out_bytes = in_bytes*3;
         ALOGV("converted stereo buffer to 5.1 out_bytes = %d",out_bytes);
         break;
     case 3:
     case 4:
     case 8:
        ALOGE("unsupported channel conversion");
     default:
        ALOGW("No conversion done");
        break;
    }

    return out_bytes;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    //ALOGV("out_write(bytes=%d)", bytes);
    struct submix_stream_out *out = reinterpret_cast<struct submix_stream_out *>(stream);
    const size_t frame_size = audio_stream_frame_size(&stream->common);
    const size_t frames = bytes / frame_size;
    struct submix_audio_device *rsxadev = (struct submix_audio_device *)out->dev;

    if(rsxadev->surround_sink_connected) {
       pthread_mutex_lock(&rsxadev->lock);

       int stream_channels = popcount(out->config_out.channel_mask);

       //keep track of current active stream
       rsxadev->config_out.channel_mask = out->config_out.channel_mask;

       //fixme: need to ensure that submix thread executes before over-writing of buffers ??
       rsxadev->output_standby = false;

       memset(rsxadev->mixbuffer,0,MAX_SURROUND_BUFFER_SIZE);

       if (stream_channels > 2) {
           //copy the buffer to mix buffer 1
           memset(rsxadev->mixbuffer_surround,0,MAX_SURROUND_BUFFER_SIZE);
           memcpy(rsxadev->mixbuffer_surround,buffer,bytes);
           rsxadev->mixbuffer_surround_bytes = bytes;
       } else {
           // the mixer thread ouput will always be stereo
           // convert the stereo data to 5.1
           // copy the buffer to mix buffer 2
           memset(rsxadev->mixbuffer_normal,0,MAX_NORMAL_BUFFER_SIZE);
           int out_bytes = convert_stereo_to_surround(rsxadev->mixbuffer_normal,(char*)buffer,bytes, 6);
           rsxadev->mixbuffer_normal_bytes = out_bytes;
       }
       pthread_mutex_unlock(&rsxadev->lock);

       ALOGVV("out_write() copy to mixbuffer %u bytes stream_ch = %d normal_bytes = %d, \
                surr_bytes = %d",bytes,stream_channels,rsxadev->mixbuffer_normal_bytes,
                                 rsxadev->mixbuffer_surround_bytes);
       usleep(frames * 1000000 / out_get_sample_rate(&stream->common));
       return bytes;
    } else {

       ssize_t written_frames = 0;
       pthread_mutex_lock(&out->dev->lock);
       out->dev->output_standby = false;
       sp<MonoPipe> sink = out->dev->rsxSink.get();
       if (sink != 0) {
          if (sink->isShutdown()) {
              sink.clear();
              pthread_mutex_unlock(&out->dev->lock);
              // the pipe has already been shutdown, this buffer will be lost but we must
              //   simulate timing so we don't drain the output faster than realtime
              usleep(frames * 1000000 / out_get_sample_rate(&stream->common));
              return bytes;
          }
       } else {
             pthread_mutex_unlock(&out->dev->lock);
             ALOGE("out_write without a pipe!");
             ALOG_ASSERT("out_write without a pipe!");
             return 0;
       }
       pthread_mutex_unlock(&out->dev->lock);

       written_frames = sink->write(buffer, frames);

       if (written_frames < 0) {
           if (written_frames == (ssize_t)NEGOTIATE) {
               ALOGE("out_write() write to pipe returned NEGOTIATE");

               pthread_mutex_lock(&out->dev->lock);
               sink.clear();
               pthread_mutex_unlock(&out->dev->lock);

               written_frames = 0;
               return 0;
           } else {
              // write() returned UNDERRUN or WOULD_BLOCK, retry
               ALOGE("out_write() write to pipe returned unexpected %d", written_frames);
               written_frames = sink->write(buffer, frames);
           }
       }

       pthread_mutex_lock(&out->dev->lock);
       sink.clear();
       pthread_mutex_unlock(&out->dev->lock);

       if (written_frames < 0) {
           ALOGE("out_write() failed writing to pipe with %d", written_frames);
           return 0;
       } else {
           ALOGV("out_write() wrote %u bytes)", written_frames * frame_size);
           return written_frames * frame_size;
       }
   } //if(rsxadev->surround_sink_connected)

   return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    return -EINVAL;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    const struct submix_stream_in *in = reinterpret_cast<const struct submix_stream_in *>(stream);
    //ALOGV("in_get_sample_rate() returns %u", in->dev->config.rate);
    return in->dev->config.rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    const struct submix_stream_in *in = reinterpret_cast<const struct submix_stream_in *>(stream);
    size_t buffer_size = in->dev->config.period_size * audio_stream_frame_size(stream);
    ALOGV("in_get_buffer_size() returns %u",buffer_size);
    return buffer_size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    const struct submix_stream_in *in = reinterpret_cast<const struct submix_stream_in *>(stream);
    struct submix_audio_device *rsxadev = (struct submix_audio_device *)in->dev;

    //TODO: Add support for dynamic channel inputs upto 7.1
    if(rsxadev->surround_sink_connected) {
        return AUDIO_CHANNEL_IN_SUBMIX_5POINT1;
    } else {
        return AUDIO_CHANNEL_IN_STEREO;
    }
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    if (format != AUDIO_FORMAT_PCM_16_BIT) {
        return -ENOSYS;
    } else {
        return 0;
    }
}

static int in_standby(struct audio_stream *stream)
{
    ALOGI("in_standby()");
    const struct submix_stream_in *in = reinterpret_cast<const struct submix_stream_in *>(stream);

    pthread_mutex_lock(&in->dev->lock);

    in->dev->input_standby = true;

    pthread_mutex_unlock(&in->dev->lock);

    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    return 0;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    //ALOGV("in_read bytes=%u", bytes);
    ssize_t frames_read = -1977;
    struct submix_stream_in *in = reinterpret_cast<struct submix_stream_in *>(stream);
    struct submix_audio_device *rsxadev = (struct submix_audio_device *)in->dev;
    size_t frame_size;
    size_t frames_to_read;

    pthread_mutex_lock(&in->dev->lock);

    if(rsxadev->surround_sink_connected) {
        int dev_channels = popcount(in->dev->config.channel_mask);
        frame_size = dev_channels * sizeof(int16_t);
        frames_to_read = bytes / frame_size;
    } else {
        frame_size = audio_stream_frame_size(&stream->common);
        frames_to_read = bytes / frame_size;
    }

    const bool output_standby_transition = (in->output_standby != in->dev->output_standby);
    in->output_standby = in->dev->output_standby;

    int channels = popcount(in_get_channels(&stream->common));

    ALOGVV("in_read :: channels = %d frame_size = %d frames_to_read = %d bytes = %d out_channel = %d",
              channels,frame_size,frames_to_read,bytes,popcount(in->dev->config_out.channel_mask));

    if (in->dev->input_standby || output_standby_transition) {
        in->dev->input_standby = false;
        // keep track of when we exit input standby (== first read == start "real recording")
        // or when we start recording silence, and reset projected time
        int rc = clock_gettime(CLOCK_MONOTONIC, &in->record_start_time);
        if (rc == 0) {
            in->read_counter_frames = 0;
        }
    }

    in->read_counter_frames += frames_to_read;
    size_t remaining_frames = frames_to_read;

    {
        // about to read from audio source
        sp<MonoPipeReader> source = in->dev->rsxSource.get();
        if (source == 0) {
            ALOGE("no audio pipe yet we're trying to read!");
            pthread_mutex_unlock(&in->dev->lock);
            usleep((bytes / frame_size) * 1000000 / in_get_sample_rate(&stream->common));
            memset(buffer, 0, bytes);
            return bytes;
        }

        pthread_mutex_unlock(&in->dev->lock);

        // read the data from the pipe (it's non blocking)
        int attempts = 0;
        char* buff = (char*)buffer;
        while ((remaining_frames > 0) && (attempts < MAX_READ_ATTEMPTS)) {
            attempts++;
            frames_read = source->read(buff, remaining_frames, AudioBufferProvider::kInvalidPTS);
            if (frames_read > 0) {
                remaining_frames -= frames_read;
                buff += frames_read * frame_size;
            } else {
                usleep(READ_ATTEMPT_SLEEP_MS * 1000);
            }
        }
        // done using the source
        pthread_mutex_lock(&in->dev->lock);
        source.clear();
        pthread_mutex_unlock(&in->dev->lock);
    }

    if (remaining_frames > 0) {
        ALOGV("  remaining_frames = %d", remaining_frames);
        memset(((char*)buffer)+ bytes - (remaining_frames * frame_size), 0,
                remaining_frames * frame_size);
    }

    // compute how much we need to sleep after reading the data by comparing the wall clock with
    //   the projected time at which we should return.
    struct timespec time_after_read;// wall clock after reading from the pipe
    struct timespec record_duration;// observed record duration
    int rc = clock_gettime(CLOCK_MONOTONIC, &time_after_read);
    const uint32_t sample_rate = in_get_sample_rate(&stream->common);
    if (rc == 0) {
        // for how long have we been recording?
        record_duration.tv_sec  = time_after_read.tv_sec - in->record_start_time.tv_sec;
        record_duration.tv_nsec = time_after_read.tv_nsec - in->record_start_time.tv_nsec;
        if (record_duration.tv_nsec < 0) {
            record_duration.tv_sec--;
            record_duration.tv_nsec += 1000000000;
        }

        // read_counter_frames contains the number of frames that have been read since the beginning
        // of recording (including this call): it's converted to usec and compared to how long we've
        // been recording for, which gives us how long we must wait to sync the projected recording
        // time, and the observed recording time
        long projected_vs_observed_offset_us =
                ((int64_t)(in->read_counter_frames
                            - (record_duration.tv_sec*sample_rate)))
                        * 1000000 / sample_rate
                - (record_duration.tv_nsec / 1000);

        ALOGV("  record duration %5lds %3ldms, will wait: %7ldus",
                record_duration.tv_sec, record_duration.tv_nsec/1000000,
                projected_vs_observed_offset_us);
        if (projected_vs_observed_offset_us > 0) {
            usleep(projected_vs_observed_offset_us);
        }
    }

#ifdef _DUMP
   {
     FILE* fp =0;
     fp = fopen("/data/readdump.pcm","ab+");
     if(fp){
       int dumpbytes = fwrite(buffer,1,bytes,fp);
       ALOGD("dump data bytes = %d",dumpbytes);
       fclose(fp);
       fp =0;
     }
   }
#endif

    ALOGV("in_read returns %d", bytes);
    return bytes;

}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out)
{
    ALOGV("adev_open_output_stream()");
    struct submix_audio_device *rsxadev = (struct submix_audio_device *)dev;
    struct submix_stream_out *out;
    int ret;

    //if the sink does not support multichannel, the profile with direct flag
    //should be unloaded
    //NOTE: before stream is opened, capability flag has to be updated
    if ((!rsxadev->surround_sink_connected) && (flags & AUDIO_OUTPUT_FLAG_DIRECT)) {
        ALOGW("adev_open_output_stream :Sink does not support direct output");
        ret = -ENOSYS;
        goto err_open;
    }

    out = (struct submix_stream_out *)calloc(1, sizeof(struct submix_stream_out));
    if (!out) {
        ret = -ENOMEM;
        goto err_open;
    }

    pthread_mutex_lock(&rsxadev->lock);

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;

    if(rsxadev->surround_sink_connected) {
       if (flags & AUDIO_OUTPUT_FLAG_DIRECT) {
           ALOGV("adev_open_output_stream: Submix has Multichannel support");
           if(config->channel_mask == 0)
              config->channel_mask = AUDIO_CHANNEL_OUT_5POINT1;
       }
       else {
           ALOGV("adev_open_output_stream: Submix has stereo support");
           if(config->channel_mask == 0)
              config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
       }

       if(!rsxadev->config.channel_mask) {
           rsxadev->config.channel_mask = AUDIO_CHANNEL_OUT_5POINT1;
       }
       //keep track of the current active channel
       rsxadev->config_out.channel_mask = config->channel_mask;
       out->config_out.channel_mask = config->channel_mask;

       ALOGV("adev_open_output_stream: in_channel = %x device_channel = %x",
                    out->config_out.channel_mask,rsxadev->config.channel_mask);
    } else {
        ALOGV("adev_open_output_stream: Submix has only stereo support");
        config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
        rsxadev->config.channel_mask = config->channel_mask;
    }

    if ((config->sample_rate != 48000) && (config->sample_rate != 44100)) {
        config->sample_rate = DEFAULT_RATE_HZ;
    }
    rsxadev->config.rate = config->sample_rate;

    config->format = AUDIO_FORMAT_PCM_16_BIT;
    rsxadev->config.format = config->format;

    rsxadev->config.period_size = DEFAULT_PERIOD_SIZE;
    rsxadev->config.period_count = DEFAULT_PERIOD_COUNT;
    out->dev = rsxadev;

    *stream_out = &out->stream;

     ALOGV("adev_open_output_stream: initializing pipe channel count = %d",popcount(config->channel_mask));

    // initialize pipe
     if((rsxadev->rsxSink == NULL || rsxadev->rsxSource == NULL))
    {
        ALOGV("adev_open_output_stream: Creating Mono pipe/reader");
        NBAIO_Format format;

        if(rsxadev->surround_sink_connected) {
            //TODO: Add support upto 7.1
            format = Format_from_SR_C(config->sample_rate, 6);
            ALOGV("format = %d",format);
        } else {
            format = Format_from_SR_C(config->sample_rate, 2);
        }

        const NBAIO_Format offers[1] = {format};
        size_t numCounterOffers = 0;
        // creating a MonoPipe with optional blocking set to true.
        MonoPipe* sink = new MonoPipe(MAX_PIPE_DEPTH_IN_FRAMES, format, true/*writeCanBlock*/);
        ssize_t index = sink->negotiate(offers, 1, NULL, numCounterOffers);
        ALOG_ASSERT(index == 0);
        MonoPipeReader* source = new MonoPipeReader(sink);
        numCounterOffers = 0;
        index = source->negotiate(offers, 1, NULL, numCounterOffers);
        ALOG_ASSERT(index == 0);
        rsxadev->rsxSink = sink;
        rsxadev->rsxSource = source;
    }

    if(rsxadev->surround_sink_connected) {
       rsxadev->num_of_streams++;
       ALOGV("adev_open_output_stream: active streams = %d",rsxadev->num_of_streams);
    }

    pthread_mutex_unlock(&rsxadev->lock);

    ALOGV("%s exit",__func__);
    return 0;

err_open:
    ALOGE("%s exit with error",__func__);
    *stream_out = NULL;
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    const struct submix_stream_out *out =
            reinterpret_cast<const struct submix_stream_out *>(stream);

    ALOGV("adev_close_output_stream() for stream");
    struct submix_audio_device *rsxadev = (struct submix_audio_device *)dev;

    pthread_mutex_lock(&rsxadev->lock);

    if(rsxadev->surround_sink_connected) {
       --rsxadev->num_of_streams;
       ALOGV("adev_close_output_stream() num = %d",rsxadev->num_of_streams);
       if(!rsxadev->num_of_streams) {
          ALOGD("no more active pipes - clear the pipes");
          rsxadev->rsxSink.clear();
          rsxadev->rsxSource.clear();
       }
       free(stream);
     } else {
         rsxadev->rsxSink.clear();
         rsxadev->rsxSource.clear();
         free(stream);
     }

    pthread_mutex_unlock(&rsxadev->lock);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct submix_audio_device *adev = (struct submix_audio_device *)dev;
    char *key,*value;
    struct str_parms *param;
    int session = 0;
    int keyvalue = 0;
    char * kvp = NULL;
    int err = 0;

    if ((kvpairs == NULL) && (adev == NULL)) {
        ALOGE("%s NUll inputs kvpairs = %s, adev = %d",__func__, kvpairs,(int)adev);
        return err;
    }

    kvp = (char*)kvpairs;
    ALOGVV("%s entered with key-value pair %s", __func__,kvpairs);

    key = strtok(kvp,"=");
    value = strtok(NULL, "=");
    if (key != NULL) {
       if (strcmp(key, AUDIO_PARAMETER_KEY_REMOTE_BGM_STATE) == 0) {
           adev->bgmstate = strdup("false");
           if (value != NULL) {
               if (strcmp(value, "true") == 0) {
                  adev->bgmstate = strdup("true");
               }
           }
       }

       if (strcmp(key, AUDIO_PARAMETER_VALUE_REMOTE_BGM_AUDIO) == 0) {
           /*by default audio stream is active*/
           gbgm_audio = strdup("true");
           if (value != NULL) {
               if (strcmp(value, "0") == 0) {
                  gbgm_audio = strdup("false");
               }
           }
           ALOGV("%s : audio state in BGM = %s",__func__,gbgm_audio);
       }

       if (strcmp(key, AUDIO_PARAMETER_VALUE_REMOTE_BGM_SESSION_ID) == 0) {
           if (value != NULL) {
               adev->bgmsession = strdup(value);
           }
       }
    }

    ALOGVV("%s exit bgmstate = %s, bgmaudio = %s bgmplayersession = %d",
                __func__,adev->bgmstate,gbgm_audio,atoi(adev->bgmsession));

    return 0;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    struct submix_audio_device *adev = (struct submix_audio_device *)dev;
    char value[32];

    ALOGVV("%s entered with keys %s", __func__,keys);

    if (strcmp(keys, AUDIO_PARAMETER_KEY_REMOTE_BGM_STATE) == 0) {
        struct str_parms *parms = str_parms_create_str(keys);
        if(!parms) {
           ALOGE("%s failed for bgm_state",__func__);
           goto error_exit;
        }
        int ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_REMOTE_BGM_STATE,
                                     value, sizeof(value));
        char *str;

        str_parms_destroy(parms);
        if (ret >= 0) {
           ALOGVV("%s adev->bgmstate %s", __func__,adev->bgmstate);
           parms = str_parms_create_str(adev->bgmstate);
           if(!parms) {
              ALOGE("%s failed for bgm_state",__func__);
              goto error_exit;
           }
           str = str_parms_to_str(parms);
           str = strtok(str, "=");
           str_parms_destroy(parms);
           ALOGVV("%s entered with key %s for which value is %s", __func__,keys,str);
           return str;
        }
    }

    if (strcmp(keys, AUDIO_PARAMETER_VALUE_REMOTE_BGM_AUDIO) == 0) {
       struct str_parms *parms = str_parms_create_str(keys);
        if(!parms) {
           ALOGE("%s failed for bgm_audio",__func__);
           goto error_exit;
        }
       int ret = str_parms_get_str(parms, AUDIO_PARAMETER_VALUE_REMOTE_BGM_AUDIO,
                                     value, sizeof(value));
       char *str;

       str_parms_destroy(parms);
       if (ret >= 0) {
          ALOGVV("%s gbgm_audio %s", __func__,gbgm_audio);
          parms = str_parms_create_str(gbgm_audio);
          if(!parms) {
             ALOGE("%s failed for bgm_state",__func__);
             goto error_exit;
          }
          str = str_parms_to_str(parms);
          str = strtok(str, "=");
          str_parms_destroy(parms);
          ALOGVV("%s entered with key %s for which value is %s", __func__,keys,str);
          return str;
       }
    }

    if (strcmp(keys, AUDIO_PARAMETER_VALUE_REMOTE_BGM_SESSION_ID) == 0) {
       struct str_parms *parms = str_parms_create_str(keys);
        if(!parms) {
           ALOGE("%s failed for bgm_session",__func__);
           goto error_exit;
        }
       int ret = str_parms_get_str(parms, AUDIO_PARAMETER_VALUE_REMOTE_BGM_SESSION_ID,
                                     value, sizeof(value));
       char *str;

       str_parms_destroy(parms);
       if (ret >= 0) {
          ALOGVV("%s adev->bgmsession %s", __func__,adev->bgmsession);
          parms = str_parms_create_str(adev->bgmsession);
          if(!parms) {
             ALOGE("%s failed for bgm_state",__func__);
             goto error_exit;
          }
          str = str_parms_to_str(parms);
          str = strtok(str, "=");
          str_parms_destroy(parms);
          ALOGVV("%s entered with key %s for which value is %s", __func__,keys,str);
          return str;
       }
    }

    if (strcmp(keys, AUDIO_PARAMETER_KEY_DIRECT_PROFILE_SUPPORTED) == 0) {

        char* surround_support = "true";
        if(!adev->surround_sink_connected)
           surround_support = "false";

        struct str_parms *parms = str_parms_create_str(surround_support);
        if(!parms) {
           ALOGE("%s failed for direct_profile_support",__func__);
           goto error_exit;
        }
        char *str;
        str = str_parms_to_str(parms);
        str = strtok(str, "=");
        str_parms_destroy(parms);
        ALOGVV("%s entered with key %s for which value is %s", __func__,keys,str);
        return str;
    }

    ALOGVV("%s exit bgmstate = %s, bgmaudio = %s bgmplayersession = %d",
                __func__,adev->bgmstate,gbgm_audio,atoi(adev->bgmsession));

error_exit:
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    ALOGI("adev_init_check()");
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev, float *volume)
{
    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted)
{
    return -ENOSYS;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted)
{
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    return -ENOSYS;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    //### TODO correlate this with pipe parameters
    struct submix_audio_device *rsxadev = (struct submix_audio_device *)dev;
    if(rsxadev->surround_sink_connected) {
        size_t buffer_size = DEFAULT_PERIOD_SIZE * popcount(config->channel_mask)
                              * sizeof(int16_t); // only PCM 16bit
        buffer_size = roundup(buffer_size);
        ALOGV("adev_get_input_buffer_size returns %u channels = %d period_size = %d",
                      buffer_size, popcount(config->channel_mask),
                       DEFAULT_PERIOD_SIZE);
        return buffer_size;
    } else {
        return 4096;
    }
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
    ALOGI("adev_open_input_stream() for channels = %x",config->channel_mask);

    struct submix_audio_device *rsxadev = (struct submix_audio_device *)dev;
    struct submix_stream_in *in;
    int ret;

    in = (struct submix_stream_in *)calloc(1, sizeof(struct submix_stream_in));
    if (!in) {
        ret = -ENOMEM;
        goto err_open;
    }

    pthread_mutex_lock(&rsxadev->lock);

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    rsxadev->surround_sink_connected = false;
    //assuming negotiation is complete and the audio source is
    // opened with required channels based on sink capability
    if(config->channel_mask == AUDIO_CHANNEL_IN_SUBMIX_5POINT1)
       rsxadev->surround_sink_connected = true;

    rsxadev->config.channel_mask = config->channel_mask;

    if ((config->sample_rate != 48000) && (config->sample_rate != 44100)) {
        config->sample_rate = DEFAULT_RATE_HZ;
    }
    rsxadev->config.rate = config->sample_rate;

    config->format = AUDIO_FORMAT_PCM_16_BIT;
    rsxadev->config.format = config->format;

    rsxadev->config.period_size = DEFAULT_PERIOD_SIZE;
    rsxadev->config.period_count = DEFAULT_PERIOD_COUNT;

    *stream_in = &in->stream;

    in->dev = rsxadev;

    in->read_counter_frames = 0;
    in->output_standby = rsxadev->output_standby;

    if(rsxadev->surround_sink_connected) {
       rsxadev->mixbuffer_surround = (char*) malloc(MAX_SURROUND_BUFFER_SIZE);
       if (!rsxadev->mixbuffer_surround) {
          ret = -ENOMEM;
          ALOGE("No memory for mix buffer surround");
          goto err_cleanup;
       }
       rsxadev->mixbuffer_normal = (char*) malloc(MAX_NORMAL_BUFFER_SIZE);
       if (!rsxadev->mixbuffer_normal) {
          ret = -ENOMEM;
          ALOGE("No memory for mix buffer surround");
          goto err_cleanup;
       }
       rsxadev->mixbuffer = (char*) malloc(MAX_SURROUND_BUFFER_SIZE);
       if (!rsxadev->mixbuffer) {
          ret = -ENOMEM;
          ALOGE("No memory for mix buffer");
          goto err_cleanup;
       }

       memset(rsxadev->mixbuffer_surround,0,MAX_SURROUND_BUFFER_SIZE);
       memset(rsxadev->mixbuffer_normal,0,MAX_NORMAL_BUFFER_SIZE);
       memset(rsxadev->mixbuffer,0,MAX_SURROUND_BUFFER_SIZE);
       rsxadev->mixbuffer_normal_bytes = 0;
       rsxadev->mixbuffer_surround_bytes = 0;

       //thread to write individual streams to the pipe
       ret = init_submix_thread(rsxadev);
       if (ret) {
          ALOGE("Submix mixer thread failed");
          goto err_cleanup;
       }
       ALOGV("initialized submix thread for mixing for device %x",rsxadev);
       rsxadev->submixThreadStart = true;
    } //surround_sink_connected

    pthread_mutex_unlock(&rsxadev->lock);

    return 0;
err_cleanup:
    rsxadev->submixThreadStart = false;
    if(rsxadev->sbm_thread)
       pthread_join(rsxadev->sbm_thread, NULL);
    if(rsxadev->mixbuffer)
       free(rsxadev->mixbuffer);
    rsxadev->mixbuffer = NULL;
    if(rsxadev->mixbuffer_surround)
       free(rsxadev->mixbuffer_surround);
    rsxadev->mixbuffer_surround = NULL;
    if(rsxadev->mixbuffer_normal)
       free(rsxadev->mixbuffer_normal);
    rsxadev->mixbuffer_normal = NULL;
    free(in);
err_open:
    *stream_in = NULL;
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    const struct submix_stream_in  *in =
            reinterpret_cast<const struct submix_stream_in  *>(stream);
    struct submix_audio_device *rsxadev = (struct submix_audio_device *)dev;

    ALOGV("adev_close_input_stream()");
    pthread_mutex_lock(&rsxadev->lock);

    MonoPipe* sink = rsxadev->rsxSink.get();
    if (sink != NULL) {
        ALOGI("shutdown active streams = %d",rsxadev->num_of_streams);
        sink->shutdown(true);
    }

    if(rsxadev->surround_sink_connected) {
       rsxadev->submixThreadStart = false;
    }

    free(stream);

    pthread_mutex_unlock(&rsxadev->lock);

    if((rsxadev->surround_sink_connected) &&
         (!rsxadev->submixThreadStart)) {
       pthread_join(rsxadev->sbm_thread, NULL);

       if(rsxadev->mixbuffer_surround) {
          free(rsxadev->mixbuffer_surround);
          rsxadev->mixbuffer_surround = NULL;
       }
       if(rsxadev->mixbuffer_normal) {
          free(rsxadev->mixbuffer_normal);
          rsxadev->mixbuffer_normal = NULL;
       }
       if(rsxadev->mixbuffer) {
          free(rsxadev->mixbuffer);
          rsxadev->mixbuffer = NULL;
       }
       ALOGV("submix thread closed");
    }
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

static int adev_close(hw_device_t *device)
{
    ALOGI("adev_close()");
    free(device);
    return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    ALOGI("adev_open(name=%s)", name);
    struct submix_audio_device *rsxadev;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    rsxadev = (submix_audio_device*) calloc(1, sizeof(struct submix_audio_device));
    if (!rsxadev)
        return -ENOMEM;

    rsxadev->bgmstate = "false";
    rsxadev->bgmsession = "0";

    rsxadev->device.common.tag = HARDWARE_DEVICE_TAG;
    rsxadev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    rsxadev->device.common.module = (struct hw_module_t *) module;
    rsxadev->device.common.close = adev_close;

    rsxadev->device.init_check = adev_init_check;
    rsxadev->device.set_voice_volume = adev_set_voice_volume;
    rsxadev->device.set_master_volume = adev_set_master_volume;
    rsxadev->device.get_master_volume = adev_get_master_volume;
    rsxadev->device.set_master_mute = adev_set_master_mute;
    rsxadev->device.get_master_mute = adev_get_master_mute;
    rsxadev->device.set_mode = adev_set_mode;
    rsxadev->device.set_mic_mute = adev_set_mic_mute;
    rsxadev->device.get_mic_mute = adev_get_mic_mute;
    rsxadev->device.set_parameters = adev_set_parameters;
    rsxadev->device.get_parameters = adev_get_parameters;
    rsxadev->device.get_input_buffer_size = adev_get_input_buffer_size;
    rsxadev->device.open_output_stream = adev_open_output_stream;
    rsxadev->device.close_output_stream = adev_close_output_stream;
    rsxadev->device.open_input_stream = adev_open_input_stream;
    rsxadev->device.close_input_stream = adev_close_input_stream;
    rsxadev->device.dump = adev_dump;

    rsxadev->input_standby = true;
    rsxadev->output_standby = true;
    rsxadev->surround_sink_connected = false;
    // we are trying to use the same mono pipe for multiple streams
    rsxadev->num_of_streams = 0;

    *device = &rsxadev->device.common;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    /* open */ adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    /* common */ {
        /* tag */                HARDWARE_MODULE_TAG,
        /* module_api_version */ AUDIO_MODULE_API_VERSION_0_1,
        /* hal_api_version */    HARDWARE_HAL_API_VERSION,
        /* id */                 AUDIO_HARDWARE_MODULE_ID,
        /* name */               "Wifi Display audio HAL",
        /* author */             "The Android Open Source Project",
        /* methods */            &hal_module_methods,
        /* dso */                NULL,
        /* reserved */           { 0 },
    },
};

} //namespace android

} //extern "C"
