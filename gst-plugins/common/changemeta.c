#include <gst/gst.h>
#include "changemeta.h"
#include "detectionmeta.h"

// Define the new metadata type
GType gst_change_meta_api_get_type(void)
{
    static GType type;
    static const gchar *tags[] = {"memory", NULL};

    if (g_once_init_enter(&type))
    {
        GType _type = gst_meta_api_type_register("GstChangeMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }

    return type;
}

// Set the initial values of the metadata
static gboolean gst_change_meta_init(GstMeta *basemeta, gpointer params, GstBuffer *buffer)
{
    GstChangeMeta *meta = (GstChangeMeta *)basemeta;

    meta->changed = FALSE;
    meta->regions = g_array_new(FALSE, FALSE, sizeof(BoundingBox));

    return TRUE;
}

// Free meta object
static void gst_change_meta_free(GstMeta *basemeta, GstBuffer *buffer)
{
    GstChangeMeta *meta = (GstChangeMeta *)basemeta;

    // Free regions array
    g_array_free(meta->regions, TRUE);
}

// Transform meta object
static gboolean gst_change_meta_transform(GstBuffer *dest, GstMeta *meta, GstBuffer *src, GQuark type, gpointer data)
{
    GstChangeMeta *oldMeta, *newMeta;
    oldMeta = (GstChangeMeta *)meta;

    if (GST_META_TRANSFORM_IS_COPY(type))
    {
        // Create new meta on destination
        newMeta = GST_CHANGE_META_ADD(dest);

        if (!newMeta)
            return FALSE;

        // Copy over content
        newMeta->changed = oldMeta->changed;
        g_array_free(newMeta->regions, TRUE);
        newMeta->regions = g_array_copy(oldMeta->regions);

        return TRUE;
    }
    else
    {
        // Transform type not supported
        return FALSE;
    }
}

// Build a metadata info object
const GstMetaInfo *gst_change_meta_get_info(void)
{
    static const GstMetaInfo *change_meta_info = NULL;

    if (g_once_init_enter(&change_meta_info))
    {
        const GstMetaInfo *meta =
            gst_meta_register(gst_change_meta_api_get_type(), "GstChangeMeta",
                              sizeof(GstChangeMeta), (GstMetaInitFunction)gst_change_meta_init,
                              (GstMetaFreeFunction)gst_change_meta_free, (GstMetaTransformFunction)gst_change_meta_transform);
        g_once_init_leave(&change_meta_info, meta);
    }

    return change_meta_info;
}
