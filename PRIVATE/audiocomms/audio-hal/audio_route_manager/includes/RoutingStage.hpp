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
#pragma once

namespace intel_audio
{

/**
 * Routing stage bits description.
 * It is used to feed the criteria of the Parameter Framework.
 * This criterion is inclusive and is used to encode
 * 5 steps of the routing (Muting, Disabling, Configuring, Enabling, Unmuting)
 * Muting -> EFlow
 * Disabling -> EPath
 * Configuring -> EConfigure
 * Enabling -> EConfigure | EPath
 * Unmuting -> EConfigure | EPath | EFlow
 */
enum RoutingStage
{
    Flow = 0,   /**< It refers to umute/unmute steps.   */
    Path,       /**< It refers to enable/disable steps  */
    Configure   /**< It refers to configure step        */
};

enum RoutingStageMask
{
    FlowMask = (1 << Flow),       /**< It refers to umute/unmute steps.   */
    PathMask = (1 << Path),       /**< It refers to enable/disable steps  */
    ConfigureMask = (1 << Configure)   /**< It refers to configure step        */
};

static const uint32_t gNbRoutingStages = 3;

} // namespace intel_audio
