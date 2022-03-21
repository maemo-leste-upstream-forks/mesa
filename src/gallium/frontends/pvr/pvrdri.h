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
#include <stdint.h>
#include <inttypes.h>

#include <glapi/glapi.h>

#include "main/mtypes.h"
#include "util/macros.h"
#include "dri_util.h"
#include "pvrdri_support.h"

struct PVRDRIConfigRec {
   struct gl_config sGLMode;
   int iSupportedAPIs;
};

/* PVR screen data */
typedef struct PVRDRIScreen_TAG {
   /* DRI screen structure pointer */
   __DRIscreen *psDRIScreen;

   /* Opaque PVR DRI Support screen structure pointer */
   struct DRISUPScreen *psDRISUPScreen;

   /* Reference count */
   int iRefCount;

#if defined(DEBUG)
   /* Counters of outstanding allocations */
   int iContextAlloc;
   int iDrawableAlloc;
   int iBufferAlloc;
#endif

   /* PVR OGLES 1 dispatch table */
   struct _glapi_table *psOGLES1Dispatch;
   /* PVR OGLES 2/3 dispatch table */
   struct _glapi_table *psOGLES2Dispatch;
   /* PVR OGL dispatch table */
   struct _glapi_table *psOGLDispatch;
} PVRDRIScreen;

/* PVR context data */
typedef struct PVRDRIContext_TAG {
   /* Pointer to DRI context */
   __DRIcontext *psDRIContext;

   /* Opaque PVR DRI Support context structure pointer */
   struct DRISUPContext *psDRISUPContext;

   /* Pointer to PVRDRIScreen structure */
   PVRDRIScreen *psPVRScreen;

   /* GL config */
   PVRDRIConfig sConfig;

   /* API */
   PVRDRIAPIType eAPI;
} PVRDRIContext;

/* PVR drawable data */
typedef struct PVRDRIDrawable_TAG {
   PVRDRIScreen *psPVRScreen;
   __DRIdrawable *psDRIDrawable;
   int iRefCount;
   PVRDRIConfig sConfig;
   struct DRISUPDrawable *psDRISUPDrawable;
   unsigned int uFourCC;
   unsigned int uDRIFormat;
} PVRDRIDrawable;

/*************************************************************************//*!
 pvrdri.c
 *//**************************************************************************/
void PVRDRIDrawableAddReference(PVRDRIDrawable *psPVRDrawable);
void PVRDRIDrawableRemoveReference(PVRDRIDrawable *psPVRDrawable);

/*************************************************************************//*!
 pvrutil.c
 *//**************************************************************************/

void PRINTFLIKE(1, 2) __driUtilMessage(const char *f, ...);
void PRINTFLIKE(1, 2) errorMessage(const char *f, ...);

mesa_format PVRDRIMesaFormatToMesaFormat(int pvrdri_mesa_format);
int PVRDRIFormatToFourCC(int dri_format);
int PVRDRIFourCCToDRIFormat(int iFourCC);

/*************************************************************************//*!
 pvrext.c
 *//**************************************************************************/

const __DRIextension **PVRDRIScreenExtensions(void);
const __DRIextension *PVRDRIScreenExtensionVersionInfo(void);

void PVRDRIAdjustExtensions(unsigned int uVersion, unsigned int uMinVersion);

/*************************************************************************//*!
 pvrcompat.c
 *//**************************************************************************/

bool PVRDRICompatInit(const struct PVRDRICallbacksV2 *psCallbacksV2,
                      unsigned int uVersionV2, unsigned int uMinVersionV2);
void PVRDRICompatDeinit(void);

bool MODSUPRegisterSupportInterfaceV2(const void *pvInterface,
                                      unsigned int uVersion,
                                      unsigned int uMinVersion);

/*************************************************************************//*!
 pvrcb.c
 *//**************************************************************************/

int MODSUPGetBuffers(struct __DRIdrawableRec *psDRIDrawable,
                     unsigned int uFourCC, uint32_t *puStamp,
                     void *pvLoaderPrivate, uint32_t uBufferMask,
                     struct PVRDRIImageList *psImageList);

bool MODSUPCreateConfigs(struct __DRIconfigRec ***psConfigs,
                         struct __DRIscreenRec *psDRIScreen,
                         int iPVRDRIMesaFormat, const uint8_t *puDepthBits,
                         const uint8_t *puStencilBits,
                         unsigned int uNumDepthStencilBits,
                         const unsigned int *puDBModes,
                         unsigned int uNumDBModes,
                         const uint8_t *puMSAASamples,
                         unsigned int uNumMSAAModes, bool bEnableAccum,
                         bool bColorDepthMatch, bool bMutableRenderBuffer,
                         int iYUVDepthRange, int iYUVCSCStandard,
                         uint32_t uMaxPbufferWidth, uint32_t uMaxPbufferHeight);

struct __DRIconfigRec **MODSUPConcatConfigs(struct __DRIscreenRec *psDRIScreen,
                                            struct __DRIconfigRec **ppsConfigA,
                                            struct __DRIconfigRec **ppsConfigB);

__DRIimage *MODSUPLookupEGLImage(struct __DRIscreenRec *psDRIScreen,
                                 void *pvImage, void *pvLoaderPrivate);

unsigned int MODSUPGetCapability(struct __DRIscreenRec *psDRIScreen,
                                 unsigned int uCapability);

int MODSUPGetDisplayFD(struct __DRIscreenRec *psDRIScreen,
                       void *pvLoaderPrivate);

bool PVRDRIConfigQuery(const PVRDRIConfig *psConfig,
                       PVRDRIConfigAttrib eConfigAttrib, int *piValueOut);

bool MODSUPConfigQuery(const PVRDRIConfig *psConfig,
                       PVRDRIConfigAttrib eConfigAttrib,
                       unsigned int *puValueOut);

void MODSUPFlushFrontBuffer(struct __DRIdrawableRec *psDRIDrawable,
                            void *pvLoaderPrivate);

void *MODSUPDrawableGetReferenceHandle(struct __DRIdrawableRec *psDRIDrawable);

void MODSUPDrawableAddReference(void *pvReferenceHandle);

void MODSUPDrawableRemoveReference(void *pvReferenceHandle);

void MODSUPDestroyLoaderImageState(const struct __DRIscreenRec *psDRIScreen,
                                   void *pvLoaderPrivate);
#endif /* defined(__PVRDRI_H__) */
