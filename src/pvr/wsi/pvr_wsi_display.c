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

#include "wsi_common_display.h"

#if defined(VK_USE_PLATFORM_XLIB_XRANDR_EXT)
#include "wsi_common_x11.h"
#endif

#include "pvr_wsi.h"
#include "pvr_mesa_wsi_interface.h"

VkResult
pvr_mesa_wsi_display_get_physical_device_display_properties(struct pvr_mesa_wsi *mwsi,
							    VkPhysicalDevice physicalDevice,
							    uint32_t *pPropertyCount,
							    VkDisplayPropertiesKHR *pProperties)
{
   return wsi_display_get_physical_device_display_properties(physicalDevice,
                                                             &mwsi->wsi,
                                                             pPropertyCount,
                                                             pProperties);
}

VkResult
pvr_mesa_wsi_display_get_physical_device_display_properties2(struct pvr_mesa_wsi *mwsi,
							     VkPhysicalDevice physicalDevice,
							     uint32_t *pPropertyCount,
							     VkDisplayProperties2KHR *pProperties)
{
   return wsi_display_get_physical_device_display_properties2(physicalDevice,
                                                              &mwsi->wsi,
                                                              pPropertyCount,
                                                              pProperties);
}

VkResult
pvr_mesa_wsi_display_get_physical_device_display_plane_properties(struct pvr_mesa_wsi *mwsi,
								  VkPhysicalDevice physicalDevice,
								  uint32_t *pPropertyCount,
								  VkDisplayPlanePropertiesKHR *pProperties)
{
   return wsi_display_get_physical_device_display_plane_properties(physicalDevice,
                                                                   &mwsi->wsi,
                                                                   pPropertyCount,
                                                                   pProperties);
}

VkResult
pvr_mesa_wsi_display_get_physical_device_display_plane_properties2(struct pvr_mesa_wsi *mwsi,
								   VkPhysicalDevice physicalDevice,
								   uint32_t *pPropertyCount,
								   VkDisplayPlaneProperties2KHR *pProperties)
{
   return wsi_display_get_physical_device_display_plane_properties2(physicalDevice,
                                                                    &mwsi->wsi,
                                                                    pPropertyCount,
                                                                    pProperties);
}

VkResult
pvr_mesa_wsi_display_get_display_plane_supported_displays(struct pvr_mesa_wsi *mwsi,
							  VkPhysicalDevice physicalDevice,
							  uint32_t planeIndex,
							  uint32_t *pDisplayCount,
							  VkDisplayKHR *pDisplays)
{
   return wsi_display_get_display_plane_supported_displays(physicalDevice,
                                                           &mwsi->wsi,
                                                           planeIndex,
                                                           pDisplayCount,
                                                           pDisplays);

}

VkResult
pvr_mesa_wsi_display_get_display_mode_properties(struct pvr_mesa_wsi *mwsi,
						 VkPhysicalDevice physicalDevice,
						 VkDisplayKHR display,
						 uint32_t *pPropertyCount,
						 VkDisplayModePropertiesKHR *pProperties)
{
   return wsi_display_get_display_mode_properties(physicalDevice,
                                                  &mwsi->wsi,
                                                  display,
                                                  pPropertyCount,
                                                  pProperties);
}

VkResult
pvr_mesa_wsi_display_get_display_mode_properties2(struct pvr_mesa_wsi *mwsi,
                                                  VkPhysicalDevice physicalDevice,
                                                  VkDisplayKHR display,
                                                  uint32_t *pPropertyCount,
                                                  VkDisplayModeProperties2KHR *pProperties)
{
   return wsi_display_get_display_mode_properties2(physicalDevice,
                                                   &mwsi->wsi,
                                                   display,
                                                   pPropertyCount,
                                                   pProperties);
}

VkResult
pvr_mesa_wsi_display_create_display_mode(struct pvr_mesa_wsi *mwsi,
                                         VkPhysicalDevice physicalDevice,
                                         VkDisplayKHR display,
                                         const VkDisplayModeCreateInfoKHR *pCreateInfo,
                                         const VkAllocationCallbacks *pAllocator,
                                         VkDisplayModeKHR *pMode)
{
   return wsi_display_create_display_mode(physicalDevice,
                                          &mwsi->wsi,
                                          display,
                                          pCreateInfo,
                                          pAllocator,
                                          pMode);
}

VkResult
pvr_mesa_wsi_get_display_plane_capabilities(struct pvr_mesa_wsi *mwsi,
                                            VkPhysicalDevice physicalDevice,
                                            VkDisplayModeKHR modeKhr,
                                            uint32_t planeIndex,
                                            VkDisplayPlaneCapabilitiesKHR *pCapabilities)
{
   return wsi_get_display_plane_capabilities(physicalDevice,
                                             &mwsi->wsi,
                                             modeKhr,
                                             planeIndex,
                                             pCapabilities);
}

VkResult
pvr_mesa_wsi_get_display_plane_capabilities2(struct pvr_mesa_wsi *mwsi,
                                             VkPhysicalDevice physicalDevice,
                                             const VkDisplayPlaneInfo2KHR *pDisplayPlaneInfo,
                                             VkDisplayPlaneCapabilities2KHR *pCapabilities)
{
   return wsi_get_display_plane_capabilities2(physicalDevice,
                                              &mwsi->wsi,
                                              pDisplayPlaneInfo,
                                              pCapabilities);
}

VkResult
pvr_mesa_wsi_create_display_surface(UNUSED struct pvr_mesa_wsi *mwsi,
                                    VkInstance instance,
                                    const VkAllocationCallbacks *pAllocator,
                                    const VkDisplaySurfaceCreateInfoKHR *pCreateInfo,
                                    VkSurfaceKHR *pSurface)
{
   return wsi_create_display_surface(instance,
                                      pAllocator,
                                      pCreateInfo,
                                      pSurface);
}

VkResult
pvr_mesa_wsi_release_display(struct pvr_mesa_wsi *mwsi,
                             VkPhysicalDevice physicalDevice,
                             VkDisplayKHR display)
{
   return wsi_release_display(physicalDevice,
                              &mwsi->wsi,
                              display);
}

#if defined(VK_USE_PLATFORM_XLIB_XRANDR_EXT)
VkResult
pvr_mesa_wsi_acquire_xlib_display(struct pvr_mesa_wsi *mwsi,
                                  VkPhysicalDevice physicalDevice,
                                  void *dpy,
                                  VkDisplayKHR display)
{
   return wsi_acquire_xlib_display(physicalDevice,
                                   &mwsi->wsi,
                                   dpy,
                                   display);
}

VkResult
pvr_mesa_wsi_get_randr_output_display(struct pvr_mesa_wsi *mwsi,
                                      VkPhysicalDevice  physicalDevice,
                                      void *dpy,
                                      uint32_t output,
                                      VkDisplayKHR *pDisplay)
{
   return wsi_get_randr_output_display(physicalDevice,
                                       &mwsi->wsi,
                                       dpy,
                                       output,
                                       pDisplay);
}
#endif

VkResult
pvr_mesa_wsi_display_power_control(struct pvr_mesa_wsi *mwsi,
                                   VkDevice device,
                                   VkDisplayKHR display,
                                   const VkDisplayPowerInfoEXT *pDisplayPowerInfo)
{
   return wsi_display_power_control(device,
                                    &mwsi->wsi,
                                    display,
                                    pDisplayPowerInfo);
}

VkResult
pvr_mesa_wsi_register_device_event(struct pvr_mesa_wsi *mwsi,
                                   VkDevice device,
                                   const VkDeviceEventInfoEXT *pDeviceEventInfo,
                                   const VkAllocationCallbacks *pAllocator,
                                   void **pFence,
                                   int syncFd)
{
   struct vk_sync *fence;
   VkResult ret;

   ret = wsi_register_device_event(device,
                                   &mwsi->wsi,
                                   pDeviceEventInfo,
                                   pAllocator,
                                   pFence ? &fence : NULL,
                                   syncFd);

   if (ret == VK_SUCCESS && pFence != NULL)
	   *pFence = fence;

   return ret;
}

VkResult
pvr_mesa_wsi_register_display_event(struct pvr_mesa_wsi *mwsi,
                                    VkDevice device,
                                    VkDisplayKHR display,
                                    const VkDisplayEventInfoEXT *pDisplayEventInfo,
                                    const VkAllocationCallbacks  *pAllocator,
                                    void **pFence,
                                    int syncFd)
{
   struct vk_sync *fence;
   VkResult ret;

   ret = wsi_register_display_event(device,
                                    &mwsi->wsi,
                                    display,
                                    pDisplayEventInfo,
                                    pAllocator,
                                    pFence ? &fence : NULL,
                                    syncFd);

   if (ret == VK_SUCCESS && pFence != NULL)
	   *pFence = fence;

   return ret;
}

VkResult
pvr_mesa_wsi_get_swapchain_counter(struct pvr_mesa_wsi *mwsi,
                                   VkDevice device,
                                   VkSwapchainKHR swapchain,
                                   VkSurfaceCounterFlagBitsEXT flagBits,
                                   uint64_t *pValue)
{
   return wsi_get_swapchain_counter(device,
                                    &mwsi->wsi,
                                    swapchain,
                                    flagBits,
                                    pValue);
}

