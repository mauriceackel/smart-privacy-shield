#include "windowlocationsmeta.h"
#include <gst/gst.h>

void window_info_clear(gpointer pointer)
{
    WindowInfo *winInfo = (WindowInfo *)pointer;
    g_free((void *)winInfo->ownerName);
    g_free((void *)winInfo->windowName);
}

void window_info_copy(WindowInfo *in, WindowInfo *out)
{
    out->ownerName = g_strdup(in->ownerName);
    out->windowName = g_strdup(in->windowName);
    out->zIndex = in->zIndex;
    out->bbox = in->bbox;
    out->id = in->id; 
}

// Define the new metadata type
GType gst_window_locations_meta_api_get_type(void)
{
    static GType type;
    static const gchar *tags[] = {"memory", NULL};

    if (g_once_init_enter(&type))
    {
        GType _type = gst_meta_api_type_register("GstWindowLocationsMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }

    return type;
}

// Set the initial values of the metadata
static gboolean gst_window_locations_meta_init(GstMeta *basemeta, gpointer params, GstBuffer *buffer)
{
    GstWindowLocationsMeta *meta = (GstWindowLocationsMeta *)basemeta;

    meta->windowInfos = g_array_new(FALSE, FALSE, sizeof(WindowInfo));
    g_array_set_clear_func(meta->windowInfos, window_info_clear);

    return TRUE;
}

// Finalize metadata
static gboolean gst_window_locations_meta_finalize(GstMeta *basemeta, GstBuffer *buffer)
{
    GstWindowLocationsMeta *meta = (GstWindowLocationsMeta *)basemeta;

    g_array_free(meta->windowInfos, TRUE);

    return TRUE;
}

// Transform meta object
static gboolean gst_window_locations_meta_transform(GstBuffer *dest, GstMeta *meta, GstBuffer *src, GQuark type, gpointer data)
{
    GstWindowLocationsMeta *oldMeta, *newMeta;
    oldMeta = (GstWindowLocationsMeta *)meta;

    if (GST_META_TRANSFORM_IS_COPY(type))
    {
        // Create new meta on destination
        newMeta = GST_WINDOW_LOCATIONS_META_ADD(dest);

        if (!newMeta)
            return FALSE;

        // Copy over content
        WindowInfo newInfo;
        for(gint i = 0; i < oldMeta->windowInfos->len; i++)
        {
            window_info_copy(&g_array_index(oldMeta->windowInfos, WindowInfo, i), &newInfo);
            g_array_append_val(newMeta->windowInfos, newInfo);
        }

        return TRUE;
    }
    else
    {
        // transform type not supported
        return FALSE;
    }
}

// Build a metadata info object
const GstMetaInfo *gst_window_locations_meta_get_info(void)
{
    static const GstMetaInfo *window_locations_meta_info = NULL;

    if (g_once_init_enter(&window_locations_meta_info))
    {
        const GstMetaInfo *meta =
            gst_meta_register(gst_window_locations_meta_api_get_type(), "GstWindowLocationsMeta",
                              sizeof(GstWindowLocationsMeta), (GstMetaInitFunction)gst_window_locations_meta_init,
                              (GstMetaFreeFunction)gst_window_locations_meta_finalize, (GstMetaTransformFunction)gst_window_locations_meta_transform);
        g_once_init_leave(&window_locations_meta_info, meta);
    }

    return window_locations_meta_info;
}
