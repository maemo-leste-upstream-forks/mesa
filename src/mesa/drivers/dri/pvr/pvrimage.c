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
#include <xf86drm.h>

#include "util/u_atomic.h"
#include "dri_util.h"

#include "img_drm_fourcc.h"
#include "pvrdri.h"
#include "pvrimage.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

struct PVRDRIImageShared
{
	int iRefCount;

	PVRDRIScreen *psPVRScreen;

	PVRDRIImageType eType;
	const PVRDRIImageFormat *psFormat;
	IMG_YUV_COLORSPACE eColourSpace;
	IMG_YUV_CHROMA_INTERP eChromaUInterp;
	IMG_YUV_CHROMA_INTERP eChromaVInterp;

	PVRDRIBufferImpl *psBuffer;
	IMGEGLImage *psEGLImage;
	PVRDRIEGLImageType eglImageType;
	struct PVRDRIImageShared *psAncestor;
};

struct __DRIimageRec
{
	int iRefCount;

	void *loaderPrivate;

	struct PVRDRIImageShared *psShared;

	IMGEGLImage *psEGLImage;
};


static struct PVRDRIImageShared *
CommonImageSharedSetup(PVRDRIScreen *psPVRScreen, PVRDRIImageType eType)
{
	struct PVRDRIImageShared *shared;

	shared = calloc(1, sizeof(*shared));
	if (!shared)
	{
		return NULL;
	}

	shared->psPVRScreen = psPVRScreen;
	shared->eType = eType;
	shared->iRefCount = 1;

	assert(shared->eColourSpace == IMG_COLORSPACE_UNDEFINED &&
	       shared->eChromaUInterp == IMG_CHROMA_INTERP_UNDEFINED &&
	       shared->eChromaVInterp == IMG_CHROMA_INTERP_UNDEFINED);

	return shared;
}

static void DestroyImageShared(struct PVRDRIImageShared *shared)
{
	int iRefCount = p_atomic_dec_return(&shared->iRefCount);

	assert(iRefCount >= 0);

	if (iRefCount > 0)
	{
		return;
	}

	switch (shared->eType)
	{
		case PVRDRI_IMAGE_FROM_NAMES:
		case PVRDRI_IMAGE_FROM_DMABUFS:
		case PVRDRI_IMAGE:
			if (shared->psBuffer)
			{
				PVRDRIBufferDestroy(shared->psBuffer);
			}
			assert(!shared->psAncestor);
			free(shared);
			return;
		case PVRDRI_IMAGE_FROM_EGLIMAGE:
			PVRDRIEGLImageDestroyExternal(shared->psPVRScreen->psImpl,
			                              shared->psEGLImage,
						      shared->eglImageType);
			free(shared);
			return;
		case PVRDRI_IMAGE_SUBIMAGE:
			if (shared->psBuffer)
			{
				PVRDRIBufferDestroy(shared->psBuffer);
			}
			if (shared->psAncestor)
			{
				DestroyImageShared(shared->psAncestor);
			}
			free(shared);
			return;
	}

	assert(!"unknown image type");
	free(shared);
}

static struct PVRDRIImageShared *
CreateImageSharedFromEGLImage(__DRIscreen *screen,
                              IMGEGLImage *psEGLImage,
			      PVRDRIEGLImageType eglImageType)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(screen);
	struct PVRDRIImageShared *shared;
	PVRDRIBufferAttribs sAttribs;
	const PVRDRIImageFormat *psFormat;

	PVRDRIEGLImageGetAttribs(psEGLImage, &sAttribs);

	psFormat = PVRDRIIMGPixelFormatToImageFormat(psPVRScreen,
						     sAttribs.ePixFormat);
	if (!psFormat)
	{
		return NULL;
	}

	shared = CommonImageSharedSetup(psPVRScreen, PVRDRI_IMAGE_FROM_EGLIMAGE);
	if (!shared)
	{
		return NULL;
	}

	shared->psEGLImage = psEGLImage;
	shared->psFormat = psFormat;
	shared->eglImageType = eglImageType;

	return shared;
}

static struct PVRDRIImageShared *
CreateImageSharedFromNames(__DRIscreen *screen,
			   int width,
			   int height,
			   int fourcc,
			   int *names,
			   int num_names,
			   int *strides,
			   int *offsets)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(screen);
	struct PVRDRIImageShared *shared;
	const PVRDRIImageFormat *psFormat;
	unsigned auiWidthShift[DRI_PLANES_MAX];
	unsigned auiHeightShift[DRI_PLANES_MAX];
	int i;

	psFormat = PVRDRIFourCCToImageFormat(psPVRScreen, fourcc);
	if (!psFormat)
	{
		errorMessage("%s: Unsupported DRI FourCC (fourcc = 0x%X)\n",
			     __func__, fourcc);
		return NULL;
	}

	if (psFormat->uiNumPlanes < num_names)
	{
		errorMessage("%s: Unexpected number of names for DRI FourCC (names = %d, fourcc = 0x%X)\n",
			     __func__, num_names, fourcc);
		return NULL;
	}

	for (i = 0; i < num_names; i++)
	{
		if (offsets[i] < 0)
		{
			errorMessage("%s: Offset %d unsupported (value = %d)\n",
				     __func__, i, offsets[i]);
			return NULL;
		}

		auiWidthShift[i] = psFormat->sPlanes[i].uiWidthShift;
		auiHeightShift[i] = psFormat->sPlanes[i].uiHeightShift;
	}

	shared = CommonImageSharedSetup(psPVRScreen, PVRDRI_IMAGE_FROM_NAMES);
	if (!shared)
	{
		return NULL;
	}

	shared->psBuffer = PVRDRIBufferCreateFromNames(psPVRScreen->psImpl,
						        width,
						        height,
						        num_names,
						        names,
						        strides,
						        offsets,
						        auiWidthShift,
						        auiHeightShift);

	if (!shared->psBuffer)
	{
		errorMessage("%s: Failed to create buffer for shared image\n", __func__);
		goto ErrorDestroyImage;
	}

	shared->psFormat = psFormat;
	shared->eColourSpace =
		PVRDRIToIMGColourSpace(psFormat,
				       __DRI_YUV_COLOR_SPACE_UNDEFINED,
				       __DRI_YUV_RANGE_UNDEFINED);
	shared->eChromaUInterp =
		PVRDRIChromaSittingToIMGInterp(psFormat,
					       __DRI_YUV_CHROMA_SITING_UNDEFINED);
	shared->eChromaVInterp =
		PVRDRIChromaSittingToIMGInterp(psFormat,
					       __DRI_YUV_CHROMA_SITING_UNDEFINED);

	return shared;

ErrorDestroyImage:
	DestroyImageShared(shared);

	return NULL;
}

static struct PVRDRIImageShared *
CreateImageSharedFromDmaBufs(__DRIscreen *screen,
			     int width,
			     int height,
			     int fourcc,
			     uint64_t modifier,
			     int *fds,
			     int num_fds,
			     int *strides,
			     int *offsets,
			     enum __DRIYUVColorSpace color_space,
			     enum __DRISampleRange sample_range,
			     enum __DRIChromaSiting horiz_siting,
			     enum __DRIChromaSiting vert_siting,
			     unsigned *error)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(screen);
	struct PVRDRIImageShared *shared;
	const PVRDRIImageFormat *psFormat;
	unsigned auiWidthShift[DRI_PLANES_MAX];
	unsigned auiHeightShift[DRI_PLANES_MAX];
	int i;

	psFormat = PVRDRIFourCCToImageFormat(psPVRScreen, fourcc);
	if (!psFormat)
	{
		errorMessage("%s: Unsupported DRI FourCC (fourcc = 0x%X)\n",
			     __func__, fourcc);
		*error = __DRI_IMAGE_ERROR_BAD_MATCH;
		return NULL;
	}

	/* When a modifier isn't specified, skip the validation */
	if (modifier != DRM_FORMAT_MOD_INVALID)
	{
		/*
		 * The modifier validation has to be done in this "higher" level
		 * function instead of pvr_dri_support. The support for
		 * modifiers is done on per format basis, but there is no way
		 * to pass the format information down to the plane creation API
		 * in pvr_dri_support.
		 */
		if (!PVRDRIValidateImageModifier(psPVRScreen, fourcc, modifier))
		{
			errorMessage("%s: Unsupported mod (fmt = %#x, mod = %"PRIx64")\n",
				     __func__, fourcc, modifier);
			*error = __DRI_IMAGE_ERROR_BAD_MATCH;
			return NULL;
		}
	}

	if (psFormat->uiNumPlanes < num_fds)
	{
		errorMessage("%s: Unexpected number of fds for format (fds = %d, fourcc = 0x%X)\n",
			     __func__, num_fds, fourcc);
		*error = __DRI_IMAGE_ERROR_BAD_MATCH;
		return NULL;
	}

	for (i = 0; i < num_fds; i++)
	{
		if (offsets[i] < 0)
		{
			errorMessage("%s: Offset %d unsupported (value = %d)\n",
				     __func__, i, offsets[i]);
			*error = __DRI_IMAGE_ERROR_BAD_ACCESS;
			return NULL;
		}

		auiWidthShift[i] = psFormat->sPlanes[i].uiWidthShift;
		auiHeightShift[i] = psFormat->sPlanes[i].uiHeightShift;
	}

	shared = CommonImageSharedSetup(psPVRScreen, PVRDRI_IMAGE_FROM_DMABUFS);
	if (!shared)
	{
		*error = __DRI_IMAGE_ERROR_BAD_ALLOC;
		return NULL;
	}

	shared->psBuffer = PVRDRIBufferCreateFromFdsWithModifier(psPVRScreen->psImpl,
								  width,
								  height,
								  modifier,
								  num_fds,
								  fds,
								  strides,
								  offsets,
								  auiWidthShift,
								  auiHeightShift);

	if (!shared->psBuffer)
	{
		errorMessage("%s: Failed to create buffer for shared image\n", __func__);
		*error = __DRI_IMAGE_ERROR_BAD_ALLOC;
		goto ErrorDestroyImage;
	}

	shared->psFormat = psFormat;
	shared->eColourSpace = PVRDRIToIMGColourSpace(psFormat, color_space, sample_range);
	shared->eChromaUInterp = PVRDRIChromaSittingToIMGInterp(psFormat, horiz_siting);
	shared->eChromaVInterp = PVRDRIChromaSittingToIMGInterp(psFormat, vert_siting);

	return shared;

ErrorDestroyImage:
	DestroyImageShared(shared);

	return NULL;
}

static struct PVRDRIImageShared *
CreateImageShared(__DRIscreen *screen,
                  int width,
                  int height,
                  int format,
                  unsigned int use,
                  int *piStride)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(screen);
	struct PVRDRIImageShared *shared;
	const PVRDRIImageFormat *psFormat;
	unsigned int uiStride;

	if ((use & __DRI_IMAGE_USE_CURSOR) && (use & __DRI_IMAGE_USE_SCANOUT))
	{
		return NULL;
	}

	psFormat = PVRDRIFormatToImageFormat(psPVRScreen, format);
	if (!psFormat)
	{
		errorMessage("%s: Unsupported DRI image format (format = 0x%X)\n",
			     __func__, format);
		return NULL;
	}

	if (psFormat->uiNumPlanes != 1)
	{
		errorMessage("%s: Only single plane formats are supported (format 0x%X has %u planes)\n",
			     __func__, format, psFormat->uiNumPlanes);
		return NULL;
	}

	shared = CommonImageSharedSetup(psPVRScreen, PVRDRI_IMAGE);
	if (!shared)
	{
		return NULL;
	}

	shared->psBuffer =
		PVRDRIBufferCreate(psPVRScreen->psImpl,
				   width,
				   height,
				   PVRDRIPixFmtGetBPP(psFormat->eIMGPixelFormat),
				   use,
				   &uiStride);
	if (!shared->psBuffer)
	{
		errorMessage("%s: Failed to create buffer\n", __func__);
		goto ErrorDestroyImage;
	}

	shared->psFormat = psFormat;

	*piStride = uiStride;

	return shared;

ErrorDestroyImage:
	DestroyImageShared(shared);

	return NULL;
}

static struct PVRDRIImageShared *
CreateImageSharedWithModifiers(__DRIscreen *screen,
			       int width,
			       int height,
			       int format,
			       const uint64_t *modifiers,
			       unsigned int modifier_count,
			       int *piStride)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(screen);
	struct PVRDRIImageShared *shared;
	const PVRDRIImageFormat *psFormat;
	unsigned int uiStride;

	psFormat = PVRDRIFormatToImageFormat(psPVRScreen, format);
	if (!psFormat)
	{
		errorMessage("%s: Unsupported DRI image format (format = 0x%X)\n",
			     __func__, format);
		return NULL;
	}

	shared = CommonImageSharedSetup(psPVRScreen, PVRDRI_IMAGE);
	if (!shared)
	{
		return NULL;
	}

	shared->psBuffer = PVRDRIBufferCreateWithModifiers(psPVRScreen->psImpl,
							   width,
							   height,
							   psFormat->iDRIFourCC,
							   psFormat->eIMGPixelFormat,
							   modifiers,
							   modifier_count,
							   &uiStride);
	if (!shared->psBuffer)
	{
		errorMessage("%s: Failed to create buffer\n", __func__);
		goto ErrorDestroyImage;
	}

	shared->psFormat = psFormat;

	*piStride = uiStride;

	return shared;

ErrorDestroyImage:
	DestroyImageShared(shared);

	return NULL;
}

static struct PVRDRIImageShared *RefImageShared(struct PVRDRIImageShared *shared)
{
	int iRefCount = p_atomic_inc_return(&shared->iRefCount);

	(void)iRefCount;
	assert(iRefCount > 1);

	return shared;
}

static struct PVRDRIImageShared *
CreateImageSharedForSubImage(struct PVRDRIImageShared *psParent, int plane)
{
	struct PVRDRIImageShared *shared;
	struct PVRDRIImageShared *psAncestor;
	PVRDRIBufferImpl *psBuffer = NULL;

	/* Sub-images represent a single plane in the parent image */
	if (!psParent->psBuffer)
	{
		return NULL;
	}

	/*
	 * The ancestor image is the owner of the original buffer that will
	 * back the new image. The parent image may be a child of that image
	 * itself. The ancestor image must not be destroyed until all the
	 * child images that refer to it have been destroyed. A reference
	 * will be taken on the ancestor to ensure that is the case.
	 * We must distinguish between the parent's buffer and the ancestor's
	 * buffer. For example, plane 0 in the parent is not necessarily plane
	 * 0 in the ancestor.
	 */
	psAncestor = psParent;
	if (psAncestor->psAncestor)
	{
		psAncestor = psAncestor->psAncestor;

		assert(!psAncestor->psAncestor);
	}

	psBuffer = PVRDRISubBufferCreate(psParent->psPVRScreen->psImpl,
					 psParent->psBuffer,
					 plane);
	/*
	 * Older versions of PVR DRI Support don't support
	 * PVRDRISubBufferCreate.
	 */
	if (!psBuffer)
	{
		return NULL;
	}

	shared = CommonImageSharedSetup(NULL, PVRDRI_IMAGE_SUBIMAGE);
	if (!shared)
	{
		goto ErrorDestroyBuffer;
	}

	shared->psAncestor = RefImageShared(psAncestor);
	shared->psBuffer = psBuffer;
	shared->psPVRScreen = psParent->psPVRScreen;

	shared->psFormat = PVRDRIIMGPixelFormatToImageFormat(psParent->psPVRScreen,
							     psParent->psFormat->sPlanes[plane].eIMGPixelFormat);
	assert(shared->psFormat);

	return shared;

ErrorDestroyBuffer:
	PVRDRIBufferDestroy(psBuffer);
	return NULL;
}

static __DRIimage *CommonImageSetup(void *loaderPrivate)
{
	__DRIimage *image;

	image = calloc(1, sizeof(*image));
	if (!image)
	{
		return NULL;
	}

	image->loaderPrivate = loaderPrivate;
	image->iRefCount = 1;

	return image;
}

void PVRDRIDestroyImage(__DRIimage *image)
{
	int iRefCount = p_atomic_dec_return(&image->iRefCount);

	assert(iRefCount >= 0);

	if (iRefCount > 0)
	{
		return;
	}

	if (image->psShared)
	{
		DestroyImageShared(image->psShared);
	}

	PVRDRIEGLImageFree(image->psEGLImage);

	free(image);
}

__DRIimage *PVRDRICreateImageFromName(__DRIscreen *screen,
				      int width, int height, int format,
				      int name, int pitch,
				      void *loaderPrivate)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(screen);
	const PVRDRIImageFormat *psFormat;
	int iStride;
	int iOffset;

	psFormat = PVRDRIFormatToImageFormat(psPVRScreen, format);
	if (!psFormat)
	{
		errorMessage("%s: Unsupported DRI image format (format = 0x%X)\n",
			     __func__, format);
		return NULL;
	}

	iStride = pitch * PVRDRIPixFmtGetBlockSize(psFormat->eIMGPixelFormat);
	iOffset = 0;

	return PVRDRICreateImageFromNames(screen, width, height, psFormat->iDRIFourCC,
					  &name, 1, &iStride, &iOffset, loaderPrivate);
}

__DRIimage *PVRDRICreateImageFromRenderbuffer2(__DRIcontext *context,
					      int           renderbuffer,
                                              void         *loaderPrivate,
					      unsigned     *error)
{
	PVRDRIContext *psPVRContext = context->driverPrivate;
	__DRIscreen *screen = psPVRContext->psPVRScreen->psDRIScreen;
	unsigned e;
	IMGEGLImage *psEGLImage;
	__DRIimage *image;

	image = CommonImageSetup(loaderPrivate);
	if (!image)
	{
		*error = __DRI_IMAGE_ERROR_BAD_ALLOC;
		return NULL;
	}

	psEGLImage = PVRDRIEGLImageCreate();
	if (!psEGLImage)
	{
		PVRDRIDestroyImage(image);

		*error = __DRI_IMAGE_ERROR_BAD_ALLOC;
		return NULL;
	}

	e = PVRDRIGetImageSource(psPVRContext->eAPI,
	                         psPVRContext->psPVRScreen->psImpl,
	                         psPVRContext->psImpl,
	                         EGL_GL_RENDERBUFFER_KHR,
	                         (uintptr_t)renderbuffer,
	                         0,
	                         psEGLImage);

	if (e != PVRDRI_IMAGE_ERROR_SUCCESS)
	{
		PVRDRIEGLImageFree(psEGLImage);
		PVRDRIDestroyImage(image);

		*error = e;
		return NULL;
	}

	PVRDRIEGLImageSetCallbackData(psEGLImage, image);
	
	/*
	 * We can't destroy the image after this point, as the
	 * renderbuffer now has a reference to it.
	 */
	image->psShared = CreateImageSharedFromEGLImage(screen,
							psEGLImage,
							PVRDRI_EGLIMAGE_IMGEGL);
	if (!image->psShared)
	{
		*error = __DRI_IMAGE_ERROR_BAD_ALLOC;
		return NULL;
	}

	image->psEGLImage = PVRDRIEGLImageDup(image->psShared->psEGLImage);
	if (!image->psEGLImage)
	{
		*error = __DRI_IMAGE_ERROR_BAD_ALLOC;
		return NULL;
	}

	image->iRefCount++;

	*error = __DRI_IMAGE_ERROR_SUCCESS;
	return image;
}

__DRIimage *PVRDRICreateImageFromRenderbuffer(__DRIcontext *context,
                                              int           renderbuffer,
                                              void         *loaderPrivate)
{
	unsigned error;

	return PVRDRICreateImageFromRenderbuffer2(context,
						  renderbuffer,
						  loaderPrivate,
						  &error);
}

__DRIimage *PVRDRICreateImage(__DRIscreen *screen,
			      int width, int height, int format,
			      unsigned int use,
			      void *loaderPrivate)
{
	__DRIimage *image;
	int iStride;

	image = CommonImageSetup(loaderPrivate);
	if (!image)
	{
		return NULL;
	}

	image->psShared = CreateImageShared(screen, width, height, format, use, &iStride);
	if (!image->psShared)
	{
		PVRDRIDestroyImage(image);
		return NULL;
	}

	image->psEGLImage = PVRDRIEGLImageCreateFromBuffer(width, height, iStride,
							    image->psShared->psFormat->eIMGPixelFormat,
							    image->psShared->eColourSpace,
							    image->psShared->eChromaUInterp,
							    image->psShared->eChromaVInterp,
							    image->psShared->psBuffer);
	if (!image->psEGLImage)
	{
		PVRDRIDestroyImage(image);
		return NULL;
	}

	PVRDRIEGLImageSetCallbackData(image->psEGLImage, image);

	return image;
}

__DRIimage *PVRDRICreateImageWithModifiers(__DRIscreen *screen,
					   int width, int height, int format,
					   const uint64_t *modifiers,
					   const unsigned int modifier_count,
					   void *loaderPrivate)
{
	__DRIimage *image;
	int iStride;

	image = CommonImageSetup(loaderPrivate);
	if (!image)
	{
		return NULL;
	}

	image->psShared = CreateImageSharedWithModifiers(screen, width, height, format,
							 modifiers, modifier_count,
							 &iStride);
	if (!image->psShared)
	{
		PVRDRIDestroyImage(image);
		return NULL;
	}

	image->psEGLImage = PVRDRIEGLImageCreateFromBuffer(width, height, iStride,
							    image->psShared->psFormat->eIMGPixelFormat,
							    image->psShared->eColourSpace,
							    image->psShared->eChromaUInterp,
							    image->psShared->eChromaVInterp,
							    image->psShared->psBuffer);
	if (!image->psEGLImage)
	{
		PVRDRIDestroyImage(image);
		return NULL;
	}

	PVRDRIEGLImageSetCallbackData(image->psEGLImage, image);

	return image;
}

GLboolean PVRDRIQueryImage(__DRIimage *image, int attrib, int *value_ptr)
{
	struct PVRDRIImageShared *shared = image->psShared;
	PVRDRIBufferAttribs sAttribs;
	int value;
	uint64_t ulValue;

	PVRDRIEGLImageGetAttribs(image->psEGLImage, &sAttribs);

	if (attrib == __DRI_IMAGE_ATTRIB_HANDLE ||
	    attrib == __DRI_IMAGE_ATTRIB_NAME ||
	    attrib == __DRI_IMAGE_ATTRIB_FD ||
	    attrib == __DRI_IMAGE_ATTRIB_OFFSET)
	{
		if (!shared->psFormat)
		{
			return GL_FALSE;
		}

		switch (shared->psFormat->iDRIComponents)
		{
			case __DRI_IMAGE_COMPONENTS_R:
			case __DRI_IMAGE_COMPONENTS_RG:
			case __DRI_IMAGE_COMPONENTS_RGB:
			case __DRI_IMAGE_COMPONENTS_RGBA:
#if defined(__DRI_IMAGE_COMPONENTS_EXTERNAL)
			case __DRI_IMAGE_COMPONENTS_EXTERNAL:
#endif
				break;
			default:
				return GL_FALSE;
		}
	}

	switch (attrib)
	{
		case __DRI_IMAGE_ATTRIB_STRIDE:
			*value_ptr = sAttribs.uiStrideInBytes;
			break;
		case __DRI_IMAGE_ATTRIB_HANDLE:
			value = PVRDRIBufferGetHandle(shared->psBuffer);
			if (value == -1)
			{
				return GL_FALSE;
			}

			*value_ptr = value;
			break;
		case __DRI_IMAGE_ATTRIB_NAME:
			value = PVRDRIBufferGetName(shared->psBuffer);
			if (value == -1)
			{
				return GL_FALSE;
			}

			*value_ptr = value;
			break;
		case __DRI_IMAGE_ATTRIB_FORMAT:
			if (!shared->psFormat)
			{
				return GL_FALSE;
			}

			*value_ptr = shared->psFormat->iDRIFormat;
			break;
		case __DRI_IMAGE_ATTRIB_WIDTH:
			*value_ptr = sAttribs.uiWidth;
			break;
		case __DRI_IMAGE_ATTRIB_HEIGHT:
			*value_ptr = sAttribs.uiHeight;
			break;
		case __DRI_IMAGE_ATTRIB_COMPONENTS:
			if (!shared->psFormat || !shared->psFormat->iDRIComponents)
			{
				return GL_FALSE;
			}

			*value_ptr = shared->psFormat->iDRIComponents;
			break;
		case __DRI_IMAGE_ATTRIB_FD:
			value = PVRDRIBufferGetFd(shared->psBuffer);
			if (value == -1)
			{
				return GL_FALSE;
			}

			*value_ptr = value;
			break;
		case __DRI_IMAGE_ATTRIB_FOURCC:
			*value_ptr = shared->psFormat->iDRIFourCC;
			break;
		case __DRI_IMAGE_ATTRIB_NUM_PLANES:
			*value_ptr = (int)shared->psFormat->uiNumPlanes;
			break;
		case __DRI_IMAGE_ATTRIB_OFFSET:
			*value_ptr = PVRDRIBufferGetOffset(shared->psBuffer);
			break;
		case __DRI_IMAGE_ATTRIB_MODIFIER_LOWER:
			ulValue = PVRDRIBufferGetModifier(shared->psBuffer);
			*value_ptr = (int)(ulValue & 0xffffffff);
			break;
		case __DRI_IMAGE_ATTRIB_MODIFIER_UPPER:
			ulValue = PVRDRIBufferGetModifier(shared->psBuffer);
			*value_ptr = (int)((ulValue >> 32) & 0xffffffff);
			break;
		default:
			return GL_FALSE;
	}

	return GL_TRUE;
}

__DRIimage *PVRDRIDupImage(__DRIimage *srcImage, void *loaderPrivate)
{
	__DRIimage *image;

	image = CommonImageSetup(loaderPrivate);
	if (!image)
	{
		return NULL;
	}

	image->psShared = RefImageShared(srcImage->psShared);

	image->psEGLImage = PVRDRIEGLImageDup(srcImage->psEGLImage);
	if (!image->psEGLImage)
	{
		PVRDRIDestroyImage(image);
		return NULL;
	}

	PVRDRIEGLImageSetCallbackData(image->psEGLImage, image);

	return image;
}

GLboolean PVRDRIValidateUsage(__DRIimage *image, unsigned int use)
{
	struct PVRDRIImageShared *shared = image->psShared;
	__DRIscreen *screen = shared->psPVRScreen->psDRIScreen;

	if (use & (__DRI_IMAGE_USE_SCANOUT | __DRI_IMAGE_USE_CURSOR))
	{
		uint64_t modifier;

		/*
		 * We are extra strict in this case as an application may ask for a
		 * handle so that the memory can be wrapped as a framebuffer/used as
		 * a cursor and this can only be done on a card node.
		 */
		if (drmGetNodeTypeFromFd(screen->fd) != DRM_NODE_PRIMARY)
		{
			return GL_FALSE;
		}

		modifier = PVRDRIBufferGetModifier(shared->psBuffer);

		if (modifier != DRM_FORMAT_MOD_INVALID &&
			modifier != DRM_FORMAT_MOD_LINEAR)
		{
			return GL_FALSE;
		}
	}
	else if (use & (__DRI_IMAGE_USE_SHARE))
	{
		/*
		 * We are less strict in this case as it's possible to share buffers
		 * using prime (but not flink) on a render node so we only need to know
		 * whether or not the fd belongs to the display.
		 */
		if (PVRDRIGetDeviceTypeFromFd(screen->fd) != PVRDRI_DEVICE_TYPE_DISPLAY)
		{
			return GL_FALSE;
		}
	}

	return GL_TRUE;
}

__DRIimage *PVRDRICreateImageFromNames(__DRIscreen *screen,
				       int width, int height, int fourcc,
				       int *names, int num_names,
				       int *strides, int *offsets,
				       void *loaderPrivate)
{
	__DRIimage *image;
	int iStride;

	image = CommonImageSetup(loaderPrivate);
	if (!image)
	{
		return NULL;
	}

	image->psShared = CreateImageSharedFromNames(screen, width, height, fourcc,
						     names, num_names, strides, offsets);
	if (!image->psShared)
	{
		PVRDRIDestroyImage(image);
		return NULL;
	}

	if (image->psShared->psFormat->uiNumPlanes == 1)
	{
		iStride = strides[0];
	}
	else
	{
		iStride = width * PVRDRIPixFmtGetBlockSize(image->psShared->psFormat->eIMGPixelFormat);
	}

	image->psEGLImage = PVRDRIEGLImageCreateFromBuffer(width, height,
							    iStride,
							    image->psShared->psFormat->eIMGPixelFormat,
							    image->psShared->eColourSpace,
							    image->psShared->eChromaUInterp,
							    image->psShared->eChromaVInterp,
							    image->psShared->psBuffer);
	if (!image->psEGLImage)
	{
		PVRDRIDestroyImage(image);
		return NULL;
	}

	PVRDRIEGLImageSetCallbackData(image->psEGLImage, image);

	return image;
}

__DRIimage *PVRDRIFromPlanar(__DRIimage *srcImage, int plane,
			     void *loaderPrivate)
{
	__DRIimage *image;

	image = CommonImageSetup(loaderPrivate);
	if (!image)
	{
		return NULL;
	}

	image->psShared = CreateImageSharedForSubImage(srcImage->psShared,
						       plane);

	if (!image->psShared)
	{
		if (plane != 0)
		{
			errorMessage("%s: plane %d not supported\n",
				     __func__, plane);
		}

		image->psShared = RefImageShared(srcImage->psShared);

		image->psEGLImage = PVRDRIEGLImageDup(srcImage->psEGLImage);
	}
	else
	{
		image->psEGLImage = PVRDRIEGLImageCreateFromSubBuffer(
					image->psShared->psFormat->eIMGPixelFormat,
					image->psShared->psBuffer);
	}

	if (!image->psEGLImage)
	{
		PVRDRIDestroyImage(image);
		return NULL;
	}

	PVRDRIEGLImageSetCallbackData(image->psEGLImage, image);

	return image;
}

__DRIimage *
PVRDRICreateImageFromTexture(__DRIcontext *context,
                             int glTarget,
                             unsigned texture,
                             int depth,
                             int level,
                             unsigned *error,
                             void *loaderPrivate)
{
	PVRDRIContext *psPVRContext = context->driverPrivate;
	__DRIscreen *screen = psPVRContext->psPVRScreen->psDRIScreen;
	IMGEGLImage *psEGLImage;
	__DRIimage *image;
	uint32_t eglTarget;
	unsigned e;

	switch (glTarget)
	{
		case GL_TEXTURE_2D:
			eglTarget = EGL_GL_TEXTURE_2D_KHR;
			break;
		case GL_TEXTURE_CUBE_MAP:
			eglTarget = EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR + depth;
			break;
		default:
			errorMessage("%s: GL Target %d is not supported\n", __func__, glTarget);
			*error = __DRI_IMAGE_ERROR_BAD_PARAMETER;
			return NULL;
	}

	image = CommonImageSetup(loaderPrivate);
	if (!image)
	{
		return NULL;
	}

	psEGLImage = PVRDRIEGLImageCreate();
	if (!psEGLImage)
	{
		PVRDRIDestroyImage(image);
		return NULL;
	}

	e = PVRDRIGetImageSource(psPVRContext->eAPI,
	                         psPVRContext->psPVRScreen->psImpl,
	                         psPVRContext->psImpl,
	                         eglTarget,
	                         (uintptr_t)texture,
	                         (uint32_t)level,
	                         psEGLImage);
	*error = e;

	if (e != PVRDRI_IMAGE_ERROR_SUCCESS)
	{
		PVRDRIEGLImageFree(psEGLImage);
		PVRDRIDestroyImage(image);
		return NULL;
	}

	PVRDRIEGLImageSetCallbackData(psEGLImage, image);

	/*
	 * We can't destroy the image after this point, as the
	 * texture now has a reference to it.
	 */
	image->psShared = CreateImageSharedFromEGLImage(screen,
							psEGLImage,
							PVRDRI_EGLIMAGE_IMGEGL);
	if (!image->psShared)
	{
		return NULL;
	}

	image->psEGLImage = PVRDRIEGLImageDup(image->psShared->psEGLImage);
	if (!image->psEGLImage)
	{
		return NULL;
	}

	image->iRefCount++;

	return image;
}

__DRIimage *PVRDRICreateImageFromFds(__DRIscreen *screen,
				     int width, int height, int fourcc,
				     int *fds, int num_fds,
				     int *strides, int *offsets,
				     void *loaderPrivate)
{
	unsigned error;

	return PVRDRICreateImageFromDmaBufs(screen, width, height, fourcc,
					    fds, num_fds, strides, offsets,
					    __DRI_YUV_COLOR_SPACE_UNDEFINED,
					    __DRI_YUV_RANGE_UNDEFINED,
					    __DRI_YUV_CHROMA_SITING_UNDEFINED,
					    __DRI_YUV_CHROMA_SITING_UNDEFINED,
					    &error,
					    loaderPrivate);
}

__DRIimage *
PVRDRICreateImageFromBuffer(__DRIcontext *context,
                            int target,
                            void *buffer,
                            unsigned *error,
                            void *loaderPrivate)
{
	PVRDRIContext *psPVRContext = context->driverPrivate;
	__DRIscreen *screen = psPVRContext->psPVRScreen->psDRIScreen;
	IMGEGLImage *psEGLImage;
	__DRIimage *image;

	switch (target)
	{
#if defined(EGL_CL_IMAGE_IMG)
		case EGL_CL_IMAGE_IMG:
			break;
#endif
		default:
			errorMessage("%s: Target %d is not supported\n", __func__, target);
			*error = __DRI_IMAGE_ERROR_BAD_PARAMETER;
			return NULL;
	}

	image = CommonImageSetup(loaderPrivate);
	if (!image)
	{
		return NULL;
	}

	psEGLImage = PVRDRIEGLImageCreate();
	if (!psEGLImage)
	{
		PVRDRIDestroyImage(image);
		return NULL;
	}

	*error = PVRDRIGetImageSource(PVRDRI_API_CL,
				      psPVRContext->psPVRScreen->psImpl,
				      psPVRContext->psImpl,
				      target,
				      (uintptr_t)buffer,
				      0,
				      psEGLImage);
	if (*error != __DRI_IMAGE_ERROR_SUCCESS)
	{
		PVRDRIEGLImageFree(psEGLImage);
		PVRDRIDestroyImage(image);
		return NULL;
	}

	PVRDRIEGLImageSetCallbackData(psEGLImage, image);

	/*
	 * We can't destroy the image after this point, as the
	 * OCL image now has a reference to it.
	 */
	image->psShared = CreateImageSharedFromEGLImage(screen,
							psEGLImage,
							PVRDRI_EGLIMAGE_IMGOCL);
	if (!image->psShared)
	{
		return NULL;
	}

	image->psEGLImage = PVRDRIEGLImageDup(image->psShared->psEGLImage);
	if (!image->psEGLImage)
	{
		return NULL;
	}

	image->iRefCount++;

	return image;
}

__DRIimage *PVRDRICreateImageFromDmaBufs2(__DRIscreen *screen,
					  int width, int height, int fourcc,
					  uint64_t modifier,
					  int *fds, int num_fds,
					  int *strides, int *offsets,
					  enum __DRIYUVColorSpace color_space,
					  enum __DRISampleRange sample_range,
					  enum __DRIChromaSiting horiz_siting,
					  enum __DRIChromaSiting vert_siting,
					  unsigned *error,
					  void *loaderPrivate)
{
	__DRIimage *image;

	image = CommonImageSetup(loaderPrivate);
	if (!image)
	{
		*error = __DRI_IMAGE_ERROR_BAD_ALLOC;
		return NULL;
	}

	image->psShared = CreateImageSharedFromDmaBufs(screen, width, height, fourcc,
						       modifier,
						       fds, num_fds, strides, offsets,
						       color_space, sample_range,
						       horiz_siting, vert_siting,
						       error);
	if (!image->psShared)
	{
		PVRDRIDestroyImage(image);
		return NULL;
	}

	image->psEGLImage = PVRDRIEGLImageCreateFromBuffer(width, height,
							    strides[0],
							    image->psShared->psFormat->eIMGPixelFormat,
							    image->psShared->eColourSpace,
							    image->psShared->eChromaUInterp,
							    image->psShared->eChromaVInterp,
							    image->psShared->psBuffer);
	if (!image->psEGLImage)
	{
		PVRDRIDestroyImage(image);
		*error = __DRI_IMAGE_ERROR_BAD_ALLOC;
		return NULL;
	}

	PVRDRIEGLImageSetCallbackData(image->psEGLImage, image);

	*error = __DRI_IMAGE_ERROR_SUCCESS;

	return image;
}

__DRIimage *PVRDRICreateImageFromDmaBufs(__DRIscreen *screen,
                                         int width, int height, int fourcc,
                                         int *fds, int num_fds,
                                         int *strides, int *offsets,
                                         enum __DRIYUVColorSpace color_space,
                                         enum __DRISampleRange sample_range,
                                         enum __DRIChromaSiting horiz_siting,
                                         enum __DRIChromaSiting vert_siting,
                                         unsigned *error,
                                         void *loaderPrivate)
{
	return PVRDRICreateImageFromDmaBufs2(screen,
					     width, height, fourcc,
					     DRM_FORMAT_MOD_INVALID,
					     fds, num_fds,
					     strides, offsets,
					     color_space,
					     sample_range,
					     horiz_siting,
					     vert_siting,
					     error,
					     loaderPrivate);
}

void PVRDRIRefImage(__DRIimage *image)
{
	int iRefCount = p_atomic_inc_return(&image->iRefCount);

	(void)iRefCount;
	assert(iRefCount > 1);
}

void PVRDRIUnrefImage(__DRIimage *image)
{
	PVRDRIDestroyImage(image);
}

PVRDRIImageType PVRDRIImageGetSharedType(__DRIimage *image)
{
	return image->psShared->eType;
}

PVRDRIBufferImpl *PVRDRIImageGetSharedBuffer(__DRIimage *pImage)
{
	assert(pImage->psShared->eType != PVRDRI_IMAGE_FROM_EGLIMAGE);

	return pImage->psShared->psBuffer;
}

IMGEGLImage *PVRDRIImageGetSharedEGLImage(__DRIimage *pImage)
{
	assert(pImage->psShared->eType == PVRDRI_IMAGE_FROM_EGLIMAGE);
	return pImage->psShared->psEGLImage;
}

IMGEGLImage *PVRDRIImageGetEGLImage(__DRIimage *pImage)
{
	return pImage->psEGLImage;
}

__DRIimage *PVRDRIScreenGetDRIImage(void *hEGLImage)
{
	PVRDRIScreen *psPVRScreen = PVRDRIThreadGetCurrentScreen();

	if (!psPVRScreen)
	{
		return NULL;
	}

	return psPVRScreen->psDRIScreen->dri2.image->lookupEGLImage(
	      psPVRScreen->psDRIScreen,
	      hEGLImage,
	      psPVRScreen->psDRIScreen->loaderPrivate);
}

void PVRDRIBlitImage(__DRIcontext *context,
		     __DRIimage *dst, __DRIimage *src,
		     int dstx0, int dsty0, int dstwidth, int dstheight,
		     int srcx0, int srcy0, int srcwidth, int srcheight,
		     int flush_flag)
{
	PVRDRIContext *psPVRContext = context->driverPrivate;
	bool res;

	res = PVRDRIBlitEGLImage(psPVRContext->psPVRScreen->psImpl,
			psPVRContext->psImpl,
			dst->psEGLImage, dst->psShared->psBuffer,
			src->psEGLImage, src->psShared->psBuffer,
			dstx0, dsty0, dstwidth, dstheight,
			srcx0, srcy0, srcwidth, srcheight,
			flush_flag);
	

	if (!res)
	{
		__driUtilMessage("%s: PVRDRIBlitEGLImage failed\n", __func__);
	}
}

int PVRDRIGetCapabilities(__DRIscreen *screen)
{
	(void) screen;

	return __DRI_IMAGE_CAP_GLOBAL_NAMES;
}

void *PVRDRIMapImage(__DRIcontext *context, __DRIimage *image,
		    int x0, int y0, int width, int height,
		    unsigned int flags, int *stride, void **data)
{
	PVRDRIContext *psPVRContext = context->driverPrivate;

	return PVRDRIMapEGLImage(psPVRContext->psPVRScreen->psImpl,
				 psPVRContext->psImpl,
				 image->psEGLImage, image->psShared->psBuffer,
				 x0, y0, width, height, flags, stride, data);
}

void PVRDRIUnmapImage(__DRIcontext *context, __DRIimage *image, void *data)
{
	PVRDRIContext *psPVRContext = context->driverPrivate;
	bool res;

	res = PVRDRIUnmapEGLImage(psPVRContext->psPVRScreen->psImpl,
			      psPVRContext->psImpl,
			      image->psEGLImage, image->psShared->psBuffer,
			      data);
	if (!res)
	{
		__driUtilMessage("%s: PVRDRIUnmapEGLImage failed\n", __func__);
	}
}
