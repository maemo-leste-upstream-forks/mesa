/*************************************************************************/ /*!
@File           sgx_queue.h
@Title          Queue related definitions
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

#if !defined(__PVRQUEUE_H__)
#define __PVRQUEUE_H__
#include <assert.h>
#include <search.h>
#include <stddef.h>

#define	PVRQ_CONTAINER_OF(p, t, f) ((t *)((char *)(p) - offsetof(t, f)))

typedef struct _PVRQElem
{
	struct _PVRQElem *pvForw;
	struct _PVRQElem *pvBack;
} PVRQElem;

typedef PVRQElem PVRQHead;

#define DECLARE_PVRQ_HEAD(h) PVRQHead h = {&h, &h}

static inline void INITIALISE_PVRQ_HEAD(PVRQHead *ph)
{
	ph->pvForw = ph->pvBack = ph;
	insque(ph, ph);
}

static inline int PVRQIsEmpty(PVRQHead *ph)
{
	return ph->pvForw == ph && ph->pvBack == ph;
}

static inline void PVRQQueue(PVRQHead *ph, PVRQElem *pe)
{
	assert(pe->pvForw == NULL && pe->pvBack == NULL);

	insque(pe, ph->pvBack);
}

static inline void PVRQDequeue(PVRQElem *pe)
{
	remque(pe);

	pe->pvForw = NULL;
	pe->pvBack = NULL;
}

#endif /* defined(__PVRQUEUE_H__) */
