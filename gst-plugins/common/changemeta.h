#pragma once

#include <gst/gst.h>
#include "detectionmeta.h"

G_BEGIN_DECLS

typedef struct _GstChangeMeta GstChangeMeta;

/**
 * GstChangeMeta:
 *
 * Metadata to capture information about changes between buffers.
 */
struct _GstChangeMeta
{
    GstMeta meta;

    gboolean changed;
    GArray *regions; // Array of bounding boxes
};

GType gst_change_meta_api_get_type(void);
const GstMetaInfo *gst_change_meta_get_info(void);
#define GST_CHANGE_META_GET(buf) ((GstChangeMeta *)gst_buffer_get_meta(buf, gst_change_meta_api_get_type()))
#define GST_CHANGE_META_ADD(buf) ((GstChangeMeta *)gst_buffer_add_meta(buf, gst_change_meta_get_info(), NULL))

G_END_DECLS
