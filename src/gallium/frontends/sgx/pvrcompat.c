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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <assert.h>

#include <drm_fourcc.h>

#include "pvrdri.h"

#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif

#define _MAKESTRING(x) # x
#define MAKESTRING(x) _MAKESTRING(x)

#define PVRDRI_SUPPORT_LIB "libpvr_dri_support.so"

static void *gpvSupLib;
static int giSupLibRef;

static struct SGXDRISupportInterfaceV0 gsSupV0;

static pthread_mutex_t gsCompatLock = PTHREAD_MUTEX_INITIALIZER;

/* Lookup a function, and set the pointer to the function address */
#define LookupFunc(func, ptr)                   \
   do {                                         \
      ptr = dlsym(gpvSupLib, MAKESTRING(func)); \
   } while(0)

/* Check if a function exists in the DRI Support interface structure */
#define HaveFuncV0(field)                       \
      ((gsSupV0.field) != NULL)                 \

/* Call a function via the DRI Support interface structure */
#define CallFuncV0(field, ...)                  \
   do {                                         \
      if (gsSupV0.field)                        \
         return gsSupV0.field(__VA_ARGS__);     \
   } while(0)

/* Calculate the start of the SGXDRISupportInterfaceV0 structure */
#define SGXDRIInterfaceV0Start(field)                   \
   (offsetof(struct SGXDRISupportInterfaceV0, field))

/* Calculate the end of the SGXDRISupportInterfaceV0 structure */
#define SGXDRIInterfaceV0End(field)                             \
   (offsetof(struct SGXDRISupportInterfaceV0, field) +          \
    sizeof((struct SGXDRISupportInterfaceV0 *)0)->field)

static void
CompatLock(void)
{
   int ret;

   ret = pthread_mutex_lock(&gsCompatLock);
   if (ret) {
      errorMessage("%s: Failed to lock mutex (%d)", __func__, ret);
      abort();
   }
}

static void
CompatUnlock(void)
{
   int ret;

   ret = pthread_mutex_unlock(&gsCompatLock);
   if (ret) {
      errorMessage("%s: Failed to unlock mutex (%d)", __func__, ret);
      abort();
   }
}

static void *
LoadLib(const char *path)
{
   void *handle;

   /* Clear the error */
   (void) dlerror();

   handle = dlopen(path, RTLD_NOW);
   if (handle) {
      __driUtilMessage("Loaded %s", path);
   } else {
      const char *error;

      error = dlerror();
      if (!error)
         error = "unknown error";

      errorMessage("%s: Couldn't load %s: %s", __func__, path, error);
   }

   return handle;
}

static void
UnloadLib(void *handle, const char *name)
{
   if (!handle)
      return;

   /* Clear the error */
   (void) dlerror();

   if (dlclose(handle)) {
      const char *error;

      error = dlerror();
      if (!error)
         error = "unknown error";

      errorMessage("%s: Couldn't unload %s: %s", __func__, name, error);
   } else {
      __driUtilMessage("Unloaded %s", name);
   }
}

static bool
LoadSupportLib(void)
{
   gpvSupLib = LoadLib(PVRDRI_SUPPORT_LIB);

   return gpvSupLib != NULL;
}

static void
UnloadSupportLib(void)
{
   UnloadLib(gpvSupLib, PVRDRI_SUPPORT_LIB);
   gpvSupLib = NULL;
}

static void
CompatDeinit(void)
{
   UnloadSupportLib();
   memset(&gsSupV0, 0, sizeof(gsSupV0));
}

bool
PVRDRICompatInit(const struct PVRDRICallbacks *psCallbacks,
                 unsigned int uVersionV0, unsigned int uMinVersionV0)
{
   bool (*pfPVRDRIRegisterCallbacks)(const void *pvCallbacks,
                                          unsigned int uVersion,
                                          unsigned int uMinVersion);
   bool res;

   CompatLock();
   res = (giSupLibRef++ != 0);
   if (res)
      goto Exit;

   res = LoadSupportLib();
   if (!res)
      goto Exit;

   LookupFunc(PVRDRIRegisterCallbacks,
              pfPVRDRIRegisterCallbacks);

   res = (pfPVRDRIRegisterCallbacks != NULL);
   if (!res)
      goto Exit;

   res = pfPVRDRIRegisterCallbacks(psCallbacks,
                                        uVersionV0, uMinVersionV0);

Exit:
   if (!res) {
      CompatDeinit();
      giSupLibRef--;
   }
   CompatUnlock();

   return res;
}

void
PVRDRICompatDeinit(void)
{
   CompatLock();
   if (--giSupLibRef == 0)
      CompatDeinit();
   CompatUnlock();
}

bool
MODSUPRegisterSupportInterfaceV2(const void *pvInterface,
                                 unsigned int uVersion,
                                 unsigned int uMinVersion)
{
   size_t uStart, uEnd;

   memset(&gsSupV0, 0, sizeof(gsSupV0));

   if (uVersion < uMinVersion)
      return false;

   /*
    * Minimum versions we support. To prevent the accumulation of old unused
    * interfaces in the PVRDRIInterfaceV2 structure, the caller specifies the
    * minimum version it supports. This will be pointed to be the psInterface
    * argument. Assuming we support that version, we must copy the structure
    * passed to us into the correct place in our version of the interface
    * structure.
    */
   switch (uMinVersion) {
   case 0:
      uStart = SGXDRIInterfaceV0Start(v0);
      break;
   default:
      return false;
   }

   /* The "default" case should be associated with the latest version */
   switch (uVersion) {
   default:
   case 0:
      uEnd = SGXDRIInterfaceV0End(v0);
      break;
   }

   memcpy(((char *) &gsSupV0) + uStart, pvInterface, uEnd - uStart);

   PVRDRIAdjustExtensions(uVersion, uMinVersion);

   return true;
}

struct DRISUPScreen *
DRISUPCreateScreen(struct __DRIscreenRec *psDRIScreen, int iFD,
                   bool bUseInvalidate, void *pvLoaderPrivate,
                   const struct __DRIconfigRec ***pppsConfigs,
                   int *piMaxGLES1Version, int *piMaxGLES2Version)
{
   CallFuncV0(v0.CreateScreen,
              psDRIScreen, iFD, bUseInvalidate, pvLoaderPrivate, pppsConfigs,
              piMaxGLES1Version, piMaxGLES2Version);

   return NULL;
}

void
DRISUPDestroyScreen(struct DRISUPScreen *psDRISUPScreen)
{
   CallFuncV0(v0.DestroyScreen,
              psDRISUPScreen);
}

unsigned int
DRISUPCreateContext(PVRDRIAPIType eAPI, PVRDRIConfig *psPVRDRIConfig,
                    struct PVRDRIContextConfig *psCtxConfig,
                    struct __DRIcontextRec *psDRIContext,
                    struct DRISUPContext *psDRISUPSharedContext,
                    struct DRISUPScreen *psDRISUPScreen,
                    struct DRISUPContext **ppsDRISUPContext)
{
   CallFuncV0(v0.CreateContext,
              eAPI, psPVRDRIConfig, psCtxConfig, psDRIContext,
              psDRISUPSharedContext, psDRISUPScreen, ppsDRISUPContext);

   return __DRI_CTX_ERROR_BAD_API;
}

void
DRISUPDestroyContext(struct DRISUPContext *psDRISUPContext)
{
   CallFuncV0(v0.DestroyContext,
              psDRISUPContext);
}

struct DRISUPDrawable *
DRISUPCreateDrawable(struct __DRIdrawableRec *psDRIDrawable,
                     struct DRISUPScreen *psDRISUPScreen,
                     void *pvLoaderPrivate, PVRDRIConfig *psPVRDRIConfig)
{
   CallFuncV0(v0.CreateDrawable,
              psDRIDrawable, psDRISUPScreen, pvLoaderPrivate, psPVRDRIConfig);

   return NULL;
}

void
DRISUPDestroyDrawable(struct DRISUPDrawable *psDRISUPDrawable)
{
   CallFuncV0(v0.DestroyDrawable,
              psDRISUPDrawable);
}

bool
DRISUPMakeCurrent(struct DRISUPContext *psDRISUPContext,
                  struct DRISUPDrawable *psDRISUPWrite,
                  struct DRISUPDrawable *psDRISUPRead)
{
   CallFuncV0(v0.MakeCurrent,
              psDRISUPContext, psDRISUPWrite, psDRISUPRead);

   return false;
}

bool
DRISUPUnbindContext(struct DRISUPContext *psDRISUPContext)
{
   CallFuncV0(v0.UnbindContext,
              psDRISUPContext);

   return false;
}

struct DRISUPBuffer *
DRISUPAllocateBuffer(struct DRISUPScreen *psDRISUPScreen,
                     unsigned int uAttachment, unsigned int uFormat,
                     int iWidth, int iHeight, unsigned int *puName,
                     unsigned int *puPitch, unsigned int *puCPP,
                     unsigned int *puFlags)
{
   CallFuncV0(v0.AllocateBuffer,
              psDRISUPScreen, uAttachment, uFormat, iWidth, iHeight, puName,
              puPitch, puCPP, puFlags);

   return NULL;
}

void
DRISUPReleaseBuffer(struct DRISUPScreen *psDRISUPScreen,
                    struct DRISUPBuffer *psDRISUPBuffer)
{
   CallFuncV0(v0.ReleaseBuffer,
              psDRISUPScreen, psDRISUPBuffer);
}

void
DRISUPSetTexBuffer2(struct DRISUPContext *psDRISUPContext, int iTarget,
                    int iFormat, struct DRISUPDrawable *psDRISUPDrawable)
{
   CallFuncV0(v0.SetTexBuffer2,
              psDRISUPContext, iTarget, iFormat, psDRISUPDrawable);
}

void
DRISUPReleaseTexBuffer(struct DRISUPContext *psDRISUPContext, int iTarget,
                       struct DRISUPDrawable *psDRISUPDrawable)
{
   CallFuncV0(v0.ReleaseTexBuffer,
              psDRISUPContext, iTarget, psDRISUPDrawable);
}

void
DRISUPFlush(struct DRISUPDrawable *psDRISUPDrawable)
{
   CallFuncV0(v0.Flush,
              psDRISUPDrawable);
}

void
DRISUPInvalidate(struct DRISUPDrawable *psDRISUPDrawable)
{
   CallFuncV0(v0.Invalidate,
              psDRISUPDrawable);
}

void
DRISUPFlushWithFlags(struct DRISUPContext *psDRISUPContext,
                     struct DRISUPDrawable *psDRISUPDrawable,
                     unsigned int uFlags, unsigned int uThrottleReason)
{
   CallFuncV0(v0.FlushWithFlags,
              psDRISUPContext, psDRISUPDrawable, uFlags, uThrottleReason);
}

__DRIimage *
DRISUPCreateImageFromName(struct DRISUPScreen *psDRISUPScreen,
                          int iWidth, int iHeight, int iFourCC, int iName,
                          int iPitch, void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageFromName,
              psDRISUPScreen, iWidth, iHeight, iFourCC, iName, iPitch,
              pvLoaderPrivate);

   return NULL;
}

__DRIimage *
DRISUPCreateImageFromRenderbuffer(struct DRISUPContext *psDRISUPContext,
                                  int iRenderBuffer, void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageFromRenderbuffer,
              psDRISUPContext, iRenderBuffer, pvLoaderPrivate);

   return NULL;
}

void
DRISUPDestroyImage(__DRIimage *psImage)
{
   CallFuncV0(v0.DestroyImage, psImage);
}

__DRIimage *
DRISUPCreateImage(struct DRISUPScreen *psDRISUPScreen,
                  int iWidth, int iHeight, int iFourCC, unsigned int uUse,
                  void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImage,
              psDRISUPScreen, iWidth, iHeight, iFourCC, uUse, pvLoaderPrivate);

   return NULL;
}

bool
DRISUPQueryImage(__DRIimage *psImage, int iAttrib, int *piValue)
{
   CallFuncV0(v0.QueryImage,
              psImage, iAttrib, piValue);

   return false;
}

__DRIimage *
DRISUPDupImage(__DRIimage *psImage, void *pvLoaderPrivate)
{
   CallFuncV0(v0.DupImage,
              psImage, pvLoaderPrivate);

   return NULL;
}

bool
DRISUPValidateImageUsage(__DRIimage *psImage, unsigned int uUse)
{
   CallFuncV0(v0.ValidateImageUsage,
              psImage, uUse);

   return false;
}

__DRIimage *
DRISUPCreateImageFromNames(struct DRISUPScreen *psDRISUPScreen,
                           int iWidth, int iHeight, int iFourCC,
                           int *piNames, int iNumNames,
                           int *piStrides, int *piOffsets,
                           void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageFromNames,
              psDRISUPScreen, iWidth, iHeight, iFourCC, piNames, iNumNames,
              piStrides, piOffsets, pvLoaderPrivate);

   return NULL;
}

__DRIimage *
DRISUPFromPlanar(__DRIimage *psImage, int iPlane, void *pvLoaderPrivate)
{
   CallFuncV0(v0.FromPlanar,
              psImage, iPlane, pvLoaderPrivate);

   return NULL;
}

__DRIimage *
DRISUPCreateImageFromTexture(struct DRISUPContext *psDRISUPContext,
                             int iTarget, unsigned int uTexture, int iDepth,
                             int iLevel, unsigned int *puError,
                             void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageFromTexture,
              psDRISUPContext, iTarget, uTexture, iDepth, iLevel, puError,
              pvLoaderPrivate);

   return NULL;
}

__DRIimage *
DRISUPCreateImageFromFDs(struct DRISUPScreen *psDRISUPcreen,
                         int iWidth, int iHeight, int iFourCC,
                         int *piFDs, int iNumFDs, int *piStrides,
                         int *piOffsets, void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageFromFDs,
              psDRISUPcreen, iWidth, iHeight, iFourCC, piFDs, iNumFDs,
              piStrides, piOffsets, pvLoaderPrivate);

   return NULL;
}

__DRIimage *
DRISUPCreateImageFromDmaBufs(struct DRISUPScreen *psDRISUPScreen,
                             int iWidth, int iHeight, int iFourCC,
                             int *piFDs, int iNumFDs,
                             int *piStrides, int *piOffsets,
                             unsigned int uColorSpace,
                             unsigned int uSampleRange,
                             unsigned int uHorizSiting,
                             unsigned int uVertSiting,
                             unsigned int *puError,
                             void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageFromDMABufs,
              psDRISUPScreen, iWidth, iHeight, iFourCC, piFDs, iNumFDs,
              piStrides, piOffsets, uColorSpace, uSampleRange,
              uHorizSiting, uVertSiting, puError, pvLoaderPrivate);

   return NULL;
}

int
DRISUPGetImageCapabilities(struct DRISUPScreen *psDRISUPScreen)
{
   CallFuncV0(v0.GetImageCapabilities,
              psDRISUPScreen);

   return 0;
}

void
DRISUPBlitImage(struct DRISUPContext *psDRISUPContext,
                __DRIimage *psDst, __DRIimage *psSrc, int iDstX0, int iDstY0,
                int iDstWidth, int iDstHeight, int iSrcX0, int iSrcY0,
                int iSrcWidth, int iSrcHeight, int iFlushFlag)
{
   CallFuncV0(v0.BlitImage,
              psDRISUPContext, psDst, psSrc, iDstX0, iDstY0,
              iDstWidth, iDstHeight, iSrcX0, iSrcY0,
              iSrcWidth, iSrcHeight, iFlushFlag);
}

void *
DRISUPMapImage(struct DRISUPContext *psDRISUPContext, __DRIimage* psImage,
               int iX0, int iY0, int iWidth, int iHeight, unsigned int uFlags,
               int *piStride, void **ppvData)
{
   CallFuncV0(v0.MapImage,
              psDRISUPContext, psImage, iX0, iY0, iWidth, iHeight, uFlags,
              piStride, ppvData);

   return NULL;
}

void
DRISUPUnmapImage(struct DRISUPContext *psDRISUPContext, __DRIimage *psImage,
                 void *pvData)
{
   CallFuncV0(v0.UnmapImage,
              psDRISUPContext, psImage, pvData);
}

__DRIimage *
DRISUPCreateImageWithModifiers(struct DRISUPScreen *psDRISUPScreen,
                               int iWidth, int iHeight, int iFourCC,
                               const uint64_t *puModifiers,
                               const unsigned int uModifierCount,
                               void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageWithModifiers,
              psDRISUPScreen, iWidth, iHeight, iFourCC, puModifiers,
              uModifierCount, pvLoaderPrivate);

   return NULL;
}

__DRIimage *
DRISUPCreateImageFromDMABufs2(struct DRISUPScreen *psDRISUPScreen,
                              int iWidth, int iHeight, int iFourCC,
                              uint64_t uModifier, int *piFDs, int iNumFDs,
                              int *piStrides, int *piOffsets,
                              unsigned int uColorSpace,
                              unsigned int uSampleRange,
                              unsigned int uHorizSiting,
                              unsigned int uVertSiting,
                              unsigned int *puError, void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageFromDMABufs2,
              psDRISUPScreen, iWidth, iHeight, iFourCC, uModifier,
              piFDs, iNumFDs, piStrides, piOffsets, uColorSpace, uSampleRange,
              uHorizSiting, uVertSiting, puError, pvLoaderPrivate);

   return NULL;
}

bool
DRISUPQueryDMABufFormats(struct DRISUPScreen *psDRISUPScreen, int iMax,
                         int *piFormats, int *piCount)
{
   CallFuncV0(v0.QueryDMABufFormats,
              psDRISUPScreen, iMax, piFormats, piCount);

   return false;
}

bool
DRISUPQueryDMABufModifiers(struct DRISUPScreen *psDRISUPScreen, int iFourCC,
                           int iMax, uint64_t *puModifiers,
                           unsigned int *piExternalOnly, int *piCount)
{
   CallFuncV0(v0.QueryDMABufModifiers,
              psDRISUPScreen, iFourCC, iMax, puModifiers, piExternalOnly,
              piCount);

   return false;
}

bool
DRISUPQueryDMABufFormatModifierAttribs(struct DRISUPScreen *psDRISUPScreen,
                                       uint32_t iFourCC, uint64_t uModifier,
                                       int iAttrib, uint64_t *puValue)
{
   CallFuncV0(v0.QueryDMABufFormatModifierAttribs,
              psDRISUPScreen, iFourCC, uModifier, iAttrib, puValue);

   return false;
}

__DRIimage *
DRISUPCreateImageFromRenderBuffer2(struct DRISUPContext *psDRISUPContext,
                                   int iRenderBuffer, void *pvLoaderPrivate,
                                   unsigned int *puError)
{
   CallFuncV0(v0.CreateImageFromRenderBuffer2,
              psDRISUPContext, iRenderBuffer, pvLoaderPrivate, puError);

   return NULL;
}

__DRIimage *
DRISUPCreateImageFromBuffer(struct DRISUPContext *psDRISUPContext,
                            int iTarget, void *pvBuffer,
                            unsigned int *puError, void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageFromBuffer,
              psDRISUPContext, iTarget, pvBuffer, puError, pvLoaderPrivate);

   return NULL;
}

int
DRISUPQueryRendererInteger(struct DRISUPScreen *psDRISUPScreen,
                           int iAttribute, unsigned int *puValue)
{
   CallFuncV0(v0.QueryRendererInteger,
              psDRISUPScreen, iAttribute, puValue);

   return -1;
}

int
DRISUPQueryRendererString(struct DRISUPScreen *psDRISUPScreen,
                          int iAttribute, const char **ppszValue)
{
   CallFuncV0(v0.QueryRendererString,
              psDRISUPScreen, iAttribute, ppszValue);

   return -1;
}

void *
DRISUPCreateFence(struct DRISUPContext *psDRISUPContext)
{
   CallFuncV0(v0.CreateFence,
              psDRISUPContext);

   return NULL;
}

void
DRISUPDestroyFence(struct DRISUPScreen *psDRISUPScreen, void *pvFence)
{
   CallFuncV0(v0.DestroyFence,
              psDRISUPScreen, pvFence);
}

bool
DRISUPClientWaitSync(struct DRISUPContext *psDRISUPContext, void *pvFence,
                     unsigned int uFlags, uint64_t uTimeout)
{
   CallFuncV0(v0.ClientWaitSync,
              psDRISUPContext, pvFence, uFlags, uTimeout);

   return false;
}

void
DRISUPServerWaitSync(struct DRISUPContext *psDRISUPContext, void *pvFence,
                     unsigned int uFlags)
{
   CallFuncV0(v0.ServerWaitSync,
              psDRISUPContext, pvFence, uFlags);
}

unsigned int
DRISUPGetFenceCapabilities(struct DRISUPScreen *psDRISUPScreen)
{
   CallFuncV0(v0.GetFenceCapabilities,
              psDRISUPScreen);

   return 0;
}

void *
DRISUPCreateFenceFD(struct DRISUPContext *psDRISUPContext, int iFD)
{
   CallFuncV0(v0.CreateFenceFD,
              psDRISUPContext, iFD);

   return NULL;
}

int
DRISUPGetFenceFD(struct DRISUPScreen *psDRISUPScreen, void *pvFence)
{
   CallFuncV0(v0.GetFenceFD,
              psDRISUPScreen, pvFence);

   return -1;
}

void *
DRISUPGetFenceFromCLEvent(struct DRISUPScreen *psDRISUPScreen,
                          intptr_t iCLEvent)
{
   CallFuncV0(v0.GetFenceFromCLEvent,
              psDRISUPScreen, iCLEvent);

   return NULL;
}

int
DRISUPGetAPIVersion(struct DRISUPScreen *psDRISUPScreen,
                    PVRDRIAPIType eAPI)
{
   CallFuncV0(v0.GetAPIVersion,
              psDRISUPScreen, eAPI);

   return 0;
}

unsigned int
DRISUPGetNumAPIProcs(struct DRISUPScreen *psDRISUPScreen,
                     PVRDRIAPIType eAPI)
{
   CallFuncV0(v0.GetNumAPIProcs,
              psDRISUPScreen, eAPI);

   return 0;
}

const char *
DRISUPGetAPIProcName(struct DRISUPScreen *psDRISUPScreen, PVRDRIAPIType eAPI,
                     unsigned int uIndex)
{
   CallFuncV0(v0.GetAPIProcName,
              psDRISUPScreen, eAPI, uIndex);

   return NULL;
}

void *
DRISUPGetAPIProcAddress(struct DRISUPScreen *psDRISUPScreen,
                        PVRDRIAPIType eAPI, unsigned int uIndex)
{
   CallFuncV0(v0.GetAPIProcAddress,
              psDRISUPScreen, eAPI, uIndex);

   return NULL;
}

void
DRISUPSetDamageRegion(struct DRISUPDrawable *psDRISUPDrawable,
                      unsigned int uNRects, int *piRects)
{
   CallFuncV0(v0.SetDamageRegion,
              psDRISUPDrawable, uNRects, piRects);
}

bool
DRISUPHaveGetFenceFromCLEvent(void)
{
   CallFuncV0(v0.HaveGetFenceFromCLEvent);

   return true;
}

__DRIimage *
DRISUPCreateImageFromDMABufs3(struct DRISUPScreen *psDRISUPScreen,
                              int iWidth, int iHeight, int iFourCC,
                              uint64_t uModifier, int *piFDs, int iNumFDs,
                              int *piStrides, int *piOffsets,
                              unsigned int uColorSpace,
                              unsigned int uSampleRange,
                              unsigned int uHorizSiting,
                              unsigned int uVertSiting,
                              uint32_t uFlags,
                              unsigned int *puError, void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageFromDMABufs3,
              psDRISUPScreen, iWidth, iHeight, iFourCC, uModifier,
              piFDs, iNumFDs, piStrides, piOffsets, uColorSpace, uSampleRange,
              uHorizSiting, uVertSiting, uFlags, puError, pvLoaderPrivate);

   return NULL;
}

__DRIimage *
DRISUPCreateImageWithModifiers2(struct DRISUPScreen *psDRISUPScreen,
                                int iWidth, int iHeight, int iFourCC,
                                const uint64_t *puModifiers,
                                const unsigned int uModifierCount,
                                unsigned int uUse,
                                void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageWithModifiers2,
              psDRISUPScreen, iWidth, iHeight, iFourCC, puModifiers,
              uModifierCount, uUse, pvLoaderPrivate);

   return NULL;
}

__DRIimage *
DRISUPCreateImageFromFDs2(struct DRISUPScreen *psDRISUPcreen,
                          int iWidth, int iHeight, int iFourCC,
                          int *piFDs, int iNumFDs, uint32_t uFlags,
                          int *piStrides, int *piOffsets,
                          void *pvLoaderPrivate)
{
   CallFuncV0(v0.CreateImageFromFDs2,
              psDRISUPcreen, iWidth, iHeight, iFourCC, piFDs, iNumFDs,
              uFlags, piStrides, piOffsets, pvLoaderPrivate);

   return NULL;
}

bool
DRISUPHaveSetInFenceFd(void)
{
	return HaveFuncV0(v0.SetInFenceFD);
}

void
DRISUPSetInFenceFd(__DRIimage *psImage, int iFd)
{
   CallFuncV0(v0.SetInFenceFD,
              psImage, iFd);
}
