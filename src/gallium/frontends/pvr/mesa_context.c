/**
 * \file context.c
 * Mesa context/visual/framebuffer management functions.
 * \author Brian Paul
 */

/*
 * Mesa 3-D graphics library
 * Version:  7.1
 *
 * Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <assert.h>

#include "main/version.h"
#include "main/errors.h"

#include "dri_support.h"
#include "dri_util.h"
#include "glapi.h"
#include "dispatch.h"
#include "pvrmesa.h"

/**
 * This is the default function we plug into all dispatch table slots This
 * helps prevents a segfault when someone calls a GL function without first
 * checking if the extension is supported.
 */
static int
generic_nop(void)
{
   _mesa_warning(NULL, "User called no-op dispatch function (an unsupported extension function?)");

   return 0;
}

/**
 * Allocate and initialise a new dispatch table.
 */
static struct _glapi_table *
pvrdri_alloc_dispatch_table(void)
{
   unsigned int numEntries = _glapi_get_dispatch_table_size();
   _glapi_proc *table;

   table = malloc(numEntries * sizeof(_glapi_proc));
   if (table)
      for (unsigned int i = 0; i < numEntries; i++)
         table[i] = (_glapi_proc) generic_nop;

   return (struct _glapi_table *) table;
}

/**
 * Return a pointer to the pointer to the dispatch table of an API in
 * PVRDRIScreen.
 */
static struct _glapi_table **
pvrdri_get_dispatch_table_ptr(PVRDRIScreen *psPVRScreen, PVRDRIAPIType eAPI)
{
   switch (eAPI) {
   case PVRDRI_API_GLES1:
      return &psPVRScreen->psOGLES1Dispatch;
   case PVRDRI_API_GLES2:
      return &psPVRScreen->psOGLES2Dispatch;
   case PVRDRI_API_GL_COMPAT:
   case PVRDRI_API_GL_CORE:
      return &psPVRScreen->psOGLDispatch;
   default:
      return NULL;
   }
}

/**
 * Return a pointer to the dispatch table of an API.
 */
static struct _glapi_table *
pvrdri_get_dispatch_table(PVRDRIScreen *psPVRScreen, PVRDRIAPIType eAPI)
{
   struct _glapi_table **ppsTable =
      pvrdri_get_dispatch_table_ptr(psPVRScreen, eAPI);

   return ppsTable ? *ppsTable : NULL;
}

/**
 * Free all dispatch tables.
 */
void
pvrdri_free_dispatch_tables(PVRDRIScreen *psPVRScreen)
{
   if (psPVRScreen->psOGLES1Dispatch != NULL) {
      free(psPVRScreen->psOGLES1Dispatch);
      psPVRScreen->psOGLES1Dispatch = NULL;
   }

   if (psPVRScreen->psOGLES2Dispatch != NULL) {
      free(psPVRScreen->psOGLES2Dispatch);
      psPVRScreen->psOGLES2Dispatch = NULL;
   }

   if (psPVRScreen->psOGLDispatch != NULL) {
      free(psPVRScreen->psOGLDispatch);
      psPVRScreen->psOGLDispatch = NULL;
   }
}

static void
pvrdri_add_mesa_dispatch(struct _glapi_table *psTable, PVRDRIAPIType eAPI,
                         struct DRISUPScreen *psDRISUPScreen,
                         unsigned int uIdx)
{
   const char *asFunc[] = { NULL, NULL };
   int iOffset;
   const char *psFunc;
   _glapi_proc pfFunc;

   pfFunc = DRISUPGetAPIProcAddress(psDRISUPScreen, eAPI, uIdx);
   if (pfFunc == NULL)
      return;

   psFunc = DRISUPGetAPIProcName(psDRISUPScreen, eAPI, uIdx);
   assert(psFunc != NULL);

   asFunc[0] = psFunc;
   iOffset = _glapi_add_dispatch(asFunc, "");
   if (iOffset == -1) {
      _mesa_warning(NULL, "Couldn't add %s to the Mesa dispatch table",
                    psFunc);
   } else {
      SET_by_offset(psTable, iOffset, pfFunc);
   }
}

static void
pvrdri_set_mesa_dispatch(struct _glapi_table *psTable, PVRDRIAPIType eAPI,
                         struct DRISUPScreen *psDRISUPScreen,
                         unsigned int uNumFuncs)
{
   for (unsigned int i = 0; i < uNumFuncs; i++)
      pvrdri_add_mesa_dispatch(psTable, eAPI, psDRISUPScreen, i);
}

bool
pvrdri_create_dispatch_table(PVRDRIScreen *psPVRScreen, PVRDRIAPIType eAPI)
{
   struct DRISUPScreen *psDRISUPScreen = psPVRScreen->psDRISUPScreen;
   struct _glapi_table **ppsTable;
   unsigned int uNumFuncs;

   ppsTable = pvrdri_get_dispatch_table_ptr(psPVRScreen, eAPI);
   if (ppsTable == NULL)
      return false;

   if (*ppsTable != NULL)
      return true;

   uNumFuncs = DRISUPGetNumAPIProcs(psDRISUPScreen, eAPI);
   if (!uNumFuncs)
      return false;

   *ppsTable = pvrdri_alloc_dispatch_table();
   if (*ppsTable == NULL)
      return false;

   pvrdri_set_mesa_dispatch(*ppsTable, eAPI, psDRISUPScreen, uNumFuncs);

   return true;
}

void
pvrdri_set_null_dispatch_table(void)
{
   _glapi_set_dispatch(NULL);
}

void
pvrdri_set_dispatch_table(PVRDRIContext *psPVRContext)
{
   struct _glapi_table *psTable;

   psTable = pvrdri_get_dispatch_table(psPVRContext->psPVRScreen,
                                       psPVRContext->eAPI);

   _glapi_set_dispatch(psTable);
}
