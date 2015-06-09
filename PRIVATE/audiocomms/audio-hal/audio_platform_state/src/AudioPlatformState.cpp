/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
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
#define LOG_TAG "AudioIntelHal/AudioPlatformState"

#include "AudioPlatformState.hpp"
#include "AudioHalConf.hpp"
#include "CriterionParameter.hpp"
#include "RogueParameter.hpp"
#include "ModemProxy.hpp"
#include "ParameterAdapter.hpp"
#include "ParameterMgrPlatformConnector.h"
#include "VolumeKeys.hpp"
#include <IoStream.hpp>
#include <Criterion.hpp>
#include <CriterionType.hpp>
#include <ParameterMgrHelper.hpp>
#include <Property.h>
#include "NaiveTokenizer.h"
#include <algorithm>
#include <convert.hpp>
#include <cutils/bitops.h>
#include <cutils/config_utils.h>
#include <cutils/misc.h>
#include <utilities/Log.hpp>
#include <fstream>

#define DIRECT_STREAM_FLAGS (AUDIO_OUTPUT_FLAG_DIRECT | AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD)

using std::string;
using audio_comms::utilities::convertTo;
using android::status_t;
using audio_comms::utilities::Log;

namespace intel_audio
{

using android::RWLock;

typedef RWLock::AutoRLock AutoR;
typedef RWLock::AutoWLock AutoW;

const char *const AudioPlatformState::mRoutePfwConfFileNamePropName =
    "AudioComms.RoutePFW.ConfPath";

const char *const AudioPlatformState::mRoutePfwDefaultConfFileName =
    "/etc/parameter-framework/ParameterFrameworkConfigurationRoute.xml";

const std::string AudioPlatformState::mHwDebugFilesPathList =
    "/Route/debug_fs/debug_files/path_list/";

// For debug purposes. This size is enough for dumping relevant informations
const uint32_t AudioPlatformState::mMaxDebugStreamSize = 998;

/// PFW related definitions
// Logger
class ParameterMgrPlatformConnectorLogger : public CParameterMgrPlatformConnector::ILogger
{
public:
    ParameterMgrPlatformConnectorLogger() {}

    virtual void log(bool isWarning, const string &log)
    {
        const static string format("route-parameter-manager: ");

        if (isWarning) {
            Log::Warning() << format << log;
        } else {
            Log::Debug() << format << log;
        }
    }
};

const std::string AudioPlatformState::mStateChangedCriterionName = "StatesChanged";
const std::string AudioPlatformState::mVoipBandCriterionName = "VoIPBandType";
const std::string AudioPlatformState::mOutputFlagsCriterionName = "OutputFlags";
const std::string AudioPlatformState::mInputDevicesCriterionName = "InputDevices";
const std::string AudioPlatformState::mAndroidModeCriterionName = "AndroidMode";
const std::string AudioPlatformState::mInputSourcesCriterionName = "InputSources";
const std::string AudioPlatformState::mPreProcRequestedByActiveInputCriterionName =
    "PreProcessorRequestedByActiveInput";
const std::string AudioPlatformState::mKeyAndroidMode =  "android_mode";
const std::string AudioPlatformState::mKeyDeviceOut = "output_devices";
const std::string AudioPlatformState::mKeyDeviceIn =  "input_devices";
const std::string AudioPlatformState::mKeyMicMute = "mic_mute";

template <>
struct AudioPlatformState::parameterManagerElementSupported<Criterion> {};
template <>
struct AudioPlatformState::parameterManagerElementSupported<CriterionType> {};

AudioPlatformState::AudioPlatformState(IStreamInterface *streamInterface)
    : mStreamInterface(streamInterface),
      mRoutePfwConnectorLogger(new ParameterMgrPlatformConnectorLogger),
      mAudioPfwHasChanged(false),
      mParameterAdapter(new ParameterAdapter(this))
{
    /// Connector
    // Fetch the name of the PFW configuration file: this name is stored in an Android property
    // and can be different for each hardware
    string routePfwConfFilePath = TProperty<string>(mRoutePfwConfFileNamePropName,
                                                    mRoutePfwDefaultConfFileName);
    Log::Info() << __FUNCTION__
                << ": Route-PFW: using configuration file: " << routePfwConfFilePath;

    mRoutePfwConnector = new CParameterMgrPlatformConnector(routePfwConfFilePath);

    // Logger
    mRoutePfwConnector->setLogger(mRoutePfwConnectorLogger);

    /// Creates State Changed criterion type.
    // This criterion type will be populated by all route criteria found in the configuration file.
    CriterionType *stateChangedCriterionType = new CriterionType(mStateChangedCriterionName, true,
                                                                 mRoutePfwConnector);
    mRouteCriterionTypeMap[mStateChangedCriterionName] = stateChangedCriterionType;

    if ((loadAudioHalConfig(gAudioHalVendorConfFilePath) != android::OK) &&
        (loadAudioHalConfig(gAudioHalConfFilePath) != android::OK)) {
        Log::Error() << "Neither vendor conf file (" << gAudioHalVendorConfFilePath
                     << ") nor system conf file (" << gAudioHalConfFilePath << ") could be found";
    }

    /// Creates hasChanged route criterion
    // Route Criteria
    mRouteCriterionMap[mStateChangedCriterionName] = new Criterion(mStateChangedCriterionName,
                                                                   stateChangedCriterionType,
                                                                   mRoutePfwConnector);
}

AudioPlatformState::~AudioPlatformState()
{
    /// Stop All Modem Proxies
    for (uint32_t index = 0; index < mModemProxyVector.size(); index++) {
        mModemProxyVector[index]->stop();
    }

    mParameterAdapter->stop();
    delete mParameterAdapter;

    // Delete All criterion
    CriterionMapIterator it;
    for (it = mRouteCriterionMap.begin(); it != mRouteCriterionMap.end(); ++it) {

        delete it->second;
    }

    // Delete All criterion type
    CriterionTypeMapIterator iter;
    for (iter = mRouteCriterionTypeMap.begin(); iter != mRouteCriterionTypeMap.end(); ++iter) {

        delete iter->second;
    }

    // Unset logger
    mRoutePfwConnector->setLogger(NULL);
    // Remove logger
    delete mRoutePfwConnectorLogger;
    // Remove connector
    delete mRoutePfwConnector;
}

status_t AudioPlatformState::start()
{
    /// Start PFW
    std::string strError;
    if (!mRoutePfwConnector->start(strError)) {
        Log::Error() << "Route PFW start error: " << strError;
        return android::NO_INIT;
    }
    Log::Debug() << __FUNCTION__ << ": Route PFW successfully started!";

    /// Start All Modem Proxies
    for (uint32_t index = 0; index < mModemProxyVector.size(); index++) {
        mModemProxyVector[index]->start();
    }
    /// Start ParameterAdapter
    mParameterAdapter->start();

    return android::OK;
}

template <>
void AudioPlatformState::addCriterionType<AudioPlatformState::Audio>(const string &typeName,
                                                                     bool isInclusive)
{
    if (mStreamInterface->addCriterionType(typeName, isInclusive)) {
        Log::Verbose() << __FUNCTION__ << ": criterionType " << typeName
                       << " already added in Audio PFW";
    }
}

template <>
void AudioPlatformState::addCriterionType<AudioPlatformState::Route>(const string &typeName,
                                                                     bool isInclusive)
{
    AUDIOCOMMS_ASSERT(mRouteCriterionTypeMap.find(typeName) == mRouteCriterionTypeMap.end(),
                      "CriterionType " << typeName << " already added");

    Log::Debug() << __FUNCTION__ << ": Adding new criterionType " << typeName << " for Route PFW";
    mRouteCriterionTypeMap[typeName] = new CriterionType(typeName,
                                                         isInclusive,
                                                         mRoutePfwConnector);
}

template <>
void AudioPlatformState::addCriterionTypeValuePair<AudioPlatformState::Audio>(
    const string &typeName,
    uint32_t numericValue,
    const string &literalValue)
{
    mStreamInterface->addCriterionTypeValuePair(typeName, literalValue, numericValue);
}

template <>
void AudioPlatformState::addCriterionTypeValuePair<AudioPlatformState::Route>(
    const string &typeName,
    uint32_t numericValue,
    const string &literalValue)
{
    AUDIOCOMMS_ASSERT(mRouteCriterionTypeMap.find(typeName) != mRouteCriterionTypeMap.end(),
                      "CriterionType " << typeName.c_str() << "not found");
    Log::Verbose() << __FUNCTION__ << ": Adding new value pair (" << numericValue
                   << "," << literalValue << ") for criterionType " << typeName << " for Route PFW";
    CriterionType *criterionType = mRouteCriterionTypeMap[typeName];
    criterionType->addValuePair(numericValue, literalValue);
}

template <AudioPlatformState::PfwInstance pfw>
void AudioPlatformState::loadCriterionType(cnode *root, bool isInclusive)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node;
    for (node = root->first_child; node != NULL; node = node->next) {

        AUDIOCOMMS_ASSERT(node != NULL, "error in parsing file");
        const char *typeName = node->name;
        char *valueNames = (char *)node->value;

        addCriterionType<pfw>(typeName, isInclusive);

        uint32_t index = 0;
        char *ctx;
        char *valueName = strtok_r(valueNames, ",", &ctx);
        while (valueName != NULL) {
            if (strlen(valueName) != 0) {

                // Conf file may use or not pair, if no pair, use incremental index, else
                // use provided index.
                if (strchr(valueName, ':') != NULL) {

                    char *first = strtok(valueName, ":");
                    char *second = strtok(NULL, ":");
                    AUDIOCOMMS_ASSERT((first != NULL) && (strlen(first) != 0) &&
                                      (second != NULL) && (strlen(second) != 0),
                                      "invalid value pair");

                    if (!convertTo<string, uint32_t>(first, index)) {
                        Log::Error() << __FUNCTION__ << ": Invalid index(" << first << ") found";
                    }
                    Log::Verbose() << __FUNCTION__ << ": name=" << typeName << ", index=" << index
                                   << ", value=" << second;
                    addCriterionTypeValuePair<pfw>(typeName, index, second);
                } else {

                    uint32_t pfwIndex = isInclusive ? 1 << index : index;
                    Log::Verbose() << __FUNCTION__ << ": name=" << typeName
                                   << ", index=" << pfwIndex << ", value=" << valueName;
                    addCriterionTypeValuePair<pfw>(typeName, pfwIndex, valueName);
                    index += 1;
                }
            }
            valueName = strtok_r(NULL, ",", &ctx);
        }
    }
}

template <AudioPlatformState::PfwInstance pfw>
void AudioPlatformState::loadInclusiveCriterionType(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, gInclusiveCriterionTypeTag.c_str());
    if (node == NULL) {
        return;
    }
    loadCriterionType<pfw>(node, true);
}

template <AudioPlatformState::PfwInstance pfw>
void AudioPlatformState::loadExclusiveCriterionType(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, gExclusiveCriterionTypeTag.c_str());
    if (node == NULL) {
        return;
    }
    loadCriterionType<pfw>(node, false);
}


void AudioPlatformState::addParameter(Parameter *param,
                                      const vector<AndroidParamMappingValuePair> &valuePairs)
{
    for_each(valuePairs.begin(), valuePairs.end(), SetAndroidParamMappingPairHelper(param));
    mParameterVector.push_back(param);
}

template <>
void AudioPlatformState::addParameter<AudioPlatformState::Audio, AudioPlatformState::ParamRogue>(
    const std::string &typeName, const std::string &paramKey, const std::string &name,
    const std::string &defaultValue, const std::vector<AndroidParamMappingValuePair> &valuePairs)
{
    Parameter *rogueParam;
    if (typeName == gUnsignedIntegerTypeTag) {
        rogueParam = new AudioRogueParameter<uint32_t>(this, paramKey,
                                                       name,
                                                       mStreamInterface,
                                                       defaultValue);
    } else if (typeName == gStringTypeTag) {
        rogueParam = new AudioRogueParameter<string>(this, paramKey, name,
                                                     mStreamInterface,
                                                     defaultValue);
    } else {
        Log::Error() << __FUNCTION__ << ": type " << typeName << " not supported ";
        return;
    }
    addParameter(rogueParam, valuePairs);
}

template <>
void AudioPlatformState::addParameter<AudioPlatformState::Audio,
                                      AudioPlatformState::ParamCriterion>(
    const std::string &typeName, const std::string &paramKey, const std::string &name,
    const std::string &defaultValue,
    const std::vector<AndroidParamMappingValuePair> &valuePairs)
{
    Parameter *paramCriterion = new AudioCriterionParameter(this, paramKey, name, typeName,
                                                            mStreamInterface, defaultValue);
    addParameter(paramCriterion, valuePairs);
}

template <>
void AudioPlatformState::addParameter<AudioPlatformState::Route,
                                      AudioPlatformState::ParamCriterion>(
    const std::string &typeName, const std::string &paramKey, const std::string &name,
    const std::string &defaultValue,
    const std::vector<AndroidParamMappingValuePair> &valuePairs)
{
    CriterionType *criterionType = getElement<CriterionType>(typeName, mRouteCriterionTypeMap);
    RouteCriterionParameter *routeParamCriterion = new RouteCriterionParameter(
        this, paramKey, name, criterionType, mRoutePfwConnector, defaultValue);
    addParameter(routeParamCriterion, valuePairs);
    addRouteCriterion(routeParamCriterion->getCriterion());
}

void AudioPlatformState::addRouteCriterion(Criterion *routeCriterion)
{
    AUDIOCOMMS_ASSERT(routeCriterion != NULL, "Invalid Route Criterion");
    const string criterionName = routeCriterion->getName();
    AUDIOCOMMS_ASSERT(mRouteCriterionMap.find(criterionName) == mRouteCriterionMap.end(),
                      "Route Criterion " << criterionName << " already added");
    mRouteCriterionTypeMap[mStateChangedCriterionName]->addValuePair(1 << mRouteCriterionMap.size(),
                                                                     criterionName);
    mRouteCriterionMap[criterionName] = routeCriterion;
}

template <>
void AudioPlatformState::addParameter<AudioPlatformState::Route, AudioPlatformState::ParamRogue>(
    const std::string &typeName, const std::string &paramKey, const std::string &name,
    const std::string &defaultValue,
    const std::vector<AndroidParamMappingValuePair> &valuePairs)
{
    RogueParameter *paramRogue;
    if (typeName == gUnsignedIntegerTypeTag) {
        paramRogue = new RouteRogueParameter<uint32_t>(this, paramKey, name, mRoutePfwConnector,
                                                       defaultValue);
    } else if (typeName == gStringTypeTag) {
        paramRogue = new RouteRogueParameter<string>(this, paramKey, name, mRoutePfwConnector,
                                                     defaultValue);
    } else {
        Log::Error() << __FUNCTION__ << ": type " << typeName << " not supported ";
        return;
    }
    addParameter(paramRogue, valuePairs);
}

void AudioPlatformState::parseChildren(cnode *root,
                                       string &path,
                                       string &defaultValue,
                                       string &key,
                                       string &type,
                                       vector<AndroidParamMappingValuePair> &valuePairs)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node;
    for (node = root->first_child; node != NULL; node = node->next) {
        AUDIOCOMMS_ASSERT(node != NULL, "error in parsing file");

        if (string(node->name) == gPathTag) {
            path = node->value;
        } else if (string(node->name) == gParameterDefaultTag) {
            defaultValue = node->value;
        } else if (string(node->name) == gAndroidParameterTag) {
            key = node->value;
        } else if (string(node->name) == gMappingTableTag) {
            valuePairs = parseMappingTable(node->value);
        } else if (string(node->name) == gTypeTag) {
            type = node->value;
        } else {
            Log::Error() << __FUNCTION__
                         << ": Unrecognized " << node->name << " " << node->value << " node ";
        }
    }
    Log::Verbose() << __FUNCTION__ << ": path=" << path << ",  key=" << key
                   << " default=" << defaultValue << ", type=" << type << "";
}

template <AudioPlatformState::PfwInstance pfw>
void AudioPlatformState::loadRogueParameterType(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");

    const char *rogueParameterName = root->name;

    vector<AndroidParamMappingValuePair> valuePairs;
    string paramKeyName = "";
    string rogueParameterPath = "";
    string typeName = "";
    string defaultValue = "";

    parseChildren(root, rogueParameterPath, defaultValue, paramKeyName, typeName, valuePairs);

    AUDIOCOMMS_ASSERT(!paramKeyName.empty(), "Rogue Parameter " << rogueParameterName <<
                      " not associated to any Android parameter");

    addParameter<pfw, ParamRogue>(typeName,
                                  paramKeyName,
                                  rogueParameterPath,
                                  defaultValue,
                                  valuePairs);
}

template <AudioPlatformState::PfwInstance pfw>
void AudioPlatformState::loadRogueParameterTypeList(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, gRogueParameterTag.c_str());
    if (node == NULL) {
        Log::Warning() << __FUNCTION__ << ": no rogue parameter type found";
        return;
    }
    for (node = node->first_child; node != NULL; node = node->next) {
        loadRogueParameterType<pfw>(node);
    }
}

template <typename T>
T *AudioPlatformState::getElement(const string &name, map<string, T *> &elementsMap)
{
    parameterManagerElementSupported<T>();
    typename map<string, T *>::iterator it = elementsMap.find(name);
    AUDIOCOMMS_ASSERT(it != elementsMap.end(), "Element " << name << " not found");
    return it->second;
}

template <typename T>
const T *AudioPlatformState::getElement(const string &name,
                                        const map<string, T *> &elementsMap) const
{
    parameterManagerElementSupported<T>();
    typename map<string, T *>::const_iterator it = elementsMap.find(name);
    AUDIOCOMMS_ASSERT(it != elementsMap.end(), "Element " << name << " not found");
    return it->second;
}

template <AudioPlatformState::PfwInstance pfw>
void AudioPlatformState::loadCriteria(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, gCriterionTag.c_str());

    if (node == NULL) {
        Log::Warning() << __FUNCTION__ << ": no inclusive criteria found";
        return;
    }
    for (node = node->first_child; node != NULL; node = node->next) {
        loadCriterion<pfw>(node);
    }
}

vector<AudioPlatformState::AndroidParamMappingValuePair> AudioPlatformState::parseMappingTable(
    const char *values)
{
    AUDIOCOMMS_ASSERT(values != NULL, "error in parsing file");
    char *mappingPairs = (char *)(values);
    char *ctx;
    vector<AndroidParamMappingValuePair> valuePairs;

    char *mappingPair = strtok_r(mappingPairs, ",", &ctx);
    while (mappingPair != NULL) {
        if (strlen(mappingPair) != 0) {

            char *first = strtok(mappingPair, ":");
            char *second = strtok(NULL, ":");
            AUDIOCOMMS_ASSERT((first != NULL) && (strlen(first) != 0) &&
                              (second != NULL) && (strlen(second) != 0),
                              "invalid value pair");
            AndroidParamMappingValuePair pair = make_pair(first, second);
            valuePairs.push_back(pair);
        }
        mappingPair = strtok_r(NULL, ",", &ctx);
    }
    return valuePairs;
}

template <>
void AudioPlatformState::addCriterion<AudioPlatformState::Audio>(const string &name,
                                                                 const string &typeName,
                                                                 const string &defaultLiteralValue)
{
    mStreamInterface->addCriterion(name, typeName, defaultLiteralValue);
}

template <>
void AudioPlatformState::addCriterion<AudioPlatformState::Route>(const string &name,
                                                                 const string &typeName,
                                                                 const string &defaultLiteralValue)
{
    CriterionType *criterionType = getElement<CriterionType>(typeName, mRouteCriterionTypeMap);
    addRouteCriterion(new Criterion(name, criterionType, mRoutePfwConnector, defaultLiteralValue));
}

template <AudioPlatformState::PfwInstance pfw>
void AudioPlatformState::loadCriterion(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    const char *criterionName = root->name;

    AUDIOCOMMS_ASSERT(mRouteCriterionMap.find(criterionName) == mRouteCriterionMap.end(),
                      "Criterion " << criterionName << " already added");

    vector<AndroidParamMappingValuePair> valuePairs;
    string paramKeyName = "";
    string path = "";
    string typeName = "";
    string defaultValue = "";

    parseChildren(root, path, defaultValue, paramKeyName, typeName, valuePairs);

    if (!paramKeyName.empty()) {
        /**
         * If a parameter key is found, this criterion is linked to a parameter received from
         * AudioSystem::setParameters.
         */
        addParameter<pfw, ParamCriterion>(typeName,
                                          paramKeyName,
                                          criterionName,
                                          defaultValue,
                                          valuePairs);
    } else {
        addCriterion<pfw>(criterionName, typeName, defaultValue);
    }
}

template <>
const string &AudioPlatformState::getPfwInstanceName<AudioPlatformState::Audio>() const
{
    return gAudioConfTag;
}

template <>
const string &AudioPlatformState::getPfwInstanceName<AudioPlatformState::Route>() const
{
    return gRouteConfTag;
}

template <AudioPlatformState::PfwInstance pfw>
void AudioPlatformState::loadConfig(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, getPfwInstanceName<pfw>().c_str());
    if (node == NULL) {
        Log::Warning() << __FUNCTION__
                       << ": Could not find node for pfw=" << getPfwInstanceName<pfw>();
        return;
    }
    Log::Debug() << __FUNCTION__ << " Loading conf for pfw " << getPfwInstanceName<pfw>();

    loadInclusiveCriterionType<pfw>(node);
    loadExclusiveCriterionType<pfw>(node);
    loadCriteria<pfw>(node);
    loadRogueParameterTypeList<pfw>(node);
}

/**
 * ModemProxy loadValueSet specialization. Only this one is available until now.
 */
template <>
void AudioPlatformState::loadValueSet<ModemProxy>(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    string libraryName;
    string libraryInstance;
    cnode *node;
    for (node = root->first_child; node != NULL; node = node->next) {
        AUDIOCOMMS_ASSERT(node != NULL, "error in parsing file");

        if (string(node->name) == gInterfaceLibraryName) {
            libraryName = node->value;
        } else if (string(node->name) == gInterfaceLibraryInstance) {
            libraryInstance = node->value;
        } else {
            Log::Error() << __FUNCTION__
                         << ": Unrecognized " << node->name << " " << node->value << " node ";
        }
    }
    Log::Verbose() << __FUNCTION__ << ": Instantiate (lib=" << libraryName
                   << ", Instance=" << libraryInstance << ") ValueSet ";
    ModemProxy *modemProxy = new ModemProxy(libraryName,
                                            libraryInstance,
                                            mParameterAdapter,
                                            mParameterAdapter);
    mModemProxyVector.push_back(modemProxy);
}

/**
 * ModemProxy loadValueSet specialization. Only this one is available until now.
 */
template <>
void AudioPlatformState::loadValueSetList<ModemProxy>(cnode *root)
{
    AUDIOCOMMS_ASSERT(root != NULL, "error in parsing file");
    cnode *node = config_find(root, gModemValueSet.c_str());
    if (node == NULL) {
        Log::Warning() << __FUNCTION__ << ": Could not find node for ValueSet=" << gModemValueSet;
        return;
    }
    Log::Verbose() << __FUNCTION__ << ": Loading conf for ValueSet=" << gModemValueSet;
    for (node = node->first_child; node != NULL; node = node->next) {
        AUDIOCOMMS_ASSERT(node != NULL, "error in parsing file");
        loadValueSet<ModemProxy>(node);
    }
}

status_t AudioPlatformState::loadAudioHalConfig(const char *path)
{
    AUDIOCOMMS_ASSERT(path != NULL, "error in parsing file: empty path");
    cnode *root;
    char *data;
    Log::Debug() << __FUNCTION__;
    data = (char *)load_file(path, NULL);
    if (data == NULL) {
        return -ENODEV;
    }
    root = config_node("", "");
    AUDIOCOMMS_ASSERT(root != NULL, "Unable to allocate a configuration node");
    config_load(root, data);

    loadConfig<Audio>(root);
    loadConfig<Route>(root);
    loadValueSetList<ModemProxy>(root);

    config_free(root);
    free(root);
    free(data);

    Log::Debug() << __FUNCTION__ << ": loaded " << path;

    return android::OK;
}

void AudioPlatformState::sync()
{
    std::for_each(mParameterVector.begin(), mParameterVector.end(), SyncParameterHelper());
    applyPlatformConfiguration();
}

void AudioPlatformState::clearKeys(KeyValuePairs *pairs)
{
    std::for_each(mParameterVector.begin(), mParameterVector.end(),
                  ClearKeyAndroidParameterHelper(pairs));
    if (pairs->size()) {
        Log::Warning() << __FUNCTION__ << ": Unhandled argument: " << pairs->toString();
    }
}

status_t AudioPlatformState::setParameters(const string &keyValuePairs)
{
    mPfwLock.writeLock();

    Log::Debug() << __FUNCTION__ << ": key value pair " << keyValuePairs;
    KeyValuePairs pairs(keyValuePairs);
    int errorCount = 0;
    std::for_each(mParameterVector.begin(), mParameterVector.end(),
                  SetFromAndroidParameterHelper(&pairs, &errorCount));
    status_t status = errorCount == 0 ? android::OK : android::BAD_VALUE;
    clearKeys(&pairs);

    if (!hasPlatformStateChanged()) {
        mPfwLock.unlock();
        return status;
    }
    // Apply Configuration
    applyPlatformConfiguration();

    // Release PFS ressource
    mPfwLock.unlock();

    // Trig the route manager
    mStreamInterface->reconsiderRouting();

    return status;
}

void AudioPlatformState::parameterHasChanged(const std::string &event)
{
    // Handle particular cases, event is the criterion name, not the key
    if (event == mAndroidModeCriterionName) {
        VolumeKeys::wakeup(getValue(mAndroidModeCriterionName) == AUDIO_MODE_IN_CALL);
    } else if (event == mInputDevicesCriterionName) {
        updateActiveStreamsParameters(false);
    }
    setPlatformStateEvent(event);
}

string AudioPlatformState::getParameters(const string &keys)
{
    AutoR lock(mPfwLock);
    KeyValuePairs pairs(keys);
    KeyValuePairs returnedPairs(keys);

    std::for_each(mParameterVector.begin(), mParameterVector.end(),
                  GetFromAndroidParameterHelper(&pairs, &returnedPairs));

    return returnedPairs.toString();
}

bool AudioPlatformState::hasPlatformStateChanged() const
{
    CriterionMapConstIterator it = mRouteCriterionMap.find(mStateChangedCriterionName);
    AUDIOCOMMS_ASSERT(it != mRouteCriterionMap.end(),
                      "state " << mStateChangedCriterionName << " not found");

    return (it->second->getValue() != 0) || mAudioPfwHasChanged;
}

void AudioPlatformState::setPlatformStateEvent(const string &eventStateName)
{
    Criterion *stateChange = getElement<Criterion>(mStateChangedCriterionName, mRouteCriterionMap);

    // Checks if eventState name is a possible value of HasChanged criteria
    int eventId = 0;
    if (!stateChange->getCriterionType()->getTypeInterface()->getNumericalValue(
            eventStateName, eventId)) {

        // Checks if eventState name is a possible value of HasChanged criteria of Route PFW.
        // If not, consider that this event is related to Audio PFW Instance.
        mAudioPfwHasChanged = true;
    }
    uint32_t platformEventChanged = stateChange->getValue() | eventId;
    stateChange->setValue<uint32_t>(platformEventChanged);
}

void AudioPlatformState::setVoipBandType(const IoStream *activeStream)
{
    CAudioBand::Type band = CAudioBand::EWide;
    if (activeStream->getSampleRate() == mVoiceStreamRateForNarrowBandProcessing) {

        band = CAudioBand::ENarrow;
    }
    setValue(band, mVoipBandCriterionName);
}

void AudioPlatformState::updateRequestedEffect()
{
    AutoW lock(mPfwLock);
    updateActiveStreamsParameters(false);
}

void AudioPlatformState::updateActiveStreamsParameters(bool isOut)
{
    StreamListConstIterator it;
    uint32_t streamsMask = 0;
    uint32_t effectRequested = 0;

    for (it = mActiveStreamsList[isOut].begin(); it != mActiveStreamsList[isOut].end(); ++it) {
        const IoStream *stream = *it;
        if (stream->isRoutedByPolicy()) {
            streamsMask |= stream->getApplicabilityMask();
            if (!isOut) {
                // Set the requested effect from this active input.
                effectRequested = stream->getEffectRequested();
                // Set the band type according to this active input.
                setVoipBandType(stream);
                // One and only one input stream must be active. @todo: check for L-dessert.
                break;
            }
        }
    }
    setValue(streamsMask, isOut ? mOutputFlagsCriterionName : mInputSourcesCriterionName);
    if (!isOut) {
        setValue(effectRequested, mPreProcRequestedByActiveInputCriterionName);
    }
    applyPlatformConfiguration();
}

void AudioPlatformState::startStream(const IoStream *startedStream)
{
    AUDIOCOMMS_ASSERT(startedStream != NULL, "NULL stream");
    AutoW lock(mPfwLock);
    bool isOut = startedStream->isOut();
    mActiveStreamsList[isOut].push_back(startedStream);
    updateActiveStreamsParameters(isOut);
}

void AudioPlatformState::stopStream(const IoStream *stoppedStream)
{
    AUDIOCOMMS_ASSERT(stoppedStream != NULL, "NULL stream");
    AutoW lock(mPfwLock);
    bool isOut = stoppedStream->isOut();
    mActiveStreamsList[isOut].remove(stoppedStream);
    updateActiveStreamsParameters(isOut);
}

void AudioPlatformState::clearPlatformStateEvents()
{
    mRouteCriterionMap[mStateChangedCriterionName]->setValue<uint32_t>(0);
    mAudioPfwHasChanged = false;
}

bool AudioPlatformState::isStarted()
{
    Log::Debug() << __FUNCTION__ << ": "
                 << (mRoutePfwConnector && mRoutePfwConnector->isStarted() ? "true" : "false");
    return mRoutePfwConnector && mRoutePfwConnector->isStarted();
}

void AudioPlatformState::applyPlatformConfiguration()
{
    mRouteCriterionMap[mStateChangedCriterionName]->setCriterionState();
    mRoutePfwConnector->applyConfigurations();
    clearPlatformStateEvents();
}

void AudioPlatformState::setValue(int value, const string &stateName)
{
    if (getElement<Criterion>(stateName, mRouteCriterionMap)->setCriterionState(value)) {

        setPlatformStateEvent(stateName);
    }
}

int AudioPlatformState::getValue(const std::string &stateName) const
{
    return getElement<Criterion>(stateName, mRouteCriterionMap)->getValue();
}

bool AudioPlatformState::isModemEmbedded() const
{
    return !mModemProxyVector.empty();
}

void AudioPlatformState::printPlatformFwErrorInfo() const
{
    Log::Error() << "^^^^  Print platform Audio firmware error info  ^^^^";

    string paramValue;

    AutoR lock(mPfwLock);
    /**
     * Get the list of files path we wish to print. This list is represented as a
     * string defined in the route manager RouteDebugFs plugin.
     */
    if (!ParameterMgrHelper::getParameterValue<std::string>(mRoutePfwConnector,
                                                            mHwDebugFilesPathList,
                                                            paramValue)) {
        Log::Error() << "Could not get path list from XML configuration";
        return;
    }

    vector<std::string> debugFiles;
    char *debugFile;
    string debugFileString;
    char *tokenString = static_cast<char *>(alloca(paramValue.length() + 1));
    vector<std::string>::const_iterator it;

    strncpy(tokenString, paramValue.c_str(), paramValue.length() + 1);

    while ((debugFile = NaiveTokenizer::getNextToken(&tokenString)) != NULL) {
        debugFileString = string(debugFile);
        debugFileString = debugFile;
        debugFiles.push_back(debugFileString);
    }

    for (it = debugFiles.begin(); it != debugFiles.end(); ++it) {
        ifstream debugStream;
        Log::Error() << "Opening file " << *it << " and reading it.";
        debugStream.open(it->c_str(), ifstream::in);

        if (debugStream.fail()) {
            Log::Error() << __FUNCTION__ << ": Unable to open file" << *it
                         << " with failbit " << (debugStream.rdstate() & ifstream::failbit)
                         << " and badbit " << (debugStream.rdstate() & ifstream::badbit);
            debugStream.close();
            continue;
        }

        while (debugStream.good()) {
            char dataToRead[mMaxDebugStreamSize];

            debugStream.read(dataToRead, mMaxDebugStreamSize);
            Log::Error() << dataToRead;
        }

        debugStream.close();
    }
}

} // namespace intel_audio
