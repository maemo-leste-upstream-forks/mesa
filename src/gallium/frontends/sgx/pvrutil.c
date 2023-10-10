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

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include "util/log.h"
#include "GL/gl.h"

#include "dri_screen.h"
#include "pvrdri.h"

#define	PVRDRIMesaFormatEntry(f) {f, PVRDRI_ ## f }

static const struct
{
	mesa_format eMesa;
	unsigned uPVRDRI;
} g_asMesaFormats[] = {
	PVRDRIMesaFormatEntry(MESA_FORMAT_B8G8R8A8_UNORM),
	PVRDRIMesaFormatEntry(MESA_FORMAT_B8G8R8X8_UNORM),
	PVRDRIMesaFormatEntry(MESA_FORMAT_R8G8B8A8_UNORM),
	PVRDRIMesaFormatEntry(MESA_FORMAT_R8G8B8X8_UNORM),
	PVRDRIMesaFormatEntry(MESA_FORMAT_B5G6R5_UNORM),
};

static const PVRDRIImageFormat g_asFormats[] =
{
	{
		.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
		.iDRIFourCC = DRM_FORMAT_R8,
		.iDRIFormat = __DRI_IMAGE_FORMAT_R8,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_R,
		.uiNumPlanes = 1,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.iDRIFormat = __DRI_IMAGE_FORMAT_R8,
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
				.iDRIFormat = __DRI_IMAGE_FORMAT_ARGB8888,
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
				.iDRIFormat = __DRI_IMAGE_FORMAT_ABGR8888,
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
				.iDRIFormat = __DRI_IMAGE_FORMAT_XRGB8888,
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
				.iDRIFormat = __DRI_IMAGE_FORMAT_XBGR8888,
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
				.iDRIFormat = __DRI_IMAGE_FORMAT_RGB565,
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
				.iDRIFormat = __DRI_IMAGE_FORMAT_ARGB4444,
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
				.iDRIFormat = __DRI_IMAGE_FORMAT_ARGB1555,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
	},
#endif
#if defined(__DRI_IMAGE_FOURCC_MT21)
	/* We patch this format into Mesa */
	{
		.eIMGPixelFormat = IMG_PIXFMT_YVU8_420_2PLANE_PACK8_P,
		.iDRIFourCC = __DRI_IMAGE_FOURCC_MT21,
		.iDRIFormat = __DRI_IMAGE_FORMAT_NONE,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_Y_UV,
		.uiNumPlanes = 2,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.iDRIFormat = __DRI_IMAGE_FORMAT_R8,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
		.sPlanes[1] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8G8_UNORM,
				.iDRIFormat = __DRI_IMAGE_FORMAT_GR88,
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
				.iDRIFormat = __DRI_IMAGE_FORMAT_R8,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
		.sPlanes[1] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8G8_UNORM,
				.iDRIFormat = __DRI_IMAGE_FORMAT_GR88,
				.uiWidthShift = 1,
				.uiHeightShift = 1
		},
	},
#if defined(__DRI_IMAGE_FOURCC_YVU420)
	{
		.eIMGPixelFormat = IMG_PIXFMT_YVU420_3PLANE,
		.iDRIFourCC = __DRI_IMAGE_FOURCC_YVU420,
		.iDRIFormat = __DRI_IMAGE_FORMAT_NONE,
		.iDRIComponents = __DRI_IMAGE_COMPONENTS_Y_U_V,
		.uiNumPlanes = 3,
		.sPlanes[0] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.iDRIFormat = __DRI_IMAGE_FORMAT_R8,
				.uiWidthShift = 0,
				.uiHeightShift = 0
		},
		.sPlanes[1] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.iDRIFormat = __DRI_IMAGE_FORMAT_R8,
				.uiWidthShift = 1,
				.uiHeightShift = 1
		},
		.sPlanes[2] =
		{
				.eIMGPixelFormat = IMG_PIXFMT_R8_UNORM,
				.iDRIFormat = __DRI_IMAGE_FORMAT_R8,
				.uiWidthShift = 1,
				.uiHeightShift = 1
		},
	},
#endif
};

const __DRIconfig **
PVRDRICreateConfigs(void)
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
						 0,
						 0);

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

const PVRDRIImageFormat *PVRDRIFormatToImageFormat(int iDRIFormat)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(g_asFormats); i++)
	{
		if (g_asFormats[i].iDRIFormat == iDRIFormat)
		{
			return &g_asFormats[i];
		}
	}

	return NULL;
}

const PVRDRIImageFormat *PVRDRIFourCCToImageFormat(int iDRIFourCC)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(g_asFormats); i++)
	{
		if (g_asFormats[i].iDRIFourCC == iDRIFourCC)
		{
			return &g_asFormats[i];
		}
	}

	return NULL;
}

const PVRDRIImageFormat *PVRDRIIMGPixelFormatToImageFormat(IMG_PIXFMT eIMGPixelFormat)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(g_asFormats); i++)
	{
		if (g_asFormats[i].eIMGPixelFormat == eIMGPixelFormat)
		{
			return &g_asFormats[i];
		}
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
		case __DRI_IMAGE_COMPONENTS_RGB:
		case __DRI_IMAGE_COMPONENTS_RGBA:
			return IMG_COLORSPACE_UNDEFINED;
		case __DRI_IMAGE_COMPONENTS_Y_U_V:
		case __DRI_IMAGE_COMPONENTS_Y_UV:
		case __DRI_IMAGE_COMPONENTS_Y_XUXV:
			break;
		default:
		   mesa_loge("Unrecognised DRI components (components = 0x%X)\n",
				     psFormat->iDRIComponents);
			assert(0);
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
				   mesa_loge("Unrecognised DRI sample range (sample range = 0x%X)\n",
						     eDRISampleRange);
					assert(0);
					return IMG_COLORSPACE_UNDEFINED;
			}
		case __DRI_YUV_COLOR_SPACE_ITU_REC709:
		case __DRI_YUV_COLOR_SPACE_ITU_REC2020:
			switch (eDRISampleRange)
			{
				case __DRI_YUV_RANGE_UNDEFINED:
				case __DRI_YUV_NARROW_RANGE:
					return IMG_COLORSPACE_BT709_CONFORMANT_RANGE;
				case __DRI_YUV_FULL_RANGE:
					return IMG_COLORSPACE_BT709_FULL_RANGE;
				default:
				   mesa_loge("Unrecognised DRI sample range (sample range = 0x%X)\n",
						     eDRISampleRange);
					assert(0);
					return IMG_COLORSPACE_UNDEFINED;
			}
		default:
		   mesa_loge("Unrecognised DRI colour space (colour space = 0x%X)\n",
				     eDRIColourSpace);
			assert(0);
			return IMG_COLORSPACE_UNDEFINED;
	}
}

IMG_YUV_CHROMA_INTERP PVRDRIChromaSittingToIMGInterp(const PVRDRIImageFormat *psFormat,
						     enum __DRIChromaSiting eChromaSitting)
{
	switch (psFormat->iDRIComponents)
	{
		case __DRI_IMAGE_COMPONENTS_RGB:
		case __DRI_IMAGE_COMPONENTS_RGBA:
			return IMG_CHROMA_INTERP_UNDEFINED;
		case __DRI_IMAGE_COMPONENTS_Y_U_V:
		case __DRI_IMAGE_COMPONENTS_Y_UV:
		case __DRI_IMAGE_COMPONENTS_Y_XUXV:
			break;
		default:
		   mesa_loge("Unrecognised DRI components (components = 0x%X)\n",
				     psFormat->iDRIComponents);
			assert(0);
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
		   mesa_loge("Unrecognised DRI chroma sitting (chroma sitting = 0x%X)\n",
				     eChromaSitting);
			assert(0);
			return IMG_CHROMA_INTERP_UNDEFINED;
	}
}
