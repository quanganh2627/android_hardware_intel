/*
 * Copyright (C) 2011 The Android Open Source Project
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

#define LOG_TAG "codec_offload_hw"
//#define LOG_NDEBUG 0
#include <media/AudioParameter.h>
#include <media/AudioSystem.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <linux/types.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <linux/poll.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <utils/threads.h>
#include <linux/sound/intel_sst_ioctl.h>
#include <compress_params.h>
#include <tinycompress.h>
#include <cutils/list.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <cutils/sched_policy.h>
#include "hardware_legacy/power.h"
extern "C" {
#include <cutils/str_parms.h>
}
#ifdef MRFLD_AUDIO
#include <tinyalsa/asoundlib.h>
#endif

#define _POSIX_SOURCE
#include <alsa/asoundlib.h>
#include <cutils/properties.h>

#define CODEC_OFFLOAD_BUFSIZE       (64*1024) /* Default buffer size in bytes */
#define CODEC_OFFLOAD_LATENCY       10      /* Default latency in mSec  */
#define CODEC_OFFLOAD_SAMPLINGRATE  48000     /* Default sampling rate in Hz */
#define CODEC_OFFLOAD_BITRATE       128000    /* Default bitrate in bps  */
#define CODEC_OFFLOAD_PCM_WORD_SIZE 16        /* Default PCM wordsize in bits */
#define CODEC_OFFLOAD_CHANNEL_COUNT 2         /* Default channel count */
#define OFFLOAD_TRANSFER_INTERVAL   8         /* Default intervel in sec */
#define OFFLOAD_MIN_ALLOWED_BUFSIZE (2*1024)   /*  bytes */
#define OFFLOAD_MAX_ALLOWED_BUFSIZE (128*1024) /*  bytes */

#define OFFLOAD_STREAM_DEFAULT_OUTPUT   2      /* Speaker */
#define MIXER_VOL_CTL_NAME "Compress Volume"
#define FILE_PATH "/proc/asound"

#ifdef MRFLD_AUDIO
/* -1440 is the value expected by vol lib for a gain of -144dB */
#define SST_VOLUME_MUTE 0xFA60 /* 2s complement of 1440 */
#else
#define SST_VOLUME_MUTE 0xA0
#define SST_VOLUME_TYPE 0x602
#define SST_VOLUME_SIZE 1
#define SST_PPP_VOL_STR_ID  0x03
#define SST_CODEC_VOLUME_CONTROL 0x67
#endif
#define CODEC_OFFLOAD_INPUT_BUFFERSIZE 320
using namespace android;
static char lockid_offload[32] = "codec_offload_hal";
enum {
    OFFLOAD_CMD_EXIT,               /* exit compress offload thread loop*/
    OFFLOAD_CMD_DRAIN,              /* send a full drain request to DSP */
    OFFLOAD_CMD_PARTIAL_DRAIN,      /* send a partial drain request to DSP */
    OFFLOAD_CMD_WAIT_FOR_BUFFER,    /* wait for buffer released by DSP */
};
/* stream states */
typedef enum {
    STREAM_CLOSED    = 0, /* Device not opened yet  */
    STREAM_OPEN      = 1, /* Device opened   */
    STREAM_READY     = 2, /* Device Ready to be written, mixer setting done */
    STREAM_RUNNING   = 3, /* STREAM_START done and write calls going  */
    STREAM_PAUSING   = 4, /* STREAM_PAUSE to call and stream is pausing */
    STREAM_RESUMING  = 5, /* STREAM_RESUME to call and is OK for writes */
    STREAM_DRAINING  = 6  /* STREAM_DRAINING to call and is OK for writes */
}sst_stream_states;
struct offload_cmd {
    struct listnode node;
    int cmd;
    int data[];
};
/* The data structure used for passing the codec specific information to the
* HAL offload hal for configuring the playback
*/
typedef struct{
        audio_format_t format;
        int32_t numChannels;
        int32_t sampleRate;
        int32_t bitsPerSample;
        int32_t avgBitRate;
    /* WMA -9 Specific */
        int32_t streamNumber;
        int32_t encryptedContentFlag;
        int32_t codecID;
        int32_t blockAlign;
        int32_t encodeOption;
    /* AAC Specific */
        int32_t downSampling;

}CodecInformation;

static CodecInformation mCodec;

struct offload_audio_device {
    struct audio_hw_device device;
    bool offload_init;
    uint32_t buffer_size;
    pthread_mutex_t lock;
    struct offload_stream_out *out;
    int  offload_out_ref_count;
};

struct offload_stream_out {
    audio_stream_out_t stream;
    pthread_cond_t  cond;
    sst_stream_states   state;
    int                 standby;
    struct compress     *compress;
    int                 fd;
    float               volume;
    bool                volume_change_requested;
    bool                muted;
    audio_format_t      format;
    uint32_t            sample_rate;
    uint32_t            buffer_size;
    uint32_t            channels;
    uint32_t            latency;
    uint32_t            adjusted_render_offset;
    uint32_t            paused_duration;
    int                 device_output;
    timer_t             paused_timer_id;
    pthread_mutex_t               lock;
    int non_blocking;
    int playback_started;
    pthread_cond_t offload_cond;
    pthread_t offload_thread;
    struct listnode offload_cmd_list;
    bool offload_thread_blocked;

    stream_callback_t offload_callback;
    void *offload_cookie;
    struct compr_gapless_mdata gapless_mdata;
    int send_new_metadata;
    int soundCardNo;
#ifdef AUDIO_OFFLOAD_SCALABILITY
    char mixVolumeCtl[PROPERTY_VALUE_MAX];
    char mixMuteCtl[PROPERTY_VALUE_MAX];
    char mixVolumeRampCtl[PROPERTY_VALUE_MAX];
#endif
};

/* The parameter structure used for getting and setting the volume
 */
struct offload_vol_algo_param {
    __u32 type;
    __u32 size;
    __u8  params;
}__attribute__((packed));

static size_t offload_dev_get_offload_buffer_size(
                                 const struct audio_hw_device *dev,
                                 uint32_t bitRate, uint32_t samplingRate,
                                 uint32_t channel);
static int destroy_offload_callback_thread(struct offload_stream_out *out);

static bool is_offload_device_available(
               struct offload_audio_device *offload_dev,
               audio_format_t format, uint32_t channels, uint32_t sample_rate)
{
    if (!offload_dev->offload_init) {
        ALOGW("is_offload_device_available: Offload device not initialized");
        return false;
    }

    ALOGV("is_offload_device_available format = %x", format);

    switch (format){
        case AUDIO_FORMAT_MP3:
        //case AUDIO_FORMAT_WMA9:
        case AUDIO_FORMAT_AAC:
            break;
        default:
            ALOGW("is_offload_device_available: Offload not possible for"
                       "format = %x", format);
            return false;
    }

    return true;
}

static int out_pause(struct audio_stream_out *stream)
{
     ALOGI("out_pause");
     struct offload_stream_out *out = (struct offload_stream_out *)stream;
     struct audio_stream_out *aout = (struct audio_stream_out *)stream;
     if(out->state != STREAM_RUNNING){
        ALOGV("out_pause ignored: the state = %d", out->state);
        return 0;
     }
     ALOGV("out_pause: out->state = %d", out->state );
     out->stream.get_render_position(aout, &out->paused_duration);

     pthread_mutex_lock(&out->lock);
     if(compress_pause(out->compress) < 0 ) {
         ALOGE("out_pause : failed in the compress pause Err=%s",
                compress_get_error(out->compress));
         pthread_mutex_unlock(&out->lock);
         return -ENOSYS;
     }
     out->state = STREAM_PAUSING;
     pthread_mutex_unlock(&out->lock);
     ALOGV("out_pause: out = %d", out->state );
     return 0;
}

static int out_resume( struct audio_stream_out *stream)
{
    ALOGI("out_resume");
    struct offload_stream_out *out = (struct offload_stream_out *)stream;

     if( out->state == STREAM_READY ) {
        ALOGV("out_resume ignored: the state = %d", out->state);
        return 0;
     }

    ALOGV("out_resume: the state = %d", out->state);
    pthread_mutex_lock(&out->lock);
    if( compress_resume(out->compress) < 0) {
        ALOGE("failed in the compress resume Err=%s",
                  compress_get_error(out->compress));
        pthread_mutex_unlock(&out->lock);
        return -ENOSYS;
    }
    out->state = STREAM_RUNNING;
    pthread_mutex_unlock(&out->lock);
    ALOGV("out_resume: out = %d", out->state);
    return 0;
}

static int close_device(struct audio_stream_out *stream)
{
    struct offload_stream_out *out = (struct offload_stream_out *)stream;
    ALOGI("close_device");
    pthread_mutex_lock(&out->lock);
    if( out->state == STREAM_DRAINING)
    {
        ALOGV("Close is called after partial drain, Call the darin");
        compress_drain(out->compress);
        ALOGV("Close: coming out of drain");
    }
    pthread_mutex_unlock(&out->lock);
    if (out->compress) {
        AudioParameter param;
        param.addInt(String8(AudioParameter::keyStreamFlags),
                                               AUDIO_OUTPUT_FLAG_NONE);
        ALOGV("close_device, setParam to indicate Offload is closing");
        AudioSystem::setParameters(0, param.toString());
        pthread_mutex_lock(&out->lock);
        ALOGV("close_device: compress_close");
        compress_close(out->compress);
        out->compress = NULL;
    }
#ifndef MRFLD_AUDIO
    if (out->fd) {
        close(out->fd);
        ALOGV("close_device: intel-sst- fd closed");
    }
    out->fd = 0;
#endif

    pthread_mutex_unlock(&out->lock);
    out->state = STREAM_CLOSED;
    return 0;
}
#ifdef AUDIO_OFFLOAD_SCALABILITY
static void open_dev_for_scalability(struct offload_stream_out *out)
{
    ALOGV("open_scalability_dev");
    // Read the property to get the mixer control names
    property_get("offload.mixer.volume.ctl.name", out->mixVolumeCtl, "0");
    property_get("offload.mixer.mute.ctl.name", out->mixMuteCtl, "0");
    property_get("offload.mixer.volume.ramp.ctl.name",
                                       out->mixVolumeRampCtl, "0");
    ALOGI("The mixer control name for volume = %s, mute = %s, Ramp = %s",
           out->mixVolumeCtl, out->mixMuteCtl, out->mixVolumeRampCtl);
}
static int out_set_volume_scalability(struct audio_stream_out *stream,
                          float left, float right)
{
    struct offload_stream_out *out = (struct offload_stream_out *)stream ;
    if(out->soundCardNo < 0) {
        ALOGE("out_set_volume_scalability: without sound card no %d open",
                                                        out->soundCardNo);
        return -EINVAL;
    }
    struct mixer *mixer;
    struct mixer_ctl* vol_ctl;
    uint16_t volume;
    mixer = mixer_open(out->soundCardNo);
    if (!mixer) {
        ALOGE("out_set_volume_scalability:Failed to open mixer for card %d\n",
                                                            out->soundCardNo);
        return -ENOSYS;
    }
    vol_ctl = mixer_get_ctl_by_name(mixer, MIXER_VOL_CTL_NAME);
    if (!vol_ctl) {
        ALOGE("out_set_volume_scalability: Error opening the mixer control %s",
                                              MIXER_VOL_CTL_NAME);
        mixer_close(mixer);
        return -EINVAL;
    }

    if (left == 0) {
        // Set the mute value for the FW i.e -144dB
       volume = SST_VOLUME_MUTE; //2s compliment of -144 dB
       vol_ctl = mixer_get_ctl_by_name(mixer, out->mixMuteCtl);
       if (!vol_ctl) {
            ALOGE("out_set_volume_scalability:Error opening mixerMutecontrol%s",
                                              out->mixMuteCtl);
            mixer_close(mixer);
            return -EINVAL;
       }
    } else {
       // gain library expects user input of integer gain in 0.1dB
       // Eg., 60 in decimal represents 6dB
       volume = (uint16_t)((20 * log10(left)) * 10);
       vol_ctl = mixer_get_ctl_by_name(mixer, out->mixVolumeCtl);
       if (!vol_ctl) {
            ALOGE("out_set_volume_scalability:Error"
                     "opening mixerVolumecontrol %s", out->mixVolumeCtl);
            mixer_close(mixer);
            return -EINVAL;
       }
    }
    ALOGV("out_set_volume_scalability: volume computed: %d", volume);
    if ((mixer_ctl_get_value(vol_ctl, 0)) == volume) {
        ALOGV("out_set_volume_scalability: No update since volume requested");
        return 0;
    }

    // Call the mixer control for ramp also.
    // TBD: how to get ramp value? default is 0
    struct mixer_ctl* volRamp_ctl;
    uint16_t volumeRamp = 0;
    volRamp_ctl = mixer_get_ctl_by_name(mixer, out->mixVolumeRampCtl);
    if (!volRamp_ctl) {
        ALOGE("out_set_volume_scalability:Error opening mixerVolRamp ctl %s",
                                              out->mixVolumeRampCtl);
        mixer_close(mixer);
        return -EINVAL;
    }
    int retval = mixer_ctl_set_value(volRamp_ctl, 0, volumeRamp);
    if (retval < 0) {
        ALOGE("cwout_set_volume_scalability: Error setting volumeRamp val%x",
                                                   volumeRamp);
        return retval;
    }
    int retval1 = mixer_ctl_set_value(vol_ctl, 0, volume);
    if (retval1 < 0) {
        ALOGE("out_set_volume_scalability: Err setting volume dB value %x",
                                                volume);
        return retval1;
    }
    ALOGV("out_set_volume_scalability: Successful in set volume=%2f (%x dB)",
                                              left, volume);
    return 0;
}
#endif

static int open_device(struct offload_stream_out *out)
{
    int card  = -1;
    int err = 0;
    struct compr_config config;
    struct snd_codec codec;
    char value[PROPERTY_VALUE_MAX];
    char id_filepath[PATH_MAX] = {0};
    char number_filepath[PATH_MAX] = {0};
    ssize_t written;
    // set the audio.device.name property in the init.<boardname>.rc file
    // or set the property at runtime in adb shell using setprop
    property_get("audio.device.name", value, "0");
    snprintf(id_filepath, sizeof(id_filepath), FILE_PATH"/%s", value);
    written = readlink(id_filepath, number_filepath, sizeof(number_filepath));
    if (written < 0) {
        ALOGE("open_device:Sound card %s does not exist", value);
        return -EINVAL;
    } else if (written >= (ssize_t)sizeof(id_filepath)) {
        ALOGE("open_device:Sound card %s Name too long", value);
        return -EINVAL;
    }
    // We are assured, because of the check in the previous elseif, that this
    // buffer is null-terminated.  So this call is safe.
    // 4 == strlen("card")
    card = atoi(number_filepath + 4);
    out->soundCardNo = card;
    property_get("offload.compress.device", value, "0");
    int device = atoi(value);

#ifdef AUDIO_OFFLOAD_SCALABILITY
    open_dev_for_scalability(out);
#endif

    ALOGV("open_device: device %d", device);
    if (out->state != STREAM_CLOSED) {
        ALOGE("open[%d] Error with stream state", out->state);
        return -EINVAL;
    }
    // update the configuration structure for given type of stream
    if (out->format == AUDIO_FORMAT_MP3) {
        codec.id = SND_AUDIOCODEC_MP3;
        /* the channel maks is the one that come to hal. Converting the mask to channel number */
        int channel_count = popcount(out->channels);
#ifndef MRFLD_AUDIO
        if (channel_count > 2) {
            channel_count = 1;
        }
#endif
        codec.ch_out = channel_count;
        codec.ch_in = channel_count;
        codec.sample_rate =  out->sample_rate;
        codec.bit_rate = mCodec.avgBitRate ? mCodec.avgBitRate : CODEC_OFFLOAD_BITRATE;
        codec.rate_control = 0;
        codec.profile = 0;
        codec.level = 0;
        codec.ch_mode = 0;
        codec.format = 0;

        ALOGI("open_device: params: codec.id =%d,codec.ch_in=%d,codec.ch_out=%d,"
          "codec.sample_rate=%d, codec.bit_rate=%d,codec.rate_control=%d,"
          "codec.profile=%d,codec.level=%d,codec.ch_mode=%d,codec.format=%x",
          codec.id, codec.ch_in,codec.ch_out,codec.sample_rate,
          codec.bit_rate, codec.rate_control, codec.profile,
          codec.level,codec.ch_mode, codec.format);

    } else if (out->format == AUDIO_FORMAT_AAC) {

        /* AAC codec parameters  */
        codec.id = SND_AUDIOCODEC_AAC;
        /* Converting the mask to channel number */
        int channel_count = popcount(out->channels);
#ifndef MRFLD_AUDIO
        if (channel_count > 2) {
            channel_count = 1;
        }
#endif
        codec.ch_out = channel_count;
        codec.ch_in = channel_count;
        codec.sample_rate =  out->sample_rate;
        codec.bit_rate = mCodec.avgBitRate ? mCodec.avgBitRate : CODEC_OFFLOAD_BITRATE;
        codec.rate_control = 0;
        codec.profile = SND_AUDIOPROFILE_AAC;
        codec.level = 0;
        codec.ch_mode = 0;
        codec.format = SND_AUDIOSTREAMFORMAT_RAW;

        ALOGI("open_device: params: codec.id =%d,codec.ch_in=%d,codec.ch_out=%d,"
          "codec.sample_rate=%d, codec.bit_rate=%d,codec.rate_control=%d,"
          "codec.profile=%d,codec.level=%d,codec.ch_mode=%d,codec.format=%x",
          codec.id, codec.ch_in,codec.ch_out,codec.sample_rate,
          codec.bit_rate, codec.rate_control, codec.profile,
          codec.level,codec.ch_mode, codec.format);
    }
    config.fragment_size = out->buffer_size;
    config.fragments = 2;
    config.codec = &codec;

    acquire_wake_lock(PARTIAL_WAKE_LOCK, lockid_offload);
    out->compress = compress_open(card, device, COMPRESS_IN, &config);
    if (!out->compress || !is_compress_ready(out->compress)) {
        ALOGE("open_device: compress_open Error  %s\n",
                                  compress_get_error(out->compress));
        ALOGE("open_device:Unable to open Compress device %d:%d\n",
                                  card, out->device_output);
        compress_close(out->compress);
        release_wake_lock(lockid_offload);
        return -EINVAL;
    }
    ALOGV("open_device: Compress device opened sucessfully");
    ALOGV("open_device: setting compress non block");
    compress_nonblock(out->compress, out->non_blocking);
#ifndef MRFLD_AUDIO
    out->fd = open("/dev/intel_sst_ctrl", O_RDWR);
    if (out->fd < 0) {
        ALOGE("error opening LPE device, error = %d",out->fd);
        close_device(&out->stream);
        release_wake_lock(lockid_offload);
        //pthread_mutex_unlock(&out->lock);
        return -EIO;
    }
    ALOGV("open_device: intel_sst_ctrl opened sucessuflly with fd=%d", out->fd);
#endif
    return 0;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    ALOGV("out_get_sample_rate:");
    struct offload_stream_out *out = (struct offload_stream_out *)stream;
    return out->sample_rate ?: CODEC_OFFLOAD_SAMPLINGRATE;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct offload_stream_out *out = (struct offload_stream_out *)stream;
    ALOGV("out_get_buffer_size: out->buffer_size %d", out->buffer_size);
    return out->buffer_size;
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    ALOGV("out_get_channels:");
    struct offload_stream_out *out = (struct offload_stream_out *)stream;
    return out->channels;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    struct offload_stream_out *out = (struct offload_stream_out *)stream;
    return out->format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    return -1;
}

static void stop_compressed_output_l(struct offload_stream_out *out)
{
    //out->offload_state = OFFLOAD_STATE_IDLE;
    out->playback_started = 0;
    out->send_new_metadata = 1;
    if (out->compress != NULL) {
        compress_stop(out->compress);
        while (out->offload_thread_blocked) {
            pthread_cond_wait(&out->cond, &out->lock);
        }
    }
}
static int out_standby(struct audio_stream *stream)
{
   struct offload_stream_out *out = (struct offload_stream_out *)stream;

    pthread_mutex_lock(&out->lock);
    if (!out->standby) {
        out->standby = true;
        stop_compressed_output_l(out);
        out->gapless_mdata.encoder_delay = 0;
        out->gapless_mdata.encoder_padding = 0;
    }
    pthread_mutex_unlock(&out->lock);
    close_device((audio_stream_out*)stream);
    ALOGV("%s: exit", __func__);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    ALOGV("out_set_parameters kvpairs = %s", kvpairs);

    struct str_parms *param;
    int value = 0;
    int delay = -1;
    int padding = -1;

    struct offload_stream_out *out = (struct offload_stream_out*)stream;

    param = str_parms_create_str(kvpairs);
    if (param == NULL) {
        ALOGE("out_set_parameters: Error creating str from kvpairs");
        return 0;
    }

    // Bits per sample - for WMA
    if (str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_BIT_PER_SAMPLE, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_BIT_PER_SAMPLE);
        mCodec.bitsPerSample = value;
    }
    // Avg bitrate in bps - for WMA/AAC/MP3
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_AVG_BIT_RATE, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_AVG_BIT_RATE);
        mCodec.avgBitRate = value;
        ALOGV("average bit rate set to %d", mCodec.avgBitRate);
    }
    // Number of channels present (for AAC)
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_NUM_CHANNEL, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_NUM_CHANNEL);
        mCodec.numChannels = value;
    }
    // Codec ID tag - Represents AudioObjectType (AAC) and FormatTag (WMA)
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_ID, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_ID);
        mCodec.codecID = value;
    }
    // Block Align - for WMA
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_BLOCK_ALIGN, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_BLOCK_ALIGN);
        mCodec.blockAlign = value;
    }
    // Sample rate - for WMA/AAC direct from parser
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_SAMPLE_RATE, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_SAMPLE_RATE);
        mCodec.sampleRate = value;
    }
    // Encode Option - for WMA
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_ENCODE_OPTION, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_ENCODE_OPTION);
        mCodec.encodeOption = value;
    }
    // Delay samples - for MP3
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_DELAY_SAMPLES, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_DELAY_SAMPLES);
        delay = value;
    }
    // Padding samples - for MP3
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_PADDING_SAMPLES, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_PADDING_SAMPLES);
        padding = value;
    }
    if (delay >= 0 && padding >= 0) {
        ALOGV("set_param: setting delay %d, padding %d", delay, padding);
        out->gapless_mdata.encoder_delay = delay;
        out->gapless_mdata.encoder_padding = padding;
        out->send_new_metadata = 1;
    }
    str_parms_destroy(param);
    return 0;
}

static char* out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    char *temp = NULL;
    struct str_parms *param;
    char value[256];
    struct offload_stream_out *out = (struct offload_stream_out *)stream;
    int ret = 0;

    param = str_parms_create_str(keys);
    if (param == NULL) {
        ALOGE("out_get_parameters: Error creating str from keys");
        return NULL;
    }

    if (str_parms_get_str(param, AUDIO_PARAMETER_STREAM_ROUTING, value,
                                strlen(AUDIO_PARAMETER_STREAM_ROUTING)) >= 0) {
        ret = str_parms_add_int(param, AUDIO_PARAMETER_STREAM_ROUTING,
                                            out->device_output);
        if (ret >= 0) {
            temp = str_parms_to_str(param);
        } else {
            temp = strdup(keys);
        }
    }
    str_parms_destroy(param);
    ALOGV("out_get_parameters: %s", temp);
    return temp;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    return CODEC_OFFLOAD_LATENCY;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    int ret = 0;
    ALOGV("out_set_volume right vol= %f, left vol = %f", right, left);
    // Check the boundary conditions and apply volume to LPE
    if (left < 0.0f || left > 1.0f) {
        ALOGE("setVolume: Invalid data as vol=%f ", left);
        return -EINVAL;
    }

    struct offload_stream_out *out = (struct offload_stream_out *)stream ;
    out->volume = left; // needed if we set volume in  out_write
    // If error happens during setting the volume, try to set while in out_write
    out->volume_change_requested = true;
#ifndef MRFLD_AUDIO
    // Device could be in standby state. Once active, set new volume
    if (!out->fd){
        ALOGV("setVolume: Requested for %2f, but yet to service when active", left);
        return 0;
    }

    pthread_mutex_lock(&out->lock);

    struct offload_vol_algo_param  sst_vol;

    ALOGV("setVolume Requested for %2f", left);
    if (left == 0) {
        /* Set the mute value for the FW i.e -96dB */
        sst_vol.params = SST_VOLUME_MUTE; //2s compliment of -96 dB
    } else {
        sst_vol.params =  (uint8_t)(20*log10(left));
    }
    sst_vol.type = SST_VOLUME_TYPE;
    sst_vol.size = SST_VOLUME_SIZE;


    out->volume  = left;
    out->volume_change_requested = false;
    ALOGV("setVolume:  volume=%x in 2s compliment", sst_vol.params);

   // Incase if device is already set with same volume, we can ignore this request
    struct offload_vol_algo_param  sst_get_vol;
    sst_get_vol.type = SST_VOLUME_TYPE; // 0x602;
    sst_get_vol.size = SST_VOLUME_SIZE; // 1;

    struct snd_ppp_params  sst_ppp_get_vol;
    sst_ppp_get_vol.algo_id = SST_CODEC_VOLUME_CONTROL; // 0x67
    sst_ppp_get_vol.str_id = SST_PPP_VOL_STR_ID; // 0x03;
    sst_ppp_get_vol.enable = 1;
    sst_ppp_get_vol.operation = 1;
    sst_ppp_get_vol.size =  sizeof(struct offload_vol_algo_param);
    sst_ppp_get_vol.params = &sst_get_vol;
    ALOGV("calling get volume");
    if (ioctl(out->fd, SNDRV_SST_GET_ALGO, &sst_ppp_get_vol) >=0) {
        ALOGV("setVolume:  The volume read by getvolume stream_id =%d, volume = %d dB",
                            sst_ppp_get_vol.str_id, sst_get_vol.params);
        if (sst_get_vol.params == sst_vol.params) {
            pthread_mutex_unlock(&out->lock);
            ALOGV("setVolume: No update since volume requested matches to one in the system.");
            out->volume_change_requested = false;
            return 0;
        }
    }
    ALOGV("setVolume:  The volume read by getvolume stream_id =%d, volume = %d dB",
                            sst_ppp_get_vol.str_id, sst_get_vol.params);
    struct snd_ppp_params  sst_ppp_vol;
    sst_ppp_vol.algo_id = SST_CODEC_VOLUME_CONTROL;
    sst_ppp_vol.str_id = SST_PPP_VOL_STR_ID; // 0x03;
    sst_ppp_vol.enable = 1;
    sst_ppp_vol.operation = 0;
    sst_ppp_vol.size =  sizeof(struct offload_vol_algo_param);
    sst_ppp_vol.params = &sst_vol;

    // Proceed doing the new setVolume to the LPE device
    int retval = ioctl(out->fd, SNDRV_SST_SET_ALGO, &sst_ppp_vol);
    if (retval <0) {
        ALOGE("setVolume: Error setting the ioctl with dB=%x", sst_vol.params);
                pthread_mutex_unlock(&out->lock);
        return retval;
    }
    ALOGV("setVolume: Successful in set volume=%2f (%x dB)", left, sst_vol.params);
    pthread_mutex_unlock(&out->lock);
#else //MRFLD_AUDIO
#ifdef AUDIO_OFFLOAD_SCALABILITY
    // Read the property to see if scalability is enabled in system.
    // use this to set the mixer controls if enabled.
    char propValue[PROPERTY_VALUE_MAX];
    if ((property_get("audio.offload.scalability", propValue, "0")) &&
        (atoi(propValue) == 1)) {
        ALOGI("setVolume: Calling out_set_volume_scalability");
        return out_set_volume_scalability(stream, left, right);
    }
#endif
    if(out->soundCardNo < 0) {
        ALOGE("setVolume: without sound card no %d open", out->soundCardNo);
        return -EINVAL;
    }
    struct mixer *mixer;
    struct mixer_ctl* vol_ctl;
    uint16_t volume;
    mixer = mixer_open(out->soundCardNo);
    if (mixer == NULL) {
        ALOGE("setVolume:Failed to open mixer for card %d\n", out->soundCardNo);
        return -ENOSYS;
    }
    vol_ctl = mixer_get_ctl_by_name(mixer, MIXER_VOL_CTL_NAME);
    if (!vol_ctl) {
        ALOGE("setVolume: Error opening the mixer control %s",
                                              MIXER_VOL_CTL_NAME);
        mixer_close(mixer);
        return -EINVAL;
    }

    if (left == 0) {
        // Set the mute value for the FW i.e -144dB
       volume = SST_VOLUME_MUTE; //2s compliment of -144 dB
    } else {
       // gain library expects user input of integer gain in 0.1dB
       // Eg., 60 in decimal represents 6dB
       volume = (uint16_t)((20 * log10(left)) * 10);
    }
    ALOGV("setVolume: volume computed: %d", volume);
    if ((mixer_ctl_get_value(vol_ctl, 0)) == volume) {
        ALOGV("setVolume: No update since volume requested matches to one in the system");
        mixer_close(mixer);
        out->volume_change_requested = false;
        return 0;
    }

    int retval = mixer_ctl_set_value(vol_ctl, 0, volume);
    if (retval < 0) {
        ALOGE("setVolume: Error setting volume with dB value %x", volume);
        mixer_close(mixer);
        return retval;
    }
    ALOGV("setVolume: Successful in set volume=%2f (%x dB)", left, volume);
    mixer_close(mixer);
#endif  //MRFLD_AUDIO
    // Volume sucessfully set, no need to do again during out_write
    out->volume_change_requested = false;
    return ret;
}

static int send_offload_cmd_l(struct offload_stream_out* out, int command)
{
    struct offload_cmd *cmd = (struct offload_cmd *)calloc(1, sizeof(struct offload_cmd));
    if (!cmd) {
        ALOGV("send_offload_cmd_l NO_MEMORY");
        return -ENOMEM;
    }

    ALOGV("%s %d", __func__, command);

    cmd->cmd = command;
    list_add_tail(&out->offload_cmd_list, &cmd->node);
    pthread_cond_signal(&out->offload_cond);
    return 0;
}
static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    struct offload_stream_out *out = (struct offload_stream_out *)stream;
    AudioParameter param;
    if (out->standby) {
        if (open_device(out)) {
            ALOGE("out_write[%d]: Device open error", out->state);
            close_device(stream);
            return -EINVAL;
        }
        out->standby = false;
        out->state = STREAM_OPEN;
    }
    if (out->send_new_metadata) {
        if ((compress_set_gapless_metadata(out->compress, &out->gapless_mdata)) < 0 ) {
            ALOGE("set_param error in setting meta data %s",
                                compress_get_error(out->compress));
            return -EINVAL;
        }
        out->send_new_metadata = 0;
    }

    int sent = 0;
    int retval = 0;

    ALOGV("out_write: state = %d", out->state);
    switch (out->state) {
        case STREAM_CLOSED:
            // Due to standby the device could be closed (power-saving mode)
            if (open_device(out)) {
                ALOGE("out_write[%d]: Device open error", out->state);
                close_device(stream);
                return retval;
            }
        case STREAM_OPEN:
            ALOGV("out_write:Indicating primary HAL about offload starting");
            param.addInt(String8(AudioParameter::keyStreamFlags),
                                   AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD);
            AudioSystem::setParameters(0, param.toString());
            out->state = STREAM_READY;
        case STREAM_READY:
        case STREAM_DRAINING:
            if (out->volume_change_requested) {
                out->stream.set_volume(&out->stream,out->volume, out->volume);
            }
            ALOGV("out_write: state = %d: writting %d bytes", out->state, bytes);
            sent = compress_write(out->compress, buffer, bytes);
            if ((sent >= 0) && (sent < (int)bytes)) {
                 ALOGV("out_write sending wait for buffer cmd");
                 send_offload_cmd_l(out, OFFLOAD_CMD_WAIT_FOR_BUFFER);
            }
            if (sent < 0) {
                ALOGE("Error: %s\n", compress_get_error(out->compress));
            }
            ALOGV("out_write: state = %d: writting Done with %d bytes",
                                                       out->state, sent);
            if (compress_start(out->compress) < 0) {
                ALOGI("write: Failed in the compress_start Err=%s",
                           compress_get_error(out->compress));
            }
            ALOGI("out_write[%d]: compress_start in state", out->state);
            out->state = STREAM_RUNNING;
            break;
        case STREAM_RUNNING:
            if (out->volume_change_requested) {
                out->stream.set_volume(&out->stream,out->volume, out->volume);
            }
            ALOGV("out_write:[%d] Writing to compress write with %d bytes..",
                                                           out->state, bytes);
            sent = compress_write(out->compress, buffer, bytes);
            if ((sent >= 0) && (sent < (int)bytes)) {
                 send_offload_cmd_l(out, OFFLOAD_CMD_WAIT_FOR_BUFFER);
            }
            if (sent < 0) {
                ALOGE("out_write:[%d] compress_write: interrupted : %s",
                                out->state, compress_get_error(out->compress));
                sent = 0;
            }
            ALOGV("out_write:[%d] written %d bytes now",
                                              out->state, (int) sent);
            break;

        default:
            ALOGW("out_write[%d]: Ignored", out->state);
            return retval;
    }
    return sent;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    unsigned int avail;
    struct timespec tstamp;
    uint32_t calTimeMs;
    struct offload_stream_out *out = (struct offload_stream_out *)stream;

    *dsp_frames = out->adjusted_render_offset;
    if (!out->compress)  {
        ALOGV("out_get_render_position: Retuning without calling compressAPI");
        return 0;
    }

    pthread_mutex_lock(&out->lock);
    switch (out->state) {
        case STREAM_RUNNING:
        case STREAM_READY:
        case STREAM_PAUSING:
        case STREAM_DRAINING:
            if (compress_get_hpointer(out->compress, &avail,&tstamp) < 0) {
                ALOGW("out_get_render_position: get_hposition Failed Err=%s",
                      compress_get_error(out->compress));
                pthread_mutex_unlock(&out->lock);
                return -EINVAL;
            }

          calTimeMs = (tstamp.tv_sec * 1000) + (tstamp.tv_nsec /1000000);
          *dsp_frames +=calTimeMs;
          ALOGV("out_get_render_position : time in millisec returned = %d",
                                                                *dsp_frames);
        break;
        default:
            pthread_mutex_unlock(&out->lock);
            return -EINVAL;
    }
    pthread_mutex_unlock(&out->lock);
    return 0;
}

static int out_set_callback(struct audio_stream_out *stream,
            stream_callback_t callback, void *cookie)
{
    struct offload_stream_out *out = (struct offload_stream_out *)stream;

    ALOGV("%s", __func__);
    pthread_mutex_lock(&out->lock);
    out->offload_callback = callback;
    out->offload_cookie = cookie;
    pthread_mutex_unlock(&out->lock);
    return 0;
}
/* The drain function implementation. This will send drain to driver */
static int out_drain(struct audio_stream_out *stream,
                     audio_drain_type_t type)
{
    ALOGV("out_drain");
    struct offload_stream_out *out = (struct offload_stream_out *)stream ;
    int status = -ENOSYS;
    pthread_mutex_lock(&out->lock);
    if ((type == AUDIO_DRAIN_EARLY_NOTIFY)) {
        ALOGV("out_drain send command PARTIAL_DRAIN");
        status = send_offload_cmd_l(out, OFFLOAD_CMD_PARTIAL_DRAIN);
    }
    else {
        ALOGV("out_drain send command DRAIN");
        status = send_offload_cmd_l(out, OFFLOAD_CMD_DRAIN);
    }
    pthread_mutex_unlock(&out->lock);
    ALOGV("out_drain return status %d", status);
    return status;

}

static int out_flush (const struct audio_stream_out *stream)
{
   struct offload_stream_out *out = (struct offload_stream_out *)stream;
   switch (out->state) {
        case STREAM_RUNNING:
        case STREAM_READY:
        case STREAM_DRAINING:
            break;
        case STREAM_PAUSING:
            out->adjusted_render_offset = 0;
            break;
        default :
            ALOGV("out_flush: ignored");
            out->adjusted_render_offset = 0;
            return 0;
    }
    ALOGV("out_flush:[%d] calling Compress Stop", out->state);
    pthread_mutex_lock(&out->lock);
    stop_compressed_output_l(out);
    pthread_mutex_unlock(&out->lock);
    out->state = STREAM_READY;
    return 0;
}

static void *offload_thread_loop(void *context)
{
    struct offload_stream_out *out = (struct offload_stream_out *) context;
    struct listnode *item;

//    out->offload_state = OFFLOAD_STATE_IDLE;
    out->playback_started = 0;

    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);
    set_sched_policy(0, SP_FOREGROUND);
    prctl(PR_SET_NAME, (unsigned long)"Offload Callback", 0, 0, 0);

    ALOGV("%s", __func__);
    pthread_mutex_lock(&out->lock);
    for (;;) {
        struct offload_cmd *cmd = NULL;
        stream_callback_event_t event;
        bool send_callback = false;

        ALOGV("%s offload_cmd_list %d",
              __func__, list_empty(&out->offload_cmd_list));
        if (list_empty(&out->offload_cmd_list)) {
            ALOGV("%s SLEEPING", __func__);
            pthread_cond_wait(&out->offload_cond, &out->lock);
            ALOGV("%s RUNNING", __func__);
            continue;
        }

        item = list_head(&out->offload_cmd_list);
        cmd = node_to_item(item, struct offload_cmd, node);
        list_remove(item);

        ALOGV("%s CMD %d out->compress %p",
               __func__, cmd->cmd, out->compress);

        if (cmd->cmd == OFFLOAD_CMD_EXIT) {
            free(cmd);
            break;
        }

        if (out->compress == NULL) {
            ALOGE("%s: Compress handle is NULL", __func__);
            pthread_cond_signal(&out->cond);
            continue;
        }
        out->offload_thread_blocked = true;
        pthread_mutex_unlock(&out->lock);
        send_callback = false;
        switch(cmd->cmd) {
        case OFFLOAD_CMD_WAIT_FOR_BUFFER:
            ALOGV("OFFLOAD_CMD_WAIT_FOR_BUFFER waiting on Compress_wait");
            compress_wait(out->compress, -1);
            ALOGV("OFFLOAD_CMD_WAIT_FOR_BUFFER coming out of Compress_wait");
            send_callback = true;
            event = STREAM_CBK_EVENT_WRITE_READY;
            break;
        case OFFLOAD_CMD_PARTIAL_DRAIN:
            ALOGV("OFFLOAD_CMD_PARTIAL_DRAIN: Calling compress_next_track");
            compress_next_track(out->compress);
            ALOGV("OFFLOAD_CMD_PARTIAL_DRAIN: Calling compress_drain");
            compress_partial_drain(out->compress);
            send_callback = true;
            event = STREAM_CBK_EVENT_DRAIN_READY;
            out->state = STREAM_DRAINING;
            break;
        case OFFLOAD_CMD_DRAIN:
            ALOGV("OFFLOAD_CMD_DRAIN: calling compress_drain");
            compress_drain(out->compress);
            send_callback = true;
            event = STREAM_CBK_EVENT_DRAIN_READY;
            out->state = STREAM_READY;
            break;
        default:
            ALOGE("%s unknown command received: %d", __func__, cmd->cmd);
            break;
        }
        pthread_mutex_lock(&out->lock);
        out->offload_thread_blocked = false;
        pthread_cond_signal(&out->cond);
        if (send_callback) {
            ALOGV("offload_thread_loop sending callback event %d", event);
            out->offload_callback(event, NULL, out->offload_cookie);
        }
        free(cmd);
    }

    pthread_cond_signal(&out->cond);
    while (!list_empty(&out->offload_cmd_list)) {
        item = list_head(&out->offload_cmd_list);
        list_remove(item);
        free(node_to_item(item, struct offload_cmd, node));
    }
    pthread_mutex_unlock(&out->lock);

    return NULL;
}
static int create_offload_callback_thread(struct offload_stream_out *out)
{
    pthread_cond_init(&out->offload_cond, (const pthread_condattr_t *) NULL);
    list_init(&out->offload_cmd_list);
    pthread_create(&out->offload_thread, (const pthread_attr_t *) NULL,
                    offload_thread_loop, out);
    return 0;
}
static int offload_dev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out)
{
    ALOGV("offload_dev_open_output_stream:  flag = 0x%x", flags);

    if (!(flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD)) {
        ALOGV("offload_dev_open_output_stream: Not for Offload. Returning");
        return -ENOSYS;
    }
    struct offload_audio_device *loffload_dev =
                                (struct offload_audio_device *)dev;
    struct offload_stream_out *out;
    int ret;
    if(loffload_dev->offload_out_ref_count == 1) {
        ALOGE("offload_dev_open_output_stream: Already device open");
        return -EINVAL;
    }

    out = (struct offload_stream_out *)
                        calloc(1, sizeof(struct offload_stream_out));
    if (!out) {
        ALOGV("offload_dev_open_output_stream NO_MEMORY");
        return -ENOMEM;
    }

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
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.set_callback = out_set_callback;
    out->stream.pause = out_pause;
    out->stream.resume = out_resume;
    out->stream.drain = out_drain;
    out->stream.flush = out_flush;
    if (flags & AUDIO_OUTPUT_FLAG_NON_BLOCKING) {
        ALOGV("offload_dev_open_output_stream: setting non-blocking to 1");
        out->non_blocking = 1;
    }
    ALOGV("offload_dev_open_output_stream: creating callback");
    create_offload_callback_thread(out);
    *stream_out = &out->stream;
    // initialize stream parameters
    out->format = config->format;
    out->sample_rate = config->sample_rate;
    out->channels = config->channel_mask;
    out->buffer_size = offload_dev_get_offload_buffer_size(dev,
                                               config->offload_info.bit_rate,
                                               config->sample_rate,
                                               config->channel_mask);
    //Default route is done for offload and let primary HAL do the routing
    out->device_output = OFFLOAD_STREAM_DEFAULT_OUTPUT;
    ret = open_device(out);
    if (ret != 0) {
        ALOGE("offload_dev_open_output_stream: open_device error");
        goto err_open;
    }

    loffload_dev->offload_out_ref_count += 1;
    loffload_dev->out = out;

    ALOGV("offload_dev_open_output_stream: offload device opened");

    out->standby = false;
    out->state = STREAM_OPEN;
    return 0;

err_open:
    ALOGE("offload_dev_open_output_stream -> err_open:");
    destroy_offload_callback_thread(out);
    free(out);
    *stream_out = NULL;
    return ret;
}

static int destroy_offload_callback_thread(struct offload_stream_out *out)
{
    ALOGI("destroy_offload_callback_thread");
    pthread_mutex_lock(&out->lock);
    stop_compressed_output_l(out);
    send_offload_cmd_l(out, OFFLOAD_CMD_EXIT);

    pthread_mutex_unlock(&out->lock);
    pthread_join(out->offload_thread, (void **) NULL);
    pthread_cond_destroy(&out->offload_cond);

    return 0;
}
static void offload_dev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct offload_audio_device *loffload_dev = (struct offload_audio_device *)dev;
    struct offload_stream_out *out = (struct offload_stream_out *)stream;
    ALOGV("offload_dev_close_output_stream");
    out_standby(&stream->common);
    destroy_offload_callback_thread(out);
    pthread_cond_destroy(&out->cond);
    pthread_mutex_destroy(&out->lock);
    //close_device(stream);
    loffload_dev->offload_out_ref_count = 0;
    free(stream);
    release_wake_lock(lockid_offload);
}

static int offload_dev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    ALOGV("offload_dev_set_parameters kvpairs = %s", kvpairs);

    struct str_parms *param;
    int value=0;

    param = str_parms_create_str(kvpairs);
    if (param == NULL) {
        ALOGE("offload_dev_set_parameters: Error creating str from kvpairs");
        return 0;
    }

    // Avg bitrate in bps - for WMA/AAC/MP3
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_AVG_BIT_RATE, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_AVG_BIT_RATE);
        mCodec.avgBitRate = value;
        ALOGV("offload_dev_set_parameters: average bit rate %d", mCodec.avgBitRate);
    }
    // Sample rate - for WMA/AAC direct from parser
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_SAMPLE_RATE, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_SAMPLE_RATE);
        mCodec.sampleRate = value;
        ALOGV("offload_dev_set_parameters: sample rate %d", mCodec.sampleRate);
    }
    // Number of channels present (for AAC)
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_NUM_CHANNEL, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_NUM_CHANNEL);
        mCodec.numChannels = value;
        ALOGV("offload_dev_set_parameters: num of channels %d", mCodec.numChannels);
    }
    // Codec ID tag - Represents AudioObjectType (AAC)
    if ( str_parms_get_int(param, AUDIO_OFFLOAD_CODEC_ID, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_ID);
        mCodec.codecID = value;
    }
    if ( str_parms_get_int(
            param, AUDIO_OFFLOAD_CODEC_DOWN_SAMPLING, &value) >= 0) {
        str_parms_del(param, AUDIO_OFFLOAD_CODEC_DOWN_SAMPLING);
        mCodec.downSampling = value;
    }
    str_parms_destroy(param);
    return 0;
}

static char * offload_dev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    return NULL;
}
// TBD - Do we need to open the compress device do init check ???
static int offload_dev_init_check(const struct audio_hw_device *dev)
{
    struct offload_audio_device *loffload_dev = (struct offload_audio_device *)dev;

    ALOGV("offload_dev_init_check");
    loffload_dev->offload_init = true;
    ALOGV("Proxy: initCheck successful");
    return 0;
}

static int offload_dev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int offload_dev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int offload_dev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    return 0;
}

static size_t offload_dev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         uint32_t sample_rate, audio_format_t format,
                                         int channel_count)
{
    return CODEC_OFFLOAD_INPUT_BUFFERSIZE;
}

static size_t offload_dev_get_offload_buffer_size(const struct audio_hw_device *dev,
                                           uint32_t bitRate, uint32_t samplingRate,
                                           uint32_t channel)
{
    // Goal is to compute an optimal bufferSize that shall be used by
    // Multimedia framework in transferring the encoded stream to LPE firmware
    // in duration of OFFLOAD_TRANSFER_INTERVAL defined
    struct offload_audio_device *loffload_dev = (struct offload_audio_device *)dev;

    //set bit rate, sample rate and channel
    mCodec.avgBitRate = bitRate;
    mCodec.sampleRate = samplingRate;
    mCodec.numChannels = channel;

    size_t bufSize = 0;
    if (bitRate >= 12000) {
        bufSize = (OFFLOAD_TRANSFER_INTERVAL*bitRate)/8; /* in bytes */
    }
    else {
        // Though we could not take the decision based on exact bit-rate,
        // select optimal bufferSize based on samplingRate & Channel of the stream
        if (samplingRate<=8000)
            bufSize = 2*1024; // Voice data in Mono/Stereo
        else if (channel == AUDIO_CHANNEL_OUT_MONO)
            bufSize = 4*1024; // Mono music
        else if (samplingRate<=32000)
            bufSize = 16*1024; // Stereo low quality music
        else if (samplingRate<=48000)
            bufSize = 32*1024; // Stereo high quality music
        else
            bufSize = 64*1024; // HiFi stereo music
    }

    if (bufSize < OFFLOAD_MIN_ALLOWED_BUFSIZE)
        bufSize = OFFLOAD_MIN_ALLOWED_BUFSIZE;
    if (bufSize > OFFLOAD_MAX_ALLOWED_BUFSIZE)
        bufSize = OFFLOAD_MAX_ALLOWED_BUFSIZE;

    // Make the bufferSize to be of 2^n bytes
    for (size_t i = 1; (bufSize & ~i) != 0; i<<=1)
         bufSize &= ~i;

    loffload_dev->buffer_size = bufSize;

    ALOGV("getOffloadBufferSize: BR=%d SR=%d CC=%x bufSize=%d",
                             bitRate, samplingRate, channel, bufSize);
    return bufSize;

}

static int offload_dev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

static int offload_dev_close(hw_device_t *device)
{
    free(device);
    return 0;
}

static uint32_t offload_dev_get_supported_devices(const struct audio_hw_device *dev)
{
    return (/* OUT */
            AUDIO_DEVICE_OUT_SPEAKER |
            AUDIO_DEVICE_OUT_WIRED_HEADSET |
            AUDIO_DEVICE_OUT_WIRED_HEADPHONE);
}

static int offload_dev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    ALOGV("offload_dev_open");
    struct offload_audio_device *offload_dev;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) {
        ALOGV("offload_dev_open: name != AUDIO_HARDWARE_INTERFACE");
        return -EINVAL;
    }

    offload_dev = (struct offload_audio_device *)calloc(1, sizeof(struct offload_audio_device));
    if (!offload_dev) {
        ALOGV("offload_dev_open: !offload_dev");
        return -ENOMEM;
    }

    offload_dev->device.common.tag = HARDWARE_DEVICE_TAG;
    offload_dev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    offload_dev->device.common.module = (struct hw_module_t *) module;
    offload_dev->device.common.close = offload_dev_close;

    offload_dev->device.get_supported_devices = offload_dev_get_supported_devices;
    offload_dev->device.init_check = offload_dev_init_check;
    offload_dev->device.set_voice_volume = offload_dev_set_voice_volume;
    offload_dev->device.set_master_volume = offload_dev_set_master_volume;
    offload_dev->device.set_mode = offload_dev_set_mode;
    offload_dev->device.set_parameters = offload_dev_set_parameters;
    offload_dev->device.get_parameters = offload_dev_get_parameters;
    //offload_dev->device.get_input_buffer_size = offload_dev_get_input_buffer_size;
    //offload_dev->device.get_offload_buffer_size = offload_dev_get_offload_buffer_size;
    offload_dev->device.open_output_stream = offload_dev_open_output_stream;
    offload_dev->device.close_output_stream = offload_dev_close_output_stream;
    offload_dev->device.dump = offload_dev_dump;

    *device = &offload_dev->device.common;
    return 0;
}



static struct hw_module_methods_t hal_module_methods = {
    open:  offload_dev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    common : {
        tag : HARDWARE_MODULE_TAG,
        module_api_version: AUDIO_DEVICE_API_VERSION_2_0,
        hal_api_version: HARDWARE_HAL_API_VERSION,
        id : AUDIO_HARDWARE_MODULE_ID,
        name : "CODEC Offload HAL",
        author : "The Android Open Source Project",
        methods : &hal_module_methods,
        dso : NULL,
        reserved : {0},
    },
};}
