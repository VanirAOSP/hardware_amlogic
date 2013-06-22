/*
 * Copyright (C) 2011 The Android Open Source Project
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

#define LOG_NDEBUG 0
#define LOG_TAG "CameraProperties"

//#include "CameraHal.h"
#include <utils/threads.h>

#include "DebugUtils.h"
#include "CameraProperties.h"

#define CAMERA_ROOT         "CameraRoot"
#define CAMERA_INSTANCE     "CameraInstance"

namespace android {

/*********************************************************
 CameraProperties - public function implemetation
**********************************************************/

CameraProperties::CameraProperties() : mCamerasSupported(0)
{
    LOG_FUNCTION_NAME;

    mCamerasSupported = 0;
    mInitialized = 0;

    LOG_FUNCTION_NAME_EXIT;
}

CameraProperties::~CameraProperties()
{
    LOG_FUNCTION_NAME;

    LOG_FUNCTION_NAME_EXIT;
}


// Initializes the CameraProperties class
status_t CameraProperties::initialize()
{
    LOG_FUNCTION_NAME;

    status_t ret;

    Mutex::Autolock lock(mLock);

    if(mInitialized)
        return NO_ERROR;

#ifdef AMLOGIC_USB_CAMERA_SUPPORT
    mCamerasSupported = 0;
    ret = loadProperties();
    mInitialized = 0;
#else
    ret = loadProperties();
    mInitialized = 1;
#endif

    LOG_FUNCTION_NAME_EXIT;

    return ret;
}

extern "C" int CameraAdapter_Capabilities(CameraProperties::Properties* properties_array,
                                          const unsigned int starting_camera,
                                          const unsigned int camera_num);
extern "C" int CameraAdapter_CameraNum();

///Loads all the Camera related properties
status_t CameraProperties::loadProperties()
{
    LOG_FUNCTION_NAME;

    status_t ret = NO_ERROR;

    // adapter updates capabilities and we update camera count
    mCamerasSupported = CameraAdapter_Capabilities(mCameraProps, mCamerasSupported, CameraAdapter_CameraNum());

    if((int)mCamerasSupported < 0) {
        LOGE("error while getting capabilities");
        ret = UNKNOWN_ERROR;
    } else if (mCamerasSupported > MAX_CAMERAS_SUPPORTED) {
        LOGE("returned too many adapaters");
        ret = UNKNOWN_ERROR;
    } else {
        LOGE("num_cameras = %d", mCamerasSupported);

        for (unsigned int i = 0; i < mCamerasSupported; i++) {
            mCameraProps[i].set(CAMERA_SENSOR_INDEX, i);
            mCameraProps[i].dump();
        }
    }

    LOGV("mCamerasSupported = %d", mCamerasSupported);
    LOG_FUNCTION_NAME_EXIT;
    return ret;
}

// Returns the number of Cameras found
int CameraProperties::camerasSupported()
{
    LOG_FUNCTION_NAME;
    return mCamerasSupported;
}

};
