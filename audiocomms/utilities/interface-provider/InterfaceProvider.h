/* @licence
 *
 * Copyright 2013-2014 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "Interface.h"

namespace NInterfaceProvider
{

/** Provide multiple interfaces, hiding real object implementation.
 * See provider patern
 */
struct IInterfaceProvider
{
    /** @return the requested interface if supported, NULL otherwise. */
    template <class RequestedInterface>
    RequestedInterface *queryInterface() const
    {
        // fixme: Should use dynamic cast as this is a string checked downcast
        //        but rtti are disabled on android
        return static_cast<RequestedInterface *>(
            queryInterface(RequestedInterface::getInterfaceName()));
    }

    /** Return a list of supported interfaces. */
    virtual std::string getInterfaceList() const = 0;

    protected:
        virtual ~IInterfaceProvider() {}

    private:
        /** @return the interface matching the given name or NULL if none matched. */
        virtual IInterface *queryInterface(const std::string &name) const = 0;
};

}
