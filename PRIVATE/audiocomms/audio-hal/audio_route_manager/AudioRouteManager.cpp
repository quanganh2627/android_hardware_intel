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
#define LOG_TAG "RouteManager"

#include "AudioRouteManager.hpp"
#include "AudioRouteManagerObserver.hpp"
#include "InterfaceProviderLib.h"
#include "Property.h"
#include <Observer.hpp>
#include <IoStream.hpp>
#include <BitField.hpp>
#include <cutils/bitops.h>
#include <string>

#include <utilities/Log.hpp>

using android::status_t;
using namespace std;
using NInterfaceProvider::CInterfaceProviderImpl;
using audio_comms::utilities::BitField;
using audio_comms::utilities::Direction;
using audio_comms::utilities::Log;

typedef android::RWLock::AutoRLock AutoR;
typedef android::RWLock::AutoWLock AutoW;


namespace intel_audio
{

const char *const AudioRouteManager::mVoiceVolume =
    "/Audio/CONFIGURATION/VOICE_VOLUME_CTRL_PARAMETER";

const char *const AudioRouteManager::mClosingRouteCriterion[Direction::_nbDirections] = {
    "ClosingCaptureRoutes", "ClosingPlaybackRoutes"
};
const char *const AudioRouteManager::mOpenedRouteCriterion[Direction::_nbDirections] = {
    "OpenedCaptureRoutes", "OpenedPlaybackRoutes"
};
const char *const AudioRouteManager::mRouteCriterionType = "RouteType";
const char *const AudioRouteManager::mRoutingStage = "RoutageState";

const char *const AudioRouteManager::mAudioPfwConfFilePropName = "AudioComms.PFW.ConfPath";

const char *const AudioRouteManager::mAudioPfwDefaultConfFileName =
    "/etc/parameter-framework/ParameterFrameworkConfiguration.xml";

class CParameterMgrPlatformConnectorLogger : public CParameterMgrPlatformConnector::ILogger
{
public:
    CParameterMgrPlatformConnectorLogger() {}

    virtual void log(bool isWarning, const string &log)
    {
        const static string format("audio-parameter-manager: ");

        if (isWarning) {
            Log::Warning() << format << log;
        } else {
            Log::Debug() << format << log;
        }
    }
};

/**
 * Routing stage criteria.
 *
 * A Flow stage on ClosingRoutes leads to Mute.
 * A Flow stage on OpenedRoutes leads to Unmute.
 * A Path stage on ClosingRoutes leads to Disable.
 * A Path stage on OpenedRoutes leads to Enable.
 * A Configure stage on ClosingRoutes leads to resetting the configuration.
 * A Configure stage on OpenedRoute lead to setting the configuration.
 */
const pair<int, const char *> AudioRouteManager::mRoutingStageValuePairs[] = {
    make_pair(FlowMask,       "Flow"),
    make_pair(PathMask,       "Path"),
    make_pair(ConfigureMask,  "Configure")
};

template <>
struct AudioRouteManager::routingElementSupported<AudioPort> {};
template <>
struct AudioRouteManager::routingElementSupported<AudioPortGroup> {};
template <>
struct AudioRouteManager::routingElementSupported<AudioRoute> {};
template <>
struct AudioRouteManager::routingElementSupported<AudioStreamRoute> {};
template <>
struct AudioRouteManager::routingElementSupported<Criterion> {};

AudioRouteManager::AudioRouteManager()
    : mRouteInterface(this),
      mStreamInterface(this),
      mAudioPfwConnectorLogger(new CParameterMgrPlatformConnectorLogger),
      mEventThread(new CEventThread(this)),
      mIsStarted(false)
{
    memset(mRoutes, 0, sizeof(mRoutes[0]) * Direction::_nbDirections);

    /// Connector
    // Fetch the name of the PFW configuration file: this name is stored in an Android property
    // and can be different for each hardware
    string audioPfwConfigurationFilePath = TProperty<string>(mAudioPfwConfFilePropName,
                                                             mAudioPfwDefaultConfFileName);
    Log::Info() << __FUNCTION__
                << ": audio PFW using configuration file: " << audioPfwConfigurationFilePath;

    mAudioPfwConnector = new CParameterMgrPlatformConnector(audioPfwConfigurationFilePath);

    mParameterHelper = new ParameterMgrHelper(mAudioPfwConnector);

    // Logger
    mAudioPfwConnector->setLogger(mAudioPfwConnectorLogger);
}

AudioRouteManager::~AudioRouteManager()
{
    AutoW lock(mRoutingLock);

    if (mIsStarted) {
        // Synchronous stop of the event thread must be called with NOT held lock as pending request
        // may need to be served
        mRoutingLock.unlock();
        mEventThread->stop();
        mRoutingLock.writeLock();
    }
    delete mEventThread;

    // Delete all routes
    RouteMapIterator it;
    for (it = mRouteMap.begin(); it != mRouteMap.end(); ++it) {

        delete it->second;
    }

    // Unset logger
    mAudioPfwConnector->setLogger(NULL);
    delete mParameterHelper;

    // Remove logger
    delete mAudioPfwConnectorLogger;

    // Remove connector
    delete mAudioPfwConnector;
}

// Interface populate
void AudioRouteManager::getImplementedInterfaces(CInterfaceProviderImpl &interfaceProvider)
{
    interfaceProvider.addInterface(&mRouteInterface);
    interfaceProvider.addInterface(&mStreamInterface);
}

status_t AudioRouteManager::stopService()
{
    AutoW lock(mRoutingLock);
    if (mIsStarted) {
        // Synchronous stop of the event thread must be called with NOT held lock as pending request
        // may need to be served
        mRoutingLock.unlock();
        mEventThread->stop();
        mRoutingLock.writeLock();
        mIsStarted = false;
    }
    return android::OK;
}

status_t AudioRouteManager::startService()
{
    AutoW lock(mRoutingLock);

    AUDIOCOMMS_ASSERT(mIsStarted != true, "Route Manager service already started!");
    AUDIOCOMMS_ASSERT(mEventThread->start(), "failure when starting event thread!");

    if (mAudioPfwConnector->isStarted()) {
        Log::Debug() << __FUNCTION__ << ": Audio PFW is already started, bailing out";
        mIsStarted = true;
        return android::OK;
    }

    // Routes Criterion Type
    AUDIOCOMMS_ASSERT(mCriterionTypesMap.find(mRouteCriterionType) != mCriterionTypesMap.end(),
                      "Route CriterionType not found");

    CriterionType *routeCriterionType = mCriterionTypesMap[mRouteCriterionType];

    // Route Criteria
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        mSelectedOpenedRoutes[i] = new Criterion(mOpenedRouteCriterion[i],
                                                 routeCriterionType,
                                                 mAudioPfwConnector);
        mSelectedClosingRoutes[i] = new Criterion(mClosingRouteCriterion[i],
                                                  routeCriterionType,
                                                  mAudioPfwConnector);
    }

    CriterionType *routageStageCriterionType = new CriterionType(mRoutingStage, true,
                                                                 mAudioPfwConnector);
    mCriterionTypesMap[mRoutingStage] = routageStageCriterionType;
    routageStageCriterionType->addValuePairs(mRoutingStageValuePairs,
                                             sizeof(mRoutingStageValuePairs) /
                                             sizeof(mRoutingStageValuePairs[0]));
    // Routing stage criterion is initialised to Configure | Path | Flow to apply all pending
    // configuration for init and minimalize cold latency at first playback / capture
    mRoutingStageCriterion = new Criterion(mRoutingStage, routageStageCriterionType,
                                           mAudioPfwConnector, Configure | Path | Flow);

    // Start PFW
    std::string strError;
    if (!mAudioPfwConnector->start(strError)) {
        Log::Error() << "parameter-manager start error: " << strError;
        mEventThread->stop();
        return android::NO_INIT;
    }
    Log::Debug() << __FUNCTION__ << ": parameter-manager successfully started!";
    mIsStarted = true;
    return android::OK;
}

void AudioRouteManager::reconsiderRouting(bool isSynchronous)
{
    AutoW lock(mRoutingLock);

    if (!mIsStarted) {
        Log::Warning() << __FUNCTION__
                       << ": Could not serve this request as Route Manager is not started";
        return;
    }

    AUDIOCOMMS_ASSERT(!mEventThread->inThreadContext(), "Failure: not in correct thread context!");

    if (!isSynchronous) {

        // Trigs the processing of the list
        mEventThread->trig(NULL);
    } else {

        // Create a route manager observer
        AudioRouteManagerObserver obs;

        // Add the observer to the route manager
        addObserver(&obs);

        // Trig the processing of the list
        mEventThread->trig(NULL);

        // Unlock to allow for sem wait
        mRoutingLock.unlock();

        // Wait a notification from the route manager
        obs.waitNotification();

        // Relock
        mRoutingLock.writeLock();

        // Remove the observer from Route Manager
        removeObserver(&obs);
    }
}

void AudioRouteManager::doReconsiderRouting()
{
    if (!checkAndPrepareRouting()) {

        // No need to reroute. Some criterion might have changed, update all criteria and apply
        // the conf in order to take for example tuning configuration that are glitch free and do
        // not need to go through the 5-steps routing.
        commitCriteriaAndApply();
        return;
    }
    Log::Debug() << __FUNCTION__
                 << ": Route state:"
                 << "\n\t-Previously Enabled Route in Input = "
                 << routeCriterionType()->getFormattedState(prevEnabledRoutes(Direction::Input))
                 << "\n\t-Previously Enabled Route in Output = "
                 << routeCriterionType()->getFormattedState(prevEnabledRoutes(Direction::Output))
                 << "\n\t-Selected Route in Input = "
                 << routeCriterionType()->getFormattedState(enabledRoutes(Direction::Input))
                 << "\n\t-Selected Route in Output = "
                 << routeCriterionType()->getFormattedState(enabledRoutes(Direction::Output))
                 << (needReflowRoutes(Direction::Input) ?
        "\n\t-Route that need reconfiguration in Input = " +
        routeCriterionType()->getFormattedState(needReflowRoutes(Direction::Input))
        : "")
                 << (needReflowRoutes(Direction::Output) ?
        "\n\t-Route that need reconfiguration in Output = "
        + routeCriterionType()->getFormattedState(needReflowRoutes(Direction::Output))
        : "")
                 << (needRepathRoutes(Direction::Input) ?
        "\n\t-Route that need rerouting in Input = " +
        routeCriterionType()->getFormattedState(needRepathRoutes(Direction::Input))
        : "")
                 << (needRepathRoutes(Direction::Output) ?
        "\n\t-Route that need rerouting in Output = "
        + routeCriterionType()->getFormattedState(needRepathRoutes(Direction::Output))
        : "");
    executeRouting();
    Log::Debug() << __FUNCTION__ << ": DONE";
}

void AudioRouteManager::executeRouting()
{
    executeMuteRoutingStage();

    executeDisableRoutingStage();

    executeConfigureRoutingStage();

    executeEnableRoutingStage();

    executeUnmuteRoutingStage();
}

void AudioRouteManager::resetRouting()
{
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {
        mRoutes[i].prevEnabled = mRoutes[i].enabled;
        mRoutes[i].enabled = 0;
        mRoutes[i].needReflow = 0;
        mRoutes[i].needRepath = 0;
    }

    resetAvailability<AudioRoute>(mRouteMap);
    resetAvailability<AudioPort>(mPortMap);
}

void AudioRouteManager::addStream(IoStream *stream)
{
    AUDIOCOMMS_ASSERT(stream != NULL, "Failure, invalid stream parameter");

    AutoW lock(mRoutingLock);
    mStreamsList[stream->isOut()].push_back(stream);
}

void AudioRouteManager::removeStream(IoStream *streamToRemove)
{
    AUDIOCOMMS_ASSERT(streamToRemove != NULL, "Failure, invalid stream parameter");

    AutoW lock(mRoutingLock);
    mStreamsList[streamToRemove->isOut()].remove(streamToRemove);
}

bool AudioRouteManager::checkAndPrepareRouting()
{
    resetRouting();

    RouteMapIterator it;
    for (it = mRouteMap.begin(); it != mRouteMap.end(); ++it) {

        AudioRoute *route =  it->second;
        prepareRoute(route);
        setBit(route->needReflow(), route->getId(), mRoutes[route->isOut()].needReflow);
        setBit(route->needRepath(), route->getId(), mRoutes[route->isOut()].needRepath);
    }

    return routingHasChanged<Direction::Output>() | routingHasChanged<Direction::Input>();
}

void AudioRouteManager::prepareRoute(AudioRoute *route)
{
    AUDIOCOMMS_ASSERT(route != NULL, "Failure, invalid route parameter");

    bool isApplicable = route->isStreamRoute() ?
                        setStreamForRoute(static_cast<AudioStreamRoute *>(route)) :
                        route->isApplicable();
    route->setUsed(isApplicable);
    setBit(isApplicable, route->getId(), mRoutes[route->isOut()].enabled);
}

bool AudioRouteManager::setStreamForRoute(AudioStreamRoute *route)
{
    AUDIOCOMMS_ASSERT(route != NULL, "Failure, invalid route parameter");

    bool isOut = route->isOut();

    StreamListIterator it;
    for (it = mStreamsList[isOut].begin(); it != mStreamsList[isOut].end(); ++it) {

        IoStream *stream = *it;
        if (stream->isStarted() && !stream->isNewRouteAvailable()) {

            if (route->isApplicable(stream)) {
                Log::Verbose() << __FUNCTION__
                               << ": stream route " << route->getName() << " is applicable";
                route->setStream(stream);
                return true;
            }
        }
    }

    return false;
}

void AudioRouteManager::executeMuteRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";
    mRoutingStageCriterion->setCriterionState<int32_t>(FlowMask);
    setRouteCriteriaForMute();
    mAudioPfwConnector->applyConfigurations();
}

void AudioRouteManager::executeDisableRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";
    mRoutingStageCriterion->setCriterionState<int32_t>(PathMask);
    setRouteCriteriaForDisable();
    doDisableRoutes();
    mAudioPfwConnector->applyConfigurations();
    doPostDisableRoutes();
}

void AudioRouteManager::executeConfigureRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";
    mRoutingStageCriterion->setCriterionState<int32_t>(ConfigureMask);

    StreamRouteMapIterator routeIt;
    for (routeIt = mStreamRouteMap.begin(); routeIt != mStreamRouteMap.end(); ++routeIt) {

        AudioStreamRoute *streamRoute = routeIt->second;
        if (streamRoute->needReflow()) {

            streamRoute->configure();
        }
    }

    setRouteCriteriaForConfigure();

    commitCriteriaAndApply();
}

void AudioRouteManager::executeEnableRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";
    mRoutingStageCriterion->setCriterionState<int32_t>(PathMask | ConfigureMask);
    doPreEnableRoutes();
    mAudioPfwConnector->applyConfigurations();
    doEnableRoutes();
}

void AudioRouteManager::executeUnmuteRoutingStage()
{
    Log::Debug() << "\t\t-" << __FUNCTION__ << "-";
    mRoutingStageCriterion->setCriterionState<int32_t>(ConfigureMask | PathMask | FlowMask);
    mAudioPfwConnector->applyConfigurations();
}

void AudioRouteManager::setRouteCriteriaForConfigure()
{
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        mSelectedClosingRoutes[i]->setCriterionState<int32_t>(0);
        mSelectedOpenedRoutes[i]->setCriterionState<int32_t>(enabledRoutes(i));
    }
}

void AudioRouteManager::setRouteCriteriaForMute()
{
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        uint32_t unmutedRoutes = prevEnabledRoutes(i) & enabledRoutes(i) & ~needReflowRoutes(i);
        uint32_t routesToMute = (prevEnabledRoutes(i) & ~enabledRoutes(i)) | needReflowRoutes(i);

        mSelectedClosingRoutes[i]->setCriterionState<int32_t>(routesToMute);
        mSelectedOpenedRoutes[i]->setCriterionState<int32_t>(unmutedRoutes);
    }
}

void AudioRouteManager::setRouteCriteriaForDisable()
{
    for (uint32_t i = 0; i < Direction::_nbDirections; i++) {

        uint32_t openedRoutes = prevEnabledRoutes(i) & enabledRoutes(i) & ~needRepathRoutes(i);
        uint32_t routesToDisable = (prevEnabledRoutes(i) & ~enabledRoutes(i)) | needRepathRoutes(i);

        mSelectedClosingRoutes[i]->setCriterionState<int32_t>(routesToDisable);
        mSelectedOpenedRoutes[i]->setCriterionState<int32_t>(openedRoutes);
    }
}

void AudioRouteManager::doDisableRoutes(bool isPostDisable)
{
    StreamRouteMapIterator it;
    for (it = mStreamRouteMap.begin(); it != mStreamRouteMap.end(); ++it) {

        AudioStreamRoute *streamRoute = it->second;

        if ((streamRoute->previouslyUsed() && !streamRoute->isUsed()) ||
            streamRoute->needRepath()) {
            Log::Verbose() << __FUNCTION__
                           << ": Route " << streamRoute->getName() << " to be disabled";
            streamRoute->unroute(isPostDisable);
        }
    }
}

void AudioRouteManager::doEnableRoutes(bool isPreEnable)
{
    StreamRouteMapIterator it;
    for (it = mStreamRouteMap.begin(); it != mStreamRouteMap.end(); ++it) {

        AudioStreamRoute *streamRoute = it->second;

        if ((!streamRoute->previouslyUsed() && streamRoute->isUsed()) ||
            streamRoute->needRepath()) {
            Log::Verbose() << __FUNCTION__
                           << ": Route" << streamRoute->getName() << " to be enabled";
            if (streamRoute->route(isPreEnable) != android::OK) {
                Log::Error() << "\t error while routing " << streamRoute->getName();
            }
        }
    }
}

template <typename T>
bool AudioRouteManager::addElement(const string &name, uint32_t id, map<string, T *> &elementsMap)
{
    if (mAudioPfwConnector->isStarted()) {
        Log::Warning() << __FUNCTION__ << ": Not allowed while Audio Parameter Manager running";
        return false;
    }
    routingElementSupported<T>();
    if (elementsMap.find(name) != elementsMap.end()) {
        Log::Warning() << __FUNCTION__ << ": element(" << name << ",=" << id << ") already added";
        return false;
    }
    elementsMap[name] = new T(name, id);
    return true;
}

template <typename T>
T *AudioRouteManager::getElement(const string &name, map<string, T *> &elementsMap)
{
    routingElementSupported<T>();
    typename map<string, T *>::iterator it = elementsMap.find(name);
    AUDIOCOMMS_ASSERT(it != elementsMap.end(), "Element " << name << " not found");
    return it->second;
}

template <typename T>
T *AudioRouteManager::findElementByName(const std::string &name, map<string, T *> elementsMap)
{
    routingElementSupported<T>();
    typename map<string, T *>::iterator it;
    it = elementsMap.find(name);
    return (it == elementsMap.end()) ? NULL : it->second;
}

template <typename T>
void AudioRouteManager::resetAvailability(map<string, T *> elementsMap)
{
    routingElementSupported<T>();
    typename map<string, T *>::iterator it;
    for (it = elementsMap.begin(); it != elementsMap.end(); ++it) {

        it->second->resetAvailability();
    }
}

bool AudioRouteManager::onEvent(int)
{
    return false;
}

bool AudioRouteManager::onError(int)
{
    return false;
}

bool AudioRouteManager::onHangup(int)
{
    return false;
}

void AudioRouteManager::onAlarm()
{
    Log::Debug() << __FUNCTION__;
}

void AudioRouteManager::onPollError()
{
}

bool AudioRouteManager::onProcess(void *, uint32_t)
{
    AutoW lock(mRoutingLock);
    doReconsiderRouting();

    // Notify all potential observer of Route Manager Subject
    notify();

    return false;
}

status_t AudioRouteManager::setVoiceVolume(float gain)
{
    AutoR lock(mRoutingLock);
    string error;
    bool ret;

    if ((gain < 0.0) || (gain > 1.0)) {
        Log::Warning() << __FUNCTION__ << ": (" << gain << ") out of range [0.0 .. 1.0]";
        return -ERANGE;
    }
    Log::Debug() << __FUNCTION__ << ": gain=" << gain;
    CParameterHandle *voiceVolumeHandle = mParameterHelper->getDynamicParameterHandle(mVoiceVolume);

    if (!voiceVolumeHandle) {
        Log::Error() << "Could not retrieve volume path handle";
        return android::INVALID_OPERATION;
    }

    if (voiceVolumeHandle->isArray()) {
        vector<double> gains;

        gains.push_back(gain);
        gains.push_back(gain);

        ret = voiceVolumeHandle->setAsDoubleArray(gains, error);
    } else {
        ret = voiceVolumeHandle->setAsDouble(gain, error);
    }

    if (!ret) {
        Log::Error() << __FUNCTION__
                     << ": Unable to set value " << gain
                     << ", from parameter path: " << voiceVolumeHandle->getPath()
                     << ", error=" << error;
        return android::INVALID_OPERATION;
    }
    return android::OK;
}

IoStream *AudioRouteManager::getVoiceOutputStream()
{
    AutoW lock(mRoutingLock);

    StreamListIterator it;
    // We take the first stream that corresponds to the primary output.
    it = mStreamsList[Direction::Output].begin();
    if (*it == NULL) {
        Log::Error() << __FUNCTION__ << ": current stream NOT FOUND for echo ref";
    }
    return *it;
}

const AudioStreamRoute *AudioRouteManager::findMatchingRoute(bool isOut, uint32_t flags) const
{
    uint32_t mask = !flags ?
                    (isOut ? static_cast<uint32_t>(AUDIO_OUTPUT_FLAG_PRIMARY) :
                     static_cast<uint32_t>(BitField::indexToMask(AUDIO_SOURCE_DEFAULT)))
                    : flags;

    StreamRouteMapConstIterator it;
    for (it = mStreamRouteMap.begin(); it != mStreamRouteMap.end(); ++it) {

        const AudioStreamRoute *streamRoute = it->second;
        if ((streamRoute->isOut() == isOut) && (mask & streamRoute->getApplicableMask())) {

            return streamRoute;
        }
    }
    return NULL;
}

uint32_t AudioRouteManager::getPeriodInUs(bool isOut, uint32_t flags) const
{
    AutoR lock(mRoutingLock);
    const AudioStreamRoute *route = findMatchingRoute(isOut, flags);
    if (route != NULL) {

        return route->getPeriodInUs();
    }
    Log::Error() << __FUNCTION__ << ": not route found, audio might not be functional";
    return 0;
}

uint32_t AudioRouteManager::getLatencyInUs(bool isOut, uint32_t flags) const
{
    AutoR lock(mRoutingLock);
    const AudioStreamRoute *route = findMatchingRoute(isOut, flags);
    if (route != NULL) {

        return route->getLatencyInUs();
    }
    Log::Error() << __FUNCTION__ << ": not route found, audio might not be functional";
    return 0;
}

void AudioRouteManager::setBit(bool isSet, uint32_t index, uint32_t &mask)
{
    if (isSet) {
        mask |= index;
    }
}

void AudioRouteManager::setRouteApplicable(const string &name, bool isApplicable)
{
    Log::Verbose() << __FUNCTION__ << ": " << name;
    AutoW lock(mRoutingLock);
    getElement<AudioRoute>(name, mRouteMap)->setApplicable(isApplicable);
}

void AudioRouteManager::setRouteNeedReconfigure(const string &name, bool needReconfigure)
{
    Log::Verbose() << __FUNCTION__ << ": " << name;
    AutoW lock(mRoutingLock);
    getElement<AudioRoute>(name, mRouteMap)->setNeedReconfigure(needReconfigure);
}

void AudioRouteManager::setRouteNeedReroute(const string &name, bool needReroute)
{
    Log::Verbose() << __FUNCTION__ << ": " << name;
    AutoW lock(mRoutingLock);
    getElement<AudioRoute>(name, mRouteMap)->setNeedReroute(needReroute);
}

void AudioRouteManager::updateStreamRouteConfig(const string &name,
                                                const StreamRouteConfig &config)
{
    AutoW lock(mRoutingLock);
    getElement<AudioStreamRoute>(name, mStreamRouteMap)->updateStreamRouteConfig(config);
}

void AudioRouteManager::addRouteSupportedEffect(const string &name, const string &effect)
{
    getElement<AudioStreamRoute>(name, mStreamRouteMap)->addEffectSupported(effect);
}

void AudioRouteManager::setPortBlocked(const string &name, bool isBlocked)
{
    AutoW lock(mRoutingLock);
    getElement<AudioPort>(name, mPortMap)->setBlocked(isBlocked);
}

template <bool isOut>
bool AudioRouteManager::routingHasChanged()
{
    return (prevEnabledRoutes(isOut) != enabledRoutes(isOut)) ||
           (needReflowRoutes(isOut) != 0) || (needRepathRoutes(isOut) != 0);
}

bool AudioRouteManager::addCriterionType(const std::string &name, bool isInclusive)
{
    AutoW lock(mRoutingLock);
    if (mAudioPfwConnector->isStarted()) {
        Log::Warning() << __FUNCTION__ << ": Not allowed while Audio Parameter Manager running";
        return true;
    }
    CriteriaTypeMapIterator it;
    if ((it = mCriterionTypesMap.find(name)) == mCriterionTypesMap.end()) {
        Log::Verbose() << __FUNCTION__ << ": adding " << name
                       << " criterion [" << (isInclusive ? "inclusive" : "exclusive") << "]";
        mCriterionTypesMap[name] = new CriterionType(name, isInclusive, mAudioPfwConnector);
        return false;
    }
    Log::Verbose() << __FUNCTION__ << ": already added " << name
                   << " criterion [" << (isInclusive ? "inclusive" : "exclusive") << "]";
    return true;
}

void AudioRouteManager::addCriterionTypeValuePair(const string &name,
                                                  const string &literal,
                                                  uint32_t value)
{
    AutoW lock(mRoutingLock);
    if (mAudioPfwConnector->isStarted()) {
        Log::Warning() << __FUNCTION__ << ": Not allowed while Audio Parameter Manager running";
        return;
    }
    AUDIOCOMMS_ASSERT(mCriterionTypesMap.find(name) != mCriterionTypesMap.end(),
                      "CriterionType " << name << " not found");

    CriterionType *criterionType = mCriterionTypesMap[name];

    if (criterionType->hasValuePairByName(literal)) {
        Log::Verbose() << __FUNCTION__ << ": value pair already added";
        return;
    }
    Log::Verbose() << __FUNCTION__ << ": appending new value pair (" << literal << "," << value
                   << ")of criterion type " << name;
    criterionType->addValuePair(value, literal);
}

void AudioRouteManager::addCriterion(const string &name, const string &criterionTypeName,
                                     const string &defaultLiteralValue /* = "" */)
{
    AutoW lock(mRoutingLock);
    if (mAudioPfwConnector->isStarted()) {
        Log::Warning() << __FUNCTION__ << ": Not allowed while Audio Parameter Manager running";
        return;
    }
    Log::Verbose() << __FUNCTION__ << ": name=" << name << " criterionType=" << criterionTypeName;
    AUDIOCOMMS_ASSERT(mCriteriaMap.find(name) == mCriteriaMap.end(),
                      "Criterion " << name << "of type " << criterionTypeName << " already added");

    // Retrieve criteria Type object
    CriteriaTypeMapIterator it = mCriterionTypesMap.find(criterionTypeName);

    AUDIOCOMMS_ASSERT(it != mCriterionTypesMap.end(),
                      "type " << criterionTypeName << "not found for " << name << " criterion");

    mCriteriaMap[name] = new Criterion(name, it->second, mAudioPfwConnector, defaultLiteralValue);
}

template <typename T>
bool AudioRouteManager::setAudioCriterion(const std::string &name, const T &value)
{
    AutoW lock(mRoutingLock);
    return getElement<Criterion>(name, mCriteriaMap)->setValue<T>(value);
}

bool AudioRouteManager::getAudioCriterion(const std::string &name, std::string &value) const
{
    AutoR lock(mRoutingLock);
    Log::Verbose() << __FUNCTION__ << ": (" << name << ", " << value << ")";
    CriteriaMapConstIterator it = mCriteriaMap.find(name);
    if (it == mCriteriaMap.end()) {
        Log::Warning() << __FUNCTION__ << ": Criterion " << name << " does not exist";
        return false;
    }
    value = it->second->getFormattedValue();
    return true;
}

template <typename T>
bool AudioRouteManager::setAudioParameter(const std::string &paramPath, const T &value)
{
    AutoW lock(mRoutingLock);
    return ParameterMgrHelper::setParameterValue<T>(mAudioPfwConnector, paramPath, value);
}

template <typename T>
bool AudioRouteManager::getAudioParameter(const std::string &paramPath, T &value) const
{
    AutoW lock(mRoutingLock);
    return ParameterMgrHelper::getParameterValue<T>(mAudioPfwConnector, paramPath, value);
}

void AudioRouteManager::commitCriteriaAndApply()
{
    CriteriaMapIterator it;
    for (it = mCriteriaMap.begin(); it != mCriteriaMap.end(); ++it) {

        it->second->setCriterionState();
    }

    mAudioPfwConnector->applyConfigurations();
}

template <typename T>
void AudioRouteManager::addRoute(const string &name,
                                 uint32_t routeId,
                                 const string &portSrc,
                                 const string &portDst,
                                 bool isOut,
                                 map<string, T *> &elementsMap)
{
    AutoW lock(mRoutingLock);
    if (addElement<T>(name, routeId, elementsMap)) {

        T *route = elementsMap[name];
        Log::Debug() << __FUNCTION__ << ": Name=" << name << ", Id=" << routeId
                     << " ports used= " << portSrc << " ," << portDst;
        route->setDirection(isOut);
        if (!portSrc.empty()) {
            route->addPort(findElementByName<AudioPort>(portSrc, mPortMap));
        }
        if (!portDst.empty()) {
            route->addPort(findElementByName<AudioPort>(portDst, mPortMap));
        }
        if (route->isStreamRoute()) {

            // Stream route must also be added in route list as well.
            AUDIOCOMMS_ASSERT(mRouteMap.find(name) == mRouteMap.end(),
                              "Fatal: route " << name << " already added to route list!");
            mRouteMap[name] = route;
        }
    }
}

void AudioRouteManager::addPort(const string &name, uint32_t portId)
{
    AutoW lock(mRoutingLock);
    Log::Debug() << __FUNCTION__ << ": Name=" << name;
    addElement<AudioPort>(name, portId, mPortMap);
}

void AudioRouteManager::addPortGroup(const string &name, int32_t groupId, const string &portMember)
{
    AutoW lock(mRoutingLock);
    if (addElement<AudioPortGroup>(name, groupId, mPortGroupMap)) {
        Log::Debug() << __FUNCTION__ << ": Group=" << name << " PortMember to add=" << portMember;
        AudioPortGroup *portGroup = mPortGroupMap[name];
        AUDIOCOMMS_ASSERT(portGroup != NULL, "Fatal: invalid port group!");

        AudioPort *port = findElementByName<AudioPort>(portMember, mPortMap);
        portGroup->addPortToGroup(port);
    }
}

} // namespace intel_audio
