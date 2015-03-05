/* InterfaceProviderImpl.cpp
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
#define LOG_TAG "INTERFACE_PROVIDER_IMPL"

#include <utils/Log.h>
#include "InterfaceProvider.h"
#include "InterfaceImplementer.h"
#include "InterfaceProviderImpl.h"

using namespace std;

namespace NInterfaceProvider
{


CInterfaceProviderImpl::CInterfaceProviderImpl()
{
}

// Interface populate from Supplier
void CInterfaceProviderImpl::addImplementedInterfaces(IInterfaceImplementer &interfaceImplementer)
{
    interfaceImplementer.getImplementedInterfaces(*this);
}

// Interface populate
void CInterfaceProviderImpl::addInterface(IInterface* pInterface)
{
    if (_interfaceMap.find(pInterface->getInterfaceType()) == _interfaceMap.end()) {

        _interfaceMap[pInterface->getInterfaceType()] = pInterface;
    } else {

        LOG_FATAL_IF(true, "%s Interface already declared: %s", __FUNCTION__, pInterface->getInterfaceType().c_str());
    }
}

// Interface query
IInterface* CInterfaceProviderImpl::queryInterface(const string& strInterfaceName) const
{
    InterfaceMapIterator it = _interfaceMap.find(strInterfaceName);

    if (it != _interfaceMap.end()) {

        return it->second;
    }

    return NULL;
}

// Interface list
string CInterfaceProviderImpl::getInterfaceList() const
{
    string strList;

    InterfaceMapIterator it;

    for (it = _interfaceMap.begin(); it != _interfaceMap.end(); ++it) {

        strList += it->first + "\n";
    }

    return strList;
}

}
