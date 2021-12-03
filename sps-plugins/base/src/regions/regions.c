#include <sps/sps.h>
#include <gst/gst.h>
#include "regions.h"
#include "../spspluginbase.h"
#include "gui-regions.h"

GVariant *create_regions_settings(GstElement *element)
{
    if (strcmp(gst_element_get_name(element), "regions") != 0 && GST_IS_BIN(element))
    {
        // Parent was passed. Retrieve actual element
        element = gst_bin_get_by_name(GST_BIN(element), "regions");
    }

    gboolean active;
    const char *prefix;
    g_object_get(element, "active", &active, "prefix", &prefix, NULL);

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
    GValue regions = G_VALUE_INIT;
    g_object_get_property(G_OBJECT(element), "regions", &regions);
    for (gint i = 0; i < gst_value_array_get_size(&regions); i++)
    {
        const GValue *region = gst_value_array_get_value(&regions, i);
        g_variant_builder_add(builder, "s", g_value_get_string(region));
    }
    g_value_unset(&regions);

    GVariant *value = g_variant_new(REGIONS_SETTINGS_FORMAT, active, prefix, builder);
    g_variant_builder_unref(builder);

    return value;
}

static void apply_element_settings(GVariant *variant, GstElement *element)
{
    if (strcmp(gst_element_get_name(element), "regions") != 0 && GST_IS_BIN(element))
    {
        // Parent was passed. Retrieve actual element
        element = gst_bin_get_by_name(GST_BIN(element), "regions");
    }
    
    gboolean active;
    const char *prefix;
    GVariantIter *regionsArray;
    g_variant_get(variant, REGIONS_SETTINGS_FORMAT, &active, &prefix, &regionsArray);

    GValue regions = G_VALUE_INIT;
    gst_value_array_init(&regions, 0);
    const char *region;
    while (g_variant_iter_next(regionsArray, "s", &region, NULL))
    {
        GValue regionValue = G_VALUE_INIT;
        g_value_init(&regionValue, G_TYPE_STRING);

        g_value_set_string(&regionValue, region);
        gst_value_array_append_value(&regions, &regionValue);

        g_value_unset(&regionValue);
        g_free((void *)region);
    }
    g_variant_iter_free(regionsArray);

    // Configure element
    g_object_set(G_OBJECT(element), "active", active, "prefix", prefix, NULL);
    g_object_set_property(G_OBJECT(element), "regions", &regions);

    // Free values
    g_value_unset(&regions);
    g_free((void *)prefix);
}

static GstElement *create_element()
{
    gboolean success = TRUE;
    GstElement *pipeline = gst_bin_new(NULL);

    // Define helper elements
    GstElement *tee, *outQueue, *appQueue, *appSink;
    tee = gst_element_factory_make("tee", NULL);
    outQueue = gst_element_factory_make("queue", NULL);
    appQueue = gst_element_factory_make("queue", NULL);
    appSink = gst_element_factory_make("appsink", "appsink");

    // Define and configure main detector
    GstElement *element = gst_element_factory_make("regions", "regions");
    if (!tee || !element || !appSink || !outQueue || !appQueue)
    {
        GST_ERROR("Error creating elements.");
        return NULL;
    }

    // Set default values
    GSettings *settings = sps_plugin_get_settings(self);
    GVariant *value = g_settings_get_value(settings, "regions");
    apply_element_settings(value, element);
    g_variant_unref(value);

    // Setup appsink
    GstCaps *caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGRA", NULL);
    g_object_set(appSink, "drop", TRUE, "max-buffers", 1, "caps", caps, NULL);
    gst_caps_unref(caps);

    // Add to subpipe and link
    gst_bin_add_many(GST_BIN(pipeline), element, tee, outQueue, appQueue, appSink, NULL);
    success &= gst_element_link(element, tee);
    success &= gst_element_link_many(tee, appQueue, appSink, NULL);
    success &= gst_element_link(tee, outQueue);

    // Add ghost pads
    GstPad *sinkPad = gst_element_get_static_pad(element, "sink");
    gst_element_add_pad(pipeline, gst_ghost_pad_new("sink", sinkPad));
    gst_object_unref(sinkPad);
    GstPad *srcPad = gst_element_get_static_pad(outQueue, "src");
    gst_element_add_pad(pipeline, gst_ghost_pad_new("src", srcPad));
    gst_object_unref(srcPad);

    if (!success)
    {
        GST_ERROR("Unable to build pipeline.");
        return NULL;
    }

    return pipeline;
}

static GtkWidget *create_settings_gui(GstElement *element)
{
    return GTK_WIDGET(sps_plugin_base_gui_regions_new(element));
}

SpsPluginElementFactory *get_factory_regions()
{
    SpsPluginElementFactory *factory;
    factory = g_malloc(sizeof(SpsPluginElementFactory));
    factory->create_element = &create_element;
    factory->create_settings_gui = &create_settings_gui;
    factory->create_element_settings = &create_regions_settings;
    factory->apply_element_settings = &apply_element_settings;

    return factory;
}
