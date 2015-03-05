/* InterfaceProviderImpl.h
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

#include <map>
#include <string>
#include "Interface.h"


namespace NInterfaceProvider
{

class IInterfaceImplementer;

class CInterfaceProviderImpl
{
    typedef std::map<std::string, IInterface*>::const_iterator InterfaceMapIterator;

public:
    CInterfaceProviderImpl();

    // Interface populate from Supplier
    void addImplementedInterfaces(IInterfaceImplementer& interfaceImplementer);

    // Interface populate
    void addInterface(IInterface* pInterface);

    // Interface query
    IInterface* queryInterface(const std::string& strInterfaceName) const;

    // Interface list
    std::string getInterfaceList() const;
private:
    // Registered interfaces
    std::map<std::string, IInterface*> _interfaceMap;
};

}
