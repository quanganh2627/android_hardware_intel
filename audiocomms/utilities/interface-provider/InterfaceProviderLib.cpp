/* InterfaceProviderLib.cpp
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
#define LOG_TAG "INTERFACE_PROVIDER_LIB"

#include "InterfaceProviderLib.h"
#include <utils/Log.h>
#include <dlfcn.h>

// Type of the entry point function to get the interface provider
typedef NInterfaceProvider::IInterfaceProvider* (* FGetInterfaceProvider)();


// Helper to retreive the library getInterfaceProvider function
NInterfaceProvider::IInterfaceProvider* getInterfaceProvider(const char* pcLibraryName)
{
    void* pHandle;
    const char* pError;
    FGetInterfaceProvider fGetInterfaceProvider;
    NInterfaceProvider::IInterfaceProvider* pInterfaceProvider;

    pHandle = dlopen(pcLibraryName, RTLD_NOW);

    ALOGD("%s: %s", __FUNCTION__, pcLibraryName);
    if (pHandle == NULL) {

        ALOGE("%s Failed to load %s: %s.", __FUNCTION__, pcLibraryName, dlerror());

        return NULL;
    }

    // Clear any existing error
    dlerror();
    // Find getInterfaceProvider() library entry point
    // Refer to dlsym man pages about the void ** cast workaround
    *(void **) (&fGetInterfaceProvider) = dlsym(pHandle, "getInterfaceProvider");

    // Did we find the symbol ?
    if ((pError = dlerror()) != NULL) {

        ALOGE("%s Failed to find symbol getInterfaceProvider in %s: %s.", __FUNCTION__, pcLibraryName, pError);

        dlclose(pHandle);
        return NULL;
    }
    // Try to retreive the interface provider
    pInterfaceProvider = fGetInterfaceProvider != NULL ? fGetInterfaceProvider() : NULL;

    if (pInterfaceProvider == NULL) {

        ALOGE("%s Failed to retrieve the interface provider", __FUNCTION__);

        dlclose(pHandle);
        return NULL;
    }

    ALOGD("%s Interface provider retrieved. Provides:\n%s", __FUNCTION__, pInterfaceProvider->getInterfaceList().c_str());
    return pInterfaceProvider;
}
