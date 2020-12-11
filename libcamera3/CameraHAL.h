/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
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

#ifndef CAMERA_HAL_H_
#define CAMERA_HAL_H_

#include <cutils/bitops.h>
#include <hardware/hardware.h>
#include <hardware/camera_common.h>
#include <hardware_legacy/uevent.h>
#include <system/camera_vendor_tags.h>
#include "Camera.h"
#include "VendorTags.h"
#include "utils/CameraConfigurationParser.h"

#define UVC_NAME "uvc"
#define OV5640_SENSOR_NAME_V1 "ov5640"
// The sensor name ov5640 on imx8mq/imx8mm(/sys/class/video4linux/videoX/name) is mx6s-csi
#define OV5640_SENSOR_NAME_V2 "mx6s-csi"
#define OV5640_SENSOR_NAME_V3 "mxc-isi-cap"
#define ISP_SENSOR_NAME "viv_v4l2_device"

#define BACK_CAMERA_NAME "back"
#define FRONT_CAMERA_NAME "front"

using namespace cameraconfigparser;

struct nodeSet {
    char nodeName[CAMERA_SENSOR_LENGTH];
    char devNode[CAMERA_SENSOR_LENGTH];
    char busInfo[CAMERA_SENSOR_LENGTH];
    bool isHeld;
    nodeSet* next;
};

// CameraHAL contains all module state that isn't specific to an individual
// camera device.
class CameraHAL
{
public:
    CameraHAL();
    ~CameraHAL();

    // Camera Module Interface (see <hardware/camera_common.h>)
    int getNumberOfCameras();
    int getCameraInfo(int camera_id, struct camera_info *info);
    int setCallbacks(const camera_module_callbacks_t *callbacks);
    void getVendorTagOps(vendor_tag_ops_t* ops);
    CameraConfigurationParser mCameraCfgParser;
    CameraDefinition mCameraDef;

    // Hardware Module Interface (see <hardware/hardware.h>)
    int openDev(const hw_module_t* mod, const char* name, hw_device_t** dev);

private:
    int32_t matchDevNodes();
    int32_t getNodeName(const char* devNode, char name[], size_t length, char busInfo[], size_t busInfoLen);
    int32_t matchNodeName(const char* nodeName, nodeSet* nodes, int32_t index);
    int32_t matchPropertyName(nodeSet* nodes, int32_t index);

    void enumSensorSet();
    void enumSensorNode(int index);

private:
    SensorSet mSets[MAX_CAMERAS];
    // Number of cameras
    int32_t mCameraCount;
    // Callback handle
    const camera_module_callbacks_t *mCallbacks;
    // Array of camera devices, contains mCameraCount device pointers
    Camera **mCameras;
};

#endif // CAMERA_HAL_H_
