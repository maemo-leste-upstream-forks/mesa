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

#include <assert.h>
#include <xf86drm.h>

#include "dri_util.h"

#include "sgx_support.h"
#include "sgx_dri.h"

#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "EGL/eglmesaext.h"

/* Maximum version numbers for each supported extension */
#define PVR_DRI_TEX_BUFFER_VERSION      2
#define PVR_DRI2_FLUSH_VERSION          4
#define PVR_DRI_IMAGE_VERSION           7
#define PVR_DRI2_ROBUSTNESS_VERSION     1
#define PVR_DRI_PRIORITY_VERSION        1
#define PVR_DRI2_FENCE_VERSION          1

/* IMG extension */
#define PVR_DRI_QUERY_BUFFERS_VERSION 1

static void
PVRDRIExtSetTexBuffer(__DRIcontext *psDRIContext, GLint iTarget,
                      GLint iFormat, __DRIdrawable *psDRIDrawable)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   (void) iTarget;
   (void) iFormat;

   if (!psPVRDrawable->bInitialised)
   {
      if (!PVRDRIDrawableInit(psPVRDrawable))
            {
         mesa_loge("%s: Couldn't initialise pixmap", __func__);
         return;
      }
   }

   PVRQElem *psQElem;

   PVRDRIDrawableLock(psPVRDrawable);
   psQElem = psPVRDrawable->sPVRContextHead.pvForw;

   while (psQElem != &psPVRDrawable->sPVRContextHead)
   {
      PVRDRIContext *psPVRContext = PVRQ_CONTAINER_OF(psQElem, PVRDRIContext, sQElem);
      PVRDRIEGLFlushBuffers(psPVRContext->eAPI,
            psPVRContext->psPVRScreen->psImpl,
            psPVRContext->psImpl,
            psPVRDrawable->psImpl,
            false,
            false,
            false);
      psQElem = psPVRContext->sQElem.pvForw;
   }

   PVRDRIDrawableUnlock(psPVRDrawable);

   PVRDRI2BindTexImage(psPVRContext->eAPI,
         psPVRContext->psPVRScreen->psImpl,
         psPVRContext->psImpl,
         psPVRDrawable->psImpl);
}

static void
PVRDRIExtReleaseTexBuffer(__DRIcontext *psDRIContext, GLint iTarget,
                          __DRIdrawable *psDRIDrawable)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   (void) iTarget;

   PVRDRI2ReleaseTexImage(psPVRContext->eAPI,
         psPVRContext->psPVRScreen->psImpl,
         psPVRContext->psImpl,
         psPVRDrawable->psImpl);
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

   PVRDRIDrawableLock(psPVRDrawable);

   (void) PVRDRIFlushBuffersForSwap(NULL, psPVRDrawable);
   /*
    * Work around for missing X Server invalidate events. Mark
    * the drawable as invalid, to force a query for new buffers.
    */
   psPVRDrawable->bDrawableInfoInvalid = true;

   PVRDRIDrawableUnlock(psPVRDrawable);
}

static void
PVRDRI2Invalidate(__DRIdrawable *psDRIDrawable)
{
   PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;

   if (psPVRDrawable->psPVRScreen->bUseInvalidate) {
      PVRDRIDrawableLock(psPVRDrawable);
      psPVRDrawable->bDrawableInfoInvalid = true;
      psPVRDrawable->bDrawableInfoUpdated = false;
      PVRDRIDrawableUnlock(psPVRDrawable);
   }
}

static void
PVRDRI2FlushWithFlags(__DRIcontext *psDRIContext,
                      __DRIdrawable *psDRIDrawable,
                      unsigned int uFlags,
                      enum __DRI2throttleReason eThrottleReason)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   (void) eThrottleReason;

   if ((uFlags & __DRI2_FLUSH_DRAWABLE) != 0) {
      PVRDRIDrawable *psPVRDrawable = psDRIDrawable->driverPrivate;

      PVRDRIDrawableLock(psPVRDrawable);

      (void) PVRDRIFlushBuffersForSwap(psPVRContext, psPVRDrawable);
      /*
       * Work around for missing X Server invalidate events. Mark
       * the drawable as invalid, to force a query for new buffers.
       */
      psPVRDrawable->bDrawableInfoInvalid = true;

      PVRDRIDrawableUnlock(psPVRDrawable);
   } else if ((uFlags & __DRI2_FLUSH_CONTEXT) != 0) {
      PVRDRIEGLFlushBuffers(psPVRContext->eAPI,
            psPVRContext->psPVRScreen->psImpl,
            psPVRContext->psImpl,
            NULL, true, false, false);
   }
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
   __EGLImage *psEGLImage;
   PVRDRIEGLImageType eglImageType;
};

struct __DRIimageRec
{
   int iRefCount;

   void *loaderPrivate;

   struct PVRDRIImageShared *psShared;

   __EGLImage *psEGLImage;
};


static struct PVRDRIImageShared *
CommonImageSharedSetup(PVRDRIImageType eType,
                       __DRIscreen *screen)
{
   struct PVRDRIImageShared *shared;

   shared = calloc(1, sizeof(*shared));
   if (!shared)
   {
      return NULL;
   }

   shared->psPVRScreen = screen->driverPrivate;
   shared->eType = eType;
   shared->iRefCount = 1;

   assert(shared->eColourSpace == IMG_COLORSPACE_UNDEFINED &&
          shared->eChromaUInterp == IMG_CHROMA_INTERP_UNDEFINED &&
          shared->eChromaVInterp == IMG_CHROMA_INTERP_UNDEFINED);

   return shared;
}

static void
DestroyImageShared(struct PVRDRIImageShared *shared)
{
   int iRefCount = __sync_sub_and_fetch(&shared->iRefCount, 1);

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
         break;
      case PVRDRI_IMAGE_FROM_EGLIMAGE:
         PVRDRIEGLImageDestroyExternal(shared->psPVRScreen->psImpl,
                                       shared->psEGLImage,
                        shared->eglImageType);
         break;
      default:
         mesa_loge("%s: Unknown image type: %d\n", __func__, (int)shared->eType);
         break;
   }

   free(shared);
}

static struct PVRDRIImageShared *
CreateImageSharedFromEGLImage(__DRIscreen *screen,
                              __EGLImage *psEGLImage,
               PVRDRIEGLImageType eglImageType)
{
   struct PVRDRIImageShared *shared;
   PVRDRIBufferAttribs sAttribs;
   const PVRDRIImageFormat *psFormat;

   PVRDRIEGLImageGetAttribs(psEGLImage, &sAttribs);

   psFormat = PVRDRIIMGPixelFormatToImageFormat(sAttribs.ePixFormat);
   if (!psFormat)
   {
      return NULL;
   }

   shared = CommonImageSharedSetup(PVRDRI_IMAGE_FROM_EGLIMAGE, screen);
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
   struct PVRDRIImageShared *shared;
   const PVRDRIImageFormat *psFormat;
   int aiPlaneNames[DRI_PLANES_MAX];
   unsigned auiWidthShift[DRI_PLANES_MAX];
   unsigned auiHeightShift[DRI_PLANES_MAX];
   int i;

   psFormat = PVRDRIFourCCToImageFormat(fourcc);
   if (!psFormat)
   {
      mesa_loge("%s: Unsupported DRI FourCC (fourcc = 0x%X)\n",
              __func__, fourcc);
      return NULL;
   }

   if (num_names != 1 && num_names != psFormat->uiNumPlanes)
   {
      mesa_loge("%s: Unexpected number of names (%d) for fourcc "
              "(#%x) - expected 1 or %u\n",
              __func__, num_names, fourcc,
              psFormat->uiNumPlanes);
      return NULL;
   }

   for (i = 0; i < psFormat->uiNumPlanes; i++)
   {
      if (offsets[i] < 0)
      {
         mesa_loge("%s: Offset %d unsupported (value = %d)\n",
                 __func__, i, offsets[i]);
         return NULL;
      }

      aiPlaneNames[i] = num_names == 1 ? names[0] : names[i];
      auiWidthShift[i] = psFormat->sPlanes[i].uiWidthShift;
      auiHeightShift[i] = psFormat->sPlanes[i].uiHeightShift;
   }

   shared = CommonImageSharedSetup(PVRDRI_IMAGE_FROM_NAMES, screen);
   if (!shared)
   {
      return NULL;
   }

   shared->psBuffer = PVRDRIBufferCreateFromNames(shared->psPVRScreen->psImpl,
                          width,
                          height,
                          psFormat->uiNumPlanes,
                          aiPlaneNames,
                          strides,
                          offsets,
                          auiWidthShift,
                          auiHeightShift);

   if (!shared->psBuffer)
   {
      mesa_loge("%s: Failed to create buffer for shared image\n", __func__);
      goto ErrorDestroyImage;
   }

   shared->psFormat = psFormat;
   shared->eColourSpace = PVRDRIToIMGColourSpace(psFormat,
                        __DRI_YUV_COLOR_SPACE_UNDEFINED,
                        __DRI_YUV_RANGE_UNDEFINED);
   shared->eChromaUInterp = PVRDRIChromaSittingToIMGInterp(psFormat,
                        __DRI_YUV_RANGE_UNDEFINED);
   shared->eChromaVInterp = PVRDRIChromaSittingToIMGInterp(psFormat,
                        __DRI_YUV_RANGE_UNDEFINED);

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
   struct PVRDRIImageShared *shared;
   const PVRDRIImageFormat *psFormat;
   int aiPlaneFds[DRI_PLANES_MAX];
   unsigned auiWidthShift[DRI_PLANES_MAX];
   unsigned auiHeightShift[DRI_PLANES_MAX];
   int i;

   psFormat = PVRDRIFourCCToImageFormat(fourcc);
   if (!psFormat)
   {
      mesa_loge("%s: Unsupported DRI FourCC (fourcc = 0x%X)\n",
              __func__, fourcc);
      *error = __DRI_IMAGE_ERROR_BAD_MATCH;
      return NULL;
   }

   if (num_fds != 1 && num_fds != psFormat->uiNumPlanes)
   {
      mesa_loge("%s: Unexpected number of fds (%d) for fourcc "
              "(#%x) - expected 1 or %u\n",
              __func__, num_fds, fourcc,
              psFormat->uiNumPlanes);
      *error = __DRI_IMAGE_ERROR_BAD_MATCH;
      return NULL;
   }

   for (i = 0; i < psFormat->uiNumPlanes; i++)
   {
      if (offsets[i] < 0)
      {
         mesa_loge("%s: Offset %d unsupported (value = %d)\n",
                 __func__, i, offsets[i]);
         *error = __DRI_IMAGE_ERROR_BAD_ACCESS;
         return NULL;
      }

      aiPlaneFds[i] = num_fds == 1 ? fds[0] : fds[i];
      auiWidthShift[i] = psFormat->sPlanes[i].uiWidthShift;
      auiHeightShift[i] = psFormat->sPlanes[i].uiHeightShift;
   }

   shared = CommonImageSharedSetup(PVRDRI_IMAGE_FROM_DMABUFS, screen);
   if (!shared)
   {
      *error = __DRI_IMAGE_ERROR_BAD_ALLOC;
      return NULL;
   }

   shared->psBuffer = PVRDRIBufferCreateFromFds(shared->psPVRScreen->psImpl,
                        width,
                        height,
                        psFormat->uiNumPlanes,
                        aiPlaneFds,
                        strides,
                        offsets,
                        auiWidthShift,
                        auiHeightShift);

   if (!shared->psBuffer)
   {
      mesa_loge("%s: Failed to create buffer for shared image\n", __func__);
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
   struct PVRDRIImageShared *shared;
   const PVRDRIImageFormat *psFormat;
   unsigned int uiStride;

   if ((use & __DRI_IMAGE_USE_CURSOR) && (use & __DRI_IMAGE_USE_SCANOUT))
   {
      return NULL;
   }

   psFormat = PVRDRIFormatToImageFormat(format);
   if (!psFormat)
   {
      mesa_loge("%s: Unsupported DRI image format (format = 0x%X)\n",
              __func__, format);
      return NULL;
   }

   shared = CommonImageSharedSetup(PVRDRI_IMAGE, screen);
   if (!shared)
   {
      return NULL;
   }

   shared->psBuffer =
      PVRDRIBufferCreate(shared->psPVRScreen->psImpl,
               width,
               height,
               PVRDRIPixFmtGetBPP(psFormat->eIMGPixelFormat),
               use,
               &uiStride);
   if (!shared->psBuffer)
   {
      mesa_loge("%s: Failed to create buffer\n", __func__);
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
RefImageShared(struct PVRDRIImageShared *shared)
{
   int iRefCount = __sync_fetch_and_add(&shared->iRefCount, 1);

   (void)iRefCount;
   assert(iRefCount > 0);

   return shared;
}

static __DRIimage *
CommonImageSetup(void *loaderPrivate)
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

static void
PVRDRIDestroyImage(__DRIimage *image)
{
   int iRefCount = __sync_sub_and_fetch(&image->iRefCount, 1);

   assert(iRefCount >= 0);

   if (iRefCount > 0)
   {
      return;
   }

   if (image->psShared)
   {
      DestroyImageShared(image->psShared);
   }

   if (image->psEGLImage)
   {
      PVRDRIEGLImageFree(image->psEGLImage);
   }

   free(image);
}

static __DRIimage *
PVRDRICreateImageFromNames(__DRIscreen *screen,
                   int width, int height, int fourcc,
                   int *names, int num_names,
                   int *strides, int *offsets,
                   void *loaderPrivate);

static __DRIimage *
PVRDRICreateImageFromName(__DRIscreen *screen,
                  int width, int height, int format,
                  int name, int pitch,
                  void *loaderPrivate)
{
   const PVRDRIImageFormat *psFormat;
   int iStride;
   int iOffset;

   psFormat = PVRDRIFormatToImageFormat(format);
   if (!psFormat)
   {
      mesa_loge("%s: Unsupported DRI image format (format = 0x%X)\n",
              __func__, format);
      return NULL;
   }

   iStride = pitch * PVRDRIPixFmtGetBlockSize(psFormat->eIMGPixelFormat);
   iOffset = 0;

   return PVRDRICreateImageFromNames(screen, width, height, psFormat->iDRIFourCC,
                 &name, 1, &iStride, &iOffset, loaderPrivate);
}

static __DRIimage *
PVRDRICreateImageFromRenderbuffer(__DRIcontext *context,
                                              int           renderbuffer,
                                              void         *loaderPrivate)
{
   PVRDRIContext *psPVRContext = context->driverPrivate;
   __DRIscreen *screen = psPVRContext->psPVRScreen->psDRIScreen;
   unsigned e;
   __EGLImage *psEGLImage;
   __DRIimage *image;

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
                            EGL_GL_RENDERBUFFER_KHR,
                            (uintptr_t)renderbuffer,
                            0,
                            psEGLImage);

   if (e != PVRDRI_IMAGE_ERROR_SUCCESS)
   {
      PVRDRIEGLImageFree(psEGLImage);
      PVRDRIDestroyImage(image);
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

static __DRIimage *
PVRDRICreateImage(__DRIscreen *screen,
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
                         image->psShared->eChromaUInterp,
                         image->psShared->psBuffer);
   if (!image->psEGLImage)
   {
      PVRDRIDestroyImage(image);
      return NULL;
   }

   PVRDRIEGLImageSetCallbackData(image->psEGLImage, image);

   return image;
}

static GLboolean
PVRDRIQueryImage(__DRIimage *image, int attrib, int *value_ptr)
{
   struct PVRDRIImageShared *shared = image->psShared;
   PVRDRIBufferAttribs sAttribs;
   int value;

   PVRDRIEGLImageGetAttribs(image->psEGLImage, &sAttribs);

   if (attrib == __DRI_IMAGE_ATTRIB_HANDLE ||
       attrib == __DRI_IMAGE_ATTRIB_NAME ||
       attrib == __DRI_IMAGE_ATTRIB_FD)
   {
      if (!shared->psFormat)
      {
         return GL_FALSE;
      }

      if (shared->psFormat->iDRIComponents != __DRI_IMAGE_COMPONENTS_RGBA &&
          shared->psFormat->iDRIComponents != __DRI_IMAGE_COMPONENTS_RGB)
      {
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
         if (!shared->psFormat)
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
         *value_ptr = 1;
         break;
      case __DRI_IMAGE_ATTRIB_OFFSET:
         *value_ptr = 0;
         break;
      default:
         return GL_FALSE;
   }

   return GL_TRUE;
}

static __DRIimage *
PVRDRIDupImage(__DRIimage *srcImage, void *loaderPrivate)
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

static GLboolean
PVRDRIValidateUsage(__DRIimage *image, unsigned int use)
{
   __DRIscreen *screen = image->psShared->psPVRScreen->psDRIScreen;

   if (use & (__DRI_IMAGE_USE_SCANOUT | __DRI_IMAGE_USE_CURSOR)) {
      /*
       * We are extra strict in this case as an application may ask for a
       * handle so that the memory can be wrapped as a framebuffer/used as
       * a cursor and this can only be done on a card node.
       */
      if (drmGetNodeTypeFromFd(screen->fd) != DRM_NODE_PRIMARY)
         return GL_FALSE;
   } else if (use & (__DRI_IMAGE_USE_SHARE)) {
      /*
       * We are less strict in this case as it's possible to share buffers
       * using prime (but not flink) on a render node so we only need to know
       * whether or not the fd belongs to the display.
       */
      if (PVRDRIGetDeviceTypeFromFd(screen->fd) != PVRDRI_DEVICE_TYPE_DISPLAY)
         return GL_FALSE;
   }

   return GL_TRUE;
}

static __DRIimage *
PVRDRICreateImageFromNames(__DRIscreen *screen,
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

static __DRIimage *
PVRDRIFromPlanar(__DRIimage *image, int plane,
              void *loaderPrivate)
{
   if (plane != 0)
   {
      mesa_loge("%s: plane %d not supported\n", __func__, plane);
   }

   return PVRDRIDupImage(image, loaderPrivate);
}

static __DRIimage *
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
   __EGLImage *psEGLImage;
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
         mesa_loge("%s: GL Target %d is not supported\n", __func__, glTarget);
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

static __DRIimage *
PVRDRICreateImageFromDmaBufs(__DRIscreen *screen,
                                         int width, int height, int fourcc,
                                         int *fds, int num_fds,
                                         int *strides, int *offsets,
                                         enum __DRIYUVColorSpace color_space,
                                         enum __DRISampleRange sample_range,
                                         enum __DRIChromaSiting horiz_siting,
                                         enum __DRIChromaSiting vert_siting,
                                         unsigned *error,
                                         void *loaderPrivate);

static __DRIimage *
PVRDRICreateImageFromFds(__DRIscreen *screen,
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

static __DRIimage *
PVRDRICreateImageFromBuffer(__DRIcontext *context,
                            int target,
                            void *buffer,
                            unsigned *error,
                            void *loaderPrivate)
{
   PVRDRIContext *psPVRContext = context->driverPrivate;
   __DRIscreen *screen = psPVRContext->psPVRScreen->psDRIScreen;
   __EGLImage *psEGLImage;
   __DRIimage *image;

   switch (target)
   {
#if defined(EGL_CL_IMAGE_IMG)
      case EGL_CL_IMAGE_IMG:
         break;
#endif
      default:
         mesa_loge("%s: Target %d is not supported\n", __func__, target);
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

static __DRIimage *
PVRDRICreateImageFromDmaBufs(__DRIscreen *screen,
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
   __DRIimage *image;

   image = CommonImageSetup(loaderPrivate);
   if (!image)
   {
      *error = __DRI_IMAGE_ERROR_BAD_ALLOC;
      return NULL;
   }

   image->psShared = CreateImageSharedFromDmaBufs(screen, width, height, fourcc,
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

void PVRDRIRefImage(__DRIimage *image)
{
   int iRefCount = __sync_fetch_and_add(&image->iRefCount, 1);

   (void)iRefCount;
   assert(iRefCount > 0);
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

__EGLImage *PVRDRIImageGetSharedEGLImage(__DRIimage *pImage)
{
   assert(pImage->psShared->eType == PVRDRI_IMAGE_FROM_EGLIMAGE);
   return pImage->psShared->psEGLImage;
}

__EGLImage *PVRDRIImageGetEGLImage(__DRIimage *pImage)
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
   .validateUsage = PVRDRIValidateUsage,
   .createImageFromNames = PVRDRICreateImageFromNames,
   .fromPlanar = PVRDRIFromPlanar,
   .createImageFromTexture = PVRDRICreateImageFromTexture,
   .createImageFromFds = PVRDRICreateImageFromFds,
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

#if defined(__DRI_PRIORITY)
static __DRIpriorityExtension pvrDRIPriority = {
   .base = {
      .name = __DRI_PRIORITY,
      .version = PVR_DRI_PRIORITY_VERSION,
   },
};
#endif /* defined(__DRI_PRIORITY) */

#if defined(__DRI2_FENCE)
static void *
PVRDRICreateFenceEXT(__DRIcontext *psDRIContext)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;
   PVRDRIScreen *psPVRScreen = psPVRContext->psPVRScreen;

   return PVRDRICreateFenceImpl(psPVRContext->eAPI,
         psPVRScreen->psImpl,
         psPVRContext->psImpl);
}

static void
PVRDRIDestroyFenceEXT(__DRIscreen *psDRIScreen, void *psDRIFence)
{
   (void) psDRIScreen;

   PVRDRIDestroyFenceImpl(psDRIFence);
}

static GLboolean
PVRDRIClientWaitSyncEXT(__DRIcontext *psDRIContext, void *psDRIFence,
                        unsigned int uFlags, uint64_t uiTimeout)
{
   bool bFlushCommands = (uFlags & __DRI2_FENCE_FLAG_FLUSH_COMMANDS);
   bool bTimeout = (uiTimeout != __DRI2_FENCE_TIMEOUT_INFINITE);
   PVRDRIAPIType eAPI = 0;
   PVRDRIContextImpl *psImpl = NULL;

   if (psDRIContext) {
      PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

      eAPI = psPVRContext->eAPI;
      psImpl = psPVRContext->psImpl;
   }

   return PVRDRIClientWaitSyncImpl(eAPI, psImpl, psDRIFence, bFlushCommands, bTimeout, uiTimeout);
}

static void
PVRDRIServerWaitSyncEXT(__DRIcontext *psDRIContext, void *psDRIFence,
                        unsigned int uFlags)
{
   PVRDRIContext *psPVRContext = psDRIContext->driverPrivate;

   (void) uFlags;
   assert(uFlags == 0);

   if (!PVRDRIServerWaitSyncImpl(psPVRContext->eAPI, psPVRContext->psImpl, psDRIFence))
      mesa_loge("%s: Server wait sync failed", __func__);
}

static __DRI2fenceExtension pvrDRIFenceExtension = {
   .base = {
      .name = __DRI2_FENCE,
      .version = PVR_DRI2_FENCE_VERSION,
   },
   .create_fence = PVRDRICreateFenceEXT,
   /* Not currently supported */
   .get_fence_from_cl_event = NULL,
   .destroy_fence = PVRDRIDestroyFenceEXT,
   .client_wait_sync = PVRDRIClientWaitSyncEXT,
   .server_wait_sync = PVRDRIServerWaitSyncEXT,
};
#endif /* defined(__DRI2_FENCE) */

#if defined(__DRI_QUERY_BUFFERS)
static void PVRDRIQueryBuffersEXT(__DRIdrawable *psDRIDrawable)
{
	PVRDRIDrawable *psPVRDrawable = (PVRDRIDrawable *)psDRIDrawable->driverPrivate;

	PVRDRIDrawableLock(psPVRDrawable);
	psPVRDrawable->bDrawableInfoUpdated =
		 PVRDRIDrawableUpdateNativeInfo(psPVRDrawable);
	PVRDRIDrawableUnlock(psPVRDrawable);
}

static __DRIqueryBuffersExtension pvrDRIQueryBuffers =  {
   .base = {
      .name = __DRI_QUERY_BUFFERS,
      .version = PVR_DRI_QUERY_BUFFERS_VERSION
   },
   .query_buffers = PVRDRIQueryBuffersEXT,
};
#endif /* defined(__DRI_QUERY_BUFFERS) */

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
#if defined(__DRI_PRIORITY)
   &pvrDRIPriority.base,
#endif
#if defined(__DRI2_FENCE)
   &pvrDRIFenceExtension.base,
#endif
#if defined(__DRI_QUERY_BUFFERS)
   &pvrDRIQueryBuffers.base,
#endif
   NULL
};

static const __DRIextension asScreenExtensionVersionInfo[] = {
   {.name = __DRI_TEX_BUFFER,.version = __DRI_TEX_BUFFER_VERSION},
   {.name = __DRI2_FLUSH,.version = __DRI2_FLUSH_VERSION},
   {.name = __DRI_IMAGE,.version = __DRI_IMAGE_VERSION},
   {.name = __DRI2_ROBUSTNESS,.version = __DRI2_ROBUSTNESS_VERSION},
#if defined(__DRI_PRIORITY)
   {.name = __DRI_PRIORITY,.version = __DRI_PRIORITY_VERSION},
#endif
#if defined(__DRI2_FENCE)
   {.name = __DRI2_FENCE,.version = __DRI2_FENCE_VERSION},
#endif
#if defined(__DRI_QUERY_BUFFERS)
   {.name = __DRI_QUERY_BUFFERS,.version = __DRI_QUERY_BUFFERS_VERSION},
#endif
   {.name = NULL,.version = 0},
};

const __DRIextension **
SGXDRIScreenExtensions(void)
{
   return apsScreenExtensions;
}

const __DRIextension *
SGXDRIScreenExtensionVersionInfo(void)
{
   return asScreenExtensionVersionInfo;
}

