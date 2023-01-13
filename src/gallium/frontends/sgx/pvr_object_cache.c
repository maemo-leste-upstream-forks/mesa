/*************************************************************************/ /*!
@File           pvr_object_cache.c
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

#include <stdlib.h>
#include <search.h>

#include "pvrqueue.h"
#include "pvr_object_cache.h"

#define	FOR_QELEM_QOBJ(psCache, psQElem, psObj)				\
	for ((psQElem) = (psCache)->sQHead.pvForw;			\
	     (psQElem) != &(psCache)->sQHead &&				\
		     ((psObj) = PVRQ_CONTAINER_OF((psQElem),		\
						  struct PVRCachedObject, sQElem))->bValid; \
	     (psQElem) = (psQElem)->pvForw)

struct PVRCachedObject
{
	PVRQElem sQElem;

	void *pvObjectData;

	bool bValid;
	unsigned uAge;
};

struct _PVRObjectCache
{
	/* LRU queue of objects in cache */
	PVRQHead sQHead;

	unsigned uNumObj;
	unsigned uMaxAge;

	void *pvCreateData;

	PVRObjectCacheInsertCB pfnInsertCB;
	PVRObjectCachePurgeCB pfnPurgeCB;
	PVRObjectCacheCompareCB pfnCompareCB;
};

static inline void
PVRQueueRemove(struct PVRCachedObject *psObj)
{
	remque(&psObj->sQElem);
}

static inline void
PVRQueueAddFront(struct _PVRObjectCache *psCache, struct PVRCachedObject *psObj)
{
	insque(&psObj->sQElem, &psCache->sQHead);
}

static inline void
PVRQueueAddBack(struct _PVRObjectCache *psCache, struct PVRCachedObject *psObj)
{
	insque(&psObj->sQElem, psCache->sQHead.pvBack);
}

static inline bool
PVRObjectInsert(struct _PVRObjectCache *psCache,
		struct PVRCachedObject *psObj,
		void *pvInsertData)
{
	psObj->pvObjectData = psCache->pfnInsertCB(psCache->pvCreateData,
						  pvInsertData);

	return (psObj->pvObjectData != NULL);
}

static inline void
PVRObjectPurge(struct _PVRObjectCache *psCache,
	       struct PVRCachedObject *psObj,
	       bool bRetired)
{
	if (psCache->pfnPurgeCB != NULL)
	{
		psCache->pfnPurgeCB(psCache->pvCreateData,
				    psObj->pvObjectData,
				    bRetired);
	}
}

static inline bool
PVRObjectInCache(struct _PVRObjectCache *psCache,
		 struct PVRCachedObject *psObj,
		 void *pvInsertData)
{
	if (psCache->pfnCompareCB != NULL)
	{
		return psCache->pfnCompareCB(psCache->pvCreateData,
					     psObj->pvObjectData,
					     pvInsertData);
	}

	return false;
}

IMG_INTERNAL void
PVRObjectCachePurge(PVRObjectCache hObjectCache)
{
	struct _PVRObjectCache *psCache = (struct _PVRObjectCache *)hObjectCache;
	PVRQElem *psQElem;
	struct PVRCachedObject *psObj;

	FOR_QELEM_QOBJ(psCache, psQElem, psObj)
	{
		PVRObjectPurge(psCache, psObj, false);

		psObj->bValid = false;
	}
}


IMG_INTERNAL bool
PVRObjectCacheInsert(PVRObjectCache hObjectCache,
		     void *pvInsertData)
{
	struct _PVRObjectCache *psCache = (struct _PVRObjectCache *)hObjectCache;
	struct PVRCachedObject *psObj;
	bool bInserted = false;
	PVRQElem *psQElem;

	/* Is the object already in the cache ? */
	FOR_QELEM_QOBJ(psCache, psQElem, psObj)
	{
		/* Cache hit? */
		if (PVRObjectInCache(psCache, psObj, pvInsertData))
		{
			/* Cache hit on first element? */
			if (psQElem == psCache->sQHead.pvForw)
			{
				return true;
			}

			/* The old object purge will bump the age to 1 */
			psObj->uAge = 0;
			bInserted = true;

			/* Move the object to the front of the queue */
			PVRQueueRemove(psObj);
			PVRQueueAddFront(psCache, psObj);

			break;
		}
	}

	/* Purge old objects */
	FOR_QELEM_QOBJ(psCache, psQElem, psObj)
	{
		/* Age the object */
		psObj->uAge++;

		/* Has the object been in the cache too long? */
		if (psCache->uMaxAge != 0 && psObj->uAge > psCache->uMaxAge)
		{
			PVRObjectPurge(psCache, psObj, true);

			psObj->bValid = false;

			/* Move the object to the back of the queue */
			PVRQueueRemove(psObj);
			PVRQueueAddBack(psCache, psObj);
		}
	}

	/*
	 * If the object wasn't added to the cache above, add it using the
	 * last entry in the queue.
	 */
	if (!bInserted)
	{
		PVRQElem *psQElem = psCache->sQHead.pvBack;
		struct PVRCachedObject *psObj = PVRQ_CONTAINER_OF(psQElem,
								  struct PVRCachedObject,
								  sQElem);

		if (psObj->bValid)
		{
			PVRObjectPurge(psCache, psObj, true);

			psObj->bValid = false;
		}

		psObj->uAge = 1;

		if (PVRObjectInsert(psCache, psObj, pvInsertData))
		{
			psObj->bValid = true;

			/* Move the object to the front of the queue */
			PVRQueueRemove(psObj);
			PVRQueueAddFront(psCache, psObj);

			bInserted = true;
		}
	}

	return bInserted;
}

IMG_INTERNAL void *
PVRObjectCacheGetObject(PVRObjectCache hObjectCache, unsigned uObj)
{
	struct _PVRObjectCache *psCache = (struct _PVRObjectCache *)hObjectCache;
	unsigned uCount = uObj;
	PVRQElem *psQElem;
	struct PVRCachedObject *psObj;

	FOR_QELEM_QOBJ(psCache, psQElem, psObj)
	{
		if ((uCount--) == 0)
		{
			return psObj->pvObjectData;
		}
	}

	return NULL;
}

IMG_INTERNAL void
PVRObjectCacheDestroy(PVRObjectCache hObjectCache)
{
	struct _PVRObjectCache *psCache = (struct _PVRObjectCache *)hObjectCache;

	PVRObjectCachePurge(psCache);

	while (psCache->sQHead.pvForw != &psCache->sQHead)
	{
		struct PVRCachedObject *psObj = PVRQ_CONTAINER_OF(psCache->sQHead.pvForw,
								  struct PVRCachedObject,
								  sQElem);

		PVRQueueRemove(psObj);
		free(psObj);
	}

	free(psCache);
}

IMG_INTERNAL PVRObjectCache
PVRObjectCacheCreate(unsigned uNumObj,
		     unsigned uMaxAge,
		     void *pvCreateData,
		     PVRObjectCacheInsertCB pfnInsertCB,
		     PVRObjectCachePurgeCB pfnPurgeCB,
		     PVRObjectCacheCompareCB pfnCompareCB)
{
	struct _PVRObjectCache *psCache;
	unsigned i;

	if (uNumObj == 0 || pfnInsertCB == NULL)
	{
		return NULL;
	}

	psCache = calloc(1, sizeof(*psCache));
	if (psCache == NULL)
	{
		return NULL;
	}

	psCache->uNumObj = uNumObj;
	psCache->uMaxAge = uMaxAge;
	psCache->pvCreateData = pvCreateData;
	psCache->pfnInsertCB = pfnInsertCB;
	psCache->pfnPurgeCB = pfnPurgeCB;
	psCache->pfnCompareCB = pfnCompareCB;

	INITIALISE_PVRQ_HEAD(&psCache->sQHead);

	for (i = 0; i < uNumObj; i++)
	{
		struct PVRCachedObject *psObj;

		psObj = calloc(1, sizeof(*psObj));
		if (psObj == NULL)
		{
			goto ExitError;
		}

		PVRQueueAddBack(psCache, psObj);
	}

	return psCache;

ExitError:
	PVRObjectCacheDestroy(psCache);

	return NULL;
}
