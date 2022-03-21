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

#include "dri_screen.h"
#include "pvrdri.h"

int
MODSUPGetBuffers(__DRIdrawable *psDRIDrawable, unsigned int uFourCC,
                 uint32_t *puStamp, void *pvLoaderPrivate,
                 uint32_t uBufferMask, struct PVRDRIImageList *psImageList)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;
   __DRIscreen *psDRIScreen = psDRIDrawable->driScreenPriv;
   struct __DRIimageList sDRIList;
   int res;

#if !defined(DRI_IMAGE_HAS_BUFFER_PREV)
   uBufferMask &= ~PVRDRI_IMAGE_BUFFER_PREV;
#endif

   if (psPVRDrawable->uFourCC != uFourCC) {
      psPVRDrawable->uDRIFormat = PVRDRIFourCCToDRIFormat(uFourCC);
      psPVRDrawable->uFourCC = uFourCC;
   }

   res = psDRIScreen->image.loader->getBuffers(psDRIDrawable,
                                               psPVRDrawable->uDRIFormat,
                                               puStamp,
                                               pvLoaderPrivate,
                                               uBufferMask, &sDRIList);

   if (res) {
      psImageList->uImageMask = sDRIList.image_mask;
      psImageList->psBack = sDRIList.back;
      psImageList->psFront = sDRIList.front;
      psImageList->psPrev = sDRIList.prev;
   }

   return res;
}

bool
MODSUPCreateConfigs(__DRIconfig ***pppsConfigs, __DRIscreen *psDRIScreen,
                    int iPVRDRIMesaFormat, const uint8_t *puDepthBits,
                    const uint8_t *puStencilBits,
                    unsigned int uNumDepthStencilBits,
                    const unsigned int *puDBModes, unsigned int uNumDBModes,
                    const uint8_t *puMSAASamples, unsigned int uNumMSAAModes,
                    bool bEnableAccum, bool bColorDepthMatch,
                    UNUSED bool bMutableRenderBuffer,
                    int iYUVDepthRange, int iYUVCSCStandard,
                    uint32_t uMaxPbufferWidth, uint32_t uMaxPbufferHeight)
{
   __DRIconfig **ppsConfigs;
   mesa_format eFormat = PVRDRIMesaFormatToMesaFormat(iPVRDRIMesaFormat);
   unsigned int i;

   (void) psDRIScreen;

   switch (eFormat) {
   case MESA_FORMAT_NONE:
      __driUtilMessage("%s: Unknown PVR DRI format: %u",
                       __func__, iPVRDRIMesaFormat);
      return false;
   default:
      break;
   }

   /*
    * The double buffered modes array argument for driCreateConfigs has
    * entries of type GLenum.
    */
   static_assert(sizeof(GLenum) == sizeof(unsigned int),
                 "Size mismatch between GLenum and unsigned int");

   ppsConfigs = driCreateConfigs(eFormat, puDepthBits, puStencilBits,
                                 uNumDepthStencilBits, (GLenum *) puDBModes,
                                 uNumDBModes, puMSAASamples, uNumMSAAModes,
                                 bEnableAccum, bColorDepthMatch,
                                 iYUVDepthRange, iYUVCSCStandard);
   if (!ppsConfigs)
      return false;

   for (i = 0; ppsConfigs[i]; i++) {
      ppsConfigs[i]->modes.maxPbufferWidth = uMaxPbufferWidth;
      ppsConfigs[i]->modes.maxPbufferHeight = uMaxPbufferHeight;
      ppsConfigs[i]->modes.maxPbufferPixels =
         uMaxPbufferWidth * uMaxPbufferHeight;
   }

   *pppsConfigs = ppsConfigs;

   return true;
}

__DRIconfig **
MODSUPConcatConfigs(__DRIscreen *psDRIScreen,
                    __DRIconfig **ppsConfigA, __DRIconfig **ppsConfigB)
{
   (void) psDRIScreen;

   return driConcatConfigs(ppsConfigA, ppsConfigB);
}

struct __DRIimageRec *
MODSUPLookupEGLImage(__DRIscreen *psDRIScreen, void *pvImage,
                     void *pvLoaderPrivate)
{
   return psDRIScreen->dri2.image->lookupEGLImage(psDRIScreen,
                                                  pvImage,
                                                  pvLoaderPrivate);
}


unsigned int
MODSUPGetCapability(__DRIscreen *psDRIScreen, unsigned int uCapability)
{
   if (psDRIScreen->image.loader->base.version >= 2 &&
       psDRIScreen->image.loader->getCapability) {
      enum dri_loader_cap eCapability =
         (enum dri_loader_cap) uCapability;

      return psDRIScreen->image.loader->getCapability(
         psDRIScreen->loaderPrivate,
         eCapability);
   }

   return 0;
}

int
MODSUPGetDisplayFD(__DRIscreen *psDRIScreen, void *pvLoaderPrivate)
{
#if __DRI_IMAGE_LOADER_VERSION >= 5
   if (psDRIScreen->image.loader->base.version >= 5 &&
       psDRIScreen->image.loader->getDisplayFD)
      return psDRIScreen->image.loader->getDisplayFD(pvLoaderPrivate);
#else
   (void) psDRIScreen;
   (void) pvLoaderPrivate;
#endif

   return -1;
}

static bool
PVRDRIConfigQueryUnsigned(const PVRDRIConfig *psConfig,
                          PVRDRIConfigAttrib eConfigAttrib,
                          unsigned int *puValueOut)
{
   if (!psConfig || !puValueOut)
      return false;

   switch (eConfigAttrib) {
   case PVRDRI_CONFIG_ATTRIB_RENDERABLE_TYPE:
      *puValueOut = psConfig->iSupportedAPIs;
      return true;
   case PVRDRI_CONFIG_ATTRIB_RGB_MODE:
      *puValueOut = psConfig->sGLMode.rgbMode;
      return true;
   case PVRDRI_CONFIG_ATTRIB_DOUBLE_BUFFER_MODE:
      *puValueOut = psConfig->sGLMode.doubleBufferMode;
      return true;
   case PVRDRI_CONFIG_ATTRIB_RED_BITS:
      *puValueOut = psConfig->sGLMode.redBits;
      return true;
   case PVRDRI_CONFIG_ATTRIB_GREEN_BITS:
      *puValueOut = psConfig->sGLMode.greenBits;
      return true;
   case PVRDRI_CONFIG_ATTRIB_BLUE_BITS:
      *puValueOut = psConfig->sGLMode.blueBits;
      return true;
   case PVRDRI_CONFIG_ATTRIB_ALPHA_BITS:
      *puValueOut = psConfig->sGLMode.alphaBits;
      return true;
   case PVRDRI_CONFIG_ATTRIB_RGB_BITS:
      *puValueOut = psConfig->sGLMode.rgbBits;
      return true;
   case PVRDRI_CONFIG_ATTRIB_DEPTH_BITS:
      *puValueOut = psConfig->sGLMode.depthBits;
      return true;
   case PVRDRI_CONFIG_ATTRIB_STENCIL_BITS:
      *puValueOut = psConfig->sGLMode.stencilBits;
      return true;
   case PVRDRI_CONFIG_ATTRIB_SAMPLE_BUFFERS:
      *puValueOut = !!psConfig->sGLMode.samples;
      return true;
   case PVRDRI_CONFIG_ATTRIB_SAMPLES:
      *puValueOut = psConfig->sGLMode.samples;
      return true;
   case PVRDRI_CONFIG_ATTRIB_BIND_TO_TEXTURE_RGB:
      *puValueOut = GL_TRUE;
      return true;
   case PVRDRI_CONFIG_ATTRIB_BIND_TO_TEXTURE_RGBA:
      *puValueOut = GL_TRUE;
      return true;
#if defined(__DRI_ATTRIB_YUV_BIT)
   case PVRDRI_CONFIG_ATTRIB_YUV_ORDER:
      *puValueOut = psConfig->sGLMode.YUVOrder;
      return true;
   case PVRDRI_CONFIG_ATTRIB_YUV_NUM_OF_PLANES:
      *puValueOut = psConfig->sGLMode.YUVNumberOfPlanes;
      return true;
   case PVRDRI_CONFIG_ATTRIB_YUV_SUBSAMPLE:
      *puValueOut = psConfig->sGLMode.YUVSubsample;
      return true;
   case PVRDRI_CONFIG_ATTRIB_YUV_DEPTH_RANGE:
      *puValueOut = psConfig->sGLMode.YUVDepthRange;
      return true;
   case PVRDRI_CONFIG_ATTRIB_YUV_CSC_STANDARD:
      *puValueOut = psConfig->sGLMode.YUVCSCStandard;
      return true;
   case PVRDRI_CONFIG_ATTRIB_YUV_PLANE_BPP:
      *puValueOut = psConfig->sGLMode.YUVPlaneBPP;
      return true;
#endif
#if !defined(__DRI_ATTRIB_YUV_BIT)
   case PVRDRI_CONFIG_ATTRIB_YUV_ORDER:
   case PVRDRI_CONFIG_ATTRIB_YUV_NUM_OF_PLANES:
   case PVRDRI_CONFIG_ATTRIB_YUV_SUBSAMPLE:
   case PVRDRI_CONFIG_ATTRIB_YUV_DEPTH_RANGE:
   case PVRDRI_CONFIG_ATTRIB_YUV_CSC_STANDARD:
   case PVRDRI_CONFIG_ATTRIB_YUV_PLANE_BPP:
      return false;
#endif
   case PVRDRI_CONFIG_ATTRIB_RED_MASK:
      *puValueOut = psConfig->sGLMode.redMask;
      return true;
   case PVRDRI_CONFIG_ATTRIB_GREEN_MASK:
      *puValueOut = psConfig->sGLMode.greenMask;
      return true;
   case PVRDRI_CONFIG_ATTRIB_BLUE_MASK:
      *puValueOut = psConfig->sGLMode.blueMask;
      return true;
   case PVRDRI_CONFIG_ATTRIB_ALPHA_MASK:
      *puValueOut = psConfig->sGLMode.alphaMask;
      return true;
   case PVRDRI_CONFIG_ATTRIB_SRGB_CAPABLE:
      *puValueOut = psConfig->sGLMode.sRGBCapable;
      return true;
   case PVRDRI_CONFIG_ATTRIB_INVALID:
      errorMessage("%s: Invalid attribute", __func__);
      assert(0);
      return false;
   default:
      return false;
   }
}

bool
PVRDRIConfigQuery(const PVRDRIConfig *psConfig,
                  PVRDRIConfigAttrib eConfigAttrib, int *piValueOut)
{
   bool bRes;
   unsigned int uValue;

   bRes = PVRDRIConfigQueryUnsigned(psConfig, eConfigAttrib, &uValue);
   if (bRes)
      *piValueOut = (int) uValue;

   return bRes;
}

bool
MODSUPConfigQuery(const PVRDRIConfig *psConfig,
                  PVRDRIConfigAttrib eConfigAttrib, unsigned int *puValueOut)
{
   return PVRDRIConfigQueryUnsigned(psConfig, eConfigAttrib, puValueOut);
}

void
MODSUPFlushFrontBuffer(struct __DRIdrawableRec *psDRIDrawable,
                       void *pvLoaderPrivate)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;
   __DRIscreen *psDRIScreen = psDRIDrawable->driScreenPriv;

   if (!psPVRDrawable->sConfig.sGLMode.doubleBufferMode)
      psDRIScreen->image.loader->flushFrontBuffer(psDRIDrawable,
                                                  pvLoaderPrivate);
}

void *
MODSUPDrawableGetReferenceHandle(struct __DRIdrawableRec *psDRIDrawable)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;

   return psPVRDrawable;
}

void
MODSUPDrawableAddReference(void *pvReferenceHandle)
{
   PVRDRIDrawable *psPVRDrawable = pvReferenceHandle;

   PVRDRIDrawableAddReference(psPVRDrawable);
}

void
MODSUPDrawableRemoveReference(void *pvReferenceHandle)
{
   PVRDRIDrawable *psPVRDrawable = pvReferenceHandle;

   PVRDRIDrawableRemoveReference(psPVRDrawable);
}

void
MODSUPDestroyLoaderImageState(const struct __DRIscreenRec *psDRIScreen,
                              void *pvLoaderPrivate)
{
   const __DRIimageLoaderExtension *psImageLoader = psDRIScreen->image.loader;
   const __DRIdri2LoaderExtension *psDRI2Loader = psDRIScreen->dri2.loader;

   if (psImageLoader && psImageLoader->base.version >= 4 &&
       psImageLoader->destroyLoaderImageState) {
      psImageLoader->destroyLoaderImageState(pvLoaderPrivate);
   } else if (psDRI2Loader && psDRI2Loader->base.version >= 5 &&
       psDRI2Loader->destroyLoaderImageState) {
      psDRI2Loader->destroyLoaderImageState(pvLoaderPrivate);
   }
}
