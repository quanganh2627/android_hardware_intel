/* AudioBand.h
 **
 ** Copyright 2012 Intel Corporation
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
#pragma once

class CAudioBand
{
public:
    enum Type {
        // First enum element
        // Unknown band
        EUnknown,

        // 4KHz bandwidth
        ENarrow,
        // 8KHz bandwidth
        EWide,
        // 16KHz bandwidth
        ESuperWide,

        // Last enum element
        ENBAudioBands
    };

    static const char* toLiteral(Type eAudioBand) {

        static const char* apcAudioBandStr[ENBAudioBands] = {
            "Unknown",
            "Narrow",
            "Wide",
            "Super Wide"
        };

        if (eAudioBand < ENBAudioBands && eAudioBand >= EUnknown) {

            return apcAudioBandStr[eAudioBand];
        } else {

            return "Invalid value";
        }
    }
};
