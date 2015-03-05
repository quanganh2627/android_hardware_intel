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

#include "ProximitySensor_input.h"

ProximitySensor::ProximitySensor(const sensor_platform_config_t *config)
    : SensorBase(config),
      mEnabled(0),
      mInputReader(32),
      mHasPendingEvent(false),
      inputDataOverrun(0),
      thresh(APDS_PROX_DEF_THRES)
{
    if (mConfig->handle != SENSORS_HANDLE_PROXIMITY)
        E("ProximitySensor: Incorrect sensor config");

    data_fd = SensorBase::openInputDev(mConfig->name);
    LOGE_IF(data_fd < 0, "Proxmity can't open proximity input dev%s",mConfig->name);

    D("ProximitySensor open proximity input dev%s",mConfig->name);

    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_PROXIMITY;
    mPendingEvent.type = SENSOR_TYPE_PROXIMITY;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

ProximitySensor::~ProximitySensor()
{
    if (mEnabled)
        enable(0, 0);
}

int ProximitySensor::calibThresh(int raw_data)
{
    int ret, fd, maxthresh = APDS990X_MAX_THRESH;
    off_t offset = 0;
    struct flock lock;
    ps_calib_t calib;

    if ((fd = open(SENSOR_CALIB_FILE, O_RDWR | O_CREAT, S_IRWXU)) < 0) {
        E("ProximitySensor: open %s failed, %s",
                                        SENSOR_CALIB_FILE, strerror(errno));
        return fd;
    }

    lock.l_type = F_WRLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;
    if (fcntl(fd, F_SETLK, &lock) < 0) {
        LOGI("%d ProximitySensor: File lock failed failed, %s",__LINE__, strerror(errno));
        close(fd);
        return -1;
    }

    memset(&calib, 0, sizeof(calib));
    while ((ret = pread(fd, &calib, sizeof(calib), offset)) > 0) {
        LOGI("ProximitySensor: pread %d bytes from seonsr config file", ret);
        if (calib.type == SENSOR_TYPE_PROXIMITY) {
            LOGI("ProximitySensor: thresh=%d, raw_data=%d", calib.thresh, raw_data);
            maxthresh = calib.thresh;
            break;
        }
        offset += sizeof(calib);
    }
    if (raw_data < maxthresh && raw_data >= APDS990X_MIN_THRESH) {
        LOGI("ProximitySensor: raw %d max %d", raw_data, maxthresh);

        maxthresh = raw_data;
        calib.thresh = maxthresh;
        calib.type = SENSOR_TYPE_PROXIMITY;
        if ((ret = pwrite(fd, &calib, sizeof(calib), offset)) > 0)
            LOGI("ProximitySensor: write %d bytes to sensor config file", ret);
        else
            LOGI("ProximitySensor: write data failed, %s", strerror(errno));
    }

    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, &lock) < 0) {
        LOGI("ProximitySensor: File unlock failed, %s", strerror(errno));
        maxthresh = -1;
    }
    close(fd);

    return maxthresh;
}

int ProximitySensor::enable(int32_t, int en)
{
    int flags = en ? 1 : 0;
    int fd;
    char buf[2] = { 0 };
    int newThresh;
    int len, raw_data = 0;

    D("ProximitySensor-%s, flags = %d, mEnabled = %d", __func__, flags, mEnabled);

    if (flags == mEnabled)
        return 0;

    fd = open(mConfig->activate_path, O_RDWR);
    if (fd < 0) {
        E("ProximitySensor%s - open %s failed, errno=%d", __func__,
                                                mConfig->activate_path, errno);
        return -1;
    }

    if (flags) {
       buf[0] = '1';
    } else {
       buf[0] = '0';
    }

    int ret = write(fd, buf, sizeof(buf));
    close(fd);

    if (ret != sizeof(buf)) {
        E("ProximitySensor%s - write %s failed, errno=%d",
                __func__, mConfig->activate_path, errno);
        return -1;
    }
    D("ProximitySensor%s %s",__func__, mConfig->activate_path);

    mEnabled = flags;
    if (!flags)
        return 0;

    /*get raw data and do calibration*/
    if ((fd = open(mConfig->data_path, O_RDONLY)) < 0) {
        LOGI("ProximitySensor: open %s failed, %s!", mConfig->data_path, strerror(errno));
        return 0;
    }
    D("ProximitySensor%s %s",__func__, mConfig->data_path);

    for (int i = 0; i < APDS990X_ENABLE_TRY; i++) {
	char raw_buf[8] = { 0 };
        usleep(100000);

        //len = read(fd, &buf, sizeof(buf) - 1);
        len = read(fd, raw_buf, sizeof(raw_buf));
        if (len < 0) {
            LOGI("ProximitySensor: read %s failed, %s!", mConfig->data_path, strerror(errno));
            close(fd);
            return 0;
        }
        sscanf(raw_buf, "%d\n", &raw_data);
        LOGI("ProximitySensor - path: %s buf: %s, raw_data: %d", mConfig->data_path, raw_buf,raw_data);

        if (raw_data > 0)
            break;
    }
    close(fd);

    LOGI("ProximitySensor - raw data for calibration:%d", raw_data);
    if ((newThresh = calibThresh(raw_data)) < 0) {
        LOGI("ProximitySensor - calibration failed");
        return 0;
    }

    newThresh += APDS990X_DISTANCE_THRESH;
    if(newThresh != thresh)
    {
        char raw_buf[8] = { 0 };

        if ((fd = open(mConfig->config_path, O_WRONLY)) < 0) {
            LOGI("ProximitySensor: open %s failed, %s", mConfig->config_path, strerror(errno));
            return 0;
        }
        D("ProximitySensor: set thresh from %d to %d.", thresh, newThresh);

       memset(raw_buf, 0, sizeof(raw_buf));
       snprintf(raw_buf, sizeof(raw_buf), "%d\n", newThresh);
       D("ProximitySensor: set thresh to %s.", raw_buf);

       if (write(fd, raw_buf, strlen(raw_buf)) < 0) {
            LOGI("ProximitySensor: write %s failed, %s", mConfig->config_path, strerror(errno));
            close(fd);
            return 0;
       }
       thresh = newThresh;

       D("ProximitySensor: set thresh to %s.", raw_buf);
       close(fd);
    }

    return 0;
}

bool ProximitySensor::hasPendingEvents() const {
    return mHasPendingEvent;
}

int ProximitySensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    D("sensor ::%s [%d] count=%d", __func__,__LINE__,count);

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;

        D(":%s, type = %d, code = %d, value: %d, count = %d, thresh = %d",
		__func__, type, event->code, event->value, count,thresh);

        if (type == EV_ABS && !inputDataOverrun) {
            int val = event->value;
	    mPendingEvent.distance = (float)(val > 0? 6 : 0);
        } else if (type == EV_SYN) {
            mPendingEvent.timestamp = timevalToNano(event->time);
            if (event->code == SYN_DROPPED) {
                LOGE("ProximitySensor: input event overrun, dropped event:drop");
                inputDataOverrun = 1;
            } else if (inputDataOverrun) {
                LOGE("ProximitySensor: input event overrun, dropped event:sync");
                inputDataOverrun = 0;
            } else {
                D("ProximitySensor::%s, in type = EV_SYN, mEnabled = %d", __func__, mEnabled);
                if (mEnabled) {
                    *data++ = mPendingEvent;
                    count--;
                    numEventReceived++;
                }
            }
        } else {
            LOGE("ProximitySensor: unknown event (type=%d, code=%d)", type, event->code);
        }
        mInputReader.next();
    }

    D("ProximitySensor::%s, return numEventReceived= %d", __func__, numEventReceived);

    return numEventReceived;
}
