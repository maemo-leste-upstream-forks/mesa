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

/* API type. */
typedef enum
{
	PVRDRI_API_NONE = 0,
	PVRDRI_API_GLES1 = 2,
	PVRDRI_API_GLES2 = 3,
	PVRDRI_API_CL = 4,
	PVRDRI_API_GL_COMPAT = 5,
	PVRDRI_API_GL_CORE = 6,
} PVRDRIAPIType;

typedef enum
{
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
	PVRDRI_CONFIG_ATTRIB_RED_MASK = 21,
	PVRDRI_CONFIG_ATTRIB_GREEN_MASK = 22,
	PVRDRI_CONFIG_ATTRIB_BLUE_MASK = 23,
	PVRDRI_CONFIG_ATTRIB_ALPHA_MASK = 24,
	PVRDRI_CONFIG_ATTRIB_SRGB_CAPABLE = 25
} PVRDRIConfigAttrib;

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
#define PVRDRI_MESA_FORMAT_YCBCR		6
#define PVRDRI_MESA_FORMAT_YUV420_2PLANE	7
#define PVRDRI_MESA_FORMAT_YVU420_2PLANE	8
#define PVRDRI_MESA_FORMAT_B8G8R8A8_SRGB	9
#define PVRDRI_MESA_FORMAT_R8G8B8A8_SRGB	10

typedef struct __DRIimageRec __DRIimage;

typedef struct PVRDRIConfigRec PVRDRIConfig;

/* The PVRDRI_GL defines match their EGL_GL counterparts */
#define PVRDRI_GL_RENDERBUFFER			0x30B9
#define PVRDRI_GL_TEXTURE_2D			0x30B1
#define PVRDRI_GL_TEXTURE_3D			0x30B2
#define PVRDRI_GL_TEXTURE_CUBE_MAP_POSITIVE_X	0x30B3
#define PVRDRI_GL_TEXTURE_CUBE_MAP_NEGATIVE_X	0x30B4
#define PVRDRI_GL_TEXTURE_CUBE_MAP_POSITIVE_Y	0x30B5
#define PVRDRI_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y	0x30B6
#define PVRDRI_GL_TEXTURE_CUBE_MAP_POSITIVE_Z	0x30B7
#define PVRDRI_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z	0x30B8

struct __DRIscreenRec;
struct __DRIcontextRec;
struct __DRIdrawableRec;

struct __DRIconfigRec;

struct DRISUPScreen;
struct DRISUPContext;
struct DRISUPDrawable;
struct DRISUPBuffer;

struct PVRDRIContextConfig
{
	unsigned int uMajorVersion;
	unsigned int uMinorVersion;
	uint32_t uFlags;

	int iResetStrategy;
	unsigned int uPriority;
	int iReleaseBehavior;
};

/*
 * PVR DRI Support interface V2.
 * This structure may change over time, as older interfaces become obsolete.
 * For example, the v0 interface may be removed if superseded by newer
 * interfaces.
 */
struct  PVRDRISupportInterfaceV2
{
	struct
	{
		struct DRISUPScreen *(*CreateScreen)
			(struct __DRIscreenRec *psDRIScreen,
			 int iFD,
			 bool bUseInvalidate,
			 void *pvLoaderPrivate,
			 const struct __DRIconfigRec ***pppsConfigs,
			 int *piMaxGLES1Version,
			 int *piMaxGLES2Version);

		void (*DestroyScreen)
			(struct DRISUPScreen *psDRISUPScreen);

		unsigned int (*CreateContext)
			(PVRDRIAPIType eAPI,
			 PVRDRIConfig *psPVRDRIConfig,
			 struct PVRDRIContextConfig *psCtxConfig,
			 struct __DRIcontextRec *psDRIContext,
			 struct DRISUPContext *psDRISUPSharedContext,
			 struct DRISUPScreen *psDRISUPScreen,
			 struct DRISUPContext **ppsDRISUPContext);

		void (*DestroyContext)
			(struct DRISUPContext *psDRISUPContext);

		struct DRISUPDrawable *(*CreateDrawable)
			(struct __DRIdrawableRec *psDRIDrawable,
			 struct DRISUPScreen *psDRISUPDrawable,
			 void *pvLoaderPrivate,
			 PVRDRIConfig *psPVRDRIConfig);

		void (*DestroyDrawable)
			(struct DRISUPDrawable *psDRISUPDrawable);

		bool (*MakeCurrent)
			(struct DRISUPContext *psDRISUPContext,
			 struct DRISUPDrawable *psDRISUPWrite,
			 struct DRISUPDrawable *psDRISUPRead);

		bool (*UnbindContext)
			(struct DRISUPContext *psDRISUPContext);

		struct DRISUPBuffer *(*AllocateBuffer)
			(struct DRISUPScreen *psDRISUPScreen,
			 unsigned int uAttachment,
			 unsigned int uFormat,
			 int iWidth,
			 int iHeight,
			 unsigned int *puName,
			 unsigned int *puPitch,
			 unsigned int *puCPP,
			 unsigned int *puFlags);

		void (*ReleaseBuffer)
			(struct DRISUPScreen *psDRISUPScreen,
			 struct DRISUPBuffer *psDRISUPBuffer);

		/* Functions to support the DRI TexBuffer extension */
		void (*SetTexBuffer2)
			(struct DRISUPContext *psDRISUPContext,
			 int iTarget,
			 int iFormat,
			 struct DRISUPDrawable *psDRISUPDrawable);

		void (*ReleaseTexBuffer)
			(struct DRISUPContext *psDRISUPContext,
			 int iTarget,
			 struct DRISUPDrawable *psDRISUPDrawable);

		/* Functions to support the DRI Flush extension */
		void (*Flush)
			(struct DRISUPDrawable *psDRISUPDrawable);

		void (*Invalidate)
			(struct DRISUPDrawable *psDRISUPDrawable);

		void (*FlushWithFlags)
			(struct DRISUPContext *psDRISUPContext,
			 struct DRISUPDrawable *psDRISUPDrawable,
			 unsigned int uFlags,
			 unsigned int uThrottleReason);

		/* Functions to support the DRI Image extension */
		__DRIimage *(*CreateImageFromName)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iWidth,
			 int iHeight,
			 int iFourCC,
			 int iName,
			 int iPitch,
			 void *pvLoaderPrivate);

		__DRIimage *(*CreateImageFromRenderbuffer)
			(struct DRISUPContext *psDRISUPContext,
			int iRenderBuffer,
			void *pvLoaderPrivate);

		void (*DestroyImage)
			(__DRIimage *psImage);

		__DRIimage *(*CreateImage)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iWidth,
			 int iHeight,
			 int iFourCC,
			 unsigned int uUse,
			 void *pvLoaderPrivate);

		bool (*QueryImage)
			(__DRIimage *psImage,
			 int iAttrib,
			 int *iValue);

		__DRIimage *(*DupImage)
			(__DRIimage *psImage,
			 void *pvLoaderPrivate);

		bool (*ValidateImageUsage)
			(__DRIimage *psImage,
			 unsigned int uUse);

		__DRIimage *(*CreateImageFromNames)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iWidth,
			 int iHeight,
			 int iFourCC,
			 int *piNames,
			 int iNumNames,
			 int *piStrides,
			 int *piOffsets,
			 void *pvLoaderPrivate);
			 __DRIimage *(*FromPlanar)(__DRIimage *psImage,
			 int iPlane,
			 void *pvLoaderPrivate);

		__DRIimage *(*CreateImageFromTexture)
			(struct DRISUPContext *psDRISUPContext,
			 int iTarget,
			 unsigned int uTexture,
			 int iDepth,
			 int iLevel,
			 unsigned int *puError,
			 void *pvLoaderPrivate);

		__DRIimage *(*CreateImageFromFDs)
			(struct DRISUPScreen *psDRISUPcreen,
			 int iWidth,
			 int iHeight,
			 int iFourCC,
			 int *piFDs,
			 int iNumFDs,
			 int *piStrides,
			 int *piOffsets,
			 void *pvLoaderPrivate);

		__DRIimage *(*CreateImageFromDMABufs)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iWidth,
			 int iHeight,
			 int iFourCC,
			 int *piFDs,
			 int iNumFDs,
			 int *piStrides,
			 int *piOffsets,
			 unsigned int uColorSpace,
			 unsigned int uSampleRange,
			 unsigned int uHorizSiting,
			 unsigned int uVertSiting,
			 unsigned int *puError,
			 void *pvLoaderPrivate);

		int (*GetImageCapabilities)
			(struct DRISUPScreen *psDRISUPScreen);

		void (*BlitImage)
			(struct DRISUPContext *psDRISUPContext,
			 __DRIimage *psDst,
			 __DRIimage *psSrc,
			 int iDstX0,
			 int iDstY0,
			 int iDstWidth,
			 int iDstHeight,
			 int iSrcX0, int
			 iSrcY0,
			 int iSrcWidth,
			 int iSrcHeight,
			 int iFlushFlag);

		void *(*MapImage)
			(struct DRISUPContext *psDRISUPContext,
			 __DRIimage *psImage,
			 int iX0,
			 int iY0,
			 int iWidth,
			 int iHeight,
			 unsigned int uFlags,
			 int *piStride,
			 void **ppvData);

		void (*UnmapImage)
			(struct DRISUPContext *psDRISUPContext,
			 __DRIimage *psImage,
			 void *pvData);

		__DRIimage *(*CreateImageWithModifiers)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iWidth,
			 int iHeight,
			 int iFourCC,
			 const uint64_t *puModifiers,
			 const unsigned int uModifierCount,
			 void *pvLoaderPrivate);

		__DRIimage *(*CreateImageFromDMABufs2)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iWidth,
			 int iHeight,
			 int iFourCC,
			 uint64_t uModifier,
			 int *piFDs,
			 int iNumFDs,
			 int *piStrides,
			 int *piOffsets,
			 unsigned int uColorSpace,
			 unsigned int uSampleRange,
			 unsigned int uHorizSiting,
			 unsigned int uVertSiting,
			 unsigned int *puError,
			 void *pvLoaderPrivate);

		bool (*QueryDMABufFormats)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iMax,
			 int *piFormats,
			 int *piCount);

		bool (*QueryDMABufModifiers)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iFourCC,
			 int iMax,
			 uint64_t *puModifiers,
			 unsigned int *piExternalOnly,
			 int *piCount);

		bool (*QueryDMABufFormatModifierAttribs)
			(struct DRISUPScreen *psDRISUPScreen,
			 uint32_t uFourcc,
			 uint64_t uModifier,
			 int iAttrib,
			 uint64_t *puValue);

		__DRIimage *(*CreateImageFromRenderBuffer2)
			(struct DRISUPContext *psDRISUPContext,
			 int iRenderBuffer,
			 void *pvLoaderPrivate,
			 unsigned int *puError);

		__DRIimage *(*CreateImageFromBuffer)
			(struct DRISUPContext *psDRISUPContext,
			 int iTarget,
			 void *pvBuffer,
			 unsigned int *puError,
			 void *pvLoaderPrivate);

		/* Functions to support the DRI Renderer Query extension */
		int (*QueryRendererInteger)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iAttribute,
			 unsigned int *puValue);

		int (*QueryRendererString)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iAttribute,
			 const char **ppszValue);

		/* Functions to support the DRI Fence extension */
		void *(*CreateFence)
			(struct DRISUPContext *psDRISUPContext);

		void (*DestroyFence)
			(struct DRISUPScreen *psDRISUPScreen,
			 void *pvFence);

		bool (*ClientWaitSync)
			(struct DRISUPContext *psDRISUPContext,
			 void *pvFence,
			 unsigned int uFlags,
			 uint64_t uTimeout);

		void (*ServerWaitSync)
			(struct DRISUPContext *psDRISUPContext,
			 void *pvFence,
			 unsigned int uFlags);

		unsigned int (*GetFenceCapabilities)
			(struct DRISUPScreen *psDRISUPScreen);

		void *(*CreateFenceFD)
			(struct DRISUPContext *psDRISUPContext,
			 int iFD);

		int (*GetFenceFD)
			(struct DRISUPScreen *psDRISUPScreen,
			 void *pvFence);

		unsigned int (*GetNumAPIProcs)
			(struct DRISUPScreen *psDRISUPScreen,
			 PVRDRIAPIType eAPI);

		const char *(*GetAPIProcName)
			(struct DRISUPScreen *psDRISUPScreen,
			 PVRDRIAPIType eAPI,
			 unsigned int uIndex);

		void *(*GetAPIProcAddress)
			(struct DRISUPScreen *psDRISUPScreen,
			 PVRDRIAPIType eAPI,
			 unsigned int uIndex);

		void (*SetDamageRegion)
			(struct DRISUPDrawable *psDRISUPDrawable,
			 unsigned int uNRects,
			 int *piRects);
	} v0;
	/* The v1 interface is an extension of v0, so v0 is required as well */
	struct {
		void *(*GetFenceFromCLEvent)
			(struct DRISUPScreen *psDRISUPScreen,
			 intptr_t iCLEvent);
	} v1;
	/* The v2 interface is an extension of v1, so v1 is required as well */
	struct {
		int (*GetAPIVersion)
			(struct DRISUPScreen *psDRISUPScreen,
			 PVRDRIAPIType eAPI);
	} v2;
	/*
	 * The v3 interface has no additional entry points. It indicates that
	 * OpenCL event based fences are available, provided the DDK is built
	 * with OpenCL support.
	 */
	/* The v4 interface is an extension of v3, so v3 is required as well */
	struct {
		bool (*HaveGetFenceFromCLEvent)(void);
	} v4;
	/* The v5 interface is an extension of v4, so v4 is required as well */
	struct {
		__DRIimage *(*CreateImageFromDMABufs3)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iWidth,
			 int iHeight,
			 int iFourCC,
			 uint64_t uModifier,
			 int *piFDs,
			 int iNumFDs,
			 int *piStrides,
			 int *piOffsets,
			 unsigned int uColorSpace,
			 unsigned int uSampleRange,
			 unsigned int uHorizSiting,
			 unsigned int uVertSiting,
			 uint32_t uFlags,
			 unsigned int *puError,
			 void *pvLoaderPrivate);

		__DRIimage *(*CreateImageWithModifiers2)
			(struct DRISUPScreen *psDRISUPScreen,
			 int iWidth,
			 int iHeight,
			 int iFourCC,
			 const uint64_t *puModifiers,
			 const unsigned int uModifierCount,
			 unsigned int uUsage,
			 void *pvLoaderPrivate);

		__DRIimage *(*CreateImageFromFDs2)
			(struct DRISUPScreen *psDRISUPcreen,
			 int iWidth,
			 int iHeight,
			 int iFourCC,
			 int *piFDs,
			 int iNumFDs,
			 uint32_t uFlags,
			 int *piStrides,
			 int *piOffsets,
			 void *pvLoaderPrivate);

		void (*SetInFenceFD)
			(__DRIimage *psImage,
			 int iFD);
	} v5;
};

struct PVRDRIImageList {
	uint32_t uImageMask;
	__DRIimage *psBack;
	__DRIimage *psFront;
	__DRIimage *psPrev;
};

/*
 * PVR DRI Support callback interface V2.
 * This structure may change over time, as older interfaces become obsolete.
 * For example, the v0 interface may be removed if superseded by newer
 * interfaces.
 */
struct PVRDRICallbacksV2
{
	struct {
		bool (*RegisterSupportInterface)
			(const void *psInterface,
			 unsigned int uVersion,
			 unsigned int uMinVersion);

		int (*GetBuffers)
			(struct __DRIdrawableRec *psDRIDrawable,
			 unsigned int uFourCC,
			 uint32_t *puStamp,
			 void *pvLoaderPrivate,
			 uint32_t uBufferMask,
			 struct PVRDRIImageList *psImageList);

		bool (*CreateConfigs)
			(struct __DRIconfigRec ***pppsConfigs,
			 struct __DRIscreenRec *psDRIScreen,
			 int iPVRDRIMesaFormat,
			 const uint8_t *puDepthBits,
			 const uint8_t *puStencilBits,
			 unsigned int uNumDepthStencilBits,
			 const unsigned int *puDBModes,
			 unsigned int uNumDBModes,
			 const uint8_t *puMSAASamples,
			 unsigned int uNumMSAAModes,
			 bool bEnableAccum,
			 bool bColorDepthMatch,
			 bool bMutableRenderBuffer,
			 int iYUVDepthRange,
			 int iYUVCSCStandard,
			 uint32_t uMaxPbufferWidth,
			 uint32_t uMaxPbufferHeight);

		struct __DRIconfigRec **(*ConcatConfigs)
			(struct __DRIscreenRec *psDRIScreen,
			 struct __DRIconfigRec **ppsConfigA,
			 struct __DRIconfigRec **ppsConfigB);

		bool (*ConfigQuery)
			(const PVRDRIConfig *psConfig,
			 PVRDRIConfigAttrib eConfigAttrib,
			 unsigned int *puValueOut);

		__DRIimage *(*LookupEGLImage)
			(struct __DRIscreenRec *psDRIScreen,
			 void *pvImage,
			 void *pvLoaderPrivate);

		unsigned int (*GetCapability)
			(struct __DRIscreenRec *psDRIScreen,
			 unsigned int uCapability);
	} v0;
	/* The v1 interface is an extension of v0, so v0 is required as well */
	struct {
		void (*FlushFrontBuffer)
			(struct __DRIdrawableRec *psDRIDrawable,
			 void *pvLoaderPrivate);
	} v1;
	/* The v2 interface is an extension of v1, so v1 is required as well */
	struct {
		int (*GetDisplayFD)
			(struct __DRIscreenRec *psDRIScreen,
			 void *pvLoaderPrivate);
	} v2;
	/* The v3 interface is an extension of v2, so v2 is required as well */
	struct {
		void *(*DrawableGetReferenceHandle)
			(struct __DRIdrawableRec *psDRIDrawable);

		void (*DrawableAddReference)
			(void *pvReferenceHandle);

		void (*DrawableRemoveReference)
			(void *pvReferenceHandle);
	} v3;
	/* The v4 interface is an extension of v3, so v3 is required as well */
	struct {
		void (*DestroyLoaderImageState)
			(const struct __DRIscreenRec *psDRIScreen,
			 void *pvLoaderPrivate);
	} v4;
};

#endif /* defined(__PVRDRIIFCE_H__) */
