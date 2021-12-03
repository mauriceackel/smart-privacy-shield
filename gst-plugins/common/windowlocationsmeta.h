#pragma once

#include <gst/gst.h>
#include "detectionmeta.h"

G_BEGIN_DECLS

typedef struct _WindowInfo WindowInfo;
struct _WindowInfo
{
    const char *ownerName;
    const char *windowName;
    gint zIndex;
    BoundingBox bbox;
    guintptr id;
};

typedef struct _GstWindowLocationsMeta GstWindowLocationsMeta;

/**
 * GstWindowLocationsMeta:
 *
 * Metadata to capture information about all windows on the screen.
 */
struct _GstWindowLocationsMeta
{
    GstMeta meta;

    // Windows
    GArray *windowInfos;
};

GType gst_window_locations_meta_api_get_type(void);
const GstMetaInfo *gst_window_locations_meta_get_info(void);
#define GST_WINDOW_LOCATIONS_META_GET(buf) ((GstWindowLocationsMeta *)gst_buffer_get_meta(buf, gst_window_locations_meta_api_get_type()))
#define GST_WINDOW_LOCATIONS_META_ADD(buf) ((GstWindowLocationsMeta *)gst_buffer_add_meta(buf, gst_window_locations_meta_get_info(), NULL))

void window_info_clear(gpointer pointer);
void window_info_copy(WindowInfo *in, WindowInfo *out);

G_END_DECLS
