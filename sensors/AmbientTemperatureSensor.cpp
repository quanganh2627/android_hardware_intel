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

#include <stdio.h>
#include <stdlib.h>
#include "AmbientTemperatureSensor.h"

AmbTempSensor::AmbTempSensor(const sensor_platform_config_t *config)
    : SensorBase(config),
      mEnabled(0)
{
    if (mConfig->handle != SENSORS_HANDLE_AMBIENT_TEMPERATURE)
        E("AmbTempSensor: Incorrect sensor config");

    data_fd = open(mConfig->data_path, O_RDONLY);
    LOGE_IF(data_fd < 0, "can't open %s", mConfig->data_path);

    /* start from -30 degree, need at lease MAX_ROW/2 items */
    mTempBase = -30;
    memset(mTable, 0, MAX_ROW);
    mTableRow = getTable(mConfig->config_path);
    if (mTableRow <= MAX_ROW/2) {
        mTable[0] = 2008;
        mTable[1] = 2003;
        mTable[2] = 1999;
        mTable[3] = 1994;
        mTable[4] = 1989;
        mTable[5] = 1984;
        mTable[6] = 1978;
        mTable[7] = 1973;
        mTable[8] = 1967;
        mTable[9] = 1960;
        mTable[10] = 1954;
        mTable[11] = 1948;
        mTable[12] = 1941;
        mTable[13] = 1934;
        mTable[14] = 1927;
        mTable[15] = 1919;
        mTable[16] = 1912;
        mTable[17] = 1904;
        mTable[18] = 1895;
        mTable[19] = 1886;
        mTable[20] = 1877;
        mTable[21] = 1869;
        mTable[22] = 1859;
        mTable[23] = 1849;
        mTable[24] = 1840;
        mTable[25] = 1829;
        mTable[26] = 1819;
        mTable[27] = 1808;
        mTable[28] = 1797;
        mTable[29] = 1785;
        mTable[30] = 1771;
        mTable[31] = 1761;
        mTable[32] = 1750;
        mTable[33] = 1737;
        mTable[34] = 1724;
        mTable[35] = 1709;
        mTable[36] = 1696;
        mTable[37] = 1682;
        mTable[38] = 1668;
        mTable[39] = 1655;
        mTable[40] = 1639;
        mTable[41] = 1625;
        mTable[42] = 1608;
        mTable[43] = 1594;
        mTable[44] = 1580;
        mTable[45] = 1564;
        mTable[46] = 1548;
        mTable[47] = 1532;
        mTable[48] = 1517;
        mTable[49] = 1500;
        mTable[50] = 1484;
        mTable[51] = 1468;
        mTable[52] = 1451;
        mTable[53] = 1434;
        mTable[54] = 1417;
        mTable[55] = 1400;
        mTable[56] = 1383;
        mTable[57] = 1365;
        mTable[58] = 1349;
        mTable[59] = 1331;
        mTable[60] = 1313;
        mTable[61] = 1296;
        mTable[62] = 1279;
        mTable[63] = 1261;
        mTable[64] = 1243;
        mTable[65] = 1226;
        mTable[66] = 1208;
        mTable[67] = 1191;
        mTable[68] = 1173;
        mTable[69] = 1155;
        mTable[70] = 1138;
        mTable[71] = 1120;
        mTable[72] = 1103;
        mTable[73] = 1085;
        mTable[74] = 1067;
        mTable[75] = 1050;
        mTable[76] = 1032;
        mTable[77] = 1015;
        mTable[78] = 998;
        mTable[79] = 981;
        mTable[80] = 964;
        mTable[81] = 948;
        mTable[82] = 931;
        mTable[83] = 914;
        mTable[84] = 898;
        mTable[85] = 881;
        mTable[86] = 865;
        mTable[87] = 849;
        mTable[88] = 833;
        mTable[89] = 818;
        mTable[90] = 802;
        mTable[91] = 786;
        mTable[92] = 771;
        mTable[93] = 758;
        mTable[94] = 743;
        mTable[95] = 728;
        mTable[96] = 713;
        mTable[97] = 699;
        mTable[98] = 685;
        mTable[99] = 671;
        mTable[100] = 656;
        mTable[101] = 643;
        mTable[102] = 629;
        mTable[103] = 616;
        mTable[104] = 603;
        mTable[105] = 590;
        mTable[106] = 578;
        mTable[107] = 565;
        mTable[108] = 553;
        mTable[109] = 541;
        mTable[110] = 530;
        mTable[111] = 518;
        mTable[112] = 507;
        mTable[113] = 493;
        mTable[114] = 483;
        mTable[115] = 472;
        mTable[116] = 462;
        mTable[117] = 452;
        mTable[118] = 443;
        mTable[119] = 434;
        mTable[120] = 423;
        mTable[121] = 414;
        mTable[122] = 404;
        mTable[123] = 395;
        mTable[124] = 387;
        mTable[125] = 378;
        mTable[126] = 370;
        mTable[127] = 361;
        mTable[128] = 353;
        mTable[129] = 344;
        mTable[130] = 337;
        mTable[131] = 332;
        mTable[132] = 326;
        mTable[133] = 318;
        mTable[134] = 311;
        mTable[135] = 304;
        mTable[136] = 298;
        mTable[137] = 291;
        mTable[138] = 285;
        mTable[139] = 278;
        mTable[140] = 272;
        mTable[141] = 266;
        mTable[142] = 260;
        mTable[143] = 255;
        mTable[144] = 249;
        mTable[145] = 244;
    }

    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSORS_HANDLE_AMBIENT_TEMPERATURE;
    mPendingEvent.type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

AmbTempSensor::~AmbTempSensor()
{
    if (mEnabled)
        enable(0, 0);
}

/*
 * A table config file must:
 * comments start with '#'
 * adc value (mV) item end with ','
 * For example:
 * # below is adc item from -30 degree
 * 2008,
 * 2003,
 * 1999,
 * ....
 */
int AmbTempSensor::getTable(const char *config_path)
{
	char buffer[MAX_SIZE];
	char val[MAX_SIZE];
	int i, j;
	FILE *file = fopen(config_path, "r");

	if (!file)
	    return 0;

	j = 0;
	while(fgets(buffer, MAX_SIZE, file) != NULL) {
		if (buffer[0] == '#')
			continue;
		i = 0;
		while ((buffer[i] != ',')
			&& (i < MAX_SIZE-1)) {
			val[i] = buffer[i];
			i++;
		}
		val[i] = '\0';
		mTable[j] = atoi(val);

		if ((j == MAX_ROW - 1) ||
			(j > 0 && (mTable[j] > mTable[j-1])))
			break;

		j++;
	}

	fclose(file);

	return j;
}

int AmbTempSensor::enable(int32_t handle, int en)
{
    int flags = en ? 1 : 0;

    D("AmbTempSensor-%s, enable= %d", __func__, en);

    if (data_fd) {
        if (ioctl(data_fd, 0, flags)) {
	    E("AmbTempSensor: ioctl set enable disable failed!");
	    return -1;
        }
        mEnabled = flags;
    }

    return 0;
}

int AmbTempSensor::setDelay(int32_t handle, int64_t delay_ns)
{
    int fd, delay_ms;
    char buf[10] = { 0 };

    if ((fd = open(mConfig->poll_path, O_RDWR)) < 0) {
        E("AmbTempSensor: Open %s failed!", mConfig->poll_path);
        return -1;
    }

    if (delay_ns < mConfig->min_delay)
        delay_ns = mConfig->min_delay;

    D("AmbTempSensor-%s, delay_ns= %d", __func__, delay_ns);
    delay_ms = delay_ns / 1000000;
    snprintf(buf, sizeof(buf), "%d", delay_ms);
    write(fd, buf, sizeof(buf));
    close(fd);

    return 0;
}

float AmbTempSensor::processRawData(int val)
{
    uint32_t i;

    for (i = 0; i < mTableRow - 1; i++) {
        if ((val <= mTable[i]) && (val > mTable[i+1]))
            break;
    }

    return (float)(i + mTempBase);
}

int AmbTempSensor::readEvents(sensors_event_t* data, int count)
{
    int val = -1;
    struct timespec t;

    D("AmbTempSensor - %s", __func__);
    if (count < 1 || data == NULL || data_fd < 0)
        return -EINVAL;

    *data = mPendingEvent;
    data->timestamp = getTimestamp();
    if (read(data_fd, &val, sizeof(int)) < 0)
        data->temperature = 0;
    else
        data->temperature = processRawData(val);

    D("AmbTempSensor - read data %f", data->temperature);

    return 1;
}
