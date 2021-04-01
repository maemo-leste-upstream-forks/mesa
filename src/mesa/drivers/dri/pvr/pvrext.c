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

/*
 * EXTENSION SUPPORT
 *
 * As the driver supports a range of Mesa versions it can be the case that it
 * needs to support different extensions and extension versions depending on
 * the version of Mesa that it's built against. As a guide the following rules
 * should be followed:
 *
 * 1) If an extension appears in some supported versions of Mesa but not others
 *    then it should be protected by the extension define, e.g.:
 *    #if defined(__DRI_IMAGE)
 *    <code>
 *    #endif
 *
 *    However, if it appears in all versions then there's no need for it to
 *    be protected.
 *
 * 2) Each driver supported extension should have a define for the maximum
 *    version supported by the driver. This should be used when initialising
 *    the corresponding extension structure. The Mesa extension version define
 *    should *NOT* be used.
 *
 * 3) If the driver supports a range of versions for a given extension then
 *    it should protect the extension code based on the Mesa extension version
 *    define. For example, if the driver has to support versions 7 to 8 of the
 *    __DRI_IMAGE extension then any fields, in the __DRIimageExtension
 *    structure, that appear in version 8 but not 7 should be protected as
 *    follows:
 *    #if (__DRI_IMAGE_VERSION >= 8)
 *    .createImageFromDmaBufs = PVRDRICreateImageFromDmaBufs,
 *    #endif
 *
 *    Obviously any other associated code should also be protected in the same
 *    way.
 */

#include "util/u_atomic.h"

#include "dri_util.h"
#include "utils.h"

#include "dri_support.h"
#include "pvrdri.h"
#include "pvrimage.h"

#include "EGL/egl.h"
#include "EGL/eglext.h"

/* Maximum version numbers for each supported extension */
#define PVR_DRI_TEX_BUFFER_VERSION	2
#define PVR_DRI2_FLUSH_VERSION		4
#define PVR_DRI_IMAGE_VERSION		17
#define PVR_DRI2_ROBUSTNESS_VERSION	1
#define PVR_DRI2_FENCE_VERSION		2

static void PVRDRIFlushDrawableContext(PVRDRIDrawable *psPVRDrawable,
				       PVRDRIContext *psPVRContext)
{
	PVRDRIContext *psPVRDrawContext = psPVRDrawable->psPVRContext;

	if (psPVRDrawContext)
	{
		PVRDRIEGLFlushBuffers(psPVRDrawContext->eAPI,
		                      psPVRDrawContext->psPVRScreen->psImpl,
		                      psPVRDrawContext->psImpl,
		                      psPVRDrawable->psImpl,
		                      false,
		                      false,
		                      (psPVRDrawContext != psPVRContext));
	}
}

static void PVRDRIExtSetTexBuffer(__DRIcontext  *psDRIContext,
				  GLint          target,
				  GLint          format,
				  __DRIdrawable *psDRIDrawable)
{
	PVRDRIDrawable *psPVRDrawable = (PVRDRIDrawable *)psDRIDrawable->driverPrivate;
	PVRDRIContext *psPVRContext = (PVRDRIContext *)psDRIContext->driverPrivate;

	(void)target;
	(void)format;

	if (!psPVRDrawable->bInitialised)
	{
		if (!PVRDRIDrawableInit(psPVRDrawable))
		{
			__driUtilMessage("%s: Couldn't initialise pixmap", __func__);
			return;
		}
	}

	PVRDRIFlushDrawableContext(psPVRDrawable, psPVRContext);
	PVRDRI2BindTexImage(psPVRContext->eAPI,
	                    psPVRContext->psPVRScreen->psImpl,
	                    psPVRContext->psImpl,
	                    psPVRDrawable->psImpl);
}

static void PVRDRIExtReleaseTexBuffer(__DRIcontext  *psDRIContext,
				      GLint          target,
				      __DRIdrawable *psDRIDrawable)
{
	PVRDRIDrawable *psPVRDrawable = (PVRDRIDrawable *)psDRIDrawable->driverPrivate;
	PVRDRIContext *psPVRContext = (PVRDRIContext *)psDRIContext->driverPrivate;

	(void)target;

	PVRDRI2ReleaseTexImage(psPVRContext->eAPI,
	                       psPVRContext->psPVRScreen->psImpl,
	                       psPVRContext->psImpl,
	                       psPVRDrawable->psImpl);
}

static __DRItexBufferExtension pvrDRITexBufferExtension =
{
	.base			= { .name = __DRI_TEX_BUFFER, .version = PVR_DRI_TEX_BUFFER_VERSION },
	.setTexBuffer		= NULL,
	.setTexBuffer2		= PVRDRIExtSetTexBuffer,
	.releaseTexBuffer	= PVRDRIExtReleaseTexBuffer
};


static void PVRDRI2Flush(__DRIdrawable *psDRIDrawable)
{
	PVRDRIDrawable *psPVRDrawable = (PVRDRIDrawable *)psDRIDrawable->driverPrivate;
	PVRDRIContext *psPVRContext = psPVRDrawable->psPVRContext;

	PVRDRIFlushBuffersForSwap(psPVRContext, psPVRDrawable);
}

static void PVRDRI2Invalidate(__DRIdrawable *psDRIDrawable)
{
	PVRDRIDrawable *psPVRDrawable = (PVRDRIDrawable *)psDRIDrawable->driverPrivate;

	if (psPVRDrawable->psPVRScreen->bUseInvalidate)
	{
		p_atomic_inc(&psPVRDrawable->iInfoInvalid);
	}
}

static void PVRDRI2FlushWithFlags(__DRIcontext *psDRIContext,
				  __DRIdrawable *psDRIDrawable,
				  unsigned uFlags,
				  enum __DRI2throttleReason eThrottleReason)
{
	PVRDRIContext *psPVRContext = (PVRDRIContext *)psDRIContext->driverPrivate;

	(void)eThrottleReason;

	if ((uFlags & __DRI2_FLUSH_DRAWABLE) != 0)
	{
		PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;

		PVRDRIFlushBuffersForSwap(psPVRContext, psPVRDrawable);
	}
	else if ((uFlags & __DRI2_FLUSH_CONTEXT) != 0)
	{
		/*
		 * __DRI2_FLUSH__CONTEXT means "glFlush". Most callers
		 * also specify __DRI2_FLUSH_DRAWABLE. An exception is
		 * GBM, which flushes after an unmap, when there doesn't
		 * appear to be a need to flush outstanding GPU operations.
		 */
	}
}

static __DRI2flushExtension pvrDRI2FlushExtension =
{
	.base			= { .name = __DRI2_FLUSH, .version = PVR_DRI2_FLUSH_VERSION },
	.flush			= PVRDRI2Flush,
	.invalidate		= PVRDRI2Invalidate,
	.flush_with_flags	= PVRDRI2FlushWithFlags,
};


static __DRIimageExtension pvrDRIImage =
{
	.base				= { .name = __DRI_IMAGE, .version = PVR_DRI_IMAGE_VERSION },
	.createImageFromName		= PVRDRICreateImageFromName,
	.createImageFromRenderbuffer	= PVRDRICreateImageFromRenderbuffer,
	.destroyImage			= PVRDRIDestroyImage,
	.createImage			= PVRDRICreateImage,
	.queryImage			= PVRDRIQueryImage,
	.dupImage			= PVRDRIDupImage,
	.validateUsage			= PVRDRIValidateUsage,
	.createImageFromNames		= PVRDRICreateImageFromNames,
	.fromPlanar			= PVRDRIFromPlanar,
	.createImageFromTexture		= PVRDRICreateImageFromTexture,
	.createImageFromFds		= PVRDRICreateImageFromFds,
	.createImageFromDmaBufs		= PVRDRICreateImageFromDmaBufs,
	.blitImage			= PVRDRIBlitImage,
	.getCapabilities		= PVRDRIGetCapabilities,
	.mapImage			= PVRDRIMapImage,
	.unmapImage			= PVRDRIUnmapImage,
	.createImageWithModifiers	= PVRDRICreateImageWithModifiers,
	.createImageFromDmaBufs2	= PVRDRICreateImageFromDmaBufs2,
	.queryDmaBufFormats		= PVRDRIQueryDmaBufFormats,
	.queryDmaBufModifiers		= PVRDRIQueryDmaBufModifiers,
	.queryDmaBufFormatModifierAttribs = PVRDRIQueryDmaBufFormatModifierAttribs,
	.createImageFromRenderbuffer2	= PVRDRICreateImageFromRenderbuffer2,
#if defined(EGL_IMG_cl_image)
	.createImageFromBuffer		= PVRDRICreateImageFromBuffer,
#endif
};

static __DRIrobustnessExtension pvrDRIRobustness =
{
	.base = { .name = __DRI2_ROBUSTNESS, .version = PVR_DRI2_ROBUSTNESS_VERSION }
};

static int
PVRDRIQueryRendererInteger(__DRIscreen *dri_screen,
                           int param, unsigned int *value)
{
	switch (param)
	{
		case __DRI2_RENDERER_HAS_CONTEXT_PRIORITY:
			value[0] = 0;
			value[0] |= __DRI2_RENDERER_HAS_CONTEXT_PRIORITY_HIGH;
			value[0] |= __DRI2_RENDERER_HAS_CONTEXT_PRIORITY_MEDIUM;
			value[0] |= __DRI2_RENDERER_HAS_CONTEXT_PRIORITY_LOW;
			return 0;

		default:
			return driQueryRendererIntegerCommon(dri_screen, param, value);
	}
}

static int
PVRDRIQueryRendererString(__DRIscreen *dri_screen,
                          int param, const char **value)
{
	return -1;
}

static const __DRI2rendererQueryExtension pvrDRIRendererQueryExtension = {
   .base = { __DRI2_RENDERER_QUERY, 1 },

   .queryInteger = PVRDRIQueryRendererInteger,
   .queryString = PVRDRIQueryRendererString,
};


#if defined(__DRI2_FENCE)
static void *PVRDRICreateFenceEXT(__DRIcontext *psDRIContext)
{
	PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;
	PVRDRIScreen *psPVRScreen = psPVRContext->psPVRScreen;

	return PVRDRICreateFenceImpl(psPVRContext->eAPI,
				     psPVRScreen->psImpl,
				     psPVRContext->psImpl);
}

static void PVRDRIDestroyFenceEXT(__DRIscreen *psDRIScreen, void *psDRIFence)
{
	(void)psDRIScreen;

	PVRDRIDestroyFenceImpl(psDRIFence);
}

static GLboolean PVRDRIClientWaitSyncEXT(__DRIcontext *psDRIContext,
					 void *psDRIFence,
					 unsigned uFlags,
					 uint64_t uiTimeout)
{
	bool bFlushCommands = (uFlags & __DRI2_FENCE_FLAG_FLUSH_COMMANDS);
	bool bTimeout = (uiTimeout != __DRI2_FENCE_TIMEOUT_INFINITE);

	if (psDRIContext && bFlushCommands)
	{
		PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;
		PVRDRIDrawable *psPVRDrawable = psPVRContext->psPVRDrawable;

		(void) PVRDRIEGLFlushBuffers(psPVRContext->eAPI,
					     psPVRContext->psPVRScreen->psImpl,
					     psPVRContext->psImpl,
					     psPVRDrawable ? psPVRDrawable->psImpl : NULL,
					     true, false, false);
	}

	return PVRDRIClientWaitSyncImpl(PVRDRI_API_NONE,
					NULL,
					psDRIFence,
					false,
					bTimeout,
					uiTimeout);
}

static void PVRDRIServerWaitSyncEXT(__DRIcontext *psDRIContext,
				    void *psDRIFence,
				    unsigned uFlags)
{
	(void)uFlags;
	assert(uFlags == 0);

	if (psDRIFence)
	{
		PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

		if (!PVRDRIServerWaitSyncImpl(psPVRContext->eAPI,
					      psPVRContext->psImpl,
					      psDRIFence))
		{
			__driUtilMessage("%s: Server wait sync failed",
					 __func__);
		}
	}
}

static unsigned PVRDRIGetFenceCapabilitiesEXT(__DRIscreen *psDRIScreen)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(psDRIScreen);

	return PVRDRIGetFenceCapabilitiesImpl(psPVRScreen->psImpl);
}

static void *PVRDRICreateFenceFdEXT(__DRIcontext *psDRIContext, int iFd)
{
	PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;
	PVRDRIScreen *psPVRScreen = psPVRContext->psPVRScreen;

	return PVRDRICreateFenceFdImpl(psPVRContext->eAPI,
				       psPVRScreen->psImpl,
				       psPVRContext->psImpl,
				       iFd);
}

static int PVRDRIGetFenceFdEXT(__DRIscreen *psDRIScreen, void *psDRIFence)
{
	(void)psDRIScreen;

	return PVRDRIGetFenceFdImpl(psDRIFence);
}

const __DRI2fenceExtension pvrDRIFenceExtension =
{
	.base				= { .name = __DRI2_FENCE, .version = PVR_DRI2_FENCE_VERSION },
	.create_fence			= PVRDRICreateFenceEXT,
	/* Not currently supported */
	.get_fence_from_cl_event	= NULL,
	.destroy_fence			= PVRDRIDestroyFenceEXT,
	.client_wait_sync		= PVRDRIClientWaitSyncEXT,
	.server_wait_sync		= PVRDRIServerWaitSyncEXT,
	.get_capabilities		= PVRDRIGetFenceCapabilitiesEXT,
	.create_fence_fd		= PVRDRICreateFenceFdEXT,
	.get_fence_fd			= PVRDRIGetFenceFdEXT,
};
#endif /* defined(__DRI2_FENCE) */

/*
 * Extension lists
 *
 * NOTE: When adding a new screen extension asScreenExtensionVersionInfo
 *       should also be updated accordingly.
 */
static const __DRIextension *apsScreenExtensions[] =
{
	&pvrDRITexBufferExtension.base,
	&pvrDRI2FlushExtension.base,
	&pvrDRIImage.base,
	&pvrDRIRobustness.base,
	&pvrDRIRendererQueryExtension.base,
#if defined(__DRI2_FENCE)
	&pvrDRIFenceExtension.base,
#endif
	&dri2ConfigQueryExtension.base,
	NULL
};

static const __DRIextension asScreenExtensionVersionInfo[] =
{
	{ .name = __DRI_TEX_BUFFER, .version = __DRI_TEX_BUFFER_VERSION },
	{ .name = __DRI2_FLUSH, .version = __DRI2_FLUSH_VERSION },
	{ .name = __DRI_IMAGE, .version = __DRI_IMAGE_VERSION },
	{ .name = __DRI2_ROBUSTNESS, .version = __DRI2_ROBUSTNESS_VERSION },
#if defined(__DRI2_FENCE)
	{ .name = __DRI2_FENCE, .version = __DRI2_FENCE_VERSION },
#endif
	{ .name = __DRI2_CONFIG_QUERY, .version = __DRI2_CONFIG_QUERY_VERSION },
	{ .name = NULL, .version = 0 },
};

const __DRIextension **PVRDRIScreenExtensions(void)
{
	if (!PVRDRIBlitEGLImage_IsSupported())
	{
		pvrDRIImage.base.version = 8;
	}
	else if (!PVRDRIMapEGLImage_IsSupported())
	{
		pvrDRIImage.base.version = 11;
	}
	else if (!PVRDRIBufferGetOffset_IsSupported())
	{
		pvrDRIImage.base.version = 12;
	}
	else if (!PVRDRIBufferCreateWithModifiers_IsSupported())
	{
		pvrDRIImage.base.version = 13;
	}
	else if (!PVRDRIBufferCreateFromFdsWithModifier_IsSupported())
	{
		pvrDRIImage.base.version = 14;
	}

	return apsScreenExtensions;
}

const __DRIextension *PVRDRIScreenExtensionVersionInfo(void)
{
	return asScreenExtensionVersionInfo;
}
