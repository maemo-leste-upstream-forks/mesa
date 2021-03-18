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

#if !defined PVR_WSI_H
#define PVR_WSI_H

#include "util/macros.h"
#include "util/u_memory.h"
#include "util/u_atomic.h"

#include "wsi_common.h"

#define  Container(p, s, m) ((s *) ((uintptr_t)(p) - Offset(s, m)))

#define _MAKE_STRING(x) # x
#define MAKE_STRING(x) _MAKE_STRING(x)

#define LOOKUP_DDK(mwsi, sym) \
   mwsi->symtab.pvr_vk_mesa_wsi_sym_addr(MAKE_STRING(sym))

#define JUMP_DDK(mwsi, sym, ...)                          \
   do {                                                   \
      void *_entry = p_atomic_read(&mwsi->symtab.sym);    \
                                                          \
      if (!_entry) {                                      \
         _entry = LOOKUP_DDK(mwsi, sym);                  \
                                                          \
         if (_entry)                                      \
            p_atomic_set(&mwsi->symtab.sym, _entry);      \
      }                                                   \
                                                          \
      if (_entry) {                                       \
         __typeof__(mwsi->symtab.sym) _func = _entry;     \
                                                          \
         return _func(__VA_ARGS__);                       \
      }                                                   \
   } while(0)

struct pvr_vk_mesa_wsi_sym_tab
{
   PFN_vkVoidFunction (VKAPI_PTR *pvr_vk_mesa_wsi_sym_addr)
      (VkPhysicalDevice physicalDevice, const char *);
};

struct pvr_mesa_wsi
{
   struct wsi_device wsi;
   struct pvr_vk_mesa_wsi_sym_tab symtab;
   VkPhysicalDevice physicalDevice;
};

static inline struct pvr_mesa_wsi *pvr_mesa_wsi(struct wsi_device *wsi_ptr)
{
   return Container(wsi_ptr, struct pvr_mesa_wsi, wsi);
}

#endif
