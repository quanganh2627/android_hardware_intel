# Copyright (C) 2011 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import common
import edify_generator
import os
"""Device-specific extension module to build/tools/releasetools for
constructing OTA packages. This implements the device-specific function
stubs defined in the DeviceSpecificParams class in common.py
and called from the ota_from_target_package script."""

# Edify generator Extensions
# WA for BZ 47136: flash modem twice
def FlashModem(dso, filename):
    dso.script.Print("Updating 3G Modem firmware image...\n");
    dso.script.script.append('flash_modem("%s");' % (filename,));

# Edify generator Extensions
def FlashNvm(dso, filename):
    dso.script.Print("Updating 3G Modem nvm...\n");
    dso.script.script.append('flash_nvm("%s");' % (filename,))

# Device-Specific function implementations
def FullOTA_Assertions(dso, **kwargs):
    pass

def FullOTA_InstallEnd(dso, **kwargs):
    FlashModem(dso, "/system/etc/firmware/modem/modem.zip");
    FlashNvm(dso, "/system/etc/firmware/modem/modem_nvm.zip");

def IncrementalOTA_Assertions(dso, **kwargs):
    pass

def IncrementalOTA_VerifyEnd(dso, **kwargs):
    pass

def IncrementalOTA_InstallEnd(dso, **kwargs):
    FlashModem(dso, "/system/etc/firmware/modem/modem.zip");
    FlashNvm(dso, "/system/etc/firmware/modem/modem_nvm.zip");

