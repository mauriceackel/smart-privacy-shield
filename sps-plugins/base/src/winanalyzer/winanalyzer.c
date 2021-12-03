#include <sps/sps.h>
#include <gst/gst.h>
#include "winanalyzer.h"
#include "../spspluginbase.h"
#include "gui-winanalyzer.h"

GVariant *create_winanalyzer_settings(GstElement *element)
{
    if (strcmp(gst_element_get_name(element), "winanalyzer") != 0 && GST_IS_BIN(element))
    {
        // Parent was passed. Retrieve actual element
        element = gst_bin_get_by_name(GST_BIN(element), "winanalyzer");
    }

    gboolean active;
    const char *prefix;
    guint64 displayId;
    g_object_get(element, "active", &active, "prefix", &prefix, "display-id", &displayId, NULL);

    // Get the full SpsSourceInfo for the display id
    SpsSourceInfo sourceInfo = {
        .name = g_strdup(""),
        .internalName = g_strdup(""),
        .id = displayId,
        .type = SOURCE_TYPE_DISPLAY,
    };
    sps_sources_recover_display(&sourceInfo);

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE(WINANALYZER_SETTINGS_FORMAT));
    g_variant_builder_add(builder, "b", active);
    g_variant_builder_add(builder, "s", prefix);
    g_variant_builder_add(builder, "(ssxx)", sourceInfo.name, sourceInfo.internalName, (gint64)sourceInfo.id, (gint64)sourceInfo.type);

    GVariant *variant = g_variant_builder_end(builder);

    // Free memory
    sps_source_info_clear(&sourceInfo);

    return variant;
}

static void apply_element_settings(GVariant *variant, GstElement *element)
{
    if (strcmp(gst_element_get_name(element), "winanalyzer") != 0 && GST_IS_BIN(element))
    {
        // Parent was passed. Retrieve actual element
        element = gst_bin_get_by_name(GST_BIN(element), "winanalyzer");
    }

    gboolean active;
    const char *prefix = NULL;
    guint64 displayId = 0;

    const char *name, *internalName;
    gint64 id, type;

    g_variant_get(variant, WINANALYZER_SETTINGS_FORMAT, &active, &prefix, &name, &internalName, &id, &type);

    // Recover source info to get display id
    SpsSourceInfo sourceInfo = {
        .name = g_strdup(name),
        .internalName = g_strdup(internalName),
        .id = (guintptr)id,
        .type = (SpsSourceType)type,
    };

    if (sps_sources_recover_display(&sourceInfo))
        displayId = sourceInfo.id;

    // Configure element
    g_object_set(G_OBJECT(element), "active", active, "prefix", prefix, "display-id", displayId, NULL);

    // Free values
    g_free((void *)prefix);
    sps_source_info_clear(&sourceInfo);
}

static GstElement *create_element()
{
    // Define and configure main detector
    GstElement *winanalyzer = gst_element_factory_make("winanalyzer", "winanalyzer");

    // Set default values
    GSettings *settings = sps_plugin_get_settings(self);
    GVariant *value = g_settings_get_value(settings, "winanalyzer");
    apply_element_settings(value, winanalyzer);
    g_variant_unref(value);

    return winanalyzer;
}

static GtkWidget *create_settings_gui(GstElement *bin)
{
    return GTK_WIDGET(sps_plugin_base_gui_winanalyzer_new(bin));
}

SpsPluginElementFactory *get_factory_winanalyzer()
{
    SpsPluginElementFactory *factory;
    factory = g_malloc(sizeof(SpsPluginElementFactory));
    factory->create_element = &create_element;
    factory->create_settings_gui = &create_settings_gui;
    factory->create_element_settings = &create_winanalyzer_settings;
    factory->apply_element_settings = &apply_element_settings;

    return factory;
}
