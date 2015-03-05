/*
 * wrs_core.cpp, Wind River OpenMax-IL Core
 *
 * Copyright (c) 2009-2010 Wind River Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <OMX_Core.h>
#include <OMX_Component.h>

#include <list.h>
#include <cmodule.h>
#include <componentbase.h>

#define NUM_COMPONENTS 3
typedef struct component_handle {

    char comp_name[OMX_MAX_STRINGNAME_SIZE];
    void * comp_handle;
    char parser_name[OMX_MAX_STRINGNAME_SIZE];
    void * parser_handle;

}ComponentHandle, *ComponentHandlePtr;

static unsigned int g_initialized = 0;
static unsigned int g_nr_instances = 0;
static struct list *preload_list=NULL;

static struct list *g_module_list = NULL;
static pthread_mutex_t g_module_lock = PTHREAD_MUTEX_INITIALIZER;

static char *omx_components[NUM_COMPONENTS][2] = {
    {"libOMXVideoDecoderAVC.so", "libmixvbp-h264.so"},
    {"libOMXVideoEncoderAVC.so", NULL},
    {NULL,NULL}
};

extern "C" bool preload_components(void)
{
    /* this function is called by openMAX-IL client when libraries are to be
     * loaded at initial stages.  Security features of the operating system require
     * to load libraries upfront */
    ComponentHandlePtr component_handle;
    int index;
    bool ret = true;

    for (index=0;omx_components[index][0];index++) {
        struct list *entry;
        component_handle = (ComponentHandlePtr) malloc(sizeof(ComponentHandle));

        strncpy(component_handle->comp_name, omx_components[index][0], OMX_MAX_STRINGNAME_SIZE);
        /* parser is not always needed */
        if (omx_components[index][1])
            strncpy(component_handle->parser_name, omx_components[index][1], OMX_MAX_STRINGNAME_SIZE);

        /* skip libraries starting with # */
        if (component_handle->comp_name[0] == '#')
            continue;

        component_handle->comp_handle = dlopen(component_handle->comp_name, RTLD_NOW);
        if (!component_handle->comp_handle) {
            ret=false;
            goto delete_comphandle;
        }


        if (component_handle->parser_name) {
            /* some components don't need a parser */
            component_handle->parser_handle = dlopen(component_handle->parser_name, RTLD_NOW);
            if (!component_handle->parser_handle) {
                ret=false;
                goto delete_comphandle;
            }
        }

        entry = list_alloc(component_handle);
        if (!entry) {
            ret=false;
            goto unload_comphandle;
        }
        preload_list = __list_add_tail(preload_list, entry);

	omx_infoLog("open component %s and parser %s", component_handle->comp_name,
			component_handle->parser_name == NULL ?
			"No parser provided":component_handle->parser_name );
	continue;

unload_comphandle:
        dlclose(component_handle->comp_handle);
delete_comphandle:
        free(component_handle);
    }
    return ret;
}

static struct list *construct_components(const char *config_file_name)
{
    FILE *config_file;
    char config_file_path[256];
    int index=0;
    struct list *head = NULL, *preload_entry;
    ComponentHandlePtr component_handle;

    list_foreach(preload_list, preload_entry) {
        CModule *cmodule;
        struct list *entry;
        OMX_ERRORTYPE ret;

        component_handle = (ComponentHandlePtr) preload_entry->data;

        /* skip libraries starting with # */
        if (component_handle->comp_name[0] == '#')
            continue;

        cmodule = new CModule(component_handle->comp_name);
        if (!cmodule)
            continue;

        omx_verboseLog("found component library %s", component_handle->comp_name);

        ret = cmodule->Load(MODULE_NOW, component_handle->comp_handle);
        if (ret != OMX_ErrorNone)
            goto delete_cmodule;

        ret = cmodule->SetParser(component_handle->parser_handle);
        if (ret != OMX_ErrorNone)
            goto delete_cmodule;

        ret = cmodule->QueryComponentNameAndRoles();
        if (ret != OMX_ErrorNone)
            goto unload_cmodule;

        entry = list_alloc(cmodule);
        if (!entry)
            goto unload_cmodule;
        head = __list_add_tail(head, entry);

        // cmodule->Unload();
        omx_verboseLog("module %s:%s added to component list",
             cmodule->GetLibraryName(), cmodule->GetComponentName());

        continue;

    unload_cmodule:
        cmodule->Unload();
    delete_cmodule:
        delete cmodule;
    }

    return head;
}

static struct list *destruct_components(struct list *head)
{
    struct list *entry, *next;
    ComponentHandlePtr component_handle;

    list_foreach_safe(head, entry, next) {
        CModule *cmodule = static_cast<CModule *>(entry->data);

        head = __list_delete(head, entry);
        delete cmodule;
    }

    return head;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Init(void)
{
    int ret;

    omx_verboseLog("%s(): enter", __FUNCTION__);

    pthread_mutex_lock(&g_module_lock);
    if (!g_initialized) {
        g_module_list = construct_components("wrs_omxil_components.list");
        if (!g_module_list) {
            pthread_mutex_unlock(&g_module_lock);
            omx_errorLog("%s(): exit failure, construct_components failed",
                 __FUNCTION__);
            return OMX_ErrorInsufficientResources;
        }

        g_initialized = 1;
    }
    pthread_mutex_unlock(&g_module_lock);

    omx_verboseLog("%s(): exit done", __FUNCTION__);
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    omx_verboseLog("%s(): enter", __FUNCTION__);

    pthread_mutex_lock(&g_module_lock);
    if (!g_nr_instances) {
        g_module_list = destruct_components(g_module_list);
        g_initialized = 0;
    } else
        ret = OMX_ErrorUndefined;
    pthread_mutex_unlock(&g_module_lock);

    omx_verboseLog("%s(): exit done (ret : 0x%08x)", __FUNCTION__, ret);
    return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(
    OMX_OUT OMX_STRING cComponentName,
    OMX_IN OMX_U32 nNameLength,
    OMX_IN OMX_U32 nIndex)
{
    CModule *cmodule;
    ComponentBase *cbase;
    struct list *entry;
    OMX_STRING cname;

    pthread_mutex_lock(&g_module_lock);
    entry = __list_entry(g_module_list, nIndex);
    if (!entry) {
        pthread_mutex_unlock(&g_module_lock);
        return OMX_ErrorNoMore;
    }
    pthread_mutex_unlock(&g_module_lock);

    cmodule = static_cast<CModule *>(entry->data);
    cname = cmodule->GetComponentName();

    strncpy(cComponentName, cname, nNameLength);

    omx_verboseLog("%s(): found %luth component %s", __FUNCTION__, nIndex, cname);
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
    OMX_OUT OMX_HANDLETYPE* pHandle,
    OMX_IN OMX_STRING cComponentName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE *pCallBacks)
{
    struct list *entry;
    OMX_ERRORTYPE ret;

    omx_verboseLog("%s(): enter, try to get %s", __FUNCTION__, cComponentName);

    pthread_mutex_lock(&g_module_lock);
    list_foreach(g_module_list, entry) {
        CModule *cmodule;
        OMX_STRING cname;

        cmodule = static_cast<CModule *>(entry->data);

        cname = cmodule->GetComponentName();
        if (!strcmp(cComponentName, cname)) {
            ComponentBase *cbase = NULL;

            ret = cmodule->InstantiateComponent(&cbase);
            if (ret != OMX_ErrorNone){
                omx_errorLog("%s(): exit failure, cmodule->Instantiate failed\n",
                     __FUNCTION__);
                goto unload_cmodule;
            }

            ret = cbase->GetHandle(pHandle, pAppData, pCallBacks);
            if (ret != OMX_ErrorNone) {
                omx_errorLog("%s(): exit failure, cbase->GetHandle failed\n",
                     __FUNCTION__);
                goto delete_cbase;
            }

            cbase->SetCModule(cmodule);

            g_nr_instances++;
            pthread_mutex_unlock(&g_module_lock);

            omx_infoLog("get handle of component %s successfully", cComponentName);
            omx_verboseLog("%s(): exit done\n", __FUNCTION__);
            return OMX_ErrorNone;

        delete_cbase:
            delete cbase;
        unload_cmodule:
            cmodule->Unload();
        unlock_list:
            pthread_mutex_unlock(&g_module_lock);

            omx_errorLog("%s(): exit failure, (ret : 0x%08x)\n", __FUNCTION__, ret);
            return ret;
        }
    }
    pthread_mutex_unlock(&g_module_lock);

    omx_errorLog("%s(): exit failure, %s not found", __FUNCTION__, cComponentName);
    return OMX_ErrorInvalidComponent;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
    OMX_IN OMX_HANDLETYPE hComponent)
{
    ComponentBase *cbase;
    CModule *cmodule;
    OMX_ERRORTYPE ret;
    char cname[OMX_MAX_STRINGNAME_SIZE];

    if (!hComponent)
        return OMX_ErrorBadParameter;

    cbase = static_cast<ComponentBase *>
        (((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (!cbase) {
        omx_errorLog("%s(): exit failure, cannot find cbase pointer\n",
             __FUNCTION__);
        return OMX_ErrorBadParameter;
    }
    strcpy(&cname[0], cbase->GetName());

    omx_verboseLog("%s(): enter, try to free %s", __FUNCTION__, cbase->GetName());


    ret = cbase->FreeHandle(hComponent);
    if (ret != OMX_ErrorNone) {
        omx_errorLog("%s(): exit failure, cbase->FreeHandle() failed (ret = 0x%08x)\n",
             __FUNCTION__, ret);
        return ret;
    }

    pthread_mutex_lock(&g_module_lock);
    g_nr_instances--;
    pthread_mutex_unlock(&g_module_lock);

    cmodule = cbase->GetCModule();
    if (!cmodule)
        omx_errorLog("fatal error, %s does not have cmodule\n", cbase->GetName());


    if (cmodule && !preload_list)
        cmodule->Unload();

    delete cbase;

    omx_infoLog("free handle of component %s successfully", cname);
    omx_verboseLog("%s(): exit done", __FUNCTION__);
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(
    OMX_IN OMX_HANDLETYPE hOutput,
    OMX_IN OMX_U32 nPortOutput,
    OMX_IN OMX_HANDLETYPE hInput,
    OMX_IN OMX_U32 nPortInput)
{
    return OMX_ErrorNotImplemented;
}

OMX_API OMX_ERRORTYPE   OMX_GetContentPipe(
    OMX_OUT OMX_HANDLETYPE *hPipe,
    OMX_IN OMX_STRING szURI)
{
    return OMX_ErrorNotImplemented;
}

OMX_API OMX_ERRORTYPE OMX_GetComponentsOfRole (
    OMX_IN OMX_STRING role,
    OMX_INOUT OMX_U32 *pNumComps,
    OMX_INOUT OMX_U8  **compNames)
{
    struct list *entry;
    OMX_U32 nr_comps = 0, copied_nr_comps = 0;

    pthread_mutex_lock(&g_module_lock);
    list_foreach(g_module_list, entry) {
        CModule *cmodule;
        OMX_STRING cname;
        bool having_role;

        cmodule = static_cast<CModule *>(entry->data);

        having_role = cmodule->QueryHavingThisRole(role);
        if (having_role) {
            if (compNames && compNames[nr_comps]) {
                cname = cmodule->GetComponentName();
                strncpy((OMX_STRING)&compNames[nr_comps][0], cname,
                        OMX_MAX_STRINGNAME_SIZE);
                copied_nr_comps++;
                omx_verboseLog("%s(): component %s has %s role", __FUNCTION__,
                     cname, role);
            }
            nr_comps++;
        }
    }
    pthread_mutex_unlock(&g_module_lock);

    if (!copied_nr_comps)
        *pNumComps = nr_comps;
    else {
        if (copied_nr_comps != *pNumComps)
            return OMX_ErrorBadParameter;
    }

    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_GetRolesOfComponent (
    OMX_IN OMX_STRING compName,
    OMX_INOUT OMX_U32 *pNumRoles,
    OMX_OUT OMX_U8 **roles)
{
    struct list *entry;

    pthread_mutex_lock(&g_module_lock);
    list_foreach(g_module_list, entry) {
        CModule *cmodule;
        ComponentBase *cbase;
        OMX_STRING cname;
        OMX_ERRORTYPE ret;

        cmodule = static_cast<CModule *>(entry->data);

        cname = cmodule->GetComponentName();
        if (!strcmp(compName, cname)) {
            pthread_mutex_unlock(&g_module_lock);
#if LOG_NDEBUG
            return cmodule->GetComponentRoles(pNumRoles, roles);
#else
            ret = cmodule->GetComponentRoles(pNumRoles, roles);
            if (ret != OMX_ErrorNone) {
                OMX_U32 i;

                for (i = 0; i < *pNumRoles; i++) {
                    omx_verboseLog("%s(): component %s has %s role", __FUNCTION__,
                         compName, &roles[i][0]);
                }
            }
            return ret;
#endif
        }
    }
    pthread_mutex_unlock(&g_module_lock);

    return OMX_ErrorInvalidComponent;
}
