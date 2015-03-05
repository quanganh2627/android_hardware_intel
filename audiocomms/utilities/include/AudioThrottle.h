/* AudioThrottle.h
 **
 ** Copyright 2013 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */


/**
 * Throttle Level
 *
 * This API is intended to trig Audio strategy to reduce power based on this throttle level.
 * It has to be sent to audio HAL through usual AudioSystem::setParameters() function.
 * The format is AUDIO_PARAMETER_THROTTLE_KEY + "=" + AUDIO_PARAMETER_THROTTLE_LEVEL_X
 */
#define AUDIO_PARAMETER_THROTTLE_KEY "throttle_key"
#define AUDIO_PARAMETER_THROTTLE_LEVEL_0 "throttle_level_0"
#define AUDIO_PARAMETER_THROTTLE_LEVEL_1 "throttle_level_1"
#define AUDIO_PARAMETER_THROTTLE_LEVEL_2 "throttle_level_2"
#define AUDIO_PARAMETER_THROTTLE_LEVEL_3 "throttle_level_3"
