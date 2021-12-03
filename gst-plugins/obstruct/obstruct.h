#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <detectionmeta.h>

enum _FilterType
{
    FILTER_TYPE_BLACK,
    FILTER_TYPE_WHITE,
    FILTER_TYPE_BLUR,
};
typedef enum _FilterType FilterType;

gboolean apply_filter(GstVideoMeta *meta, guint8 *data, GstDetection *detection, FilterType filterType, gfloat margin);

#ifdef __cplusplus
}
#endif
