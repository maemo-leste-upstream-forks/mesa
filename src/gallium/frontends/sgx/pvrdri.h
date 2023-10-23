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

#if !defined(__PVRDRI_H__)
#define __PVRDRI_H__

#include <stdbool.h>

#include <glapi/glapi.h>

#include "main/mtypes.h"
#include "GL/internal/dri_interface.h"
#include "drm-uapi/drm_fourcc.h"
#include "util/log.h"

#include "sgx_support.h"
#include "pvrqueue.h"
#include "pvr_object_cache.h"

#if !defined(ARRAY_SIZE)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

/* This should match PVRDRIIMPL_MAX_PLANES */
#define DRI_PLANES_MAX 3

#define	DRI2_BUFFERS_MAX (3)

/* PVR screen data */
typedef struct PVRDRIScreen_TAG {
   /* DRI screen structure pointer */
   __DRIscreen *psDRIScreen;

   /* Mutex for this screen */
   pthread_mutex_t     sMutex;

   /* X Server sends invalidate events */
   bool                bUseInvalidate;

   /* Reference count */
   int iRefCount;

   /* PVR OGLES 1 dispatch table */
   struct _glapi_table *psOGLES1Dispatch;
   /* PVR OGLES 2/3 dispatch table */
   struct _glapi_table *psOGLES2Dispatch;
   /* PVR OGL dispatch table */
   struct _glapi_table *psOGLDispatch;

   PVRDRIScreenImpl    *psImpl;
} PVRDRIScreen;

/* PVR context data */
typedef struct PVRDRIContext_TAG {
   PVRQElem sQElem;

   /* Pointer to DRI context */
   __DRIcontext *psDRIContext;

   /* Pointer to PVRDRIScreen structure */
   PVRDRIScreen *psPVRScreen;

   /* Pointer to currently bound drawable */
   struct PVRDRIDrawable_TAG *psPVRDrawable;

   /* API */
   PVRDRIAPIType eAPI;

   PVRDRIContextImpl *psImpl;
} PVRDRIContext;

/* PVR drawable data */
typedef struct PVRDRIDrawable_TAG {
	/** Ptr to PVR screen, that spawned this drawable */
	PVRDRIScreen       *psPVRScreen;

	/** DRI drawable data */
	__DRIdrawable      *psDRIDrawable;

	/** Are surface/buffers created? */
	bool                bInitialised;

	/** Are we using double buffering? */
	bool                bDoubleBuffered;

	/** Buffer stride */
	unsigned            uStride;

	/* Number of bytes per pixel */
	unsigned int        uBytesPerPixel;

	/* List of contexts bound to this drawable */
	PVRQHead            sPVRContextHead;

	/* Mutex for this drawable */
	pthread_mutex_t     sMutex;

	/* IMG Pixel format for this drawable */
	IMG_PIXFMT          ePixelFormat;

	/* Indicates the drawable info is invalid */
	bool                bDrawableInfoInvalid;

	/* Indicates updated drawable info is available */
	bool                bDrawableInfoUpdated;

	/* Buffer cache handle */
	PVRObjectCache      hBufferCache;

	/* Queue of buffers evicted from cache, waiting for flush */
	PVRQHead            sCacheFlushHead;

	union {
		struct {
			__DRIbuffer	sDRI;
			int		w;
			int		h;
		} sBuffer;
		struct {
			__DRIimage	*psDRI;
		} sImage;
	} uDRI;
	PVRDRIDrawableImpl *psImpl;
} PVRDRIDrawable;

typedef enum PVRDRIBufferBackingType_TAG
{
	PVRDRI_BUFFER_BACKING_INVALID = 0,
	PVRDRI_BUFFER_BACKING_DRI2,
	PVRDRI_BUFFER_BACKING_IMAGE,
} PVRDRIBufferBackingType;

typedef struct PVRDRIBuffer_TAG
{
	PVRDRIBufferBackingType eBackingType;

	union
	{
		struct
		{
			uint32_t uiName;
			PVRDRIBufferImpl *psBuffer;

		} sDRI2;

		struct
		{
			__DRIimage *psImage;
		} sImage;
	} uBacking;

	PVRQElem sCacheFlushElem;
} PVRDRIBuffer;

typedef struct PVRDRIImageFormat_TAG
{
	IMG_PIXFMT eIMGPixelFormat;
	int iDRIFourCC;
	int iDRIFormat;
	int iDRIComponents;
	unsigned uiNumPlanes;
	struct
	{
		IMG_PIXFMT eIMGPixelFormat;
		int iDRIFormat;
		unsigned int uiWidthShift;
		unsigned int uiHeightShift;
	} sPlanes[DRI_PLANES_MAX];
} PVRDRIImageFormat;


/*************************************************************************//*!
 pvrdri.c
 *//**************************************************************************/

void PVRDRIScreenLock(PVRDRIScreen *psPVRScreen);
void PVRDRIScreenUnlock(PVRDRIScreen *psPVRScreen);

PVRDRIScreen *PVRDRIThreadGetCurrentScreen(void);
void PVRDRIThreadSetCurrentScreen(PVRDRIScreen *psPVRScreen);

bool PVRDRIFlushBuffersForSwap(PVRDRIContext *psPVRContext,
                               PVRDRIDrawable *psPVRDrawable);

/*************************************************************************//*!
 pvrutil.c
 *//**************************************************************************/
void __attribute__((format(printf, 1, 2))) __driUtilMessage(const char *f, ...);

const __DRIconfig **PVRDRICreateConfigs(void);

const PVRDRIImageFormat *PVRDRIFormatToImageFormat(int iDRIFormat);
const PVRDRIImageFormat *PVRDRIFourCCToImageFormat(int iDRIFourCC);
const PVRDRIImageFormat *PVRDRIIMGPixelFormatToImageFormat(IMG_PIXFMT eIMGPixelFormat);

IMG_YUV_COLORSPACE PVRDRIToIMGColourSpace(const PVRDRIImageFormat *psFormat,
					  enum __DRIYUVColorSpace eDRIColourSpace,
					  enum __DRISampleRange eDRISampleRange);
IMG_YUV_CHROMA_INTERP PVRDRIChromaSittingToIMGInterp(const PVRDRIImageFormat *psFormat,
						     enum __DRIChromaSiting eChromaSitting);

/*************************************************************************//*!
 pvrext.c
 *//**************************************************************************/

const __DRIextension **SGXDRIScreenExtensions(void);
const __DRIextension *SGXDRIScreenExtensionVersionInfo(void);

/*************************************************************************//*!
 pvrdrawable.c
 *//**************************************************************************/

void PVRDRIDrawableLock(PVRDRIDrawable *psPVRDrawable);
void PVRDRIDrawableUnlock(PVRDRIDrawable *psPVRDrawable);

bool PVRDRIDrawableInit(PVRDRIDrawable *psPVRDrawable);
void PVRDRIDrawableDeinit(PVRDRIDrawable *psPVRDrawable);

/* Callbacks into non-impl layer */
bool PVRDRIDrawableUpdateNativeInfo(PVRDRIDrawable *psPVRDrawable);
bool PVRDRIDrawableRecreate(PVRDRIDrawable *psPVRDrawable);
bool PVRDRIDrawableGetParameters(PVRDRIDrawable *psPVRDrawable,
                                 PVRDRIBufferImpl **ppsDstBuffer,
                                 PVRDRIBufferImpl **ppsAccumBuffer,
                                 PVRDRIBufferAttribs *psAttribs,
                                 bool *pbDoubleBuffered);
PVRDRIImageType PVRDRIImageGetSharedType(__DRIimage *image);
PVRDRIBufferImpl *PVRDRIImageGetSharedBuffer(__DRIimage *image);
__EGLImage *PVRDRIImageGetSharedEGLImage(__DRIimage *image);
__EGLImage *PVRDRIImageGetEGLImage(__DRIimage *image);

__DRIimage *PVRDRIScreenGetDRIImage(void *hEGLImage);
void PVRDRIRefImage(__DRIimage *image);
void PVRDRIUnrefImage(__DRIimage *image);

/*************************************************************************//*!
 pvrcompat.c
 *//**************************************************************************/

bool MODSUPRegisterSupportInterfaceV0(const void *pvInterface,
                                      unsigned int uVersion,
                                      unsigned int uMinVersion);

#endif /* defined(__PVRDRI_H__) */
