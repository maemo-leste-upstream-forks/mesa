/*************************************************************************//*!
 @File
 @Title          PVR DRI Surface/Drawable code
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
 *//**************************************************************************/

#include <assert.h>

#include "dri_util.h"

#include "pvrdri.h"

/***********************************************************************************
 Function Name : PVRDRIDrawableLock
 Inputs     : psPVRDrawable - PVRDRI drawable structure
 Returns : Boolean
 Description   : Lock drawable mutex (can be called recursively)
 ************************************************************************************/
void PVRDRIDrawableLock(PVRDRIDrawable *psPVRDrawable)
{
   int res = pthread_mutex_lock(&psPVRDrawable->sMutex);
   if (res != 0) {
      mesa_loge("%s: Failed to lock drawable (%d)\n", __func__, res);
      abort();
   }
}

/***********************************************************************************
 Function Name : PVRDRIDrawableUnlock
 Inputs     : psPVRDrawable - PVRDRI drawable structure
 Returns : Boolean
 Description   : Unlock drawable mutex (can be called recursively)
 ************************************************************************************/
void PVRDRIDrawableUnlock(PVRDRIDrawable *psPVRDrawable)
{
   int res = pthread_mutex_unlock(&psPVRDrawable->sMutex);
   if (res != 0) {
      mesa_loge("%s: Failed to unlock drawable (%d)\n", __func__, res);
      abort();
   }
}

static PVRDRIBufferImpl* PVRGetBackingBuffer(PVRDRIBuffer *psPVRDRIBuffer)
{
   if (psPVRDRIBuffer)
   {
      switch (psPVRDRIBuffer->eBackingType)
      {
      case PVRDRI_BUFFER_BACKING_DRI2:
         return psPVRDRIBuffer->uBacking.sDRI2.psBuffer;
      case PVRDRI_BUFFER_BACKING_IMAGE:
         return PVRDRIImageGetSharedBuffer(psPVRDRIBuffer->uBacking.sImage.psImage);
      default:
         assert(0);
         return NULL;
      }
   }

   return NULL;
}

static inline void PVRDRIMarkAllRenderSurfacesAsInvalid(PVRDRIDrawable *psPVRDrawable)
{
   PVRQElem *psQElem = psPVRDrawable->sPVRContextHead.pvForw;

   while (psQElem != &psPVRDrawable->sPVRContextHead)
   {
      PVRDRIContext *psPVRContext = PVRQ_CONTAINER_OF(psQElem, PVRDRIContext, sQElem);
      PVRDRIEGLMarkRendersurfaceInvalid(psPVRContext->eAPI,
            psPVRContext->psPVRScreen->psImpl,
            psPVRContext->psImpl);
      psQElem = psPVRContext->sQElem.pvForw;
   }

   /* No need to flush surfaces evicted from the cache */
   INITIALISE_PVRQ_HEAD(&psPVRDrawable->sCacheFlushHead);
}

/*************************************************************************//*!
 PVR drawable local functions (image driver loader)
 *//**************************************************************************/

/*************************************************************************//*!
 Function Name		: PVRImageDrawableGetNativeInfo
 Inputs			: psPVRDrawable
 Returns		: Boolean
 Description		: Update native drawable information.
 *//**************************************************************************/
static bool PVRImageDrawableGetNativeInfo(PVRDRIDrawable *psPVRDrawable)
{
   __DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
   __DRIscreen *psDRIScreen = psPVRDrawable->psPVRScreen->psDRIScreen;
   struct __DRIimageList sImages;
   uint32_t uiBufferMask;
   const PVRDRIImageFormat *psFormat;

   assert(psDRIScreen->image.loader != NULL);
   assert(psDRIScreen->image.loader->getBuffers);

   psFormat = PVRDRIIMGPixelFormatToImageFormat(psPVRDrawable->ePixelFormat);
   if (!psFormat)
   {
      mesa_loge("%s: Unsupported format (format = %u)\n",
            __func__, psPVRDrawable->ePixelFormat);
      return false;
   }

   uiBufferMask = psPVRDrawable->bDoubleBuffered ?
                                                   __DRI_IMAGE_BUFFER_BACK :
                                                   __DRI_IMAGE_BUFFER_FRONT;

   if (!psDRIScreen->image.loader->getBuffers(psDRIDrawable,
         psFormat->iDRIFormat,
         NULL,
         psDRIDrawable->loaderPrivate,
         uiBufferMask,
         &sImages))
         {
      mesa_loge("%s: Image get buffers call failed\n", __func__);
      return false;
   }

   psPVRDrawable->uDRI.sImage.psDRI =
         (sImages.image_mask & __DRI_IMAGE_BUFFER_BACK) ?
                                                          sImages.back :
                                                          sImages.front;

   return true;
}

/*************************************************************************//*!
 Function Name		: PVRImageDrawableCreate
 Inputs			: psPVRDrawable
 Returns		: Boolean
 Description		: Create drawable
 *//**************************************************************************/
static bool PVRImageDrawableCreate(PVRDRIDrawable *psPVRDrawable)
{
   __DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
   uint32_t uBytesPerPixel;
   PVRDRIBufferAttribs sBufferAttribs;

   if (!PVRImageDrawableGetNativeInfo(psPVRDrawable))
         {
      return false;
   }

   PVRDRIEGLImageGetAttribs(
         PVRDRIImageGetEGLImage(psPVRDrawable->uDRI.sImage.psDRI),
         &sBufferAttribs);
   uBytesPerPixel = PVRDRIPixFmtGetBlockSize(sBufferAttribs.ePixFormat);

   psDRIDrawable->w = sBufferAttribs.uiWidth;
   psDRIDrawable->h = sBufferAttribs.uiHeight;
   psPVRDrawable->uStride = sBufferAttribs.uiStrideInBytes;
   psPVRDrawable->uBytesPerPixel = uBytesPerPixel;

   if (!PVRObjectCacheInsert(psPVRDrawable->hBufferCache,
         psPVRDrawable->uDRI.sImage.psDRI))
         {
      mesa_loge("%s: Couldn't insert buffer into cache\n", __func__);
      return false;
   }

   if (!PVREGLDrawableCreate(psPVRDrawable->psPVRScreen->psImpl,
         psPVRDrawable->psImpl))
         {
      mesa_loge("%s: Couldn't create EGL drawable\n", __func__);
      return false;
   }

   return true;
}

/*************************************************************************//*!
 Function Name		: PVRImageDrawableRecreate
 Inputs			: psPVRDrawable
 Returns		: Boolean
 Description		: Recreate drawable, if necessary.
 *//**************************************************************************/
static bool PVRImageDrawableRecreate(PVRDRIDrawable *psPVRDrawable)
{
   __DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
   PVRDRIBuffer *psPVRDRIBuffer;
   uint32_t uBytesPerPixel;
   PVRDRIBufferAttribs sBufferAttribs;
   bool bRecreate;

   psPVRDRIBuffer = (psPVRDrawable->bDoubleBuffered) ? NULL :
                                                       PVRObjectCacheGetObject(psPVRDrawable->hBufferCache, 0);

   PVRDRIEGLImageGetAttribs(
         PVRDRIImageGetEGLImage(psPVRDrawable->uDRI.sImage.psDRI),
         &sBufferAttribs);
   uBytesPerPixel = PVRDRIPixFmtGetBlockSize(sBufferAttribs.ePixFormat);

   bRecreate = (psPVRDRIBuffer &&
         psPVRDRIBuffer->uBacking.sImage.psImage !=
               psPVRDrawable->uDRI.sImage.psDRI) ||
         (psDRIDrawable->w != sBufferAttribs.uiWidth) ||
         (psDRIDrawable->h != sBufferAttribs.uiHeight) ||
         (psPVRDrawable->uStride !=
               sBufferAttribs.uiStrideInBytes) ||
         (psPVRDrawable->uBytesPerPixel != uBytesPerPixel);

   if (bRecreate)
   {
      PVRDRIMarkAllRenderSurfacesAsInvalid(psPVRDrawable);
      PVRObjectCachePurge(psPVRDrawable->hBufferCache);

      psDRIDrawable->w = sBufferAttribs.uiWidth;
      psDRIDrawable->h = sBufferAttribs.uiHeight;
      psPVRDrawable->uStride = sBufferAttribs.uiStrideInBytes;
      psPVRDrawable->uBytesPerPixel = uBytesPerPixel;
   }

   if (!PVRObjectCacheInsert(psPVRDrawable->hBufferCache,
         psPVRDrawable->uDRI.sImage.psDRI))
         {
      mesa_loge("%s: Couldn't insert buffer into cache\n", __func__);
      return false;
   }

   if (bRecreate)
   {
      if (!PVREGLDrawableRecreate(psPVRDrawable->psPVRScreen->psImpl,
            psPVRDrawable->psImpl))
            {
         mesa_loge("%s: Couldn't recreate EGL drawable\n", __func__);
         return false;
      }
   }

   return true;
}

static void* PVRImageObjectCacheInsert(void *pvCreateData, void *pvInsertData)
{
   __DRIimage *psImage = pvInsertData;
   PVRDRIBuffer *psPVRDRIBuffer;
   (void) pvCreateData;

   assert(PVRDRIImageGetSharedBuffer(psImage) != NULL);

   psPVRDRIBuffer = calloc(1, sizeof(*psPVRDRIBuffer));
   if (psPVRDRIBuffer == NULL)
   {
      mesa_loge("%s: Failed to create PVR DRI buffer", __func__);
      return NULL;
   }

   psPVRDRIBuffer->eBackingType = PVRDRI_BUFFER_BACKING_IMAGE;
   psPVRDRIBuffer->uBacking.sImage.psImage = psImage;

   /* As a precaution, take a reference on the image so it doesn't disappear unexpectedly */
   PVRDRIRefImage(psImage);

   return psPVRDRIBuffer;
}

static void PVRImageObjectCachePurge(void *pvCreateData,
      void *pvObjectData,
      bool bRetired)
{
   PVRDRIDrawable *psPVRDrawable = pvCreateData;
   PVRDRIBuffer *psPVRDRIBuffer = pvObjectData;

   if (bRetired)
   {
      /*
       * Delay flush until later, as it may not be safe
       * to do the flush within GetDrawableInfo.
       */
      PVRQQueue(&psPVRDrawable->sCacheFlushHead, &psPVRDRIBuffer->sCacheFlushElem);
   }
   else
   {
      PVRDRIUnrefImage(psPVRDRIBuffer->uBacking.sImage.psImage);
      free(psPVRDRIBuffer);
   }
}

static bool PVRImageObjectCacheCompare(void *pvCreateData,
      void *pvObjectData,
      void *pvInsertData)
{
   __DRIimage *psImage = pvInsertData;
   PVRDRIBuffer *psPVRDRIBuffer = pvObjectData;
   (void) pvCreateData;

   return psPVRDRIBuffer->uBacking.sImage.psImage == psImage;
}

/*************************************************************************//*!
 PVR drawable local functions (DRI2 loader)
 *//**************************************************************************/

/***********************************************************************************
 Function Name		: PVRDRI2DrawableGetNativeInfo
 Inputs			: psPVRDrawable
 Returns		: Boolean
 Description		: Update native drawable information.
 ************************************************************************************/
static bool PVRDRI2DrawableGetNativeInfo(PVRDRIDrawable *psPVRDrawable)
{
   __DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
   __DRIscreen *psDRIScreen = psPVRDrawable->psPVRScreen->psDRIScreen;
   unsigned int auiAttachmentReq[2];
   __DRIbuffer *psDRIBuffers;
   int iBufCount;
   int i;
   int w, h;

   assert(psDRIScreen->dri2.loader);
   assert(psDRIScreen->dri2.loader->getBuffersWithFormat);

   if (psPVRDrawable->bDoubleBuffered)
   {
      auiAttachmentReq[0] = __DRI_BUFFER_BACK_LEFT;
   }
   else
   {
      auiAttachmentReq[0] = __DRI_BUFFER_FRONT_LEFT;
   }

   auiAttachmentReq[1] = PVRDRIPixFmtGetDepth(psPVRDrawable->ePixelFormat);

   /* Do not free psDRIBuffers when finished with it */
   psDRIBuffers = psDRIScreen->dri2.loader->getBuffersWithFormat(psDRIDrawable,
         &w,
         &h,
         &auiAttachmentReq[0],
         1,
         &iBufCount,
         psDRIDrawable->loaderPrivate);
   if (psDRIBuffers == NULL)
   {
      mesa_loge("%s: DRI2 get buffers call failed\n", __func__);
      return false;
   }

   for (i = 0; i < iBufCount; i++)
         {
      if (psDRIBuffers[i].attachment == auiAttachmentReq[0] ||
            (psDRIBuffers[i].attachment == __DRI_BUFFER_FAKE_FRONT_LEFT &&
                  auiAttachmentReq[0] == __DRI_BUFFER_FRONT_LEFT))
            {
         break;
      }
   }

   if (i == iBufCount)
         {
      mesa_loge("%s: Couldn't get DRI buffer information\n", __func__);
      return false;
   }

   psPVRDrawable->uDRI.sBuffer.sDRI = psDRIBuffers[i];
   psPVRDrawable->uDRI.sBuffer.w = w;
   psPVRDrawable->uDRI.sBuffer.h = h;

   return true;
}

/***********************************************************************************
 Function Name		: PVRDRI2DrawableCreate
 Inputs			: psPVRDrawable
 Returns		: Boolean
 Description		: Create drawable.
 ************************************************************************************/
static bool PVRDRI2DrawableCreate(PVRDRIDrawable *psPVRDrawable)
{
   __DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;

   if (!PVRDRI2DrawableGetNativeInfo(psPVRDrawable))
         {
      return false;
   }

   psDRIDrawable->w = psPVRDrawable->uDRI.sBuffer.w;
   psDRIDrawable->h = psPVRDrawable->uDRI.sBuffer.h;
   psPVRDrawable->uStride = psPVRDrawable->uDRI.sBuffer.sDRI.pitch;
   psPVRDrawable->uBytesPerPixel = psPVRDrawable->uDRI.sBuffer.sDRI.cpp;

   if (!PVRObjectCacheInsert(psPVRDrawable->hBufferCache, &psPVRDrawable->uDRI.sBuffer.sDRI))
         {
      mesa_loge("%s: Couldn't insert buffer into cache\n", __func__);
      return false;
   }

   if (!PVREGLDrawableCreate(psPVRDrawable->psPVRScreen->psImpl,
         psPVRDrawable->psImpl))
         {
      mesa_loge("%s: Couldn't create EGL drawable\n", __func__);
      return false;
   }

   return true;
}

/***********************************************************************************
 Function Name		: PVRDRI2DrawableRecreate
 Inputs			: psPVRDrawable
 Returns		: Boolean
 Description		: Recreate a drawable, if necessary.
 ************************************************************************************/
static bool PVRDRI2DrawableRecreate(PVRDRIDrawable *psPVRDrawable)
{
   __DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
   PVRDRIBuffer *psPVRDRIBuffer;
   bool bRecreate;

   /*
    * If we're single buffered, check the name of the first buffer
    * in the cache (there should only be one) against the new name of
    * the new DRI buffer. Otherwise don't, as the name will change as
    * part of swap buffers.
    */
   psPVRDRIBuffer = (psPVRDrawable->bDoubleBuffered) ? NULL :
                                                       PVRObjectCacheGetObject(psPVRDrawable->hBufferCache, 0);

   bRecreate = (psPVRDRIBuffer &&
         psPVRDRIBuffer->uBacking.sDRI2.uiName !=
               psPVRDrawable->uDRI.sBuffer.sDRI.name) ||
         (psDRIDrawable->w != psPVRDrawable->uDRI.sBuffer.w) ||
         (psDRIDrawable->h != psPVRDrawable->uDRI.sBuffer.h) ||
         (psPVRDrawable->uStride !=
               psPVRDrawable->uDRI.sBuffer.sDRI.pitch) ||
         (psPVRDrawable->uBytesPerPixel !=
               psPVRDrawable->uDRI.sBuffer.sDRI.cpp);

   if (bRecreate)
   {
      PVRDRIMarkAllRenderSurfacesAsInvalid(psPVRDrawable);
      PVRObjectCachePurge(psPVRDrawable->hBufferCache);

      psDRIDrawable->w = psPVRDrawable->uDRI.sBuffer.w;
      psDRIDrawable->h = psPVRDrawable->uDRI.sBuffer.h;
      psPVRDrawable->uStride =
            psPVRDrawable->uDRI.sBuffer.sDRI.pitch;
      psPVRDrawable->uBytesPerPixel =
            psPVRDrawable->uDRI.sBuffer.sDRI.cpp;
   }

   if (!PVRObjectCacheInsert(psPVRDrawable->hBufferCache,
         &psPVRDrawable->uDRI.sBuffer.sDRI))
         {
      mesa_loge("%s: Couldn't insert buffer into cache\n",
            __func__);
      return false;
   }

   if (bRecreate)
   {
      if (!PVREGLDrawableRecreate(psPVRDrawable->psPVRScreen->psImpl,
            psPVRDrawable->psImpl))
            {
         mesa_loge("%s: Couldn't recreate EGL drawable\n",
               __func__);
         return false;
      }
   }

   return true;
}

static void* PVRDRI2ObjectCacheInsert(void *pvCreateData, void *pvInsertData)
{
   PVRDRIDrawable *psPVRDrawable = pvCreateData;
   __DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
   PVRDRIScreen *psPVRScreen = psPVRDrawable->psPVRScreen;
   PVRDRIBuffer *psPVRDRIBuffer;
   __DRIbuffer *psDRIBuffer = pvInsertData;

   if (PVRDRIPixFmtGetBlockSize(psPVRDrawable->ePixelFormat) != psDRIBuffer->cpp)
         {
      mesa_loge("%s: DRI buffer format doesn't match drawable format\n", __func__);
      return NULL;
   }

   psPVRDRIBuffer = calloc(1, sizeof(*psPVRDRIBuffer));
   if (psPVRDRIBuffer == NULL)
   {
      mesa_loge("%s: Failed to create PVR DRI buffer", __func__);
      return NULL;
   }

   psPVRDRIBuffer->eBackingType = PVRDRI_BUFFER_BACKING_DRI2;
   psPVRDRIBuffer->uBacking.sDRI2.uiName = psDRIBuffer->name;
   psPVRDRIBuffer->uBacking.sDRI2.psBuffer =
         PVRDRIBufferCreateFromName(psPVRScreen->psImpl,
               psDRIBuffer->name,
               psDRIDrawable->w,
               psDRIDrawable->h,
               psDRIBuffer->pitch,
               0);
   if (!psPVRDRIBuffer->uBacking.sDRI2.psBuffer)
   {
      free(psPVRDRIBuffer);
      return NULL;
   }

   return psPVRDRIBuffer;
}

static void PVRDRI2ObjectCachePurge(void *pvCreateData,
      void *pvObjectData,
      bool bRetired)
{
   PVRDRIDrawable *psPVRDrawable = pvCreateData;
   PVRDRIBuffer *psPVRDRIBuffer = pvObjectData;

   if (bRetired)
   {
      /*
       * Delay flush until later, as it may not be safe
       * to do the flush within GetDrawableInfo.
       */
      PVRQQueue(&psPVRDrawable->sCacheFlushHead, &psPVRDRIBuffer->sCacheFlushElem);
   }
   else
   {
      PVRDRIBufferDestroy(psPVRDRIBuffer->uBacking.sDRI2.psBuffer);
      free(psPVRDRIBuffer);
   }
}

static bool PVRDRI2ObjectCacheCompare(void *pvCreateData,
      void *pvObjectData,
      void *pvInsertData)
{
   __DRIbuffer *psDRIBuffer = pvInsertData;
   PVRDRIBuffer *psPVRDRIBuffer = pvObjectData;
   (void) pvCreateData;

   return psDRIBuffer->name == psPVRDRIBuffer->uBacking.sDRI2.uiName;
}

/*************************************************************************//*!
 PVR drawable local functions
 *//**************************************************************************/

/***********************************************************************************
 Function Name		: PVRDRIDrawableUpdateNativeInfo
 Inputs			: psPVRDrawable
 Description		: Update native drawable information.
 ************************************************************************************/
bool PVRDRIDrawableUpdateNativeInfo(PVRDRIDrawable *psPVRDrawable)
{
   return (psPVRDrawable->psPVRScreen->psDRIScreen->image.loader) ?
                                                                    PVRImageDrawableGetNativeInfo(psPVRDrawable) :
                                                                    PVRDRI2DrawableGetNativeInfo(psPVRDrawable);
}

/*************************************************************************//*!
 Function Name	: PVRDRIDrawableRecreate
 Inputs		: psPVRDrawable
 Description	: Create drawable
 *//**************************************************************************/
bool PVRDRIDrawableRecreate(PVRDRIDrawable *psPVRDrawable)
{
   bool bRes;

   PVRDRIDrawableLock(psPVRDrawable);

   if (psPVRDrawable->psPVRScreen->bUseInvalidate)
   {
      if (!psPVRDrawable->bDrawableInfoInvalid)
      {
         PVRDRIDrawableUnlock(psPVRDrawable);
         return true;
      }
   }

   if (!psPVRDrawable->bDrawableInfoUpdated)
   {
      if (!PVRDRIDrawableUpdateNativeInfo(psPVRDrawable))
            {
         PVRDRIDrawableUnlock(psPVRDrawable);
         return false;
      }
   }

   if (psPVRDrawable->psPVRScreen->psDRIScreen->image.loader)
   {
      bRes = PVRImageDrawableRecreate(psPVRDrawable);
   }
   else
   {
      bRes = PVRDRI2DrawableRecreate(psPVRDrawable);
   }

   if (bRes)
   {
      psPVRDrawable->bDrawableInfoUpdated = false;
      psPVRDrawable->bDrawableInfoInvalid = false;
   }

   PVRDRIDrawableUnlock(psPVRDrawable);

   return bRes;
}

/*************************************************************************//*!
 Function Name	: PVRDRIDrawableCreate
 Inputs		: psPVRDrawable
 Description	: Create drawable
 *//**************************************************************************/
static bool PVRDRIDrawableCreate(PVRDRIDrawable *psPVRDrawable)
{
   bool bRes;

   if (psPVRDrawable->psPVRScreen->psDRIScreen->image.loader)
   {
      bRes = PVRImageDrawableCreate(psPVRDrawable);
   }
   else
   {
      bRes = PVRDRI2DrawableCreate(psPVRDrawable);
   }

   return bRes;
}

/*************************************************************************//*!
 PVR drawable interface
 *//**************************************************************************/
bool PVRDRIDrawableInit(PVRDRIDrawable *psPVRDrawable)
{
   unsigned uNumBufs = psPVRDrawable->bDoubleBuffered ? DRI2_BUFFERS_MAX : 1;
   PVRObjectCacheInsertCB pfnInsert;
   PVRObjectCachePurgeCB pfnPurge;
   PVRObjectCacheCompareCB pfnCompare;

   if (psPVRDrawable->bInitialised)
   {
      return true;
   }

   if (psPVRDrawable->psPVRScreen->psDRIScreen->image.loader)
   {
      pfnInsert = PVRImageObjectCacheInsert;
      pfnPurge = PVRImageObjectCachePurge;
      pfnCompare = PVRImageObjectCacheCompare;
   }
   else
   {
      assert(psPVRDrawable->psPVRScreen->psDRIScreen->dri2.loader);

      pfnInsert = PVRDRI2ObjectCacheInsert;
      pfnPurge = PVRDRI2ObjectCachePurge;
      pfnCompare = PVRDRI2ObjectCacheCompare;
   }

   psPVRDrawable->hBufferCache = PVRObjectCacheCreate(uNumBufs,
         uNumBufs,
         psPVRDrawable,
         pfnInsert,
         pfnPurge,
         pfnCompare);
   if (psPVRDrawable->hBufferCache == NULL)
   {
      mesa_loge("%s: Failed to create buffer cache\n", __func__);
      return false;
   }

   if (!PVRDRIDrawableCreate(psPVRDrawable))
         {
      PVRObjectCacheDestroy(psPVRDrawable->hBufferCache);
      psPVRDrawable->hBufferCache = NULL;

      return false;
   }

   psPVRDrawable->bInitialised = true;

   return true;
}

void PVRDRIDrawableDeinit(PVRDRIDrawable *psPVRDrawable)
{
   (void) PVREGLDrawableDestroy(psPVRDrawable->psPVRScreen->psImpl,
         psPVRDrawable->psImpl);

   if (psPVRDrawable->hBufferCache != NULL)
   {
      PVRObjectCacheDestroy(psPVRDrawable->hBufferCache);
      psPVRDrawable->hBufferCache = NULL;
   }

   psPVRDrawable->bInitialised = false;
}

bool PVRDRIDrawableGetParameters(PVRDRIDrawable *psPVRDrawable,
      PVRDRIBufferImpl **ppsDstBuffer,
      PVRDRIBufferImpl **ppsAccumBuffer,
      PVRDRIBufferAttribs *psAttribs,
      bool *pbDoubleBuffered)
{
   __DRIdrawable *psDRIDrawable = psPVRDrawable->psDRIDrawable;
   PVRDRIBuffer *psPVRDRIBuffer;
   PVRDRIBufferImpl *psDstBuffer;

   if (ppsDstBuffer || ppsAccumBuffer)
         {
      psPVRDRIBuffer = PVRObjectCacheGetObject(psPVRDrawable->hBufferCache, 0);

      psDstBuffer = PVRGetBackingBuffer(psPVRDRIBuffer);
      if (!psDstBuffer)
      {
         mesa_loge("%s: Couldn't get render buffer from cache\n", __func__);
         return false;
      }
   }

   if (ppsDstBuffer)
   {
      *ppsDstBuffer = psDstBuffer;
   }

   if (ppsAccumBuffer)
   {
      psPVRDRIBuffer = PVRObjectCacheGetObject(psPVRDrawable->hBufferCache, 1);

      *ppsAccumBuffer = PVRGetBackingBuffer(psPVRDRIBuffer);
      if (!*ppsAccumBuffer)
      {
         *ppsAccumBuffer = psDstBuffer;
      }
   }

   if (psAttribs)
   {
      psAttribs->uiWidth = psDRIDrawable->w;
      psAttribs->uiHeight = psDRIDrawable->h;
      psAttribs->ePixFormat = psPVRDrawable->ePixelFormat;
      psAttribs->uiStrideInBytes = psPVRDrawable->uStride;
   }

   if (pbDoubleBuffered)
   {
      *pbDoubleBuffered = psPVRDrawable->bDoubleBuffered;
   }

   return true;
}
