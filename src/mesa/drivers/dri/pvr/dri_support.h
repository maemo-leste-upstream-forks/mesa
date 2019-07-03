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

#include "imgpixfmts.h"
#include "imgyuv.h"

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
	PVRDRI_API_GLES1 = 2,
	PVRDRI_API_GLES2 = 3,
	PVRDRI_API_CL = 4,
} PVRDRIAPIType;

/* API sub type. */
typedef enum
{
	PVRDRI_API_SUB_NONE,
} PVRDRIAPISubType;

typedef enum
{
	PVRDRI_DRAWABLE_NONE = 0,
	PVRDRI_DRAWABLE_WINDOW = 1,
	PVRDRI_DRAWABLE_PIXMAP = 2,
	PVRDRI_DRAWABLE_PBUFFER = 3,
} PVRDRIDrawableType;

typedef enum
{
	PVRDRI_IMAGE = 1,
	PVRDRI_IMAGE_FROM_NAMES,
	PVRDRI_IMAGE_FROM_EGLIMAGE,
	PVRDRI_IMAGE_FROM_DMABUFS,
	PVRDRI_IMAGE_SUBIMAGE,
} PVRDRIImageType;

typedef enum
{
	PVRDRI_EGLIMAGE_NONE = 0,
	PVRDRI_EGLIMAGE_IMGEGL,
	PVRDRI_EGLIMAGE_IMGOCL,
} PVRDRIEGLImageType;

typedef enum
{
	/* Since PVRDRICallbacks version 2 */
	PVRDRI_CONFIG_ATTRIB_INVALID = 0,
	PVRDRI_CONFIG_ATTRIB_RENDERABLE_TYPE = 1,
	PVRDRI_CONFIG_ATTRIB_RGB_MODE = 2,
	PVRDRI_CONFIG_ATTRIB_DOUBLE_BUFFER_MODE = 3,
	PVRDRI_CONFIG_ATTRIB_RED_BITS = 4,
	PVRDRI_CONFIG_ATTRIB_GREEN_BITS = 5,
	PVRDRI_CONFIG_ATTRIB_BLUE_BITS = 6,
	PVRDRI_CONFIG_ATTRIB_ALPHA_BITS = 7,
	PVRDRI_CONFIG_ATTRIB_RGB_BITS = 8,
	PVRDRI_CONFIG_ATTRIB_DEPTH_BITS = 9,
	PVRDRI_CONFIG_ATTRIB_STENCIL_BITS = 10,
	PVRDRI_CONFIG_ATTRIB_SAMPLE_BUFFERS = 11,
	PVRDRI_CONFIG_ATTRIB_SAMPLES = 12,
	PVRDRI_CONFIG_ATTRIB_BIND_TO_TEXTURE_RGB = 13,
	PVRDRI_CONFIG_ATTRIB_BIND_TO_TEXTURE_RGBA = 14,
	PVRDRI_CONFIG_ATTRIB_YUV_ORDER = 15,
	PVRDRI_CONFIG_ATTRIB_YUV_NUM_OF_PLANES = 16,
	PVRDRI_CONFIG_ATTRIB_YUV_SUBSAMPLE = 17,
	PVRDRI_CONFIG_ATTRIB_YUV_DEPTH_RANGE = 18,
	PVRDRI_CONFIG_ATTRIB_YUV_CSC_STANDARD = 19,
	PVRDRI_CONFIG_ATTRIB_YUV_PLANE_BPP = 20,
} PVRDRIConfigAttrib;

typedef enum
{
	/* Since PVRDRICallbacks version 2 */
	PVRDRI_BUFFER_ATTRIB_INVALID = 0,
	PVRDRI_BUFFER_ATTRIB_TYPE = 1,
	PVRDRI_BUFFER_ATTRIB_WIDTH = 2,
	PVRDRI_BUFFER_ATTRIB_HEIGHT = 3,
	PVRDRI_BUFFER_ATTRIB_STRIDE = 4,
	PVRDRI_BUFFER_ATTRIB_PIXEL_FORMAT = 5,
} PVRDRIBufferAttrib;

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
#define PVRDRI_API_BIT_GLES3		0x0040

/* Mesa config formats. These need not match their MESA_FORMAT counterparts */
#define	PVRDRI_MESA_FORMAT_NONE			0
#define PVRDRI_MESA_FORMAT_B8G8R8A8_UNORM	1
#define PVRDRI_MESA_FORMAT_B8G8R8X8_UNORM	2
#define PVRDRI_MESA_FORMAT_B5G6R5_UNORM		3
#define PVRDRI_MESA_FORMAT_R8G8B8A8_UNORM	4
#define PVRDRI_MESA_FORMAT_R8G8B8X8_UNORM	5
#define PVRDRI_MESA_FORMAT_YCBCR		6
#define PVRDRI_MESA_FORMAT_YUV420_2PLANE	7
#define PVRDRI_MESA_FORMAT_YVU420_2PLANE	8

/* The blit flags match their DRI counterparts */
#define PVRDRI_BLIT_FLAG_FLUSH		0x0001
#define PVRDRI_BLIT_FLAG_FINISH		0x0002

/* The image mapping flags match their DRI counterparts */
#define	PVRDRI_IMAGE_TRANSFER_READ		0x1
#define	PVRDRI_IMAGE_TRANSFER_WRITE		0x2
#define	PVRDRI_IMAGE_TRANSFER_READ_WRITE	\
		(PVRDRI_IMAGE_TRANSFER_READ | PVRDRI_IMAGE_TRANSFER_WRITE)

/* The YUV defines match their DRI counterparts */
#define PVRDRI_YUV_ORDER_NONE 0x0
#define PVRDRI_YUV_ORDER_YUV  0x1
#define PVRDRI_YUV_ORDER_YVU  0x2
#define PVRDRI_YUV_ORDER_YUYV 0x4
#define PVRDRI_YUV_ORDER_UYVY 0x8
#define PVRDRI_YUV_ORDER_YVYU 0x10
#define PVRDRI_YUV_ORDER_VYUY 0x20
#define PVRDRI_YUV_ORDER_AYUV 0x40

#define PVRDRI_YUV_SUBSAMPLE_NONE  0x0
#define PVRDRI_YUV_SUBSAMPLE_4_2_0 0x1
#define PVRDRI_YUV_SUBSAMPLE_4_2_2 0x2
#define PVRDRI_YUV_SUBSAMPLE_4_4_4 0x4

#define PVRDRI_YUV_DEPTH_RANGE_NONE    0x0
#define PVRDRI_YUV_DEPTH_RANGE_LIMITED 0x1
#define PVRDRI_YUV_DEPTH_RANGE_FULL    0x2

#define PVRDRI_YUV_CSC_STANDARD_NONE 0x0
#define PVRDRI_YUV_CSC_STANDARD_601  0x1
#define PVRDRI_YUV_CSC_STANDARD_709  0x2
#define PVRDRI_YUV_CSC_STANDARD_2020 0x4

#define PVRDRI_YUV_PLANE_BPP_NONE 0x0
#define PVRDRI_YUV_PLANE_BPP_0    0x1
#define PVRDRI_YUV_PLANE_BPP_8    0x2
#define PVRDRI_YUV_PLANE_BPP_10   0x4

/* Flags for PVRDRICallbacks.DrawableGetParametersV2 */
/* Since callback interface version 2 */
#define PVRDRI_GETPARAMS_FLAG_ALLOW_RECREATE	0x1
/* Since callback interface version 3 */
#define PVRDRI_GETPARAMS_FLAG_NO_UPDATE		0x2

/*
 * Capabilities that might be returned by PVRDRIInterface.GetFenceCapabilities.
 * These match their _DRI_FENCE_CAP counterparts.
 *
 * Since PVRDRIInterface version 2.
 */
#define	PVRDRI_FENCE_CAP_NATIVE_FD 0x1

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

typedef struct IMGEGLImageRec IMGEGLImage;
typedef struct __DRIimageRec __DRIimage;

/* PVRDRI interface opaque types */
typedef struct PVRDRIScreenImplRec PVRDRIScreenImpl;
typedef struct PVRDRIContextImplRec PVRDRIContextImpl;
typedef struct PVRDRIDrawableImplRec PVRDRIDrawableImpl;
typedef struct PVRDRIBufferImplRec PVRDRIBufferImpl;

typedef struct PVRDRIDrawable_TAG PVRDRIDrawable;

typedef void (*PVRDRIGLAPIProc)(void);

/* Since PVRDRICallbacks version 2 */
typedef struct PVRDRIConfigRec PVRDRIConfig;

typedef struct {
	/**********************************************************************
	 * Version 0 interface
	 **********************************************************************/

	PVRDRIDeviceType     (*GetDeviceTypeFromFd)(int iFd);

	bool                 (*IsFirstScreen)(PVRDRIScreenImpl *psScreenImpl);

	uint32_t             (*PixFmtGetDepth)(IMG_PIXFMT eFmt);
	uint32_t             (*PixFmtGetBPP)(IMG_PIXFMT eFmt);
	uint32_t             (*PixFmtGetBlockSize)(IMG_PIXFMT eFmt);

	/* ScreenImpl functions */
	PVRDRIScreenImpl    *(*CreateScreen)(int iFd);
	void                 (*DestroyScreen)(PVRDRIScreenImpl *psScreenImpl);

	int                  (*APIVersion)(PVRDRIAPIType eAPI,
	                                   PVRDRIAPISubType eAPISub,
	                                   PVRDRIScreenImpl *psScreenImpl);

	void                *(*EGLGetLibHandle)(PVRDRIAPIType eAPI,
	                                        PVRDRIScreenImpl *psScreenImpl);

	PVRDRIGLAPIProc      (*EGLGetProcAddress)(PVRDRIAPIType eAPI,
	                                          PVRDRIScreenImpl *psScreenImpl,
	                                          const char *psProcName);

	bool                 (*EGLFlushBuffers)(PVRDRIAPIType eAPI,
	                                        PVRDRIScreenImpl *psScreenImpl,
	                                        PVRDRIContextImpl *psContextImpl,
	                                        PVRDRIDrawableImpl *psDrawableImpl,
	                                        bool bFlushAllSurfaces,
	                                        bool bSwapBuffers,
	                                        bool bWaitForHW);
	/* EGLFreeResources is deprecated */
	bool                 (*EGLFreeResources)(PVRDRIScreenImpl *psPVRScreenImpl);
	void                 (*EGLMarkRendersurfaceInvalid)(PVRDRIAPIType eAPI,
	                                                    PVRDRIScreenImpl *psScreenImpl,
	                                                    PVRDRIContextImpl *psContextImpl);
	/* EGLSetFrontBufferCallback is deprecated */
	void                 (*EGLSetFrontBufferCallback)(PVRDRIAPIType eAPI,
	                                                  PVRDRIScreenImpl *psScreenImpl,
	                                                  PVRDRIDrawableImpl *psDrawableImpl,
	                                                  void (*pfnCallback)(PVRDRIDrawable *));

	/* Deprecated in version 1 (since 1.10) */
	unsigned             (*CreateContext)(PVRDRIContextImpl **ppsContextImpl,
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

	void                 (*DestroyContext)(PVRDRIContextImpl *psContextImpl,
	                                           PVRDRIAPIType eAPI,
	                                           PVRDRIScreenImpl *psScreenImpl);

	bool                 (*MakeCurrentGC)(PVRDRIAPIType eAPI,
	                                      PVRDRIScreenImpl *psScreenImpl,
	                                      PVRDRIContextImpl *psContextImpl,
	                                      PVRDRIDrawableImpl *psWriteImpl,
	                                      PVRDRIDrawableImpl *psReadImpl);

	void                 (*MakeUnCurrentGC)(PVRDRIAPIType eAPI,
	                                        PVRDRIScreenImpl *psScreenImpl);

	unsigned             (*GetImageSource)(PVRDRIAPIType eAPI,
	                                       PVRDRIScreenImpl *psScreenImpl,
	                                       PVRDRIContextImpl *psContextImpl,
	                                       uint32_t  uiTarget,
	                                       uintptr_t uiBuffer,
	                                       uint32_t  uiLevel,
	                                       IMGEGLImage *psEGLImage);

	bool                 (*BindTexImage)(PVRDRIAPIType eAPI,
	                                     PVRDRIScreenImpl *psScreenImpl,
	                                     PVRDRIContextImpl *psContextImpl,
	                                     PVRDRIDrawableImpl *psDrawableImpl);

	void                 (*ReleaseTexImage)(PVRDRIAPIType eAPI,
	                                        PVRDRIScreenImpl *psScreenImpl,
	                                        PVRDRIContextImpl *psContextImpl,
	                                        PVRDRIDrawableImpl *psDrawableImpl);

	/* Deprecated in version 1 (since 1.10) */
	PVRDRIDrawableImpl  *(*CreateDrawable)(PVRDRIDrawable *psPVRDrawable);

	void                 (*DestroyDrawable)(PVRDRIDrawableImpl *psScreenImpl);
	bool                 (*EGLDrawableCreate)(PVRDRIScreenImpl *psScreenImpl,
	                                          PVRDRIDrawableImpl *psDrawableImpl);
	bool                 (*EGLDrawableRecreate)(PVRDRIScreenImpl *psScreenImpl,
	                                            PVRDRIDrawableImpl *psDrawableImpl);
	bool                 (*EGLDrawableDestroy)(PVRDRIScreenImpl *psScreenImpl,
	                                           PVRDRIDrawableImpl *psDrawableImpl);
	void                 (*EGLDrawableDestroyConfig)(PVRDRIDrawableImpl *psDrawableImpl);

	/* Buffer functions */
	PVRDRIBufferImpl    *(*BufferCreate)(PVRDRIScreenImpl *psScreenImpl,
	                                     int iWidth,
	                                     int iHeight,
	                                     unsigned int uiBpp,
	                                     unsigned int uiUseFlags,
	                                     unsigned int *puiStride);

	PVRDRIBufferImpl    *(*BufferCreateWithModifiers)(PVRDRIScreenImpl *psScreenImpl,
	                                                  int iWidth,
	                                                  int iHeight,
	                                                  int iFormat,
	                                                  IMG_PIXFMT eIMGPixelFormat,
	                                                  const uint64_t *puiModifiers,
	                                                  unsigned int uiModifierCount,
	                                                  unsigned int *puiStride);

	PVRDRIBufferImpl    *(*BufferCreateFromNames)(PVRDRIScreenImpl *psScreenImpl,
	                                              int iWidth,
	                                              int iHeight,
	                                              unsigned uiNumPlanes,
	                                              const int *piName,
	                                              const int *piStride,
	                                              const int *piOffset,
	                                              const unsigned int *puiWidthShift,
	                                              const unsigned int *puiHeightShift);

	PVRDRIBufferImpl    *(*BufferCreateFromName)(PVRDRIScreenImpl *psScreenImpl,
	                                             int iName,
	                                             int iWidth,
	                                             int iHeight,
	                                             int iStride,
	                                             int iOffset);

	/* BufferCreateFromFds is deprecated */
	PVRDRIBufferImpl    *(*BufferCreateFromFds)(PVRDRIScreenImpl *psScreenImpl,
	                                            int iWidth,
	                                            int iHeight,
	                                            unsigned uiNumPlanes,
	                                            const int *piFd,
	                                            const int *piStride,
	                                            const int *piOffset,
	                                            const unsigned int *puiWidthShift,
	                                            const unsigned int *puiHeightShift);

	PVRDRIBufferImpl    *(*BufferCreateFromFdsWithModifier)(PVRDRIScreenImpl *psScreenImpl,
	                                                        int iWidth,
	                                                        int iHeight,
	                                                        uint64_t uiModifier,
	                                                        unsigned uiNumPlanes,
	                                                        const int *piFd,
	                                                        const int *piStride,
	                                                        const int *piOffset,
	                                                        const unsigned int *puiWidthShift,
	                                                        const unsigned int *puiHeightShift);

	PVRDRIBufferImpl    *(*SubBufferCreate)(PVRDRIScreenImpl *psScreen,
	                                        PVRDRIBufferImpl *psParent,
	                                        int plane);

	void                 (*BufferDestroy)(PVRDRIBufferImpl *psBuffer);

	int                  (*BufferGetFd)(PVRDRIBufferImpl *psBuffer);

	int                  (*BufferGetHandle)(PVRDRIBufferImpl *psBuffer);

	uint64_t             (*BufferGetModifier)(PVRDRIBufferImpl *psBuffer);

	int                  (*BufferGetName)(PVRDRIBufferImpl *psBuffer);

	int                  (*BufferGetOffset)(PVRDRIBufferImpl *psBuffer);

	/* Image functions */
	IMGEGLImage         *(*EGLImageCreate)(void);
	IMGEGLImage         *(*EGLImageCreateFromBuffer)(int iWidth,
	                                                 int iHeight,
	                                                 int iStride,
	                                                 IMG_PIXFMT ePixelFormat,
	                                                 IMG_YUV_COLORSPACE eColourSpace,
	                                                 IMG_YUV_CHROMA_INTERP eChromaUInterp,
	                                                 IMG_YUV_CHROMA_INTERP eChromaVInterp,
	                                                 PVRDRIBufferImpl *psBuffer);

	IMGEGLImage         *(*EGLImageCreateFromSubBuffer)(IMG_PIXFMT ePixelFormat,
	                                                    PVRDRIBufferImpl *psSubBuffer);

	IMGEGLImage         *(*EGLImageDup)(IMGEGLImage *psIn);

	void                 (*EGLImageSetCallbackData)(IMGEGLImage *psEGLImage, __DRIimage *image);

	void                 (*EGLImageDestroyExternal)(PVRDRIScreenImpl *psScreenImpl,
	                                                IMGEGLImage *psEGLImage,
	                                                PVRDRIEGLImageType eglImageType);
	void                 (*EGLImageFree)(IMGEGLImage *psEGLImage);

	void                 (*EGLImageGetAttribs)(IMGEGLImage *psEGLImage,
	                                           PVRDRIBufferAttribs *psAttribs);

	/* Sync functions */
	void                *(*CreateFence)(PVRDRIAPIType eAPI,
	                                    PVRDRIScreenImpl *psScreenImpl,
	                                    PVRDRIContextImpl *psContextImpl);

	void                 (*DestroyFence)(void *psDRIFence);

	/*
	 * Support for flushing commands in ClientWaitSync is deprecated
	 * in version 2 (since 1.11), with the caller being responsible
	 * for the flush. A context and API need not be supplied if a flush
	 * isn't requested.
	 */
	bool                 (*ClientWaitSync)(PVRDRIAPIType eAPI,
	                                       PVRDRIContextImpl *psContextImpl,
	                                       void *psDRIFence,
	                                       bool bFlushCommands,
	                                       bool bTimeout,
	                                       uint64_t uiTimeout);

	bool                 (*ServerWaitSync)(PVRDRIAPIType eAPI,
	                                       PVRDRIContextImpl *psContextImpl,
	                                       void *psDRIFence);

	/* Deprecated in version 2 (since 1.11) */
	void                 (*DestroyFences)(PVRDRIScreenImpl *psScreenImpl);

	/* EGL interface functions */
	/* Deprecated in version 1 (since 1.10) */
	bool                 (*EGLDrawableConfigFromGLMode)(PVRDRIDrawableImpl *psPVRDrawable,
	                                                    PVRDRIConfigInfo *psConfigInfo,
	                                                    int supportedAPIs,
	                                                    IMG_PIXFMT ePixFmt);

	/* Blit functions */
	bool                 (*BlitEGLImage)(PVRDRIScreenImpl *psScreenImpl,
	                                     PVRDRIContextImpl *psContextImpl,
	                                     IMGEGLImage *psDstImage,
	                                     PVRDRIBufferImpl *psDstBuffer,
	                                     IMGEGLImage *psSrcImage,
	                                     PVRDRIBufferImpl *psSrcBuffer,
	                                     int iDstX, int iDstY,
	                                     int iDstWidth, int iDstHeight,
	                                     int iSrcX, int iSrcY,
	                                     int iSrcWidth, int iSrcHeight,
	                                     int iFlushFlag);

	/* Mapping functions */
	void                 *(*MapEGLImage)(PVRDRIScreenImpl *psScreenImpl,
	                                     PVRDRIContextImpl *psContextImpl,
	                                     IMGEGLImage *psImage,
	                                     PVRDRIBufferImpl *psBuffer,
	                                     int iX, int iY,
	                                     int iWidth, int iHeight,
	                                     unsigned iFlags,
	                                     int *piStride,
	                                     void **ppvData);

	bool                 (*UnmapEGLImage)(PVRDRIScreenImpl *psScreenImpl,
	                                      PVRDRIContextImpl *psContextImpl,
	                                      IMGEGLImage *psImage, PVRDRIBufferImpl *psBuffer,
	                                      void *pvData);

	/* PVR utility support functions */
	bool                 (*MesaFormatSupported)(unsigned fmt);
	unsigned             (*DepthStencilBitArraySize)(void);
	const uint8_t       *(*DepthBitsArray)(void);
	const uint8_t       *(*StencilBitsArray)(void);
	unsigned             (*MSAABitArraySize)(void);
	const uint8_t       *(*MSAABitsArray)(void);
	uint32_t             (*MaxPBufferWidth)(void);
	uint32_t             (*MaxPBufferHeight)(void);

	unsigned             (*GetNumAPIFuncs)(PVRDRIAPIType eAPI);
	const char          *(*GetAPIFunc)(PVRDRIAPIType eAPI, unsigned index);

	int                  (*QuerySupportedFormats)(PVRDRIScreenImpl *psScreenImpl,
	                                              unsigned uNumFormats,
	                                              const int *piFormats,
	                                              const IMG_PIXFMT *peImgFormats,
	                                              bool *pbSupported);

	int                  (*QueryModifiers)(PVRDRIScreenImpl *psScreenImpl,
	                                       int iFormat,
	                                       IMG_PIXFMT eImgFormat,
	                                       uint64_t *puModifiers,
	                                       unsigned *puExternalOnly);

	/**********************************************************************
	 * Version 1 functions
	 **********************************************************************/

	unsigned             (*CreateContextV1)(PVRDRIScreenImpl *psScreenImpl,
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

	PVRDRIDrawableImpl  *(*CreateDrawableWithConfig)(PVRDRIDrawable *psPVRDrawable,
							 PVRDRIConfig *psConfig);

	/**********************************************************************
	 * Version 2 functions
	 **********************************************************************/

	unsigned             (*GetFenceCapabilities)(PVRDRIScreenImpl *psScreenImpl);

	void                *(*CreateFenceFd)(PVRDRIAPIType eAPI,
	                                      PVRDRIScreenImpl *psScreenImpl,
	                                      PVRDRIContextImpl *psContextImpl,
                                              int iFd);

	int                  (*GetFenceFd)(void *psDRIFence);
} PVRDRISupportInterface;

/* Callbacks into non-impl layer */
typedef struct
{
	/**********************************************************************
	 * Version 0 callbacks
	 **********************************************************************/

	/*
	 * DrawableRecreate and DrawableGetParameters are deprecated in
	 * version 1.
	 */
	bool                 (*DrawableRecreate)(PVRDRIDrawable *psPVRDrawable);
	bool                 (*DrawableGetParameters)(PVRDRIDrawable *psPVRDrawable,
	                                              PVRDRIBufferImpl **ppsDstBuffer,
	                                              PVRDRIBufferImpl **ppsAccumBuffer,
	                                              PVRDRIBufferAttribs *psAttribs,
	                                              bool *pbDoubleBuffered);

	PVRDRIImageType      (*ImageGetSharedType)(__DRIimage *image);
	PVRDRIBufferImpl    *(*ImageGetSharedBuffer)(__DRIimage *image);
	IMGEGLImage           *(*ImageGetSharedEGLImage)(__DRIimage *image);
	IMGEGLImage           *(*ImageGetEGLImage)(__DRIimage *image);
	__DRIimage          *(*ScreenGetDRIImage)(void *hEGLImage);
	void                 (*RefImage)(__DRIimage *image);
	void                 (*UnrefImage)(__DRIimage *image);

	/*
	 * If the DRI module calls PVRDRIRegisterCallbacks, or
	 * PVRDRIRegisterVersionedCallbacks with any version number,
	 * the DRI support library can use the callbacks above.
	 * The callbacks below can only be called if
	 * PVRDRIRegisterVersionedCallbacks is called with a suitable
	 * version number.
	 */

	/**********************************************************************
	 * Version 1 callbacks
	 **********************************************************************/

	/*
	 * Deprecated in version 2 (since 1.10).
	 *
	 * DrawableGetParametersV1 is a replacement for DrawableRecreate
	 * and DrawableGetParameters. The DRI Support library must use one
	 * interface or the other, otherwise the results are undefined.
	 */
	bool                 (*DrawableGetParametersV1)(PVRDRIDrawable *psPVRDrawable,
                                                        bool bAllowRecreate,
	                                                PVRDRIBufferImpl **ppsDstBuffer,
	                                                PVRDRIBufferImpl **ppsAccumBuffer,
	                                                PVRDRIBufferAttribs *psAttribs,
	                                                bool *pbDoubleBuffered);

	/*
	 * Register the DRI Support interface with the DRI module.
	 * The caller is not required to preserve the PVRDRICallbacks structure
	 * after the call, so the callee must make a copy.
	 */
	bool                 (*RegisterSupportInterfaceV1)(const PVRDRISupportInterface *psInterface,
							   unsigned uVersion);

	/**********************************************************************
	 * Version 2 callbacks
	 **********************************************************************/

	bool                 (*ConfigQuery)(const PVRDRIConfig *psConfig,
	                                    PVRDRIConfigAttrib eConfigAttrib,
	                                    int *piValueOut);
	/*
	 * DrawableGetParametersV2 is a replacement for DrawableGetParametersV1.
	 * Unlike earlier versions, the caller is expected to query drawable
	 * information (via DrawableQuery) instead of this information being
	 * returned by the callback.
	 */
	bool                 (*DrawableGetParametersV2)(PVRDRIDrawable *psPVRDrawable,
	                                                uint32_t uiFlags,
	                                                PVRDRIBufferImpl **ppsDstBuffer,
	                                                PVRDRIBufferImpl **ppsAccumBuffer);
	bool                 (*DrawableQuery)(const PVRDRIDrawable *psPVRDrawable,
	                                      PVRDRIBufferAttrib eBufferAttrib,
	                                      uint32_t *uiValueOut);

	/**********************************************************************
	 * Version 3 callbacks
	 **********************************************************************/
} PVRDRICallbacks;

/*
 * Older versions of the DRI support library don't support
 * PVRDRIRegisterVersionedCallbacks.
 * The caller is not required to preserve the PVRDRICallbacks structure
 * after the call, so the callee must make a copy.
 */
bool PVRDRIRegisterVersionedCallbacks(const PVRDRICallbacks *psCallbacks,
				      unsigned uVersion);

/******************************************************************************
 * Everything beyond this point is deprecated
 ******************************************************************************/

/*
 * Calling PVRDRIRegisterCallbacks is equivalent to calling
 * PVRDRIRegisterVersionedCallbacks with a version number of zero.
 * The caller is not required to preserve the PVRDRICallbacks structure
 * after the call, so the callee must make a copy.
 */
void PVRDRIRegisterCallbacks(PVRDRICallbacks *callbacks);

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
			      IMGEGLImage *psEGLImage);

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

PVRDRIBufferImpl *
PVRDRIBufferCreateWithModifiers(PVRDRIScreenImpl *psScreenImpl,
			        int iWidth,
			        int iHeight,
			        int iFormat,
				IMG_PIXFMT eIMGPixelFormat,
			        const uint64_t *puiModifiers,
			        unsigned int uiModifierCount,
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
				      const unsigned int *puiHeightShift);

PVRDRIBufferImpl *PVRDRISubBufferCreate(PVRDRIScreenImpl *psScreen,
					PVRDRIBufferImpl *psParent,
					int plane);

void PVRDRIBufferDestroy(PVRDRIBufferImpl *psBuffer);

int PVRDRIBufferGetFd(PVRDRIBufferImpl *psBuffer);

int PVRDRIBufferGetHandle(PVRDRIBufferImpl *psBuffer);

uint64_t PVRDRIBufferGetModifier(PVRDRIBufferImpl *psBuffer);

int PVRDRIBufferGetName(PVRDRIBufferImpl *psBuffer);

int PVRDRIBufferGetOffset(PVRDRIBufferImpl *psBuffer);

/* Image functions */
IMGEGLImage *PVRDRIEGLImageCreate(void);
IMGEGLImage *PVRDRIEGLImageCreateFromBuffer(int iWidth,
					   int iHeight,
					   int iStride,
					   IMG_PIXFMT ePixelFormat,
					   IMG_YUV_COLORSPACE eColourSpace,
					   IMG_YUV_CHROMA_INTERP eChromaUInterp,
					   IMG_YUV_CHROMA_INTERP eChromaVInterp,
					   PVRDRIBufferImpl *psBuffer);

IMGEGLImage *PVRDRIEGLImageCreateFromSubBuffer(IMG_PIXFMT ePixelFormat,
					       PVRDRIBufferImpl *psSubBuffer);

IMGEGLImage *PVRDRIEGLImageDup(IMGEGLImage *psIn);

void PVRDRIEGLImageSetCallbackData(IMGEGLImage *psEGLImage, __DRIimage *image);

void PVRDRIEGLImageDestroyExternal(PVRDRIScreenImpl *psScreenImpl,
                                   IMGEGLImage *psEGLImage,
				   PVRDRIEGLImageType eglImageType);
void PVRDRIEGLImageFree(IMGEGLImage *psEGLImage);

void PVRDRIEGLImageGetAttribs(IMGEGLImage *psEGLImage, PVRDRIBufferAttribs *psAttribs);

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

/* Blit functions */
bool PVRDRIBlitEGLImage(PVRDRIScreenImpl *psScreenImpl,
		        PVRDRIContextImpl *psContextImpl,
			IMGEGLImage *psDstImage, PVRDRIBufferImpl *psDstBuffer,
			IMGEGLImage *psSrcImage, PVRDRIBufferImpl *psSrcBuffer,
			int iDstX, int iDstY, int iDstWidth, int iDstHeight,
			int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight,
			int iFlushFlag);

/* Mapping functions */
void *PVRDRIMapEGLImage(PVRDRIScreenImpl *psScreenImpl,
			PVRDRIContextImpl *psContextImpl,
			IMGEGLImage *psImage, PVRDRIBufferImpl *psBuffer,
			int iX, int iY, int iWidth, int iHeight,
			unsigned uiFlags, int *piStride, void **ppvData);

bool PVRDRIUnmapEGLImage(PVRDRIScreenImpl *psScreenImpl,
			 PVRDRIContextImpl *psContextImpl,
			 IMGEGLImage *psImage, PVRDRIBufferImpl *psBuffer,
			 void *pvData);

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

int PVRDRIQuerySupportedFormats(PVRDRIScreenImpl *psScreenImpl,
				unsigned uNumFormats,
				const int *piFormats,
				const IMG_PIXFMT *peImgFormats,
				bool *pbSupported);

int PVRDRIQueryModifiers(PVRDRIScreenImpl *psScreenImpl,
			 int iFormat,
			 IMG_PIXFMT eImgFormat,
			 uint64_t *puModifiers,
			 unsigned *puExternalOnly);

#endif /* defined(__PVRDRIIFCE_H__) */
