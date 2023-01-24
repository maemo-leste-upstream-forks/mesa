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

#include <string.h>

#include "pvr_wsi.h"
#include "pvr_mesa_wsi_interface.h"

static inline uint32_t
pvr_vk_wsi_get_version(struct pvr_mesa_wsi *mwsi)
{
   JUMP_DDK(mwsi, pvr_vk_mesa_wsi_get_version,
                  mwsi->physicalDevice);

   return 0;
}


VkResult
pvr_mesa_wsi_init(struct pvr_mesa_wsi **pmwsi,
                  VkPhysicalDevice physicalDevice,
                  PFN_vkVoidFunction (VKAPI_PTR *pvr_vk_mesa_wsi_sym_addr)
                     (VkPhysicalDevice physicalDevice, const char *),
                  const VkAllocationCallbacks *alloc,
                  int fd,
                  bool sw)
{
   struct pvr_mesa_wsi *mwsi;
   VkResult result;

   mwsi = vk_zalloc(alloc, sizeof(*mwsi), 8,
                    VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!mwsi)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   mwsi->symtab.pvr_vk_mesa_wsi_sym_addr = pvr_vk_mesa_wsi_sym_addr;
   mwsi->physicalDevice = physicalDevice;

   mwsi->pvr_vk_wsi_version = pvr_vk_wsi_get_version(mwsi);
   if (mwsi->pvr_vk_wsi_version < 1) {
      vk_free(alloc, mwsi);
      return VK_ERROR_FEATURE_NOT_PRESENT;
   }

   result = wsi_device_init2(&mwsi->wsi,
                             physicalDevice,
                             pvr_vk_mesa_wsi_sym_addr,
                             alloc,
                             fd, NULL, sw,
                             true, NULL);
   if (result != VK_SUCCESS) {
      vk_free(alloc, mwsi);
      return result;
   }

   if (!sw)
      mwsi->wsi.supports_modifiers = true;

   *pmwsi = mwsi;

   return VK_SUCCESS;
}

void
pvr_mesa_wsi_finish(struct pvr_mesa_wsi *mwsi,
		    const VkAllocationCallbacks *alloc)
{
   wsi_device_finish(&mwsi->wsi, alloc);

   vk_free(alloc, mwsi);
}

VkResult
pvr_mesa_wsi_common_get_surface_support(struct pvr_mesa_wsi *mwsi,
                                        uint32_t queueFamilyIndex,
                                        VkSurfaceKHR surface,
                                        VkBool32 *pSupported)
{
   return wsi_common_get_surface_support(&mwsi->wsi,
                                         queueFamilyIndex,
                                         surface,
                                         pSupported);
}

VkResult
pvr_mesa_wsi_common_get_surface_capabilities(struct pvr_mesa_wsi *mwsi,
                                             VkSurfaceKHR surface,
                                             VkSurfaceCapabilitiesKHR *pSurfaceCapabilities)
{
   return wsi_common_get_surface_capabilities(&mwsi->wsi,
                                              surface,
                                              pSurfaceCapabilities);
}

VkResult
pvr_mesa_wsi_common_get_surface_capabilities2(struct pvr_mesa_wsi *mwsi,
                                              const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                              VkSurfaceCapabilities2KHR *pSurfaceCapabilities)
{
   return wsi_common_get_surface_capabilities2(&mwsi->wsi,
                                               pSurfaceInfo,
                                               pSurfaceCapabilities);
}

VkResult
pvr_mesa_wsi_common_get_surface_capabilities2ext(struct pvr_mesa_wsi *mwsi,
                                                 VkSurfaceKHR surface,
                                                 VkSurfaceCapabilities2EXT *pSurfaceCapabilities)
{
   return wsi_common_get_surface_capabilities2ext(&mwsi->wsi,
                                                  surface,
                                                  pSurfaceCapabilities);
}

VkResult
pvr_mesa_wsi_common_get_surface_formats(struct pvr_mesa_wsi *mwsi,
                                        VkSurfaceKHR surface,
                                        uint32_t *pSurfaceFormatCount,
                                        VkSurfaceFormatKHR *pSurfaceFormats)
{
   return wsi_common_get_surface_formats(&mwsi->wsi,
                                         surface,
                                         pSurfaceFormatCount,
                                         pSurfaceFormats);
}

VkResult
pvr_mesa_wsi_common_get_surface_formats2(struct pvr_mesa_wsi *mwsi,
                                         const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                         uint32_t *pSurfaceFormatCount,
                                         VkSurfaceFormat2KHR *pSurfaceFormats)
{
   return wsi_common_get_surface_formats2(&mwsi->wsi,
                                          pSurfaceInfo,
                                          pSurfaceFormatCount,
                                          pSurfaceFormats);
}

VkResult
pvr_mesa_wsi_common_get_surface_present_modes(struct pvr_mesa_wsi *mwsi,
                                              VkSurfaceKHR surface,
                                              uint32_t *pPresentModeCount,
                                              VkPresentModeKHR *pPresentModes)
{
   return wsi_common_get_surface_present_modes(&mwsi->wsi,
                                               surface,
                                               pPresentModeCount,
                                               pPresentModes);
}

VkResult
pvr_mesa_wsi_common_create_swapchain(struct pvr_mesa_wsi *mwsi,
                                     VkDevice device,
                                     const VkSwapchainCreateInfoKHR *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator,
                                     VkSwapchainKHR *pSwapchain)
{
   return wsi_common_create_swapchain(&mwsi->wsi,
                                      device,
                                      pCreateInfo,
                                      pAllocator,
                                      pSwapchain);
}

void
pvr_mesa_wsi_common_destroy_swapchain(UNUSED struct pvr_mesa_wsi *mwsi,
                                      VkDevice device,
                                      VkSwapchainKHR swapchain,
                                      const VkAllocationCallbacks *pAllocator)
{
   return wsi_common_destroy_swapchain(device,
                                       swapchain,
                                       pAllocator);
}

VkResult
pvr_mesa_wsi_common_get_images(UNUSED struct pvr_mesa_wsi *mwsi,
                               VkSwapchainKHR swapchain,
                               uint32_t *pSwapchainImageCount,
                               VkImage *pSwapchainImages)
{
   return wsi_common_get_images(swapchain,
                                pSwapchainImageCount,
                                pSwapchainImages);
}

VkResult
pvr_mesa_wsi_common_acquire_next_image2(struct pvr_mesa_wsi *mwsi,
                                        VkDevice device,
                                        const VkAcquireNextImageInfoKHR *pAcquireInfo,
                                        uint32_t *pImageIndex)
{
   return wsi_common_acquire_next_image2(&mwsi->wsi,
                                         device,
                                         pAcquireInfo,
                                         pImageIndex);
}

VkResult
pvr_mesa_wsi_common_queue_present(struct pvr_mesa_wsi *mwsi,
                                  VkDevice device,
                                  VkQueue queue,
                                  int queue_family_index,
                                  const VkPresentInfoKHR *pPresentInfo)
{
   return wsi_common_queue_present(&mwsi->wsi,
                                   device,
                                   queue,
                                   queue_family_index,
                                   pPresentInfo);
}

VkResult
pvr_mesa_wsi_common_get_present_rectangles(struct pvr_mesa_wsi *mwsi,
                                           VkSurfaceKHR surface,
                                           uint32_t* pRectCount,
                                           VkRect2D* pRects)
{
   return wsi_common_get_present_rectangles(&mwsi->wsi,
                                            surface,
                                            pRectCount,
                                            pRects);
}

uint32_t
pvr_mesa_wsi_get_version(UNUSED struct pvr_mesa_wsi *mwsi)
{
   return 1;
}

void
pvr_mesa_wsi_surface_destroy(UNUSED struct pvr_mesa_wsi *mwsi,
			     VkSurfaceKHR surface,
			     const VkAllocationCallbacks *pAllocator)
{
   wsi_surface_destroy(surface, pAllocator);
}

/*
 * The mwsi parameter is currently unused. Note that it is invalid for
 * pvr_mesa_wsi_init, which is responsible for allocating it.
*/
PUBLIC void *
pvr_mesa_wsi_sym_addr(UNUSED struct pvr_mesa_wsi *mwsi, const char *name)
{
   static const struct {
      char *name;
      void *addr;
   } lookup[] = {
      { "pvr_mesa_wsi_init",
            pvr_mesa_wsi_init },
      { "pvr_mesa_wsi_finish",
            pvr_mesa_wsi_finish },
      { "pvr_mesa_wsi_common_get_surface_support",
            pvr_mesa_wsi_common_get_surface_support },
      { "pvr_mesa_wsi_common_get_surface_capabilities",
            pvr_mesa_wsi_common_get_surface_capabilities },
      { "pvr_mesa_wsi_common_get_surface_capabilities2",
            pvr_mesa_wsi_common_get_surface_capabilities2 },
      { "pvr_mesa_wsi_common_get_surface_capabilities2ext",
            pvr_mesa_wsi_common_get_surface_capabilities2ext },
      { "pvr_mesa_wsi_common_get_surface_formats",
            pvr_mesa_wsi_common_get_surface_formats },
      { "pvr_mesa_wsi_common_get_surface_formats2",
            pvr_mesa_wsi_common_get_surface_formats2 },
      { "pvr_mesa_wsi_common_get_surface_present_modes",
            pvr_mesa_wsi_common_get_surface_present_modes },
      { "pvr_mesa_wsi_common_create_swapchain",
            pvr_mesa_wsi_common_create_swapchain },
      { "pvr_mesa_wsi_common_destroy_swapchain",
            pvr_mesa_wsi_common_destroy_swapchain },
      { "pvr_mesa_wsi_common_get_images",
            pvr_mesa_wsi_common_get_images },
      { "pvr_mesa_wsi_common_acquire_next_image2",
            pvr_mesa_wsi_common_acquire_next_image2 },
      { "pvr_mesa_wsi_common_queue_present",
            pvr_mesa_wsi_common_queue_present },
      { "pvr_mesa_wsi_common_get_present_rectangles",
            pvr_mesa_wsi_common_get_present_rectangles },
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
      { "pvr_mesa_wsi_get_physical_device_wayland_presentation_support",
            pvr_mesa_wsi_get_physical_device_wayland_presentation_support },
      { "pvr_mesa_wsi_create_wayland_surface",
            pvr_mesa_wsi_create_wayland_surface },
#endif
#if defined(VK_USE_PLATFORM_XCB_KHR)
      { "pvr_mesa_wsi_get_physical_device_xcb_presentation_support",
            pvr_mesa_wsi_get_physical_device_xcb_presentation_support },
      { "pvr_mesa_wsi_create_xcb_surface",
            pvr_mesa_wsi_create_xcb_surface },
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
      { "pvr_mesa_wsi_create_xlib_surface",
            pvr_mesa_wsi_create_xlib_surface },
#endif
      { "pvr_mesa_wsi_display_get_physical_device_display_properties",
            pvr_mesa_wsi_display_get_physical_device_display_properties },
      { "pvr_mesa_wsi_display_get_physical_device_display_properties2",
            pvr_mesa_wsi_display_get_physical_device_display_properties2 },
      { "pvr_mesa_wsi_display_get_physical_device_display_plane_properties",
            pvr_mesa_wsi_display_get_physical_device_display_plane_properties },
      { "pvr_mesa_wsi_display_get_physical_device_display_plane_properties2",
            pvr_mesa_wsi_display_get_physical_device_display_plane_properties2 },
      { "pvr_mesa_wsi_display_get_display_plane_supported_displays",
            pvr_mesa_wsi_display_get_display_plane_supported_displays },
      { "pvr_mesa_wsi_display_get_display_mode_properties",
            pvr_mesa_wsi_display_get_display_mode_properties },
      { "pvr_mesa_wsi_display_get_display_mode_properties2",
            pvr_mesa_wsi_display_get_display_mode_properties2 },
      { "pvr_mesa_wsi_display_create_display_mode",
            pvr_mesa_wsi_display_create_display_mode },
      { "pvr_mesa_wsi_get_display_plane_capabilities",
            pvr_mesa_wsi_get_display_plane_capabilities },
      { "pvr_mesa_wsi_get_display_plane_capabilities2",
            pvr_mesa_wsi_get_display_plane_capabilities2 },
      { "pvr_mesa_wsi_create_display_surface",
            pvr_mesa_wsi_create_display_surface },
      { "pvr_mesa_wsi_release_display",
            pvr_mesa_wsi_release_display },
#if defined(VK_USE_PLATFORM_XLIB_XRANDR_EXT)
      { "pvr_mesa_wsi_acquire_xlib_display",
            pvr_mesa_wsi_acquire_xlib_display },
      { "pvr_mesa_wsi_get_randr_output_display",
            pvr_mesa_wsi_get_randr_output_display },
#endif
      { "pvr_mesa_wsi_display_power_control",
            pvr_mesa_wsi_display_power_control },
      { "pvr_mesa_wsi_register_device_event",
            pvr_mesa_wsi_register_device_event },
      { "pvr_mesa_wsi_register_display_event",
            pvr_mesa_wsi_register_display_event },
      { "pvr_mesa_wsi_get_swapchain_counter",
            pvr_mesa_wsi_get_swapchain_counter },
      { "pvr_mesa_wsi_get_version",
            pvr_mesa_wsi_get_version },
      { "pvr_mesa_wsi_surface_destroy",
            pvr_mesa_wsi_surface_destroy },
   };
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(lookup); i++) {
      if (!strcmp(name, lookup[i].name))
         return lookup[i].addr;
   }

   return NULL;
}
