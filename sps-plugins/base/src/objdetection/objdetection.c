#include <sps/sps.h>
#include <gst/gst.h>
#include "objdetection.h"
#include "../spspluginbase.h"
#include "gui-objdetection.h"

GVariant *create_objdetection_settings(GstElement *element)
{
    if (strcmp(gst_element_get_name(element), "objdetection") != 0 && GST_IS_BIN(element))
    {
        // Parent was passed. Retrieve actual element
        element = gst_bin_get_by_name(GST_BIN(element), "objdetection");
    }

    gboolean active;
    const char *prefix, *modelPath;
    g_object_get(element, "active", &active, "prefix", &prefix, "model-path", &modelPath, NULL);

    return g_variant_new(OBJDETECTION_SETTINGS_FORMAT, active, prefix, modelPath);
}

static void apply_element_settings(GVariant *variant, GstElement *element)
{
    if (strcmp(gst_element_get_name(element), "objdetection") != 0 && GST_IS_BIN(element))
    {
        // Parent was passed. Retrieve actual element
        element = gst_bin_get_by_name(GST_BIN(element), "objdetection");
    }

    gboolean active;
    const char *prefix, *modelPath;
    g_variant_get(variant, OBJDETECTION_SETTINGS_FORMAT, &active, &prefix, &modelPath);

    // Configure element
    g_object_set(G_OBJECT(element), "active", active, "prefix", prefix, "model-path", modelPath, NULL);

    // Free values
    g_free((void *)prefix);
    g_free((void *)modelPath);
}

static GstElement *create_element()
{
    gboolean success = TRUE;
    GstElement *pipeline = gst_bin_new(NULL);

    // Define helper elements
    GstElement *tee, *queueModel, *queueBypass, *videoConvert, *videoScale;
    tee = gst_element_factory_make("tee", NULL);
    queueModel = gst_element_factory_make("queue", NULL);
    queueBypass = gst_element_factory_make("queue", NULL);
    videoConvert = gst_element_factory_make("videoconvert", NULL);
    videoScale = gst_element_factory_make("videoscale", NULL);

    // Define and configure main detector
    GstElement *objdetection = gst_element_factory_make("objdetection", "objdetection");
    if (!tee || !queueModel || !queueBypass || !videoConvert || !videoScale || !objdetection)
    {
        GST_ERROR("Error creating elements.");
        return NULL;
    }

    // Set default values
    GSettings *settings = sps_plugin_get_settings(self);
    GVariant *value = g_settings_get_value(settings, "objdetection");
    apply_element_settings(value, objdetection);
    g_variant_unref(value);

    // Add to subpipe and link
    gst_bin_add_many(GST_BIN(pipeline), tee, queueModel, queueBypass, videoConvert, videoScale, objdetection, NULL);
    success &= gst_element_link_many(tee, queueModel, videoConvert, videoScale, NULL);
    success &= gst_element_link_pads(videoScale, NULL, objdetection, "model_sink");
    success &= gst_element_link(tee, queueBypass);
    success &= gst_element_link_pads(queueBypass, NULL, objdetection, "bypass_sink");

    // Add ghost pads
    GstPad *sinkPad = gst_element_get_static_pad(tee, "sink");
    gst_element_add_pad(pipeline, gst_ghost_pad_new("sink", sinkPad));
    gst_object_unref(sinkPad);
    GstPad *srcPad = gst_element_get_static_pad(objdetection, "src");
    gst_element_add_pad(pipeline, gst_ghost_pad_new("src", srcPad));
    gst_object_unref(srcPad);

    if (!success)
    {
        GST_ERROR("Unable to build pipeline.");
        return NULL;
    }

    return pipeline;
}

static GtkWidget *create_settings_gui(GstElement *bin)
{
    return GTK_WIDGET(sps_plugin_base_gui_objdetection_new(bin));
}

SpsPluginElementFactory *get_factory_objdetection()
{
    SpsPluginElementFactory *factory;
    factory = g_malloc(sizeof(SpsPluginElementFactory));
    factory->create_element = &create_element;
    factory->create_settings_gui = &create_settings_gui;
    factory->create_element_settings = &create_objdetection_settings;
    factory->apply_element_settings = &apply_element_settings;

    return factory;
}
