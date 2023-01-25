# Copyright (c) Imagination Technologies Ltd.
#
# The contents of this file are subject to the MIT license as set out below.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

files_libsgx = files(
  'mesa_context.c',
  'pvr_object_cache.c',
  'pvrdrawable.c',
  'pvrdri.c',
  'pvrext.c',
  'pvrutil.c',
)

libsgx_c_args = []

libsgx_dep = [dep_libdrm]
if with_platform_tizen
  libsgx_dep += dep_dlog
endif

libsgx = static_library(
  'sgx',
  [files_libsgx, main_dispatch_h],
  include_directories : [
    inc_include, inc_util, inc_mesa, inc_mapi, inc_src, inc_gallium,
    inc_gallium_aux, inc_pvr,
  ],
  c_args : [libsgx_c_args],
  gnu_symbol_visibility : 'hidden',
  dependencies : [libsgx_dep],
)
