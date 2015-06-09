/*
 * Copyright (c) 2012 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Camera_CameraAreas"

#include "CameraAreas.h"
#include "LogHelper.h"

namespace android {

CameraAreas::CameraAreas()
    :
    mAreas()
{
}

CameraAreas::~CameraAreas()
{
}

bool CameraAreas::operator==(const CameraAreas& other) const
{
    LOG1("@%s", __FUNCTION__);

    if (mAreas.size() != other.mAreas.size()) {
        return false;
    }

    for (size_t i = 0; i < mAreas.size(); ++i) {
        if (mAreas.itemAt(i) != other.mAreas.itemAt(i))
            return false;
    }

    return true;
}

CameraAreas::CameraAreas(const CameraAreas& other)
    :
    mAreas(other.mAreas)
{
}

CameraAreas& CameraAreas::operator=(const CameraAreas& other)
{
    if (this != &other) {
        mAreas = other.mAreas;
    }
    return *this;
}


bool CameraAreas::operator!=(const CameraAreas& other) const
{
    LOG1("@%s", __FUNCTION__);

    return !(*this == other);
}

status_t CameraAreas::scan(const char* stringArea, int maxSize)
{
    LOG1("@%s", __FUNCTION__);

    if (maxSize <= 0) {
        return NO_ERROR;
    }

    if (stringArea == NULL) {
        ALOGE("No areas string");
        return BAD_VALUE;
    }

    LOG1("Scanning areas from params: %s", stringArea);

    if (strcmp ("(0,0,0,0,0)", stringArea) == 0) {
        mAreas.clear();
        LOG1("Scanning areas success (No areas)");
        return NO_ERROR;
   }

    Vector<CameraAreaItem> newAreas;
    CameraAreaItem newItem;

    const char *argTail = stringArea;
    int areaCount = 0;

    if (newAreas.setCapacity(maxSize) == NO_MEMORY) {
        ALOGE("Memory allocation failed");
        return NO_MEMORY;
    }

    while (argTail && areaCount < maxSize) {
        // String format: "(topleftx,toplefty,bottomrightx,bottomrighty,weight),(...)"
        int i = sscanf(argTail, "(%d,%d,%d,%d,%d)",
                       &newItem.x_left,
                       &newItem.y_top,
                       &newItem.x_right,
                       &newItem.y_bottom,
                       &newItem.weight);
        if (i != 5) {
            ALOGE("bad window format");
            return BAD_VALUE;
        }

        LOG1("\tArea %d (%d,%d,%d,%d,%d)",
            areaCount,
            newItem.x_left,
            newItem.y_top,
            newItem.x_right,
            newItem.y_bottom,
            newItem.weight);

        if (!isValidArea(newItem)) {
            ALOGE("bad area %d (%d,%d,%d,%d,%d)",
                 areaCount,
                 newItem.x_left,
                 newItem.y_top,
                 newItem.x_right,
                 newItem.y_bottom,
                 newItem.weight);
            return BAD_VALUE;
        }

        areaCount++;
        newAreas.push(newItem);
        // Not validate separator between windows, should?
        argTail = strchr(argTail + 1, '(');
    }
    if (argTail) {
        ALOGE("Too many areas or bad format");
        return BAD_VALUE;
    }

    mAreas.clear();
    mAreas = newAreas;

    LOG1("Scanning areas success (%d areas)", mAreas.size());

    return NO_ERROR;
}

bool CameraAreas::isEmpty() const
{
    LOG1("@%s: mAreas.isEmpty(): %d", __FUNCTION__, mAreas.isEmpty());
    return mAreas.isEmpty();
}

int CameraAreas::numOfAreas() const
{
    return mAreas.size();
}

void CameraAreas::toWindows(CameraWindow *windows) const
{
    LOG1("@%s", __FUNCTION__);

    for (size_t i = 0; i < mAreas.size(); ++i) {
        windows[i].y_top = mAreas.itemAt(i).y_top ;
        windows[i]. y_bottom = mAreas.itemAt(i).y_bottom;
        windows[i].x_right = mAreas.itemAt(i).x_right ;
        windows[i].x_left = mAreas.itemAt(i).x_left;
        windows[i].weight = mAreas.itemAt(i).weight;
    }
}

bool CameraAreas::isValidArea(const struct CameraAreaItem& area) const
{
    if (area.x_right <= area.x_left ||
        area.y_bottom <= area.y_top)
        return false;
    if (area.y_top < -1000)
        return false;
    if (area.y_bottom > 1000)
        return false;
    if (area.x_right > 1000)
        return false;
    if (area.x_left < -1000)
        return false;
    if ( (area.weight < 1) || (area.weight > 1000) )
        return false;
    return true;
}


// private types' function definitions
bool
CameraAreas::CameraAreaItem::operator==(const CameraAreaItem& other) const
{
    if (y_top != other.y_top)
        return false;
    if (y_bottom != other.y_bottom)
        return false;
    if (x_right != other.x_right)
        return false;
    if (x_left != other.x_left)
        return false;
    if (weight != other.weight)
        return false;
    return true;
}

bool
CameraAreas::CameraAreaItem::operator!=(const CameraAreaItem& other) const
{
    return !(*this == other);
}

} // namespace android
