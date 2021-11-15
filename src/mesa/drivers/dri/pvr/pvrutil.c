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

#include "drm-uapi/drm_fourcc.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "dri_util.h"

#include "pvrdri.h"

#define MESSAGE_LENGTH_MAX 1024

/*
 * define before including android/log.h and dlog.h as this is used by these
 * headers
 */
#define LOG_TAG "PVR-MESA"

#if defined(HAVE_ANDROID_PLATFORM)
#include <android/log.h>
#define err_printf(f, args...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, f, ##args))
#define dbg_printf(f, args...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, f, ##args))
#elif defined(HAVE_TIZEN_PLATFORM)
#include <dlog.h>
#define err_printf(f, args...) LOGE(f, ##args)
#define dbg_printf(f, args...) LOGD(f, ##args)
#else
#define err_printf(f, args...) fprintf(stderr, f, ##args)
#define dbg_printf(f, args...) fprintf(stderr, "LibGL: " f "\n", ##args)
#endif /* HAVE_ANDROID_PLATFORM */

#define	PVRDRIMesaFormatEntry(f) {f, PVRDRI_ ## f }

static const struct
{
	mesa_format eMesa;
	unsigned uPVRDRI;
} g_asMesaFormats[] = {
	PVRDRIMesaFormatEntry(MESA_FORMAT_B8G8R8A8_UNORM),
	PVRDRIMesaFormatEntry(MESA_FORMAT_B8G8R8X8_UNORM),
#ifdef HAVE_ANDROID_PLATFORM
	PVRDRIMesaFormatEntry(MESA_FORMAT_R8G8B8A8_UNORM),
	PVRDRIMesaFormatEntry(MESA_FORMAT_R8G8B8X8_UNORM),
#endif
	PVRDRIMesaFormatEntry(MESA_FORMAT_B5G6R5_UNORM),
};

/* See pvrdri.h for documentation on PVRDRIImageFormat */
static const PVRDRIImageFormat g_asFormats[] =
{
	{
		.eIMGPixelFormat = IMG_PIXFMT_R10G10B10A2_UNORM,
		.iDRIFourCC = DRM_FORMAT_ABGR2101010,
		.iDRIFormat = __DRI_IMAGE_FORMAT_ABGR2101010,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_RGBA,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
			.eIMGPixelFormat = IMG_PIXFMT_R10G10B10A2_UNORM,
			.uiWidthShift = 0,
			.uiHeightShift = 0
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_B8G8R8A8_UNORM,
		.iDRIFourCC = DRM_FORMAT_ARGB8888,
		.iDRIFormat = __DRI_IMAGE_FORMAT_ARGB8888,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_RGBA,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_B8G8R8A8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_R8G8B8A8_UNORM,
		.iDRIFourCC = DRM_FORMAT_ABGR8888,
		.iDRIFormat = __DRI_IMAGE_FORMAT_ABGR8888,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_RGBA,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8G8B8A8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_B8G8R8X8_UNORM,
		.iDRIFourCC = DRM_FORMAT_XRGB8888,
		.iDRIFormat = __DRI_IMAGE_FORMAT_XRGB8888,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_RGB,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_B8G8R8X8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_R8G8B8X8_UNORM,
		.iDRIFourCC = DRM_FORMAT_XBGR8888,
		.iDRIFormat = __DRI_IMAGE_FORMAT_XBGR8888,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_RGB,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8G8B8X8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_B5G6R5_UNORM,
		.iDRIFourCC = DRM_FORMAT_RGB565,
		.iDRIFormat = __DRI_IMAGE_FORMAT_RGB565,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_RGB,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_B5G6R5_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_R8G8_UNORM,
		.iDRIFourCC = DRM_FORMAT_GR88,
		.iDRIFormat = __DRI_IMAGE_FORMAT_GR88,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_RG,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8G8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
		.iDRIFourCC = DRM_FORMAT_R8,
		.iDRIFormat = __DRI_IMAGE_FORMAT_R8,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_R,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
		{
		.eIMGPixelFormat = IMG_PIXFMT_L8A8_UNORM,
		.iDRIFourCC = DRM_FORMAT_GR88,
		.iDRIFormat = __DRI_IMAGE_FORMAT_GR88,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_RG,
		.bQueryDmaBufFormatsExclude = true,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_L8A8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_L8_UNORM,
		.iDRIFourCC = DRM_FORMAT_R8,
		.iDRIFormat = __DRI_IMAGE_FORMAT_R8,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_R,
		.bQueryDmaBufFormatsExclude = true,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_L8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_D32_FLOAT,
		.iDRIFourCC = 0,
		.iDRIFormat = __DRI_IMAGE_FORMAT_NONE,
		.iDRIComponents = 0,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_D32_FLOAT,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_S8_UINT,
		.iDRIFourCC = 0,
		.iDRIFormat = __DRI_IMAGE_FORMAT_NONE,
		.iDRIComponents = 0,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_S8_UINT,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
#if defined(__DRI_IMAGE_FORMAT_ARGB4444)
	/* We patch this format into Mesa */
	{
		.eIMGPixelFormat = IMG_PIXFMT_B4G4R4A4_UNORM,
		.iDRIFourCC = DRM_FORMAT_ARGB4444,
		.iDRIFormat = __DRI_IMAGE_FORMAT_ARGB4444,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_RGBA,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_B4G4R4A4_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
#endif
#if defined(__DRI_IMAGE_FORMAT_ARGB1555)
	/* We patch this format into Mesa */
	{
		.eIMGPixelFormat = IMG_PIXFMT_B5G5R5A1_UNORM,
		.iDRIFourCC = DRM_FORMAT_ARGB1555,
		.iDRIFormat = __DRI_IMAGE_FORMAT_ARGB1555,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_RGBA,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_B5G5R5A1_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
#endif
#if defined(__DRI_IMAGE_COMPONENTS_EXTERNAL)
	{
		.eIMGPixelFormat = IMG_PIXFMT_YUYV,
		.iDRIFourCC = DRM_FORMAT_YUYV,
		.iDRIFormat = __DRI_IMAGE_FORMAT_YUYV,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_EXTERNAL,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
			.eIMGPixelFormat = IMG_PIXFMT_YUYV,
			.uiWidthShift = 0,
			.uiHeightShift = 0
		},
	},
#if defined(DRM_FORMAT_YVU444_PACK10_IMG)
	{
		.eIMGPixelFormat = IMG_PIXFMT_YVU10_444_1PLANE_PACK10,
		.iDRIFourCC = DRM_FORMAT_YVU444_PACK10_IMG,
		.iDRIFormat = __DRI_IMAGE_FORMAT_YVU444_PACK10_IMG,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_EXTERNAL,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
			.eIMGPixelFormat = IMG_PIXFMT_YVU10_444_1PLANE_PACK10,
			.uiWidthShift = 0,
			.uiHeightShift = 0,
		},
	},
#endif
#endif
#if defined(DRM_FORMAT_MT21)
	/* We patch this format into Mesa */
	{
		.eIMGPixelFormat = IMG_PIXFMT_YVU8_420_2PLANE_PACK8_P,
		.iDRIFourCC = DRM_FORMAT_MT21,
		.iDRIFormat = __DRI_IMAGE_FORMAT_NONE,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_Y_UV,
		.uiNumPlanes = 2,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
		.sPlanes[1] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8G8_UNORM,
				.uiWidthShift = 1,
				.uiHeightShift = 1
		},
	},
#endif
	{
		.eIMGPixelFormat = IMG_PIXFMT_YUV420_2PLANE,
		.iDRIFourCC = DRM_FORMAT_NV12,
		.iDRIFormat = __DRI_IMAGE_FORMAT_NONE,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_Y_UV,
		.uiNumPlanes = 2,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
		.sPlanes[1] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8G8_UNORM,
				.uiWidthShift = 1,
				.uiHeightShift = 1
		},
	},
#if defined(DRM_FORMAT_NV21)
	{
		.eIMGPixelFormat = IMG_PIXFMT_YVU420_2PLANE,
		.iDRIFourCC = DRM_FORMAT_NV21,
		.iDRIFormat = __DRI_IMAGE_FORMAT_NONE,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_Y_UV,
		.uiNumPlanes = 2,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
		.sPlanes[1] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8G8_UNORM,
				.uiWidthShift = 1,
				.uiHeightShift = 1
		},
	},
#endif
	{
		.eIMGPixelFormat = IMG_PIXFMT_YUV420_3PLANE,
		.iDRIFourCC = DRM_FORMAT_YUV420,
		.iDRIFormat = __DRI_IMAGE_FORMAT_NONE,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_Y_U_V,
		.uiNumPlanes = 3,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
		.sPlanes[1] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.uiWidthShift = 1,
				.uiHeightShift = 1
		},
		.sPlanes[2] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.uiWidthShift = 1,
				.uiHeightShift = 1
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_YVU420_3PLANE,
		.iDRIFourCC = DRM_FORMAT_YVU420,
		.iDRIFormat = __DRI_IMAGE_FORMAT_NONE,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_Y_U_V,
		.uiNumPlanes = 3,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
		.sPlanes[1] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.uiWidthShift = 1,
				.uiHeightShift = 1
		},
		.sPlanes[2] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.uiWidthShift = 1,
				.uiHeightShift = 1
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_D16_UNORM,
		.iDRIFourCC = 0,
		.iDRIFormat = __DRI_IMAGE_FORMAT_NONE,
		.iDRIComponents = 0,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_D16_UNORM,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
	{
		.eIMGPixelFormat = IMG_PIXFMT_D24_UNORM_X8_TYPELESS,
		.iDRIFourCC = 0,
		.iDRIFormat = __DRI_IMAGE_FORMAT_NONE,
		.iDRIComponents = 0,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_D24_UNORM_X8_TYPELESS,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
};

/*
 * Check if a PVR Screen has support for a particular format based upon its
 * position in g_asFormats. If querying of this information isn't supported
 * by pvr_dri_support then assume the format is supported.
 */
static inline bool
PVRDRIScreenHasFormatFromIdx(const PVRDRIScreen * const psPVRScreen,
			     const unsigned int uiFormatIdx)
{
	if (psPVRScreen->iNumFormats > 0)
	{
		if (uiFormatIdx < ARRAY_SIZE(g_asFormats))
		{
			return psPVRScreen->pbHasFormat[uiFormatIdx];
		}

		return false;
	}

	assert(psPVRScreen->iNumFormats == -1);

	return true;
}

/* Standard error message */
void PRINTFLIKE(1, 2) errorMessage(const char *f, ...)
{
	char message[MESSAGE_LENGTH_MAX];
	va_list args;

	va_start(args, f);
	vsnprintf(message, sizeof message, f, args);
	va_end(args);

	err_printf("%s", message);
}

void PRINTFLIKE(1, 2) __driUtilMessage(const char *f, ...)
{
	char message[MESSAGE_LENGTH_MAX];
	va_list args;

	/*
	 * On Android and Tizen, always print messages; otherwise, only print if
	 * the environment variable LIBGL_DEBUG=verbose
	 */
#if !defined(HAVE_ANDROID_PLATFORM) && !defined(HAVE_TIZEN_PLATFORM)
	char *ev = getenv("LIBGL_DEBUG");

	if (!ev || strcmp(ev, "verbose") != 0)
	{
		return;
	}
#endif

	va_start(args, f);
	vsnprintf(message, sizeof message, f, args);
	va_end(args);

	dbg_printf("%s", message);
}

const __DRIconfig **PVRDRICreateConfigs(void)
{
	static const GLenum asBackBufferModes[]	= { __DRI_ATTRIB_SWAP_NONE, __DRI_ATTRIB_SWAP_UNDEFINED };
	const uint8_t *puDepthBits = PVRDRIDepthBitsArray();
	const uint8_t *puStencilBits = PVRDRIStencilBitsArray();
	const uint8_t *puMSAASamples = PVRDRIMSAABitsArray();
	const unsigned uNumBackBufferModes = ARRAY_SIZE(asBackBufferModes);
	const unsigned uNumDepthStencilBits = PVRDRIDepthStencilBitArraySize();
	const unsigned uNumMSAASamples = PVRDRIMSAABitArraySize();
	__DRIconfig **ppsConfigs = NULL;
	__DRIconfig **ppsNewConfigs;
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(g_asMesaFormats); i++)
	{
		if (!PVRDRIMesaFormatSupported(g_asMesaFormats[i].uPVRDRI))
			continue;

		ppsNewConfigs = driCreateConfigs(g_asMesaFormats[i].eMesa,
						 puDepthBits,
						 puStencilBits,
						 uNumDepthStencilBits, 
						 asBackBufferModes,
						 uNumBackBufferModes, 
						 puMSAASamples,
						 uNumMSAASamples,
						 GL_FALSE,
						 GL_FALSE,
						 GL_FALSE
#if defined(__DRI_ATTRIB_YUV_BIT)
						 , __DRI_ATTRIB_YUV_DEPTH_RANGE_NONE,
						 __DRI_ATTRIB_YUV_CSC_STANDARD_NONE
#endif
						 );

		ppsConfigs = driConcatConfigs(ppsConfigs, ppsNewConfigs);
	}

	if (ppsConfigs)
	{
		for (i = 0; ppsConfigs[i]; i++)
		{
			ppsConfigs[i]->modes.maxPbufferWidth =
						PVRDRIMaxPBufferWidth();
			ppsConfigs[i]->modes.maxPbufferHeight =
						PVRDRIMaxPBufferHeight();

			ppsConfigs[i]->modes.maxPbufferPixels =
						PVRDRIMaxPBufferWidth() *
						PVRDRIMaxPBufferHeight();
		}
	}

	return (const __DRIconfig **)ppsConfigs;
}

const PVRDRIImageFormat *PVRDRIFormatToImageFormat(PVRDRIScreen *psPVRScreen,
						   int iDRIFormat)
{
	unsigned i;

	assert(iDRIFormat != __DRI_IMAGE_FORMAT_NONE);

	for (i = 0; i < ARRAY_SIZE(g_asFormats); i++)
	{
		if (g_asFormats[i].iDRIFormat != iDRIFormat)
		{
			continue;
		}

		if (!PVRDRIScreenHasFormatFromIdx(psPVRScreen, i))
		{
			break;
		}

		return &g_asFormats[i];
	}

	return NULL;
}

const PVRDRIImageFormat *PVRDRIFourCCToImageFormat(PVRDRIScreen *psPVRScreen,
						   int iDRIFourCC)
{
	unsigned i;

	if (!iDRIFourCC)
	{
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(g_asFormats); i++)
	{
		if (g_asFormats[i].iDRIFourCC != iDRIFourCC)
		{
			continue;
		}

		if (!PVRDRIScreenHasFormatFromIdx(psPVRScreen, i))
		{
			break;
		}

		return &g_asFormats[i];
	}

	return NULL;
}

const PVRDRIImageFormat *PVRDRIIMGPixelFormatToImageFormat(PVRDRIScreen *psPVRScreen,
							   IMG_PIXFMT eIMGPixelFormat)
{
	unsigned i;

	assert(eIMGPixelFormat != IMG_PIXFMT_UNKNOWN);

	for (i = 0; i < ARRAY_SIZE(g_asFormats); i++)
	{
		if (g_asFormats[i].eIMGPixelFormat != eIMGPixelFormat)
		{
			continue;
		}

		/*
		 * Assume that the screen has the format, i.e. it's supported by
		 * the HW+SW, since we can only have an IMG_PIXFMT from having
		 * called one of the other PVRDRI*ToImageFormat functions or
		 * one of the pvr_dri_support functions.
		 */
		assert(PVRDRIScreenHasFormatFromIdx(psPVRScreen, i));

		return &g_asFormats[i];
	}

	return NULL;
}

/*
 * The EGL_EXT_image_dma_buf_import says that if a hint is unspecified then
 * the implementation may guess based on the pixel format or may fallback
 * to some default value. Furthermore, if a hint is unsupported then the
 * implementation may use whichever settings it wants to achieve the closest
 * match.
 */
IMG_YUV_COLORSPACE PVRDRIToIMGColourSpace(const PVRDRIImageFormat *psFormat,
					  enum __DRIYUVColorSpace eDRIColourSpace,
					  enum __DRISampleRange eDRISampleRange)
{
	switch (psFormat->iDRIComponents)
	{
		case __DRI_IMAGE_COMPONENTS_R:
		case __DRI_IMAGE_COMPONENTS_RG:
		case __DRI_IMAGE_COMPONENTS_RGB:
		case __DRI_IMAGE_COMPONENTS_RGBA:
			return IMG_COLORSPACE_UNDEFINED;
		case __DRI_IMAGE_COMPONENTS_Y_U_V:
		case __DRI_IMAGE_COMPONENTS_Y_UV:
		case __DRI_IMAGE_COMPONENTS_Y_XUXV:
#if defined(__DRI_IMAGE_COMPONENTS_EXTERNAL)
		case __DRI_IMAGE_COMPONENTS_EXTERNAL:
#endif
			break;
		default:
			errorMessage("Unrecognised DRI components (components = 0x%X)\n",
				     psFormat->iDRIComponents);
			unreachable("unhandled DRI component");
			return IMG_COLORSPACE_UNDEFINED;
	}

	switch (eDRIColourSpace)
	{
		case __DRI_YUV_COLOR_SPACE_UNDEFINED:
		case __DRI_YUV_COLOR_SPACE_ITU_REC601:
			switch (eDRISampleRange)
			{
				case __DRI_YUV_RANGE_UNDEFINED:
				case __DRI_YUV_NARROW_RANGE:
					return IMG_COLORSPACE_BT601_CONFORMANT_RANGE;
				case __DRI_YUV_FULL_RANGE:
					return IMG_COLORSPACE_BT601_FULL_RANGE;
				default:
					errorMessage("Unrecognised DRI sample range (sample range = 0x%X)\n",
						     eDRISampleRange);
					unreachable("unhandled sample range");
					return IMG_COLORSPACE_UNDEFINED;
			}
		case __DRI_YUV_COLOR_SPACE_ITU_REC709:
			switch (eDRISampleRange)
			{
				case __DRI_YUV_RANGE_UNDEFINED:
				case __DRI_YUV_NARROW_RANGE:
					return IMG_COLORSPACE_BT709_CONFORMANT_RANGE;
				case __DRI_YUV_FULL_RANGE:
					return IMG_COLORSPACE_BT709_FULL_RANGE;
				default:
					errorMessage("Unrecognised DRI sample range (sample range = 0x%X)\n",
						     eDRISampleRange);
					unreachable("unhandled sample range");
					return IMG_COLORSPACE_UNDEFINED;
			}
		case __DRI_YUV_COLOR_SPACE_ITU_REC2020:
			switch (eDRISampleRange)
			{
				case __DRI_YUV_RANGE_UNDEFINED:
				case __DRI_YUV_NARROW_RANGE:
					return IMG_COLORSPACE_BT2020_CONFORMANT_RANGE;
				case __DRI_YUV_FULL_RANGE:
					return IMG_COLORSPACE_BT2020_FULL_RANGE;
				default:
					errorMessage("Unrecognised DRI sample range (sample range = 0x%X)\n",
						     eDRISampleRange);
					assert(0);
					return IMG_COLORSPACE_UNDEFINED;
			}
		default:
			errorMessage("Unrecognised DRI colour space (colour space = 0x%X)\n",
				     eDRIColourSpace);
			unreachable("unhandled color space");
			return IMG_COLORSPACE_UNDEFINED;
	}
}

IMG_YUV_CHROMA_INTERP PVRDRIChromaSittingToIMGInterp(const PVRDRIImageFormat *psFormat,
						     enum __DRIChromaSiting eChromaSitting)
{
	switch (psFormat->iDRIComponents)
	{
		case __DRI_IMAGE_COMPONENTS_R:
		case __DRI_IMAGE_COMPONENTS_RG:
		case __DRI_IMAGE_COMPONENTS_RGB:
		case __DRI_IMAGE_COMPONENTS_RGBA:
			return IMG_CHROMA_INTERP_UNDEFINED;
		case __DRI_IMAGE_COMPONENTS_Y_U_V:
		case __DRI_IMAGE_COMPONENTS_Y_UV:
		case __DRI_IMAGE_COMPONENTS_Y_XUXV:
#if defined(__DRI_IMAGE_COMPONENTS_EXTERNAL)
		case __DRI_IMAGE_COMPONENTS_EXTERNAL:
#endif
			break;
		default:
			errorMessage("Unrecognised DRI components (components = 0x%X)\n",
				     psFormat->iDRIComponents);
			unreachable("unhandled dri component");
			return IMG_CHROMA_INTERP_UNDEFINED;
	}

	switch (eChromaSitting)
	{
		case __DRI_YUV_CHROMA_SITING_UNDEFINED:
		case __DRI_YUV_CHROMA_SITING_0:
			return IMG_CHROMA_INTERP_ZERO;
		case __DRI_YUV_CHROMA_SITING_0_5:
			return IMG_CHROMA_INTERP_HALF;
		default:
			errorMessage("Unrecognised DRI chroma sitting (chroma sitting = 0x%X)\n",
				     eChromaSitting);
			unreachable("unhandled chroma sitting");
			return IMG_CHROMA_INTERP_UNDEFINED;
	}
}

bool PVRDRIGetSupportedFormats(PVRDRIScreen *psPVRScreen)
{
	int *piFormats;
	IMG_PIXFMT *peImgFormats;
	bool bRet = false;
	unsigned i;

	piFormats = malloc(ARRAY_SIZE(g_asFormats) * sizeof(*piFormats));
	peImgFormats = malloc(ARRAY_SIZE(g_asFormats) * sizeof(*peImgFormats));

	psPVRScreen->pbHasFormat = malloc(ARRAY_SIZE(g_asFormats) *
					  sizeof(*psPVRScreen->pbHasFormat));

	psPVRScreen->psModifiers = calloc(ARRAY_SIZE(g_asFormats),
					   sizeof(*psPVRScreen->psModifiers));

	if (!piFormats || !peImgFormats ||
	    !psPVRScreen->pbHasFormat || !psPVRScreen->psModifiers)
	{
		errorMessage("Out of memory\n");

		goto err_free;
	}

	for (i = 0; i < ARRAY_SIZE(g_asFormats); i++)
	{
		piFormats[i] = g_asFormats[i].bQueryDmaBufFormatsExclude ?
			       0 : g_asFormats[i].iDRIFourCC;

		peImgFormats[i] = g_asFormats[i].eIMGPixelFormat;

		psPVRScreen->psModifiers[i].iNumModifiers = -1;
	}

	psPVRScreen->iNumFormats =
			PVRDRIQuerySupportedFormats(psPVRScreen->psImpl,
						    ARRAY_SIZE(g_asFormats),
						    piFormats,
						    peImgFormats,
						    psPVRScreen->pbHasFormat);
	if (psPVRScreen->iNumFormats == 0)
	{
		__driUtilMessage("Couldn't query supported pixel formats\n");
		goto err_free;
	}

	bRet = true;
	goto cleanup;

err_free:
	free(psPVRScreen->psModifiers);
	psPVRScreen->psModifiers = NULL;

	free(psPVRScreen->pbHasFormat);
	psPVRScreen->pbHasFormat = NULL;
cleanup:
	free(peImgFormats);
	free(piFormats);
	return bRet;
}

GLboolean PVRDRIQueryDmaBufFormats(__DRIscreen *screen, int max,
				   int *formats, int *count)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(screen);
	int i, j;

	assert(psPVRScreen->iNumFormats != 0);

	if (psPVRScreen->iNumFormats < 0)
	{
		return GL_FALSE;
	}

	if (!max)
	{
		*count = psPVRScreen->iNumFormats;
		return GL_TRUE;
	}

	for (i = 0, j = 0; i < ARRAY_SIZE(g_asFormats) && j < max; i++)
	{
		if (psPVRScreen->pbHasFormat[i])
		{
			formats[j++] = g_asFormats[i].iDRIFourCC;
		}
	}

	*count = j;

	return GL_TRUE;
}

static bool PVRDRIGetSupportedModifiers(PVRDRIScreen *psPVRScreen,
					struct PVRDRIModifiers *psModifiers,
					const PVRDRIImageFormat *psFormat)
{
	int iNumModifiers;

	iNumModifiers = PVRDRIQueryModifiers(psPVRScreen->psImpl,
					     psFormat->iDRIFourCC,
					     psFormat->eIMGPixelFormat,
					     NULL, NULL);
	if (iNumModifiers < 0)
	{
		errorMessage("Couldn't query modifiers for format 0x%x\n",
			     psFormat->iDRIFourCC);
		return false;
	}

	psModifiers->puModifiers = malloc(iNumModifiers *
					  sizeof(*psModifiers->puModifiers));
	psModifiers->puExternalOnly = malloc(iNumModifiers *
					  sizeof(*psModifiers->puExternalOnly));
	if (!psModifiers->puModifiers || !psModifiers->puExternalOnly)
	{
		free(psModifiers->puModifiers);
		psModifiers->puModifiers = NULL;

		free(psModifiers->puExternalOnly);
		psModifiers->puExternalOnly = NULL;

		errorMessage("Out of memory\n");

		return false;
	}
	psModifiers->iNumModifiers = iNumModifiers;

	iNumModifiers = PVRDRIQueryModifiers(psPVRScreen->psImpl,
					    psFormat->iDRIFourCC,
					    psFormat->eIMGPixelFormat,
					    psModifiers->puModifiers,
					    psModifiers->puExternalOnly);

	assert(iNumModifiers == psModifiers->iNumModifiers);

	return true;
}

static bool PVRDRIGetModifiersForFormat(PVRDRIScreen *psPVRScreen,
					int fourcc,
					const PVRDRIImageFormat **ppsFormat,
					const struct PVRDRIModifiers **ppsModifiers)
{
	const PVRDRIImageFormat *psFormat;
	struct PVRDRIModifiers *psModifiers;
	unsigned uIdx;

	assert(psPVRScreen->iNumFormats != 0);

	if (psPVRScreen->iNumFormats < 0)
	{
		return false;
	}

	psFormat = PVRDRIFourCCToImageFormat(psPVRScreen, fourcc);
	if (!psFormat)
	{
		return false;
	}

	uIdx = psFormat - g_asFormats;
	psModifiers = &psPVRScreen->psModifiers[uIdx];

	if (psModifiers->iNumModifiers < 0)
	{
		if (!PVRDRIGetSupportedModifiers(psPVRScreen,
						 psModifiers,
						 psFormat))
		{
			return false;
		}
	}

	*ppsFormat = psFormat;
	*ppsModifiers = psModifiers;

	return true;
}

bool PVRDRIValidateImageModifier(PVRDRIScreen *psPVRScreen, const int iFourcc,
				 const uint64_t uiModifier)
{
	const PVRDRIImageFormat *psFormat;
	const struct PVRDRIModifiers *psModifiers;

	if (!PVRDRIGetModifiersForFormat(psPVRScreen, iFourcc, &psFormat,
					 &psModifiers))
	{
		return false;
	}

	for (unsigned i = 0; i < psModifiers->iNumModifiers; i++)
	{
		if (psModifiers->puModifiers[i] == uiModifier)
		{
			return true;
		}
	}

	return false;
}

GLboolean PVRDRIQueryDmaBufModifiers(__DRIscreen *screen, int fourcc,
				     int max, uint64_t *modifiers,
				     unsigned int *external_only,
				     int *count)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(screen);
	const PVRDRIImageFormat *psFormat;
	const struct PVRDRIModifiers *psModifiers;
	int num_copy;

	if (!PVRDRIGetModifiersForFormat(psPVRScreen,
					fourcc,
					&psFormat,
					&psModifiers))
	{
		return GL_FALSE;
	}

	if (!max)
	{
		*count = psModifiers->iNumModifiers;
		return GL_TRUE;
	}

	num_copy = (max < psModifiers->iNumModifiers) ?
			max : psModifiers->iNumModifiers;

	if (modifiers)
	{
		(void) memcpy(modifiers,
			      psModifiers->puModifiers,
			      sizeof(*modifiers) * num_copy);
	}

	if (external_only)
	{
		(void) memcpy(external_only,
			      psModifiers->puExternalOnly,
			      sizeof(*external_only) * num_copy);
	}

	*count = num_copy;

	return GL_TRUE;
}

GLboolean PVRDRIQueryDmaBufFormatModifierAttribs(__DRIscreen *screen,
						 uint32_t fourcc,
						 uint64_t modifier,
						 int attrib,
						 uint64_t *value)
{
	PVRDRIScreen *psPVRScreen = DRIScreenPrivate(screen);
	const PVRDRIImageFormat *psFormat;
	const struct PVRDRIModifiers *psModifiers;
	int i;

	if (!PVRDRIGetModifiersForFormat(psPVRScreen,
					fourcc,
					&psFormat,
					&psModifiers))
	{
		return GL_FALSE;
	}

	for (i = 0; i < psModifiers->iNumModifiers; i++)
	{
		if (psModifiers->puModifiers[i] == modifier)
		{
			break;
		}
	}
	if (i == psModifiers->iNumModifiers)
	{
		return GL_FALSE;
	}

	switch (attrib)
	{
		case __DRI_IMAGE_FORMAT_MODIFIER_ATTRIB_PLANE_COUNT:
			*value = psFormat->uiNumPlanes;
			break;
		default:
			return GL_FALSE;
	}

	return GL_TRUE;
}

void PVRDRIDestroyFormatInfo(PVRDRIScreen *psPVRScreen)
{
	unsigned i;

	if (psPVRScreen->psModifiers)
	{
		for (i = 0; i < ARRAY_SIZE(g_asFormats); i++)
		{
			free(psPVRScreen->psModifiers[i].puModifiers);
			free(psPVRScreen->psModifiers[i].puExternalOnly);
		}
		free(psPVRScreen->psModifiers);
	}

	free(psPVRScreen->pbHasFormat);
}
