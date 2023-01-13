/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Title          PVR DRI interface definition
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        MIT

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/ /**************************************************************************/

#if !defined(__PVRDRIIFCE_H__)
#define __PVRDRIIFCE_H__

#include <stdint.h>
#include <stdbool.h>

#include <imgpixfmts.h>

typedef enum
{
	PVRDRI_DEVICE_TYPE_INVALID = 0,
	PVRDRI_DEVICE_TYPE_UNKNOWN,
	PVRDRI_DEVICE_TYPE_DISPLAY,
	PVRDRI_DEVICE_TYPE_RENDER,
} PVRDRIDeviceType;

/* API type. */
typedef enum
{
	PVRDRI_API_NONE = 0,
	PVRDRI_API_GL = 1,
	PVRDRI_API_GLES1 = 2,
	PVRDRI_API_GLES2 = 3,
	PVRDRI_API_CL = 4,
} PVRDRIAPIType;

/* API sub type. */
typedef enum
{
	PVRDRI_API_SUB_NONE,
	PVRDRI_API_SUB_GL_COMPAT,
	PVRDRI_API_SUB_GL_CORE,
} PVRDRIAPISubType;

typedef enum
{
	PVRDRI_IMAGE = 1,
	PVRDRI_IMAGE_FROM_NAMES,
	PVRDRI_IMAGE_FROM_EGLIMAGE,
	PVRDRI_IMAGE_FROM_DMABUFS,
} PVRDRIImageType;

typedef enum
{
	PVRDRI_EGLIMAGE_NONE = 0,
	PVRDRI_EGLIMAGE_IMGEGL,
	PVRDRI_EGLIMAGE_IMGOCL,
} PVRDRIEGLImageType;

/* The context flags match their __DRI_CTX_FLAG and EGL_CONTEXT counterparts */
#define PVRDRI_CONTEXT_FLAG_DEBUG			0x00000001
#define PVRDRI_CONTEXT_FLAG_FORWARD_COMPATIBLE		0x00000002
#define PVRDRI_CONTEXT_FLAG_ROBUST_BUFFER_ACCESS	0x00000004

/* The context error codes match their __DRI_CTX_ERROR counterparts */
#define PVRDRI_CONTEXT_ERROR_SUCCESS			0
/* Out of memory */
#define PVRDRI_CONTEXT_ERROR_NO_MEMORY			1
/* Unsupported API */
#define PVRDRI_CONTEXT_ERROR_BAD_API			2
/* Unsupported version of API */
#define PVRDRI_CONTEXT_ERROR_BAD_VERSION		3
/* Unsupported context flag or combination of flags */
#define PVRDRI_CONTEXT_ERROR_BAD_FLAG			4
/* Unrecognised context attribute */
#define PVRDRI_CONTEXT_ERROR_UNKNOWN_ATTRIBUTE		5
/* Unrecognised context flag */
#define PVRDRI_CONTEXT_ERROR_UNKNOWN_FLAG		6

/*
 * The context priority defines match their __DRI_CTX counterparts, and
 * the context priority values used by the DDK.
 */
#define PVRDRI_CONTEXT_PRIORITY_LOW	0
#define PVRDRI_CONTEXT_PRIORITY_MEDIUM	1
#define PVRDRI_CONTEXT_PRIORITY_HIGH	2

/* The image error flags match their __DRI_IMAGE_ERROR counterparts */
#define PVRDRI_IMAGE_ERROR_SUCCESS		0
#define PVRDRI_IMAGE_ERROR_BAD_ALLOC		1
#define PVRDRI_IMAGE_ERROR_BAD_MATCH		2
#define PVRDRI_IMAGE_ERROR_BAD_PARAMETER	3
#define PVRDRI_IMAGE_ERROR_BAD_ACCESS		4

/* The buffer flags match their __DRI_IMAGE_USE counterparts */
#define PVDRI_BUFFER_USE_SHARE		0x0001
#define PVDRI_BUFFER_USE_SCANOUT	0x0002
#define PVDRI_BUFFER_USE_CURSOR		0x0004
#define PVDRI_BUFFER_USE_LINEAR		0x0008

/* EGL_RENDERABLE_TYPE mask bits */
#define PVRDRI_API_BIT_GLES		0x0001
#define PVRDRI_API_BIT_GLES2		0x0004
#define PVRDRI_API_BIT_GL		0x0008
#define PVRDRI_API_BIT_GLES3		0x0040

/* Mesa config formats. These need not match their MESA_FORMAT counterparts */
#define	PVRDRI_MESA_FORMAT_NONE			0
#define PVRDRI_MESA_FORMAT_B8G8R8A8_UNORM	1
#define PVRDRI_MESA_FORMAT_B8G8R8X8_UNORM	2
#define PVRDRI_MESA_FORMAT_B5G6R5_UNORM		3
#define PVRDRI_MESA_FORMAT_R8G8B8A8_UNORM	4
#define PVRDRI_MESA_FORMAT_R8G8B8X8_UNORM	5

typedef struct 
{
	IMG_PIXFMT          ePixFormat;
	uint32_t            uiWidth;
	uint32_t            uiHeight;
	uint32_t            uiStrideInBytes;
} PVRDRIBufferAttribs;

typedef struct
{
	int sampleBuffers;
	int samples;

	int redBits;
	int greenBits;
	int blueBits;
	int alphaBits;

	int rgbBits;
	int depthBits;
	int stencilBits;

	bool doubleBufferMode;

	int bindToTextureRgb;
	int bindToTextureRgba;
} PVRDRIConfigInfo;

typedef struct EGLImageRec __EGLImage;
typedef struct __DRIimageRec __DRIimage;

/* PVRDRI interface opaque types */
typedef struct PVRDRIScreenImplRec PVRDRIScreenImpl;
typedef struct PVRDRIContextImplRec PVRDRIContextImpl;
typedef struct PVRDRIDrawableImplRec PVRDRIDrawableImpl;
typedef struct PVRDRIBufferImplRec PVRDRIBufferImpl;

typedef struct PVRDRIDrawable_TAG PVRDRIDrawable;

PVRDRIDeviceType PVRDRIGetDeviceTypeFromFd(int iFd);

bool PVRDRIIsFirstScreen(PVRDRIScreenImpl *psScreenImpl);

uint32_t PVRDRIPixFmtGetDepth(IMG_PIXFMT eFmt);
uint32_t PVRDRIPixFmtGetBPP(IMG_PIXFMT eFmt);
uint32_t PVRDRIPixFmtGetBlockSize(IMG_PIXFMT eFmt);

/* ScreenImpl functions */
PVRDRIScreenImpl *PVRDRICreateScreenImpl(int iFd);
void PVRDRIDestroyScreenImpl(PVRDRIScreenImpl *psScreenImpl);

int PVRDRIAPIVersion(PVRDRIAPIType eAPI,
		     PVRDRIAPISubType eAPISub,
		     PVRDRIScreenImpl *psScreenImpl);

void *PVRDRIEGLGetLibHandle(PVRDRIAPIType eAPI,
			    PVRDRIScreenImpl *psScreenImpl);

typedef void (*PVRDRIGLAPIProc)(void);
PVRDRIGLAPIProc PVRDRIEGLGetProcAddress(PVRDRIAPIType eAPI,
					  PVRDRIScreenImpl *psScreenImpl,
					  const char *psProcName);

bool PVRDRIEGLFlushBuffers(PVRDRIAPIType eAPI,
                           PVRDRIScreenImpl *psScreenImpl,
                           PVRDRIContextImpl *psContextImpl,
                           PVRDRIDrawableImpl *psDrawableImpl,
                           bool bFlushAllSurfaces,
                           bool bSwapBuffers,
                           bool bWaitForHW);
bool PVRDRIEGLFreeResources(PVRDRIScreenImpl *psPVRScreenImpl);
void PVRDRIEGLMarkRendersurfaceInvalid(PVRDRIAPIType eAPI,
                                       PVRDRIScreenImpl *psScreenImpl,
                                       PVRDRIContextImpl *psContextImpl);
void PVRDRIEGLSetFrontBufferCallback(PVRDRIAPIType eAPI,
                                        PVRDRIScreenImpl *psScreenImpl,
                                        PVRDRIDrawableImpl *psDrawableImpl,
                                        void (*pfnCallback)(PVRDRIDrawable *));

unsigned PVRDRICreateContextImpl(PVRDRIContextImpl **ppsContextImpl,
				 PVRDRIAPIType eAPI,
				 PVRDRIAPISubType eAPISub,
				 PVRDRIScreenImpl *psScreenImpl,
				 const PVRDRIConfigInfo *psConfigInfo,
				 unsigned uMajorVersion,
				 unsigned uMinorVersion,
				 uint32_t uFlags,
				 bool bNotifyReset,
				 unsigned uPriority,
				 PVRDRIContextImpl *psSharedContextImpl);

void PVRDRIDestroyContextImpl(PVRDRIContextImpl *psContextImpl,
			      PVRDRIAPIType eAPI,
			      PVRDRIScreenImpl *psScreenImpl);

bool PVRDRIMakeCurrentGC(PVRDRIAPIType eAPI,
			 PVRDRIScreenImpl *psScreenImpl,
			 PVRDRIContextImpl *psContextImpl,
			 PVRDRIDrawableImpl *psWriteImpl,
			 PVRDRIDrawableImpl *psReadImpl);

void PVRDRIMakeUnCurrentGC(PVRDRIAPIType eAPI,
			   PVRDRIScreenImpl *psScreenImpl);

unsigned PVRDRIGetImageSource(PVRDRIAPIType eAPI,
			      PVRDRIScreenImpl *psScreenImpl,
			      PVRDRIContextImpl *psContextImpl,
			      uint32_t  uiTarget,
			      uintptr_t uiBuffer,
			      uint32_t  uiLevel,
			      __EGLImage *psEGLImage);

bool PVRDRI2BindTexImage(PVRDRIAPIType eAPI,
			 PVRDRIScreenImpl *psScreenImpl,
			 PVRDRIContextImpl *psContextImpl,
			 PVRDRIDrawableImpl *psDrawableImpl);

void PVRDRI2ReleaseTexImage(PVRDRIAPIType eAPI,
			    PVRDRIScreenImpl *psScreenImpl,
			    PVRDRIContextImpl *psContextImpl,
			    PVRDRIDrawableImpl *psDrawableImpl);

/* DrawableImpl functions */
PVRDRIDrawableImpl *PVRDRICreateDrawableImpl(PVRDRIDrawable *psPVRDrawable);
void PVRDRIDestroyDrawableImpl(PVRDRIDrawableImpl *psScreenImpl);
bool PVREGLDrawableCreate(PVRDRIScreenImpl *psScreenImpl,
                          PVRDRIDrawableImpl *psDrawableImpl);
bool PVREGLDrawableRecreate(PVRDRIScreenImpl *psScreenImpl,
                            PVRDRIDrawableImpl *psDrawableImpl);
bool PVREGLDrawableDestroy(PVRDRIScreenImpl *psScreenImpl,
                           PVRDRIDrawableImpl *psDrawableImpl);
void PVREGLDrawableDestroyConfig(PVRDRIDrawableImpl *psDrawableImpl);

/* Buffer functions */
PVRDRIBufferImpl *PVRDRIBufferCreate(PVRDRIScreenImpl *psScreenImpl,
				     int iWidth,
				     int iHeight,
				     unsigned int uiBpp,
				     unsigned int uiUseFlags,
				     unsigned int *puiStride);

PVRDRIBufferImpl *PVRDRIBufferCreateFromNames(PVRDRIScreenImpl *psScreenImpl,
					   int iWidth,
					   int iHeight,
					   unsigned uiNumPlanes,
					   const int *piName,
					   const int *piStride,
					   const int *piOffset,
					   const unsigned int *puiWidthShift,
					   const unsigned int *puiHeightShift);

PVRDRIBufferImpl *PVRDRIBufferCreateFromName(PVRDRIScreenImpl *psScreenImpl,
					     int iName,
					     int iWidth,
					     int iHeight,
					     int iStride,
					     int iOffset);

PVRDRIBufferImpl *PVRDRIBufferCreateFromFds(PVRDRIScreenImpl *psScreenImpl,
					   int iWidth,
					   int iHeight,
					   unsigned uiNumPlanes,
					   const int *piFd,
					   const int *piStride,
					   const int *piOffset,
					   const unsigned int *puiWidthShift,
					   const unsigned int *puiHeightShift);

void PVRDRIBufferDestroy(PVRDRIBufferImpl *psBuffer);

int PVRDRIBufferGetFd(PVRDRIBufferImpl *psBuffer);

int PVRDRIBufferGetHandle(PVRDRIBufferImpl *psBuffer);

int PVRDRIBufferGetName(PVRDRIBufferImpl *psBuffer);


/* Image functions */
__EGLImage *PVRDRIEGLImageCreate(void);
__EGLImage *PVRDRIEGLImageCreateFromBuffer(int iWidth,
					   int iHeight,
					   int iStride,
					   IMG_PIXFMT ePixelFormat,
					   IMG_YUV_COLORSPACE eColourSpace,
					   IMG_YUV_CHROMA_INTERP eChromaUInterp,
					   IMG_YUV_CHROMA_INTERP eChromaVInterp,
					   PVRDRIBufferImpl *psBuffer);

__EGLImage *PVRDRIEGLImageDup(__EGLImage *psIn);

void PVRDRIEGLImageSetCallbackData(__EGLImage *psEGLImage, __DRIimage *image);

void PVRDRIEGLImageDestroyExternal(PVRDRIScreenImpl *psScreenImpl,
                                   __EGLImage *psEGLImage,
				   PVRDRIEGLImageType eglImageType);
void PVRDRIEGLImageFree(__EGLImage *psEGLImage);

void PVRDRIEGLImageGetAttribs(__EGLImage *psEGLImage, PVRDRIBufferAttribs *psAttribs);

/* Sync functions */
void *PVRDRICreateFenceImpl(PVRDRIAPIType eAPI,
			    PVRDRIScreenImpl *psScreenImpl,
			    PVRDRIContextImpl *psContextImpl);

void PVRDRIDestroyFenceImpl(void *psDRIFence);

bool PVRDRIClientWaitSyncImpl(PVRDRIAPIType eAPI,
			      PVRDRIContextImpl *psContextImpl,
			      void *psDRIFence,
			      bool bFlushCommands,
			      bool bTimeout,
			      uint64_t uiTimeout);

bool PVRDRIServerWaitSyncImpl(PVRDRIAPIType eAPI,
			      PVRDRIContextImpl *psContextImpl,
			      void *psDRIFence);

void PVRDRIDestroyFencesImpl(PVRDRIScreenImpl *psScreenImpl);

/* EGL interface functions */
bool PVRDRIEGLDrawableConfigFromGLMode(PVRDRIDrawableImpl *psPVRDrawable,
				       PVRDRIConfigInfo *psConfigInfo,
				       int supportedAPIs,
				       IMG_PIXFMT ePixFmt);

/* Callbacks into non-impl layer */
typedef struct
{
	bool                 (*DrawableRecreate)(PVRDRIDrawable *psPVRDrawable);
	bool                 (*DrawableGetParameters)(PVRDRIDrawable *psPVRDrawable,
	                                              PVRDRIBufferImpl **ppsDstBuffer,
	                                              PVRDRIBufferImpl **ppsAccumBuffer,
	                                              PVRDRIBufferAttribs *psAttribs,
	                                              bool *pbDoubleBuffered);
	PVRDRIImageType      (*ImageGetSharedType)(__DRIimage *image);
	PVRDRIBufferImpl    *(*ImageGetSharedBuffer)(__DRIimage *image);
	__EGLImage           *(*ImageGetSharedEGLImage)(__DRIimage *image);
	__EGLImage           *(*ImageGetEGLImage)(__DRIimage *image);
	__DRIimage          *(*ScreenGetDRIImage)(void *hEGLImage);
	void                 (*RefImage)(__DRIimage *image);
	void                 (*UnrefImage)(__DRIimage *image);
} PVRDRICallbacks;

void PVRDRIRegisterCallbacks(PVRDRICallbacks *callbacks);

/* PVR utility support functions */
bool PVRDRIMesaFormatSupported(unsigned fmt);
unsigned PVRDRIDepthStencilBitArraySize(void);
const uint8_t *PVRDRIDepthBitsArray(void);
const uint8_t *PVRDRIStencilBitsArray(void);
unsigned PVRDRIMSAABitArraySize(void);
const uint8_t *PVRDRIMSAABitsArray(void);
uint32_t PVRDRIMaxPBufferWidth(void);
uint32_t PVRDRIMaxPBufferHeight(void);

unsigned PVRDRIGetNumAPIFuncs(PVRDRIAPIType eAPI);
const char *PVRDRIGetAPIFunc(PVRDRIAPIType eAPI, unsigned index);

#endif /* defined(__PVRDRIIFCE_H__) */
