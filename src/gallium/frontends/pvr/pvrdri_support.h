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

#if !defined(__PVRDRI_SUPPORT_H__)
#define __PVRDRI_SUPPORT_H__

#include <stdint.h>
#include <stdbool.h>

#include "dri_support.h"

struct DRISUPScreen *DRISUPCreateScreen(struct __DRIscreenRec *psDRIScreen,
                                        int iFD, bool bUseInvalidate,
                                        void *pvLoaderPrivate,
                                        const struct __DRIconfigRec ***pppsConfigs,
                                        int *piMaxGLES1Version,
                                        int *piMaxGLES2Version);
void DRISUPDestroyScreen(struct DRISUPScreen *psDRISUPScreen);

unsigned int DRISUPCreateContext(PVRDRIAPIType eAPI,
                                 PVRDRIConfig *psPVRDRIConfig,
                                 struct PVRDRIContextConfig *psCtxConfig,
                                 struct __DRIcontextRec *psDRIContext,
                                 struct DRISUPContext *psDRISUPSharedContext,
                                 struct DRISUPScreen *psDRISUPScreen,
                                 struct DRISUPContext **ppsDRISUPContext);
void DRISUPDestroyContext(struct DRISUPContext *psDRISUPContext);

struct DRISUPDrawable *DRISUPCreateDrawable(struct __DRIdrawableRec *psDRIDrawable,
                                            struct DRISUPScreen *psDRISUPScreen,
                                            void *pvLoaderPrivate,
                                            PVRDRIConfig *psPVRDRIConfig);
void DRISUPDestroyDrawable(struct DRISUPDrawable *psDRISUPDrawable);

bool DRISUPMakeCurrent(struct DRISUPContext *psDRISUPContext,
                       struct DRISUPDrawable *psDRISUPWrite,
                       struct DRISUPDrawable *psDRISUPRead);
bool DRISUPUnbindContext(struct DRISUPContext *psDRISUPContext);

struct DRISUPBuffer *DRISUPAllocateBuffer(struct DRISUPScreen *psDRISUPScreen,
                                          unsigned int uAttchment,
                                          unsigned int uFormat,
                                          int iWidth, int iHeight,
                                          unsigned int *puName,
                                          unsigned int *puPitch,
                                          unsigned int *puCPP,
                                          unsigned int *puFlags);
void DRISUPReleaseBuffer(struct DRISUPScreen *psDRISUPScreen,
                         struct DRISUPBuffer *psDRISUPBuffer);
void DRISUPSetTexBuffer2(struct DRISUPContext *psDRISUPContext,
                         int iTarget, int iFormat,
                         struct DRISUPDrawable *psDRISUPDrawable);
void DRISUPReleaseTexBuffer(struct DRISUPContext *psDRISUPContext,
                            int iTarget,
                            struct DRISUPDrawable *psDRISUPDrawable);

void DRISUPFlush(struct DRISUPDrawable *psDRISUPDrawable);
void DRISUPInvalidate(struct DRISUPDrawable *psDRISUPDrawable);
void DRISUPFlushWithFlags(struct DRISUPContext *psDRISUPContext,
                          struct DRISUPDrawable *psDRISUPDrawable,
                          unsigned int uFlags, unsigned int uThrottleReason);

__DRIimage *DRISUPCreateImageFromName(struct DRISUPScreen *psDRISUPScreen,
                                      int iWidth, int iHeight, int iFourCC,
                                      int iName, int iPitch,
                                      void *pvLoaderPrivate);
__DRIimage *DRISUPCreateImageFromRenderbuffer(struct DRISUPContext *psDRISUPContext,
                                              int iRenderBuffer,
                                              void *pvLoaderPrivate);
void DRISUPDestroyImage(__DRIimage *psImage);
__DRIimage *DRISUPCreateImage(struct DRISUPScreen *psDRISUPScreen,
                              int iWidth, int iHeight, int iFourCC,
                              unsigned int uUse, void *pvLoaderPrivate);
bool DRISUPQueryImage(__DRIimage *psImage, int iAttrib, int *piValue);
__DRIimage *DRISUPDupImage(__DRIimage *psImage, void *pvLoaderPrivate);
bool DRISUPValidateImageUsage(__DRIimage *psImage, unsigned int uUse);
__DRIimage *DRISUPCreateImageFromNames(struct DRISUPScreen *psDRISUPScreen,
                                       int iWidth, int iHeight, int iFourCC,
                                       int *piNames, int iNumNames,
                                       int *piStrides, int *piOffsets,
                                       void *pvLoaderPrivate);
__DRIimage *DRISUPFromPlanar(__DRIimage *psImage, int iPlane,
                             void *pvLoaderPrivate);
__DRIimage *DRISUPCreateImageFromTexture(struct DRISUPContext *psDRISUPContext,
                                         int iTarget, unsigned int uTexture,
                                         int iDepth, int iLevel,
                                         unsigned int *puError,
                                         void *pvLoaderPrivate);
__DRIimage *DRISUPCreateImageFromFDs(struct DRISUPScreen *psDRISUPcreen,
                                     int iWidth, int iHeight, int iFourCC,
                                     int *piFDs, int iNumFDs,
                                     int *piStrides, int *piOffsets,
                                     void *pvLoaderPrivate);
__DRIimage *DRISUPCreateImageFromDmaBufs(struct DRISUPScreen *psDRISUPScreen,
                                         int iWidth, int iHeight, int iFourCC,
                                         int *piFDs, int iNumFDs,
                                         int *piStrides, int *piOffsets,
                                         unsigned int uColorSpace,
                                         unsigned int uSampleRange,
                                         unsigned int uHorizSiting,
                                         unsigned int uVertSiting,
                                         unsigned int *puError,
                                         void *pvLoaderPrivate);
int DRISUPGetImageCapabilities(struct DRISUPScreen *psDRISUPScreen);
void DRISUPBlitImage(struct DRISUPContext *psDRISUPContext,
                     __DRIimage *psDst, __DRIimage *psSrc,
                     int iDstX0, int iDstY0, int iDstWidth, int iDstHeight,
                     int iSrcX0, int iSrcY0, int iSrcWidth, int iSrcHeight,
                     int iFlushFlag);
void *DRISUPMapImage(struct DRISUPContext *psDRISUPContext,
                     __DRIimage *psImage,
                     int iX0, int iY0, int iWidth, int iHeight,
                     unsigned int uFlags, int *piStride, void **ppvData);
void DRISUPUnmapImage(struct DRISUPContext *psDRISUPContext,
                      __DRIimage *psImage, void *pvData);
__DRIimage *DRISUPCreateImageWithModifiers(struct DRISUPScreen *psDRISUPScreen,
                                           int iWidth, int iHeight, int iFourCC,
                                           const uint64_t *puModifiers,
                                           const unsigned int uModifierCount,
                                           void *pvLoaderPrivate);
__DRIimage *DRISUPCreateImageFromDMABufs2(struct DRISUPScreen *psDRISUPScreen,
                                          int iWidth, int iHeight,
                                          int iFourCC, uint64_t uModifier,
                                          int *piFDs, int iNumFDs,
                                          int *piStrides, int *piOffsets,
                                          unsigned int uColorSpace,
                                          unsigned int uSampleRange,
                                          unsigned int uHorizSiting,
                                          unsigned int uVertSiting,
                                          unsigned int *puError,
                                          void *pvLoaderPrivate);

bool DRISUPQueryDMABufFormats(struct DRISUPScreen *psDRISUPScreen, int iMax,
                              int *piFormats, int *piCount);
bool DRISUPQueryDMABufModifiers(struct DRISUPScreen *psDRISUPScreen,
                                int iFourCC, int iMax, uint64_t *puModifiers,
                                unsigned int *piExternalOnly, int *piCount);
bool DRISUPQueryDMABufFormatModifierAttribs(struct DRISUPScreen *psDRISUPScreen,
                                            uint32_t uFourcc,
                                            uint64_t uModifier,
                                            int iAttrib, uint64_t *puValue);

__DRIimage *DRISUPCreateImageFromRenderBuffer2(struct DRISUPContext *psDRISUPContext,
                                               int iRenderBuffer,
                                               void *pvLoaderPrivate,
                                               unsigned int *puError);

__DRIimage *DRISUPCreateImageFromBuffer(struct DRISUPContext *psDRISUPContext,
                                        int iTarget, void *pvBuffer,
                                        unsigned int *puError,
                                        void *pvLoaderPrivate);

int DRISUPQueryRendererInteger(struct DRISUPScreen *psDRISUPScreen,
                               int iAttribute, unsigned int *puValue);
int DRISUPQueryRendererString(struct DRISUPScreen *psDRISUPScreen,
                              int iAttribute, const char **ppszValue);

void *DRISUPCreateFence(struct DRISUPContext *psDRISUPContext);
void DRISUPDestroyFence(struct DRISUPScreen *psDRISUPScreen, void *pvFence);
bool DRISUPClientWaitSync(struct DRISUPContext *psDRISUPContext,
                          void *pvFence, unsigned int uFlags,
                          uint64_t uTimeout);
void DRISUPServerWaitSync(struct DRISUPContext *psDRISUPContext,
                          void *pvFence, unsigned int uFlags);
unsigned int DRISUPGetFenceCapabilities(struct DRISUPScreen *psDRISUPScreen);
void *DRISUPCreateFenceFD(struct DRISUPContext *psDRISUPContext, int iFD);
int DRISUPGetFenceFD(struct DRISUPScreen *psDRISUPScreen, void *pvFence);
void *DRISUPGetFenceFromCLEvent(struct DRISUPScreen *psDRISUPScreen,
                                intptr_t iCLEvent);
int DRISUPGetAPIVersion(struct DRISUPScreen *psDRISUPScreen,
                        PVRDRIAPIType eAPI);

unsigned int DRISUPGetNumAPIProcs(struct DRISUPScreen *psDRISUPScreen,
                                  PVRDRIAPIType eAPI);
const char *DRISUPGetAPIProcName(struct DRISUPScreen *psDRISUPScreen,
                                 PVRDRIAPIType eAPI, unsigned int uIndex);
void *DRISUPGetAPIProcAddress(struct DRISUPScreen *psDRISUPScreen,
                              PVRDRIAPIType eAPI, unsigned int uIndex);

void DRISUPSetDamageRegion(struct DRISUPDrawable *psDRISUPDrawable,
                           unsigned int uNRects, int *piRects);

bool DRISUPHaveGetFenceFromCLEvent(void);

__DRIimage *DRISUPCreateImageFromDMABufs3(struct DRISUPScreen *psDRISUPScreen,
                                          int iWidth, int iHeight,
                                          int iFourCC, uint64_t uModifier,
                                          int *piFDs, int iNumFDs,
                                          int *piStrides, int *piOffsets,
                                          unsigned int uColorSpace,
                                          unsigned int uSampleRange,
                                          unsigned int uHorizSiting,
                                          unsigned int uVertSiting,
                                          uint32_t uFlags,
                                          unsigned int *puError,
                                          void *pvLoaderPrivate);
__DRIimage *DRISUPCreateImageWithModifiers2(struct DRISUPScreen *psDRISUPScreen,
                                            int iWidth, int iHeight,
                                            int iFourCC,
                                            const uint64_t *puModifiers,
                                            const unsigned int uModifierCount,
                                            unsigned int uUse,
                                            void *pvLoaderPrivate);
__DRIimage *DRISUPCreateImageFromFDs2(struct DRISUPScreen *psDRISUPcreen,
                                      int iWidth, int iHeight, int iFourCC,
                                      int *piFDs, int iNumFDs, uint32_t uFlags,
                                      int *piStrides, int *piOffsets,
                                      void *pvLoaderPrivate);

bool DRISUPHaveSetInFenceFd(void);
void DRISUPSetInFenceFd(__DRIimage *psImage, int iFd);
#endif /* defined(__PVRDRI_SUPPORT_H__) */
