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
#include "pvr_object_cache.h"

static PVRDRIBufferImpl *PVRGetBackingBuffer(PVRDRIBuffer *psPVRBuffer)
{
	if (psPVRBuffer)
	{
		switch (psPVRBuffer->eBackingType)
		{
			case PVRDRI_BUFFER_BACKING_DRI2:
				return psPVRBuffer->uBacking.sDRI2.psBuffer;
			case PVRDRI_BUFFER_BACKING_IMAGE:
				return PVRDRIImageGetSharedBuffer(psPVRBuffer->uBacking.sImage.psImage);
			default:
				assert(0);
				return NULL;
		}
	}

	return NULL;
}

static void *PVRImageObjectCacheInsert(void *pvCreateData, void *pvInsertData)
{
	__DRIimage *psImage = pvInsertData;
	PVRDRIBuffer *psPVRBuffer;
	(void)pvCreateData;

	assert(PVRDRIImageGetSharedBuffer(psImage) != NULL);

	psPVRBuffer = calloc(1, sizeof(*psPVRBuffer));
	if (psPVRBuffer == NULL)
	{
		errorMessage("%s: Failed to create PVR DRI buffer", __func__);
		return NULL;
	}

	psPVRBuffer->eBackingType = PVRDRI_BUFFER_BACKING_IMAGE;
	psPVRBuffer->uBacking.sImage.psImage = psImage;

	/* As a precaution, take a reference on the image so it doesn't disappear unexpectedly */
	PVRDRIRefImage(psImage);

	return psPVRBuffer;
}

static void PVRImageObjectCachePurge(void *pvCreateData,
				     void *pvObjectData,
				     bool bRetired)
{
	PVRDRIDrawable *psPVRDrawable = pvCreateData;
	PVRDRIBuffer *psPVRBuffer = pvObjectData;

	if (bRetired)
	{
		/*
		 * Delay flush until later, as it may not be safe
		 * to do the flush within GetDrawableInfo.
		 */
		PVRQQueue(&psPVRDrawable->sCacheFlushHead, &psPVRBuffer->sCacheFlushElem);
	}
	else
	{
		PVRDRIUnrefImage(psPVRBuffer->uBacking.sImage.psImage);
		free(psPVRBuffer);
	}
}

static bool PVRImageObjectCacheCompare(void *pvCreateData,
				       void *pvObjectData,
				       void *pvInsertData)
{
	__DRIimage *psImage = pvInsertData;
	PVRDRIBuffer *psPVRBuffer = pvObjectData;
	(void)pvCreateData;

	return psPVRBuffer->uBacking.sImage.psImage == psImage;
}

static void *PVRDRI2ObjectCacheInsert(void *pvCreateData, void *pvInsertData)
{
	PVRDRIDrawable *psPVRDrawable = pvCreateData;
	__DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
	PVRDRIScreen *psPVRScreen = psPVRDrawable->psPVRScreen;
	PVRDRIBuffer *psPVRBuffer;
	__DRIbuffer *psDRIBuffer = pvInsertData;

	if (PVRDRIPixFmtGetBlockSize(psPVRDrawable->ePixelFormat) != psDRIBuffer->cpp)
	{
		errorMessage("%s: DRI buffer format doesn't match drawable format\n", __func__);
		return NULL;
	}

	psPVRBuffer = calloc(1, sizeof(*psPVRBuffer));
	if (psPVRBuffer == NULL)
	{
		errorMessage("%s: Failed to create PVR DRI buffer", __func__);
		return NULL;
	}

	psPVRBuffer->eBackingType = PVRDRI_BUFFER_BACKING_DRI2;
	psPVRBuffer->uBacking.sDRI2.uiName = psDRIBuffer->name;
	psPVRBuffer->uBacking.sDRI2.psBuffer =
			PVRDRIBufferCreateFromName(psPVRScreen->psImpl,
						   psDRIBuffer->name,
						   psDRIDrawable->w,
						   psDRIDrawable->h,
						   psDRIBuffer->pitch,
						   0);
	if (!psPVRBuffer->uBacking.sDRI2.psBuffer)
	{
		free(psPVRBuffer);
		return NULL;
	}

	return psPVRBuffer;
}

static void PVRDRI2ObjectCachePurge(void *pvCreateData,
				    void *pvObjectData,
				    bool bRetired)
{
	PVRDRIDrawable *psPVRDrawable = pvCreateData;
	PVRDRIBuffer *psPVRBuffer = pvObjectData;

	if (bRetired)
	{
		/*
		 * Delay flush until later, as it may not be safe
		 * to do the flush within GetDrawableInfo.
		 */
		PVRQQueue(&psPVRDrawable->sCacheFlushHead, &psPVRBuffer->sCacheFlushElem);
	}
	else
	{
		PVRDRIBufferDestroy(psPVRBuffer->uBacking.sDRI2.psBuffer);
		free(psPVRBuffer);
	}
}

static bool PVRDRI2ObjectCacheCompare(void *pvCreateData,
				      void *pvObjectData,
				      void *pvInsertData)
{
	__DRIbuffer *psDRIBuffer = pvInsertData;
	PVRDRIBuffer *psPVRBuffer = pvObjectData;
	(void)pvCreateData;

	return psDRIBuffer->name == psPVRBuffer->uBacking.sDRI2.uiName;
}


/*************************************************************************/ /*!
 PVR drawable local functions
*/ /**************************************************************************/

static inline void PVRDRIMarkAllRenderSurfacesAsInvalid(PVRDRIDrawable *psPVRDrawable)
{
	PVRQElem *psQElem = psPVRDrawable->sPVRContextHead.pvForw;

	while (psQElem != &psPVRDrawable->sPVRContextHead)
	{
		PVRDRIContext *psPVRContext = PVRQ_CONTAINER_OF(psQElem, PVRDRIContext, sQElem);
		PVRDRIEGLMarkRendersurfaceInvalid(psPVRContext->eAPI,
						  psPVRContext->psPVRScreen->psImpl,
						  psPVRContext->psImpl);
		psQElem = psPVRContext->sQElem.pvForw;
	}

	/* No need to flush surfaces evicted from the cache */
	INITIALISE_PVRQ_HEAD(&psPVRDrawable->sCacheFlushHead);
}

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

	if (psPVRDrawable->bDoubleBuffered)
	{
		uiBufferMask = __DRI_IMAGE_BUFFER_BACK;
	}
	else
	{
		uiBufferMask = __DRI_IMAGE_BUFFER_FRONT;
	}

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

	psPVRDrawable->uDRI.sImage.psDRI =
		(sImages.image_mask & __DRI_IMAGE_BUFFER_BACK) ?
			sImages.back : sImages.front;

	return true;
}

static bool PVRDRI2DrawableGetNativeInfo(PVRDRIDrawable *psPVRDrawable)
{
	__DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
	__DRIscreen *psDRIScreen = psPVRDrawable->psPVRScreen->psDRIScreen;
	__DRIbuffer *psDRIBuffers;
	int i;
	int iBufCount;
	int w;
	int h;
	unsigned int auiAttachmentReq[2];

	assert(psDRIScreen->dri2.loader);
	assert(psDRIScreen->dri2.loader->getBuffersWithFormat);

	auiAttachmentReq[0] = psPVRDrawable->bDoubleBuffered;
	auiAttachmentReq[1] = PVRDRIPixFmtGetDepth(psPVRDrawable->ePixelFormat);

	psDRIBuffers = psDRIScreen->dri2.loader->getBuffersWithFormat(
			       psDRIDrawable, &w, &h, auiAttachmentReq,
			       1, &iBufCount, psDRIDrawable->loaderPrivate);

	if (!psDRIBuffers)
	{
		errorMessage("%s: DRI2 get buffers call failed\n",
			     __func__);
		return false;
	}

	for (i = 0; i < iBufCount; i++)
	{
		if (psDRIBuffers->attachment == auiAttachmentReq[0] ||
		    (psDRIBuffers->attachment == __DRI_BUFFER_FAKE_FRONT_LEFT &&
		     auiAttachmentReq[0] == 0))
		{
			break;
		}

		psDRIBuffers++;
	}

	if (iBufCount)
	{
		psPVRDrawable->uDRI.sBuffer.sDRI.attachment =
				psDRIBuffers->attachment;
		psPVRDrawable->uDRI.sBuffer.sDRI.name = psDRIBuffers->name;
		psPVRDrawable->uDRI.sBuffer.sDRI.pitch = psDRIBuffers->pitch;
		psPVRDrawable->uDRI.sBuffer.sDRI.cpp = psDRIBuffers->cpp;
		psPVRDrawable->uDRI.sBuffer.sDRI.flags = psDRIBuffers->flags;
		psPVRDrawable->uDRI.sBuffer.w = w;
		psPVRDrawable->uDRI.sBuffer.h = h;
		return true;
	}

	errorMessage("%s: Couldn't get DRI buffer information\n", __func__);

	return false;
}

static bool PVRDRIDrawableUpdateNativeInfo(PVRDRIDrawable *psPVRDrawable)
{
  if (psPVRDrawable->psPVRScreen->psDRIScreen->image.loader)
    return PVRImageDrawableGetNativeInfo(psPVRDrawable);
  else
    return PVRDRI2DrawableGetNativeInfo(psPVRDrawable);
}

static bool PVRDRI2DrawableRecreate(PVRDRIDrawable *psPVRDrawable)
{
	__DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
	PVRDRIBuffer *psPVRBuffer = NULL;

	if (!psPVRDrawable->bDoubleBuffered)
	{
		psPVRBuffer = PVRObjectCacheGetObject(
				      psPVRDrawable->hBufferCache, 0);
	}

	if (psPVRDrawable->bDoubleBuffered ||
	    !psPVRBuffer ||
	    psPVRBuffer->uBacking.sDRI2.uiName ==
	    psPVRDrawable->uDRI.sBuffer.sDRI.name)
	{
		if (psDRIDrawable->w == psPVRDrawable->uDRI.sBuffer.w &&
		    psDRIDrawable->h == psPVRDrawable->uDRI.sBuffer.h &&
		    psPVRDrawable->uStride ==
		    psPVRDrawable->uDRI.sBuffer.sDRI.pitch &&
		    psPVRDrawable->uBytesPerPixel ==
		    psPVRDrawable->uDRI.sBuffer.sDRI.cpp)
		{
			if (!PVRObjectCacheInsert(psPVRDrawable->hBufferCache,
						  &psPVRDrawable->uDRI))
			{
				goto ErrorCacheInsert;
			}
			return true;
		}
	}

	PVRDRIMarkAllRenderSurfacesAsInvalid(psPVRDrawable);
	PVRObjectCachePurge(psPVRDrawable->hBufferCache);

	psDRIDrawable->w = psPVRDrawable->uDRI.sBuffer.w;
	psDRIDrawable->h = psPVRDrawable->uDRI.sBuffer.h;
	psPVRDrawable->uStride = psPVRDrawable->uDRI.sBuffer.sDRI.pitch;
	psPVRDrawable->uBytesPerPixel = psPVRDrawable->uDRI.sBuffer.sDRI.cpp;

	if (!PVRObjectCacheInsert(psPVRDrawable->hBufferCache,
				 &psPVRDrawable->uDRI))
	{
		goto ErrorCacheInsert;
	}

	if (!PVREGLDrawableRecreate(psPVRDrawable->psPVRScreen->psImpl,
				    psPVRDrawable->psImpl))
	{
		errorMessage("%s: Couldn't recreate EGL drawable\n",
			     "PVRDRI2DrawableRecreate");
		return false;
	}

	return true;

ErrorCacheInsert:
	errorMessage("%s: Couldn't insert buffer into cache\n", __func__);

	return false;
}

static bool PVRImageDrawableRecreate(PVRDRIDrawable *psPVRDrawable)
{
	unsigned int uBytesPerPixel;
	PVRDRIBuffer *psPVRBuffer = NULL;
	PVRDRIBufferAttribs sAttribs;

	PVRDRIEGLImageGetAttribs(PVRDRIImageGetEGLImage(
					 psPVRDrawable->uDRI.sImage.psDRI),
				 &sAttribs);
	uBytesPerPixel = PVRDRIPixFmtGetBlockSize(sAttribs.ePixFormat);

	if (!psPVRDrawable->bDoubleBuffered)
	{
		psPVRBuffer = PVRObjectCacheGetObject(
				      psPVRDrawable->hBufferCache, 0);
	}

	if ((psPVRBuffer && psPVRBuffer->uBacking.sImage.psImage !=
	     psPVRDrawable->uDRI.sImage.psDRI) ||
	    psPVRDrawable->psDRIDrawable->w != sAttribs.uiWidth ||
	    psPVRDrawable->psDRIDrawable->h != sAttribs.uiHeight ||
	    psPVRDrawable->uStride != sAttribs.uiStrideInBytes ||
	    psPVRDrawable->uBytesPerPixel != uBytesPerPixel)
	{
		PVRDRIMarkAllRenderSurfacesAsInvalid(psPVRDrawable);
		PVRObjectCachePurge(psPVRDrawable->hBufferCache);

		psPVRDrawable->psDRIDrawable->w = sAttribs.uiWidth;
		psPVRDrawable->psDRIDrawable->h = sAttribs.uiHeight;
		psPVRDrawable->uStride = sAttribs.uiStrideInBytes;
		psPVRDrawable->uBytesPerPixel = uBytesPerPixel;

		if (!PVRObjectCacheInsert(psPVRDrawable->hBufferCache,
					  psPVRDrawable->uDRI.sImage.psDRI))
		{
			goto ErrorCacheInsert;
		}

		if (!PVREGLDrawableRecreate(psPVRDrawable->psPVRScreen->psImpl,
					    psPVRDrawable->psImpl))
		{
			errorMessage("%s: Couldn't recreate EGL drawable\n",
				     __func__);
			return false;
		}

		return true;
	}


	if (PVRObjectCacheInsert(psPVRDrawable->hBufferCache,
				 psPVRDrawable->uDRI.sImage.psDRI))
	{
		return true;
	}

ErrorCacheInsert:
	errorMessage("%s: Couldn't insert buffer into cache\n", __func__);

	return false;
}

/*************************************************************************/ /*!
 Function Name	: PVRDRIDrawableRecreate
 Inputs		: psPVRDrawable
 Description	: Recreate drawable
*/ /**************************************************************************/
bool PVRDRIDrawableRecreate(PVRDRIDrawable *psPVRDrawable)
{
	bool rv = true;
	PVRDRIDrawableLock(psPVRDrawable);

	if (psPVRDrawable->psPVRScreen->bUseInvalidate &&
	    !psPVRDrawable->bDrawableInfoInvalid)
	{
		PVRDRIDrawableUnlock(psPVRDrawable);
		return true;
	}


	if (!psPVRDrawable->bDrawableInfoUpdated)
	{
		rv = PVRDRIDrawableUpdateNativeInfo(psPVRDrawable);
	}

	if (rv)
	{
		if (!psPVRDrawable->psPVRScreen->psDRIScreen->image.loader)
		{
			rv = PVRDRI2DrawableRecreate(psPVRDrawable);
		}
		else
		{
			rv = PVRImageDrawableRecreate(psPVRDrawable);
		}
	}


	if (rv)
		psPVRDrawable->bDrawableInfoInvalid = false;

	PVRDRIDrawableUnlock(psPVRDrawable);

	return rv;
}

/*************************************************************************/ /*!
 PVR drawable interface
*/ /**************************************************************************/

static bool PVRImageDrawableCreate(PVRDRIDrawable *psPVRDrawable)
{
	IMGEGLImage *psEGLImage;
	PVRDRIBufferAttribs sAttribs;

	if (!PVRImageDrawableGetNativeInfo(psPVRDrawable))
		return false;

	psEGLImage = PVRDRIImageGetEGLImage(psPVRDrawable->uDRI.sImage.psDRI);
	PVRDRIEGLImageGetAttribs(psEGLImage, &sAttribs);

	psPVRDrawable->psDRIDrawable->w = sAttribs.uiWidth;
	psPVRDrawable->psDRIDrawable->h = sAttribs.uiHeight;
	psPVRDrawable->uStride = sAttribs.uiStrideInBytes;
	psPVRDrawable->uBytesPerPixel =
			PVRDRIPixFmtGetBlockSize(sAttribs.ePixFormat);

	if (!PVRObjectCacheInsert(psPVRDrawable->hBufferCache,
				  psPVRDrawable->uDRI.sImage.psDRI) )
	{
		errorMessage("%s: Couldn't insert buffer into cache\n",
			     __func__);
		return false;
	}

	if (!PVREGLDrawableCreate(psPVRDrawable->psPVRScreen->psImpl,
				  psPVRDrawable->psImpl))
	{
		errorMessage("%s: Couldn't create EGL drawable\n", __func__);
		return false;
	}

	return true;
}

static bool PVRDRI2DrawableCreate(PVRDRIDrawable *psPVRDrawable)
{
	if (!PVRDRI2DrawableGetNativeInfo(psPVRDrawable))
		return false;

	psPVRDrawable->psDRIDrawable->w = psPVRDrawable->uDRI.sBuffer.w;
	psPVRDrawable->psDRIDrawable->h = psPVRDrawable->uDRI.sBuffer.h;
	psPVRDrawable->uStride = psPVRDrawable->uDRI.sBuffer.sDRI.pitch;
	psPVRDrawable->uBytesPerPixel = psPVRDrawable->uDRI.sBuffer.sDRI.cpp;

	if (!PVRObjectCacheInsert(psPVRDrawable->hBufferCache,
				  &psPVRDrawable->uDRI))
	{
		errorMessage("%s: Couldn't insert buffer into cache\n",
			     __func__);
		return false;
	}
	if (!PVREGLDrawableCreate(psPVRDrawable->psPVRScreen->psImpl,
				  psPVRDrawable->psImpl))
	{
		errorMessage("%s: Couldn't create EGL drawable\n", __func__);
		return false;
	}

	return true;
}

bool PVRDRIDrawableInit(PVRDRIDrawable *psPVRDrawable)
{
	unsigned uNumBufs = psPVRDrawable->bDoubleBuffered ?
				    DRI2_BUFFERS_MAX : 1;
	PVRObjectCacheInsertCB pfnInsert;
	PVRObjectCachePurgeCB pfnPurge;
	PVRObjectCacheCompareCB pfnCompare;

	if (psPVRDrawable->bInitialised)
	{
		return true;
	}

	if (psPVRDrawable->psPVRScreen->psDRIScreen->image.loader)
	{
		pfnInsert = PVRImageObjectCacheInsert;
		pfnPurge = PVRImageObjectCachePurge;
		pfnCompare = PVRImageObjectCacheCompare;
	}
	else
	{
		assert(psPVRDrawable->psPVRScreen->psDRIScreen->dri2.loader);

		pfnInsert = PVRDRI2ObjectCacheInsert;
		pfnPurge = PVRDRI2ObjectCachePurge;
		pfnCompare = PVRDRI2ObjectCacheCompare;
	}

	psPVRDrawable->hBufferCache = PVRObjectCacheCreate(uNumBufs,
							   uNumBufs,
							   psPVRDrawable,
							   pfnInsert,
							   pfnPurge,
							   pfnCompare);
	if (psPVRDrawable->hBufferCache == NULL)
	{
		errorMessage("%s: Failed to create buffer cache\n", __func__);
		return false;
	}


	if (psPVRDrawable->psPVRScreen->psDRIScreen->image.loader)
	{
		if (!PVRImageDrawableCreate(psPVRDrawable))
			goto ErrorCacheDestroy;
	}
	else
	{
		if (!PVRDRI2DrawableCreate(psPVRDrawable))
			goto ErrorCacheDestroy;
	}

	psPVRDrawable->bInitialised = true;

	return true;

ErrorCacheDestroy:
	PVRObjectCacheDestroy(psPVRDrawable->hBufferCache);
	psPVRDrawable->hBufferCache = NULL;

	return false;
}

void PVRDRIDrawableDeinit(PVRDRIDrawable *psPVRDrawable)
{
	(void) PVREGLDrawableDestroy(psPVRDrawable->psPVRScreen->psImpl,
				     psPVRDrawable->psImpl);

	if (psPVRDrawable->hBufferCache)
	{
		PVRObjectCacheDestroy(psPVRDrawable->hBufferCache);
		psPVRDrawable->hBufferCache = NULL;
	}

	psPVRDrawable->bInitialised = false;
}

bool PVRDRIDrawableGetParameters(PVRDRIDrawable *psPVRDrawable,
				 PVRDRIBufferImpl **ppsDstBuffer,
				 PVRDRIBufferImpl **ppsAccumBuffer,
				 PVRDRIBufferAttribs *psAttribs,
				 bool *pbDoubleBuffered)
{
	if (ppsDstBuffer || ppsAccumBuffer)
	{
		PVRDRIBuffer *psPVRBuffer;
		PVRDRIBufferImpl *psDstBuffer;

		psPVRBuffer = PVRObjectCacheGetObject(
				      psPVRDrawable->hBufferCache, 0);
		psDstBuffer = PVRGetBackingBuffer(psPVRBuffer);

		if (!psDstBuffer)
		{
			errorMessage("%s: Couldn't get render buffer from cache\n",
				     __func__);
			return false;
		}

		if (ppsDstBuffer)
		{
			*ppsDstBuffer = psDstBuffer;
		}

		if (!ppsDstBuffer || ppsAccumBuffer)
		{
			psPVRBuffer = PVRObjectCacheGetObject(
					      psPVRDrawable->hBufferCache, 1);
			*ppsAccumBuffer = PVRGetBackingBuffer(psPVRBuffer);

			if (!*ppsAccumBuffer)
				*ppsAccumBuffer = psDstBuffer;
		}
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
		*pbDoubleBuffered = psPVRDrawable->bDoubleBuffered;
	}

	return true;
}
