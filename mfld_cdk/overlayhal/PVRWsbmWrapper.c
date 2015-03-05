/*
 * Copyright (C) 2008 The Android Open Source Project
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
#define LOG_TAG	"Overlay_CDK"

#include <wsbm_pool.h>
#include <wsbm_driver.h>
#include <wsbm_manager.h>
#include <wsbm_util.h>
#include <psb_drm.h>

#include <cutils/log.h>
#include <xf86drm.h>

#include <OverlayHALUtils.h>

struct _WsbmBufferPool * mainPool = NULL;

struct PVRWsbmValidateNode
{
struct  _ValidateNode base;
    struct psb_validate_arg arg;
};

static struct _ValidateNode * pvrAlloc(struct _WsbmVNodeFuncs * func,
                                       int typeId)
{
    LOGV("%s: allocating ...\n", __func__);

    if(typeId == 0) {
        struct PVRWsbmValidateNode * vNode = malloc(sizeof(*vNode));
        if(!vNode) {
            LOGE("%s: allocate memory failed\n", __func__);
            return NULL;
        }

        vNode->base.func = func;
        vNode->base.type_id = 0;
        return &vNode->base;
    } else {
        struct _ValidateNode * node = malloc(sizeof(*node));
        if(!node) {
            LOGE("%s: allocate node failed\n", __func__);
            return NULL;
        }

        node->func = func;
        node->type_id = 1;
        return node;
    }
}

static void pvrFree(struct _ValidateNode * node)
{
    LOGV("%s: free ...\n", __func__);
    if(node->type_id == 0) {
        free(containerOf(node, struct PVRWsbmValidateNode, base));
    } else {
        free(node);
    }
}

static void pvrClear(struct _ValidateNode * node)
{
    LOGV("%s: clearing ...\n", __FUNCTION__);
    if(node->type_id == 0) {
        struct PVRWsbmValidateNode * vNode =
            containerOf(node, struct PVRWsbmValidateNode, base);
        memset(&vNode->arg.d.req, 0, sizeof(vNode->arg.d.req));
    }
}

static struct _WsbmVNodeFuncs vNodeFuncs = {
    .alloc  = pvrAlloc,
    .free   = pvrFree,
    .clear  = pvrClear,
};

int pvrWsbmInitialize(int drmFD)
{
    union drm_psb_extension_arg arg;
    const char drmExt[] = "psb_ttm_placement_alphadrop";
    int ret = 0;

    LOGV("%s: wsbm initializing...\n", __func__);

    if(drmFD <= 0) {
        LOGE("%s: invalid drm fd %d\n", __func__, drmFD);
        return drmFD;
    }

    /*init wsbm*/
    ret = wsbmInit(wsbmNullThreadFuncs(), &vNodeFuncs);
    if (ret) {
        LOGE("%s: WSBM init failed with error code %d\n",
             __func__, ret);
        return ret;
    }

    LOGV("%s: DRM_PSB_EXTENSION %d\n", __func__, DRM_PSB_EXTENSION);

    /*get devOffset via drm IOCTL*/
    strncpy(arg.extension, drmExt, sizeof(drmExt));

    ret = drmCommandWriteRead(drmFD, 6/*DRM_PSB_EXTENSION*/, &arg, sizeof(arg));
    if(ret || !arg.rep.exists) {
        LOGE("%s: get device offset failed with error code %d\n",
             __func__, ret);
        goto out;
    }

    LOGV("%s: ioctl offset 0x%x\n", __func__, arg.rep.driver_ioctl_offset);

    mainPool = wsbmTTMPoolInit(drmFD, arg.rep.driver_ioctl_offset);
    if(!mainPool) {
        LOGE("%s: TTM Pool init failed\n", __func__);
        ret = -EINVAL;
        goto out;
    }

    LOGV("%s: PVRWsbm initialized successfully. mainPool %p\n",
         __func__, mainPool);

    return 0;

out:
    wsbmTakedown();
    return ret;
}

void pvrWsbmTakedown()
{
    LOGV("%s: Takedown wsbm...\n", __func__);

    wsbmPoolTakeDown(mainPool);
    wsbmTakedown();
}

int pvrWsbmAllocateTTMBuffer(uint32_t size, uint32_t align,void ** buf)
{
    struct _WsbmBufferObject * wsbmBuf = NULL;
    int ret = 0;
    int offset = 0;

    LOGV("%s: allocating TTM buffer... size %d\n",
         __func__, align_to(size, 4096));

    if(!buf) {
        LOGE("%s: invalid parameter\n", __func__);
        return -EINVAL;
    }

    LOGV("%s: mainPool %p\n", __func__, mainPool);

    ret = wsbmGenBuffers(mainPool, 1, &wsbmBuf, 0,
                        (WSBM_PL_FLAG_VRAM | WSBM_PL_FLAG_TT |
                        /*WSBM_PL_FLAG_NO_EVICT |*/ WSBM_PL_FLAG_SHARED));
    if(ret) {
        LOGE("%s: wsbmGenBuffers failed with error code %d\n",
             __func__, ret);
        return ret;
    }

    ret = wsbmBOData(wsbmBuf, align_to(size, 4096), NULL, NULL, 0);
    if(ret) {
        LOGE("%s: wsbmBOData failed with error code %d\n",
             __func__, ret);
        /*FIXME: should I unreference this buffer here?*/
        return ret;
    }

    wsbmBOReference(wsbmBuf);

    *buf = wsbmBuf;

    LOGV("%s: ttm buffer allocated. %p\n", __func__, *buf);
    return 0;
}

int pvrWsbmWrapTTMBuffer(uint32_t handle, void **buf)
{
    int ret = 0;
    struct _WsbmBufferObject *wsbmBuf;

    if (!buf) {
        LOGE("%s: Invalid parameter\n", __func__);
        return -EINVAL;
    }

    ret = wsbmGenBuffers(mainPool, 1, &wsbmBuf, 0,
                        (WSBM_PL_FLAG_VRAM | WSBM_PL_FLAG_TT |
                        /*WSBM_PL_FLAG_NO_EVICT |*/ WSBM_PL_FLAG_SHARED));

    if (ret) {
        LOGE("%s: wsbmGenBuffers failed with error code %d\n",
             __func__, ret);
        return ret;
    }

    ret = wsbmBOSetReferenced(wsbmBuf, handle);
    if (ret) {
        LOGE("%s: wsbmBOSetReferenced failed with error code %d\n",
             __func__, ret);

        return ret;
    }

    *buf = (void *)wsbmBuf;

    LOGV("%s: wrap buffer %p for handle 0x%x\n", __func__, wsbmBuf, handle);
    return 0;
}

int pvrWsbmUnReference(void *buf)
{
    struct _WsbmBufferObject *wsbmBuf;

    if (!buf) {
        LOGE("%s: Invalid parameter\n", __func__);
        return -EINVAL;
    }

    wsbmBuf = (struct _WsbmBufferObject *)buf;

    wsbmBOUnreference(&wsbmBuf);

    return 0;
}

int pvrWsbmDestroyTTMBuffer(void * buf)
{
    LOGV("%s: destroying buffer...\n", __func__);

    if(!buf) {
        LOGE("%s: invalid ttm buffer\n", __func__);
        return -EINVAL;
    }

    /*FIXME: should I unmap this buffer object first?*/
    wsbmBOUnmap((struct _WsbmBufferObject *)buf);

    wsbmBOUnreference((struct _WsbmBufferObject **)&buf);

    LOGV("%s: destroyed\n", __func__);

    return 0;
}

void * pvrWsbmGetCPUAddress(void * buf)
{
    if(!buf) {
        LOGE("%s: Invalid ttm buffer\n", __func__);
        return NULL;
    }

    LOGV("%s: getting cpu address. buffer object %p\n", __func__, buf);

    void * address = wsbmBOMap((struct _WsbmBufferObject *)buf,
                                WSBM_ACCESS_READ | WSBM_ACCESS_WRITE);
    if(!address) {
        LOGE("%s: buffer object mapping failed\n", __func__);
        return NULL;
    }

    LOGV("%s: mapped successfully. %p, size %ld\n", __func__,
        address, wsbmBOSize((struct _WsbmBufferObject *)buf));

    return address;
}

uint32_t pvrWsbmGetGttOffset(void * buf)
{
    if(!buf) {
        LOGE("%s: Invalid ttm buffer\n", __func__);
        return 0;
    }

    LOGV("%s: getting gtt offset... buffer object %p\n", __func__, buf);

    uint32_t offset =
        wsbmBOOffsetHint((struct _WsbmBufferObject *)buf) & 0x0fffffff;

    LOGV("%s: successfully. offset %x\n", __func__, offset >> 12);

    return offset >> 12;
}
