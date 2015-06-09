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

#define LOG_TAG "usb_dock_audio_hw"
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

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>

#define DEFAULT_CARD       3
#define DEFAULT_DEVICE     0

struct pcm_config pcm_config_default = {
    .channels = 2,
    .rate = 48000,
    .period_size = 1024,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_HD_default = {
    .channels = 2,
    .rate = 192000,
    .period_size = 8192,
    .period_count = 4,
    .format = PCM_FORMAT_S24_LE,
};

struct pcm_config pcm_config_audio_capture = {
    .channels = 2,
    .period_count = 2,
    .period_size = 1024,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .stop_threshold = 0,
    .silence_threshold = 0,
};

#define CAPTURE_DURATION_MSEC 20

struct audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */

    int card;
    int device;
    bool standby;
    struct pcm * active_pcm;
    bool hdaudio_active;
};

struct stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    struct pcm *pcm;
    bool standby;

    /* PCM Stream Configurations */
    struct pcm_config pcm_config;
    uint32_t   channel_mask;

    /* ALSA PCM Configurations */
    uint32_t   sample_rate;
    uint32_t   buffer_size;
    uint32_t   channels;
    uint32_t   latency;

    audio_output_flags_t flags;

    struct audio_device *dev;
};

struct stream_in {
    struct audio_stream_in stream;
    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    struct pcm_config pcm_config;
    struct pcm *pcm;
    int standby;
    int source;
    int device;
    audio_channel_mask_t channel_mask;

    struct audio_device *dev;
};

/**
 * NOTE: when multiple mutexes have to be acquired, always respect the
 * following order: hw device > out stream
 */

/* Helper functions */

static int out_standby(struct audio_stream *stream);

static char* get_card_name_from_substr(const char*name)
{
    FILE *fp;
    char alsacard[500];
    char* substr;
    int found = 0;

    if((fp = fopen("/proc/asound/cards","r")) == NULL) {
        ALOGE("Cannot open /proc/asound/cards file to get sound card info");
    } else {
        while((fgets(alsacard, sizeof(alsacard), fp) != NULL)) {
              ALOGVV("alsacard %s", alsacard);
              if (strstr(alsacard, "USB-Audio")) {
                  found = 1;
                  break;
              } else if (strstr(alsacard, "USB Audio")) {
                  found = 1;
                  break;
              }
         }
         fclose(fp);
    }
    if(found) {
      ALOGD("Found USB card %s",alsacard);

      substr = strtok(alsacard,"[") ? strtok(NULL,"]") : NULL;
      ALOGVV("filter 1 substr %s",substr);
      if(!substr) return NULL;

      // remove spaces if any in the stripped name
      substr = strtok(substr," ");
      if(!substr) return NULL;
      ALOGV("usb string substr %s",substr);

      return substr;
    }
    else
      return NULL;
}

// This function return the card number associated with the card ID (name)
// passed as argument
static int get_card_number_by_name(const char* name)
{
    char id_filepath[PATH_MAX] = {0};
    char number_filepath[PATH_MAX] = {0};
    ssize_t written;
    char* opstr = NULL;

    //find the card containing USB Audio short/long name
    opstr= get_card_name_from_substr(name);
    if(opstr == NULL) {
       ALOGE("Sound card substr %s does not exist - setting default", name);
       return DEFAULT_CARD;
    }

    ALOGD("Found USB card opstr = %s",opstr);

    snprintf(id_filepath, sizeof(id_filepath), "/proc/asound/%s", opstr);
    written = readlink(id_filepath, number_filepath, sizeof(number_filepath));
    if (written < 0) {
        ALOGE("Sound card %s does not exist - setting default", opstr);
        return DEFAULT_CARD;
    } else if (written >= (ssize_t)sizeof(id_filepath)) {
        ALOGE("Sound card %s name is too long - setting default", opstr);
        return DEFAULT_CARD;
    }

    // We are assured, because of the check in the previous elseif, that this
    // buffer is null-terminated.  So this call is safe.
    // 4 == strlen("card")
    return atoi(number_filepath + 4);
}

/**
 * NOTE: when multiple mutexes have to be acquired, always respect the
 * following order: hw device > out stream
 */

/* Helper functions */
/* must be called with hw device and output stream mutexes locked */

static int format_to_bits(enum pcm_format pcmformat)
{
  switch (pcmformat) {
    case PCM_FORMAT_S32_LE:
         return 32;
    case PCM_FORMAT_S24_LE:
         return 24;
    case PCM_FORMAT_S16_LE:
    default:
         return 16;
  };
}

static int bits_to_format(int bits_per_sample)
{
  switch (bits_per_sample) {
    case 32:
         return PCM_FORMAT_S32_LE;
    case 24:
         return PCM_FORMAT_S24_LE;
    case 16:
    default:
         return PCM_FORMAT_S16_LE;
  };
}


audio_format_t tiny_to_framework_format(enum pcm_format format)
{
    audio_format_t fwFormat;

    switch(format) {
    case PCM_FORMAT_S24_LE:
// [Fix me]the decoder gives 32 bit samples at present
//    case PCM_FORMAT_S32_LE:
        fwFormat = AUDIO_FORMAT_PCM_8_24_BIT;
        break;
    case PCM_FORMAT_S16_LE:
    default:
        fwFormat = AUDIO_FORMAT_PCM_16_BIT;
        break;
    }
    return fwFormat;
}

static int validate_output_hardware_params(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    struct pcm_params *params;
    unsigned int min;
    unsigned int max;

    params = pcm_params_get(adev->card, adev->device, PCM_OUT);
    if (params == NULL) {
        ALOGW("Device does not exist.\n");
        return -1;
    }

    //always open the device with max capabilities if
    // input params are not supported

    min = pcm_params_get_min(params, PCM_PARAM_RATE);
    max = pcm_params_get_max(params, PCM_PARAM_RATE);
    ALOGI("hw supported Rate:\tmin=%uHz\tmax=%uHz\n", min, max);
    if((out->pcm_config.rate < min) || (out->pcm_config.rate > max)) {
       out->pcm_config.rate = max;
    }

    min = pcm_params_get_min(params, PCM_PARAM_CHANNELS);
    max = pcm_params_get_max(params, PCM_PARAM_CHANNELS);
    ALOGI("hw supported Channels:\tmin=%u\t\tmax=%u\n", min, max);
    if((out->pcm_config.channels < min) || (out->pcm_config.channels > max)) {
       out->pcm_config.channels = max;
    }

    min = pcm_params_get_min(params, PCM_PARAM_SAMPLE_BITS);
    max = pcm_params_get_max(params, PCM_PARAM_SAMPLE_BITS);
    ALOGI("hw supported Sample bits:\tmin=%u\t\tmax=%u\n", min, max);

    unsigned int bits_per_sample = format_to_bits(out->pcm_config.format);

    if((bits_per_sample < min) || (bits_per_sample > max)) {
       bits_per_sample = max;
    }

    out->pcm_config.format = bits_to_format(bits_per_sample);

   ALOGV("%s exit supported params %d,%d,%d,%d,%d",__func__,
          out->pcm_config.channels,
          out->pcm_config.rate,
          out->pcm_config.period_size,
          out->pcm_config.period_count,
          out->pcm_config.format);

    pcm_params_free(params);

    return 0;

}

static int start_output_stream(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    int ret = 0;

    ALOGV("%s enter input params %d,%d,%d,%d,%d",__func__,
          out->pcm_config.channels,
          out->pcm_config.rate,
          out->pcm_config.period_size,
          out->pcm_config.period_count,
          out->pcm_config.format);

    out->pcm_config.start_threshold = 0;
    out->pcm_config.stop_threshold = 0;
    out->pcm_config.silence_threshold = 0;

    // close the active device and make HD device active
    if(out->flags & AUDIO_OUTPUT_FLAG_DIRECT) {
        ALOGV("direct flag active stream");
        if(adev->active_pcm) {
          ALOGVV("Current active stream is closed to open HD interface %d",(int)adev->active_pcm);
          pcm_close(adev->active_pcm);
        }
        adev->hdaudio_active = true;
        adev->active_pcm = NULL;
    }

    // if HD device is active, use the same for subsequent streams
    if((adev->active_pcm) && (adev->hdaudio_active)) {
       out->pcm = adev->active_pcm;
       ALOGV("USB PCM interface already opened - use same params to render till its closed");
       goto skip_open;
    }

    /*TODO - this needs to be updated once the device connect intent sends
      card, device id*/
    adev->card = get_card_number_by_name("USB Audio");
    adev->device = DEFAULT_DEVICE;
    ALOGD("%s: USB card number = %d, device = %d",__func__,adev->card,adev->device);

    /* query & validate sink supported params and input params*/
    ret = validate_output_hardware_params(out);
    if(ret != 0) {
        ALOGE("Unsupport input stream parameters");
        adev->active_pcm = NULL;
        adev->hdaudio_active = false;
        out->pcm = NULL;
        return -ENOSYS;
    }

    out->pcm = pcm_open(adev->card, adev->device, PCM_OUT, &out->pcm_config);

    if (out->pcm && !pcm_is_ready(out->pcm)) {
        ALOGE("pcm_open() failed: %s", pcm_get_error(out->pcm));
        pcm_close(out->pcm);
        adev->active_pcm = NULL;
        adev->hdaudio_active = false;
        out->pcm = NULL;
        return -ENOSYS;
    }

    //update the newly opened pcm to be active
    adev->active_pcm = out->pcm;

skip_open:
    ALOGV("Initialized PCM device for channels %d sample rate %d activepcm = %d",
               out->pcm_config.channels,out->pcm_config.rate,(int)adev->active_pcm);
    ALOGV("%s exit",__func__);
    return 0;
}

static int close_device(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out ? out->dev : NULL;

    if(adev == NULL) return -ENOSYS;

    /* close the device
     * 1. if HD stream close is invoked
     * 2. if no HD stream is active.
     */
    if((adev->active_pcm) && (adev->hdaudio_active) && (out->flags & AUDIO_OUTPUT_FLAG_DIRECT)) {
        ALOGV("%s HD PCM device closed",__func__);
        pcm_close(adev->active_pcm);
        adev->active_pcm = NULL;
        adev->hdaudio_active = false;
    } else if((out->pcm) && (!adev->hdaudio_active)) {
        ALOGV("%s Default PCM device closed",__func__);
        pcm_close(out->pcm);
        out->pcm = NULL;
    }

    return 0;
}

static int validate_input_hardware_params(struct stream_in *in)
{
    struct audio_device *adev = in->dev;
    struct pcm_params *params;
    unsigned int min;
    unsigned int max;

    params = pcm_params_get(adev->card, adev->device, PCM_IN);
    if (params == NULL) {
        ALOGW("Device does not exist.\n");
        return -1;
    }

    /* always open the device with min capabilities if
     * input params are not supported
     */

    min = pcm_params_get_min(params, PCM_PARAM_RATE);
    max = pcm_params_get_max(params, PCM_PARAM_RATE);
    ALOGI("hw supported Rate:\tmin=%uHz\tmax=%uHz\n", min, max);
    if((in->pcm_config.rate < min) || (in->pcm_config.rate > max)) {
       in->pcm_config.rate = min;
    }

    min = pcm_params_get_min(params, PCM_PARAM_CHANNELS);
    max = pcm_params_get_max(params, PCM_PARAM_CHANNELS);
    ALOGI("hw supported Channels:\tmin=%u\t\tmax=%u\n", min, max);
    if((in->pcm_config.channels < min) || (in->pcm_config.channels > max)) {
       in->pcm_config.channels = min;
    }

    min = pcm_params_get_min(params, PCM_PARAM_SAMPLE_BITS);
    max = pcm_params_get_max(params, PCM_PARAM_SAMPLE_BITS);
    ALOGI("hw supported Sample bits:\tmin=%u\t\tmax=%u\n", min, max);

    unsigned int bits_per_sample = format_to_bits(in->pcm_config.format);

    if((bits_per_sample < min) || (bits_per_sample > max)) {
       bits_per_sample = min;
    }

    in->pcm_config.format = bits_to_format(bits_per_sample);

    min = pcm_params_get_min(params, PCM_PARAM_PERIODS);
    max = pcm_params_get_max(params, PCM_PARAM_PERIODS);
    ALOGI("hw supported period_count:\tmin=%u\t\tmax=%u\n", min, max);
    if((in->pcm_config.period_count < min) || (in->pcm_config.period_count > max)) {
       in->pcm_config.period_count = min;
    }

    min = pcm_params_get_min(params, PCM_PARAM_PERIOD_SIZE);
    max = pcm_params_get_max(params, PCM_PARAM_PERIOD_SIZE);
    ALOGI("hw supported period_size:\tmin=%u\t\tmax=%u\n", min, max);
    if((in->pcm_config.period_size < min) || (in->pcm_config.period_size > max)) {
       in->pcm_config.period_size = min;
    }

    ALOGV("%s :: Exit supported params \
         \r \t channels = %d,    \
         \r \t rate     = %d,    \
         \r \t period_size = %d, \
         \r \t period_count = %d, \
         \r \t format = %d", __func__,
       in->pcm_config.channels,in->pcm_config.rate,
       in->pcm_config.period_size,in->pcm_config.period_count,
       in->pcm_config.format);

    pcm_params_free(params);

    return 0;

}

static int check_input_parameters(uint32_t sample_rate,
                                  audio_format_t format,
                                  int channel_count)
{
    if (format != AUDIO_FORMAT_PCM_16_BIT) return -EINVAL;

    if ((channel_count < 1) || (channel_count > 2)) return -EINVAL;

    switch (sample_rate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static size_t get_input_buffer_size(uint32_t sample_rate,
                                    audio_format_t format,
                                    int channel_count)
{
    size_t size = 0;

    if (check_input_parameters(sample_rate, format, channel_count) != 0)
        return 0;

    size = (sample_rate * CAPTURE_DURATION_MSEC) / 1000;
    /*only 16 bit capture is supported*/
    size *= sizeof(short) * channel_count;

    /* make sure the size is multiple of 64 to suit the framework*/
    size += 0x3f;
    size &= ~0x3f;

    return size;
}

int start_input_stream(struct stream_in *in)
{
    int ret = 0;
    struct audio_device *adev = in->dev;

    adev->card = get_card_number_by_name("USB Audio");
    adev->device = DEFAULT_DEVICE;
    ALOGVV("%s: USB card number = %d, device = %d",__func__,adev->card,adev->device);

    ALOGV("%s :: requested params \
         \r \t channels = %d,    \
         \r \t rate     = %d,    \
         \r \t period_size = %d, \
         \r \t period_count = %d, \
         \r \t format = %d", __func__,
       in->pcm_config.channels,in->pcm_config.rate,
       in->pcm_config.period_size,in->pcm_config.period_count,
       in->pcm_config.format);

    ret = validate_input_hardware_params(in);
    if(ret != 0) {
        ALOGE("Unsupport input stream parameters");
        in->pcm = NULL;
        return -ENOSYS;
    }

    in->pcm = pcm_open(adev->card, adev->device,
                           PCM_IN, &in->pcm_config);
    if (in->pcm && !pcm_is_ready(in->pcm)) {
        ALOGE("%s: %s", __func__, pcm_get_error(in->pcm));
        pcm_close(in->pcm);
        in->pcm = NULL;
        ret = -EIO;
        ALOGD("%s: exit: status(%d)", __func__, ret);
        return ret;
    } else {
        ALOGV("%s: exit capture pcm = %x", __func__,in->pcm);
        return ret;
    }
}

/* API functions */

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    return out->pcm_config.rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    size_t buf_size;

    buf_size = out->pcm_config.period_size *
                  audio_stream_frame_size((struct audio_stream *)stream);

    ALOGV("%s : %d, period_size : %d, frame_size : %d",
        __func__,
        buf_size,
        out->pcm_config.period_size,
        audio_stream_frame_size((struct audio_stream *)stream));

    return buf_size;
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    ALOGV("%s channel mask : %x",__func__,out->channel_mask);
    return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    ALOGV("%s format : %x",__func__,out->pcm_config.format);
    return tiny_to_framework_format(out->pcm_config.format);
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    ALOGV("%s enter standby = %d rate = %d",__func__,out->standby,out->pcm_config.rate);
    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);

    if(!out->standby) {
        close_device(stream);
        out->standby = true;
        out->pcm = NULL;
    }

    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);

    ALOGV("%s exit",__func__);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    struct str_parms *parms;
    char value[32];
    int ret;
    int routing = 0;

    ALOGV("%s enter",__func__);

    parms = str_parms_create_str(kvpairs);
    pthread_mutex_lock(&adev->lock);

    if (parms == NULL) {
        ALOGE("Couldn't extract string params from key value pair");
        pthread_mutex_unlock(&adev->lock);
        return 0;
    }

    ret = str_parms_get_str(parms, "card", value, sizeof(value));
    if (ret >= 0)
        adev->card = atoi(value);

    ret = str_parms_get_str(parms, "device", value, sizeof(value));
    if (ret >= 0)
        adev->device = atoi(value);

    pthread_mutex_unlock(&adev->lock);
    str_parms_destroy(parms);

    ALOGV("%s exit",__func__);
    return 0;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    return (out->pcm_config.period_size * out->pcm_config.period_count * 1000) /
            out_get_sample_rate(&stream->common);
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    int ret = 0;
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out ? out->dev : NULL;

    if(adev == NULL) return -ENOSYS;

    ALOGV("%s enter",__func__);

    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);

    // there is a possibility that the HD interface is open
    // and normal pcm stream is still active. Feed the new
    // interface to normal pcm stream
    if(adev->active_pcm) {
      if(adev->active_pcm != out->pcm)
         out->pcm = adev->active_pcm;
    }

    if ((out->standby) || (!adev->active_pcm)) {
        ret = start_output_stream(out);
        if (ret != 0) {
            goto err;
        }
        out->standby = false;
    }

    if(!out->pcm){
       ALOGD("%s: null handle to write - device already closed",__func__);
       goto err;
    }

    ret = pcm_write(out->pcm, (void *)buffer, bytes);

    ALOGVV("%s: pcm_write returned = %d rate = %d",__func__,ret,out->pcm_config.rate);

err:
    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);


    if (ret != 0) {
        uint64_t duration_ms = ((bytes * 1000)/
                                (audio_stream_frame_size(&stream->common)) /
                                (out_get_sample_rate(&stream->common)));
        ALOGV("%s : silence written", __func__);
        usleep(duration_ms * 1000);
    }

    ALOGV("%s exit",__func__);

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

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_out *out;
    int ret;
    ALOGV("%s enter card %d device %d ",__func__, adev->card, adev->device);

    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (!out)
        return -ENOMEM;

    out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;

    if (flags & AUDIO_OUTPUT_FLAG_DIRECT) {
        ALOGV("%s: USB Audio (device mode) HD",__func__);
        if (config->format == 0)
            config->format = pcm_config_HD_default.format;
        if (config->channel_mask == 0)
            config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
        if (config->sample_rate == 0)
            config->sample_rate = pcm_config_HD_default.rate;
        out->pcm_config.period_size        = pcm_config_HD_default.period_size;
        out->flags |= flags;
    } else {
        ALOGV("%s: USB Audio (device mode) Stereo",__func__);
        if (config->format == 0)
            config->format = pcm_config_default.format;
        if (config->channel_mask == 0)
            config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
        if (config->sample_rate == 0)
            config->sample_rate = pcm_config_default.rate;

        out->pcm_config.period_size        = pcm_config_default.period_size;
    }

    out->channel_mask                      = config->channel_mask;

    out->pcm_config.channels               = popcount(config->channel_mask);
    out->pcm_config.rate                   = config->sample_rate;
    out->pcm_config.format                 = config->format;
    out->pcm_config.period_count           = pcm_config_default.period_count;

    out->stream.common.get_sample_rate     = out_get_sample_rate;
    out->stream.common.set_sample_rate     = out_set_sample_rate;
    out->stream.common.get_buffer_size     = out_get_buffer_size;
    out->stream.common.get_channels        = out_get_channels;
    out->stream.common.get_format          = out_get_format;
    out->stream.common.set_format          = out_set_format;
    out->stream.common.standby             = out_standby;
    out->stream.common.dump                = out_dump;
    out->stream.common.set_parameters      = out_set_parameters;
    out->stream.common.get_parameters      = out_get_parameters;
    out->stream.common.add_audio_effect    = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency                = out_get_latency;
    out->stream.set_volume                 = out_set_volume;
    out->stream.write                      = out_write;
    out->stream.get_render_position        = out_get_render_position;
    out->stream.get_next_write_timestamp   = out_get_next_write_timestamp;

    out->dev                               = adev;
    config->format                         = out_get_format(&out->stream.common);
    config->channel_mask                   = out_get_channels(&out->stream.common);
    config->sample_rate                    = out_get_sample_rate(&out->stream.common);

    out->standby = true;
    adev->card = -1;
    adev->device = -1;

    *stream_out = &out->stream;
    ALOGV("%s exit",__func__);
    return 0;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    ALOGV("%s enter",__func__);
    out_standby(&stream->common);
    free(stream);
    ALOGV("%s exit",__func__);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    return 0;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
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
    int channel_count = popcount(config->channel_mask);

    return get_input_buffer_size(config->sample_rate, config->format, channel_count);
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
     struct stream_in *in = (struct stream_in *)stream;

     return in->pcm_config.rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return in->pcm_config.period_size * audio_stream_frame_size(stream);
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return in->channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    ALOGV("%s: enter", __func__);
    pthread_mutex_lock(&in->lock);
    if (!in->standby) {
        pthread_mutex_lock(&adev->lock);
        in->standby = true;
        if (in->pcm) {
            pcm_close(in->pcm);
            in->pcm = NULL;
        }
        pthread_mutex_unlock(&adev->lock);
    }
    pthread_mutex_unlock(&in->lock);
    ALOGV("%s: exit:  status(%d)", __func__, 0);
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    return -ENOSYS;
}

static char* in_get_parameters(const struct audio_stream *stream,
                               const char *keys)
{
    return strdup("");
}

static ssize_t in_read(struct audio_stream_in *stream, void *buffer,
                       size_t bytes)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    int i, ret = -1;

    ALOGV("%s enter",__func__);

    pthread_mutex_lock(&in->lock);
    if (in->standby) {
        pthread_mutex_lock(&adev->lock);
        ret = start_input_stream(in);
        pthread_mutex_unlock(&adev->lock);
        if (ret != 0) {
            goto exit;
        }
        in->standby = false;
    }

    if (in->pcm) {
        ret = pcm_read(in->pcm, buffer, bytes);
    }
    ALOGV("in_read returned %d bytes ret = %d",bytes,ret);

exit:
    pthread_mutex_unlock(&in->lock);

    if (ret != 0) {
        in_standby(&in->stream.common);
        uint64_t duration_ms = ((bytes * 1000)/
                                (audio_stream_frame_size(&in->stream.common)) /
                                (in_get_sample_rate(&in->stream.common)));
        ALOGV("%s : silence read - read failed", __func__);
        usleep(duration_ms * 1000);
    }
    ALOGV("%s exit",__func__);

    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in;
    int ret, buffer_size, frame_size;
    int channel_count = popcount(config->channel_mask);

    ALOGV("%s: enter", __func__);
    *stream_in = NULL;
    if (check_input_parameters(config->sample_rate, config->format, channel_count) != 0)
        return -EINVAL;

    in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    if (!in)
        return -ENOMEM;

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.standby = in_standby;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    in->device = devices;
    in->source = AUDIO_SOURCE_DEFAULT;
    in->dev = adev;
    in->standby = true;
    in->channel_mask = config->channel_mask;

    /* Update config params with the requested sample rate and channels */
    in->pcm_config = pcm_config_audio_capture;
    in->pcm_config.channels = channel_count;
    in->pcm_config.rate = config->sample_rate;

    frame_size = audio_stream_frame_size((struct audio_stream *)in);
    buffer_size = get_input_buffer_size(config->sample_rate,
                                        config->format,
                                        channel_count);
    in->pcm_config.period_size = buffer_size / frame_size;

    *stream_in = &in->stream;
    ALOGV("%s: exit", __func__);
    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    ALOGV("%s", __func__);

    in_standby(&stream->common);
    free(stream);

    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct audio_device *adev = (struct audio_device *)device;
    free(device);
    return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct audio_device *adev;
    int ret;

    ALOGV("%s enter",__func__);

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct audio_device));
    if (!adev)
        return -ENOMEM;

    adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->hw_device.common.module = (struct hw_module_t *) module;
    adev->hw_device.common.close = adev_close;

    adev->hw_device.init_check = adev_init_check;
    adev->hw_device.set_voice_volume = adev_set_voice_volume;
    adev->hw_device.set_master_volume = adev_set_master_volume;
    adev->hw_device.set_mode = adev_set_mode;
    adev->hw_device.set_mic_mute = adev_set_mic_mute;
    adev->hw_device.get_mic_mute = adev_get_mic_mute;
    adev->hw_device.set_parameters = adev_set_parameters;
    adev->hw_device.get_parameters = adev_get_parameters;
    adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->hw_device.open_output_stream = adev_open_output_stream;
    adev->hw_device.close_output_stream = adev_close_output_stream;
    adev->hw_device.open_input_stream = adev_open_input_stream;
    adev->hw_device.close_input_stream = adev_close_input_stream;
    adev->hw_device.dump = adev_dump;


    adev->active_pcm =  NULL;
    adev->hdaudio_active =  false;

    *device = &adev->hw_device.common;

    ALOGV("%s exit",__func__);

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "USB dgtl dock audio HW HAL",
        .author = "Intel :The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
