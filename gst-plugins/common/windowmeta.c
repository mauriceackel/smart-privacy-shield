#include "windowmeta.h"
#include <gst/gst.h>

// Define the new metadata type
GType gst_window_meta_api_get_type(void)
{
    static GType type;
    static const gchar *tags[] = {"memory", NULL};

    if (g_once_init_enter(&type))
    {
        GType _type = gst_meta_api_type_register("GstWindowMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }

    return type;
}

// Set the initial values of the metadata
static gboolean gst_window_meta_init(GstMeta *basemeta, gpointer params, GstBuffer *buffer)
{
    GstWindowMeta *meta = (GstWindowMeta *)basemeta;

    meta->width = meta->height = meta->x = meta->y = 0;

    return TRUE;
}

// Transform meta object
static gboolean gst_window_meta_transform(GstBuffer *dest, GstMeta *meta, GstBuffer *src, GQuark type, gpointer data)
{
    GstWindowMeta *oldMeta, *newMeta;
    oldMeta = (GstWindowMeta *)meta;

    if (GST_META_TRANSFORM_IS_COPY(type))
    {
        // Create new meta on destination
        newMeta = GST_WINDOW_META_ADD(dest);

        if (!newMeta)
            return FALSE;

        // Copy over content
        newMeta->width = oldMeta->width;
        newMeta->height = oldMeta->height;
        newMeta->x = oldMeta->x;
        newMeta->y = oldMeta->y;

        return TRUE;
    }
    else
    {
        // transform type not supported
        return FALSE;
    }
}

// Build a metadata info object
const GstMetaInfo *gst_window_meta_get_info(void)
{
    static const GstMetaInfo *window_meta_info = NULL;

    if (g_once_init_enter(&window_meta_info))
    {
        const GstMetaInfo *meta =
            gst_meta_register(gst_window_meta_api_get_type(), "GstWindowMeta",
                              sizeof(GstWindowMeta), (GstMetaInitFunction)gst_window_meta_init,
                              (GstMetaFreeFunction)NULL, (GstMetaTransformFunction)gst_window_meta_transform);
        g_once_init_leave(&window_meta_info, meta);
    }

    return window_meta_info;
}
