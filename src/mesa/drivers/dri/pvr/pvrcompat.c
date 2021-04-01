/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
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
#define DRM_FORMAT_MOD_INVALID ((1ULL<<56) - 1)
#endif

#define _MAKESTRING(x) # x
#define MAKESTRING(x) _MAKESTRING(x)

#define	PVRDRI_SUPPORT_LIB	"libpvr_dri_support.so"

static void *gpvSupLib;
static int giSupLibRef;

static PVRDRISupportInterface gsSup;
static unsigned guSupVer;

static pthread_mutex_t gsCompatLock = PTHREAD_MUTEX_INITIALIZER;

static void DumSupFunc(void) { abort(); }

/* Call a function via the DRI Support interface structure */
#define CallFunc(field, ver, ...) 					\
	do { 								\
		if (guSupVer >= ver && gsSup.field)			\
			return gsSup.field(__VA_ARGS__);		\
	} while(0)

/* Define a function to test for a given DRI Support function */
#define DefineFunc_IsSupported(func, field, ver)			\
	bool func ## _IsSupported(void)					\
	{ return guSupVer >= ver && gsSup.field; }


/*
 * Lookup a function, and set the pointer to the function address.
 * If lookup fails, the pointer is set to the address of the dummy
 * support function, DumSupFunc.
 */
#define LookupFunc(func, ptr)						\
	do {								\
		if (!ptr)						\
		{							\
			ptr = dlsym(gpvSupLib, MAKESTRING(func));	\
			if (!ptr)					\
				ptr = (void *)DumSupFunc; 		\
		}							\
	} while(0)

/*
 * Lookup a legacy DRI support function, and set the function pointer
 * in the support interface structure, gsSup.
 * If lookup fails, the function pointer is set to the address of the
 * dummy support function, DumSupFunc.
 */
#define LookupLegacyFunc(func, field)					\
	LookupFunc(func, gsSup.field)

/*
 * This legacy version of CallFunc should only be used for functions that
 * used to be called directly in older versions of the DRI Support library.
 * Do not use this macro for functions that have only ever been available
 * via the PVRDRISupportInterface structure.
 * In summary, if you are modifying this file to support a new version of
 * the PVRDRISupportInterface structure, use CallFunc rather than this
 * macro.
 */
#define CallFuncLegacy(func, field, ...) 				\
	do { 								\
		LookupLegacyFunc(func, field);				\
									\
		if (gsSup.field != (void *)DumSupFunc)			\
			return gsSup.field(__VA_ARGS__);		\
	} while(0)

/*
 * Legacy version of DefineFunc_IsSupported.
 * See the comments for CallFuncLegacy.
 */
#define DefineFunc_IsSupportedLegacy(func, field) 			\
	bool func ## _IsSupported(void) 				\
	{ 								\
		LookupLegacyFunc(func, field);				\
									\
		return gsSup.field != (void *)DumSupFunc;		\
	}

/*
 * Calculate the size of a particular version of the PVRDRISupportInterface
 * structure from the name of the last field in that version of the
 * structure.
 */
#define PVRDRIInterfaceSize(field) \
				(offsetof(PVRDRISupportInterface, field) + \
				sizeof((PVRDRISupportInterface *)0)->field)

static void
CompatLock(void)
{
	int ret;

	ret = pthread_mutex_lock(&gsCompatLock);
	if (ret)
	{
		errorMessage("%s: Failed to lock mutex (%d)",
				 __func__, ret);
		abort();
	}
}

static void
CompatUnlock(void)
{
	int ret;

	ret = pthread_mutex_unlock(&gsCompatLock);
	if (ret)
	{
		errorMessage("%s: Failed to unlock mutex (%d)",
				 __func__, ret);
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
	if (handle)
	{
		__driUtilMessage("Loaded %s\n", path);
	}
	else
	{
		const char *error = dlerror();

		if (!error)
		{
			error = "unknown error";
		}

		errorMessage("%s: Couldn't load %s: %s\n",
		             __func__, path, error);
	}

	return handle;
}

static void
UnloadLib(void *handle, const char *name)
{
	if (!handle)
	{
		return;
	}

	/* Clear the error */
	(void) dlerror();

	if (dlclose(handle))
	{
		const char *error = dlerror();

		if (!error)
		{
			error = "unknown error";
		}

		errorMessage("%s: Couldn't unload %s: %s\n",
		             __func__, name, error);
	}
	else
	{
		__driUtilMessage("Unloaded %s\n", name);
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

static bool
RegisterCallbacksCompat(const PVRDRICallbacks *psCallbacks)
{
	void (*pfRegisterCallbacks)(PVRDRICallbacks *psCallbacks) = NULL;

	LookupFunc(PVRDRIRegisterCallbacks, pfRegisterCallbacks);

	if (pfRegisterCallbacks != (void *)&DumSupFunc)
	{
		PVRDRICallbacks sCallbacks = *psCallbacks;

		pfRegisterCallbacks(&sCallbacks);

		return true;
	}

	return false;
}

static void
CompatDeinit(void)
{
	UnloadSupportLib();
	memset(&gsSup, 0, sizeof(gsSup));
	guSupVer = 0;
}

bool
PVRDRICompatInit(const PVRDRICallbacks *psCallbacks, unsigned uVersion)
{
	bool res;

	bool (*pfRegisterVersionedCallbacks)(const PVRDRICallbacks *psCallbacks,
						    unsigned uVersion) = NULL;

	CompatLock();
	res = (giSupLibRef++ != 0);
	if (res)
	{
		goto Exit;
	}

	res = LoadSupportLib();
	if (!res)
	{
		goto Exit;
	}

	LookupFunc(PVRDRIRegisterVersionedCallbacks, pfRegisterVersionedCallbacks);

	if (pfRegisterVersionedCallbacks != (void *)&DumSupFunc)
	{
		res = pfRegisterVersionedCallbacks(psCallbacks, uVersion);
	}
	else
	{
		res = RegisterCallbacksCompat(psCallbacks);
	}
Exit:
	if (!res)
	{
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
	{
		CompatDeinit();
	}

	CompatUnlock();
}

bool
PVRDRIRegisterSupportInterfaceV1(const PVRDRISupportInterface *psInterface,
				 unsigned uVersion)
{
	size_t uSize;

	/* The "default" case should be associated with the latest version */
	switch (uVersion)
	{
		default:
		case 2:
			uSize = PVRDRIInterfaceSize(GetFenceFd);
			break;
		case 1:
			uSize = PVRDRIInterfaceSize(CreateDrawableWithConfig);
			break;
		case 0:
			uSize = PVRDRIInterfaceSize(QueryModifiers);
			break;
	}

	memcpy(&gsSup, psInterface, uSize);

	guSupVer = uVersion;

	return true;
}

PVRDRIDeviceType
PVRDRIGetDeviceTypeFromFd(int iFd)
{
	CallFuncLegacy(PVRDRIGetDeviceTypeFromFd,
		       GetDeviceTypeFromFd,
		       iFd);

	return PVRDRI_DEVICE_TYPE_INVALID;
}


bool
PVRDRIIsFirstScreen(PVRDRIScreenImpl *psScreenImpl)
{
	CallFuncLegacy(PVRDRIIsFirstScreen,
		       IsFirstScreen,
		       psScreenImpl);

	return false;
}


uint32_t
PVRDRIPixFmtGetDepth(IMG_PIXFMT eFmt)
{
	CallFuncLegacy(PVRDRIPixFmtGetDepth,
		       PixFmtGetDepth,
		       eFmt);

	return 0;
}

uint32_t
PVRDRIPixFmtGetBPP(IMG_PIXFMT eFmt)
{
	CallFuncLegacy(PVRDRIPixFmtGetBPP,
		       PixFmtGetBPP,
		       eFmt);

	return 0;
}

uint32_t
PVRDRIPixFmtGetBlockSize(IMG_PIXFMT eFmt)
{
	CallFuncLegacy(PVRDRIPixFmtGetBlockSize,
		       PixFmtGetBlockSize,
		       eFmt);

	return 0;
}

PVRDRIScreenImpl *
PVRDRICreateScreenImpl(int iFd)
{
	CallFuncLegacy(PVRDRICreateScreenImpl,
		       CreateScreen,
		       iFd);

	return NULL;
}

void
PVRDRIDestroyScreenImpl(PVRDRIScreenImpl *psScreenImpl)
{
	CallFuncLegacy(PVRDRIDestroyScreenImpl,
		       DestroyScreen,
		       psScreenImpl);
}

int
PVRDRIAPIVersion(PVRDRIAPIType eAPI,
		 PVRDRIAPISubType eAPISub,
		 PVRDRIScreenImpl *psScreenImpl)
{
	CallFuncLegacy(PVRDRIAPIVersion,
		       APIVersion,
		       eAPI,
		       eAPISub,
		       psScreenImpl);

	return 0;
}

void *
PVRDRIEGLGetLibHandle(PVRDRIAPIType eAPI,
		      PVRDRIScreenImpl *psScreenImpl)
{
	CallFuncLegacy(PVRDRIEGLGetLibHandle,
		       EGLGetLibHandle,
		       eAPI,
		       psScreenImpl);

	return NULL;
}

PVRDRIGLAPIProc
PVRDRIEGLGetProcAddress(PVRDRIAPIType eAPI,
			PVRDRIScreenImpl *psScreenImpl,
			const char *psProcName)
{
	CallFuncLegacy(PVRDRIEGLGetProcAddress,
		       EGLGetProcAddress,
		       eAPI,
		       psScreenImpl,
		       psProcName);

	return (PVRDRIGLAPIProc)NULL;
}

bool
PVRDRIEGLFlushBuffers(PVRDRIAPIType eAPI,
		      PVRDRIScreenImpl *psScreenImpl,
		      PVRDRIContextImpl *psContextImpl,
		      PVRDRIDrawableImpl *psDrawableImpl,
		      bool bFlushAllSurfaces,
		      bool bSwapBuffers,
		      bool bWaitForHW)
{
	CallFuncLegacy(PVRDRIEGLFlushBuffers,
		       EGLFlushBuffers,
		       eAPI,
		       psScreenImpl,
		       psContextImpl,
		       psDrawableImpl,
		       bFlushAllSurfaces,
		       bSwapBuffers,
		       bWaitForHW);

	return false;
}

void
PVRDRIEGLMarkRendersurfaceInvalid(PVRDRIAPIType eAPI,
				  PVRDRIScreenImpl *psScreenImpl,
				  PVRDRIContextImpl *psContextImpl)
{
	CallFuncLegacy(PVRDRIEGLMarkRendersurfaceInvalid,
		       EGLMarkRendersurfaceInvalid,
		       eAPI,
		       psScreenImpl,
		       psContextImpl);
}

static inline void PVRDRIConfigFromMesa(PVRDRIConfigInfo *psConfigInfo,
					const struct gl_config *psGLMode)
{
	memset(psConfigInfo, 0, sizeof(*psConfigInfo));

	if (psGLMode)
	{
		psConfigInfo->samples           = psGLMode->samples;
		psConfigInfo->redBits           = psGLMode->redBits;
		psConfigInfo->greenBits         = psGLMode->greenBits;
		psConfigInfo->blueBits          = psGLMode->blueBits;
		psConfigInfo->alphaBits         = psGLMode->alphaBits;
		psConfigInfo->rgbBits           = psGLMode->rgbBits;
		psConfigInfo->depthBits         = psGLMode->depthBits;
		psConfigInfo->stencilBits       = psGLMode->stencilBits;
		psConfigInfo->doubleBufferMode  = psGLMode->doubleBufferMode;

		psConfigInfo->sampleBuffers     = psGLMode->sampleBuffers;
		psConfigInfo->bindToTextureRgb  = psGLMode->bindToTextureRgb;
		psConfigInfo->bindToTextureRgba = psGLMode->bindToTextureRgba;
	}
}

unsigned
PVRDRISupportCreateContext(PVRDRIScreenImpl *psScreenImpl,
			   PVRDRIContextImpl *psSharedContextImpl,
			   PVRDRIConfig *psConfig,
			   PVRDRIAPIType eAPI,
			   PVRDRIAPISubType eAPISub,
			   unsigned uMajorVersion,
			   unsigned uMinorVersion,
			   uint32_t uFlags,
			   bool bNotifyReset,
			   unsigned uPriority,
			   PVRDRIContextImpl **ppsContextImpl)
{
	PVRDRIConfigInfo sConfigInfo;

	CallFunc(CreateContextV1, 1,
		 psScreenImpl, psSharedContextImpl, psConfig, eAPI, eAPISub,
		 uMajorVersion, uMinorVersion, uFlags, bNotifyReset, uPriority,
		 ppsContextImpl);

	PVRDRIConfigFromMesa(&sConfigInfo, &psConfig->sGLMode);

	CallFuncLegacy(PVRDRICreateContextImpl,
		       CreateContext,
		       ppsContextImpl,
		       eAPI,
		       eAPISub,
		       psScreenImpl,
		       &sConfigInfo,
		       uMajorVersion,
		       uMinorVersion,
		       uFlags,
		       bNotifyReset,
		       uPriority,
		       psSharedContextImpl);

	return PVRDRI_CONTEXT_ERROR_BAD_API;
}

void
PVRDRIDestroyContextImpl(PVRDRIContextImpl *psContextImpl,
			 PVRDRIAPIType eAPI,
			 PVRDRIScreenImpl *psScreenImpl)
{
	CallFuncLegacy(PVRDRIDestroyContextImpl,
		       DestroyContext,
		       psContextImpl,
		       eAPI,
		       psScreenImpl);
}

bool
PVRDRIMakeCurrentGC(PVRDRIAPIType eAPI,
		    PVRDRIScreenImpl *psScreenImpl,
		    PVRDRIContextImpl *psContextImpl,
		    PVRDRIDrawableImpl *psWriteImpl,
		    PVRDRIDrawableImpl *psReadImpl)
{
	CallFuncLegacy(PVRDRIMakeCurrentGC,
		       MakeCurrentGC,
		       eAPI,
		       psScreenImpl,
		       psContextImpl,
		       psWriteImpl,
		       psReadImpl);

	return false;
}

void
PVRDRIMakeUnCurrentGC(PVRDRIAPIType eAPI,
		      PVRDRIScreenImpl *psScreenImpl)
{
	CallFuncLegacy(PVRDRIMakeUnCurrentGC,
		       MakeUnCurrentGC,
		       eAPI,
		       psScreenImpl);
}

unsigned
PVRDRIGetImageSource(PVRDRIAPIType eAPI,
		     PVRDRIScreenImpl *psScreenImpl,
		     PVRDRIContextImpl *psContextImpl,
		     uint32_t  uiTarget,
		     uintptr_t uiBuffer,
		     uint32_t  uiLevel,
		     IMGEGLImage *psEGLImage)
{
	CallFuncLegacy(PVRDRIGetImageSource,
		       GetImageSource,
		       eAPI,
		       psScreenImpl,
		       psContextImpl,
		       uiTarget,
		       uiBuffer,
		       uiLevel,
		       psEGLImage);

	return PVRDRI_IMAGE_ERROR_BAD_MATCH;
}

bool
PVRDRI2BindTexImage(PVRDRIAPIType eAPI,
		    PVRDRIScreenImpl *psScreenImpl,
		    PVRDRIContextImpl *psContextImpl,
		    PVRDRIDrawableImpl *psDrawableImpl)
{
	CallFuncLegacy(PVRDRI2BindTexImage,
		       BindTexImage,
		       eAPI,
		       psScreenImpl,
		       psContextImpl,
		       psDrawableImpl);

	return false;
}

void
PVRDRI2ReleaseTexImage(PVRDRIAPIType eAPI,
		       PVRDRIScreenImpl *psScreenImpl,
		       PVRDRIContextImpl *psContextImpl,
		       PVRDRIDrawableImpl *psDrawableImpl)
{
	CallFuncLegacy(PVRDRI2ReleaseTexImage,
		       ReleaseTexImage,
		       eAPI,
		       psScreenImpl,
		       psContextImpl,
		       psDrawableImpl);
}

PVRDRIDrawableImpl *
PVRDRICreateDrawableImpl(PVRDRIDrawable *psPVRDrawable)
{
	CallFuncLegacy(PVRDRICreateDrawableImpl,
		       CreateDrawable,
		       psPVRDrawable);

	return NULL;
}

PVRDRIDrawableImpl *
PVRDRISupportCreateDrawable(PVRDRIDrawable *psPVRDrawable,
			    PVRDRIConfig *psConfig)
{
	PVRDRIDrawableImpl *psDrawableImpl;
	PVRDRIConfigInfo sConfigInfo;
	IMG_PIXFMT ePixelFormat;

	CallFunc(CreateDrawableWithConfig, 1,
		 psPVRDrawable,
		 psConfig);

	ePixelFormat = PVRDRIGetPixelFormat(&psConfig->sGLMode);
	if (ePixelFormat == IMG_PIXFMT_UNKNOWN)
	{
		__driUtilMessage("%s: Couldn't work out pixel format", __func__);
		return NULL;
	}

	psDrawableImpl = PVRDRICreateDrawableImpl(psPVRDrawable);
	if (!psDrawableImpl)
	{
		return NULL;
	}

	PVRDRIConfigFromMesa(&sConfigInfo, &psConfig->sGLMode);
	if (!PVRDRIEGLDrawableConfigFromGLMode(psDrawableImpl,
	                                       &sConfigInfo,
	                                       psConfig->iSupportedAPIs,
	                                       ePixelFormat))
	{
		__driUtilMessage("%s: Couldn't derive EGL config", __func__);
		PVRDRIDestroyDrawableImpl(psDrawableImpl);
		return NULL;
	}

	return psDrawableImpl;
}

void
PVRDRIDestroyDrawableImpl(PVRDRIDrawableImpl *psScreenImpl)
{
	CallFuncLegacy(PVRDRIDestroyDrawableImpl,
		       DestroyDrawable,
		       psScreenImpl);
}

bool
PVREGLDrawableCreate(PVRDRIScreenImpl *psScreenImpl,
		     PVRDRIDrawableImpl *psDrawableImpl)
{
	CallFuncLegacy(PVREGLDrawableCreate,
		       EGLDrawableCreate,
		       psScreenImpl,
		       psDrawableImpl);

	return false;
}

bool
PVREGLDrawableRecreate(PVRDRIScreenImpl *psScreenImpl,
		       PVRDRIDrawableImpl *psDrawableImpl)
{
	CallFuncLegacy(PVREGLDrawableRecreate,
		       EGLDrawableRecreate,
		       psScreenImpl,
		       psDrawableImpl);

	return false;
}

bool
PVREGLDrawableDestroy(PVRDRIScreenImpl *psScreenImpl,
		      PVRDRIDrawableImpl *psDrawableImpl)
{
	CallFuncLegacy(PVREGLDrawableDestroy,
		       EGLDrawableDestroy,
		       psScreenImpl,
		       psDrawableImpl);

	return false;
}

void
PVREGLDrawableDestroyConfig(PVRDRIDrawableImpl *psDrawableImpl)
{
	CallFuncLegacy(PVREGLDrawableDestroyConfig,
		       EGLDrawableDestroyConfig,
		       psDrawableImpl);
}

PVRDRIBufferImpl *
PVRDRIBufferCreate(PVRDRIScreenImpl *psScreenImpl,
		   int iWidth,
		   int iHeight,
		   unsigned int uiBpp,
		   unsigned int uiUseFlags,
		   unsigned int *puiStride)
{
	CallFuncLegacy(PVRDRIBufferCreate,
		       BufferCreate,
		       psScreenImpl,
		       iWidth,
		       iHeight,
		       uiBpp,
		       uiUseFlags,
		       puiStride);

	return NULL;
}

DefineFunc_IsSupportedLegacy(PVRDRIBufferCreateWithModifiers,
			     BufferCreateWithModifiers)

PVRDRIBufferImpl *
PVRDRIBufferCreateWithModifiers(PVRDRIScreenImpl *psScreenImpl,
				int iWidth,
				int iHeight,
				int format,
				IMG_PIXFMT eIMGPixelFormat,
				const uint64_t *puiModifiers,
				unsigned int uiModifierCount,
				unsigned int *puiStride)
{
	CallFuncLegacy(PVRDRIBufferCreateWithModifiers,
		       BufferCreateWithModifiers,
		       psScreenImpl,
		       iWidth,
		       iHeight,
		       format,
		       eIMGPixelFormat,
		       puiModifiers,
		       uiModifierCount,
		       puiStride);

	return NULL;
}

PVRDRIBufferImpl *
PVRDRIBufferCreateFromNames(PVRDRIScreenImpl *psScreenImpl,
			    int iWidth,
			    int iHeight,
			    unsigned uiNumPlanes,
			    const int *piName,
			    const int *piStride,
			    const int *piOffset,
			    const unsigned int *puiWidthShift,
			    const unsigned int *puiHeightShift)
{
	CallFuncLegacy(PVRDRIBufferCreateFromNames,
		       BufferCreateFromNames,
		       psScreenImpl,
		       iWidth,
		       iHeight,
		       uiNumPlanes,
		       piName,
		       piStride,
		       piOffset,
		       puiWidthShift,
		       puiHeightShift);

	return NULL;
}

PVRDRIBufferImpl *
PVRDRIBufferCreateFromName(PVRDRIScreenImpl *psScreenImpl,
			   int iName,
			   int iWidth,
			   int iHeight,
			   int iStride,
			   int iOffset)
{
	CallFuncLegacy(PVRDRIBufferCreateFromName,
		       BufferCreateFromName,
		       psScreenImpl,
		       iName,
		       iWidth,
		       iHeight,
		       iStride,
		       iOffset);

	return NULL;
}

PVRDRIBufferImpl *
PVRDRIBufferCreateFromFds(PVRDRIScreenImpl *psScreenImpl,
			  int iWidth,
			  int iHeight,
			  unsigned uiNumPlanes,
			  const int *piFd,
			  const int *piStride,
			  const int *piOffset,
			  const unsigned int *puiWidthShift,
			  const unsigned int *puiHeightShift)
{
	CallFuncLegacy(PVRDRIBufferCreateFromFds,
		       BufferCreateFromFds,
		       psScreenImpl,
		       iWidth,
		       iHeight,
		       uiNumPlanes,
		       piFd,
		       piStride,
		       piOffset,
		       puiWidthShift,
		       puiHeightShift);

	return NULL;
}

DefineFunc_IsSupportedLegacy(PVRDRIBufferCreateFromFdsWithModifier,
			     BufferCreateFromFdsWithModifier)

PVRDRIBufferImpl *
PVRDRIBufferCreateFromFdsWithModifier(PVRDRIScreenImpl *psScreenImpl,
				      int iWidth,
				      int iHeight,
				      uint64_t uiModifier,
				      unsigned uiNumPlanes,
				      const int *piFd,
				      const int *piStride,
				      const int *piOffset,
				      const unsigned int *puiWidthShift,
				      const unsigned int *puiHeightShift)
{
	CallFuncLegacy(PVRDRIBufferCreateFromFdsWithModifier,
		       BufferCreateFromFdsWithModifier,
		       psScreenImpl,
		       iWidth,
		       iHeight,
		       uiModifier,
		       uiNumPlanes,
		       piFd,
		       piStride,
		       piOffset,
		       puiWidthShift,
		       puiHeightShift);

	if (uiModifier == DRM_FORMAT_MOD_INVALID)
	{
		CallFuncLegacy(PVRDRIBufferCreateFromFds,
			       BufferCreateFromFds,
			       psScreenImpl,
			       iWidth,
			       iHeight,
			       uiNumPlanes,
			       piFd,
			       piStride,
			       piOffset,
			       puiWidthShift,
			       puiHeightShift);
	}

	return NULL;
}

PVRDRIBufferImpl *
PVRDRISubBufferCreate(PVRDRIScreenImpl *psScreen,
		      PVRDRIBufferImpl *psParentBuffer,
		      int plane)
{
	CallFuncLegacy(PVRDRISubBufferCreate,
		       SubBufferCreate,
		       psScreen,
		       psParentBuffer,
		       plane);

	return NULL;
}

void
PVRDRIBufferDestroy(PVRDRIBufferImpl *psBuffer)
{
	CallFuncLegacy(PVRDRIBufferDestroy,
		       BufferDestroy,
		       psBuffer);
}

int
PVRDRIBufferGetFd(PVRDRIBufferImpl *psBuffer)
{
	CallFuncLegacy(PVRDRIBufferGetFd,
		       BufferGetFd,
		       psBuffer);

	return -1;
}

int
PVRDRIBufferGetHandle(PVRDRIBufferImpl *psBuffer)
{
	CallFuncLegacy(PVRDRIBufferGetHandle,
		       BufferGetHandle,
		       psBuffer);

	return 0;
}

uint64_t
PVRDRIBufferGetModifier(PVRDRIBufferImpl *psBuffer)
{
	CallFuncLegacy(PVRDRIBufferGetModifier,
		       BufferGetModifier,
		       psBuffer);

	return DRM_FORMAT_MOD_INVALID;
}

int
PVRDRIBufferGetName(PVRDRIBufferImpl *psBuffer)
{
	CallFuncLegacy(PVRDRIBufferGetName,
		       BufferGetName,
		       psBuffer);

	return 0;
}

DefineFunc_IsSupportedLegacy(PVRDRIBufferGetOffset, BufferGetOffset)

int
PVRDRIBufferGetOffset(PVRDRIBufferImpl *psBuffer)
{
	CallFuncLegacy(PVRDRIBufferGetOffset,
		       BufferGetOffset,
		       psBuffer);

	return 0;
}

IMGEGLImage *
PVRDRIEGLImageCreate(void)
{
	CallFuncLegacy(PVRDRIEGLImageCreate,
		       EGLImageCreate);

	return NULL;
}

IMGEGLImage *
PVRDRIEGLImageCreateFromBuffer(int iWidth,
			       int iHeight,
			       int iStride,
			       IMG_PIXFMT ePixelFormat,
			       IMG_YUV_COLORSPACE eColourSpace,
			       IMG_YUV_CHROMA_INTERP eChromaUInterp,
			       IMG_YUV_CHROMA_INTERP eChromaVInterp,
			       PVRDRIBufferImpl *psBuffer)
{
	CallFuncLegacy(PVRDRIEGLImageCreateFromBuffer,
		       EGLImageCreateFromBuffer,
		       iWidth,
		       iHeight,
		       iStride,
		       ePixelFormat,
		       eColourSpace,
		       eChromaUInterp,
		       eChromaVInterp,
		       psBuffer);

	return NULL;
}

IMGEGLImage *
PVRDRIEGLImageCreateFromSubBuffer(IMG_PIXFMT ePixelFormat,
				  PVRDRIBufferImpl *psSubBuffer)
{
	CallFuncLegacy(PVRDRIEGLImageCreateFromSubBuffer,
		       EGLImageCreateFromSubBuffer,
		       ePixelFormat,
		       psSubBuffer);

	return NULL;
}

IMGEGLImage *
PVRDRIEGLImageDup(IMGEGLImage *psIn)
{
	CallFuncLegacy(PVRDRIEGLImageDup,
		       EGLImageDup,
		       psIn);

	return NULL;
}

void
PVRDRIEGLImageSetCallbackData(IMGEGLImage *psEGLImage, __DRIimage *image)
{
	CallFuncLegacy(PVRDRIEGLImageSetCallbackData,
		       EGLImageSetCallbackData,
		       psEGLImage,
		       image);
}

void
PVRDRIEGLImageDestroyExternal(PVRDRIScreenImpl *psScreenImpl,
			      IMGEGLImage *psEGLImage,
			      PVRDRIEGLImageType eglImageType)
{
	CallFuncLegacy(PVRDRIEGLImageDestroyExternal,
		       EGLImageDestroyExternal,
		       psScreenImpl,
		       psEGLImage,
		       eglImageType);
}

void
PVRDRIEGLImageFree(IMGEGLImage *psEGLImage)
{
	CallFuncLegacy(PVRDRIEGLImageFree,
		       EGLImageFree,
		       psEGLImage);
}

void
PVRDRIEGLImageGetAttribs(IMGEGLImage *psEGLImage,
			 PVRDRIBufferAttribs *psAttribs)
{
	CallFuncLegacy(PVRDRIEGLImageGetAttribs,
		       EGLImageGetAttribs,
		       psEGLImage,
		       psAttribs);
}

void *
PVRDRICreateFenceImpl(PVRDRIAPIType eAPI,
		      PVRDRIScreenImpl *psScreenImpl,
		      PVRDRIContextImpl *psContextImpl)
{
	CallFuncLegacy(PVRDRICreateFenceImpl,
		       CreateFence,
		       eAPI,
		       psScreenImpl,
		       psContextImpl);

	return NULL;
}

void *
PVRDRICreateFenceFdImpl(PVRDRIAPIType eAPI,
		      PVRDRIScreenImpl *psScreenImpl,
		      PVRDRIContextImpl *psContextImpl,
		      int iFd)
{
	CallFunc(CreateFenceFd, 2,
		 eAPI,
	         psScreenImpl,
		 psContextImpl,
		 iFd);

	return NULL;
}

unsigned PVRDRIGetFenceCapabilitiesImpl(PVRDRIScreenImpl *psScreenImpl)
{
	CallFunc(GetFenceCapabilities, 2,
		 psScreenImpl);

	return 0;
}

int PVRDRIGetFenceFdImpl(void *psDRIFence)
{
	CallFunc(GetFenceFd, 2,
		 psDRIFence);

	return -1;
}

void
PVRDRIDestroyFenceImpl(void *psDRIFence)
{
	CallFuncLegacy(PVRDRIDestroyFenceImpl,
		       DestroyFence,
		       psDRIFence);
}

bool
PVRDRIClientWaitSyncImpl(PVRDRIAPIType eAPI,
			 PVRDRIContextImpl *psContextImpl,
			 void *psDRIFence,
			 bool bFlushCommands,
			 bool bTimeout,
			 uint64_t uiTimeout)
{
	CallFuncLegacy(PVRDRIClientWaitSyncImpl,
		       ClientWaitSync,
		       eAPI,
		       psContextImpl,
		       psDRIFence,
		       bFlushCommands,
		       bTimeout,
		       uiTimeout);

	return false;
}

bool
PVRDRIServerWaitSyncImpl(PVRDRIAPIType eAPI,
			 PVRDRIContextImpl *psContextImpl,
			 void *psDRIFence)
{
	CallFuncLegacy(PVRDRIServerWaitSyncImpl,
		       ServerWaitSync,
		       eAPI,
		       psContextImpl,
		       psDRIFence);

	return false;
}

void
PVRDRIDestroyFencesImpl(PVRDRIScreenImpl *psScreenImpl)
{
	CallFuncLegacy(PVRDRIDestroyFencesImpl,
		       DestroyFences,
		       psScreenImpl);
}

bool
PVRDRIEGLDrawableConfigFromGLMode(PVRDRIDrawableImpl *psPVRDrawable,
				  PVRDRIConfigInfo *psConfigInfo,
				  int supportedAPIs,
				  IMG_PIXFMT ePixFmt)
{
	CallFuncLegacy(PVRDRIEGLDrawableConfigFromGLMode,
		       EGLDrawableConfigFromGLMode,
		       psPVRDrawable,
		       psConfigInfo,
		       supportedAPIs,
		       ePixFmt);

	return false;
}

DefineFunc_IsSupportedLegacy(PVRDRIBlitEGLImage, BlitEGLImage)

bool
PVRDRIBlitEGLImage(PVRDRIScreenImpl *psScreenImpl,
		   PVRDRIContextImpl *psContextImpl,
		   IMGEGLImage *psDstImage,
		   PVRDRIBufferImpl *psDstBuffer,
		   IMGEGLImage *psSrcImage,
		   PVRDRIBufferImpl *psSrcBuffer,
		   int iDstX, int iDstY,
		   int iDstWidth, int iDstHeight,
		   int iSrcX, int iSrcY,
		   int iSrcWidth, int iSrcHeight,
		   int iFlushFlag)
{
	CallFuncLegacy(PVRDRIBlitEGLImage,
		       BlitEGLImage,
		       psScreenImpl,
		       psContextImpl,
		       psDstImage, psDstBuffer,
		       psSrcImage, psSrcBuffer,
		       iDstX, iDstY,
		       iDstWidth, iDstHeight,
		       iSrcX, iSrcY,
		       iSrcWidth, iSrcHeight,
		       iFlushFlag);

	return false;
}

DefineFunc_IsSupportedLegacy(PVRDRIMapEGLImage, MapEGLImage)

void *
PVRDRIMapEGLImage(PVRDRIScreenImpl *psScreenImpl,
		  PVRDRIContextImpl *psContextImpl,
		  IMGEGLImage *psImage,
		  PVRDRIBufferImpl *psBuffer,
		  int iX, int iY, int iWidth, int iHeight,
		  unsigned uiFlags, int *iStride, void **ppvData)
{
	CallFuncLegacy(PVRDRIMapEGLImage,
		       MapEGLImage,
		       psScreenImpl,
		       psContextImpl,
		       psImage, psBuffer,
		       iX, iY, iWidth, iHeight,
		       uiFlags, iStride, ppvData);

	return false;
}

bool
PVRDRIUnmapEGLImage(PVRDRIScreenImpl *psScreenImpl,
		    PVRDRIContextImpl *psContextImpl,
		    IMGEGLImage *psImage,
		    PVRDRIBufferImpl *psBuffer,
		    void *pvData)
{
	CallFuncLegacy(PVRDRIUnmapEGLImage,
		       UnmapEGLImage,
		       psScreenImpl,
		       psContextImpl,
		       psImage, psBuffer,
		       pvData);

	return false;
}

bool
PVRDRIMesaFormatSupported(unsigned fmt)
{
	CallFuncLegacy(PVRDRIMesaFormatSupported,
		       MesaFormatSupported,
		       fmt);

	return false;
}

unsigned
PVRDRIDepthStencilBitArraySize(void)
{
	CallFuncLegacy(PVRDRIDepthStencilBitArraySize,
		       DepthStencilBitArraySize);

	return 0;
}

const uint8_t *
PVRDRIDepthBitsArray(void)
{
	CallFuncLegacy(PVRDRIDepthBitsArray,
		       DepthBitsArray);

	return NULL;
}

const uint8_t *
PVRDRIStencilBitsArray(void)
{
	CallFuncLegacy(PVRDRIStencilBitsArray,
		       StencilBitsArray);

	return NULL;
}

unsigned
PVRDRIMSAABitArraySize(void)
{
	CallFuncLegacy(PVRDRIMSAABitArraySize,
		       MSAABitArraySize);

	return 0;
}

const uint8_t *
PVRDRIMSAABitsArray(void)
{
	CallFuncLegacy(PVRDRIMSAABitsArray,
		       MSAABitsArray);

	return NULL;
}

uint32_t
PVRDRIMaxPBufferWidth(void)
{
	CallFuncLegacy(PVRDRIMaxPBufferWidth,
		       MaxPBufferWidth);

	return 0;
}

uint32_t
PVRDRIMaxPBufferHeight(void)
{
	CallFuncLegacy(PVRDRIMaxPBufferHeight,
		       MaxPBufferHeight);

	return 0;
}


unsigned
PVRDRIGetNumAPIFuncs(PVRDRIAPIType eAPI)
{
	CallFuncLegacy(PVRDRIGetNumAPIFuncs,
		       GetNumAPIFuncs,
		       eAPI);

	return 0;
}

const char *
PVRDRIGetAPIFunc(PVRDRIAPIType eAPI, unsigned index)
{
	CallFuncLegacy(PVRDRIGetAPIFunc,
		       GetAPIFunc,
		       eAPI,
		       index);

	return NULL;
}

int
PVRDRIQuerySupportedFormats(PVRDRIScreenImpl *psScreenImpl,
			    unsigned uNumFormats,
			    const int *iFormats,
			    const IMG_PIXFMT *peImgFormats,
			    bool *bSupported)
{
	CallFuncLegacy(PVRDRIQuerySupportedFormats,
		       QuerySupportedFormats,
		       psScreenImpl,
		       uNumFormats,
		       iFormats,
		       peImgFormats,
		       bSupported);

	return -1;
}

int
PVRDRIQueryModifiers(PVRDRIScreenImpl *psScreenImpl,
		     int iFormat,
		     IMG_PIXFMT eImgFormat,
		     uint64_t *puModifiers,
		     unsigned *puExternalOnly)
{
	CallFuncLegacy(PVRDRIQueryModifiers,
		       QueryModifiers,
		       psScreenImpl,
		       iFormat,
		       eImgFormat,
		       puModifiers,
		       puExternalOnly);

	return -1;
}
