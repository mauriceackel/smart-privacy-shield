#pragma once

#include <gst/gst.h>

typedef struct _CompositionData CompositionData;
struct _CompositionData
{
    GstPad *pad;
    gint x, y;
    gint width, height;
};

void compute_positioning(GArray *compData, gdouble targetRatio);
