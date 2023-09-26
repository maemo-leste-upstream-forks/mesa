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

#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <drm_fourcc.h>

#include "pvrdri.h"

#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif

#define _MAKESTRING(x) #x
#define MAKESTRING(x) _MAKESTRING(x)

#define PVRDRI_SUPPORT_LIB "libpvr_dri_support.so"

static void *gpvSupLib;
static int giSupLibRef;

static struct SGXDRISupportInterfaceV0 gsSupV0;

static pthread_mutex_t gsCompatLock = PTHREAD_MUTEX_INITIALIZER;

/* Lookup a function, and set the pointer to the function address */
#define LookupFunc(func, ptr)                                                  \
  do {                                                                         \
    ptr = dlsym(gpvSupLib, MAKESTRING(func));                                  \
  } while (0)

/* Check if a function exists in the DRI Support interface structure */
#define HaveFuncV0(field) ((gsSupV0.field) != NULL)

/* Call a function via the DRI Support interface structure */
#define CallFuncV0(field, ...)                                                 \
  do {                                                                         \
    if (gsSupV0.field)                                                         \
      return gsSupV0.field(__VA_ARGS__);                                       \
  } while (0)

/* Calculate the start of the SGXDRISupportInterfaceV0 structure */
#define SGXDRIInterfaceV0Start(field)                                          \
  (offsetof(struct SGXDRISupportInterfaceV0, field))

/* Calculate the end of the SGXDRISupportInterfaceV0 structure */
#define SGXDRIInterfaceV0End(field)                                            \
  (offsetof(struct SGXDRISupportInterfaceV0, field) +                          \
   sizeof((struct SGXDRISupportInterfaceV0 *)0)->field)

static void CompatLock(void) {
  int ret;

  ret = pthread_mutex_lock(&gsCompatLock);
  if (ret) {
    mesa_loge("%s: Failed to lock mutex (%d)", __func__, ret);
    abort();
  }
}

static void CompatUnlock(void) {
  int ret;

  ret = pthread_mutex_unlock(&gsCompatLock);
  if (ret) {
    mesa_loge("%s: Failed to unlock mutex (%d)", __func__, ret);
    abort();
  }
}

static void *LoadLib(const char *path) {
  void *handle;

  /* Clear the error */
  (void)dlerror();

  handle = dlopen(path, RTLD_NOW);
  if (handle) {
    mesa_logi("Loaded %s", path);
  } else {
    const char *error;

    error = dlerror();
    if (!error)
      error = "unknown error";

    mesa_loge("%s: Couldn't load %s: %s", __func__, path, error);
  }

  return handle;
}

static void UnloadLib(void *handle, const char *name) {
  if (!handle)
    return;

  /* Clear the error */
  (void)dlerror();

  if (dlclose(handle)) {
    const char *error;

    error = dlerror();
    if (!error)
      error = "unknown error";

    mesa_loge("%s: Couldn't unload %s: %s", __func__, name, error);
  } else {
    mesa_logi("Unloaded %s", name);
  }
}

static bool LoadSupportLib(void) {
  gpvSupLib = LoadLib(PVRDRI_SUPPORT_LIB);

  return gpvSupLib != NULL;
}

static void UnloadSupportLib(void) {
  UnloadLib(gpvSupLib, PVRDRI_SUPPORT_LIB);
  gpvSupLib = NULL;
}

static void CompatDeinit(void) {
  UnloadSupportLib();
  memset(&gsSupV0, 0, sizeof(gsSupV0));
}

bool PVRDRICompatInit(const PVRDRICallbacks *psCallbacks,
                      unsigned int uVersionV0, unsigned int uMinVersionV0) {
  bool (*pfPVRDRIRegisterCallbacks)(
      const void *pvCallbacks, unsigned int uVersion, unsigned int uMinVersion);
  bool res;

  CompatLock();
  res = (giSupLibRef++ != 0);
  if (res)
    goto Exit;

  res = LoadSupportLib();
  if (!res)
    goto Exit;

  LookupFunc(PVRDRIRegisterCallbacks, pfPVRDRIRegisterCallbacks);

  res = (pfPVRDRIRegisterCallbacks != NULL);
  if (!res)
    goto Exit;

  res = pfPVRDRIRegisterCallbacks(psCallbacks, uVersionV0, uMinVersionV0);

Exit:
  if (!res) {
    CompatDeinit();
    giSupLibRef--;
  }
  CompatUnlock();

  return res;
}

void PVRDRICompatDeinit(void) {
  CompatLock();
  if (--giSupLibRef == 0)
    CompatDeinit();
  CompatUnlock();
}

PVRDRIDeviceType PVRDRIGetDeviceTypeFromFd(int iFd) {
  CallFuncV0(v0.PVRDRIGetDeviceTypeFromFd, iFd);
  return PVRDRI_DEVICE_TYPE_UNKNOWN;
}

bool PVRDRIIsFirstScreen(PVRDRIScreenImpl *psScreenImpl) {
  CallFuncV0(v0.PVRDRIIsFirstScreen, psScreenImpl);
  return false;
}

uint32_t PVRDRIPixFmtGetDepth(IMG_PIXFMT eFmt) {
  CallFuncV0(v0.PVRDRIPixFmtGetDepth, eFmt);
  return 0;
}
uint32_t PVRDRIPixFmtGetBPP(IMG_PIXFMT eFmt) {
  CallFuncV0(v0.PVRDRIPixFmtGetBPP, eFmt);
  return 0;
}
uint32_t PVRDRIPixFmtGetBlockSize(IMG_PIXFMT eFmt) {
  CallFuncV0(v0.PVRDRIPixFmtGetBlockSize, eFmt);
  return 0;
}

/* ScreenImpl functions */
PVRDRIScreenImpl *PVRDRICreateScreenImpl(int iFd) {
  CallFuncV0(v0.PVRDRICreateScreenImpl, iFd);
  return NULL;
}
void PVRDRIDestroyScreenImpl(PVRDRIScreenImpl *psScreenImpl) {
  CallFuncV0(v0.PVRDRIDestroyScreenImpl, psScreenImpl);
}

int PVRDRIAPIVersion(PVRDRIAPIType eAPI, PVRDRIAPISubType eAPISub,
                     PVRDRIScreenImpl *psScreenImpl) {
  CallFuncV0(v0.PVRDRIAPIVersion, eAPI, eAPISub, psScreenImpl);
  return 0;
}

void *PVRDRIEGLGetLibHandle(PVRDRIAPIType eAPI,
                            PVRDRIScreenImpl *psScreenImpl) {
  CallFuncV0(v0.PVRDRIEGLGetLibHandle, eAPI, psScreenImpl);
  return NULL;
}
PVRDRIGLAPIProc PVRDRIEGLGetProcAddress(PVRDRIAPIType eAPI,
                                        PVRDRIScreenImpl *psScreenImpl,
                                        const char *psProcName) {
  CallFuncV0(v0.PVRDRIEGLGetProcAddress, eAPI, psScreenImpl, psProcName);
  return NULL;
}

bool PVRDRIEGLFlushBuffers(PVRDRIAPIType eAPI, PVRDRIScreenImpl *psScreenImpl,
                           PVRDRIContextImpl *psContextImpl,
                           PVRDRIDrawableImpl *psDrawableImpl,
                           bool bFlushAllSurfaces, bool bSwapBuffers,
                           bool bWaitForHW) {
  CallFuncV0(v0.PVRDRIEGLFlushBuffers, eAPI, psScreenImpl, psContextImpl,
             psDrawableImpl, bFlushAllSurfaces, bSwapBuffers, bWaitForHW);
  return false;
}
bool PVRDRIEGLFreeResources(PVRDRIScreenImpl *psPVRScreenImpl) {
  CallFuncV0(v0.PVRDRIEGLFreeResources, psPVRScreenImpl);
  return false;
}
void PVRDRIEGLMarkRendersurfaceInvalid(PVRDRIAPIType eAPI,
                                       PVRDRIScreenImpl *psScreenImpl,
                                       PVRDRIContextImpl *psContextImpl) {
  CallFuncV0(v0.PVRDRIEGLMarkRendersurfaceInvalid, eAPI, psScreenImpl,
             psContextImpl);
}
void PVRDRIEGLSetFrontBufferCallback(PVRDRIAPIType eAPI,
                                     PVRDRIScreenImpl *psScreenImpl,
                                     PVRDRIDrawableImpl *psDrawableImpl,
                                     void pfnCallback(PVRDRIDrawable *)) {
  CallFuncV0(v0.PVRDRIEGLSetFrontBufferCallback, eAPI, psScreenImpl,
             psDrawableImpl, pfnCallback);
}

unsigned PVRDRICreateContextImpl(PVRDRIContextImpl **ppsContextImpl,
                                 PVRDRIAPIType eAPI, PVRDRIAPISubType eAPISub,
                                 PVRDRIScreenImpl *psScreenImpl,
                                 const PVRDRIConfigInfo *psConfigInfo,
                                 unsigned uMajorVersion, unsigned uMinorVersion,
                                 uint32_t uFlags, bool bNotifyReset,
                                 unsigned uPriority,
                                 PVRDRIContextImpl *psSharedContextImpl) {
  CallFuncV0(v0.PVRDRICreateContextImpl, ppsContextImpl, eAPI, eAPISub,
             psScreenImpl, psConfigInfo, uMajorVersion, uMinorVersion, uFlags,
             bNotifyReset, uPriority, psSharedContextImpl);
  return 0;
}

void PVRDRIDestroyContextImpl(PVRDRIContextImpl *psContextImpl,
                              PVRDRIAPIType eAPI,
                              PVRDRIScreenImpl *psScreenImpl) {
  CallFuncV0(v0.PVRDRIDestroyContextImpl, psContextImpl, eAPI, psScreenImpl);
}

bool PVRDRIMakeCurrentGC(PVRDRIAPIType eAPI, PVRDRIScreenImpl *psScreenImpl,
                         PVRDRIContextImpl *psContextImpl,
                         PVRDRIDrawableImpl *psWriteImpl,
                         PVRDRIDrawableImpl *psReadImpl) {
  CallFuncV0(v0.PVRDRIMakeCurrentGC, eAPI, psScreenImpl, psContextImpl,
             psWriteImpl, psReadImpl);
  return false;
}

void PVRDRIMakeUnCurrentGC(PVRDRIAPIType eAPI, PVRDRIScreenImpl *psScreenImpl) {
  CallFuncV0(v0.PVRDRIMakeUnCurrentGC, eAPI, psScreenImpl);
}

unsigned PVRDRIGetImageSource(PVRDRIAPIType eAPI,
                              PVRDRIScreenImpl *psScreenImpl,
                              PVRDRIContextImpl *psContextImpl,
                              uint32_t uiTarget, uintptr_t uiBuffer,
                              uint32_t uiLevel, __EGLImage *psEGLImage) {
  CallFuncV0(v0.PVRDRIGetImageSource, eAPI, psScreenImpl, psContextImpl,
             uiTarget, uiBuffer, uiLevel, psEGLImage);
  return 0;
}

bool PVRDRI2BindTexImage(PVRDRIAPIType eAPI, PVRDRIScreenImpl *psScreenImpl,
                         PVRDRIContextImpl *psContextImpl,
                         PVRDRIDrawableImpl *psDrawableImpl) {
  CallFuncV0(v0.PVRDRI2BindTexImage, eAPI, psScreenImpl, psContextImpl,
             psDrawableImpl);
  return false;
}

void PVRDRI2ReleaseTexImage(PVRDRIAPIType eAPI, PVRDRIScreenImpl *psScreenImpl,
                            PVRDRIContextImpl *psContextImpl,
                            PVRDRIDrawableImpl *psDrawableImpl) {
  CallFuncV0(v0.PVRDRI2ReleaseTexImage, eAPI, psScreenImpl, psContextImpl,
             psDrawableImpl);
}

/* DrawableImpl functions */
PVRDRIDrawableImpl *PVRDRICreateDrawableImpl(PVRDRIDrawable *psPVRDrawable) {
  CallFuncV0(v0.PVRDRICreateDrawableImpl, psPVRDrawable);
  return NULL;
}
void PVRDRIDestroyDrawableImpl(PVRDRIDrawableImpl *psScreenImpl) {
  CallFuncV0(v0.PVRDRIDestroyDrawableImpl, psScreenImpl);
}
bool PVREGLDrawableCreate(PVRDRIScreenImpl *psScreenImpl,
                          PVRDRIDrawableImpl *psDrawableImpl) {
  CallFuncV0(v0.PVREGLDrawableCreate, psScreenImpl, psDrawableImpl);
  return false;
}
bool PVREGLDrawableRecreate(PVRDRIScreenImpl *psScreenImpl,
                            PVRDRIDrawableImpl *psDrawableImpl) {
  CallFuncV0(v0.PVREGLDrawableRecreate, psScreenImpl, psDrawableImpl);
  return false;
}
bool PVREGLDrawableDestroy(PVRDRIScreenImpl *psScreenImpl,
                           PVRDRIDrawableImpl *psDrawableImpl) {
  CallFuncV0(v0.PVREGLDrawableDestroy, psScreenImpl, psDrawableImpl);
  return false;
}
void PVREGLDrawableDestroyConfig(PVRDRIDrawableImpl *psDrawableImpl) {
  CallFuncV0(v0.PVREGLDrawableDestroyConfig, psDrawableImpl);
}

/* Buffer functions */
PVRDRIBufferImpl *PVRDRIBufferCreate(PVRDRIScreenImpl *psScreenImpl, int iWidth,
                                     int iHeight, unsigned int uiBpp,
                                     unsigned int uiUseFlags,
                                     unsigned int *puiStride) {
  CallFuncV0(v0.PVRDRIBufferCreate, psScreenImpl, iWidth, iHeight, uiBpp,
             uiUseFlags, puiStride);
  return NULL;
}

PVRDRIBufferImpl *PVRDRIBufferCreateFromNames(
    PVRDRIScreenImpl *psScreenImpl, int iWidth, int iHeight,
    unsigned uiNumPlanes, const int *piName, const int *piStride,
    const int *piOffset, const unsigned int *puiWidthShift,
    const unsigned int *puiHeightShift) {
  CallFuncV0(v0.PVRDRIBufferCreateFromNames, psScreenImpl, iWidth, iHeight,
             uiNumPlanes, piName, piStride, piOffset, puiWidthShift,
             puiHeightShift);
  return NULL;
}

PVRDRIBufferImpl *PVRDRIBufferCreateFromName(PVRDRIScreenImpl *psScreenImpl,
                                             int iName, int iWidth, int iHeight,
                                             int iStride, int iOffset) {
  CallFuncV0(v0.PVRDRIBufferCreateFromName, psScreenImpl, iName, iWidth,
             iHeight, iStride, iOffset);
  return NULL;
}

PVRDRIBufferImpl *
PVRDRIBufferCreateFromFds(PVRDRIScreenImpl *psScreenImpl, int iWidth,
                          int iHeight, unsigned uiNumPlanes, const int *piFd,
                          const int *piStride, const int *piOffset,
                          const unsigned int *puiWidthShift,
                          const unsigned int *puiHeightShift) {
  CallFuncV0(v0.PVRDRIBufferCreateFromFds, psScreenImpl, iWidth, iHeight,
             uiNumPlanes, piFd, piStride, piOffset, puiWidthShift,
             puiHeightShift);
  return NULL;
}

void PVRDRIBufferDestroy(PVRDRIBufferImpl *psBuffer) {
  CallFuncV0(v0.PVRDRIBufferDestroy, psBuffer);
}

int PVRDRIBufferGetFd(PVRDRIBufferImpl *psBuffer) {
  CallFuncV0(v0.PVRDRIBufferGetFd, psBuffer);
  return 0;
}

int PVRDRIBufferGetHandle(PVRDRIBufferImpl *psBuffer) {
  CallFuncV0(v0.PVRDRIBufferGetHandle, psBuffer);
  return 0;
}

int PVRDRIBufferGetName(PVRDRIBufferImpl *psBuffer) {
  CallFuncV0(v0.PVRDRIBufferGetName, psBuffer);
  return 0;
}

/* Image functions */
__EGLImage *PVRDRIEGLImageCreate(void) {
  CallFuncV0(v0.PVRDRIEGLImageCreate);
  return NULL;
}
__EGLImage *PVRDRIEGLImageCreateFromBuffer(int iWidth, int iHeight, int iStride,
                                           IMG_PIXFMT ePixelFormat,
                                           IMG_YUV_COLORSPACE eColourSpace,
                                           IMG_YUV_CHROMA_INTERP eChromaUInterp,
                                           IMG_YUV_CHROMA_INTERP eChromaVInterp,
                                           PVRDRIBufferImpl *psBuffer) {
  CallFuncV0(v0.PVRDRIEGLImageCreateFromBuffer, iWidth, iHeight, iStride,
             ePixelFormat, eColourSpace, eChromaUInterp, eChromaVInterp,
             psBuffer);
  return NULL;
}

__EGLImage *PVRDRIEGLImageDup(__EGLImage *psIn) {
  CallFuncV0(v0.PVRDRIEGLImageDup, psIn);
  return NULL;
}

void PVRDRIEGLImageSetCallbackData(__EGLImage *psEGLImage, __DRIimage *image) {
  CallFuncV0(v0.PVRDRIEGLImageSetCallbackData, psEGLImage, image);
}

void PVRDRIEGLImageDestroyExternal(PVRDRIScreenImpl *psScreenImpl,
                                   __EGLImage *psEGLImage,
                                   PVRDRIEGLImageType eglImageType) {
  CallFuncV0(v0.PVRDRIEGLImageDestroyExternal, psScreenImpl, psEGLImage,
             eglImageType);
}
void PVRDRIEGLImageFree(__EGLImage *psEGLImage) {
  CallFuncV0(v0.PVRDRIEGLImageFree, psEGLImage);
}

void PVRDRIEGLImageGetAttribs(__EGLImage *psEGLImage,
                              PVRDRIBufferAttribs *psAttribs) {
  CallFuncV0(v0.PVRDRIEGLImageGetAttribs, psEGLImage, psAttribs);
}

/* Sync functions */
void *PVRDRICreateFenceImpl(PVRDRIAPIType eAPI, PVRDRIScreenImpl *psScreenImpl,
                            PVRDRIContextImpl *psContextImpl) {
  CallFuncV0(v0.PVRDRICreateFenceImpl, eAPI, psScreenImpl, psContextImpl);
  return NULL;
}

void PVRDRIDestroyFenceImpl(void *psDRIFence) {
  CallFuncV0(v0.PVRDRIDestroyFenceImpl, psDRIFence);
}

bool PVRDRIClientWaitSyncImpl(PVRDRIAPIType eAPI,
                              PVRDRIContextImpl *psContextImpl,
                              void *psDRIFence, bool bFlushCommands,
                              bool bTimeout, uint64_t uiTimeout) {
  CallFuncV0(v0.PVRDRIClientWaitSyncImpl, eAPI, psContextImpl, psDRIFence,
             bFlushCommands, bTimeout, uiTimeout);
  return false;
}

bool PVRDRIServerWaitSyncImpl(PVRDRIAPIType eAPI,
                              PVRDRIContextImpl *psContextImpl,
                              void *psDRIFence) {
  CallFuncV0(v0.PVRDRIServerWaitSyncImpl, eAPI, psContextImpl, psDRIFence);
  return false;
}

void PVRDRIDestroyFencesImpl(PVRDRIScreenImpl *psScreenImpl) {
  CallFuncV0(v0.PVRDRIDestroyFencesImpl, psScreenImpl);
}

/* EGL interface functions */
bool PVRDRIEGLDrawableConfigFromGLMode(PVRDRIDrawableImpl *psPVRDrawable,
                                       PVRDRIConfigInfo *psConfigInfo,
                                       int supportedAPIs, IMG_PIXFMT ePixFmt) {
  CallFuncV0(v0.PVRDRIEGLDrawableConfigFromGLMode, psPVRDrawable, psConfigInfo,
             supportedAPIs, ePixFmt);
  return false;
}

void PVRDRIRegisterCallbacks(PVRDRICallbacks *callbacks) {
  CallFuncV0(v0.PVRDRIRegisterCallbacks, callbacks);
}

/* PVR utility support functions */
bool PVRDRIMesaFormatSupported(unsigned fmt) {
  CallFuncV0(v0.PVRDRIMesaFormatSupported, fmt);
  return false;
}
unsigned PVRDRIDepthStencilBitArraySize(void) {
  CallFuncV0(v0.PVRDRIDepthStencilBitArraySize);
  return 0;
}
const uint8_t *PVRDRIDepthBitsArray(void) {
  CallFuncV0(v0.PVRDRIDepthBitsArray);
  return 0;
}
const uint8_t *PVRDRIStencilBitsArray(void) {
  CallFuncV0(v0.PVRDRIStencilBitsArray);
  return 0;
}
unsigned PVRDRIMSAABitArraySize(void) {
  CallFuncV0(v0.PVRDRIMSAABitArraySize);
  return 0;
}
const uint8_t *PVRDRIMSAABitsArray(void) {
  CallFuncV0(v0.PVRDRIMSAABitsArray);
  return 0;
}
uint32_t PVRDRIMaxPBufferWidth(void) {
  CallFuncV0(v0.PVRDRIMaxPBufferWidth);
  return 0;
}
uint32_t PVRDRIMaxPBufferHeight(void) {
  CallFuncV0(v0.PVRDRIMaxPBufferHeight);
  return 0;
}

unsigned PVRDRIGetNumAPIFuncs(PVRDRIAPIType eAPI) {
  CallFuncV0(v0.PVRDRIGetNumAPIFuncs, eAPI);
  return 0;
}
const char *PVRDRIGetAPIFunc(PVRDRIAPIType eAPI, unsigned index) {
  CallFuncV0(v0.PVRDRIGetAPIFunc, eAPI, index);
  return 0;
}
