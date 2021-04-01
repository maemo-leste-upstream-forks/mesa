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

#include "util/u_atomic.h"

#include "dri_util.h"

#include "pvrdri.h"
#include "pvrimage.h"

static inline void PVRDRIMarkRenderSurfaceAsInvalid(PVRDRIDrawable *psPVRDrawable)
{
	PVRDRIContext *psPVRContext = psPVRDrawable->psPVRContext;

	if (psPVRContext)
	{

		PVRDRIEGLMarkRendersurfaceInvalid(psPVRContext->eAPI,
						  psPVRContext->psPVRScreen->psImpl,
						  psPVRContext->psImpl);
	}
}

/*************************************************************************/ /*!
 PVR drawable local functions (image driver loader)
*/ /**************************************************************************/

static inline void PVRDrawableImageDestroy(PVRDRIDrawable *psPVRDrawable)
{
	if (psPVRDrawable->psImage)
	{
		PVRDRIUnrefImage(psPVRDrawable->psImage);
		psPVRDrawable->psImage = NULL;
	}
}

static inline void PVRDrawableImageAccumDestroy(PVRDRIDrawable *psPVRDrawable)
{
	if (psPVRDrawable->psImageAccum)
	{
		PVRDRIUnrefImage(psPVRDrawable->psImageAccum);
		psPVRDrawable->psImageAccum = NULL;
	}
}

static void PVRDrawableImageUpdate(PVRDRIDrawable *psPVRDrawable)
{
	if (psPVRDrawable->psImage != psPVRDrawable->psDRI)
	{
		assert(PVRDRIImageGetSharedBuffer(psPVRDrawable->psDRI) != NULL);

		PVRDrawableImageDestroy(psPVRDrawable);

		PVRDRIRefImage(psPVRDrawable->psDRI);
		psPVRDrawable->psImage = psPVRDrawable->psDRI;
	}

	if (psPVRDrawable->psImageAccum != psPVRDrawable->psDRIAccum)
	{
		PVRDrawableImageAccumDestroy(psPVRDrawable);

		if (psPVRDrawable->psDRIAccum)
		{
			PVRDRIRefImage(psPVRDrawable->psDRIAccum);
			psPVRDrawable->psImageAccum = psPVRDrawable->psDRIAccum;
		}
	}
}

/*************************************************************************/ /*!
 Function Name		: PVRImageDrawableGetNativeInfo
 Inputs			: psPVRDrawable
 Returns		: Boolean
 Description		: Update native drawable information.
*/ /**************************************************************************/
static bool PVRImageDrawableGetNativeInfo(PVRDRIDrawable *psPVRDrawable)
{
	__DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
	__DRIscreen *psDRIScreen = psPVRDrawable->psPVRScreen->psDRIScreen;
	struct __DRIimageList sImages;
	uint32_t uiBufferMask;
	const PVRDRIImageFormat *psFormat;

	assert(psDRIScreen->image.loader != NULL);
	assert(psDRIScreen->image.loader->getBuffers);

	psFormat = PVRDRIIMGPixelFormatToImageFormat(psPVRDrawable->psPVRScreen,
						     psPVRDrawable->ePixelFormat);
	if (!psFormat)
	{
		errorMessage("%s: Unsupported format (format = %u)\n",
			     __func__, psPVRDrawable->ePixelFormat);
		return false;
	}

	if (psPVRDrawable->sConfig.sGLMode.doubleBufferMode)
	{
		uiBufferMask = __DRI_IMAGE_BUFFER_BACK;
	}
	else
	{
		uiBufferMask = __DRI_IMAGE_BUFFER_FRONT;
	}

#if defined(DRI_IMAGE_HAS_BUFFER_PREV)
	uiBufferMask |= __DRI_IMAGE_BUFFER_PREV;
#endif

	if (!psDRIScreen->image.loader->getBuffers(psDRIDrawable,
						   psFormat->iDRIFormat,
						   NULL,
						   psDRIDrawable->loaderPrivate,
						   uiBufferMask,
						   &sImages))
	{
		errorMessage("%s: Image get buffers call failed\n", __func__);
		return false;
	}

	psPVRDrawable->psDRI =
		(sImages.image_mask & __DRI_IMAGE_BUFFER_BACK) ?
			sImages.back : sImages.front;

#if defined(DRI_IMAGE_HAS_BUFFER_PREV)
	if (sImages.image_mask & __DRI_IMAGE_BUFFER_PREV)
	{
		psPVRDrawable->psDRIAccum = sImages.prev;
	}
	else
#endif
	{
		psPVRDrawable->psDRIAccum = NULL;
	}

	return true;
}

/*************************************************************************/ /*!
 Function Name		: PVRImageDrawableCreate
 Inputs			: psPVRDrawable
 Returns		: Boolean
 Description		: Create drawable
*/ /**************************************************************************/
static bool PVRImageDrawableCreate(PVRDRIDrawable *psPVRDrawable)
{
	__DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
	uint32_t uBytesPerPixel;
	PVRDRIBufferAttribs sBufferAttribs;

	if (!PVRImageDrawableGetNativeInfo(psPVRDrawable))
	{
		return false;
	}

	PVRDRIEGLImageGetAttribs(
		PVRDRIImageGetEGLImage(psPVRDrawable->psDRI),
					&sBufferAttribs);
	uBytesPerPixel = PVRDRIPixFmtGetBlockSize(sBufferAttribs.ePixFormat);

	psDRIDrawable->w = sBufferAttribs.uiWidth;
	psDRIDrawable->h = sBufferAttribs.uiHeight;
	psPVRDrawable->uStride = sBufferAttribs.uiStrideInBytes;
	psPVRDrawable->uBytesPerPixel = uBytesPerPixel;

	PVRDrawableImageUpdate(psPVRDrawable);

	if (!PVREGLDrawableCreate(psPVRDrawable->psPVRScreen->psImpl,
				  psPVRDrawable->psImpl))
	{
		errorMessage("%s: Couldn't create EGL drawable\n", __func__);
		return false;
	}

	return true;
}

/*************************************************************************/ /*!
 Function Name		: PVRImageDrawableUpdate
 Inputs			: psPVRDrawable
 Returns		: Boolean
 Description		: Update drawable
*/ /**************************************************************************/
static bool PVRImageDrawableUpdate(PVRDRIDrawable *psPVRDrawable,
				   bool bAllowRecreate)
{
	__DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
	uint32_t uBytesPerPixel;
	PVRDRIBufferAttribs sBufferAttribs;
	bool bRecreate;

	PVRDRIEGLImageGetAttribs(
		PVRDRIImageGetEGLImage(psPVRDrawable->psDRI),
					&sBufferAttribs);
	uBytesPerPixel = PVRDRIPixFmtGetBlockSize(sBufferAttribs.ePixFormat);

	bRecreate = (!psPVRDrawable->sConfig.sGLMode.doubleBufferMode &&
			psPVRDrawable->psImage !=
				psPVRDrawable->psDRI) ||
			(psDRIDrawable->w != sBufferAttribs.uiWidth) ||
			(psDRIDrawable->h != sBufferAttribs.uiHeight) ||
			(psPVRDrawable->uStride !=
				sBufferAttribs.uiStrideInBytes) ||
			(psPVRDrawable->uBytesPerPixel != uBytesPerPixel);

	if (bRecreate)
	{
		if (bAllowRecreate)
		{
			PVRDRIMarkRenderSurfaceAsInvalid(psPVRDrawable);

			psDRIDrawable->w = sBufferAttribs.uiWidth;
			psDRIDrawable->h = sBufferAttribs.uiHeight;
			psPVRDrawable->uStride = sBufferAttribs.uiStrideInBytes;
			psPVRDrawable->uBytesPerPixel = uBytesPerPixel;
		}
		else
		{
			return false;
		}
	}

	PVRDrawableImageUpdate(psPVRDrawable);

	if (bRecreate)
	{
		if (!PVREGLDrawableRecreate(psPVRDrawable->psPVRScreen->psImpl,
		                            psPVRDrawable->psImpl))
		{
			errorMessage("%s: Couldn't recreate EGL drawable\n",
				     __func__);
			return false;
		}
	}

	return true;
}

/*************************************************************************/ /*!
 PVR drawable local functions
*/ /**************************************************************************/

/*************************************************************************/ /*!
 Function Name	: PVRDRIDrawableUpdate
 Inputs		: psPVRDrawable
 Description	: Update drawable
*/ /**************************************************************************/
static bool PVRDRIDrawableUpdate(PVRDRIDrawable *psPVRDrawable,
				 bool bAllowRecreate)
{
	bool bRes;
	int iInfoInvalid = 0;

	/*
	 * The test for bDrawableUpdating is needed because drawable
	 * parameters are fetched (via KEGLGetDrawableParameters) when
	 * a drawable is recreated.
	 * The test for bFlushInProgress is to prevent the drawable
	 * information being updated during a flush, which could result
	 * in a call back into the Mesa platform code during the
	 * processing for a buffer swap, which could corrupt the platform
	 * state.
	 */
	if (psPVRDrawable->bDrawableUpdating ||
	    psPVRDrawable->bFlushInProgress)
	{
		return false;
	}
	psPVRDrawable->bDrawableUpdating = true;

	if (psPVRDrawable->psPVRScreen->bUseInvalidate)
	{
		iInfoInvalid = p_atomic_read(&psPVRDrawable->iInfoInvalid);
		bRes = !iInfoInvalid;
		if (bRes)
		{
			goto ExitNotUpdating;
		}
	}

	bRes = PVRImageDrawableGetNativeInfo(psPVRDrawable);
	if (!bRes)
	{
		goto ExitNotUpdating;
	}

	bRes = PVRImageDrawableUpdate(psPVRDrawable, bAllowRecreate);
	if (bRes && iInfoInvalid)
	{
		p_atomic_add(&psPVRDrawable->iInfoInvalid, -iInfoInvalid);
	}

ExitNotUpdating:
	psPVRDrawable->bDrawableUpdating = false;
	return bRes;
}

/*************************************************************************/ /*!
 Function Name	: PVRDRIDrawableRecreateV0
 Inputs		: psPVRDrawable
 Description	: Recreate drawable
*/ /**************************************************************************/
bool PVRDRIDrawableRecreateV0(PVRDRIDrawable *psPVRDrawable)
{
	return PVRDRIDrawableUpdate(psPVRDrawable, true);
}

/*************************************************************************/ /*!
 PVR drawable interface
*/ /**************************************************************************/
bool PVRDRIDrawableInit(PVRDRIDrawable *psPVRDrawable)
{
	if (psPVRDrawable->bInitialised)
	{
		return true;
	}

	if (!PVRImageDrawableCreate(psPVRDrawable))
	{
		return false;
	}

	psPVRDrawable->bInitialised = true;

	return true;
}

void PVRDRIDrawableDeinit(PVRDRIDrawable *psPVRDrawable)
{
	(void) PVREGLDrawableDestroy(psPVRDrawable->psPVRScreen->psImpl,
	                             psPVRDrawable->psImpl);

	PVRDrawableImageDestroy(psPVRDrawable);
	PVRDrawableImageAccumDestroy(psPVRDrawable);

	psPVRDrawable->bInitialised = false;
}

static bool PVRDRIDrawableGetParameters(PVRDRIDrawable *psPVRDrawable,
					PVRDRIBufferImpl **ppsDstBuffer,
					PVRDRIBufferImpl **ppsAccumBuffer)
{
	if (ppsDstBuffer || ppsAccumBuffer)
	{
		PVRDRIBufferImpl *psDstBuffer;
		PVRDRIBufferImpl *psAccumBuffer;

		psDstBuffer = PVRDRIImageGetSharedBuffer(psPVRDrawable->psImage);
		if (!psDstBuffer)
		{
			errorMessage("%s: Couldn't get backing buffer\n",
				     __func__);
			return false;
		}

		if (psPVRDrawable->psImageAccum)
		{
			psAccumBuffer =
				PVRDRIImageGetSharedBuffer(psPVRDrawable->psImageAccum);
			if (!psAccumBuffer)
			{
				psAccumBuffer = psDstBuffer;
			}
		}
		else
		{
			psAccumBuffer = psDstBuffer;
		}

		if (ppsDstBuffer)
		{
			*ppsDstBuffer = psDstBuffer;
		}

		if (ppsAccumBuffer)
		{
			*ppsAccumBuffer = psAccumBuffer;
		}
	}

	return true;
}

bool PVRDRIDrawableGetParametersV0(PVRDRIDrawable *psPVRDrawable,
				   PVRDRIBufferImpl **ppsDstBuffer,
				   PVRDRIBufferImpl **ppsAccumBuffer,
				   PVRDRIBufferAttribs *psAttribs,
				   bool *pbDoubleBuffered)
{
	/*
	 * Some drawable updates may be required, which stop short of
	 * recreating the drawable.
	 */
	(void) PVRDRIDrawableUpdate(psPVRDrawable, false);

	if (!PVRDRIDrawableGetParameters(psPVRDrawable,
					 ppsDstBuffer,
					 ppsAccumBuffer))
	{
		return false;
	}

	if (psAttribs)
	{
		psAttribs->uiWidth           = psPVRDrawable->psDRIDrawable->w;
		psAttribs->uiHeight          = psPVRDrawable->psDRIDrawable->h;
		psAttribs->ePixFormat        = psPVRDrawable->ePixelFormat;
		psAttribs->uiStrideInBytes   = psPVRDrawable->uStride;
	}

	if (pbDoubleBuffered)
	{
		*pbDoubleBuffered =
			psPVRDrawable->sConfig.sGLMode.doubleBufferMode;
	}

	return true;
}

bool PVRDRIDrawableGetParametersV1(PVRDRIDrawable *psPVRDrawable,
				   bool bAllowRecreate,
				   PVRDRIBufferImpl **ppsDstBuffer,
				   PVRDRIBufferImpl **ppsAccumBuffer,
				   PVRDRIBufferAttribs *psAttribs,
				   bool *pbDoubleBuffered)
{
	if (!PVRDRIDrawableUpdate(psPVRDrawable, bAllowRecreate))
	{
		if (bAllowRecreate)
		{
			return false;
		}
	}

	if (!PVRDRIDrawableGetParameters(psPVRDrawable,
					 ppsDstBuffer,
					 ppsAccumBuffer))
	{
		return false;
	}

	if (psAttribs)
	{
		psAttribs->uiWidth           = psPVRDrawable->psDRIDrawable->w;
		psAttribs->uiHeight          = psPVRDrawable->psDRIDrawable->h;
		psAttribs->ePixFormat        = psPVRDrawable->ePixelFormat;
		psAttribs->uiStrideInBytes   = psPVRDrawable->uStride;
	}

	if (pbDoubleBuffered)
	{
		*pbDoubleBuffered =
			psPVRDrawable->sConfig.sGLMode.doubleBufferMode;
	}

	return true;
}

bool PVRDRIDrawableQuery(const PVRDRIDrawable *psPVRDrawable,
			 PVRDRIBufferAttrib eBufferAttrib,
			 uint32_t *uiValueOut)
{
	if (!psPVRDrawable || !uiValueOut)
	{
		return false;
	}

	switch (eBufferAttrib)
	{
		case PVRDRI_BUFFER_ATTRIB_TYPE:
			*uiValueOut = (uint32_t) psPVRDrawable->eType;
			return true;
		case PVRDRI_BUFFER_ATTRIB_WIDTH:
			*uiValueOut = (uint32_t) psPVRDrawable->psDRIDrawable->w;
			return true;
		case PVRDRI_BUFFER_ATTRIB_HEIGHT:
			*uiValueOut = (uint32_t) psPVRDrawable->psDRIDrawable->h;
			return true;
		case PVRDRI_BUFFER_ATTRIB_STRIDE:
			*uiValueOut = (uint32_t) psPVRDrawable->uStride;
			return true;
		case PVRDRI_BUFFER_ATTRIB_PIXEL_FORMAT:
			STATIC_ASSERT(sizeof(psPVRDrawable->ePixelFormat) <= sizeof(*uiValueOut));
			*uiValueOut = (uint32_t) psPVRDrawable->ePixelFormat;
			return true;
		case PVRDRI_BUFFER_ATTRIB_INVALID:
			errorMessage("%s: Invalid attribute", __func__);
			assert(0);
			break;
	}

	return false;
}

bool PVRDRIDrawableGetParametersV2(PVRDRIDrawable *psPVRDrawable,
				   uint32_t uiFlags,
				   PVRDRIBufferImpl **ppsDstBuffer,
				   PVRDRIBufferImpl **ppsAccumBuffer)
{
	const bool bNoUpdate = uiFlags & PVRDRI_GETPARAMS_FLAG_NO_UPDATE;

	if (!bNoUpdate)
	{
		const bool bAllowRecreate =
			uiFlags & PVRDRI_GETPARAMS_FLAG_ALLOW_RECREATE;

		if (!PVRDRIDrawableUpdate(psPVRDrawable, bAllowRecreate))
		{
			if (bAllowRecreate)
			{
				return false;
			}
		}
	}

	return PVRDRIDrawableGetParameters(psPVRDrawable,
					   ppsDstBuffer, ppsAccumBuffer);
}
