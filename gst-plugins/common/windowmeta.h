#pragma once

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstWindowMeta GstWindowMeta;

/**
 * GstWindowMeta:
 *
 * Metadata to capture information about a window on the screen.
 */
struct _GstWindowMeta
{
    GstMeta meta;

    // Window position and size on screen
    gint x, y;
    gint width, height;
};

GType gst_window_meta_get_type(void);
const GstMetaInfo *gst_window_meta_get_info(void);
#define GST_WINDOW_META_GET(buf) ((GstWindowMeta *)gst_buffer_get_meta(buf, gst_window_meta_api_get_type()))
#define GST_WINDOW_META_ADD(buf) ((GstWindowMeta *)gst_buffer_add_meta(buf, gst_window_meta_get_info(), NULL))

G_END_DECLS
