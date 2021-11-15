/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
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

#if !defined(__PVRIMAGE_H__)
#define __PVRIMAGE_H__

#include "dri_support.h"

__DRIimage *PVRDRICreateImageFromName(__DRIscreen *screen,
				      int width, int height, int format,
				      int name, int pitch,
				      void *loaderPrivate);

__DRIimage *PVRDRICreateImageFromRenderbuffer(__DRIcontext *context,
					      int renderbuffer,
					      void *loaderPrivate);

void PVRDRIDestroyImage(__DRIimage *image);

__DRIimage *PVRDRICreateImage(__DRIscreen *screen,
			      int width, int height, int format,
			      unsigned int use,
			      void *loaderPrivate);

__DRIimage *PVRDRICreateImageWithModifiers(__DRIscreen *screen,
			      int width, int height, int format,
			      const uint64_t *modifiers,
			      const unsigned int modifier_count,
			      void *loaderPrivate);

GLboolean PVRDRIQueryImage(__DRIimage *image, int attrib, int *value);

__DRIimage *PVRDRIDupImage(__DRIimage *image, void *loaderPrivate);

GLboolean PVRDRIValidateUsage(__DRIimage *image, unsigned int use);

__DRIimage *PVRDRICreateImageFromNames(__DRIscreen *screen,
				       int width, int height, int fourcc,
				       int *names, int num_names,
				       int *strides, int *offsets,
				       void *loaderPrivate);

__DRIimage *PVRDRIFromPlanar(__DRIimage *image, int plane,
			     void *loaderPrivate);

__DRIimage *PVRDRICreateImageFromTexture(__DRIcontext *context,
					 int glTarget,
					 unsigned texture,
					 int depth,
					 int level,
					 unsigned *error,
					 void *loaderPrivate);

__DRIimage *PVRDRICreateImageFromFds(__DRIscreen *screen,
				     int width, int height, int fourcc,
				     int *fds, int num_fds,
				     int *strides, int *offsets,
				     void *loaderPrivate);

__DRIimage *PVRDRICreateImageFromBuffer(__DRIcontext *context,
					int target,
					void *buffer,
					unsigned *error,
					void *loaderPrivate);

__DRIimage *PVRDRICreateImageFromDmaBufs(__DRIscreen *screen,
                                         int width, int height, int fourcc,
                                         int *fds, int num_fds,
                                         int *strides, int *offsets,
                                         enum __DRIYUVColorSpace color_space,
                                         enum __DRISampleRange sample_range,
                                         enum __DRIChromaSiting horiz_siting,
                                         enum __DRIChromaSiting vert_siting,
                                         unsigned *error,
                                         void *loaderPrivate);

__DRIimage *PVRDRICreateImageFromDmaBufs2(__DRIscreen *screen,
					  int width, int height, int fourcc,
					  uint64_t modifier,
					  int *fds, int num_fds,
					  int *strides, int *offsets,
					  enum __DRIYUVColorSpace color_space,
					  enum __DRISampleRange sample_range,
					  enum __DRIChromaSiting horiz_siting,
					  enum __DRIChromaSiting vert_siting,
					  unsigned *error,
					  void *loaderPrivate);

void PVRDRIBlitImage(__DRIcontext *context,
		     __DRIimage *dst, __DRIimage *src,
		     int dstx0, int dsty0, int dstwidth, int dstheight,
		     int srcx0, int srcy0, int srcwidth, int srcheight,
		     int flush_flag);

int PVRDRIGetCapabilities(__DRIscreen *screen);

void *PVRDRIMapImage(__DRIcontext *context, __DRIimage *image,
		    int x0, int y0, int width, int height,
		    unsigned int flags, int *stride, void **data);

void PVRDRIUnmapImage(__DRIcontext *context, __DRIimage *image, void *data);

__DRIimage *PVRDRICreateImageFromRenderbuffer2(__DRIcontext *context,
					       int renderbuffer,
					       void *loaderPrivate,
					       unsigned *error);

#endif /* !defined(__PVRIMAGE_H__) */
