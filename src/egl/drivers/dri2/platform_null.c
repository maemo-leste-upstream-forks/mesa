/*
 * Copyright (c) Imagination Technologies Ltd.
 *
 * Parts based on platform_wayland, which has:
 *
 * Copyright © 2011-2012 Intel Corporation
 * Copyright © 2012 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <dlfcn.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <util/u_vector.h>
#include <xf86drm.h>

#include "egl_dri2.h"
#include "loader.h"
#include "util/os_file.h"

#define NULL_CARD_MINOR_MAX 63U

/*
 * Need at least version 4 for __DRI_IMAGE_ATTRIB_WIDTH and
 * __DRI_IMAGE_ATTRIB_HEIGHT
 */
#define NULL_IMAGE_EXTENSION_VERSION_MIN 4

struct object_property {
   uint32_t object_id;
   uint32_t prop_id;
   uint64_t prop_value;
};

static inline
uint32_t get_back_buffer_id(struct dri2_egl_surface *dri2_surf)
{
   uintptr_t offset = ((uintptr_t) dri2_surf->back) -
      ((uintptr_t) &dri2_surf->color_buffers[0]);

   assert(dri2_surf->back && !(offset >> 32));

   return (uint32_t) (offset / sizeof(dri2_surf->color_buffers[0]));
}

#define object_property_set_named(output, object_type, prop_name, value)    \
   {                                                                        \
      .object_id = (output)->object_type##_id,                              \
      .prop_id = property_id_get_for_name((output)->object_type##_prop_res, \
                                          prop_name),                       \
      .prop_value = value,                                                  \
   }

static const struct dri2_null_yuv_attrib {
   uint32_t order;
   uint32_t subsample;
   uint32_t num_planes;
   uint32_t plane_bpp;
} dri2_null_yuv_attribs[] = {
   {
      /* __DRI_IMAGE_FORMAT_YUYV */
      .order = __DRI_ATTRIB_YUV_ORDER_YUYV_BIT,
      .subsample = __DRI_ATTRIB_YUV_SUBSAMPLE_4_2_2_BIT,
      .num_planes = 1,
      .plane_bpp = __DRI_ATTRIB_YUV_PLANE_BPP_8_BIT,
   },
   {
      /* __DRI_IMAGE_FORMAT_NV12 */
      .order = __DRI_ATTRIB_YUV_ORDER_YUV_BIT,
      .subsample = __DRI_ATTRIB_YUV_SUBSAMPLE_4_2_0_BIT,
      .num_planes = 2,
      .plane_bpp = __DRI_ATTRIB_YUV_PLANE_BPP_8_BIT,
   },
   {
      /* __DRI_IMAGE_FORMAT_NV21 */
      .order = __DRI_ATTRIB_YUV_ORDER_YVU_BIT,
      .subsample = __DRI_ATTRIB_YUV_SUBSAMPLE_4_2_0_BIT,
      .num_planes = 2,
      .plane_bpp = __DRI_ATTRIB_YUV_PLANE_BPP_8_BIT,
   },
   {
      /* __DRI_IMAGE_FORMAT_YU12 */
      .order = __DRI_ATTRIB_YUV_ORDER_YUV_BIT,
      .subsample = __DRI_ATTRIB_YUV_SUBSAMPLE_4_2_0_BIT,
      .num_planes = 3,
      .plane_bpp = __DRI_ATTRIB_YUV_PLANE_BPP_8_BIT,
   },
   {
      /* __DRI_IMAGE_FORMAT_YV12 */
      .order = __DRI_ATTRIB_YUV_ORDER_YVU_BIT,
      .subsample = __DRI_ATTRIB_YUV_SUBSAMPLE_4_2_0_BIT,
      .num_planes = 3,
      .plane_bpp = __DRI_ATTRIB_YUV_PLANE_BPP_8_BIT,
   },
   {
      /* __DRI_IMAGE_FORMAT_UYVY */
      .order = __DRI_ATTRIB_YUV_ORDER_UYVY_BIT,
      .subsample = __DRI_ATTRIB_YUV_SUBSAMPLE_4_2_2_BIT,
      .num_planes = 1,
      .plane_bpp = __DRI_ATTRIB_YUV_PLANE_BPP_8_BIT,
   },
   {
      /* __DRI_IMAGE_FORMAT_YVYU */
      .order = __DRI_ATTRIB_YUV_ORDER_YVYU_BIT,
      .subsample = __DRI_ATTRIB_YUV_SUBSAMPLE_4_2_2_BIT,
      .num_planes = 1,
      .plane_bpp = __DRI_ATTRIB_YUV_PLANE_BPP_8_BIT,
   },
   {
      /* __DRI_IMAGE_FORMAT_VYUY */
      .order = __DRI_ATTRIB_YUV_ORDER_VYUY_BIT,
      .subsample = __DRI_ATTRIB_YUV_SUBSAMPLE_4_2_2_BIT,
      .num_planes = 1,
      .plane_bpp = __DRI_ATTRIB_YUV_PLANE_BPP_8_BIT,
   },
};

/*
 * The index of entries in this table is used as a bitmask in
 * dri2_dpy->formats, which tracks the formats supported by the display.
 */
static const struct dri2_null_format {
   uint32_t drm_format;
   int dri_image_format;
   int rgba_shifts[4];
   unsigned int rgba_sizes[4];
   const struct dri2_null_yuv_attrib *yuv;
} dri2_null_formats[] = {
   {
      .drm_format = DRM_FORMAT_XRGB8888,
      .dri_image_format = __DRI_IMAGE_FORMAT_XRGB8888,
      .rgba_shifts = { 16, 8, 0, -1 },
      .rgba_sizes = { 8, 8, 8, 0 },
      .yuv = NULL,
   },
   {
      .drm_format = DRM_FORMAT_ARGB8888,
      .dri_image_format = __DRI_IMAGE_FORMAT_ARGB8888,
      .rgba_shifts = { 16, 8, 0, 24 },
      .rgba_sizes = { 8, 8, 8, 8 },
      .yuv = NULL,
   },
   {
      .drm_format = DRM_FORMAT_RGB565,
      .dri_image_format = __DRI_IMAGE_FORMAT_RGB565,
      .rgba_shifts = { 11, 5, 0, -1 },
      .rgba_sizes = { 5, 6, 5, 0 },
      .yuv = NULL,
   },
   {
      .drm_format = DRM_FORMAT_YUYV,
      .dri_image_format = __DRI_IMAGE_FORMAT_YUYV,
      .rgba_shifts = { -1, -1, -1, -1 },
      .rgba_sizes = { 0, 0, 0, 0 },
      .yuv = &dri2_null_yuv_attribs[0],
   },
   {
      .drm_format = DRM_FORMAT_NV12,
      .dri_image_format = __DRI_IMAGE_FORMAT_NV12,
      .rgba_shifts = { -1, -1, -1, -1 },
      .rgba_sizes = { 0, 0, 0, 0 },
      .yuv = &dri2_null_yuv_attribs[1],
   },
   {
      .drm_format = DRM_FORMAT_NV21,
      .dri_image_format = __DRI_IMAGE_FORMAT_NV21,
      .rgba_shifts = { -1, -1, -1, -1 },
      .rgba_sizes = { 0, 0, 0, 0 },
      .yuv = &dri2_null_yuv_attribs[2],
   },
   {
      .drm_format = DRM_FORMAT_YUV420,
      .dri_image_format = __DRI_IMAGE_FORMAT_YU12,
      .rgba_shifts = { -1, -1, -1, -1 },
      .rgba_sizes = { 0, 0, 0, 0 },
      .yuv = &dri2_null_yuv_attribs[3],
   },
   {
      .drm_format = DRM_FORMAT_YVU420,
      .dri_image_format = __DRI_IMAGE_FORMAT_YV12,
      .rgba_shifts = { -1, -1, -1, -1 },
      .rgba_sizes = { 0, 0, 0, 0 },
      .yuv = &dri2_null_yuv_attribs[4],
   },
   {
      .drm_format = DRM_FORMAT_UYVY,
      .dri_image_format = __DRI_IMAGE_FORMAT_UYVY,
      .rgba_shifts = { -1, -1, -1, -1 },
      .rgba_sizes = { 0, 0, 0, 0 },
      .yuv = &dri2_null_yuv_attribs[5],
   },
   {
      .drm_format = DRM_FORMAT_YVYU,
      .dri_image_format = __DRI_IMAGE_FORMAT_YVYU,
      .rgba_shifts = { -1, -1, -1, -1 },
      .rgba_sizes = { 0, 0, 0, 0 },
      .yuv = &dri2_null_yuv_attribs[6],
   },
   {
      .drm_format = DRM_FORMAT_VYUY,
      .dri_image_format = __DRI_IMAGE_FORMAT_VYUY,
      .rgba_shifts = { -1, -1, -1, -1 },
      .rgba_sizes = { 0, 0, 0, 0 },
      .yuv = &dri2_null_yuv_attribs[7],
   },
};


static int
format_idx_get_from_config(struct dri2_egl_display *dri2_dpy,
                           const __DRIconfig *config)
{
   int shifts[4];
   unsigned int sizes[4];

   dri2_get_shifts_and_sizes(dri2_dpy->core, config, shifts, sizes);

   for (unsigned int i = 0; i < ARRAY_SIZE(dri2_null_formats); i++) {
      const struct dri2_null_format *format = &dri2_null_formats[i];

      if (shifts[0] == format->rgba_shifts[0] &&
          shifts[1] == format->rgba_shifts[1] &&
          shifts[2] == format->rgba_shifts[2] &&
          shifts[3] == format->rgba_shifts[3] &&
          sizes[0] == format->rgba_sizes[0] &&
          sizes[1] == format->rgba_sizes[1] &&
          sizes[2] == format->rgba_sizes[2] &&
          sizes[3] == format->rgba_sizes[3]) {
         return i;
      }
   }

   return -1;
}

static int
yuv_format_idx_get_from_config(struct dri2_egl_display *dri2_dpy,
                               const __DRIconfig *dri_config)
{
   for (unsigned int i = 0; i < ARRAY_SIZE(dri2_null_formats); i++) {
      const struct dri2_null_yuv_attrib *yuv = dri2_null_formats[i].yuv;
      unsigned order, subsample, num_planes, plane_bpp;

      if (!yuv)
         continue;

      dri2_dpy->core->getConfigAttrib(dri_config, __DRI_ATTRIB_YUV_ORDER,
                                      &order);
      dri2_dpy->core->getConfigAttrib(dri_config, __DRI_ATTRIB_YUV_SUBSAMPLE,
                                      &subsample);
      dri2_dpy->core->getConfigAttrib(dri_config, __DRI_ATTRIB_YUV_NUMBER_OF_PLANES,
                                      &num_planes);
      dri2_dpy->core->getConfigAttrib(dri_config, __DRI_ATTRIB_YUV_PLANE_BPP,
                                      &plane_bpp);

      if (order != yuv->order || subsample != yuv->subsample ||
          num_planes != yuv->num_planes || plane_bpp != yuv->plane_bpp)
         continue;

      return i;
   }

   return -1;
}

static int
format_idx_get_from_dri_image_format(uint32_t dri_image_format)
{
   for (unsigned int i = 0; i < ARRAY_SIZE(dri2_null_formats); i++)
      if (dri2_null_formats[i].dri_image_format == dri_image_format)
         return i;

   return -1;
}

static int
format_idx_get_from_drm_format(uint32_t drm_format)
{
   for (unsigned int i = 0; i < ARRAY_SIZE(dri2_null_formats); i++)
      if (dri2_null_formats[i].drm_format == drm_format)
         return i;

   return -1;
}

static inline uint32_t
blob_id_from_property_value(uint64_t prop_value)
{
   /* The KMS properties documetation, 01.org/linuxgraphics, says:
    *
    *     For all property types except blob properties the value is a 64-bit
    *     unsigned integer.
    */
   assert(!(prop_value >> 32));
   return (uint32_t) prop_value;
}

static int
atomic_state_add_object_properties(drmModeAtomicReq *atomic_state,
                                   const struct object_property *props,
                                   const unsigned prop_count)
{
   for (unsigned i = 0; i < prop_count; i++) {
      int err;

      if (props[i].prop_id == 0)
         return -EINVAL;

      err = drmModeAtomicAddProperty(atomic_state, props[i].object_id,
                                     props[i].prop_id, props[i].prop_value);
      if (err < 0)
         return err;
   }

   return 0;
}


static uint32_t
property_id_get_for_name(drmModePropertyRes **prop_res, const char *prop_name)
{
   if (prop_res)
      for (unsigned i = 0; prop_res[i]; i++)
         if (!strcmp(prop_res[i]->name, prop_name))
            return prop_res[i]->prop_id;

   return 0;
}

static drmModePropertyRes **
object_get_property_resources(int fd, uint32_t object_id, uint32_t object_type)
{
   drmModeObjectProperties *props;
   drmModePropertyRes **prop_res;

   props = drmModeObjectGetProperties(fd, object_id, object_type);
   if (!props)
      return NULL;

   prop_res = malloc((props->count_props + 1) * sizeof(*prop_res));
   if (prop_res) {
      prop_res[props->count_props] = NULL;

      for (unsigned i = 0; i < props->count_props; i++) {
         prop_res[i] = drmModeGetProperty(fd, props->props[i]);
         if (!prop_res[i]) {
            while (i--) {
               drmModeFreeProperty(prop_res[i]);
               free(prop_res);
               prop_res = NULL;
            }
            break;
         }
      }
   }

   drmModeFreeObjectProperties(props);

   return prop_res;
}

static void
object_free_property_resources(int fd, drmModePropertyRes **prop_res)
{
   for (unsigned i = 0; prop_res[i]; i++)
      drmModeFreeProperty(prop_res[i]);
   free(prop_res);
}

static bool
object_property_value_for_name(int fd, uint32_t object_id, uint32_t object_type,
                               const char *prop_name, uint64_t *value_out)
{
   drmModeObjectProperties *plane_props;
   bool found = false;

   plane_props = drmModeObjectGetProperties(fd, object_id, object_type);
   if (!plane_props)
      return false;

   for (unsigned i = 0; i < plane_props->count_props; i++) {
      drmModePropertyRes *prop;

      prop = drmModeGetProperty(fd, plane_props->props[i]);
      if (!prop)
         continue;

      found = !strcmp(prop->name, prop_name);
      drmModeFreeProperty(prop);
      if (found) {
         *value_out = plane_props->prop_values[i];
         break;
      }
   }

   drmModeFreeObjectProperties(plane_props);

   return found;
}

static int
connector_choose_mode(drmModeConnector *connector)
{
   if (!connector->count_modes)
      return -1;

   for (unsigned i = 0; i < connector->count_modes; i++) {
      if (connector->modes[i].flags & DRM_MODE_FLAG_INTERLACE)
         continue;

      if (connector->modes[i].type & DRM_MODE_TYPE_PREFERRED)
         return i;
   }

   return 0;
}

static drmModeConnector *
connector_get(int fd, drmModeRes *resources)
{
   /* Find the first connected connector */
   for (unsigned i = 0; i < resources->count_connectors; i++) {
      drmModeConnector *connector;

      connector = drmModeGetConnector(fd, resources->connectors[i]);
      if (!connector)
         continue;

      if (connector->connection == DRM_MODE_CONNECTED)
         return connector;

      drmModeFreeConnector(connector);
   }

   return NULL;
}

static drmModeCrtc *
crtc_get_for_connector(int fd, drmModeRes *resources,
                       drmModeConnector *connector)
{
   for (unsigned i = 0; i < connector->count_encoders; i++) {
      drmModeEncoder *encoder;

      encoder = drmModeGetEncoder(fd, connector->encoders[i]);
      if (!encoder)
         continue;

      for (unsigned j = 0; j < resources->count_crtcs; j++) {
         if (encoder->possible_crtcs & (1 << j)) {
            drmModeCrtc *crtc;

            crtc = drmModeGetCrtc(fd, resources->crtcs[j]);
            if (crtc) {
               drmModeFreeEncoder(encoder);
               return crtc;
            }
         }
      }

      drmModeFreeEncoder(encoder);
   }

   return NULL;
}

static drmModePlane *
primary_plane_get_for_crtc(int fd, drmModeRes *resources, drmModeCrtc *crtc)
{
   drmModePlaneRes *plane_resources;
   unsigned crtc_idx;

   plane_resources = drmModeGetPlaneResources(fd);
   if (!plane_resources)
      return NULL;

   for (crtc_idx = 0; crtc_idx < resources->count_crtcs; crtc_idx++)
      if (resources->crtcs[crtc_idx] == crtc->crtc_id)
         break;
   assert(crtc_idx != resources->count_crtcs);

   for (unsigned i = 0; i < plane_resources->count_planes; i++) {
      const uint32_t crtc_bit = 1 << crtc_idx;
      drmModePlane *plane;

      plane = drmModeGetPlane(fd, plane_resources->planes[i]);
      if (!plane)
         continue;

      if (plane->possible_crtcs & crtc_bit) {
         uint64_t type;
         bool res;

         res = object_property_value_for_name(fd, plane->plane_id,
                                              DRM_MODE_OBJECT_PLANE,
                                              "type", &type);
         if (res && type == DRM_PLANE_TYPE_PRIMARY) {
            drmModeFreePlaneResources(plane_resources);
            return plane;
         }
      }

      drmModeFreePlane(plane);
   }

   drmModeFreePlaneResources(plane_resources);

   return NULL;
}

static bool
display_output_atomic_init(int fd, struct display_output *output)
{
   drmModePropertyRes **connector_prop_res;
   drmModePropertyRes **crtc_prop_res;
   drmModePropertyRes **plane_prop_res;
   int err;

   connector_prop_res = object_get_property_resources(fd, output->connector_id,
                                                      DRM_MODE_OBJECT_CONNECTOR);
   if (!connector_prop_res)
      return false;

   crtc_prop_res = object_get_property_resources(fd, output->crtc_id,
                                                 DRM_MODE_OBJECT_CRTC);
   if (!crtc_prop_res)
      goto err_free_connector_prop_res;

   plane_prop_res = object_get_property_resources(fd, output->plane_id,
                                                  DRM_MODE_OBJECT_PLANE);
   if (!plane_prop_res)
      goto err_free_crtc_prop_res;

   err = drmModeCreatePropertyBlob(fd, &output->mode, sizeof(output->mode),
                                   &output->mode_blob_id);
   if (err)
      goto err_free_plane_prop_res;

   output->atomic_state = drmModeAtomicAlloc();
   if (!output->atomic_state)
      goto err_destroy_mode_prop_blob;

   if (property_id_get_for_name(plane_prop_res, "IN_FENCE_FD"))
      output->in_fence_supported = true;

   output->connector_prop_res = connector_prop_res;
   output->crtc_prop_res = crtc_prop_res;
   output->plane_prop_res = plane_prop_res;

   return true;

err_destroy_mode_prop_blob:
   drmModeDestroyPropertyBlob(fd, output->mode_blob_id);
err_free_plane_prop_res:
   object_free_property_resources(fd, plane_prop_res);
err_free_crtc_prop_res:
   object_free_property_resources(fd, crtc_prop_res);
err_free_connector_prop_res:
   object_free_property_resources(fd, connector_prop_res);
   return false;
}

static void
display_output_atomic_add_in_fence(struct display_output *output,
                                   int kms_in_fence_fd)
{
   const struct object_property obj_sync_props[] = {
      object_property_set_named(output, plane, "IN_FENCE_FD", kms_in_fence_fd),
   };
   int err;

   /* Explicit synchronisation is not being used */
   if (kms_in_fence_fd < 0)
      return;

   err = atomic_state_add_object_properties(output->atomic_state,
                                            obj_sync_props,
                                            ARRAY_SIZE(obj_sync_props));
   if (err)
      _eglLog(_EGL_DEBUG, "%s: failed to add props ERR = %d", __func__, err);
}

static void
atomic_claim_in_fence_fd(struct dri2_egl_surface *dri2_surf,
                         struct swap_queue_elem *swap_data)
{
   /* Explicit synchronisation is not being used */
   if (!dri2_surf->enable_out_fence)
      return;

   if (dri2_surf->out_fence_fd < 0) {
      _eglLog(_EGL_DEBUG, "%s: missing 'in' fence", __func__);
      return;
   }

   /* Take ownership of the fd */
   swap_data->kms_in_fence_fd = dri2_surf->out_fence_fd;
   dri2_surf->out_fence_fd = -1;
}

static void
atomic_relinquish_in_fence_fd(struct dri2_egl_surface *dri2_surf,
                              struct swap_queue_elem *swap_data)
{
   /* KMS is now in control of the fence (post drmModeAtomicCommit) */
   close(swap_data->kms_in_fence_fd);
   swap_data->kms_in_fence_fd = -1;
}

static int
display_output_atomic_flip(int fd, struct display_output *output, uint32_t fb_id,
                           uint32_t flags, void *flip_data)
{
   const struct object_property obj_props[] = {
      object_property_set_named(output, plane, "FB_ID", fb_id),
   };
   struct dri2_egl_surface *dri2_surf = flip_data;
   struct swap_queue_elem *swap_data = dri2_surf->swap_data;
   int err;

   /* Reset atomic state */
   drmModeAtomicSetCursor(output->atomic_state, 0);

   err = atomic_state_add_object_properties(output->atomic_state, obj_props,
                                            ARRAY_SIZE(obj_props));
   if (err)
      return err;

   display_output_atomic_add_in_fence(output, swap_data->kms_in_fence_fd);

   /*
    * Don't block - like drmModePageFlip, drmModeAtomicCommit will return
    * -EBUSY if the commit can't be queued in the kernel.
    */
   flags |= DRM_MODE_ATOMIC_NONBLOCK;

   err = drmModeAtomicCommit(fd, output->atomic_state, flags, flip_data);

   atomic_relinquish_in_fence_fd(dri2_surf, swap_data);

   return err;
}

static int
display_output_atomic_modeset(int fd, struct display_output *output, uint32_t fb_id)
{
   /* SRC_W and SRC_H in 16.16 fixed point */
   const struct object_property obj_props[] = {
      object_property_set_named(output, connector, "CRTC_ID", output->crtc_id),
      object_property_set_named(output, crtc, "ACTIVE", 1),
      object_property_set_named(output, crtc, "MODE_ID", output->mode_blob_id),
      object_property_set_named(output, plane, "FB_ID", fb_id),
      object_property_set_named(output, plane, "CRTC_ID", output->crtc_id),
      object_property_set_named(output, plane, "CRTC_X", 0),
      object_property_set_named(output, plane, "CRTC_Y", 0),
      object_property_set_named(output, plane, "CRTC_W", output->mode.hdisplay),
      object_property_set_named(output, plane, "CRTC_H", output->mode.vdisplay),
      object_property_set_named(output, plane, "SRC_X", 0),
      object_property_set_named(output, plane, "SRC_Y", 0),
      object_property_set_named(output, plane, "SRC_W", output->mode.hdisplay << 16),
      object_property_set_named(output, plane, "SRC_H", output->mode.vdisplay << 16),
   };
   int err;

   /* Reset atomic state */
   drmModeAtomicSetCursor(output->atomic_state, 0);

   err = atomic_state_add_object_properties(output->atomic_state, obj_props,
                                            ARRAY_SIZE(obj_props));
   if (err)
      return false;

   return drmModeAtomicCommit(fd, output->atomic_state,
                              DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
}

static void
swap_enqueue_data(struct dri2_egl_surface *dri2_surf, uint32_t back_id,
                  uint32_t interval)
{
   struct swap_queue_elem *swap_data;

   pthread_mutex_lock(&dri2_surf->mutex);
   swap_data = &dri2_surf->swap_queue[dri2_surf->swap_queue_idx_tail];
   swap_data->swap_interval = interval;
   swap_data->fb_id = dri2_surf->back->fb_id;
   swap_data->back_id = back_id;

   atomic_claim_in_fence_fd(dri2_surf, swap_data);

   dri2_surf->swap_queue_idx_tail++;
   dri2_surf->swap_queue_idx_tail %= ARRAY_SIZE(dri2_surf->swap_queue);

   /* Notify the swap thread there is new work to do */
   pthread_cond_signal(&dri2_surf->swap_queue_cond);
   pthread_mutex_unlock(&dri2_surf->mutex);
}

static void
swap_dequeue_data_start(struct dri2_egl_surface *dri2_surf)
{
   pthread_mutex_lock(&dri2_surf->mutex);
   while (dri2_surf->swap_queue_idx_head == dri2_surf->swap_queue_idx_tail)
      pthread_cond_wait(&dri2_surf->swap_queue_cond, &dri2_surf->mutex);

   dri2_surf->swap_data =
      &dri2_surf->swap_queue[dri2_surf->swap_queue_idx_head];
   pthread_mutex_unlock(&dri2_surf->mutex);
}

static void
swap_dequeue_data_finish(struct dri2_egl_surface *dri2_surf)
{
   pthread_mutex_lock(&dri2_surf->mutex);

   if (dri2_surf->current)
      dri2_surf->current->locked = false;

   dri2_surf->current =
      &dri2_surf->color_buffers[dri2_surf->swap_data->back_id];
   dri2_surf->swap_state = SWAP_IDLE;

   dri2_surf->swap_queue_idx_head++;
   dri2_surf->swap_queue_idx_head %= ARRAY_SIZE(dri2_surf->swap_queue);

   /* Notify get_back_bo that a buffer has become available */
   pthread_cond_signal(&dri2_surf->swap_unlock_buffer_cond);
   pthread_mutex_unlock(&dri2_surf->mutex);
}

static void
swap_drain_queue_data(struct dri2_egl_surface *dri2_surf)
{
   pthread_mutex_lock(&dri2_surf->mutex);
   while (dri2_surf->swap_queue_idx_head != dri2_surf->swap_queue_idx_tail)
      pthread_cond_wait(&dri2_surf->swap_unlock_buffer_cond, &dri2_surf->mutex);
   pthread_mutex_unlock(&dri2_surf->mutex);
}

static void
flip_handler(int fd, unsigned int sequence, unsigned int tv_sec,
             unsigned int tv_usec, void *flip_data)
{
   struct dri2_egl_surface *dri2_surf = flip_data;

   (void) tv_sec;
   (void) tv_usec;
   (void) sequence;

   /* Ultimate queueing ops */
   swap_dequeue_data_finish(dri2_surf);
}

static void
vblank_handler(int fd, unsigned int sequence, unsigned int tv_sec,
               unsigned int tv_usec, void *vblank_data)
{
   struct dri2_egl_surface *dri2_surf = vblank_data;

   (void) tv_sec;
   (void) tv_usec;
   (void) sequence;

   dri2_surf->swap_state = SWAP_FLIP;
}

static int
drm_event_process(int fd)
{
   static drmEventContext evctx = {
      .version = 2,
      .page_flip_handler = flip_handler,
      .vblank_handler = vblank_handler
   };
   struct pollfd pfd = {.fd = fd, .events = POLLIN};
   int ret;

   do {
      ret = poll(&pfd, 1, -1);
   } while (ret > 0 && pfd.revents != pfd.events);

   if (ret <= 0)
      /* Man says:
       *
       * On error, -1 is returned, and errno is set to indicate the
       * cause of the error.
       */
      return -1;

   drmHandleEvent(fd, &evctx);

   return 0;
}

static bool
plane_init_in_formats(int fd, drmModePlane *plane, struct u_vector *modifiers,
                      uint32_t *in_formats_id_out, unsigned *formats_out)
{
   uint32_t blob_id, prev_fmt = DRM_FORMAT_INVALID, count_formats = 0;
   drmModeFormatModifierIterator drm_iter = {0};
   drmModePropertyBlobRes *blob;
   uint64_t prop_value;
   int idx, err;

   assert(plane && in_formats_id_out && formats_out);

   err = !object_property_value_for_name(fd, plane->plane_id,
                                         DRM_MODE_OBJECT_PLANE,
                                         "IN_FORMATS", &prop_value);
   if (err)
      return false;

   blob_id = blob_id_from_property_value(prop_value);
   blob = drmModeGetPropertyBlob(fd, blob_id);

   while (drmModeFormatModifierBlobIterNext(blob, &drm_iter)) {
      if (drm_iter.fmt != prev_fmt) {
         prev_fmt = drm_iter.fmt;
         count_formats++;

         idx = format_idx_get_from_drm_format(drm_iter.fmt);
         if (idx < 0)
            continue;

         *formats_out |= (1 << idx);
      }
   }

   drmModeFreePropertyBlob(blob);

   if (!count_formats) {
      /* None of the formats in the IN_FORMATS blob has associated modifiers */
      _eglLog(_EGL_WARNING, "no format-modifiers found in IN_FORMATS");
      return false;
   }

   if (plane->count_formats != count_formats)
      /* Only some of the formats in the IN_FORMATS blob have associated modifiers,
       * try to use this subset.
       */
      _eglLog(_EGL_WARNING, "discarding formats without modifiers");

   /* Allocate space for modifiers, if ENOMEM fallback to plane formats */
   if (!u_vector_init(modifiers, sizeof(uint64_t), 64)) {
     _eglLog(_EGL_WARNING, "failed to allocate modifiers");
     return false;
   }

   *in_formats_id_out = blob_id;
   return true;
}

static bool
display_output_init(int fd, struct display_output *output, bool use_atomic,
                    bool prefer_in_formats, bool *in_formats_enabled_out)
{
   drmModeRes *resources;
   drmModeConnector *connector;
   drmModeCrtc *crtc;
   drmModePlane *plane;
   unsigned mode_idx;

   resources = drmModeGetResources(fd);
   if (!resources)
      return false;

   connector = connector_get(fd, resources);
   if (!connector)
      goto err_free_resources;

   crtc = crtc_get_for_connector(fd, resources, connector);
   if (!crtc)
      goto err_free_connector;

   plane = primary_plane_get_for_crtc(fd, resources, crtc);
   if (!plane)
      goto err_free_crtc;

   mode_idx = connector_choose_mode(connector);
   if (mode_idx < 0)
      goto err_free_plane;
   output->mode = connector->modes[mode_idx];

   assert(in_formats_enabled_out && !(*in_formats_enabled_out));

   /* Track display supported formats. Look them up from IN_FORMATS blobs
    * if they are available, otherwise use plane formats.
    */
   if (prefer_in_formats)
      *in_formats_enabled_out = plane_init_in_formats(fd, plane,
                                                      &output->modifiers,
                                                      &output->in_formats_id,
                                                      &output->formats);

   if (!*in_formats_enabled_out) {
      _eglLog(_EGL_WARNING, "fallback to plane formats");

      for (unsigned i = 0; i < plane->count_formats; i++) {
         int format_idx;

         format_idx = format_idx_get_from_drm_format(plane->formats[i]);
         if (format_idx == -1)
            continue;

         output->formats |= (1 << format_idx);
      }
   }

   /* At this point we can only shut down if the look up failed and
    * it is safe to pass NULL to drmModeFreeFormats().
    */
   if (!output->formats)
      goto err_free_plane;

   output->connector_id = connector->connector_id;
   output->crtc_id = crtc->crtc_id;
   output->plane_id = plane->plane_id;

   drmModeFreePlane(plane);
   drmModeFreeCrtc(crtc);
   drmModeFreeConnector(connector);
   drmModeFreeResources(resources);

   if (use_atomic) {
      if (!display_output_atomic_init(fd, output)) {
         _eglLog(_EGL_DEBUG,
                 "failed to initialise atomic support (using legacy mode)");
      }
   }

   return true;

err_free_plane:
   drmModeFreePlane(plane);
err_free_crtc:
   drmModeFreeCrtc(crtc);
err_free_connector:
   drmModeFreeConnector(connector);
err_free_resources:
   drmModeFreeResources(resources);
   return false;
}

static int
display_output_flip(int fd, struct display_output *output, uint32_t fb_id,
                    uint32_t flags, void *flip_data)
{
   int err;

   do {
      if (output->atomic_state)
         err = display_output_atomic_flip(fd, output, fb_id, flags, flip_data);
      else
         err = drmModePageFlip(fd, output->crtc_id, fb_id, flags, flip_data);
   } while (err == -EBUSY);

   return err;
}

static int
display_request_vblank(int fd, uint32_t target_frame, uint32_t flags,
                       void *vblank_data)
{
   drmVBlank vblank = {
      .request = {
         .type = flags,
         .sequence = target_frame,
         .signal = (unsigned long)vblank_data,
      }
   };

   return drmWaitVBlank(fd, &vblank);
}

static int
display_get_vblank_sequence(int fd, uint32_t *current_vblank_out)
{
   drmVBlank vblank = { .request = { .type = DRM_VBLANK_RELATIVE } };
   int err;

   err = drmWaitVBlank(fd, &vblank);
   if (err)
      return err;

   *current_vblank_out = vblank.reply.sequence;

   return 0;
}

static int
display_output_modeset(int fd, struct display_output *output, uint32_t fb_id)
{
   if (output->atomic_state)
      return display_output_atomic_modeset(fd, output, fb_id);

   return drmModeSetCrtc(fd, output->crtc_id, fb_id, 0, 0,
                         &output->connector_id, 1, &output->mode);
}

static int
swap_idle_get_target_frame(struct dri2_egl_surface *dri2_surf,
                           uint32_t *current_vblank_out, uint32_t *target_frame_out)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
   int err;

   /* For intarvals bigger than 1, always update current_vblank. The
    * spec isn't fully clear, nonetheless page 25 and 26 of the PDF of the
    * EGL 1.5 spec say:
    *
    *     "[the parameter interval] indicates the number of swap intervals
    *      that will elapse before a buffer swap takes place after calling
    *      eglSwapBuffers."
    *
    * We need to guarantee that the target frame is always ahead of the
    * current vblank by the number of intervals set at the time swapBuffer
    * is called. For intervals of 1 or 0, we don't need a target frame.
    */
   err = display_get_vblank_sequence(dri2_dpy->fd_dpy, current_vblank_out);
   if (err)
      return err;

   assert(dri2_surf->swap_data->swap_interval > 0);

   /* -1 accounts for vsync locked flip, so get a vblank one frame earlier */
   *target_frame_out =
      *current_vblank_out + dri2_surf->swap_data->swap_interval - 1;

   return 0;
}

static int
swap_idle_state_transition(struct dri2_egl_surface *dri2_surf,
                           uint32_t *target_frame_out)
{
   uint32_t current_vblank = 0;
   uint32_t target_frame = 0;
   int err;

   /* update dri2_surf->swap_data */
   swap_dequeue_data_start(dri2_surf);

   /* update next target frame */
   if (dri2_surf->swap_data->swap_interval > 1) {
      err = swap_idle_get_target_frame(dri2_surf, &current_vblank,
                                       &target_frame);
      if (err) {
         dri2_surf->swap_state = SWAP_ERROR;
         return err;
      }
   }

   dri2_surf->swap_state =
      target_frame <= current_vblank ? SWAP_FLIP : SWAP_VBLANK;
   *target_frame_out = target_frame;

   return 0;
}

static int
swap_vblank_state_transition(struct dri2_egl_surface *dri2_surf,
                             uint32_t target_frame)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
   uint32_t flags = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
   int err;

   err = display_request_vblank(dri2_dpy->fd_dpy, target_frame,
                                flags, dri2_surf);
   if (err) {
      dri2_surf->swap_state = SWAP_ERROR;
      return err;
   }

   dri2_surf->swap_state = SWAP_POLL;

   return 0;
}

static int
swap_flip_state_transition(struct dri2_egl_surface *dri2_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
   uint32_t flags;
   int err;

   flags = DRM_MODE_PAGE_FLIP_EVENT;
   if (dri2_surf->swap_data->swap_interval == 0) {
      assert(!dri2_dpy->atomic_enabled);
      flags |= DRM_MODE_PAGE_FLIP_ASYNC;
   }

   err = display_output_flip(dri2_dpy->fd_dpy, &dri2_dpy->output,
                             dri2_surf->swap_data->fb_id, flags, dri2_surf);
   if (err) {
      dri2_surf->swap_state = SWAP_ERROR;
      return err;
   }

   dri2_surf->swap_state = SWAP_POLL;

   return 0;
}

static int
swap_poll_state_transition(struct dri2_egl_surface *dri2_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
   int err;

   /* dri2_surf->swap_state is being set inside the handler */
   err = drm_event_process(dri2_dpy->fd_dpy);
   if (err) {
      dri2_surf->swap_state = SWAP_ERROR;
      return err;
   }

   return 0;
}

static inline void
swap_error_print_message(const int state, const int err)
{
   switch (state) {
   case SWAP_IDLE:
      _eglLog(_EGL_WARNING,
              "failed to get a target frame (err=%d)", err);
      break;
   case SWAP_FLIP:
      _eglLog(_EGL_WARNING,
              "failed to schedule a pageflip (err=%d)", err);
      break;
   case SWAP_VBLANK:
      _eglLog(_EGL_WARNING,
              "failed to request a vblank event (err=%d)", err);
      break;
   case SWAP_POLL:
      _eglLog(_EGL_WARNING,
              "failed to poll for drm event (err=%d)", err);
      break;
   case SWAP_ERROR:
      _eglLog(_EGL_WARNING,
              "failed to swap buffers, unknown swap state");
      break;
   default:
      _eglLog(_EGL_FATAL,
              "failed to swap buffers (unknown error)");
   };
}

static void
swap_error_state_handler(struct dri2_egl_surface *dri2_surf,
                         int state, int err)
{
   static bool do_log = true;

   if (do_log) {
      swap_error_print_message(state, err);
      do_log = false;
   }

   swap_dequeue_data_finish(dri2_surf);
}

static void *
swap_queue_processor_worker(void *data)
{
   struct dri2_egl_surface *dri2_surf = data;
   int state = SWAP_IDLE, err = SWAP_ERROR;
   uint32_t target_frame = 0;

   assert(dri2_surf->swap_state == SWAP_IDLE);

   while (1) {
      switch (dri2_surf->swap_state) {
      case SWAP_IDLE:
         err = swap_idle_state_transition(dri2_surf, &target_frame);
         break;
      case SWAP_VBLANK:
         err = swap_vblank_state_transition(dri2_surf, target_frame);
         break;
      case SWAP_FLIP:
         err = swap_flip_state_transition(dri2_surf);
         break;
      case SWAP_POLL:
         err = swap_poll_state_transition(dri2_surf);
         break;
      case SWAP_ERROR:
         swap_error_state_handler(dri2_surf, state, err);
         break;
      default:
         dri2_surf->swap_state = SWAP_ERROR;
         break;
      }

      if (!err)
         state = dri2_surf->swap_state;
   }

   return NULL;
}

static bool
add_fb_for_dri_image(struct dri2_egl_display *dri2_dpy, __DRIimage *image,
                     uint32_t *fb_id_out)
{
   int handle, stride, width, height, format, l_mod, h_mod, offset;
   uint64_t modifier = DRM_FORMAT_MOD_INVALID;
   uint64_t *modifiers = NULL, mods[4] = {0};
   uint32_t handles[4] = {0};
   uint32_t pitches[4] = {0};
   uint32_t offsets[4] = {0};
   __DRIimage *p_image;
   uint32_t flags = 0;
   int format_idx;
   int num_planes;

   dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_WIDTH, &width);
   dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_HEIGHT, &height);
   dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_FORMAT, &format);

   format_idx = format_idx_get_from_dri_image_format(format);
   assert(format_idx != -1);

   if (dri2_dpy->in_formats_enabled) {
      dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_MODIFIER_UPPER, &h_mod);
      dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_MODIFIER_LOWER, &l_mod);

      modifier = combine_u32_into_u64((uint32_t) h_mod, (uint32_t) l_mod);
      modifiers = mods;

      flags |= DRM_MODE_FB_MODIFIERS;
   }

   dri2_dpy->image->queryImage(image,
                               __DRI_IMAGE_ATTRIB_NUM_PLANES, &num_planes);
   if (num_planes <= 0)
      num_planes = 1;

   for (int i = 0; i < num_planes; i++) {
      if (dri2_dpy->in_formats_enabled) {
         assert(modifiers && modifier != DRM_FORMAT_MOD_INVALID);
         modifiers[i] = modifier;
      }

      p_image = dri2_dpy->image->fromPlanar(image, i, NULL);
      if (!p_image) {
         assert(i == 0);
         p_image = image;
      }

      dri2_dpy->image->queryImage(p_image, __DRI_IMAGE_ATTRIB_STRIDE,
                                  &stride);
      dri2_dpy->image->queryImage(p_image, __DRI_IMAGE_ATTRIB_OFFSET,
                                  &offset);
      dri2_dpy->image->queryImage(p_image, __DRI_IMAGE_ATTRIB_HANDLE,
                                  &handle);

      if (p_image != image)
         dri2_dpy->image->destroyImage(p_image);

      pitches[i] = (uint32_t) stride;
      offsets[i] = (uint32_t) offset;
      handles[i] = (uint32_t) handle;
   }

   return !drmModeAddFB2WithModifiers(dri2_dpy->fd_dpy, width, height,
                                      dri2_null_formats[format_idx].drm_format,
                                      handles, pitches, offsets, modifiers,
                                      fb_id_out, flags);
}

static __DRIimage *
create_image(struct dri2_egl_surface *dri2_surf, uint32_t flags)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
   uint32_t count_modifiers;
   uint64_t *modifiers;

   if (dri2_dpy->in_formats_enabled) {
      count_modifiers = u_vector_length(&dri2_dpy->output.modifiers);
      modifiers = u_vector_tail(&dri2_dpy->output.modifiers);

      return dri2_dpy->image->createImageWithModifiers(dri2_dpy->dri_screen,
                                                       dri2_surf->base.Width,
                                                       dri2_surf->base.Height,
                                                       dri2_surf->format,
                                                       modifiers,
                                                       count_modifiers,
                                                       NULL);
   }

   return dri2_dpy->image->createImage(dri2_dpy->dri_screen,
                                       dri2_surf->base.Width,
                                       dri2_surf->base.Height,
                                       dri2_surf->format,
                                       flags,
                                       NULL);
}

static bool
get_front_bo(struct dri2_egl_surface *dri2_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
   unsigned int use = 0;

   if (dri2_surf->base.Type == EGL_WINDOW_BIT)
      use |= __DRI_IMAGE_USE_SCANOUT;

   dri2_surf->front_buffer.dri_image = create_image(dri2_surf, use);
   if (!dri2_surf->front_buffer.dri_image)
      return false;

   if (dri2_surf->base.Type == EGL_WINDOW_BIT) {
      if (!add_fb_for_dri_image(dri2_dpy, dri2_surf->front_buffer.dri_image,
                                &dri2_surf->front_buffer.fb_id)) {
         dri2_dpy->image->destroyImage(dri2_surf->front_buffer.dri_image);
         dri2_surf->front_buffer.dri_image = NULL;
         return false;
      }
   }

   return true;
}

static bool
get_back_bo(struct dri2_egl_surface *dri2_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);

   pthread_mutex_lock(&dri2_surf->mutex);
   while (!dri2_surf->back) {
      for (unsigned i = 0; i < ARRAY_SIZE(dri2_surf->color_buffers); i++) {
         if (!dri2_surf->color_buffers[i].locked) {
            dri2_surf->back = &dri2_surf->color_buffers[i];
            break;
         }
      }

      /* Wait for a flip to get a buffer off the screen and unlock it */
      if (!dri2_surf->back)
         pthread_cond_wait(&dri2_surf->swap_unlock_buffer_cond, &dri2_surf->mutex);
   }

   if (!dri2_surf->back->dri_image) {
      dri2_surf->back->dri_image = create_image(dri2_surf,
                                                __DRI_IMAGE_USE_SCANOUT);
      if (!dri2_surf->back->dri_image)
         goto err_unlock;
   }

   if (!dri2_surf->back->fb_id) {
      if (!add_fb_for_dri_image(dri2_dpy, dri2_surf->back->dri_image,
                                &dri2_surf->back->fb_id))
         goto err_unlock;
   }

   dri2_surf->back->locked = 1;
   pthread_mutex_unlock(&dri2_surf->mutex);

   return true;

err_unlock:
   pthread_mutex_unlock(&dri2_surf->mutex);
   return false;
}

static void surface_swap_queue_init(struct dri2_egl_surface *dri2_surf)
{
   struct swap_queue_elem *swap_queue = &dri2_surf->swap_queue[0];
   const int num_el = ARRAY_SIZE(dri2_surf->swap_queue);

   for (int i = 0; i < num_el; i++)
      swap_queue[i].kms_in_fence_fd = -1;
}

static bool
in_formats_get_modifiers(const int fd, const uint32_t in_formats_id,
                         const int drm_format, struct u_vector *modifiers)
{
   drmModeFormatModifierIterator drm_iter = {0};
   drmModePropertyBlobRes *blob;
   uint64_t *mod = NULL;

   blob = drmModeGetPropertyBlob(fd, in_formats_id);

   while (drmModeFormatModifierBlobIterNext(blob, &drm_iter)) {
      if (drm_iter.fmt == drm_format) {
         assert(drm_iter.mod != DRM_FORMAT_MOD_INVALID);

         mod = u_vector_add(modifiers);
         *mod = drm_iter.mod;
      }
   }

   drmModeFreePropertyBlob(blob);

   return mod != NULL;
}

static _EGLSurface *
create_surface(_EGLDisplay *disp, _EGLConfig *config, EGLint type,
               const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct display_output *output = &dri2_dpy->output;
   struct dri2_egl_config *dri2_config = dri2_egl_config(config);
   struct dri2_egl_surface *dri2_surf;
   const __DRIconfig *dri_config;
   unsigned int render_type;
   _EGLSurface *surf;
   int format_idx;
   bool ret;

   dri2_surf = calloc(1, sizeof(*dri2_surf));
   if (!dri2_surf) {
      _eglError(EGL_BAD_ALLOC, "failed to create surface");
      return NULL;
   }
   surf = &dri2_surf->base;

   if (!dri2_init_surface(surf, disp, type, config, attrib_list,
                          output->in_fence_supported, NULL))
      goto err_free_surface;

   dri_config = dri2_get_dri_config(dri2_config, type,
                                    dri2_surf->base.GLColorspace);
   if (!dri_config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto err_free_surface;
   }

   dri2_surf->dri_drawable =
      dri2_dpy->image_driver->createNewDrawable(dri2_dpy->dri_screen,
                                                dri_config, dri2_surf);
   if (!dri2_surf->dri_drawable) {
      _eglError(EGL_BAD_ALLOC, "failed to create drawable");
       goto err_free_surface;
   }

   if (!dri2_dpy->core->getConfigAttrib(dri_config, __DRI_ATTRIB_RENDER_TYPE,
                                        &render_type))
      goto err_free_surface;

   if (render_type & __DRI_ATTRIB_YUV_BIT)
      format_idx = yuv_format_idx_get_from_config(dri2_dpy, dri_config);
   else
      format_idx = format_idx_get_from_config(dri2_dpy, dri_config);
   assert(format_idx != -1);

   dri2_surf->format = dri2_null_formats[format_idx].dri_image_format;

   if (dri2_dpy->in_formats_enabled) {
      ret = in_formats_get_modifiers(dri2_dpy->fd_dpy,
                                     dri2_dpy->output.in_formats_id,
                                     dri2_null_formats[format_idx].drm_format,
                                     &dri2_dpy->output.modifiers);
      if (!ret)
         goto err_free_surface;
   }

   surface_swap_queue_init(dri2_surf);

   return surf;

err_free_surface:
   free(dri2_surf);
   return NULL;
}

static void
destroy_surface(_EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(surf->Resource.Display);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);
   dri2_fini_surface(surf);
   free(surf);
}

static EGLBoolean
dri2_null_destroy_surface(_EGLDisplay *disp, _EGLSurface *surf);

static _EGLSurface *
dri2_null_create_window_surface(_EGLDisplay *disp, _EGLConfig *config,
                                void *native_window, const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf;
   _EGLSurface *surf;
   int err;

   if (dri2_dpy->output.in_use) {
      _eglError(EGL_BAD_NATIVE_WINDOW, "window in use");
      return NULL;
   }

   surf = create_surface(disp, config, EGL_WINDOW_BIT, attrib_list);
   if (!surf)
      return NULL;
   dri2_surf = dri2_egl_surface(surf);

   dri2_surf->base.Width = dri2_dpy->output.mode.hdisplay;
   dri2_surf->base.Height = dri2_dpy->output.mode.vdisplay;

   /* After the dri2_surf is created, init thread's data */
   dri2_surf->mutex_init = !pthread_mutex_init(&dri2_surf->mutex, NULL);
   if (!dri2_surf->mutex_init) {
      _eglError(EGL_BAD_ALLOC, "failed to init swap thread mutex");
      goto err_destroy_surface;
   }

   dri2_surf->cond_init = !pthread_cond_init(&dri2_surf->swap_queue_cond, NULL);
   if (!dri2_surf->cond_init) {
      _eglError(EGL_BAD_ALLOC, "failed to init swap queue condition");
      goto err_destroy_surface;
   }

   dri2_surf->cond_init_unlock_buffer =
      !pthread_cond_init(&dri2_surf->swap_unlock_buffer_cond, NULL);
   if (!dri2_surf->cond_init_unlock_buffer) {
      _eglError(EGL_BAD_ALLOC, "failed to init swap buffer unlock condition");
      goto err_destroy_surface;
   }

   if (!get_front_bo(dri2_surf))  {
      _eglError(EGL_BAD_NATIVE_WINDOW, "window get buffer");
      goto err_destroy_surface;
   }

   err = display_output_modeset(dri2_dpy->fd_dpy, &dri2_dpy->output,
                                dri2_surf->front_buffer.fb_id);
   if (err) {
      _eglError(EGL_BAD_NATIVE_WINDOW, "window set mode");
      goto err_destroy_surface;
   }

   dri2_dpy->output.in_use = true;
   dri2_surf->swap_state = SWAP_IDLE;

   err = pthread_create(&dri2_surf->swap_queue_processor, NULL,
                        swap_queue_processor_worker, dri2_surf);
   if (err) {
      _eglError(EGL_BAD_ALLOC, "failed to create swap thread");
      goto err_destroy_surface;
   }

   return surf;

err_destroy_surface:
   dri2_null_destroy_surface(disp, surf);
   return NULL;
}

static _EGLSurface *
dri2_null_create_pbuffer_surface(_EGLDisplay *disp, _EGLConfig *config,
                                 const EGLint *attrib_list)
{
   return create_surface(disp, config, EGL_PBUFFER_BIT, attrib_list);
}

static void
dri2_null_init_front_buffer_render(_EGLSurface *draw)
{
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(draw);

   dri2_surf->front_render_init = true;

   /* Drain the queue. swap_buffer_unlock_cond signals for the last time
    * when the last back buffer in the queue went on screen and it's being
    * tracked as current by then.
    */
   swap_drain_queue_data(dri2_surf);

   /* If previously flipped, take a reference to the current buffer */
   if (dri2_surf->current) {
      assert(dri2_surf->current->dri_image);
      dri2_surf->back = dri2_surf->current;

      for (unsigned i = 0; i < DRI2_SURFACE_NUM_COLOR_BUFFERS; i++)
         dri2_surf->color_buffers[i].age = 0;

      return;
   }

   /* If the application hasn't yet fetched a back buffer, then it's not too
    * late to use front buffer's dri_image and fb_id.
    */
   if (!dri2_surf->back) {
      assert(dri2_surf->front_buffer.dri_image);
      dri2_surf->back = &dri2_surf->front_buffer;

      /* Don't need to reset buffer age since no flip was requested yet */

      return;
   }

   /* In order to initialise one color buffer for front buffer rendering,
    * one page flip must occur.
    */
   swap_enqueue_data(dri2_surf, get_back_buffer_id(dri2_surf), 1);

   return dri2_null_init_front_buffer_render(draw);
}

static void
dri2_null_disable_front_buffer_render(_EGLSurface *draw)
{
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(draw);

   dri2_surf->front_render_enabled = false;
   dri2_surf->front_render_init = false;
   dri2_surf->back = NULL;
}

static EGLBoolean
dri2_null_destroy_surface(_EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);
   EGLint type = surf->Type;

   /* If there's a current surface then a page flip has been performed, so make
    * sure we process the flip event.
    */
   if (dri2_surf->swap_queue_processor) {
      swap_drain_queue_data(dri2_surf);
      pthread_cancel(dri2_surf->swap_queue_processor);
      pthread_join(dri2_surf->swap_queue_processor, NULL);
   }

   if (dri2_surf->cond_init)
      pthread_cond_destroy(&dri2_surf->swap_queue_cond);

   if (dri2_surf->cond_init_unlock_buffer)
      pthread_cond_destroy(&dri2_surf->swap_unlock_buffer_cond);

   if (dri2_surf->mutex_init)
      pthread_mutex_destroy(&dri2_surf->mutex);

   if (dri2_surf->front_buffer.dri_image)
      dri2_dpy->image->destroyImage(dri2_surf->front_buffer.dri_image);

   if (dri2_surf->front_buffer.fb_id)
         drmModeRmFB(dri2_dpy->fd_dpy, dri2_surf->front_buffer.fb_id);

   for (unsigned i = 0; i < ARRAY_SIZE(dri2_surf->color_buffers); i++) {
      if (dri2_surf->color_buffers[i].fb_id)
         drmModeRmFB(dri2_dpy->fd_dpy, dri2_surf->color_buffers[i].fb_id);
      if (dri2_surf->color_buffers[i].dri_image)
         dri2_dpy->image->destroyImage(dri2_surf->color_buffers[i].dri_image);
   }

   destroy_surface(surf);

   if (type == EGL_WINDOW_BIT)
      dri2_dpy->output.in_use = false;

   return EGL_TRUE;
}

static EGLBoolean
dri2_null_swap_buffers(_EGLDisplay *disp, _EGLSurface *draw)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(draw);
   uint32_t back_id;

   if (dri2_surf->base.Type != EGL_WINDOW_BIT)
      return EGL_TRUE;

   /* Flush and early return, no swap takes place */
   if (dri2_surf->front_render_enabled) {
      dri2_flush_drawable_for_swapbuffers(disp, draw);

      if (!dri2_surf->front_render_init)
         dri2_null_init_front_buffer_render(draw);

      return EGL_TRUE;
   }

   for (unsigned i = 0; i < ARRAY_SIZE(dri2_surf->color_buffers); i++)
      if (dri2_surf->color_buffers[i].age > 0)
         dri2_surf->color_buffers[i].age++;

   /* Make sure we have a back buffer in case we're swapping without
    * ever rendering. */
   if (!get_back_bo(dri2_surf)) {
      _eglError(EGL_BAD_ALLOC, "dri2_null_swap_buffers");
      return EGL_FALSE;
   }

   dri2_flush_drawable_for_swapbuffers(disp, draw);
   dri2_dpy->flush->invalidate(dri2_surf->dri_drawable);

   back_id = get_back_buffer_id(dri2_surf);
   assert(dri2_surf->back == &dri2_surf->color_buffers[back_id]);

   swap_enqueue_data(dri2_surf, back_id, draw->SwapInterval);

   /* This back buffer is tracked in the swap_data, safe to drop it now */
   dri2_surf->back->age = 1;
   dri2_surf->back = NULL;

   return EGL_TRUE;
}

static EGLint
dri2_null_query_buffer_age(_EGLDisplay *disp, _EGLSurface *surface)
{
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surface);

   if (!get_back_bo(dri2_surf)) {
      _eglError(EGL_BAD_ALLOC, "failed to get back buffer to query age");
      return -1;
   }

   return dri2_surf->back->age;
}

static EGLBoolean
dri2_null_swap_interval(_EGLDisplay *dpy, _EGLSurface *draw, EGLint interval)
{
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(draw);
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);

   /* dri2_dpy tracks whether the display driver is async flip capable.
    * If it isn't, enable front buffer rendering when swap interval
    * 0 is passed in from the application.
    */
   if (!interval && !dri2_dpy->async_flip_enabled) {
      if (!dri2_surf->front_render_enabled)
         dri2_surf->front_render_enabled = true;
   } else {
      if (dri2_surf->front_render_enabled)
         dri2_null_disable_front_buffer_render(draw);
   }

   _eglLog(_EGL_DEBUG, "DRI2: set swap interval to %d", interval);
   draw->SwapInterval = interval;
   return EGL_TRUE;
}

static struct dri2_egl_display_vtbl dri2_null_display_vtbl = {
   .create_window_surface = dri2_null_create_window_surface,
   .create_pbuffer_surface = dri2_null_create_pbuffer_surface,
   .destroy_surface = dri2_null_destroy_surface,
   .create_image = dri2_create_image_khr,
   .swap_interval = dri2_null_swap_interval,
   .swap_buffers = dri2_null_swap_buffers,
   .query_buffer_age = dri2_null_query_buffer_age,
   .get_dri_drawable = dri2_surface_get_dri_drawable,
};

static int
dri2_null_image_get_buffers(__DRIdrawable *driDrawable, unsigned int format,
                            uint32_t *stamp, void *loaderPrivate,
                            uint32_t buffer_mask, struct __DRIimageList *buffers)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;

   buffers->image_mask = 0;
   buffers->back = NULL;
   buffers->front = NULL;

   if (buffer_mask & __DRI_IMAGE_BUFFER_FRONT)
      if (!get_front_bo(dri2_surf))
         return 0;

   if (buffer_mask & __DRI_IMAGE_BUFFER_BACK)
      if (!get_back_bo(dri2_surf))
         return 0;

   if (buffer_mask & __DRI_IMAGE_BUFFER_FRONT) {
      buffers->image_mask |= __DRI_IMAGE_BUFFER_FRONT;
      buffers->front = dri2_surf->front_buffer.dri_image;
   }

   if (buffer_mask & __DRI_IMAGE_BUFFER_BACK) {
      buffers->image_mask |= __DRI_IMAGE_BUFFER_BACK;
      buffers->back = dri2_surf->back->dri_image;
   }

   return 1;
}

static unsigned
dri2_null_get_capability(void *loaderPrivate, enum dri_loader_cap cap)
{
   switch (cap) {
   case DRI_LOADER_CAP_YUV_SURFACE_IMG:
      return 1;
   default:
      return 0;
   }
}

static void
dri2_null_flush_front_buffer(__DRIdrawable * driDrawable, void *loaderPrivate)
{
   (void) driDrawable;
   (void) loaderPrivate;
}

static int
dri2_null_get_display_fd(void *loaderPrivate)
{
   _EGLDisplay *disp = loaderPrivate;
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);

   return dri2_dpy->fd_dpy;
}

static const __DRIimageLoaderExtension image_loader_extension = {
   .base = { __DRI_IMAGE_LOADER, 5 },

   .getBuffers          = dri2_null_image_get_buffers,
   .flushFrontBuffer    = dri2_null_flush_front_buffer,
   .getCapability       = dri2_null_get_capability,
   .getDisplayFD        = dri2_null_get_display_fd,
};

static const __DRIextension *image_loader_extensions[] = {
   &image_loader_extension.base,
   &image_lookup_extension.base,
   &use_invalidate.base,
   NULL,
};

static bool
dri2_null_device_is_kms(int fd)
{
   drmModeRes *resources;
   bool is_kms;

   resources = drmModeGetResources(fd);
   if (!resources)
      return false;

   is_kms = resources->count_crtcs != 0 &&
      resources->count_connectors != 0 &&
      resources->count_encoders != 0;

   drmModeFreeResources(resources);

   return is_kms;
}

static bool
dri2_null_try_device(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);

   dri2_dpy->fd = -1;

   if (!dri2_null_device_is_kms(dri2_dpy->fd_dpy))
      return false;

#if defined(NULL_DRI_DRIVER_NAME)
   /* Skip devices not handled by NULL_DRI_DRIVER_NAME */
   {
      char *driver_name = loader_get_driver_for_fd(dri2_dpy->fd_dpy);
      bool skip = !driver_name || !!strcmp(driver_name, NULL_DRI_DRIVER_NAME);

      free(driver_name);

      if (skip)
         return false;
   }
#endif

   dri2_dpy->fd = os_dupfd_cloexec(dri2_dpy->fd_dpy);
   if (dri2_dpy->fd < 0) {
      _eglLog(_EGL_WARNING, "DRI2: failed to dup display FD");
      dri2_dpy->fd = dri2_dpy->fd_dpy;
   } else {
      int fd_old;
      bool is_different_gpu;

      fd_old = dri2_dpy->fd;
      dri2_dpy->fd = loader_get_user_preferred_fd(dri2_dpy->fd,
                                                  &is_different_gpu);
      if (dri2_dpy->fd == fd_old) {
         close (dri2_dpy->fd);
         dri2_dpy->fd = dri2_dpy->fd_dpy;
      }
   }

   dri2_dpy->driver_name = loader_get_driver_for_fd(dri2_dpy->fd);
   if (!dri2_dpy->driver_name)
      return false;

   if (dri2_load_driver_dri3(disp)) {
      _EGLDevice *dev = _eglAddDevice(dri2_dpy->fd, false);
      if (!dev) {
         dlclose(dri2_dpy->driver);
         _eglLog(_EGL_WARNING, "DRI2: failed to find EGLDevice");
      } else {
         dri2_dpy->loader_extensions = image_loader_extensions;
         dri2_dpy->own_device = 1;
         disp->Device = dev;
         return true;
      }
   }

   free(dri2_dpy->driver_name);
   dri2_dpy->driver_name = NULL;

   return false;
}

static bool
dri2_null_probe_device(_EGLDisplay *disp, unsigned minor)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   char *card_path;

   if (asprintf(&card_path, DRM_DEV_NAME, DRM_DIR_NAME, minor) < 0)
      goto cleanup;

   dri2_dpy->fd_dpy = loader_open_device(card_path);
   free(card_path);
   if (dri2_dpy->fd_dpy < 0)
      goto cleanup;

   if (dri2_null_try_device(disp))
      return true;

   close(dri2_dpy->fd_dpy);

   if (dri2_dpy->fd >= 0 && dri2_dpy->fd != dri2_dpy->fd_dpy)
      close(dri2_dpy->fd);

cleanup:
   dri2_dpy->fd_dpy = -1;
   dri2_dpy->fd = -1;

   return false;
}

static bool
dri2_null_probe_devices(_EGLDisplay *disp)
{
   const char *null_drm_display = getenv("NULL_DRM_DISPLAY");

   if (null_drm_display) {
      char *endptr;
      long val = strtol(null_drm_display, &endptr, 10);

      if (endptr != null_drm_display && !*endptr &&
          val >= 0 && val <= NULL_CARD_MINOR_MAX) {
         if (dri2_null_probe_device(disp, (unsigned)val))
            return true;
      } else {
         _eglLog(_EGL_FATAL, "NULL_DRM_DISPLAY is invalid: %s",
                 null_drm_display);
      }
   } else {
      for (unsigned i = 0; i <= NULL_CARD_MINOR_MAX; i++) {
         if (dri2_null_probe_device(disp, i))
            return true;
      }
   }

   return false;
}

static EGLBoolean
dri2_null_add_configs_for_formats(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   unsigned int count = 0;

   for (unsigned i = 0; dri2_dpy->driver_configs[i]; i++) {
      struct dri2_egl_config *dri2_conf;
      EGLint surface_type = EGL_WINDOW_BIT;
      unsigned int render_type;
      int format_idx;

      if (!dri2_dpy->core->getConfigAttrib(dri2_dpy->driver_configs[i],
                                           __DRI_ATTRIB_RENDER_TYPE,
                                           &render_type))
         continue;

      if (render_type & __DRI_ATTRIB_YUV_BIT) {
         format_idx = yuv_format_idx_get_from_config(dri2_dpy,
                                                     dri2_dpy->driver_configs[i]);
      } else {
         format_idx = format_idx_get_from_config(dri2_dpy,
                                                 dri2_dpy->driver_configs[i]);
         surface_type |= EGL_PBUFFER_BIT;
      }

      if (format_idx == -1)
         continue;

      if (!(dri2_dpy->output.formats & (1 << format_idx))) {
         _eglLog(_EGL_DEBUG, "unsupported drm format 0x%04x",
                 dri2_null_formats[format_idx].drm_format);
         continue;
      }

      dri2_conf = dri2_add_config(disp,
                                  dri2_dpy->driver_configs[i], count + 1,
                                  surface_type, NULL, NULL, NULL);
      if (dri2_conf)
         count++;
   }

   return count != 0;
}
static void
dri2_null_setup_swap_interval(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   const int swap_max_interval = 10; /* Arbitrary max value */
   uint64_t value;
   int err;

   dri2_setup_swap_interval(disp, swap_max_interval);

   err = drmGetCap(dri2_dpy->fd_dpy, DRM_CAP_ASYNC_PAGE_FLIP, &value);
   if (err || value == 0) {

      /* DRM/KMS does not support async page flip. In order to support
       * swap interval 0, use front buffer rendering.
       */
      _eglLog(_EGL_DEBUG,
              "drm async flip not supported, use front buffer");
   } else {

      /* drm/atomic: Reject FLIP_ASYNC unconditionally
       * upstream f2cbda2dba11de868759cae9c0d2bab5b8411406
       *
       * Legacy DRM/KMS can use DRM_MODE_PAGE_FLIP_ASYNC, for atomic
       * drivers fallback to front buffer rendering.
       */
      if (dri2_dpy->atomic_enabled)
         _eglLog(_EGL_DEBUG,
                 "async flip not supported by atomic, use front buffer");
      else
         dri2_dpy->async_flip_enabled = true;
   }
}

EGLBoolean
dri2_initialize_null(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy;
   bool prefer_in_formats = false;
   uint64_t value;
   int err;

   dri2_dpy = calloc(1, sizeof(*dri2_dpy));
   if (!dri2_dpy)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   disp->DriverData = (void *) dri2_dpy;

   dri2_dpy->fd_dpy = -1;
   dri2_dpy->fd = -1;

   if (!dri2_null_probe_devices(disp)) {
      _eglError(EGL_NOT_INITIALIZED, "failed to load driver");
      goto cleanup;
   }

   /*
    * Try to use atomic modesetting if available and fallback to legacy kernel
    * modesetting if not. If this succeeds then universal planes will also have
    * been enabled.
    */
   err = drmSetClientCap(dri2_dpy->fd_dpy, DRM_CLIENT_CAP_ATOMIC, 1);
   dri2_dpy->atomic_enabled = !err;

   if (!dri2_dpy->atomic_enabled) {
      /*
       * Enable universal planes so that we can get the pixel formats for the
       * primary plane
       */
      err = drmSetClientCap(dri2_dpy->fd_dpy, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
      if (err) {
         _eglError(EGL_NOT_INITIALIZED, "failed to enable universal planes");
         goto cleanup;
      }

      dri2_dpy->atomic_enabled = false;
   }

   if (!dri2_create_screen(disp)) {
      _eglError(EGL_NOT_INITIALIZED, "failed to create screen");
      goto cleanup;
   }

   if (!dri2_setup_extensions(disp)) {
      _eglError(EGL_NOT_INITIALIZED, "failed to find required DRI extensions");
      goto cleanup;
   }

   dri2_setup_screen(disp);
   dri2_null_setup_swap_interval(disp);

   if (dri2_dpy->image->base.version < NULL_IMAGE_EXTENSION_VERSION_MIN) {
      _eglError(EGL_NOT_INITIALIZED, "image extension version too old");
      goto cleanup;
   }

   err = drmGetCap(dri2_dpy->fd_dpy, DRM_CAP_ADDFB2_MODIFIERS, &value);
   if (!err && value) {
      /* in_formats could be supported by the platform, however not being
       * actually enabled, i.e. in_formats init can still fail.
       */
      prefer_in_formats = dri2_dpy->image->base.version >= 14 &&
         dri2_dpy->image->createImageWithModifiers;
   }

   if (!display_output_init(dri2_dpy->fd_dpy, &dri2_dpy->output,
                            dri2_dpy->atomic_enabled,
                            prefer_in_formats,
                            &dri2_dpy->in_formats_enabled)) {
      _eglError(EGL_NOT_INITIALIZED, "failed to create output");
      goto cleanup;
   }

   if (!dri2_null_add_configs_for_formats(disp)) {
      _eglError(EGL_NOT_INITIALIZED, "failed to add configs");
      goto cleanup;
   }

   disp->Extensions.EXT_buffer_age = EGL_TRUE;
   disp->Extensions.KHR_image_base = EGL_TRUE;

   /* Fill vtbl last to prevent accidentally calling virtual function during
    * initialization.
    */
   dri2_dpy->vtbl = &dri2_null_display_vtbl;

   return EGL_TRUE;

cleanup:
   dri2_display_destroy(disp);
   return EGL_FALSE;
}

void
dri2_teardown_null(struct dri2_egl_display *dri2_dpy)
{
   drmModeAtomicFree(dri2_dpy->output.atomic_state);

   if (dri2_dpy->output.mode_blob_id)
      drmModeDestroyPropertyBlob(dri2_dpy->fd_dpy, dri2_dpy->output.mode_blob_id);

   if (dri2_dpy->output.plane_prop_res) {
      for (unsigned i = 0; dri2_dpy->output.plane_prop_res[i]; i++)
         drmModeFreeProperty(dri2_dpy->output.plane_prop_res[i]);
      free(dri2_dpy->output.plane_prop_res);
   }

   if (dri2_dpy->output.crtc_prop_res) {
      for (unsigned i = 0; dri2_dpy->output.crtc_prop_res[i]; i++)
         drmModeFreeProperty(dri2_dpy->output.crtc_prop_res[i]);
      free(dri2_dpy->output.crtc_prop_res);
   }

   if (dri2_dpy->output.connector_prop_res) {
      for (unsigned i = 0; dri2_dpy->output.connector_prop_res[i]; i++)
         drmModeFreeProperty(dri2_dpy->output.connector_prop_res[i]);
      free(dri2_dpy->output.connector_prop_res);
   }

   u_vector_finish(&dri2_dpy->output.modifiers);
}
