/* Interface.h
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
#pragma once

#include <string>

namespace NInterfaceProvider
{

struct IInterface
{
    virtual std::string getInterfaceType() const = 0;

protected:
    virtual ~IInterface() {}
};

}

#define INTERFACE_NAME(name) \
    static std::string getInterfaceName() { return name; } \
    virtual std::string getInterfaceType() const { return getInterfaceName(); }
