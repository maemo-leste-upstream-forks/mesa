/*
 * Copyright (c) Imagination Technologies Ltd.
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include <assert.h>

#include "util/u_atomic.h"
#include <util/xmlconfig.h>
#include <util/driconf.h>

#include "dri_screen.h"

#include "pvrdri.h"
#include "pvrmesa.h"

#define PVR_IMAGE_LOADER_VER_MIN 1

struct PVRBuffer {
   __DRIbuffer sDRIBuffer;
   struct DRISUPBuffer *psDRISUPBuffer;
};

/*************************************************************************//*!
 Local functions
 *//**************************************************************************/

static bool
PVRLoaderIsSupported(__DRIscreen *psDRIScreen)
{
   if (psDRIScreen->image.loader) {
      if (psDRIScreen->image.loader->base.version < PVR_IMAGE_LOADER_VER_MIN) {
         __driUtilMessage("%s: Image loader extension version %d but need %d",
                          __func__, psDRIScreen->image.loader->base.version,
                          PVR_IMAGE_LOADER_VER_MIN);
         return false;
      } else if (!psDRIScreen->image.loader->getBuffers) {
         __driUtilMessage("%s: Image loader extension missing support for getBuffers",
                          __func__);
         return false;
      }
   } else {
      __driUtilMessage("%s: Image loader extension required", __func__);
      return false;
   }

   return true;
}

static inline struct DRISUPContext *
getSharedContextImpl(void *pvSharedContextPrivate)
{
   if (pvSharedContextPrivate == NULL)
      return NULL;

   return ((PVRDRIContext *) pvSharedContextPrivate)->psDRISUPContext;
}

static void
PVRDRIScreenAddReference(PVRDRIScreen *psPVRScreen)
{
   int iRefCount = p_atomic_inc_return(&psPVRScreen->iRefCount);

   (void) iRefCount;
   assert(iRefCount > 1);
}

static void
PVRDRIScreenRemoveReference(PVRDRIScreen *psPVRScreen)
{
   int iRefCount = p_atomic_dec_return(&psPVRScreen->iRefCount);

   assert(iRefCount >= 0);

   if (iRefCount != 0)
      return;

   pvrdri_free_dispatch_tables(psPVRScreen);
   DRISUPDestroyScreen(psPVRScreen->psDRISUPScreen);
   PVRDRICompatDeinit();
   free(psPVRScreen);
}

void
PVRDRIDrawableAddReference(PVRDRIDrawable *psPVRDrawable)
{
   int iRefCount = p_atomic_inc_return(&psPVRDrawable->iRefCount);

   (void) iRefCount;
   assert(iRefCount > 1);
}

void
PVRDRIDrawableRemoveReference(PVRDRIDrawable *psPVRDrawable)
{
   int iRefCount = p_atomic_dec_return(&psPVRDrawable->iRefCount);

   assert(iRefCount >= 0);

   if (iRefCount != 0)
      return;

   DRISUPDestroyDrawable(psPVRDrawable->psDRISUPDrawable);

#if defined(DEBUG)
   p_atomic_dec(&psPVRDrawable->psPVRScreen->iDrawableAlloc);
#endif

   PVRDRIScreenRemoveReference(psPVRDrawable->psPVRScreen);
   free(psPVRDrawable);
}

static void
PVRScreenPrintExtensions(__DRIscreen *psDRIScreen)
{
   /* Don't attempt to print anything if LIBGL_DEBUG isn't in the environment */
   if (getenv("LIBGL_DEBUG") == NULL)
      return;

   if (psDRIScreen->extensions) {
      const __DRIextension *psScreenExtensionVersionInfo =
         PVRDRIScreenExtensionVersionInfo();
      int i;
      int j;

      __driUtilMessage("Supported screen extensions:");

      for (i = 0; psDRIScreen->extensions[i]; i++) {
         for (j = 0; psScreenExtensionVersionInfo[j].name; j++) {
            if (strcmp(psDRIScreen->extensions[i]->name,
                       psScreenExtensionVersionInfo[j].name) == 0) {
               __driUtilMessage("\t%s (supported version: %u - max version: %u)",
                                psDRIScreen->extensions[i]->name,
                                psDRIScreen->extensions[i]->version,
                                psScreenExtensionVersionInfo[j].version);
               break;
            }
         }

         if (psScreenExtensionVersionInfo[j].name == NULL) {
            __driUtilMessage("\t%s (supported version: %u - max version: unknown)",
                             psDRIScreen->extensions[i]->name,
                             psDRIScreen->extensions[i]->version);
         }
      }
   } else {
      __driUtilMessage("No screen extensions found");
   }
}

/*************************************************************************//*!
 Mesa driver API functions
 *//**************************************************************************/

static const __DRIconfig **
PVRDRIInitScreen(__DRIscreen *psDRIScreen)
{
   PVRDRIScreen *psPVRScreen;
   const __DRIconfig **ppsConfigs;
   int iMaxGLES1Version, iMaxGLES2Version;
   const struct PVRDRICallbacksV2 sDRICallbacksV2 = {
      /* Version 0 callbacks */
      .v0.RegisterSupportInterface = MODSUPRegisterSupportInterfaceV2,
      .v0.GetBuffers = MODSUPGetBuffers,
      .v0.CreateConfigs = MODSUPCreateConfigs,
      .v0.ConcatConfigs = MODSUPConcatConfigs,
      .v0.ConfigQuery = MODSUPConfigQuery,
      .v0.LookupEGLImage = MODSUPLookupEGLImage,
      .v0.GetCapability = MODSUPGetCapability,
      /* Version 1 callbacks */
      .v1.FlushFrontBuffer = MODSUPFlushFrontBuffer,
      /* Version 2 callbacks */
      .v2.GetDisplayFD = MODSUPGetDisplayFD,
      /* Version 3 callbacks */
      .v3.DrawableGetReferenceHandle = MODSUPDrawableGetReferenceHandle,
      .v3.DrawableAddReference = MODSUPDrawableAddReference,
      .v3.DrawableRemoveReference = MODSUPDrawableRemoveReference,
      /* Version 4 callbacks */
      .v4.DestroyLoaderImageState = MODSUPDestroyLoaderImageState,
   };

   if (!PVRLoaderIsSupported(psDRIScreen))
      return NULL;

   if (!PVRDRICompatInit(&sDRICallbacksV2, 4, 0))
      return NULL;

   psPVRScreen = calloc(1, sizeof(*psPVRScreen));
   if (psPVRScreen == NULL) {
      __driUtilMessage("%s: Couldn't allocate PVRDRIScreen", __func__);
      goto ErrorCompatDeinit;
   }

   psDRIScreen->driverPrivate = psPVRScreen;
   psPVRScreen->psDRIScreen = psDRIScreen;
   psPVRScreen->iRefCount = 1;

   psPVRScreen->psDRISUPScreen =
      DRISUPCreateScreen(psDRIScreen, psDRIScreen->fd,
                         psDRIScreen->dri2.useInvalidate != NULL,
                         psDRIScreen->loaderPrivate,
                         &ppsConfigs, &iMaxGLES1Version, &iMaxGLES2Version);
   if (!psPVRScreen->psDRISUPScreen)
      goto ErrorScreenFree;

   psDRIScreen->max_gl_es1_version = iMaxGLES1Version;
   psDRIScreen->max_gl_es2_version = iMaxGLES2Version;

   psDRIScreen->max_gl_compat_version =
      DRISUPGetAPIVersion(psPVRScreen->psDRISUPScreen, PVRDRI_API_GL_COMPAT);
   psDRIScreen->max_gl_core_version =
      DRISUPGetAPIVersion(psPVRScreen->psDRISUPScreen, PVRDRI_API_GL_CORE);

   psDRIScreen->extensions = PVRDRIScreenExtensions();

   PVRScreenPrintExtensions(psDRIScreen);

   return ppsConfigs;

ErrorScreenFree:
   psDRIScreen->driverPrivate = NULL;
   free(psPVRScreen);

ErrorCompatDeinit:
   PVRDRICompatDeinit();

   return NULL;
}

static void
PVRDRIDestroyScreen(__DRIscreen *psDRIScreen)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

#if defined(DEBUG)
   if (psPVRScreen->iBufferAlloc != 0 ||
       psPVRScreen->iDrawableAlloc != 0 ||
       psPVRScreen->iContextAlloc != 0) {
      errorMessage("%s: Outstanding allocations: Contexts: %d Drawables: %d Buffers: %d",
                   __func__, psPVRScreen->iContextAlloc,
                   psPVRScreen->iDrawableAlloc, psPVRScreen->iBufferAlloc);

      if (psPVRScreen->iRefCount > 1) {
         errorMessage("%s: PVRDRIScreen resources will not be freed until its %d references are removed",
                      __func__, psPVRScreen->iRefCount - 1);
      }
   }
#endif

   PVRDRIScreenRemoveReference(psPVRScreen);
}

static int
PVRDRIScreenSupportedAPIs(PVRDRIScreen *psPVRScreen)
{
   unsigned int api_mask = psPVRScreen->psDRIScreen->api_mask;
   int supported = 0;

   if ((api_mask & (1 << __DRI_API_GLES)) != 0)
      supported |= PVRDRI_API_BIT_GLES;

   if ((api_mask & (1 << __DRI_API_GLES2)) != 0)
      supported |= PVRDRI_API_BIT_GLES2;

   if ((api_mask & (1 << __DRI_API_GLES3)) != 0)
      supported |= PVRDRI_API_BIT_GLES3;

   if ((api_mask & (1 << __DRI_API_OPENGL)) != 0)
      supported |= PVRDRI_API_BIT_GL;

   if ((api_mask & (1 << __DRI_API_OPENGL_CORE)) != 0)
      supported |= PVRDRI_API_BIT_GL;

   return supported;
}

static GLboolean
PVRDRICreateContext(gl_api eMesaAPI, const struct gl_config *psGLMode,
                    __DRIcontext *psDRIContext,
                    const struct __DriverContextConfig *psCtxConfig,
                    unsigned int *puError, void *pvSharedContextPrivate)
{
   __DRIscreen *psDRIScreen = psDRIContext->driScreenPriv;
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;
   PVRDRIContext *psPVRContext;
   struct DRISUPContext *psDRISUPContext;
   struct DRISUPContext *psDRISUPSharedContext;
   struct PVRDRIContextConfig sCtxConfig;
   PVRDRIAPIType eAPI;

   psDRISUPSharedContext = getSharedContextImpl(pvSharedContextPrivate);

   sCtxConfig.uMajorVersion = psCtxConfig->major_version;
   sCtxConfig.uMinorVersion = psCtxConfig->minor_version;
   sCtxConfig.uFlags = psCtxConfig->flags;
   sCtxConfig.iResetStrategy = __DRI_CTX_RESET_NO_NOTIFICATION;
   sCtxConfig.uPriority = __DRI_CTX_PRIORITY_MEDIUM;
   sCtxConfig.iReleaseBehavior = __DRI_CTX_RELEASE_BEHAVIOR_FLUSH;

   psPVRContext = calloc(1, sizeof(*psPVRContext));
   if (psPVRContext == NULL) {
      __driUtilMessage("%s: Couldn't allocate PVRDRIContext", __func__);
      *puError = __DRI_CTX_ERROR_NO_MEMORY;
      return GL_FALSE;
   }

   psPVRContext->psDRIContext = psDRIContext;
   psPVRContext->psPVRScreen = psPVRScreen;

   if (psGLMode)
      psPVRContext->sConfig.sGLMode = *psGLMode;

   switch (eMesaAPI) {
   case API_OPENGLES:
      eAPI = PVRDRI_API_GLES1;
      break;
   case API_OPENGLES2:
      eAPI = PVRDRI_API_GLES2;
      break;
   case API_OPENGL_COMPAT:
      eAPI = PVRDRI_API_GL_COMPAT;
      break;
   case API_OPENGL_CORE:
      eAPI = PVRDRI_API_GL_CORE;
      break;
   default:
      __driUtilMessage("%s: Unsupported API: %d",
                       __func__, (int) eMesaAPI);
      *puError = __DRI_CTX_ERROR_BAD_API;
      goto ErrorContextFree;
   }
   psPVRContext->eAPI = eAPI;

   if ((psCtxConfig->attribute_mask &
        __DRIVER_CONTEXT_ATTRIB_RESET_STRATEGY) != 0) {
      sCtxConfig.iResetStrategy = psCtxConfig->reset_strategy;
   }

   if ((psCtxConfig->attribute_mask &
        __DRIVER_CONTEXT_ATTRIB_RELEASE_BEHAVIOR) != 0) {
      sCtxConfig.iReleaseBehavior = psCtxConfig->release_behavior;
   }

   if ((psCtxConfig->attribute_mask &
        __DRIVER_CONTEXT_ATTRIB_PRIORITY) != 0) {
      sCtxConfig.uPriority = psCtxConfig->priority;
   }

   *puError = DRISUPCreateContext(eAPI, &psPVRContext->sConfig, &sCtxConfig,
                                  psDRIContext, psDRISUPSharedContext,
                                  psPVRScreen->psDRISUPScreen,
                                  &psDRISUPContext);
   if (*puError != __DRI_CTX_ERROR_SUCCESS)
      goto ErrorContextFree;

   psPVRContext->psDRISUPContext = psDRISUPContext;

   if (!pvrdri_create_dispatch_table(psPVRScreen, eAPI)) {
      __driUtilMessage("%s: Couldn't create dispatch table", __func__);
      *puError = __DRI_CTX_ERROR_BAD_API;
      goto ErrorContextDestroy;
   }
#if defined(DEBUG)
   p_atomic_inc(&psPVRScreen->iContextAlloc);
#endif

   psDRIContext->driverPrivate = (void *) psPVRContext;
   PVRDRIScreenAddReference(psPVRScreen);

   *puError = __DRI_CTX_ERROR_SUCCESS;

   return GL_TRUE;

ErrorContextDestroy:
   DRISUPDestroyContext(psPVRContext->psDRISUPContext);

ErrorContextFree:
   free(psPVRContext);

   return GL_FALSE;
}

static void
PVRDRIDestroyContext(__DRIcontext *psDRIContext)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;
   PVRDRIScreen *psPVRScreen = psPVRContext->psPVRScreen;

   DRISUPDestroyContext(psPVRContext->psDRISUPContext);

#if defined(DEBUG)
   p_atomic_dec(&psPVRScreen->iContextAlloc);
#endif

   PVRDRIScreenRemoveReference(psPVRScreen);
   free(psPVRContext);
}

static GLboolean
PVRDRICreateBuffer(__DRIscreen *psDRIScreen, __DRIdrawable *psDRIDrawable,
                   const struct gl_config *psGLMode, GLboolean bIsPixmap)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;
   PVRDRIDrawable *psPVRDrawable = NULL;

   /* No known callers ever set this to true */
   if (bIsPixmap)
      return GL_FALSE;

   if (!psGLMode) {
      __driUtilMessage("%s: Invalid GL config", __func__);
      return GL_FALSE;
   }

   psPVRDrawable = calloc(1, sizeof(*psPVRDrawable));
   if (!psPVRDrawable) {
      __driUtilMessage("%s: Couldn't allocate PVR drawable", __func__);
      goto ErrorDrawableFree;
   }

   psDRIDrawable->driverPrivate = (void *) psPVRDrawable;
   psPVRDrawable->iRefCount = 1;
   psPVRDrawable->psDRIDrawable = psDRIDrawable;
   psPVRDrawable->psPVRScreen = psPVRScreen;
   psPVRDrawable->sConfig.sGLMode = *psGLMode;
   psPVRDrawable->sConfig.iSupportedAPIs =
      PVRDRIScreenSupportedAPIs(psPVRScreen);

   psPVRDrawable->psDRISUPDrawable =
      DRISUPCreateDrawable(psDRIDrawable, psPVRScreen->psDRISUPScreen,
                           psDRIDrawable->loaderPrivate,
                           &psPVRDrawable->sConfig);
   if (!psPVRDrawable->psDRISUPDrawable) {
      __driUtilMessage("%s: Couldn't create DRI Support drawable",
                       __func__);
      goto ErrorDrawableFree;
   }

   /* Initialisation is completed in MakeCurrent */
#if defined(DEBUG)
   p_atomic_inc(&psPVRScreen->iDrawableAlloc);
#endif
   PVRDRIScreenAddReference(psPVRScreen);
   return GL_TRUE;

ErrorDrawableFree:
   DRISUPDestroyDrawable(psPVRDrawable->psDRISUPDrawable);
   free(psPVRDrawable);
   psDRIDrawable->driverPrivate = NULL;

   return GL_FALSE;
}

static void
PVRDRIDestroyBuffer(__DRIdrawable *psDRIDrawable)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;

   PVRDRIDrawableRemoveReference(psPVRDrawable);
}

static GLboolean
PVRDRIMakeCurrent(__DRIcontext *psDRIContext,
                  __DRIdrawable *psDRIWrite, __DRIdrawable *psDRIRead)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;
   struct DRISUPDrawable *psDRISUPWrite;
   struct DRISUPDrawable *psDRISUPRead;

   if (psDRIWrite) {
      PVRDRIDrawable *psPVRWrite = psDRIWrite->driverPrivate;

      psDRISUPWrite = psPVRWrite->psDRISUPDrawable;
   } else {
      psDRISUPWrite = NULL;
   }

   if (psDRIRead) {
      PVRDRIDrawable *psPVRRead = psDRIRead->driverPrivate;

      psDRISUPRead = psPVRRead->psDRISUPDrawable;
   } else {
      psDRISUPRead = NULL;
   }

   if (!DRISUPMakeCurrent(psPVRContext->psDRISUPContext,
                          psDRISUPWrite, psDRISUPRead))
      goto ErrorUnlock;

   pvrdri_set_dispatch_table(psPVRContext);

   return GL_TRUE;

ErrorUnlock:
   return GL_FALSE;
}

static GLboolean
PVRDRIUnbindContext(__DRIcontext *psDRIContext)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   pvrdri_set_null_dispatch_table();
   DRISUPUnbindContext(psPVRContext->psDRISUPContext);

   return GL_TRUE;
}

static __DRIbuffer *
PVRDRIAllocateBuffer(__DRIscreen *psDRIScreen, unsigned int uAttachment,
                     unsigned int uFormat, int iWidth, int iHeight)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;
   struct PVRBuffer *psBuffer;

   psBuffer = calloc(1, sizeof(*psBuffer));
   if (psBuffer == NULL) {
      __driUtilMessage("%s: Failed to allocate buffer", __func__);
      return NULL;
   }

   psBuffer->psDRISUPBuffer =
      DRISUPAllocateBuffer(psPVRScreen->psDRISUPScreen, uAttachment, uFormat,
                           iWidth, iHeight, &psBuffer->sDRIBuffer.name,
                           &psBuffer->sDRIBuffer.pitch,
                           &psBuffer->sDRIBuffer.cpp,
                           &psBuffer->sDRIBuffer.flags);
   if (!psBuffer->psDRISUPBuffer) {
      __driUtilMessage("%s: Failed to create DRI Support buffer", __func__);
      goto ErrorFreeDRIBuffer;
   }

   psBuffer->sDRIBuffer.attachment = uAttachment;

#if defined(DEBUG)
   p_atomic_inc(&psPVRScreen->iBufferAlloc);
#endif

   return &psBuffer->sDRIBuffer;

ErrorFreeDRIBuffer:
   free(psBuffer);

   return NULL;
}

static void
PVRDRIReleaseBuffer(__DRIscreen *psDRIScreen, __DRIbuffer *psDRIBuffer)
{
   struct PVRBuffer *psPVRBuffer = (struct PVRBuffer *) psDRIBuffer;
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   DRISUPReleaseBuffer(psPVRScreen->psDRISUPScreen,
                       psPVRBuffer->psDRISUPBuffer);

#if defined(DEBUG)
   p_atomic_dec(&psPVRScreen->iBufferAlloc);
#endif

   free(psPVRBuffer);
}

static char *
PVRDRIGetXMLConfigOptions(const char *pszDriverName)
{
   const driOptionDescription asConfigOptions[] =
   {
      DRI_CONF_SECTION_MISCELLANEOUS
      DRI_CONF_OPT_B("pvr_driconf_not_used", true,
                     "The PowerVR driver does not use DRIConf")
      DRI_CONF_SECTION_END
   };

   (void) pszDriverName;

   return driGetOptionsXml(&asConfigOptions[0], ARRAY_SIZE(asConfigOptions));
}

const struct __DriverAPIRec pvr_driver_api = {
   .InitScreen = PVRDRIInitScreen,
   .DestroyScreen = PVRDRIDestroyScreen,
   .CreateContext = PVRDRICreateContext,
   .DestroyContext = PVRDRIDestroyContext,
   .CreateBuffer = PVRDRICreateBuffer,
   .DestroyBuffer = PVRDRIDestroyBuffer,
   .SwapBuffers = NULL,
   .MakeCurrent = PVRDRIMakeCurrent,
   .UnbindContext = PVRDRIUnbindContext,
   .AllocateBuffer = PVRDRIAllocateBuffer,
   .ReleaseBuffer = PVRDRIReleaseBuffer,
};

static const struct __DRIDriverVtableExtensionRec pvr_vtable = {
   .base = {__DRI_DRIVER_VTABLE, 1},
   .vtable = &pvr_driver_api,
};

const __DRIconfigOptionsExtension pvr_config_options = {
   .base = { __DRI_CONFIG_OPTIONS, 2 },
   .getXml = PVRDRIGetXMLConfigOptions,
};

const __DRIextension *pvr_driver_extensions[] = {
   &driCoreExtension.base,
   &driImageDriverExtension.base,
   &pvrDRI2Extension.base,
   &pvr_vtable.base,
   &pvr_config_options.base,
   NULL
};
