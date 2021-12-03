
#include <gst/gst.h>
#include <gst/video/video.h>
#include "gstautocompositor.h"
#include "compose.h"

GST_DEBUG_CATEGORY(gst_auto_compositor_debug);
G_DEFINE_TYPE(GstAutoCompositor, gst_auto_compositor, GST_TYPE_BIN);

static void gst_auto_compositor_recompute_positions(GstAutoCompositor *self)
{
    GstElement *compositor = self->compositor;

    GArray *compData = g_array_new(FALSE, FALSE, sizeof(CompositionData));

    // Recompute positions
    for (GList *l = compositor->sinkpads; l != NULL; l = l->next)
    {
        // Get caps of pad
        GstCaps *caps = gst_pad_get_current_caps(GST_PAD(l->data));
        if (!caps)
            continue;
        GstVideoInfo vInfo;
        gst_video_info_from_caps(&vInfo, caps);
        gst_caps_unref(caps);

        // Prepare composition data
        CompositionData data = {
            .pad = GST_PAD(l->data),
            .width = vInfo.width,
            .height = vInfo.height,
        };
        g_array_append_val(compData, data);
    }

    compute_positioning(compData, 1.6f);

    for (gint i = 0; i < compData->len; i++)
    {
        CompositionData data = g_array_index(compData, CompositionData, i);
        g_object_set(G_OBJECT(data.pad), "xpos", data.x, "ypos", data.y, NULL);
    }
}

static gboolean gst_auto_compositor_pad_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    // Handle event as normal
    gboolean ret = gst_pad_event_default(pad, parent, event);

    // If this was a caps event, recompute positions
    if (event->type == GST_EVENT_CAPS)
    {
        GstAutoCompositor *self = GST_AUTO_COMPOSITOR(parent);
        gst_auto_compositor_recompute_positions(self);
    }

    return ret;
}

static GstPad *gst_auto_compositor_request_new_pad(GstElement *element, GstPadTemplate *templ, const gchar *name, const GstCaps *caps)
{
    GstAutoCompositor *self = GST_AUTO_COMPOSITOR(element);

    // Create the pad on the compositor
    GstPad *pad = gst_element_request_pad(self->compositor, templ, name, caps);

    // Create ghost pad for the bin
    GstPad *ghostPad = gst_ghost_pad_new(gst_pad_get_name(pad), pad);
    // Register events for recomputation
    gst_pad_set_event_function(ghostPad, gst_auto_compositor_pad_event);
    // Add ghost pad to bin
    gst_element_add_pad(GST_ELEMENT(self), ghostPad);

    gst_object_unref(pad);

    return ghostPad;
}

static void gst_auto_compositor_release_pad(GstElement *element, GstPad *pad)
{
    GstAutoCompositor *self = GST_AUTO_COMPOSITOR(element);

    // Get pad on compositor
    GstPad *targetPad = gst_ghost_pad_get_target(GST_GHOST_PAD(pad));

    // Remove pad on compositor
    gst_element_release_request_pad(self->compositor, targetPad);
    gst_object_unref(targetPad);

    // Recompute positions
    gst_auto_compositor_recompute_positions(self);

    // Remove pad on the bin
    gst_element_remove_pad(GST_ELEMENT(self), pad);
}

static void gst_auto_compositor_init(GstAutoCompositor *self)
{
    // Create wrapped compositor element
    GstElement *compositor = gst_element_factory_make("compositor", NULL);
    self->compositor = compositor;
    g_return_if_fail(compositor != NULL);

    // Configure and add to bin
    g_object_set(G_OBJECT(compositor), "background", 1, NULL); // Background black
    gst_bin_add(GST_BIN(self), compositor);

    // Ghost compositor source pad to bin
    GstPad *pad = GST_AGGREGATOR_SRC_PAD(GST_AGGREGATOR(compositor));
    GstPad *ghostPad = gst_ghost_pad_new(gst_pad_get_name(pad), pad);
    gst_element_add_pad(GST_ELEMENT(self), ghostPad);

    // Trade pad templates
    GstElementClass *selfClass = GST_ELEMENT_GET_CLASS(self);
    GstElementClass *compositorClass = GST_ELEMENT_GET_CLASS(self->compositor);
    for (GList *l = compositorClass->padtemplates; l != NULL; l = l->next)
        gst_element_class_add_pad_template(selfClass, GST_PAD_TEMPLATE(l->data));
}

static void gst_auto_compositor_class_init(GstAutoCompositorClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GstElementClass *gstelement_class = (GstElementClass *)klass;

    gstelement_class->request_new_pad = gst_auto_compositor_request_new_pad;
    gstelement_class->release_pad = gst_auto_compositor_release_pad;

    // Set plugin metadata
    gst_element_class_set_static_metadata(gstelement_class,
                                          "Auto compositor", "Filter/Editor/Video/Compositor",
                                          "Auto compositor", "Maurice Ackel <m.ackel@icloud.com>");
}

// Plugin initializer that registers the plugin
static gboolean plugin_init(GstPlugin *plugin)
{
    gboolean ret;

    GST_DEBUG_CATEGORY_INIT(gst_auto_compositor_debug, "autocompositor", 0, "autocompositor debug");

    ret = gst_element_register(plugin, "autocompositor", GST_RANK_NONE, GST_TYPE_AUTO_COMPOSITOR);

    return ret;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  autocompositor,
                  "Auto compositor plugin",
                  plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
