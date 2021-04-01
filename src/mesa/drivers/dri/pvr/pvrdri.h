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

#if !defined(__PVRDRI2_H__)
#define __PVRDRI2_H__
#include <stdbool.h>

#include <glapi/glapi.h>

#include "main/mtypes.h"
#include "GL/internal/dri_interface.h"

#include "util/macros.h"

#include "dri_support.h"

/* This should match EGL_MAX_PLANES */
#define DRI_PLANES_MAX 3

#define	DRI2_BUFFERS_MAX (3)

#define	DRIScreenPrivate(pScreen)	((pScreen)->driverPrivate)

struct PVRDRIConfigRec
{
	struct gl_config sGLMode;
	int iSupportedAPIs;
};

struct PVRDRIModifiers
{
	/* Number of modifiers for a given format */
	int         iNumModifiers;
	/* Array of modifiers */
	uint64_t    *puModifiers;
	/*
	 * Array of booleans that indicates which modifiers in the above array
	 * can only be used for EGL Image External (and so not for scanout).
	 */
	unsigned    *puExternalOnly;
};

/** Our PVR related screen data */
typedef struct PVRDRIScreen_TAG
{
	/* DRI screen structure pointer */
	__DRIscreen        *psDRIScreen;
	/* X Server sends invalidate events */
	bool                bUseInvalidate;
	/* Reference count */
	int iRefCount;

#if defined(DEBUG)
	/* Counters of outstanding allocations */
	int iContextAlloc, iDrawableAlloc, iBufferAlloc;
#endif

	/* PVR OGLES 1 dispatch table */
	struct _glapi_table *psOGLES1Dispatch;
	/* PVR OGLES 2/3 dispatch table */
	struct _glapi_table *psOGLES2Dispatch;

	PVRDRIScreenImpl    *psImpl;

	/*
	 * Number of supported formats:
	 * -1 -> couldn't be queried (pvr_dri_support too old)
	 *  0 -> uninitialised or initialisation failed
	 */
	int                 iNumFormats;
	/* Indicates which entries in the image format array are supported */
	bool                *pbHasFormat;
	/* Array of modifiers for each image format array entry */
	struct PVRDRIModifiers *psModifiers;
} PVRDRIScreen;

/** Our PVR related context data */
typedef struct PVRDRIContext_TAG
{
	/* Pointer to DRI context */
	__DRIcontext              *psDRIContext;
	/* Pointer to PVRDRIScreen structure */
	PVRDRIScreen              *psPVRScreen;

	PVRDRIConfig               sConfig;

	/* Pointer to currently bound drawable */
	struct PVRDRIDrawable_TAG *psPVRDrawable;

	/* API */
	PVRDRIAPIType             eAPI;

	PVRDRIContextImpl         *psImpl;
} PVRDRIContext;

/** Our PVR related drawable data */
typedef struct PVRDRIDrawable_TAG
{
	/** Ptr to PVR screen, that spawned this drawable */
	PVRDRIScreen       *psPVRScreen;

	/** DRI drawable data */
	__DRIdrawable      *psDRIDrawable;

	PVRDRIDrawableType  eType;

	PVRDRIConfig        sConfig;

	/** Are surface/buffers created? */
	bool                bInitialised;

	/** Buffer stride */
	unsigned            uStride;

	/* Number of bytes per pixel */
	unsigned int        uBytesPerPixel;

	/* Context bound to this drawable */
	PVRDRIContext       *psPVRContext;

	/* IMG Pixel format for this drawable */
	IMG_PIXFMT          ePixelFormat;

	/* Indicates the drawable info is invalid */
	int                 iInfoInvalid;

	/* Indicates the drawable is currently being updated */
	bool                bDrawableUpdating;

	/* Indicates a flush is in progress */
	bool                bFlushInProgress;

	__DRIimage *psDRI;
	__DRIimage *psImage;

	__DRIimage *psDRIAccum;
	__DRIimage *psImageAccum;

	PVRDRIDrawableImpl *psImpl;
} PVRDRIDrawable;

typedef struct PVRDRIImageFormat_TAG
{
	/*
	 * IMG pixel format for the entire/overall image, e.g.
	 * IMG_PIXFMT_B8G8R8A8_UNORM or IMG_PIXFMT_YUV420_2PLANE.
	 */
	IMG_PIXFMT eIMGPixelFormat;

	/*
	 * DRI fourcc for the entire/overall image (defined by dri_interface.h),
	 * e.g. __DRI_IMAGE_FOURCC_ARGB8888 or __DRI_IMAGE_FOURCC_NV12.
	 */
	int iDRIFourCC;

	/*
	 * DRI format for the entire/overall image (defined by dri_interface.h),
	 * e.g. __DRI_IMAGE_FORMAT_ARGB8888. This isn't applicable for YUV
	 * formats and should be set to __DRI_IMAGE_FORMAT_NONE.
	 */
	int iDRIFormat;

	/*
	 * DRI components for the entire/overall image (defined by
	 * dri_interface.h), e.g. __DRI_IMAGE_COMPONENTS_RGBA or
	 * __DRI_IMAGE_COMPONENTS_Y_UV.
	 *
	 * This specifies the image components and their groupings, in terms of
	 * sub-images/planes, but not the order in which they appear.
	 *
	 * For example:
	 * - any combination of BGRA channels would correspond to
	 *   __DRI_IMAGE_COMPONENTS_RGBA
	 * - any combination of BGR or BGRX would correspond to
	 *   __DRI_IMAGE_COMPONENTS_RGB
	 * - any combination of YUV with 2 planes would correspond to
	 *   __DRI_IMAGE_COMPONENTS_Y_UV
	 */
	int iDRIComponents;

	/* The number of sub-images/planes that make up the overall image */
	unsigned uiNumPlanes;

	/*
	 * Don't return the format when the queryDmaBufFormats DRI Image
	 * extension function is called. Some DRM formats map to multiple
	 * IMG formats. The query should return just one of them.
	 */
	bool bQueryDmaBufFormatsExclude;

	/* Per-plane information */
	struct
	{
		/* IMG pixel format for the plane */
		IMG_PIXFMT eIMGPixelFormat;

		/*
		 * This is the amount that the image width should be bit-shifted
		 * in order to give the plane width. This value can be determined
		 * from the YUV sub-sampling ratios and should either be 0 (full
		 * width), 1 (half width) or 2 (quarter width).
		 */
		unsigned int uiWidthShift;

		/*
		 * This is the amount that the image height should be bit-shifted
		 * in order to give the plane height. This value can be determined
		 * from the YUV sub-sampling ratios and should either be 0 (full
		 * height) or 1 (half height).
		 */
		unsigned int uiHeightShift;
	} sPlanes[DRI_PLANES_MAX];
} PVRDRIImageFormat;


/*************************************************************************/ /*!
 pvrdri.c
*/ /**************************************************************************/

IMG_PIXFMT PVRDRIGetPixelFormat(const struct gl_config *psGLMode);
PVRDRIScreen *PVRDRIThreadGetCurrentScreen(void);
void PVRDRIThreadSetCurrentScreen(PVRDRIScreen *psPVRScreen);

void PVRDRIFlushBuffersForSwap(PVRDRIContext *psPVRContext,
                               PVRDRIDrawable *psPVRDrawable);


/*************************************************************************/ /*!
 pvrutil.c
*/ /**************************************************************************/

void PRINTFLIKE(1, 2) __driUtilMessage(const char *f, ...);
void PRINTFLIKE(1, 2) errorMessage(const char *f, ...);

const __DRIconfig **PVRDRICreateConfigs(void);

const PVRDRIImageFormat *PVRDRIFormatToImageFormat(PVRDRIScreen *psPVRScreen,
						   int iDRIFormat);
const PVRDRIImageFormat *PVRDRIFourCCToImageFormat(PVRDRIScreen *psPVRScreen,
						   int iDRIFourCC);
const PVRDRIImageFormat *PVRDRIIMGPixelFormatToImageFormat(PVRDRIScreen *psPVRScreen,
							   IMG_PIXFMT eIMGPixelFormat);

IMG_YUV_COLORSPACE PVRDRIToIMGColourSpace(const PVRDRIImageFormat *psFormat,
					  enum __DRIYUVColorSpace eDRIColourSpace,
					  enum __DRISampleRange eDRISampleRange);
IMG_YUV_CHROMA_INTERP PVRDRIChromaSittingToIMGInterp(const PVRDRIImageFormat *psFormat,
						     enum __DRIChromaSiting eChromaSitting);

bool PVRDRIGetSupportedFormats(PVRDRIScreen *psPVRScreen);

GLboolean PVRDRIQueryDmaBufFormats(__DRIscreen *screen, int max,
				   int *formats, int *count);

GLboolean PVRDRIQueryDmaBufModifiers(__DRIscreen *screen, int fourcc,
				     int max, uint64_t *modifiers,
				     unsigned int *external_only,
				     int *count);

GLboolean PVRDRIQueryDmaBufFormatModifierAttribs(__DRIscreen *screen,
						 uint32_t fourcc,
						 uint64_t modifier,
						 int attrib,
						 uint64_t *value);

void PVRDRIDestroyFormatInfo(PVRDRIScreen *psPVRScreen);

/*************************************************************************/ /*!
 pvrdrawable.c
*/ /**************************************************************************/

bool PVRDRIDrawableInit(PVRDRIDrawable *psPVRDrawable);
void PVRDRIDrawableDeinit(PVRDRIDrawable *psPVRDrawable);

/* Callbacks into non-impl layer */

/* Version 0 callbacks (deprecated) */
bool PVRDRIDrawableRecreateV0(PVRDRIDrawable *psPVRDrawable);
bool PVRDRIDrawableGetParametersV0(PVRDRIDrawable *psPVRDrawable,
				   PVRDRIBufferImpl **ppsDstBuffer,
				   PVRDRIBufferImpl **ppsAccumBuffer,
				   PVRDRIBufferAttribs *psAttribs,
				   bool *pbDoubleBuffered);

/* Version 1 callbacks (deprecated) */
bool PVRDRIDrawableGetParametersV1(PVRDRIDrawable *psPVRDrawable,
				   bool bAllowRecreate,
				   PVRDRIBufferImpl **ppsDstBuffer,
				   PVRDRIBufferImpl **ppsAccumBuffer,
				   PVRDRIBufferAttribs *psAttribs,
				   bool *pbDoubleBuffered);

/* Version 2 callbacks */
bool PVRDRIDrawableQuery(const PVRDRIDrawable *psPVRDrawable,
			 PVRDRIBufferAttrib eBufferAttrib,
			 uint32_t *uiValueOut);
bool PVRDRIDrawableGetParametersV2(PVRDRIDrawable *psPVRDrawable,
				   uint32_t uiFlags,
				   PVRDRIBufferImpl **ppsDstBuffer,
				   PVRDRIBufferImpl **ppsAccumBuffer);

/*************************************************************************/ /*!
 pvrimage.c
*/ /**************************************************************************/

__DRIimage *PVRDRIScreenGetDRIImage(void *hEGLImage);
void PVRDRIRefImage(__DRIimage *image);
void PVRDRIUnrefImage(__DRIimage *image);

/* Callbacks into non-impl layer */
PVRDRIImageType PVRDRIImageGetSharedType(__DRIimage *image);
PVRDRIBufferImpl *PVRDRIImageGetSharedBuffer(__DRIimage *image);
IMGEGLImage *PVRDRIImageGetSharedEGLImage(__DRIimage *image);
IMGEGLImage *PVRDRIImageGetEGLImage(__DRIimage *image);

/*************************************************************************/ /*!
 pvrext.c
*/ /**************************************************************************/

const __DRIextension **PVRDRIScreenExtensions(void);
const __DRIextension *PVRDRIScreenExtensionVersionInfo(void);

/*************************************************************************/ /*!
 pvrcompat.c
*/ /**************************************************************************/

bool PVRDRICompatInit(const PVRDRICallbacks *psCallbacks, unsigned uVersion);
void PVRDRICompatDeinit(void);

bool PVRDRIRegisterSupportInterfaceV1(const PVRDRISupportInterface *psInterface,
				      unsigned uVersion);

unsigned PVRDRISupportCreateContext(PVRDRIScreenImpl *psScreenImpl,
				    PVRDRIContextImpl *psSharedContextImpl,
				    PVRDRIConfig *psConfig,
				    PVRDRIAPIType eAPI,
				    PVRDRIAPISubType eAPISub,
				    unsigned uMajorVersion,
				    unsigned uMinorVersion,
				    uint32_t uFlags,
				    bool bNotifyReset,
				    unsigned uPriority,
				    PVRDRIContextImpl **ppsContextImpl);

PVRDRIDrawableImpl *PVRDRISupportCreateDrawable(PVRDRIDrawable *psPVRDrawable,
						PVRDRIConfig *psConfig);

bool PVRDRIBlitEGLImage_IsSupported(void);
bool PVRDRIMapEGLImage_IsSupported(void);
bool PVRDRIBufferGetOffset_IsSupported(void);
bool PVRDRIBufferCreateWithModifiers_IsSupported(void);
bool PVRDRIBufferCreateFromFdsWithModifier_IsSupported(void);

void *PVRDRICreateFenceFdImpl(PVRDRIAPIType eAPI,
			      PVRDRIScreenImpl *psScreenImpl,
			      PVRDRIContextImpl *psContextImpl,
			      int fd);

unsigned PVRDRIGetFenceCapabilitiesImpl(PVRDRIScreenImpl *psScreenImpl);

int PVRDRIGetFenceFdImpl(void *psDRIFence);

bool PVRDRIValidateImageModifier(PVRDRIScreen *psPVRScreen, const int iFourcc,
				 const uint64_t uiModifier);
#endif /* defined(__PVRDRI2_H__) */
