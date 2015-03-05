#include "PSHSensor.hpp"

psh_sensor_t SensorHubHelper::getType(int sensorType, sensors_subname subname)
{
        switch (sensorType) {
        case SENSOR_TYPE_ACCELEROMETER:
                if (subname == SECONDARY)
                        return SENSOR_ACCELEROMETER_SEC;
                return SENSOR_ACCELEROMETER;
        case SENSOR_TYPE_MAGNETIC_FIELD:
                if (subname == SECONDARY)
                        return SENSOR_COMP_SEC;
                return SENSOR_COMP;
        case SENSOR_TYPE_ORIENTATION:
                return SENSOR_ORIENTATION;
        case SENSOR_TYPE_GYROSCOPE:
                if (subname == SECONDARY)
                        return SENSOR_GYRO_SEC;
                return SENSOR_GYRO;
        case SENSOR_TYPE_LIGHT:
                if (subname == SECONDARY)
                        return SENSOR_ALS_SEC;
                return SENSOR_ALS;
        case SENSOR_TYPE_PRESSURE:
                if (subname == SECONDARY)
                        return SENSOR_BARO_SEC;
                return SENSOR_BARO;
        case SENSOR_TYPE_PROXIMITY:
                if (subname == SECONDARY)
                        return SENSOR_PROXIMITY_SEC;
                return SENSOR_PROXIMITY;
        case SENSOR_TYPE_GRAVITY:
                return SENSOR_GRAVITY;
        case SENSOR_TYPE_LINEAR_ACCELERATION:
                return SENSOR_LINEAR_ACCEL;
        case SENSOR_TYPE_ROTATION_VECTOR:
                return SENSOR_ROTATION_VECTOR;
        case SENSOR_TYPE_GESTURE_FLICK:
                return SENSOR_GESTURE_FLICK;
        case SENSOR_TYPE_TERMINAL:
                return SENSOR_TC;
        case SENSOR_TYPE_SHAKE:
                return SENSOR_SHAKING;
        case SENSOR_TYPE_SIMPLE_TAPPING:
                return SENSOR_STAP;
        case SENSOR_TYPE_MOVE_DETECT:
                return SENSOR_MOVE_DETECT;
        case SENSOR_TYPE_STEP_DETECTOR:
                return SENSOR_STEPDETECTOR;
        case SENSOR_TYPE_STEP_COUNTER:
                return SENSOR_STEPCOUNTER;
        case SENSOR_TYPE_SIGNIFICANT_MOTION:
                return SENSOR_SIGNIFICANT_MOTION;
        case SENSOR_TYPE_GAME_ROTATION_VECTOR:
                return SENSOR_GAME_ROTATION_VECTOR;
        case SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR:
                return SENSOR_GEOMAGNETIC_ROTATION_VECTOR;
        case SENSOR_TYPE_GESTURE:
        case SENSOR_TYPE_PHYSICAL_ACTIVITY:
        case SENSOR_TYPE_PEDOMETER:
        case SENSOR_TYPE_TEMPERATURE:
        case SENSOR_TYPE_RELATIVE_HUMIDITY:
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
        case SENSOR_TYPE_AUDIO_CLASSIFICATION:
        default:
                LOGE("%s: Unsupported Sensor Type: %d", __FUNCTION__, sensorType);
                break;
        }
        return SENSOR_INVALID;
}

void SensorHubHelper::getStartStreamingParameters(int sensorType, int &dataRate, int &bufferDelay, streaming_flag &flag)
{
        switch (sensorType) {
        case SENSOR_TYPE_PROXIMITY:
                flag = NO_STOP_WHEN_SCREEN_OFF;
        case SENSOR_TYPE_GESTURE_FLICK:
                dataRate = GF_SAMPLE_RATE;
                bufferDelay = GF_BUF_DELAY;
                break;
        case SENSOR_TYPE_TERMINAL:
                dataRate = TERM_RATE;
                bufferDelay = TERM_DELAY;
                break;
        case SENSOR_TYPE_SHAKE:
                dataRate = SHAKING_SAMPLE_RATE;
                bufferDelay = SHAKING_BUF_DELAY;
                break;
        case SENSOR_TYPE_SIMPLE_TAPPING:
                dataRate = STAP_SAMPLE_RATE;
                bufferDelay = STAP_BUF_DELAY;
                break;
        default:
                break;
        }
}

bool SensorHubHelper::setPSHPropertyIfNeeded(int sensorType, struct sensor_hub_methods methods, handle_t handler) {
        switch (sensorType) {
        case SENSOR_TYPE_SHAKE: {
                int sensitivity = SHAKE_SEN_MEDIUM;
                return methods.psh_set_property(handler, PROP_SHAKING_SENSITIVITY, &sensitivity);
        }
        default:
                break;
        }
        return true;
}

int SensorHubHelper::getGestureFlickEvent(struct gesture_flick_data data)
{
        switch ((FlickGestures)data.flick) {
        case LEFT_FLICK:
                return SENSOR_EVENT_TYPE_GESTURE_LEFT_FLICK;
        case RIGHT_FLICK:
                return SENSOR_EVENT_TYPE_GESTURE_RIGHT_FLICK;
        case LEFT_TWICE:
                return SENSOR_EVENT_TYPE_GESTURE_LEFT_FLICK_TWICE;
        case RIGHT_TWICE:
                return SENSOR_EVENT_TYPE_GESTURE_RIGHT_FLICK_TWICE;
        case UP_FLICK:
                return SENSOR_EVENT_TYPE_GESTURE_UP_FLICK;
        case DOWN_FLICK:
                return SENSOR_EVENT_TYPE_GESTURE_DOWN_FLICK;
        case NO_FLICK:
        default:
                return SENSOR_EVENT_TYPE_GESTURE_NO_FLICK;
        }
        return SENSOR_EVENT_TYPE_GESTURE_NO_FLICK;
}

int SensorHubHelper::getTerminalEvent(struct tc_data data)
{
        if (data.orien_z != FACE_UNKNOWN
            && data.orien_xy != ORIENTATION_UNKNOWN) {
                // This shouldn't happen
                return SENSOR_EVENT_TYPE_TERMINAL_UNKNOWN;
        }

        if (data.orien_xy == ORIENTATION_UNKNOWN) {
                // Face up/down
                switch (data.orien_z) {
                case FACE_UP:
                        return SENSOR_EVENT_TYPE_TERMINAL_FACE_UP;
                case FACE_DOWN:
                        return SENSOR_EVENT_TYPE_TERMINAL_FACE_DOWN;
                default:
                        return SENSOR_EVENT_TYPE_TERMINAL_UNKNOWN;
                }
        }

        if (data.orien_z == FACE_UNKNOWN) {
                // Orientation up/down
                switch (data.orien_xy) {
                case PORTRAIT_UP:
                        return SENSOR_EVENT_TYPE_TERMINAL_PORTRAIT_UP;
                case PORTRAIT_DOWN:
                        return SENSOR_EVENT_TYPE_TERMINAL_PORTRAIT_DOWN;
                case HORIZONTAL_UP:
                        return SENSOR_EVENT_TYPE_TERMINAL_HORIZONTAL_UP;
                case HORIZONTAL_DOWN:
                        return SENSOR_EVENT_TYPE_TERMINAL_HORIZONTAL_DOWN;
                default:
                        return SENSOR_EVENT_TYPE_TERMINAL_UNKNOWN;
                }
        }

        // Shouldn't reach here
        return SENSOR_EVENT_TYPE_TERMINAL_UNKNOWN;
}

int SensorHubHelper::getShakeEvent(struct shaking_data data)
{
        if (data.shaking == SHAKING) {
                return SENSOR_EVENT_TYPE_SHAKE;
        }
        return INVALID_EVENT;
}

int SensorHubHelper::getSimpleTappingEvent(struct stap_data data)
{
        if (data.stap == DOUBLE_TAPPING) {
                return SENSOR_EVENT_TYPE_SIMPLE_TAPPING_DOUBLE_TAPPING;
        }
        return INVALID_EVENT;
}

int SensorHubHelper::getMoveDetectEvent(struct md_data data)
{
        switch (data.state) {
        case MD_MOVE:
                return SENSOR_EVENT_TYPE_MOVE_DETECT_MOVE;
        case MD_SLIGHT:
                return SENSOR_EVENT_TYPE_MOVE_DETECT_SLIGHT;
        case MD_STILL:
                return SENSOR_EVENT_TYPE_MOVE_DETECT_STILL;
        default:
                return INVALID_EVENT;
        }
}

size_t SensorHubHelper::getUnitSize(int sensorType)
{
        switch (sensorType) {
        case SENSOR_TYPE_ACCELEROMETER:
                return sizeof(struct accel_data);
        case SENSOR_TYPE_MAGNETIC_FIELD:
                return sizeof(struct compass_raw_data);
        case SENSOR_TYPE_ORIENTATION:
                return sizeof(struct orientation_data);
        case SENSOR_TYPE_GYROSCOPE:
                return sizeof(struct gyro_raw_data);
        case SENSOR_TYPE_LIGHT:
                return sizeof(struct als_raw_data);
        case SENSOR_TYPE_PRESSURE:
                return sizeof(struct baro_raw_data);
        case SENSOR_TYPE_PROXIMITY:
                return sizeof(struct ps_phy_data);
        case SENSOR_TYPE_GRAVITY:
                return sizeof(struct gravity_data);
        case SENSOR_TYPE_LINEAR_ACCELERATION:
                return sizeof(struct linear_accel_data);
        case SENSOR_TYPE_ROTATION_VECTOR:
                return sizeof(struct rotation_vector_data);
        case SENSOR_TYPE_GESTURE_FLICK:
                return sizeof(struct gesture_flick_data);
        case SENSOR_TYPE_GESTURE:
                return sizeof(struct gs_data);
        case SENSOR_TYPE_PHYSICAL_ACTIVITY:
                return sizeof(struct phy_activity_data);
        case SENSOR_TYPE_TERMINAL:
                return sizeof(struct tc_data);
        case SENSOR_TYPE_PEDOMETER:
                return sizeof(struct pedometer_data);
        case SENSOR_TYPE_SHAKE:
                return sizeof(struct shaking_data);
        case SENSOR_TYPE_SIMPLE_TAPPING:
                return sizeof(struct stap_data);
        case SENSOR_TYPE_MOVE_DETECT:
                return sizeof(struct md_data);
        case SENSOR_TYPE_STEP_DETECTOR:
                return sizeof(struct stepdetector_data);
        case SENSOR_TYPE_STEP_COUNTER:
                return sizeof(struct stepcounter_data);
        case SENSOR_TYPE_SIGNIFICANT_MOTION:
                return sizeof(struct sm_data);
        case SENSOR_TYPE_GAME_ROTATION_VECTOR:
                return sizeof(struct game_rotation_vector_data);
        case SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR:
                return sizeof(struct geomagnetic_rotation_vector_data);
        case SENSOR_TYPE_TEMPERATURE:
        case SENSOR_TYPE_RELATIVE_HUMIDITY:
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
        case SENSOR_TYPE_AUDIO_CLASSIFICATION:
        default:
                LOGE("%s: Unsupported Sensor Type: %d", __FUNCTION__, sensorType);
                break;
        }
        return -1;
}

void SensorHubHelper::accelFilter(struct SensorDevice &device, struct sensorhub_event_t &event)
{
    static int x, y, z, filter = -1;
    int delta_x, delta_y, delta_z;

    if (filter < 0)
        filter = (int)(ACCEL_FILTER / device.getScale(AXIS_X));

    delta_x = event.data[0] - x;
    if (delta_x > -filter && delta_x < filter)
        event.data[0] = x;
    else
        x = event.data[0];

    delta_y = event.data[1] - y;
    if (delta_y > -filter && delta_y < filter)
        event.data[1] = y;
    else
        y = event.data[1];

    delta_z = event.data[2] - z;
    if (delta_z > -filter && delta_z < filter)
        event.data[2] = z;
    else
        z = event.data[2];
}

ssize_t SensorHubHelper::readSensorhubEvents(struct SensorDevice &device, int fd,
		struct sensorhub_event_t* events, size_t count, int64_t &last_timestamp)
{
        int sensorType;
        int64_t current_timestamp, timestamp_step;

        if (fd < 0)
                return fd;

        if (count <= 0)
                return 0;

        sensorType = device.getType();
        size_t unitSize = getUnitSize(sensorType);
        size_t streamSize = unitSize * count;
        byte* stream = new byte[streamSize];

        streamSize = read(fd, reinterpret_cast<void *>(stream), streamSize);

        if (streamSize % unitSize != 0) {
                LOGE("%s line: %d: invalid stream size: type: %d size: %d",
                     __FUNCTION__, __LINE__, sensorType, streamSize);
                delete[] stream;
                return -1;
        }

        count = streamSize / unitSize;
        current_timestamp = getTimestamp();
        timestamp_step = (current_timestamp - last_timestamp) / count;
        switch (sensorType) {
        case SENSOR_TYPE_ACCELEROMETER:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = (reinterpret_cast<struct accel_data*>(stream))[i].x;
                        events[i].data[1] = (reinterpret_cast<struct accel_data*>(stream))[i].y;
                        events[i].data[2] = (reinterpret_cast<struct accel_data*>(stream))[i].z;
                        events[i].accuracy = SENSOR_STATUS_ACCURACY_MEDIUM;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                        accelFilter(device, events[i]);
                }
                break;
        case SENSOR_TYPE_MAGNETIC_FIELD:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = (reinterpret_cast<struct compass_raw_data *>(stream))[i].x;
                        events[i].data[1] = (reinterpret_cast<struct compass_raw_data *>(stream))[i].y;
                        events[i].data[2] = (reinterpret_cast<struct compass_raw_data *>(stream))[i].z;
                        if((reinterpret_cast<struct compass_raw_data *>(stream))[i].accuracy)
                                events[i].accuracy = SENSOR_STATUS_ACCURACY_HIGH;
                        else
                                events[i].accuracy = SENSOR_STATUS_ACCURACY_LOW;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_ORIENTATION:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = (reinterpret_cast<struct orientation_data*>(stream))[i].azimuth;
                        events[i].data[1] = (reinterpret_cast<struct orientation_data*>(stream))[i].pitch;
                        events[i].data[2] = (reinterpret_cast<struct orientation_data*>(stream))[i].roll;
                        events[i].accuracy = SENSOR_STATUS_ACCURACY_MEDIUM;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_GYROSCOPE:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = (reinterpret_cast<struct gyro_raw_data*>(stream))[i].x;
                        events[i].data[1] = (reinterpret_cast<struct gyro_raw_data*>(stream))[i].y;
                        events[i].data[2] = (reinterpret_cast<struct gyro_raw_data*>(stream))[i].z;
                        if ((reinterpret_cast<struct gyro_raw_data*>(stream))[i].accuracy)
                                events[i].accuracy = SENSOR_STATUS_ACCURACY_HIGH;
                        else
                                events[i].accuracy = SENSOR_STATUS_ACCURACY_LOW;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_LIGHT:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = (reinterpret_cast<struct als_raw_data *>(stream))[i].lux;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_PRESSURE:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = (reinterpret_cast<struct baro_raw_data*>(stream))[i].p;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_PROXIMITY:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = (reinterpret_cast<struct ps_phy_data*>(stream))[i].near == 0 ? 1 : 0;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_GRAVITY:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = (reinterpret_cast<struct gravity_data*>(stream))[i].x;
                        events[i].data[1] = (reinterpret_cast<struct gravity_data*>(stream))[i].y;
                        events[i].data[2] = (reinterpret_cast<struct gravity_data*>(stream))[i].z;
                        events[i].accuracy = SENSOR_STATUS_ACCURACY_MEDIUM;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_LINEAR_ACCELERATION:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = (reinterpret_cast<struct linear_accel_data*>(stream))[i].x;
                        events[i].data[1] = (reinterpret_cast<struct linear_accel_data*>(stream))[i].y;
                        events[i].data[2] = (reinterpret_cast<struct linear_accel_data*>(stream))[i].z;
                        events[i].accuracy = SENSOR_STATUS_ACCURACY_MEDIUM;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_ROTATION_VECTOR:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = (reinterpret_cast<struct rotation_vector_data*>(stream))[i].x;
                        events[i].data[1] = (reinterpret_cast<struct rotation_vector_data*>(stream))[i].y;
                        events[i].data[2] = (reinterpret_cast<struct rotation_vector_data*>(stream))[i].z;
                        events[i].data[3] = (reinterpret_cast<struct rotation_vector_data*>(stream))[i].w;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_GESTURE_FLICK:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = getGestureFlickEvent((reinterpret_cast<struct gesture_flick_data*>(stream))[i]);
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_TERMINAL:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = getTerminalEvent((reinterpret_cast<struct tc_data*>(stream))[i]);
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_SHAKE:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = getShakeEvent((reinterpret_cast<struct shaking_data*>(stream))[i]);
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_SIMPLE_TAPPING:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = getSimpleTappingEvent((reinterpret_cast<struct stap_data*>(stream))[i]);
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_MOVE_DETECT:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = getMoveDetectEvent((reinterpret_cast<struct md_data*>(stream))[i]);
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_STEP_DETECTOR:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = ((reinterpret_cast<struct stepdetector_data*>(stream))[i]).state;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_STEP_COUNTER:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].step_counter = ((reinterpret_cast<struct stepcounter_data*>(stream))[i]).num;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_SIGNIFICANT_MOTION:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = ((reinterpret_cast<struct sm_data*>(stream))[i]).state;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_GAME_ROTATION_VECTOR:
                for (unsigned int i = 0; i < count; i++) {
                        events[i].data[0] = (reinterpret_cast<struct game_rotation_vector_data*>(stream))[i].x;
                        events[i].data[1] = (reinterpret_cast<struct game_rotation_vector_data*>(stream))[i].y;
                        events[i].data[2] = (reinterpret_cast<struct game_rotation_vector_data*>(stream))[i].z;
                        events[i].data[3] = (reinterpret_cast<struct game_rotation_vector_data*>(stream))[i].w;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR:
                for (unsigned int i = 0; i < count; i++) {
                        LOGI("geomagnetic_rotation_vector event");
                        events[i].data[0] = (reinterpret_cast<struct geomagnetic_rotation_vector_data*>(stream))[i].x;
                        events[i].data[1] = (reinterpret_cast<struct geomagnetic_rotation_vector_data*>(stream))[i].y;
                        events[i].data[2] = (reinterpret_cast<struct geomagnetic_rotation_vector_data*>(stream))[i].z;
                        events[i].data[3] = (reinterpret_cast<struct geomagnetic_rotation_vector_data*>(stream))[i].w;
                        events[i].timestamp = last_timestamp + timestamp_step * (i + 1);
                }
                break;
        case SENSOR_TYPE_TEMPERATURE:
        case SENSOR_TYPE_RELATIVE_HUMIDITY:
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
        case SENSOR_TYPE_AUDIO_CLASSIFICATION:
        case SENSOR_TYPE_GESTURE:
        case SENSOR_TYPE_PHYSICAL_ACTIVITY:
        case SENSOR_TYPE_PEDOMETER:
        default:
                LOGE("%s: Unsupported Sensor Type: %d", __FUNCTION__, sensorType);
                break;
        }
        last_timestamp = current_timestamp;

        delete[] stream;

        return count;
}
