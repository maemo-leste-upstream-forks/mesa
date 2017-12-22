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
#include <stdlib.h>
#include <assert.h>

#include "drm-uapi/drm_fourcc.h"

#include "dri_util.h"
#include "pvrdri.h"

#define MESSAGE_LENGTH_MAX 1024

/*
 * Define before including android/log.h and dlog.h as this is used by these
 * headers.
 */
#define LOG_TAG "PVR-MESA"

#if defined(HAVE_ANDROID_PLATFORM)
#include <android/log.h>
#define err_printf(f, args...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, f, ##args))
#define dbg_printf(f, args...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, f, ##args))
#else
#define err_printf(f, args...) fprintf(stderr, f "\n", ##args)
#define dbg_printf(f, args...) fprintf(stderr, "LibGL: " f "\n", ##args)
#endif /* HAVE_ANDROID_PLATFORM */

/* Standard error message */
void PRINTFLIKE(1, 2)
errorMessage(const char *f, ...)
{
   char message[MESSAGE_LENGTH_MAX];
   va_list args;

   va_start(args, f);
   vsnprintf(message, sizeof message, f, args);
   va_end(args);

   err_printf("%s", message);
}

void PRINTFLIKE(1, 2)
__driUtilMessage(const char *f, ...)
{
   char message[MESSAGE_LENGTH_MAX];
   va_list args;

   /*
    * On Android, always print messages; otherwise, only print if
    * the environment variable LIBGL_DEBUG=verbose.
    */
#if !defined(HAVE_ANDROID_PLATFORM)
   char *ev = getenv("LIBGL_DEBUG");

   if (!ev || strcmp(ev, "verbose") != 0)
      return;
#endif

   va_start(args, f);
   vsnprintf(message, sizeof message, f, args);
   va_end(args);

   dbg_printf("%s", message);
}

mesa_format
PVRDRIMesaFormatToMesaFormat(int pvrdri_mesa_format)
{
   switch (pvrdri_mesa_format) {
   case PVRDRI_MESA_FORMAT_NONE:
      return MESA_FORMAT_NONE;
   case PVRDRI_MESA_FORMAT_B8G8R8A8_UNORM:
      return MESA_FORMAT_B8G8R8A8_UNORM;
   case PVRDRI_MESA_FORMAT_B8G8R8X8_UNORM:
      return MESA_FORMAT_B8G8R8X8_UNORM;
   case PVRDRI_MESA_FORMAT_B5G6R5_UNORM:
      return MESA_FORMAT_B5G6R5_UNORM;
   case PVRDRI_MESA_FORMAT_R8G8B8A8_UNORM:
      return MESA_FORMAT_R8G8B8A8_UNORM;
   case PVRDRI_MESA_FORMAT_R8G8B8X8_UNORM:
      return MESA_FORMAT_R8G8B8X8_UNORM;
   case PVRDRI_MESA_FORMAT_YCBCR:
      return MESA_FORMAT_YCBCR;
   case PVRDRI_MESA_FORMAT_YUV420_2PLANE:
      return MESA_FORMAT_YUV420_2PLANE;
   case PVRDRI_MESA_FORMAT_YVU420_2PLANE:
      return MESA_FORMAT_YVU420_2PLANE;
   case PVRDRI_MESA_FORMAT_B8G8R8A8_SRGB:
      return MESA_FORMAT_B8G8R8A8_SRGB;
   case PVRDRI_MESA_FORMAT_R8G8B8A8_SRGB:
      return MESA_FORMAT_R8G8B8A8_SRGB;
   case PVRDRI_MESA_FORMAT_YUV420_3PLANE:
         return MESA_FORMAT_YUV420_3PLANE;
   case PVRDRI_MESA_FORMAT_YVU420_3PLANE:
         return MESA_FORMAT_YVU420_3PLANE;
   case PVRDRI_MESA_FORMAT_YCBCR_REV:
      return MESA_FORMAT_YCBCR_REV;
   case PVRDRI_MESA_FORMAT_YVYU:
      return MESA_FORMAT_YVYU;
   case PVRDRI_MESA_FORMAT_VYUY:
      return MESA_FORMAT_VYUY;
   default:
      __driUtilMessage("%s: Unknown format: %d", __func__, pvrdri_mesa_format);
      break;
   }

   return MESA_FORMAT_NONE;
}

int
PVRDRIFormatToFourCC(int dri_format)
{
   switch (dri_format) {
   case __DRI_IMAGE_FORMAT_RGB565:
      return DRM_FORMAT_RGB565;
   case __DRI_IMAGE_FORMAT_XRGB8888:
      return DRM_FORMAT_XRGB8888;
   case __DRI_IMAGE_FORMAT_ARGB8888:
      return DRM_FORMAT_ARGB8888;
   case __DRI_IMAGE_FORMAT_ABGR8888:
      return DRM_FORMAT_ABGR8888;
   case __DRI_IMAGE_FORMAT_XBGR8888:
      return DRM_FORMAT_XBGR8888;
   case __DRI_IMAGE_FORMAT_R8:
      return DRM_FORMAT_R8;
   case __DRI_IMAGE_FORMAT_GR88:
      return DRM_FORMAT_GR88;
   case __DRI_IMAGE_FORMAT_NONE:
      return 0;
   case __DRI_IMAGE_FORMAT_XRGB2101010:
      return DRM_FORMAT_XRGB2101010;
   case __DRI_IMAGE_FORMAT_ARGB2101010:
      return DRM_FORMAT_ARGB2101010;
   case __DRI_IMAGE_FORMAT_SARGB8:
      return __DRI_IMAGE_FOURCC_SARGB8888;
   case __DRI_IMAGE_FORMAT_ARGB1555:
      return DRM_FORMAT_ARGB1555;
   case __DRI_IMAGE_FORMAT_R16:
      return DRM_FORMAT_R16;
   case __DRI_IMAGE_FORMAT_GR1616:
      return DRM_FORMAT_GR1616;
   case __DRI_IMAGE_FORMAT_YUYV:
      return DRM_FORMAT_YUYV;
   case __DRI_IMAGE_FORMAT_XBGR2101010:
      return DRM_FORMAT_XBGR2101010;
   case __DRI_IMAGE_FORMAT_ABGR2101010:
      return DRM_FORMAT_ABGR2101010;
   case __DRI_IMAGE_FORMAT_SABGR8:
      return __DRI_IMAGE_FOURCC_SABGR8888;
   case __DRI_IMAGE_FORMAT_UYVY:
      return DRM_FORMAT_UYVY;
   case __DRI_IMAGE_FORMAT_ARGB4444:
      return DRM_FORMAT_ARGB4444;
   case __DRI_IMAGE_FORMAT_YVU444_PACK10_IMG:
      return DRM_FORMAT_YVU444_PACK10_IMG;
   case __DRI_IMAGE_FORMAT_BGR888:
      return DRM_FORMAT_BGR888;
   case __DRI_IMAGE_FORMAT_AXBXGXRX106106106106:
      return DRM_FORMAT_AXBXGXRX106106106106;
   case __DRI_IMAGE_FORMAT_NV12:
      return DRM_FORMAT_NV12;
   case __DRI_IMAGE_FORMAT_NV21:
      return DRM_FORMAT_NV21;
   case __DRI_IMAGE_FORMAT_YU12:
      return DRM_FORMAT_YUV420;
   case __DRI_IMAGE_FORMAT_YV12:
      return DRM_FORMAT_YVU420;
   case __DRI_IMAGE_FORMAT_YVYU:
      return DRM_FORMAT_YVYU;
   case __DRI_IMAGE_FORMAT_VYUY:
      return DRM_FORMAT_VYUY;
   default:
      __driUtilMessage("%s: Unknown format: %d", __func__, dri_format);
      break;
   }

   return 0;
}

int
PVRDRIFourCCToDRIFormat(int iFourCC)
{
   switch (iFourCC) {
   case 0:
      return __DRI_IMAGE_FORMAT_NONE;
   case DRM_FORMAT_RGB565:
      return __DRI_IMAGE_FORMAT_RGB565;
   case DRM_FORMAT_XRGB8888:
      return __DRI_IMAGE_FORMAT_XRGB8888;
   case DRM_FORMAT_ARGB8888:
      return __DRI_IMAGE_FORMAT_ARGB8888;
   case DRM_FORMAT_ABGR8888:
      return __DRI_IMAGE_FORMAT_ABGR8888;
   case DRM_FORMAT_XBGR8888:
      return __DRI_IMAGE_FORMAT_XBGR8888;
   case DRM_FORMAT_R8:
      return __DRI_IMAGE_FORMAT_R8;
   case DRM_FORMAT_GR88:
      return __DRI_IMAGE_FORMAT_GR88;
   case DRM_FORMAT_XRGB2101010:
      return __DRI_IMAGE_FORMAT_XRGB2101010;
   case DRM_FORMAT_ARGB2101010:
      return __DRI_IMAGE_FORMAT_ARGB2101010;
   case __DRI_IMAGE_FOURCC_SARGB8888:
      return __DRI_IMAGE_FORMAT_SARGB8;
   case DRM_FORMAT_ARGB1555:
      return __DRI_IMAGE_FORMAT_ARGB1555;
   case DRM_FORMAT_R16:
      return __DRI_IMAGE_FORMAT_R16;
   case DRM_FORMAT_GR1616:
      return __DRI_IMAGE_FORMAT_GR1616;
   case DRM_FORMAT_YUYV:
      return __DRI_IMAGE_FORMAT_YUYV;
   case DRM_FORMAT_XBGR2101010:
      return __DRI_IMAGE_FORMAT_XBGR2101010;
   case DRM_FORMAT_ABGR2101010:
      return __DRI_IMAGE_FORMAT_ABGR2101010;
   case __DRI_IMAGE_FOURCC_SABGR8888:
      return __DRI_IMAGE_FORMAT_SABGR8;
   case DRM_FORMAT_UYVY:
      return __DRI_IMAGE_FORMAT_UYVY;
   case DRM_FORMAT_ARGB4444:
      return __DRI_IMAGE_FORMAT_ARGB4444;
   case DRM_FORMAT_YVU444_PACK10_IMG:
      return __DRI_IMAGE_FORMAT_YVU444_PACK10_IMG;
   case DRM_FORMAT_BGR888:
      return __DRI_IMAGE_FORMAT_BGR888;
   case DRM_FORMAT_AXBXGXRX106106106106:
      return __DRI_IMAGE_FORMAT_AXBXGXRX106106106106;
   case DRM_FORMAT_NV12:
      return __DRI_IMAGE_FORMAT_NV12;
   case DRM_FORMAT_NV21:
      return __DRI_IMAGE_FORMAT_NV21;
   case DRM_FORMAT_YUV420:
      return __DRI_IMAGE_FORMAT_YU12;
   case DRM_FORMAT_YVU420:
      return __DRI_IMAGE_FORMAT_YV12;
   case DRM_FORMAT_YVYU:
      return __DRI_IMAGE_FORMAT_YVYU;
   case DRM_FORMAT_VYUY:
      return __DRI_IMAGE_FORMAT_VYUY;
   default:
      __driUtilMessage("%s: Unknown format: %d", __func__, iFourCC);
      break;
   }

   return 0;
}
