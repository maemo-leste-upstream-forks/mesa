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

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <xf86drm.h>

#include "EGL/egl.h"
#include "EGL/eglext.h"

#include "util/u_atomic.h"
#include "dri_util.h"

#include "pvrdri.h"
#include "pvrimage.h"
#include "pvr_object_cache.h"

#include "pvrmesa.h"

#define PVR_IMAGE_LOADER_VER_MIN 1
#define PVR_DRI2_LOADER_VER_MIN 3

#define PVRDRI_FLUSH_WAIT_FOR_HW        (1U << 0)
#define PVRDRI_FLUSH_NEW_EXTERNAL_FRAME (1U << 1)
#define PVRDRI_FLUSH_ALL_SURFACES       (1U << 2)

typedef struct PVRBufferRec
{
	__DRIbuffer sDRIBuffer;
	PVRDRIBufferImpl *psImpl;
} PVRBuffer;

/* We need to know the current screen in order to lookup EGL images. */
static __thread PVRDRIScreen *gpsPVRScreen;

/*************************************************************************/ /*!
 Local functions
*/ /**************************************************************************/

static bool PVRLoaderIsSupported(__DRIscreen *psDRIScreen)
{
	if (psDRIScreen->image.loader)
	{
		if (psDRIScreen->image.loader->base.version < PVR_IMAGE_LOADER_VER_MIN)
		{
			__driUtilMessage("%s: Image loader extension version %d but need %d",
					 __func__,
					 psDRIScreen->image.loader->base.version,
					 PVR_IMAGE_LOADER_VER_MIN);
			return false;
		}
		else if (!psDRIScreen->image.loader->getBuffers)
		{
			__driUtilMessage("%s: Image loader extension missing support for getBuffers",
					 __func__);
			return false;
		}
	}
	else
	{
		if (!psDRIScreen->dri2.loader)
		{
			__driUtilMessage("%s: Missing required loader extension (need either the image or DRI2 loader extension)",
					 __func__);
			return false;
		}
		else if (psDRIScreen->dri2.loader->base.version < PVR_DRI2_LOADER_VER_MIN)
		{
			__driUtilMessage("%s: DRI2 loader extension version %d but need %d",
					 __func__,
					 psDRIScreen->dri2.loader->base.version,
					 PVR_DRI2_LOADER_VER_MIN);
			return false;
		}
		else if (!psDRIScreen->dri2.loader->getBuffersWithFormat)
		{
			__driUtilMessage("%s: DRI2 loader extension missing support for getBuffersWithFormat",
					 __func__);
			return false;
		}
	}

	return true;
}

static inline bool PVRDRIFlushBuffers(PVRDRIContext *psPVRContext,
				      PVRDRIDrawable *psPVRDrawable,
				      bool bFlushAllSurfaces,
				      bool bSwapBuffers,
				      bool bWaitForHW)
{
	return PVRDRIEGLFlushBuffers(psPVRContext->eAPI,
				     psPVRContext->psPVRScreen->psImpl,
				     psPVRContext->psImpl,
				     psPVRDrawable ? psPVRDrawable->psImpl : NULL,
				     bFlushAllSurfaces,
				     bSwapBuffers,
				     bWaitForHW);
}

static inline bool PVRDRIFlushBuffersAndWait(PVRDRIContext *psPVRContext)
{
	return PVRDRIFlushBuffers(psPVRContext, NULL, true, false, true);
}

static inline bool PVRDRIFlushContextForSwapNoWait(PVRDRIContext *psPVRContext,
						   PVRDRIDrawable *psPVRDrawable)
{
	return PVRDRIFlushBuffers(psPVRContext, psPVRDrawable, false, true, false);
}

static inline bool PVRDRIFlushBuffersForSwapAndWait(PVRDRIContext *psPVRContext)
{
	return PVRDRIFlushBuffers(psPVRContext, NULL, true, true, true);
}

static void PVRDRIFlushDrawable(PVRDRIDrawable *psPVRDrawable,
				bool bSwapBuffers,
				PVRDRIContext *psPVRSwapContext)
{
	PVRQElem *psQElem = psPVRDrawable->sPVRContextHead.pvForw;

	while (psQElem != &psPVRDrawable->sPVRContextHead)
	{
		PVRDRIContext *psPVRContext = PVRQ_CONTAINER_OF(psQElem, PVRDRIContext, sQElem);

		if (bSwapBuffers && (psPVRContext == psPVRSwapContext || psPVRSwapContext == NULL))
		{
			(void) PVRDRIFlushBuffersForSwapAndWait(psPVRContext);
		}
		else
		{
			(void) PVRDRIFlushBuffersAndWait(psPVRContext);
		}

		psQElem = psPVRContext->sQElem.pvForw;
	}
}

static bool PVRDRICacheFlushSurfaces(bool bSwapBuffers,
				     PVRDRIContext *psPVRSwapContext,
				     PVRDRIDrawable *psPVRDrawable)
{
	if (PVRQIsEmpty(&psPVRDrawable->sCacheFlushHead))
	{
		return false;
	}

	PVRDRIFlushDrawable(psPVRDrawable, bSwapBuffers, psPVRSwapContext);

	while (!PVRQIsEmpty(&psPVRDrawable->sCacheFlushHead))
	{
		PVRDRIBuffer *psPVRBuffer = PVRQ_CONTAINER_OF(psPVRDrawable->sCacheFlushHead.pvForw,
							      PVRDRIBuffer,
							      sCacheFlushElem);

		PVRQDequeue(&psPVRBuffer->sCacheFlushElem);

		switch (psPVRBuffer->eBackingType)
		{
			case PVRDRI_BUFFER_BACKING_DRI2:
				PVRDRIBufferDestroy(psPVRBuffer->uBacking.sDRI2.psBuffer);
				break;
			case PVRDRI_BUFFER_BACKING_IMAGE:
				PVRDRIUnrefImage(psPVRBuffer->uBacking.sImage.psImage);
				break;
			default:
				assert(0);
				continue;
		}

		free(psPVRBuffer);
	}

	return true;
}

static void
PVRDRIFlushDrawableForSwapNoWait(PVRDRIDrawable *psPVRDrawable)
{
	PVRQElem *psQElem = psPVRDrawable->sPVRContextHead.pvForw;

	while (psQElem != &psPVRDrawable->sPVRContextHead)
	{
		PVRDRIContext *psPVRContext = PVRQ_CONTAINER_OF(psQElem, PVRDRIContext, sQElem);

		(void) PVRDRIFlushContextForSwapNoWait(psPVRContext, psPVRDrawable);

		psQElem = psPVRContext->sQElem.pvForw;
	}
}

bool PVRDRIFlushBuffersForSwap(PVRDRIContext *psPVRContext,
			       PVRDRIDrawable *psPVRDrawable)
{
	if (PVRDRICacheFlushSurfaces(true, psPVRContext, psPVRDrawable))
	{
		return true;
	}

	if (psPVRContext != NULL)
	{
		return PVRDRIFlushContextForSwapNoWait(psPVRContext, psPVRDrawable);
	}

	PVRDRIFlushDrawableForSwapNoWait(psPVRDrawable);

	return true;
}

static bool
PVRDRIFlushBuffersGC(PVRDRIContext *psPVRContext)
{
	if (psPVRContext->psPVRDrawable != NULL)
	{
		if (PVRDRICacheFlushSurfaces(false, NULL,
					     psPVRContext->psPVRDrawable))
		{
			return true;
		}
	}

	return PVRDRIFlushBuffersAndWait(psPVRContext);
}

static inline PVRDRIContextImpl *
getSharedContextImpl(void *pvSharedContextPrivate)
{
	if (pvSharedContextPrivate == NULL)
	{
		return NULL;
	}
	return ((PVRDRIContext *)pvSharedContextPrivate)->psImpl;
}


static void PVRDRIScreenAddReference(PVRDRIScreen *psPVRScreen)
{
	int iRefCount = p_atomic_inc_return(&psPVRScreen->iRefCount);
	(void)iRefCount;
	assert(iRefCount > 1);
}

static void PVRDRIScreenRemoveReference(PVRDRIScreen *psPVRScreen)
{
	int iRefCount = p_atomic_dec_return(&psPVRScreen->iRefCount);

	assert(iRefCount >= 0);

	if (iRefCount != 0)
	{
		return;
	}

	pvrdri_free_dispatch_tables(psPVRScreen);

	PVRDRIEGLFreeResources(psPVRScreen->psImpl);
	PVRDRIDestroyFencesImpl(psPVRScreen->psImpl);
	PVRDRIDestroyFormatInfo(psPVRScreen);
	PVRDRIDestroyScreenImpl(psPVRScreen->psImpl);
	PVRMutexDeinit(&psPVRScreen->sMutex);

	free(psPVRScreen);
}

static void PVRScreenPrintExtensions(__DRIscreen *psDRIScreen)
{
	/* Don't attempt to print anything if LIBGL_DEBUG isn't in the environment */
	if (getenv("LIBGL_DEBUG") == NULL)
	{
		return;
	}

	if (psDRIScreen->extensions)
	{
		const __DRIextension *psScreenExtensionVersionInfo = PVRDRIScreenExtensionVersionInfo();
		int i;
		int j;

		__driUtilMessage("Supported screen extensions:");

		for (i = 0; psDRIScreen->extensions[i]; i++)
		{
			for (j = 0; psScreenExtensionVersionInfo[j].name; j++)
			{
				if (strcmp(psDRIScreen->extensions[i]->name,
				           psScreenExtensionVersionInfo[j].name) == 0)
				{
					__driUtilMessage("\t%s (supported version: %u - max version: %u)",
							 psDRIScreen->extensions[i]->name,
							 psDRIScreen->extensions[i]->version,
							 psScreenExtensionVersionInfo[j].version);
					break;
				}
			}

			if (psScreenExtensionVersionInfo[j].name == NULL)
			{
				__driUtilMessage("\t%s (supported version: %u - max version: unknown)",
						 psDRIScreen->extensions[i]->name,
						 psDRIScreen->extensions[i]->version);
			}
		}
	}
	else
	{
		__driUtilMessage("No screen extensions found");
	}
}

/*************************************************************************/ /*!
 Mesa driver API functions
*/ /**************************************************************************/
static const __DRIconfig **PVRDRIInitScreen(__DRIscreen *psDRIScreen)
{
	PVRDRIScreen *psPVRScreen;
	const __DRIconfig **configs;
	const PVRDRICallbacks sDRICallbacks = {
		/* Version 0 callbacks */
		.DrawableRecreate            = PVRDRIDrawableRecreate,
		.DrawableGetParameters       = PVRDRIDrawableGetParameters,
		.ImageGetSharedType          = PVRDRIImageGetSharedType,
		.ImageGetSharedBuffer        = PVRDRIImageGetSharedBuffer,
		.ImageGetSharedEGLImage      = PVRDRIImageGetSharedEGLImage,
		.ImageGetEGLImage            = PVRDRIImageGetEGLImage,
		.ScreenGetDRIImage           = PVRDRIScreenGetDRIImage,
		.RefImage                    = PVRDRIRefImage,
		.UnrefImage                  = PVRDRIUnrefImage,
	};

	if (!PVRLoaderIsSupported(psDRIScreen))
	{
		return NULL;
	}

	if (!PVRDRICompatInit(&sDRICallbacks))
	{
		return NULL;
	}

	psPVRScreen = calloc(1, sizeof(*psPVRScreen));
	if (psPVRScreen == NULL)
	{
		__driUtilMessage("%s: Couldn't allocate PVRDRIScreen",
				 __func__);
		goto ErrorCompatDeinit;
	}

	DRIScreenPrivate(psDRIScreen) = psPVRScreen;
	psPVRScreen->psDRIScreen = psDRIScreen;

	if (!PVRMutexInit(&psPVRScreen->sMutex, PTHREAD_MUTEX_RECURSIVE))
	{
		__driUtilMessage("%s: Screen mutex initialisation failed",
				 __func__);
		goto ErrorScreenFree;
	}

	psPVRScreen->iRefCount = 1;
	psPVRScreen->bUseInvalidate = (psDRIScreen->dri2.useInvalidate != NULL);

	psDRIScreen->extensions = PVRDRIScreenExtensions();

	psPVRScreen->psImpl = PVRDRICreateScreenImpl(psDRIScreen->fd);

	if (psPVRScreen->psImpl == NULL)
	{
		goto ErrorScreenMutexDeinit;
	}

	if (!PVRDRIGetSupportedFormats(psPVRScreen))
	{
		goto ErrorScreenImplDeinit;
	}

	if (PVRDRIIsFirstScreen(psPVRScreen->psImpl))
	{
		psDRIScreen->max_gl_compat_version =
				PVRDRIAPIVersion(PVRDRI_API_GL,
						 PVRDRI_API_SUB_GL_COMPAT,
						 psPVRScreen->psImpl);
		psDRIScreen->max_gl_core_version =
				PVRDRIAPIVersion(PVRDRI_API_GL,
						 PVRDRI_API_SUB_GL_CORE,
						 psPVRScreen->psImpl);
	}

	psDRIScreen->max_gl_es1_version =
				PVRDRIAPIVersion(PVRDRI_API_GLES1,
						 PVRDRI_API_SUB_NONE,
						 psPVRScreen->psImpl);

	psDRIScreen->max_gl_es2_version =
				PVRDRIAPIVersion(PVRDRI_API_GLES2,
						 PVRDRI_API_SUB_NONE,
						 psPVRScreen->psImpl);

	configs = PVRDRICreateConfigs();

	if (configs == NULL)
	{
		__driUtilMessage("%s: No framebuffer configs", __func__);
		goto ErrorDestroyFormatInfo;
	}

	PVRScreenPrintExtensions(psDRIScreen);

	return configs;

ErrorDestroyFormatInfo:
	PVRDRIDestroyFormatInfo(psPVRScreen);

ErrorScreenImplDeinit:
	PVRDRIDestroyScreenImpl(psPVRScreen->psImpl);

ErrorScreenMutexDeinit:
	PVRMutexDeinit(&psPVRScreen->sMutex);

ErrorScreenFree:
	free(psPVRScreen);

ErrorCompatDeinit:
	PVRDRICompatDeinit();

	return NULL;
}

static void PVRDRIDestroyScreen(__DRIscreen *psDRIScreen)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(psDRIScreen);

#if defined(DEBUG)
	if (psPVRScreen->iBufferAlloc != 0 || psPVRScreen->iDrawableAlloc != 0 || psPVRScreen->iContextAlloc != 0)
	{
		errorMessage("%s: Outstanding allocations: Contexts: %d Drawables: %d Buffers: %d.",
						 __func__,
						 psPVRScreen->iContextAlloc,
						 psPVRScreen->iDrawableAlloc,
						 psPVRScreen->iBufferAlloc);

		if (psPVRScreen->iRefCount > 1)
		{
			errorMessage("%s: PVRDRIScreen resources will not be freed until its %d references are removed.",
							 __func__,
							 psPVRScreen->iRefCount - 1);
		}
	}
#endif

	PVRDRIScreenRemoveReference(psPVRScreen);

	PVRDRICompatDeinit();
}

unsigned PVRDRISupportCreateContext(PVRDRIScreenImpl *psScreenImpl,
				    PVRDRIContextImpl *psSharedContextImpl,
				    const struct gl_config *psGLMode,
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

	PVRDRIConfigFromMesa(&sConfigInfo, psGLMode);

	return PVRDRICreateContextImpl(ppsContextImpl,
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
}

static GLboolean PVRDRICreateContext(gl_api eMesaAPI,
                                     const struct gl_config *psGLMode,
                                     __DRIcontext *psDRIContext,
                                     const struct __DriverContextConfig *psCtxConfig,
                                     unsigned *puError,
                                     void *pvSharedContextPrivate)
{
	__DRIscreen *psDRIScreen = psDRIContext->driScreenPriv;
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(psDRIScreen);
	PVRDRIContext *psPVRContext;
	PVRDRIAPISubType eAPISub = PVRDRI_API_SUB_NONE;
	bool notify_reset = false;
	unsigned uPriority = PVRDRI_CONTEXT_PRIORITY_MEDIUM;
	bool bResult;

	psPVRContext = calloc(1, sizeof(*psPVRContext));
	if (psPVRContext == NULL)
	{
		__driUtilMessage("%s: Couldn't allocate PVRDRIContext",
				 __func__);
		*puError = __DRI_CTX_ERROR_NO_MEMORY;
		return GL_FALSE;
	}

	psPVRContext->psDRIContext = psDRIContext;
	psPVRContext->psPVRScreen = psPVRScreen;

	switch (eMesaAPI)
	{
		case API_OPENGL_COMPAT:
			psPVRContext->eAPI = PVRDRI_API_GL;
			eAPISub = PVRDRI_API_SUB_GL_COMPAT;
			break;
		case API_OPENGLES:
			psPVRContext->eAPI = PVRDRI_API_GLES1;
			break;
		case API_OPENGLES2:
			psPVRContext->eAPI = PVRDRI_API_GLES2;
			break;
		case API_OPENGL_CORE:
			eAPISub = PVRDRI_API_SUB_GL_CORE;
			psPVRContext->eAPI = PVRDRI_API_GL;
			break;
		default:
			__driUtilMessage("%s: Unsupported API: %d",
					 __func__,
					 (int)eMesaAPI);
			goto ErrorContextFree;
	}

	*puError = PVRDRISupportCreateContext(psPVRScreen->psImpl,
					      getSharedContextImpl(pvSharedContextPrivate),
					      psGLMode,
					      psPVRContext->eAPI,
					      eAPISub,
					      psCtxConfig->major_version,
					      psCtxConfig->minor_version,
					      psCtxConfig->flags,
					      notify_reset,
					      uPriority,
					      &psPVRContext->psImpl);
	if (*puError != __DRI_CTX_ERROR_SUCCESS)
	{
		goto ErrorContextFree;
	}

	/*
	 * The dispatch table must be created after the context, because
	 * PVRDRIContextCreate loads the API library, and we need the
	 * library handle to populate the dispatch table.
	 */
	PVRDRIScreenLock(psPVRScreen);
	bResult = pvrdri_create_dispatch_table(psPVRScreen, psPVRContext->eAPI);
	PVRDRIScreenUnlock(psPVRScreen);

	if (!bResult)
	{
		__driUtilMessage("%s: Couldn't create dispatch table",
				 __func__);
		*puError = __DRI_CTX_ERROR_BAD_API;
		goto ErrorContextDestroy;
	}

#if defined(DEBUG)
	p_atomic_inc(&psPVRScreen->iContextAlloc);
#endif

	psDRIContext->driverPrivate = (void *)psPVRContext;
	PVRDRIScreenAddReference(psPVRScreen);

	*puError = __DRI_CTX_ERROR_SUCCESS;

	return GL_TRUE;

ErrorContextDestroy:
	PVRDRIDestroyContextImpl(psPVRContext->psImpl,
				 psPVRContext->eAPI,
				 psPVRScreen->psImpl);
ErrorContextFree:
	free(psPVRContext);

	return GL_FALSE;
}

IMG_PIXFMT PVRDRIGetPixelFormat(const struct gl_config *psGLMode)
{
	switch (psGLMode->rgbBits)
	{
		case 32:
		case 24:
			if (psGLMode->redMask   == 0x00FF0000 &&
			    psGLMode->greenMask == 0x0000FF00 &&
			    psGLMode->blueMask  == 0x000000FF)
			{
				if (psGLMode->alphaMask == 0xFF000000)
				{
					return IMG_PIXFMT_B8G8R8A8_UNORM;
				}
				else if (psGLMode->alphaMask == 0)
				{
					return IMG_PIXFMT_B8G8R8X8_UNORM;
				}
			}

			if (psGLMode->redMask   == 0x000000FF &&
			    psGLMode->greenMask == 0x0000FF00 &&
			    psGLMode->blueMask  == 0x00FF0000)
			{
				if (psGLMode->alphaMask == 0xFF000000)
				{
					return IMG_PIXFMT_R8G8B8A8_UNORM;
				}
				else if (psGLMode->alphaMask == 0)
				{
					return IMG_PIXFMT_R8G8B8X8_UNORM;
				}
			}

			__driUtilMessage("%s: Unsupported buffer format", __func__);
			return IMG_PIXFMT_UNKNOWN;

		case 16:
			if (psGLMode->redMask   == 0xF800 &&
			    psGLMode->greenMask == 0x07E0 &&
			    psGLMode->blueMask  == 0x001F)
			{
				return IMG_PIXFMT_B5G6R5_UNORM;
			}

		default:
			errorMessage("%s: Unsupported screen format\n", __func__);
			return IMG_PIXFMT_UNKNOWN;
	}
}

static int PVRDRIScreenSupportedAPIs(PVRDRIScreen *psPVRScreen)
{
	unsigned api_mask = psPVRScreen->psDRIScreen->api_mask;
	int supported = 0;

	if ((api_mask & (1 << __DRI_API_OPENGL)) != 0)
	{
		supported |= PVRDRI_API_BIT_OPENGL;
	}

	if ((api_mask & (1 << __DRI_API_GLES)) != 0)
	{
		supported |= PVRDRI_API_BIT_GLES;
	}

	if ((api_mask & (1 << __DRI_API_GLES2)) != 0)
	{
		supported |= PVRDRI_API_BIT_GLES2;
	}

	if ((api_mask & (1 << __DRI_API_OPENGL_CORE)) != 0)
	{
		supported |= PVRDRI_API_BIT_OPENGL;
	}

	if ((api_mask & (1 << __DRI_API_GLES3)) != 0)
	{
		supported |= PVRDRI_API_BIT_GLES3;
	}

	return supported;
}

static GLboolean PVRDRICreateBuffer(__DRIscreen *psDRIScreen,
				    __DRIdrawable *psDRIDrawable,
				    const struct gl_config *psGLMode,
				    GLboolean bIsPixmap)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(psDRIScreen);
	PVRDRIDrawable *psPVRDrawable = NULL;
	int iSupportedAPIs = PVRDRIScreenSupportedAPIs(psPVRScreen);
	PVRDRIConfigInfo sConfigInfo;
	PVRDRIDrawableImpl *psDrawableImpl = NULL;

	/* No known callers ever set this to true */
	if (bIsPixmap)
	{
		return GL_FALSE;
	}

	if (!psGLMode)
	{
		__driUtilMessage("%s: Invalid GL config", __func__);
		return GL_FALSE;
	}

	psPVRDrawable = calloc(1, sizeof(*psPVRDrawable));
	if (!psPVRDrawable ||
	    (psDrawableImpl = PVRDRICreateDrawableImpl(psPVRDrawable)) == 0)
	{
		__driUtilMessage("%s: Couldn't allocate PVR drawable",
				 __func__);
		goto ErrorDrawableFree;
	}

	psPVRDrawable->psImpl = psDrawableImpl;

	psDRIDrawable->driverPrivate = psPVRDrawable;

	INITIALISE_PVRQ_HEAD(&psPVRDrawable->sPVRContextHead);
	INITIALISE_PVRQ_HEAD(&psPVRDrawable->sCacheFlushHead);

	psPVRDrawable->psPVRScreen = psPVRScreen;
	psPVRDrawable->psDRIDrawable = psDRIDrawable;
	psPVRDrawable->bDoubleBuffered = psGLMode->doubleBufferMode != 0;

	psPVRDrawable->ePixelFormat = PVRDRIGetPixelFormat(psGLMode);
	if (psPVRDrawable->ePixelFormat == IMG_PIXFMT_UNKNOWN)
	{
		__driUtilMessage("%s: Couldn't work out pixel format",
				 __func__);
		goto ErrorDrawableFree;
	}

	if (!PVRMutexInit(&psPVRDrawable->sMutex, PTHREAD_MUTEX_RECURSIVE))
	{
		__driUtilMessage("%s: Couldn't initialise drawable mutex",
				 __func__);
		goto ErrorDrawableFree;
	}

	PVRDRIConfigFromMesa(&sConfigInfo, psGLMode);

	if (!PVRDRIEGLDrawableConfigFromGLMode(psDrawableImpl,
					       &sConfigInfo,
					       iSupportedAPIs,
					       psPVRDrawable->ePixelFormat))
	{
		__driUtilMessage("%s: Couldn't derive EGL config", __func__);
		goto ErrirDrawableMutexDeinit;
	}

#if defined(DEBUG)
	p_atomic_inc(&psPVRScreen->iDrawableAlloc);
#endif

	PVRDRIScreenAddReference(psPVRScreen);

	return GL_TRUE;

ErrirDrawableMutexDeinit:
	PVRMutexDeinit(&psPVRDrawable->sMutex);

ErrorDrawableFree:
	PVRDRIDestroyDrawableImpl(psDrawableImpl);
	free(psPVRDrawable);
	psDRIDrawable->driverPrivate = NULL;

	return GL_FALSE;
}

static void PVRDRIDestroyBuffer(__DRIdrawable *psDRIDrawable)
{
	PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;
	PVRDRIScreen *psPVRScreen = psPVRDrawable->psPVRScreen;
	PVRQElem *psQElem;

	PVRDRIScreenLock(psPVRScreen);

	psQElem = psPVRDrawable->sPVRContextHead.pvForw;


	while (psQElem != &psPVRDrawable->sPVRContextHead)
	{
		PVRDRIContext *psPVRContext = PVRQ_CONTAINER_OF(psQElem,
								PVRDRIContext,
								sQElem);

		psQElem = psPVRContext->sQElem.pvForw;
		PVRDRIFlushBuffersGC(psPVRContext);

		if (psPVRDrawable == psPVRContext->psPVRDrawable)
		{
			if (psPVRContext->psPVRDrawable)
			{
				PVRDRIEGLMarkRendersurfaceInvalid(psPVRContext->eAPI,
								  psPVRContext->psPVRScreen->psImpl,
								  psPVRContext->psImpl);

				psPVRContext->psPVRDrawable = NULL;
			}
		}

		PVRQDequeue(&psPVRContext->sQElem);

	}

	PVRDRIDrawableDeinit(psPVRDrawable);
	PVREGLDrawableDestroyConfig(psPVRDrawable->psImpl);
	PVRMutexDeinit(&psPVRDrawable->sMutex);
	PVRDRIDestroyDrawableImpl(psPVRDrawable->psImpl);
	free(psPVRDrawable);

#if defined(DEBUG)
	p_atomic_dec(&psPVRScreen->iDrawableAlloc);
#endif
	PVRDRIScreenUnlock(psPVRScreen);
	PVRDRIScreenRemoveReference(psPVRScreen);
}

static void PVRDRIDisplayFrontBuffer(PVRDRIDrawable *psPVRDrawable)
{
	if (!psPVRDrawable->bDoubleBuffered)
	{
		PVRDRIScreen *psPVRScreen = psPVRDrawable->psPVRScreen;
		__DRIscreen *psDRIScreen = psPVRScreen->psDRIScreen;

		/* Only double buffered drawables should need flushing */
		assert(PVRQIsEmpty(&psPVRDrawable->sCacheFlushHead));

		if (psDRIScreen->image.loader && psDRIScreen->image.loader->flushFrontBuffer)
		{
			psDRIScreen->image.loader->flushFrontBuffer(psPVRDrawable->psDRIDrawable,
								    psPVRDrawable->psDRIDrawable->loaderPrivate);
		}
		else if (psDRIScreen->dri2.loader && psDRIScreen->dri2.loader->flushFrontBuffer)
		{
			psDRIScreen->dri2.loader->flushFrontBuffer(psPVRDrawable->psDRIDrawable,
								   psPVRDrawable->psDRIDrawable->loaderPrivate);
		}
	}
}

static GLboolean PVRDRIMakeCurrent(__DRIcontext *psDRIContext,
				   __DRIdrawable *psDRIWrite,
				   __DRIdrawable *psDRIRead)
{
	PVRDRIContext *psPVRContext = (PVRDRIContext *)psDRIContext->driverPrivate;
	PVRDRIDrawable *psPVRWrite = (psDRIWrite) ? (PVRDRIDrawable *)psDRIWrite->driverPrivate : NULL;
	PVRDRIDrawable *psPVRRead = (psDRIRead) ? (PVRDRIDrawable *)psDRIRead->driverPrivate : NULL;

	PVRDRIScreenLock(psPVRContext->psPVRScreen);

	if (psPVRWrite != NULL)
	{
		if (!PVRDRIDrawableInit(psPVRWrite))
		{
			__driUtilMessage("%s: Couldn't initialise write drawable",
					 __func__);
			goto ErrorUnlock;
		}
	}

	if (psPVRRead != NULL)
	{
		if (!PVRDRIDrawableInit(psPVRRead))
		{
			__driUtilMessage("%s: Couldn't initialise read drawable",
					 __func__);
			goto ErrorUnlock;
		}
	}

	if (!PVRDRIMakeCurrentGC(psPVRContext->eAPI,
				 psPVRContext->psPVRScreen->psImpl,
				 psPVRContext->psImpl,
				 psPVRWrite == NULL ? NULL : psPVRWrite->psImpl,
				 psPVRRead  == NULL ? NULL : psPVRRead->psImpl))
	{
		pvrdri_set_null_dispatch_table();
		goto ErrorUnlock;
	}

	PVRQDequeue(&psPVRContext->sQElem);

	if (psPVRWrite != NULL)
	{
		PVRQQueue(&psPVRWrite->sPVRContextHead, &psPVRContext->sQElem);
	}

	psPVRContext->psPVRDrawable = psPVRWrite;

	if (psPVRWrite != NULL && psPVRContext->eAPI == PVRDRI_API_GL)
	{
		PVRDRIEGLSetFrontBufferCallback(psPVRContext->eAPI,
						psPVRContext->psPVRScreen->psImpl,
						psPVRWrite->psImpl,
						PVRDRIDisplayFrontBuffer);
	}

	pvrdri_set_dispatch_table(psPVRContext);

	PVRDRIThreadSetCurrentScreen(psPVRContext->psPVRScreen);

	PVRDRIScreenUnlock(psPVRContext->psPVRScreen);

	return GL_TRUE;

ErrorUnlock:
	PVRDRIScreenUnlock(psPVRContext->psPVRScreen);

	return GL_FALSE;
}

static void PVRDRIDestroyContext(__DRIcontext *psDRIContext)
{
	PVRDRIContext *psPVRContext = (PVRDRIContext *)psDRIContext->driverPrivate;
	PVRDRIScreen *psPVRScreen = psPVRContext->psPVRScreen;

	PVRDRIScreenLock(psPVRScreen);

	if (psPVRContext->psPVRDrawable)
	{
		PVRDRIFlushBuffersGC(psPVRContext);

		if (psPVRContext->psPVRDrawable)
			psPVRContext->psPVRDrawable = NULL;
	}

	PVRQDequeue(&psPVRContext->sQElem);

	PVRDRIDestroyContextImpl(psPVRContext->psImpl,
				 psPVRContext->eAPI,
				 psPVRScreen->psImpl);

	free(psPVRContext);

#if defined(DEBUG)
	p_atomic_dec(&psPVRScreen->iContextAlloc);
#endif
	PVRDRIScreenUnlock(psPVRScreen);
	PVRDRIScreenRemoveReference(psPVRScreen);
}

static GLboolean PVRDRIUnbindContext(__DRIcontext *psDRIContext)
{
	PVRDRIContext *psPVRContext = (PVRDRIContext *)psDRIContext->driverPrivate;
	PVRDRIScreen *psPVRScreen = psPVRContext->psPVRScreen;

	pvrdri_set_null_dispatch_table();

	PVRDRIScreenLock(psPVRScreen);
	PVRDRIFlushBuffersGC(psPVRContext);
	PVRDRIMakeUnCurrentGC(psPVRContext->eAPI,
			      psPVRContext->psPVRScreen->psImpl);

	if (psPVRContext->psPVRDrawable)
		psPVRContext->psPVRDrawable = NULL;

	PVRQDequeue(&psPVRContext->sQElem);
	PVRDRIThreadSetCurrentScreen(NULL);
	PVRDRIScreenUnlock(psPVRScreen);

	return GL_TRUE;
}

static __DRIbuffer *PVRDRIAllocateBuffer(__DRIscreen *psDRIScreen,
					 unsigned int uAttachment,
					 unsigned int uFormat,
					 int iWidth,
					 int iHeight)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(psDRIScreen);
	PVRBuffer *psBuffer;
	unsigned int uiBpp;

	/* GEM names are only supported on primary nodes */
	if (drmGetNodeTypeFromFd(psDRIScreen->fd) != DRM_NODE_PRIMARY)
	{
		__driUtilMessage("%s: Cannot allocate buffer", __func__);
		return NULL;
	}

	/* This is based upon PVRDRIGetPixelFormat */
	switch (uFormat)
	{
		case 32:
		case 16:
			/* Format (depth) and bpp match */
			uiBpp = uFormat;
			break;
		case 24:
			uiBpp = 32;
			break;
		default:
			__driUtilMessage("%s: Unsupported format '%u'",
					 __func__, uFormat);
			return NULL;
	}

	psBuffer = calloc(1, sizeof(*psBuffer));
	if (psBuffer == NULL)
	{
		__driUtilMessage("%s: Failed to allocate buffer", __func__);
		return NULL;
	}

	psBuffer->psImpl = PVRDRIBufferCreate(psPVRScreen->psImpl,
					      iWidth,
					      iHeight,
					      uiBpp,
					      PVDRI_BUFFER_USE_SHARE,
					      &psBuffer->sDRIBuffer.pitch);
	if (!psBuffer->psImpl)
	{
		__driUtilMessage("%s: Failed to create backing buffer",
				 __func__);
		goto ErrorFreeDRIBuffer;
	}

	psBuffer->sDRIBuffer.attachment = uAttachment;
	psBuffer->sDRIBuffer.name = PVRDRIBufferGetName(psBuffer->psImpl);
	psBuffer->sDRIBuffer.cpp = uiBpp / 8;

#if defined(DEBUG)
	p_atomic_inc(&psPVRScreen->iBufferAlloc);
#endif

	return &psBuffer->sDRIBuffer;

ErrorFreeDRIBuffer:
	free(psBuffer);

	return NULL;
}

static void PVRDRIReleaseBuffer(__DRIscreen *psDRIScreen,
				__DRIbuffer *psDRIBuffer)
{
	PVRBuffer *psBuffer = (PVRBuffer *)psDRIBuffer;
#if defined(DEBUG)
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(psDRIScreen);
#endif

	(void)psDRIScreen;

	PVRDRIBufferDestroy(psBuffer->psImpl);
	free(psBuffer);

#if defined(DEBUG)
	p_atomic_dec(&psPVRScreen->iBufferAlloc);
#endif
}

/* Publish our driver implementation to the world. */
static const struct __DriverAPIRec pvr_driver_api =
{
	.InitScreen     = PVRDRIInitScreen,
	.DestroyScreen  = PVRDRIDestroyScreen,
	.CreateContext  = PVRDRICreateContext,
	.DestroyContext = PVRDRIDestroyContext,
	.CreateBuffer   = PVRDRICreateBuffer,
	.DestroyBuffer  = PVRDRIDestroyBuffer,
	.SwapBuffers    = NULL,
	.MakeCurrent    = PVRDRIMakeCurrent,
	.UnbindContext  = PVRDRIUnbindContext,
	.AllocateBuffer = PVRDRIAllocateBuffer,
	.ReleaseBuffer  = PVRDRIReleaseBuffer,
};

static const struct __DRIDriverVtableExtensionRec pvr_vtable = {
   .base = { __DRI_DRIVER_VTABLE, 1 },
   .vtable = &pvr_driver_api,
};

static const __DRIextension *pvr_driver_extensions[] = {
    &driCoreExtension.base,
    &driImageDriverExtension.base,
    &driDRI2Extension.base,
    &pvr_vtable.base,
    NULL
};

const __DRIextension **__driDriverGetExtensions_pvr(void);
PUBLIC const __DRIextension **__driDriverGetExtensions_pvr(void)
{
   globalDriverAPI = &pvr_driver_api;

   return pvr_driver_extensions;
}

/*************************************************************************/ /*!
 Global functions
*/ /**************************************************************************/

void PVRDRIThreadSetCurrentScreen(PVRDRIScreen *psPVRScreen)
{
	gpsPVRScreen = psPVRScreen;
}

PVRDRIScreen *PVRDRIThreadGetCurrentScreen(void)
{
	return gpsPVRScreen;
}
