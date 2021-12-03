#include <sps/sps.h>
#include <gst/gst.h>
#include "../spspluginbase.h"
#include "changedetector.h"
#include "gui-changedetector.h"

GVariant *create_changedetector_settings(GstElement *element)
{
    gboolean active;
    gint threshold;
    g_object_get(element, "active", &active, "threshold", &threshold, NULL);

    return g_variant_new(CHANGEDETECTOR_SETTINGS_FORMAT, active, threshold);
}

static void apply_element_settings(GVariant *variant, GstElement *element)
{
    gboolean active;
    gint threshold;
    g_variant_get(variant, CHANGEDETECTOR_SETTINGS_FORMAT, &active, &threshold);
    g_object_set(element, "active", active, "threshold", threshold, NULL);
}

static GstElement *create_element()
{
    GstElement *element = gst_element_factory_make("changedetector", NULL);

    // Set default values
    GSettings *settings = sps_plugin_get_settings(self);
    GVariant *value = g_settings_get_value(settings, "changedetector");
    apply_element_settings(value, element);
    g_variant_unref(value);

    return element;
}

static GtkWidget *create_settings_gui(GstElement *element)
{
    return GTK_WIDGET(sps_plugin_base_gui_changedetector_new(element));
}

SpsPluginElementFactory *get_factory_changedetector()
{
    SpsPluginElementFactory *factory;
    factory = g_malloc(sizeof(SpsPluginElementFactory));
    factory->create_element = &create_element;
    factory->create_settings_gui = &create_settings_gui;
    factory->create_element_settings = &create_changedetector_settings;
    factory->apply_element_settings = &apply_element_settings;

    return factory;
}
