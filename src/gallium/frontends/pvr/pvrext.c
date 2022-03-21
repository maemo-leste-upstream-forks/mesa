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

/*
 * EXTENSION SUPPORT
 *
 * As the driver supports a range of Mesa versions it can be the case that it
 * needs to support different extensions and extension versions depending on
 * the version of Mesa that it's built against. As a guide the following rules
 * should be followed:
 *
 * 1) If an extension appears in some supported versions of Mesa but not others
 *    then it should be protected by the extension define, e.g.:
 *    #if defined(__DRI_IMAGE)
 *    <code>
 *    #endif
 *
 *    However, if it appears in all versions then there's no need for it to
 *    be protected.
 *
 * 2) Each driver supported extension should have a define for the maximum
 *    version supported by the driver. This should be used when initialising
 *    the corresponding extension structure. The Mesa extension version define
 *    should *NOT* be used.
 *
 * 3) If the driver supports a range of versions for a given extension then
 *    it should protect the extension code based on the Mesa extension version
 *    define. For example, if the driver has to support versions 7 to 8 of the
 *    __DRI_IMAGE extension then any fields, in the __DRIimageExtension
 *    structure, that appear in version 8 but not 7 should be protected as
 *    follows:
 *    #if (__DRI_IMAGE_VERSION >= 8)
 *    .createImageFromDmaBufs = PVRDRICreateImageFromDmaBufs,
 *    #endif
 *
 *    Obviously any other associated code should also be protected in the same
 *    way.
 */

#include "dri_util.h"
#include "dri_query_renderer.h"

#include "dri_support.h"
#include "pvrdri.h"

#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "EGL/eglmesaext.h"

/* Maximum version numbers for each supported extension */
#define PVR_DRI_TEX_BUFFER_VERSION      3
#define PVR_DRI2_FLUSH_VERSION          4
#define PVR_DRI_IMAGE_VERSION           21
#define PVR_DRI2_ROBUSTNESS_VERSION     1
#define PVR_DRI2_FENCE_VERSION          2
#define PVR_DRI2_RENDERER_QUERY_VERSION 1
#define PVR_DRI2_BUFFER_DAMAGE_VERSION  1

static void
PVRDRIExtSetTexBuffer(__DRIcontext *psDRIContext, GLint iTarget,
                      GLint iFormat, __DRIdrawable *psDRIDrawable)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   DRISUPSetTexBuffer2(psPVRContext->psDRISUPContext,
                       iTarget, iFormat, psPVRDrawable->psDRISUPDrawable);
}

static void
PVRDRIExtReleaseTexBuffer(__DRIcontext *psDRIContext, GLint iTarget,
                          __DRIdrawable *psDRIDrawable)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   DRISUPReleaseTexBuffer(psPVRContext->psDRISUPContext,
                          iTarget, psPVRDrawable->psDRISUPDrawable);

}

static __DRItexBufferExtension pvrDRITexBufferExtension = {
   .base = {
      .name = __DRI_TEX_BUFFER,
      .version = PVR_DRI_TEX_BUFFER_VERSION,
   },
   .setTexBuffer = NULL,
   .setTexBuffer2 = PVRDRIExtSetTexBuffer,
   .releaseTexBuffer = PVRDRIExtReleaseTexBuffer,
};

static void
PVRDRI2Flush(__DRIdrawable *psDRIDrawable)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;

   DRISUPFlush(psPVRDrawable->psDRISUPDrawable);
}

static void
PVRDRI2Invalidate(__DRIdrawable *psDRIDrawable)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;

   DRISUPInvalidate(psPVRDrawable->psDRISUPDrawable);
}

static void
PVRDRI2FlushWithFlags(__DRIcontext *psDRIContext,
                      __DRIdrawable *psDRIDrawable,
                      unsigned int uFlags,
                      enum __DRI2throttleReason eThrottleReason)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;
   struct DRISUPDrawable *psDRISUPDrawable;

   if ((uFlags & __DRI2_FLUSH_DRAWABLE) != 0) {
      PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;

      psDRISUPDrawable = psPVRDrawable->psDRISUPDrawable;
   } else {
      psDRISUPDrawable = NULL;
   }

   DRISUPFlushWithFlags(psPVRContext->psDRISUPContext, psDRISUPDrawable,
                        uFlags, (unsigned int) eThrottleReason);
}

static __DRI2flushExtension pvrDRI2FlushExtension = {
   .base = {
      .name = __DRI2_FLUSH,
      .version = PVR_DRI2_FLUSH_VERSION,
   },
   .flush = PVRDRI2Flush,
   .invalidate = PVRDRI2Invalidate,
   .flush_with_flags = PVRDRI2FlushWithFlags,
};

static __DRIimage *
PVRDRICreateImageFromName(__DRIscreen *psDRIScreen, int iWidth, int iHeight,
                          int iFormat, int iName, int iPitch,
                          void *pvLoaderPrivate)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;
   int iFourCC = PVRDRIFormatToFourCC(iFormat);

   return DRISUPCreateImageFromName(psPVRScreen->psDRISUPScreen,
                                    iWidth, iHeight, iFourCC, iName, iPitch,
                                    pvLoaderPrivate);
}

static __DRIimage *
PVRDRICreateImageFromRenderbuffer(__DRIcontext *psDRIContext,
                                  int iRenderBuffer, void *pvLoaderPrivate)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   return DRISUPCreateImageFromRenderbuffer(psPVRContext->psDRISUPContext,
                                            iRenderBuffer, pvLoaderPrivate);
}

static void
PVRDRIDestroyImage(__DRIimage *psImage)
{
   DRISUPDestroyImage(psImage);
}

static __DRIimage *
PVRDRICreateImage(__DRIscreen *psDRIScreen, int iWidth, int iHeight,
                  int iFormat, unsigned int uUse, void *pvLoaderPrivate)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;
   int iFourCC = PVRDRIFormatToFourCC(iFormat);

   return DRISUPCreateImage(psPVRScreen->psDRISUPScreen, iWidth, iHeight,
                            iFourCC, uUse, pvLoaderPrivate);
}

static GLboolean
PVRDRIQueryImage(__DRIimage *psImage, int iAttrib, int *piValue)
{
   int iFourCC;

   switch (iAttrib) {
   case __DRI_IMAGE_ATTRIB_FORMAT:
      if (DRISUPQueryImage(psImage,
                           __DRI_IMAGE_ATTRIB_FOURCC, &iFourCC)) {
         *piValue = PVRDRIFourCCToDRIFormat(iFourCC);
         return GL_TRUE;
      }
      return GL_FALSE;
   default:
      return DRISUPQueryImage(psImage, iAttrib, piValue);
   }

}

static __DRIimage *
PVRDRIDupImage(__DRIimage *psImage, void *pvLoaderPrivate)
{
   return DRISUPDupImage(psImage, pvLoaderPrivate);
}

static GLboolean
PVRDRIValidateImageUsage(__DRIimage *psImage, unsigned int uUse)
{
   return DRISUPValidateImageUsage(psImage, uUse);
}

static __DRIimage *
PVRDRICreateImageFromNames(__DRIscreen *psDRIScreen, int iWidth, int iHeight,
                           int iFourCC, int *piNames, int iNumNames,
                           int *piStrides, int *piOffsets,
                           void *pvLoaderPrivate)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPCreateImageFromNames(psPVRScreen->psDRISUPScreen,
                                     iWidth, iHeight, iFourCC,
                                     piNames, iNumNames,
                                     piStrides, piOffsets, pvLoaderPrivate);
}

static __DRIimage *
PVRDRIFromPlanar(__DRIimage *psImage, int iPlane, void *pvLoaderPrivate)
{
   return DRISUPFromPlanar(psImage, iPlane, pvLoaderPrivate);
}

static __DRIimage *
PVRDRICreateImageFromTexture(__DRIcontext *psDRIContext, int iTarget,
                             unsigned int uTexture, int iDepth, int iLevel,
                             unsigned int *puError, void *pvLoaderPrivate)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;
   int iEGLTarget;

   switch (iTarget) {
   case GL_TEXTURE_2D:
      iEGLTarget = PVRDRI_GL_TEXTURE_2D;
      break;
   case GL_TEXTURE_3D:
      iEGLTarget = PVRDRI_GL_TEXTURE_3D;
      break;
   case GL_TEXTURE_CUBE_MAP:
      iEGLTarget = PVRDRI_GL_TEXTURE_CUBE_MAP_POSITIVE_X;
      break;
   default:
      errorMessage("%s: GL Target %d is not supported",
                   __func__, iTarget);
      *puError = __DRI_IMAGE_ERROR_BAD_PARAMETER;
      return NULL;
   }

   return DRISUPCreateImageFromTexture(psPVRContext->psDRISUPContext,
                                       iEGLTarget, uTexture, iDepth, iLevel,
                                       puError, pvLoaderPrivate);
}

static __DRIimage *
PVRDRICreateImageFromFds(__DRIscreen *psDRIScreen, int iWidth, int iHeight,
                         int iFourCC, int *piFDs, int iNumFDs,
                         int *piStrides, int *piOffsets,
                         void *pvLoaderPrivate)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPCreateImageFromFDs(psPVRScreen->psDRISUPScreen,
                                   iWidth, iHeight, iFourCC, piFDs, iNumFDs,
                                   piStrides, piOffsets, pvLoaderPrivate);
}

static __DRIimage *
PVRDRICreateImageFromDmaBufs(__DRIscreen *psDRIScreen,
                             int iWidth, int iHeight, int iFourCC,
                             int *piFDs, int iNumFDs,
                             int *piStrides, int *piOffsets,
                             enum __DRIYUVColorSpace eColorSpace,
                             enum __DRISampleRange eSampleRange,
                             enum __DRIChromaSiting eHorizSiting,
                             enum __DRIChromaSiting eVertSiting,
                             unsigned int *puError, void *pvLoaderPrivate)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPCreateImageFromDmaBufs(psPVRScreen->psDRISUPScreen,
                                       iWidth, iHeight, iFourCC,
                                       piFDs, iNumFDs, piStrides, piOffsets,
                                       (unsigned int) eColorSpace,
                                       (unsigned int) eSampleRange,
                                       (unsigned int) eHorizSiting,
                                       (unsigned int) eVertSiting,
                                       puError, pvLoaderPrivate);
}

static void
PVRDRIBlitImage(__DRIcontext *psDRIContext,
                __DRIimage *psDst, __DRIimage *psSrc,
                int iDstX0, int iDstY0, int iDstWidth, int iDstHeight,
                int iSrcX0, int iSrcY0, int iSrcWidth, int iSrcHeight,
                int iFlushFlag)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   return DRISUPBlitImage(psPVRContext->psDRISUPContext,
                          psDst, psSrc,
                          iDstX0, iDstY0, iDstWidth, iDstHeight,
                          iSrcX0, iSrcY0, iSrcWidth, iSrcHeight,
                          iFlushFlag);
}

static int
PVRDRIGetCapabilities(__DRIscreen *psDRIScreen)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPGetImageCapabilities(psPVRScreen->psDRISUPScreen);
}

static void *
PVRDRIMapImage(__DRIcontext *psDRIContext, __DRIimage *psImage,
               int iX0, int iY0, int iWidth, int iHeight,
               unsigned int iFlags, int *piStride, void **ppvData)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   return DRISUPMapImage(psPVRContext->psDRISUPContext, psImage,
                         iX0, iY0, iWidth, iHeight, iFlags, piStride,
                         ppvData);
}

static void
PVRDRIUnmapImage(__DRIcontext *psDRIContext, __DRIimage *psImage,
                 void *pvData)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   return DRISUPUnmapImage(psPVRContext->psDRISUPContext, psImage, pvData);
}

static __DRIimage *
PVRDRICreateImageWithModifiers(__DRIscreen *psDRIScreen,
                               int iWidth, int iHeight, int iFormat,
                               const uint64_t *puModifiers,
                               const unsigned int uModifierCount,
                               void *pvLoaderPrivate)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;
   int iFourCC = PVRDRIFormatToFourCC(iFormat);

   return DRISUPCreateImageWithModifiers(psPVRScreen->psDRISUPScreen,
                                         iWidth, iHeight, iFourCC,
                                         puModifiers, uModifierCount,
                                         pvLoaderPrivate);
}

static __DRIimage *
PVRDRICreateImageFromDmaBufs2(__DRIscreen *psDRIScreen,
                              int iWidth, int iHeight,
                              int iFourCC, uint64_t uModifier,
                              int *piFDs, int iNumFDs,
                              int *piStrides, int *piOffsets,
                              enum __DRIYUVColorSpace eColorSpace,
                              enum __DRISampleRange eSampleRange,
                              enum __DRIChromaSiting eHorizSiting,
                              enum __DRIChromaSiting eVertSiting,
                              unsigned int *puError, void *pvLoaderPrivate)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPCreateImageFromDMABufs2(psPVRScreen->psDRISUPScreen,
                                        iWidth, iHeight, iFourCC, uModifier,
                                        piFDs, iNumFDs, piStrides, piOffsets,
                                        (unsigned int) eColorSpace,
                                        (unsigned int) eSampleRange,
                                        (unsigned int) eHorizSiting,
                                        (unsigned int) eVertSiting,
                                        puError, pvLoaderPrivate);
}

static GLboolean
PVRDRIQueryDmaBufFormats(__DRIscreen *psDRIScreen, int iMax,
                         int *piFormats, int *piCount)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPQueryDMABufFormats(psPVRScreen->psDRISUPScreen, iMax,
                                   piFormats, piCount);
}

static GLboolean
PVRDRIQueryDmaBufModifiers(__DRIscreen *psDRIScreen, int iFourCC, int iMax,
                           uint64_t *puModifiers,
                           unsigned int *puExternalOnly, int *piCount)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPQueryDMABufModifiers(psPVRScreen->psDRISUPScreen, iFourCC,
                                     iMax,
                                     puModifiers, puExternalOnly, piCount);
}

static GLboolean
PVRDRIQueryDmaBufFormatModifierAttribs(__DRIscreen *psDRIScreen,
                                       uint32_t uFourCC, uint64_t uModifier,
                                       int iAttrib, uint64_t *puValue)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;
   struct DRISUPScreen *psDRISUPScreen = psPVRScreen->psDRISUPScreen;

   return DRISUPQueryDMABufFormatModifierAttribs(psDRISUPScreen, uFourCC,
                                                 uModifier, iAttrib, puValue);
}

static __DRIimage *
PVRDRICreateImageFromRenderbuffer2(__DRIcontext *psDRIContext,
                                   int iRenderBuffer, void *pvLoaderPrivate,
                                   unsigned int *puError)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;
   struct DRISUPContext *psDRISUPContext = psPVRContext->psDRISUPContext;

   return DRISUPCreateImageFromRenderBuffer2(psDRISUPContext, iRenderBuffer,
                                             pvLoaderPrivate, puError);
}

static __DRIimage *
PVRDRICreateImageFromDmaBufs3(__DRIscreen *psDRIScreen,
                              int iWidth, int iHeight,
                              int iFourCC, uint64_t uModifier,
                              int *piFDs, int iNumFDs,
                              int *piStrides, int *piOffsets,
                              enum __DRIYUVColorSpace eColorSpace,
                              enum __DRISampleRange eSampleRange,
                              enum __DRIChromaSiting eHorizSiting,
                              enum __DRIChromaSiting eVertSiting,
                              uint32_t uFlags, unsigned int *puError,
                              void *pvLoaderPrivate)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPCreateImageFromDMABufs3(psPVRScreen->psDRISUPScreen,
                                        iWidth, iHeight, iFourCC, uModifier,
                                        piFDs, iNumFDs, piStrides, piOffsets,
                                        (unsigned int) eColorSpace,
                                        (unsigned int) eSampleRange,
                                        (unsigned int) eHorizSiting,
                                        (unsigned int) eVertSiting,
                                        uFlags, puError, pvLoaderPrivate);
}

static __DRIimage *
PVRDRICreateImageWithModifiers2(__DRIscreen *psDRIScreen,
                                int iWidth, int iHeight, int iFormat,
                                const uint64_t *puModifiers,
                                const unsigned int uModifierCount,
                                unsigned int uUse,
                                void *pvLoaderPrivate)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;
   int iFourCC = PVRDRIFormatToFourCC(iFormat);

   return DRISUPCreateImageWithModifiers2(psPVRScreen->psDRISUPScreen,
                                          iWidth, iHeight, iFourCC,
                                          puModifiers, uModifierCount,
                                          uUse, pvLoaderPrivate);
}

static __DRIimage *
PVRDRICreateImageFromFds2(__DRIscreen *psDRIScreen, int iWidth, int iHeight,
                          int iFourCC, int *piFDs, int iNumFDs,
                          uint32_t uFlags, int *piStrides, int *piOffsets,
                          void *pvLoaderPrivate)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPCreateImageFromFDs2(psPVRScreen->psDRISUPScreen,
                                    iWidth, iHeight, iFourCC, piFDs, iNumFDs,
                                    uFlags, piStrides, piOffsets,
                                    pvLoaderPrivate);
}

static void
PVRDRISetInFenceFd(__DRIimage *psImage, int iFd)
{
   return DRISUPSetInFenceFd(psImage, iFd);
}

#if defined(EGL_IMG_cl_image)
static __DRIimage *
PVRDRICreateImageFromBuffer(__DRIcontext *psDRIContext, int iTarget,
                            void *pvBuffer, unsigned int *puError,
                            void *pvLoaderPrivate)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   return DRISUPCreateImageFromBuffer(psPVRContext->psDRISUPContext, iTarget,
                                      pvBuffer, puError, pvLoaderPrivate);
}
#endif

static __DRIimageExtension pvrDRIImage = {
   .base = {
      .name = __DRI_IMAGE,
      .version = PVR_DRI_IMAGE_VERSION,
   },
   .createImageFromName = PVRDRICreateImageFromName,
   .createImageFromRenderbuffer = PVRDRICreateImageFromRenderbuffer,
   .destroyImage = PVRDRIDestroyImage,
   .createImage = PVRDRICreateImage,
   .queryImage = PVRDRIQueryImage,
   .dupImage = PVRDRIDupImage,
   .validateUsage = PVRDRIValidateImageUsage,
   .createImageFromNames = PVRDRICreateImageFromNames,
   .fromPlanar = PVRDRIFromPlanar,
   .createImageFromTexture = PVRDRICreateImageFromTexture,
   .createImageFromFds = PVRDRICreateImageFromFds,
   .createImageFromDmaBufs = PVRDRICreateImageFromDmaBufs,
   .blitImage = PVRDRIBlitImage,
   .getCapabilities = PVRDRIGetCapabilities,
   .mapImage = PVRDRIMapImage,
   .unmapImage = PVRDRIUnmapImage,
   .createImageWithModifiers = PVRDRICreateImageWithModifiers,
   .createImageFromDmaBufs2 = PVRDRICreateImageFromDmaBufs2,
   .queryDmaBufFormats = PVRDRIQueryDmaBufFormats,
   .queryDmaBufModifiers = PVRDRIQueryDmaBufModifiers,
   .queryDmaBufFormatModifierAttribs =
   PVRDRIQueryDmaBufFormatModifierAttribs,
   .createImageFromRenderbuffer2 = PVRDRICreateImageFromRenderbuffer2,
   .createImageFromDmaBufs3 = PVRDRICreateImageFromDmaBufs3,
   .createImageWithModifiers2 = PVRDRICreateImageWithModifiers2,
   .createImageFromFds2 = PVRDRICreateImageFromFds2,
   .setInFenceFd = PVRDRISetInFenceFd,
#if defined(EGL_IMG_cl_image)
   .createImageFromBuffer = PVRDRICreateImageFromBuffer,
#endif
};

static __DRIrobustnessExtension pvrDRIRobustness = {
   .base = {
      .name = __DRI2_ROBUSTNESS,
      .version = PVR_DRI2_ROBUSTNESS_VERSION,
   }
};

static int
PVRDRIQueryRendererInteger(__DRIscreen *psDRIScreen, int iAttribute,
                           unsigned int *puValue)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;
   int res;

   res = DRISUPQueryRendererInteger(psPVRScreen->psDRISUPScreen,
                                    iAttribute, puValue);
   if (res == -1)
      return driQueryRendererIntegerCommon(psDRIScreen, iAttribute, puValue);

   return res;
}

static int
PVRDRIQueryRendererString(__DRIscreen *psDRIScreen, int iAttribute,
                          const char **ppszValue)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPQueryRendererString(psPVRScreen->psDRISUPScreen, iAttribute,
                                    ppszValue);
}

static const __DRI2rendererQueryExtension pvrDRIRendererQueryExtension = {
   .base = {
      .name = __DRI2_RENDERER_QUERY,
      .version = PVR_DRI2_RENDERER_QUERY_VERSION,
   },
   .queryInteger = PVRDRIQueryRendererInteger,
   .queryString = PVRDRIQueryRendererString,
};


static void *
PVRDRICreateFenceEXT(__DRIcontext *psDRIContext)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   return DRISUPCreateFence(psPVRContext->psDRISUPContext);
}

static void
PVRDRIDestroyFenceEXT(__DRIscreen *psDRIScreen, void *pvFence)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPDestroyFence(psPVRScreen->psDRISUPScreen, pvFence);
}

static GLboolean
PVRDRIClientWaitSyncEXT(__DRIcontext *psDRIContext, void *pvFence,
                        unsigned int uFlags, uint64_t uTimeout)
{
   struct DRISUPContext *psDRISUPContext;

   if (psDRIContext) {
      PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

      psDRISUPContext = psPVRContext->psDRISUPContext;
   } else {
      psDRISUPContext = NULL;
   }

   return DRISUPClientWaitSync(psDRISUPContext, pvFence, uFlags, uTimeout);
}

static void
PVRDRIServerWaitSyncEXT(__DRIcontext *psDRIContext, void *pvFence,
                        unsigned int uFlags)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   return DRISUPServerWaitSync(psPVRContext->psDRISUPContext, pvFence, uFlags);
}

static unsigned int
PVRDRIGetFenceCapabilitiesEXT(__DRIscreen *psDRIScreen)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPGetFenceCapabilities(psPVRScreen->psDRISUPScreen);
}

static void *
PVRDRICreateFenceFdEXT(__DRIcontext *psDRIContext, int iFD)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   return DRISUPCreateFenceFD(psPVRContext->psDRISUPContext, iFD);
}

static int
PVRDRIGetFenceFdEXT(__DRIscreen *psDRIScreen, void *pvFence)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPGetFenceFD(psPVRScreen->psDRISUPScreen, pvFence);
}

static void *
PVRDRIGetFenceFromClEventEXT(__DRIscreen *psDRIScreen, intptr_t iClEvent)
{
   PVRDRIScreen *psPVRScreen = psDRIScreen->driverPrivate;

   return DRISUPGetFenceFromCLEvent(psPVRScreen->psDRISUPScreen, iClEvent);
}

__DRI2fenceExtension pvrDRIFenceExtension = {
   .base = {
      .name = __DRI2_FENCE,
      .version = PVR_DRI2_FENCE_VERSION,
   },
   .create_fence = PVRDRICreateFenceEXT,
   .get_fence_from_cl_event = PVRDRIGetFenceFromClEventEXT,
   .destroy_fence = PVRDRIDestroyFenceEXT,
   .client_wait_sync = PVRDRIClientWaitSyncEXT,
   .server_wait_sync = PVRDRIServerWaitSyncEXT,
   .get_capabilities = PVRDRIGetFenceCapabilitiesEXT,
   .create_fence_fd = PVRDRICreateFenceFdEXT,
   .get_fence_fd = PVRDRIGetFenceFdEXT,
};

static void
PVRDRISetDamageRegion(__DRIdrawable *psDRIDrawable,
                      unsigned int uNRects, int *piRects)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;

   DRISUPSetDamageRegion(psPVRDrawable->psDRISUPDrawable, uNRects, piRects);
}

const __DRI2bufferDamageExtension pvrDRIbufferDamageExtension = {
   .base = {
      .name = __DRI2_BUFFER_DAMAGE,
      .version = PVR_DRI2_BUFFER_DAMAGE_VERSION,
   },
   .set_damage_region = PVRDRISetDamageRegion,
};

/*
 * Extension lists
 *
 * NOTE: When adding a new screen extension asScreenExtensionVersionInfo
 *       should also be updated accordingly.
 */
static const __DRIextension *apsScreenExtensions[] = {
   &pvrDRITexBufferExtension.base,
   &pvrDRI2FlushExtension.base,
   &pvrDRIImage.base,
   &pvrDRIRobustness.base,
   &pvrDRIRendererQueryExtension.base,
   &pvrDRIFenceExtension.base,
   &pvrDRIbufferDamageExtension.base,
   &dri2ConfigQueryExtension.base,
   NULL
};

static const __DRIextension asScreenExtensionVersionInfo[] = {
   {.name = __DRI_TEX_BUFFER,.version = __DRI_TEX_BUFFER_VERSION},
   {.name = __DRI2_FLUSH,.version = __DRI2_FLUSH_VERSION},
   {.name = __DRI_IMAGE,.version = __DRI_IMAGE_VERSION},
   {.name = __DRI2_ROBUSTNESS,.version = __DRI2_ROBUSTNESS_VERSION},
   {.name = __DRI2_RENDERER_QUERY,.version = __DRI2_RENDERER_QUERY_VERSION},
   {.name = __DRI2_FENCE,.version = __DRI2_FENCE_VERSION},
   {.name = __DRI2_BUFFER_DAMAGE,.version = __DRI2_BUFFER_DAMAGE_VERSION},
   {.name = __DRI2_CONFIG_QUERY,.version = __DRI2_CONFIG_QUERY_VERSION},
   {.name = NULL,.version = 0},
};

const __DRIextension **
PVRDRIScreenExtensions(void)
{
   return apsScreenExtensions;
}

const __DRIextension *
PVRDRIScreenExtensionVersionInfo(void)
{
   return asScreenExtensionVersionInfo;
}

void
PVRDRIAdjustExtensions(unsigned int uVersion, unsigned int uMinVersion)
{
   /* __DRI2fenceExtension adjustment */
   switch (uVersion) {
   default:
   case 5:
   case 4:
      /* Is the KHR_cl_event2 EGL extension supported? */
      if (!DRISUPHaveGetFenceFromCLEvent())
         pvrDRIFenceExtension.get_fence_from_cl_event = NULL;

      break;
   case 3:
      break;
   case 2:
   case 1:
   case 0:
      /* The KHR_cl_event2 EGL extension is not supported */
      pvrDRIFenceExtension.get_fence_from_cl_event = NULL;
      break;
   }

   /* __DRIimageExtension adjustment */
   switch (uVersion) {
   default:
   case 5:
      if (!DRISUPHaveSetInFenceFd())
         pvrDRIImage.setInFenceFd = NULL;

      break;
   case 4:
   case 3:
   case 2:
   case 1:
   case 0:
      /*
       * The following are not supported:
       *    createImageFromDmaBufs3
       *    createImageWithModifiers2
       *    createImageFromFds2
       *    setInFenceFd
       */
      pvrDRIImage.base.version = 17;
      break;
   }
}
