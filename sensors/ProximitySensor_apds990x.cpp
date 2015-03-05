/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "ProximitySensor_apds990x.h"

ProximitySensor::ProximitySensor(const sensor_platform_config_t *config)
    : SensorBase(config),
      mEnabled(0),
      mHasPendingEvent(false)
{
    if (mConfig->handle != SENSORS_HANDLE_PROXIMITY)
        E("ProximitySensor: Incorrect sensor config");

    data_fd = open(mConfig->activate_path, O_RDONLY);
    LOGE_IF(data_fd < 0, "can't open %s", mConfig->activate_path);

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

int ProximitySensor::getThresh(ps_calib_t *calib)
{
    int thresh;

    thresh = calib->average;
    if (thresh < APDS990X_MIN_CROSSTALK)
	    thresh = APDS990X_MIN_CROSSTALK;

    thresh = (thresh * 22) / 10;
    if (thresh > APDS990X_MAX_CROSSTALK)
	    thresh = APDS990X_MAX_CROSSTALK;

    I("ProximitySensor: crosstalk average: %d, set thresh to %d",
            calib->average, thresh);
    return thresh;
}

int ProximitySensor::calibCrosstalk(int raw_data)
{
    int i, ret, fd, sum = 0;
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
        I("ProximitySensor: File lock failed failed, %s", strerror(errno));
        close(fd);
        return -1;
    }

    memset(&calib, 0, sizeof(calib));
    if ((ret = pread(fd, &calib, sizeof(calib), 0)) > 0) {
        I("ProximitySensor: crosstalk sample num=%d, average=%d, raw_data=%d",
                calib.num, calib.average, raw_data);
    } else {
        I("ProximitySensor: No calibration file found, raw_data=%d", raw_data);
    }

    if (calib.num == 0 && raw_data == APDS990X_PS_INIT_DATA) {
            I("ProximitySensor: No data for calibration, abort...");
            return -1;
    }
    if (raw_data < APDS990X_MIN_CROSSTALK)
        raw_data += APDS990X_MIN_CROSSTALK;

    if (calib.num > SAMPLE_MAX_NUM) {
        E("ProximitySensor: crosstalk sample count error: %d", calib.num);
        return -1;
    }
    if (calib.num == 0) {
        if (raw_data > APDS990X_MAX_CROSSTALK)
            return -1;
        calib.average = raw_data;
        calib.crosstalk[0] = raw_data;
        calib.num = 1;
    } else if (calib.num < SAMPLE_MAX_NUM) {
        if (raw_data > APDS990X_MAX_CROSSTALK) {
            I("ProximitySensor: Ignore raw data %d, average: %d, num: %d",
                    raw_data, calib.average, calib.num);
            return getThresh(&calib);
        }
        calib.crosstalk[calib.num++] = raw_data;
        for (i = 0; i < calib.num; i++)
            sum += calib.crosstalk[i];
        calib.average = sum / calib.num;
    } else {
        if (raw_data > calib.average * 2) {
            I("ProximitySensor: Ignore raw data %d, average: %d, num: %d",
                    raw_data, calib.average, calib.num);
            return getThresh(&calib);
        }
        for (i = 1; i < calib.num; i++) {
            sum += calib.crosstalk[i];
            calib.crosstalk[i - 1] = calib.crosstalk[i];
        }
        sum += raw_data;
        calib.average = sum / calib.num;
        calib.crosstalk[i - 1] = raw_data;
    }
    if ((ret = pwrite(fd, &calib, sizeof(calib), 0)) > 0)
        D("ProximitySensor: Write %d bytes to sensor config file", ret);
    else
        E("ProximitySensor: Write data failed, %s", strerror(errno));

    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, &lock) < 0)
        E("ProximitySensor: File unlock failed, %s", strerror(errno));

    close(fd);

    return getThresh(&calib);
}

int ProximitySensor::enable(int32_t, int en)
{
    int fd, thresh, ret;
    int flag = en ? 1 : 0;
    int raw_data = APDS990X_PS_INIT_DATA;
    char buf[16] = { 0 };
    struct pollfd pfd;

    I("ProximitySensor - %s - enable=%d", __func__, en);
    if (flag == mEnabled)
        return 0;

    if (ioctl(data_fd, 0, en)) {
        E("ProximitySensor: ioctl set %s failed", mConfig->activate_path);
        return -1;
    }
    mEnabled = flag;
    if (!flag)
        return 0;

    if ((fd = open(mConfig->data_path, O_RDONLY)) < 0) {
        I("ProximitySensor: open %s failed, %s!",
                mConfig->data_path, strerror(errno));
        return 0;
    }

    pfd.fd = data_fd;
    pfd.events = POLLIN;
    /* Waiting for first sensor raw data */
    ret = poll(&pfd, 1, 50);
    if (ret < 0) {
        E("ProximitySeneor - %s: poll first data error, %s",
                __func__, strerror(errno));
    } else {
        if (ret == 0)
            I("ProximitySeneor - %s: poll first data timeout", __func__);
        ret = read(fd, &buf, sizeof(buf) - 1);
        if (ret < 0) {
            E("ProximitySensor: read %s failed, %s!",
				    mConfig->data_path, strerror(errno));

            close(fd);
            return 0;
        }
        sscanf(buf, "%d\n", &raw_data);
    }
    close(fd);

    if ((thresh = calibCrosstalk(raw_data)) < 0) {
        E("ProximitySensor - calibration failed");
        return 0;
    }
    if ((fd = open(mConfig->config_path, O_WRONLY)) < 0) {
        E("ProximitySensor: open %s failed, %s",
			mConfig->config_path, strerror(errno));
        return 0;
    }

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), "%d\n", thresh);
    if (write(fd, buf, strlen(buf)) < 0) {
        E("ProximitySensor: write %s failed, %s",
                mConfig->config_path, strerror(errno));
    }
    close(fd);
    return 0;
}

bool ProximitySensor::hasPendingEvents() const {
    return mHasPendingEvent;
}

int ProximitySensor::readEvents(sensors_event_t* data, int count)
{
    int val = -1;
    struct timespec t;

    D("ProximitySensor - %s", __func__);
    if (count < 1 || data == NULL || data_fd < 0)
        return -EINVAL;

    *data = mPendingEvent;
    data->timestamp = getTimestamp();
    read(data_fd, &val, sizeof(int));
    data->distance = (float)(val == 1 ? 0 : 6);
    I("ProximitySensor - read data %f, %s",
            data->distance, data->distance > 0 ? "far" : "near");

    return 1;
}
