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

struct audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    int card;
    int device;
    bool standby;
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

    struct audio_device *dev;
};

/**
 * NOTE: when multiple mutexes have to be acquired, always respect the
 * following order: hw device > out stream
 */

/* Helper functions */

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
              ALOGV("alsacard %s", alsacard);
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
    ALOGD("Found USB card %s",alsacard);

    substr = strtok(alsacard,"[");
    substr = strtok(NULL,"]");
    ALOGV("filter 1 substr %s",substr);
    // remove spaces if any in the stripped name
    substr = strtok(substr," ");
    ALOGV("usb string substr %s",substr);

    if(found)
      return substr;
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
static int start_output_stream(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    int i;

    ALOGV("%s enter card %d device %d",__func__, adev->card, adev->device);
    if ((adev->card < 0) || (adev->device < 0)){
         adev->card = DEFAULT_CARD;
         adev->device = DEFAULT_DEVICE;
         ALOGV("%s: set card %d & device %d", __func__,adev->card,adev->device);
    }

   ALOGV("%s enter %d,%d,%d,%d,%d",__func__,
          out->pcm_config.channels,
          out->pcm_config.rate,
          out->pcm_config.period_size,
          out->pcm_config.period_count,
          out->pcm_config.format);

    out->pcm_config.start_threshold = 0;
    out->pcm_config.stop_threshold = 0;
    out->pcm_config.silence_threshold = 0;

    /*TODO - this needs to be updated once the device connect intent sends
      card, device id*/
    adev->card = get_card_number_by_name("USB Audio");
    ALOGD("%s: USB card number = %d, device = %d",__func__,adev->card,adev->device);

    out->pcm = pcm_open(adev->card, adev->device, PCM_OUT, &out->pcm_config);

    if (out->pcm && !pcm_is_ready(out->pcm)) {
        ALOGE("pcm_open() failed: %s", pcm_get_error(out->pcm));
        pcm_close(out->pcm);
        return -ENOMEM;
    }

    ALOGV("Initialized PCM device for channels %d",out->pcm_config.channels);
    ALOGV("%s exit",__func__);
    return 0;
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
    return AUDIO_CHANNEL_OUT_STEREO;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    ALOGV("%s enter standby = %d",__func__,out->standby);
    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);

    if (!out->standby) {
        pcm_close(out->pcm);
        out->pcm = NULL;
        out->standby = true;
        ALOGV("%s PCM device closed",__func__);
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

    ALOGV("%s enter",__func__);

    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);

    if (out->standby) {
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

    ALOGV("%s: pcm_write returned = %d",__func__,ret);

err:
    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);


    if (ret != 0) {
        usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
               out_get_sample_rate(&stream->common));
        ALOGV("%s Silence write",__func__);
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
    if (config->sample_rate == 0)
        config->sample_rate = pcm_config_default.rate;
    if (config->channel_mask == 0)
        config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;

    out->channel_mask                      = config->channel_mask;

    out->pcm_config.channels               = popcount(config->channel_mask);
    out->pcm_config.rate                   = config->sample_rate;
    out->pcm_config.period_size            = pcm_config_default.period_size;
    out->pcm_config.period_count           = pcm_config_default.period_count;
    out->pcm_config.format                 = pcm_config_default.format;

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

err_open:
    ALOGE("%s exit with error",__func__);
    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);
    free(out);
    *stream_out = NULL;
    return ret;
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
    return 0;
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
    return -ENOSYS;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
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
