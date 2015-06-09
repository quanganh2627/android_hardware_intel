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

#include "Parameter.hpp"

class CriterionType;
class Criterion;
class CParameterMgrPlatformConnector;

namespace intel_audio
{

class IStreamInterface;

/**
 * This class intends to address the Android Parameters that are associated to a criterion.
 * Each time the key of these android parameters is detected, this class will wrap the accessor
 * of the android parameter to accessors of the PFW criterion.
 */
class CriterionParameter : public Parameter
{
public:

    CriterionParameter(ParameterChangedObserver *observer,
                       const std::string &key,
                       const std::string &name,
                       const std::string &defaultValue)
        : Parameter(observer, key, name, defaultValue)
    {}

    virtual bool set(const std::string &androidParamValue);
};

/**
 * This class intends to address the Android Parameters that are associated to a criterion
 * of the Route PFW Instance
 * Each time the key of these android parameters is detected, this class will wrap the accessor
 * of the android parameter to accessors of the Route PFW criterion.
 */
class RouteCriterionParameter : public CriterionParameter
{
public:
    RouteCriterionParameter(ParameterChangedObserver *observer,
                            const std::string &key,
                            const std::string &name,
                            CriterionType *criterionType,
                            CParameterMgrPlatformConnector *connector,
                            const std::string &defaultValue = "");

    virtual ~RouteCriterionParameter();

    virtual bool setValue(const std::string &value);

    virtual bool getValue(std::string &value) const;

    /**
     * No need to sync route criterion parameter, as the default value is set at construction
     * time.
     *
     * @return always true.
     */
    virtual bool sync() { return true; }

    Criterion *getCriterion() { return mCriterion; }

private:
    Criterion *mCriterion; /**< Associated Route PFW criterion to the android parameter. */
};

/**
 * This class intends to address the Android Parameters that are associated to a criterion
 * of the Audio PFW Instance
 * Each time the key of this android parameters is detected, this class will wrap the accessor
 * of the android parameter to accessors of the Audio PFW criterion.
 */
class AudioCriterionParameter : public CriterionParameter
{
public:
    AudioCriterionParameter(ParameterChangedObserver *observer,
                            const std::string &key,
                            const std::string &name,
                            const std::string &typeName,
                            IStreamInterface *streamInterface,
                            const std::string &defaultValue = "");

    virtual bool setValue(const std::string &value);

    virtual bool getValue(std::string &value) const;

    virtual bool sync();

private:
    IStreamInterface *mStreamInterface; /**< Handle on stream interface of Route Manager. */
};

} // namespace intel_audio
