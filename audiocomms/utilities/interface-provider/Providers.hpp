/* @licence
 * INTEL CONFIDENTIAL
 * Copyright  2013-2014 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intels prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

#pragma once

/** Includes for private parts. */
#include "InterfaceProvider.h"
#include "InterfaceProviderLib.h"
#include "NonCopyable.hpp"
#include <utility>
#include <map>

namespace NInterfaceProvider
{

/** Manage multiple interface provider.
 *
 * When multiple interface provider should be created from multiple libraries
 * this class can be used to manage them.
 *
 * getInterfaceProvider call on a library should return a interface provider
 * providing the Creator interface.
 * A call to it's create interface should return an interface provider.
 * This provider will be asociated with the name used to create it.
 *
 * @todo Should not rely on dynamic lib to get the Creator object.
 *       Maybe add a CreatorTrait ?
 *
 * @tparam the type of the identificator used to create the appropriate
 *         interface provider.
 * @see Creator
 * */
template <class InstanceId>
class Providers : public audio_comms::utilities::NonCopyable
{
private:
    /** Use a map as creations are rare and lookup by key frequent. */
    typedef std::map<InstanceId, IInterfaceProvider *> LoadedProviders;

public:

    /** Interface that the interface provider returned by getInterfaceProvider
     * must propose.
     *
     * @see getProvider
     */
    struct Creator : public IInterface
    {
        INTERFACE_NAME("ProviderCreator");
        /** Method that should return a provider based on the provided name. */
        virtual IInterfaceProvider *create(InstanceId name) = 0;
    };

    /**  Return the interface provider created with <name>, creating it does not exist.
     *
     * param[in] libraryPath path to the library containing the Creator to be used
     *           to create an interfaceProvider if none was already with the same name.
     * param[in] name of the interface provider to retrieve.
     * @return the interface provider created with name or NULL on error.
     *
     * @todo Should return a Result to return contextual error.
     */
    IInterfaceProvider *getProvider(const std::string &libraryPath, const InstanceId &name)
    {
        typename LoadedProviders::iterator it = createdProviders.find(name);
        if (it != createdProviders.end()) {
            return it->second;
        }

        IInterfaceProvider *provider = createProvider(libraryPath, name);
        if (provider == NULL) {
            return NULL;
        }
        createdProviders.insert(std::make_pair(name, provider));

        return provider;
    }

private:
    /** Open get the interface provider of a library and return it's Creator interface.
     *
     * @param[in] libraryPath the path to the library where to find wanted Creator provider.
     * @return the Creator interface or NULL if it could not be retreive.
     */
    Creator *getInterfaceCreator(const std::string &libraryPath)
    {
        IInterfaceProvider *provider = ::getInterfaceProvider(libraryPath.c_str());
        if (provider == NULL) {
            return NULL;
        }

        return provider->queryInterface<Creator>();
    }

    /** Create a provider given a library containing a Creator and an instance name. */
    IInterfaceProvider *createProvider(const std::string &libraryPath, const InstanceId &name)
    {
        Creator *creator = getInterfaceCreator(libraryPath);
        if (creator == NULL) {
            return NULL;
        }

        return creator->create(name);
    }

    /** Collection of created providers and associated names. */
    LoadedProviders createdProviders;
};

}
