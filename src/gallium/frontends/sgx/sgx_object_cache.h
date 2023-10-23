/*************************************************************************/ /*!
@File           sgx_object_cache.h
@Title          Cache for objects.
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

#if !defined(__PVR_OBJECT_CACHE_H__)
#define __PVR_OBJECT_CACHE_H__

#include "stdbool.h"
#ifndef IMG_INTERNAL
#define IMG_INTERNAL	__attribute__((visibility("hidden")))
#endif

typedef struct _PVRObjectCache *PVRObjectCache;

/* Cache insert callback. Returns object data on success or NULL otherwise. */
typedef void *(*PVRObjectCacheInsertCB)(void *pvCreateData,
					void *pvInsertData);

/*
 * Cache purge callback. The object data is the data returned by the insert
 * callback. The bRetired flag indicates an object has been retired due to
 * age, or lack of space in the cache, rather than an explicit purge.
 */
typedef void (*PVRObjectCachePurgeCB)(void *pvCreateData,
				      void *pvObjectData,
				      bool bRetired);
/*
 * Cache compare callback. The object data is the data returned by the insert
 * callback.
 */
typedef bool (*PVRObjectCacheCompareCB)(void *pvCreateData,
                                        void *pvObjectData,
                                        void *pvInsertData);


/**************************************************************************/ /*!
@Function       PVRObjectCacheCreate
@Description    Creates an object cache.
@Input          uNumObj             The maximum number of objects that can be
                                    stored in the cache.
@Input          uMaxAge             The maximum age of an object before it gets
                                    purged from the cache. If 0 then age related
                                    purging is disabled.
@Input          pvCreateData        Optional data to be passed to the provided
                                    callback functions.
@Input          pfnInsertCB         Callback used when data is inserted into
                                    the cache.
@Input          pfnPurgeCB          Optional callback used when an object is
                                    purged from the cache.
@Input          pfnCompareCB        Optional callback used when inserting a new
                                    object into the cache to determine whether
                                    or not the object already exists within the
                                    cache.
@Return         PVRObjectCache      Returns a handle to the newly created object
                                    cache on success or NULL otherwise.
*/ /***************************************************************************/
IMG_INTERNAL PVRObjectCache PVRObjectCacheCreate(unsigned uNumObj,
						 unsigned uMaxAge,
						 void *pvCreateData,
						 PVRObjectCacheInsertCB pfnInsertCB,
						 PVRObjectCachePurgeCB pfnPurgeCB,
						 PVRObjectCacheCompareCB pfnCompareCB);

/**************************************************************************/ /*!
@Function       PVRObjectCacheInsert
@Description    Insert an object into the cache, giving it a starting age of
                one. If the object was already contained within the cache then
                the insert callback will not be called and the existing cache
                object will have its age reset to one. All other objects in the
                cache will have their age increased by one. This may result in
                an object being purged from the cache if its age exceeds the
                maximum age specified when creating the cache or the cache is
                already full.
@Input          hObjectCache        Handle to an object cache.
@Input          pvInsertData        Data passed to the insert callback.
@Return         IMG_BOOL            Returns IMG_TRUE on success or IMG_FALSE
                                    otherwise.
*/ /***************************************************************************/
IMG_INTERNAL bool PVRObjectCacheInsert(PVRObjectCache hObjectCache,
                                       void *pvInsertData);

/**************************************************************************/ /*!
@Function       PVRObjectCacheGetObject
@Description    Returns object data from the cache based on the given object
                number, whereby the youngest object corresponds to zero.
@Input          hObjectCache        Handle to an object cache.
@Input          uObj                Number of the object to return.
@Return         void *              Returns object data (as returned by the
                                    insert callback) on success or NULL
                                    otherwise.
*/ /***************************************************************************/
IMG_INTERNAL void *PVRObjectCacheGetObject(PVRObjectCache hObjectCache,
					   unsigned uObj);

/**************************************************************************/ /*!
@Function       PVRObjectCachePurge
@Description    Purges the given cache of all objects.
@Input          hObjectCache        Handle to an object cache.
@Return         void
*/ /***************************************************************************/
IMG_INTERNAL void PVRObjectCachePurge(PVRObjectCache hObjectCache);

/**************************************************************************/ /*!
@Function       PVRObjectCacheDestroy
@Description    Destroy the given object cache, purging all remaining objects
                in the cache.
@Input          hObjectCache        Handle to an object cache.
@Return         void
*/ /***************************************************************************/
IMG_INTERNAL void PVRObjectCacheDestroy(PVRObjectCache hObjectCache);

#endif /* defined(__PVR_OBJECT_CACHE_H__) */
