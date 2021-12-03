#pragma once

#include <gst/gst.h>
#include "gstchangedetector.h"

#ifdef __cplusplus
extern "C"
{
#endif

void detect_changes(GstChangeDetector *filter, GstBuffer *current, GstBuffer *previous);

#ifdef __cplusplus
}
#endif
