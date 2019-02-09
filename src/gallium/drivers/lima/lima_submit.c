/*
 * Copyright (C) 2017 Lima Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <string.h>

#include "xf86drm.h"
#include "libsync.h"
#include "lima_drm.h"

#include "util/list.h"
#include "util/ralloc.h"
#include "util/u_dynarray.h"
#include "util/os_time.h"

#include "lima_screen.h"
#include "lima_context.h"
#include "lima_submit.h"
#include "lima_bo.h"
#include "lima_util.h"

struct lima_submit_job {
   struct list_head list;
   uint32_t fence;

   struct util_dynarray bos;
};

struct lima_submit {
   struct lima_screen *screen;
   uint32_t pipe;
   uint32_t ctx;

   int in_sync_fd;
   uint32_t in_sync;
   uint32_t out_sync;

   struct util_dynarray gem_bos;

   struct list_head busy_job_list;
   struct list_head free_job_list;
   struct lima_submit_job *current_job;
};


#define VOID2U64(x) ((uint64_t)(unsigned long)(x))

struct lima_submit *lima_submit_create(struct lima_context *ctx, uint32_t pipe)
{
   struct lima_submit *s;

   s = rzalloc(ctx, struct lima_submit);
   if (!s)
      return NULL;

   s->screen = lima_screen(ctx->base.screen);
   s->pipe = pipe;
   s->ctx = ctx->id;
   s->in_sync_fd = -1;

   int err = drmSyncobjCreate(s->screen->fd, DRM_SYNCOBJ_CREATE_SIGNALED,
                              &s->out_sync);
   if (err)
      goto err_out0;

   err = drmSyncobjCreate(s->screen->fd, DRM_SYNCOBJ_CREATE_SIGNALED,
                          &s->in_sync);
   if (err)
      goto err_out1;

   util_dynarray_init(&s->gem_bos, s);

   list_inithead(&s->busy_job_list);
   list_inithead(&s->free_job_list);
   return s;

err_out1:
   drmSyncobjDestroy(s->screen->fd, s->out_sync);
err_out0:
   ralloc_free(s);
   return NULL;
}

void lima_submit_free(struct lima_submit *submit)
{
   if (submit->in_sync_fd >= 0)
      close(submit->in_sync_fd);
   drmSyncobjDestroy(submit->screen->fd, submit->in_sync);
   drmSyncobjDestroy(submit->screen->fd, submit->out_sync);
}

static struct lima_submit_job *lima_submit_job_alloc(struct lima_submit *submit)
{
   struct lima_submit_job *job;

   if (list_empty(&submit->free_job_list)) {
      job = rzalloc(submit, struct lima_submit_job);
      if (!job)
         return NULL;
      util_dynarray_init(&job->bos, job);
   }
   else {
      job = list_first_entry(&submit->free_job_list, struct lima_submit_job, list);
      list_del(&job->list);
   }

   return job;
}

static void lima_submit_job_free(struct lima_submit *submit,
                                 struct lima_submit_job *job)
{
   util_dynarray_foreach(&job->bos, struct lima_bo *, bo) {
      lima_bo_free(*bo);
   }
   util_dynarray_clear(&job->bos);
   list_add(&job->list, &submit->free_job_list);
}

bool lima_submit_add_bo(struct lima_submit *submit, struct lima_bo *bo, uint32_t flags)
{
   util_dynarray_foreach(&submit->gem_bos, struct drm_lima_gem_submit_bo, gem_bo) {
      if (bo->handle == gem_bo->handle) {
         gem_bo->flags |= flags;
         return true;
      }
   }

   struct drm_lima_gem_submit_bo *submit_bo =
      util_dynarray_grow(&submit->gem_bos, sizeof(*submit_bo));
   submit_bo->handle = bo->handle;
   submit_bo->flags = flags;

   if (!submit->current_job)
      submit->current_job = lima_submit_job_alloc(submit);

   struct lima_submit_job *job = submit->current_job;
   struct lima_bo **jbo = util_dynarray_grow(&job->bos, sizeof(*jbo));
   *jbo = bo;

   /* prevent bo from being freed when submit start */
   lima_bo_reference(bo);

   return true;
}

bool lima_submit_start(struct lima_submit *submit, void *frame, uint32_t size)
{
   struct drm_lima_gem_submit req = {
      .ctx = submit->ctx,
      .pipe = submit->pipe,
      .nr_bos = submit->gem_bos.size / sizeof(struct drm_lima_gem_submit_bo),
      .bos = VOID2U64(util_dynarray_begin(&submit->gem_bos)),
      .frame = VOID2U64(frame),
      .frame_size = size,
   };

   if (submit->in_sync_fd >= 0) {
      int err = drmSyncobjImportSyncFile(submit->screen->fd, submit->in_sync,
                                         submit->in_sync_fd);
      if (err)
         return false;

      req.in_sync[0] = submit->in_sync;
      close(submit->in_sync_fd);
      submit->in_sync_fd = -1;
   }

   bool ret = drmIoctl(submit->screen->fd, DRM_IOCTL_LIMA_GEM_SUBMIT, &req) == 0;

   struct lima_submit_job *job = submit->current_job;
   if (ret) {
      list_add(&job->list, &submit->busy_job_list);

      int i = 0;
      list_for_each_entry_safe(struct lima_submit_job, j,
                               &submit->busy_job_list, list) {
         if (i++ >= req.done) {
            list_del(&j->list);
            lima_submit_job_free(submit, j);
         }
      }
   }
   else
      lima_submit_job_free(submit, job);

   util_dynarray_clear(&submit->gem_bos);
   submit->current_job = NULL;
   return ret;
}

bool lima_submit_wait(struct lima_submit *submit, uint64_t timeout_ns)
{
   uint64_t abs_timeout = os_time_get_absolute_timeout(timeout_ns);
   if (abs_timeout == OS_TIMEOUT_INFINITE)
      abs_timeout = INT64_MAX;

   bool ret = !drmSyncobjWait(submit->screen->fd, &submit->out_sync, 1, abs_timeout, 0, NULL);
   if (ret) {
      list_for_each_entry_safe(struct lima_submit_job, j,
                               &submit->busy_job_list, list) {
         list_del(&j->list);
         lima_submit_job_free(submit, j);
      }
   }
   return ret;
}

bool lima_submit_has_bo(struct lima_submit *submit, struct lima_bo *bo, bool all)
{
   util_dynarray_foreach(&submit->gem_bos, struct drm_lima_gem_submit_bo, gem_bo) {
      if (bo->handle == gem_bo->handle) {
         if (all)
            return true;
         else
            return gem_bo->flags & LIMA_SUBMIT_BO_WRITE;
      }
   }

   return false;
}

bool lima_submit_add_in_sync(struct lima_submit *submit, int fd)
{
   return !sync_accumulate("lima", &submit->in_sync_fd, fd);
}

bool lima_submit_get_out_sync(struct lima_submit *submit, int *fd)
{
   return !drmSyncobjExportSyncFile(submit->screen->fd, submit->out_sync, fd);
}
