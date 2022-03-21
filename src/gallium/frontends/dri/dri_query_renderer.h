#ifndef DRI_QUERY_RENDERER_H
#define DRI_QUERY_RENDERER_H

#include "dri_util.h"

int
driQueryRendererIntegerCommon(__DRIscreen *psp, int param, unsigned int *value);

extern const
__DRI2rendererQueryExtension dri2RendererQueryExtension;

#endif
