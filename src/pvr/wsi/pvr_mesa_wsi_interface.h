/*************************************************************************/ /*!
@File
@Title          PVR interface to the Vulkan WSI layer in Mesa
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
*/ /**************************************************************************/

#ifndef PVR_MESA_WSI_INTERFACE_H
#define PVR_MESA_WSI_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#include <vulkan/vulkan.h>

/*
 * The pvr_mesa_wsi structure holds the Mesa WSI state, and is opaque to
 * the PowerVK DDK.
 */
struct pvr_mesa_wsi;

/*
 * Functions defined in Mesa for use by the PowerVR DDK.
 * All functions have a "pvr_mesa_wsi" prefix.
 * Since the introduction of version 1 of the interface, the following
 * functions are now regarded as forming version 0 of the interface.
 */

void *
pvr_mesa_wsi_sym_addr(struct pvr_mesa_wsi *mwsi,
                      const char *name);

VkResult
pvr_mesa_wsi_init(struct pvr_mesa_wsi **mwsi,
                  VkPhysicalDevice physicalDevice,
                  PFN_vkVoidFunction (VKAPI_PTR *pvr_vk_mesa_wsi_sym_addr)
                     (VkPhysicalDevice physicalDevice, const char *),
                  const VkAllocationCallbacks *alloc,
                  int fd,
                  bool sw);

void
pvr_mesa_wsi_finish(struct pvr_mesa_wsi *mwsi,
                    const VkAllocationCallbacks *alloc);

VkResult
pvr_mesa_wsi_common_get_surface_support(struct pvr_mesa_wsi *mwsi,
                                        uint32_t queueFamilyIndex,
                                        VkSurfaceKHR surface,
                                        VkBool32 *pSupported);

VkResult
pvr_mesa_wsi_common_get_surface_capabilities(struct pvr_mesa_wsi *mwsi,
                                             VkSurfaceKHR surface,
                                             VkSurfaceCapabilitiesKHR *pSurfaceCapabilities);

VkResult
pvr_mesa_wsi_common_get_surface_capabilities2(struct pvr_mesa_wsi *mwsi,
                                              const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                              VkSurfaceCapabilities2KHR *pSurfaceCapabilities);

VkResult
pvr_mesa_wsi_common_get_surface_capabilities2ext(struct pvr_mesa_wsi *mwsi,
                                                 VkSurfaceKHR surface,
                                                 VkSurfaceCapabilities2EXT *pSurfaceCapabilities);

VkResult
pvr_mesa_wsi_common_get_surface_formats(struct pvr_mesa_wsi *mwsi,
                                        VkSurfaceKHR surface,
                                        uint32_t *pSurfaceFormatCount,
                                        VkSurfaceFormatKHR *pSurfaceFormats);

VkResult
pvr_mesa_wsi_common_get_surface_formats2(struct pvr_mesa_wsi *mwsi,
                                         const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                         uint32_t *pSurfaceFormatCount,
                                         VkSurfaceFormat2KHR *pSurfaceFormats);

VkResult
pvr_mesa_wsi_common_get_surface_present_modes(struct pvr_mesa_wsi *mwsi,
                                              VkSurfaceKHR surface,
                                              uint32_t *pPresentModeCount,
                                              VkPresentModeKHR *pPresentModes);

VkResult
pvr_mesa_wsi_common_create_swapchain(struct pvr_mesa_wsi *mwsi,
                                     VkDevice device,
                                     const VkSwapchainCreateInfoKHR *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator,
                                     VkSwapchainKHR *pSwapchain);
void
pvr_mesa_wsi_common_destroy_swapchain(struct pvr_mesa_wsi *mwsi,
                                      VkDevice device,
                                      VkSwapchainKHR swapchain,
                                      const VkAllocationCallbacks *pAllocator);

VkResult
pvr_mesa_wsi_common_get_images(struct pvr_mesa_wsi *mwsi,
                               VkSwapchainKHR swapchain,
                               uint32_t *pSwapchainImageCount,
                               VkImage *pSwapchainImages);

VkResult
pvr_mesa_wsi_common_acquire_next_image2(struct pvr_mesa_wsi *mwsi,
                                        VkDevice device,
                                        const VkAcquireNextImageInfoKHR *pAcquireInfo,
                                        uint32_t *pImageIndex);

VkResult
pvr_mesa_wsi_common_queue_present(struct pvr_mesa_wsi *mwsi,
                                  VkDevice device,
                                  VkQueue queue,
                                  int queue_family_index,
                                  const VkPresentInfoKHR *pPresentInfo);

VkResult
pvr_mesa_wsi_common_get_present_rectangles(struct pvr_mesa_wsi *mwsi,
                                           VkSurfaceKHR surface,
                                           uint32_t* pRectCount,
                                           VkRect2D* pRects);

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
VkBool32
pvr_mesa_wsi_get_physical_device_wayland_presentation_support(struct pvr_mesa_wsi *mwsi,
                                                              uint32_t queueFamilyIndex,
                                                              void *display);

VkResult
pvr_mesa_wsi_create_wayland_surface(struct pvr_mesa_wsi *mwsi,
                                    const VkAllocationCallbacks *pAllocator,
                                    const VkWaylandSurfaceCreateInfoKHR *pCreateInfo,
                                    VkSurfaceKHR *pSurface);
#endif

#if defined(VK_USE_PLATFORM_XCB_KHR)
VkBool32
pvr_mesa_wsi_get_physical_device_xcb_presentation_support(struct pvr_mesa_wsi *mwsi,
                                                          uint32_t queueFamilyIndex,
                                                          void *connection,
                                                          uint32_t visualId);
VkResult
pvr_mesa_wsi_create_xcb_surface(struct pvr_mesa_wsi *mwsi,
                                const VkAllocationCallbacks *pAllocator,
                                const VkXcbSurfaceCreateInfoKHR *pCreateInfo,
                                VkSurfaceKHR *pSurface);
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
VkResult
pvr_mesa_wsi_create_xlib_surface(struct pvr_mesa_wsi *mwsi,
                                 const VkAllocationCallbacks *pAllocator,
                                 const VkXlibSurfaceCreateInfoKHR *pCreateInfo,
                                 VkSurfaceKHR *pSurface);
#endif

VkResult
pvr_mesa_wsi_display_get_physical_device_display_properties(struct pvr_mesa_wsi *mwsi,
							    VkPhysicalDevice physicalDevice,
							    uint32_t *pPropertyCount,
							    VkDisplayPropertiesKHR *pProperties);

VkResult
pvr_mesa_wsi_display_get_physical_device_display_properties2(struct pvr_mesa_wsi *mwsi,
							     VkPhysicalDevice physicalDevice,
							     uint32_t *pPropertyCount,
							     VkDisplayProperties2KHR *pProperties);

VkResult
pvr_mesa_wsi_display_get_physical_device_display_plane_properties(struct pvr_mesa_wsi *mwsi,
								  VkPhysicalDevice physicalDevice,
								  uint32_t *pPropertyCount,
								  VkDisplayPlanePropertiesKHR *pProperties);

VkResult
pvr_mesa_wsi_display_get_physical_device_display_plane_properties2(struct pvr_mesa_wsi *mwsi,
								   VkPhysicalDevice physicalDevice,
								   uint32_t *pPropertyCount,
								   VkDisplayPlaneProperties2KHR *pProperties);

VkResult
pvr_mesa_wsi_display_get_display_plane_supported_displays(struct pvr_mesa_wsi *mwsi,
							  VkPhysicalDevice physicalDevice,
							  uint32_t planeIndex,
							  uint32_t *pDisplayCount,
							  VkDisplayKHR *pDisplays);

VkResult
pvr_mesa_wsi_display_get_display_mode_properties(struct pvr_mesa_wsi *mwsi,
						 VkPhysicalDevice physicalDevice,
						 VkDisplayKHR display,
						 uint32_t *pPropertyCount,
						 VkDisplayModePropertiesKHR *pProperties);

VkResult
pvr_mesa_wsi_display_get_display_mode_properties2(struct pvr_mesa_wsi *mwsi,
                                                  VkPhysicalDevice physicalDevice,
                                                  VkDisplayKHR display,
                                                  uint32_t *pPropertyCount,
                                                  VkDisplayModeProperties2KHR *pProperties);

VkResult
pvr_mesa_wsi_display_create_display_mode(struct pvr_mesa_wsi *mwsi,
                                         VkPhysicalDevice physicalDevice,
                                         VkDisplayKHR display,
                                         const VkDisplayModeCreateInfoKHR *pCreateInfo,
                                         const VkAllocationCallbacks *pAllocator,
                                         VkDisplayModeKHR *pMode);

VkResult
pvr_mesa_wsi_get_display_plane_capabilities(struct pvr_mesa_wsi *mwsi,
                                            VkPhysicalDevice physicalDevice,
                                            VkDisplayModeKHR modeKhr,
                                            uint32_t planeIndex,
                                            VkDisplayPlaneCapabilitiesKHR *pCapabilities);

VkResult
pvr_mesa_wsi_get_display_plane_capabilities2(struct pvr_mesa_wsi *mwsi,
                                             VkPhysicalDevice physicalDevice,
                                             const VkDisplayPlaneInfo2KHR *pDisplayPlaneInfo,
                                             VkDisplayPlaneCapabilities2KHR *pCapabilities);

VkResult
pvr_mesa_wsi_create_display_surface(struct pvr_mesa_wsi *mwsi,
                                    VkInstance instance,
                                    const VkAllocationCallbacks *pAllocator,
                                    const VkDisplaySurfaceCreateInfoKHR *pCreateInfo,
                                    VkSurfaceKHR *pSurface);

VkResult
pvr_mesa_wsi_release_display(struct pvr_mesa_wsi *mwsi,
                             VkPhysicalDevice physicalDevice,
                             VkDisplayKHR display);

#if defined(VK_USE_PLATFORM_XLIB_XRANDR_EXT)
VkResult
pvr_mesa_wsi_acquire_xlib_display(struct pvr_mesa_wsi *mwsi,
                                  VkPhysicalDevice physicalDevice,
                                  void *dpy,
                                  VkDisplayKHR display);

VkResult
pvr_mesa_wsi_get_randr_output_display(struct pvr_mesa_wsi *mwsi,
                                      VkPhysicalDevice  physicalDevice,
                                      void *dpy,
                                      uint32_t output,
                                      VkDisplayKHR *pDisplay);

#endif

VkResult
pvr_mesa_wsi_display_power_control(struct pvr_mesa_wsi *mwsi,
                                   VkDevice device,
                                   VkDisplayKHR display,
                                   const VkDisplayPowerInfoEXT *pDisplayPowerInfo);

VkResult
pvr_mesa_wsi_register_device_event(struct pvr_mesa_wsi *mwsi,
                                   VkDevice device,
                                   const VkDeviceEventInfoEXT *pDeviceEventInfo,
                                   const VkAllocationCallbacks *pAllocator,
                                   void **pFence,
                                   int syncFd);

VkResult
pvr_mesa_wsi_register_display_event(struct pvr_mesa_wsi *mwsi,
                                    VkDevice device,
                                    VkDisplayKHR display,
                                    const VkDisplayEventInfoEXT *pDisplayEventInfo,
                                    const VkAllocationCallbacks  *pAllocator,
                                    void **pFence,
                                    int syncFd);

VkResult
pvr_mesa_wsi_get_swapchain_counter(struct pvr_mesa_wsi *mwsi,
                                   VkDevice device,
                                   VkSwapchainKHR swapchain,
                                   VkSurfaceCounterFlagBitsEXT flagBits,
                                   uint64_t *pValue);

/*
 * The following are available in version 1 of the interface.
 * Version 1 also supports the version 0 interface.
 */

uint32_t
pvr_mesa_wsi_get_version(struct pvr_mesa_wsi *mwsi);

void
pvr_mesa_wsi_surface_destroy(struct pvr_mesa_wsi *mwsi,
                             VkSurfaceKHR surface,
                             const VkAllocationCallbacks *pAllocator);

/*
 * Functions defined in the PowerVR DDK for use by Mesa.
 * All functions have a "pvr_vk_mesa_wsi" prefix.
 * Since the introduction of version 1 of the interface, the following
 * function is now regarded as forming version 0 of the interface.
 */
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
pvr_vk_mesa_wsi_sym_addr(VkPhysicalDevice physicalDevice,
                         const char *name);

/*
 * The following is available in version 1 of the interface.
 * Version 1 also supports the version 0 interface.
 */
uint32_t
pvr_vk_mesa_wsi_get_version(VkPhysicalDevice physicalDevice);

#endif /* PVR_MESA_WSI_INTERFACE_H */
