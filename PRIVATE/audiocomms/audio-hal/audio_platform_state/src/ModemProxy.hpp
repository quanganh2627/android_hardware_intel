/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2014 Intel
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
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#pragma once

#include "ValueSet.hpp"
#include "ModemAudioManagerObserver.h"
#include <EventListener.h>
#include <InterfaceProviderImpl.h>
#include <NonCopyable.hpp>


#include <list>
#include <string>
#include <utils/List.h>
#include <utils/threads.h>
#include <vector>


struct IModemAudioManagerInterface;

namespace intel_audio
{

class ModemProxy
    : public ValueSet,
      private IModemAudioManagerObserver
{
public:
    ModemProxy(const std::string &libraryName,
               const std::string &instance,
               ValueObserver *observer, ValueRegister *reg);
    virtual ~ModemProxy();

    /**
     * Starts the modem proxy. Once started, AudioHAL will be notified of
     *      -state of the modem.
     *      -status of audio in the modem.
     *      -any change of band for CSV call.
     *
     * @return true if modem audio manager successfully started, false otherwise
     */
    bool start();

    /**
     * Stops the modem proxy.
     */
    void stop();

private:
    virtual void onModemAudioStatusChanged();
    virtual void onModemStateChanged();
    virtual void onModemAudioBandChanged();

    const std::string isModemAlive(void *context) const;
    /**
     * Get the status of the Audio link
     * @param[in] context
     * @return @see AudioStatus
     */
    const std::string getModemAudioStatus(void *context) const;

    const std::string getAudioBand(void *context) const;

    IModemAudioManagerInterface *mModemAudioManagerInterface; /**< Audio AT Manager interface. */

    static const std::string &mKeyState;
    static const std::string &mKeyCallStatus;
    static const std::string &mKeyBandType;
    static const std::string &mLiteralFalseValue;
    const std::string mInstance;
};

} // namespace intel_audio
