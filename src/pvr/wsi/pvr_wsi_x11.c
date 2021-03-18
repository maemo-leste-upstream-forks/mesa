/*
 * Copyright © Imagination Technologies Ltd.
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

#include "wsi_common_x11.h"

#include "pvr_wsi.h"
#include "pvr_mesa_wsi_interface.h"

VkBool32
pvr_mesa_wsi_get_physical_device_xcb_presentation_support(struct pvr_mesa_wsi *mwsi,
                                                          uint32_t queueFamilyIndex,
                                                          void *connection,
                                                          uint32_t visual_id)
{
   return wsi_get_physical_device_xcb_presentation_support(&mwsi->wsi,
                                                           queueFamilyIndex,
                                                           connection,
                                                           visual_id);
}

VkResult
pvr_mesa_wsi_create_xcb_surface(UNUSED struct pvr_mesa_wsi *mwsi,
                                const VkAllocationCallbacks *pAllocator,
                                const VkXcbSurfaceCreateInfoKHR *pCreateInfo,
                                VkSurfaceKHR *pSurface)
{
   return wsi_create_xcb_surface(pAllocator,
				 pCreateInfo,
				 pSurface);
}

VkResult
pvr_mesa_wsi_create_xlib_surface(UNUSED struct pvr_mesa_wsi *mwsi,
                                 const VkAllocationCallbacks *pAllocator,
                                 const VkXlibSurfaceCreateInfoKHR *pCreateInfo,
                                 VkSurfaceKHR *pSurface)
{
   return wsi_create_xlib_surface(pAllocator,
				  pCreateInfo,
				  pSurface);
}
