/*
 * Copyright 2012 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * FaceDetection Interface for native android.
 *
 * @file CameraFaceDetection.h
 * @version 1.03
 * @date 2012-MAR-26
 * @brief APIs for Face detection, combined with eye detection and mouth detection
 * @author Olaworks Inc. ( kyom@olaworks.com, cbheo@olaworks.com )
 */

#ifndef _CAMERAFACEDETECTION_H_
#define _CAMERAFACEDETECTION_H_

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * declaration of image orientation.
 */
enum {
    IMAGE_ORIENTATION_NORMAL = 0, /**< Normal case                        */
    IMAGE_ORIENTATION_CW90, /**<  90-degree clockwise rotated        */
    IMAGE_ORIENTATION_CW180, /**<  180-degree clockwise rotated       */
    IMAGE_ORIENTATION_CW270
/**<  90-degree counterclockwise rotated */
};

/** 
 * definition of parameter keys
 */
#define PARAM_STRIDE  0x01 /**<  the stride of input image        */
#define PARAM_IMAGEFORMAT 0x02 /**<  the format of input image, default is YUV420SP        */
#define PARAM_ORIENTATION 0x03 /**<  the orientation of input image, may be one of IMAGE_ORIENTATION_XXXX      */
#define PARAM_FACESIZE_MIN 0x04 /**<  the mininum size of detectable face. This should be bigger than 22 at least.         */
#define PARAM_FACESIZE_MAX 0x05 /**<  the maximum size of detectable face. Not supported yet.        */
#define PARAM_LIMIT_NFACE   0x06    /**<  the limit of number of faces detectable concurrently. Support up to 32 faces.         */
#define PARAM_ENABLE_EYEDETECTION   0x07    /**<  the flag for Eyedetection, if true, it would find eyes for each faces found. Please make sure that ED requires more computational resources. Default is false. */

/* FORWARDING DECLARATION from system/core/include/Camera.h */
/**
 * The information of a face from camera face detection.
 * Please note that this declaration is forwarded from system/core/include/Camera.h of ICS source tree
 *
 *  @brief the face information structure.
 */
typedef struct {
    /**
     * Bounds of the face [left, top, right, bottom]. (-1000, -1000) represents
     * the top-left of the camera field of view, and (1000, 1000) represents the
     * bottom-right of the field of view. The width and height cannot be 0 or
     * negative. This is supported by both hardware and software face detection.
     *
     * The direction is relative to the sensor orientation, that is, what the
     * sensor sees. The direction is not affected by the rotation or mirroring
     * of CAMERA_CMD_SET_DISPLAY_ORIENTATION.
     */
    int rect[4];

    /**
     * The confidence level of the face. The range is 1 to 100. 100 is the
     * highest confidence. This is supported by both hardware and software
     * face detection.
     */
    int score;

    /**
     * An unique id per face while the face is visible to the tracker. If
     * the face leaves the field-of-view and comes back, it will get a new
     * id. If the value is 0, id is not supported.
     */
    int id;

    /**
     * The coordinates of the center of the left eye. The range is -1000 to
     * 1000. -2000, -2000 if this is not supported.
     */
    int left_eye[2];

    /**
     * The coordinates of the center of the right eye. The range is -1000 to
     * 1000. -2000, -2000 if this is not supported.
     */
    int right_eye[2];

    /**
     * The coordinates of the center of the mouth. The range is -1000 to 1000.
     * -2000, -2000 if this is not supported.
     */
    int mouth[2];

} face_info_t;

/* end of fowarding declaration */

/**
 * the maximum number of faces detectable at the same time. It is 32.
 */

#define MAX_DETECTABLE (32)

/**
 *  @struct CameraFaceDetection CameraFaceDetection.h "CameraFaceDetection.h"
 *  @brief the CameraFaceDetection structure. 
 */
struct CameraFaceDetection {
    int numDetected; /**< the number of detected faces. */
    face_info_t detectedFaces[MAX_DETECTABLE]; /**< detailed information for each face */
};
typedef struct CameraFaceDetection CameraFaceDetection;

/**
 * @brief Create Face detector object and return error code.
 * @param pCfd [IN/OUT] : the reference of handle pointer.
 * @return 0 if success, otherwise negative.
 */
int CameraFaceDetection_Create(CameraFaceDetection** pCfd);

/**
 * @brief Detect faces, find eye/mouth of each face. The result will be in the struct.
 * @note This is only for streaming preview frames, not for still cut image.
 * This will process image by gradually, so some faces may not be detected at the first call.
 *
 *  @param pCfd [IN] : the handle created by CameraFaceDetection_Create. Must not be null.
 *  @param imageData [IN] : the image buffer formatted by YUV420NV21. Must not be null.
 *  @param width [IN] : the image width.
 *  @param height [IN] : the image height.
 *
 *  @return the number of detected faces on success. This should be the same as CameraFaceDetection::numDetected. Otherwise negatives.
 */
int CameraFaceDetection_FindFace(CameraFaceDetection* pCfd,
        unsigned char* imageData, int width, int height);

/**
 * @brief Set various parameters. Keys are defined as PARAM_XXX.
 *
 * @param pCfd [IN] : the reference of handle pointer.
 * @param key [IN] : the key which decides what should be set.
 * @param value [IN] : the value according to key.
 *
 * @return 0 if success, otherwise negative.
 */
int CameraFaceDetection_SetParam(CameraFaceDetection* pCfd, int key, int value);

/**
 * @brief Destroy Face detector object and make the pointer NULL.
 *
 * @param pCfd [IN/OUT] : the reference of handle pointer.
 *
 * @return 0 if success, otherwise negative.
 */
int CameraFaceDetection_Destroy(CameraFaceDetection** pCfd);

#ifdef __cplusplus
}
#endif

#endif
