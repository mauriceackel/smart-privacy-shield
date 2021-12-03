#include <sps/sps.h>
#include <gst/gst.h>
#include "../spspluginbase.h"
#include "obstruct.h"
#include "gui-obstruct.h"

GVariant *create_obstruct_settings(GstElement *element)
{
    gboolean active, invert;
    gint filterType;
    gfloat margin;
    g_object_get(element, "active", &active, "invert", &invert, "filter-type", &filterType, "margin", &margin, NULL);

    GValue labels = G_VALUE_INIT;
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
    g_object_get_property(G_OBJECT(element), "labels", &labels);
    guint size = gst_value_array_get_size(&labels);
    for (gint i = 0; i < size; i++)
    {
        const GValue *label = gst_value_array_get_value(&labels, i);
        g_variant_builder_add(builder, "s", g_value_get_string(label));
    }
    g_value_unset(&labels);

    GVariant *value = g_variant_new(OBSTRUCT_SETTINGS_FORMAT, active, invert, filterType, margin, builder);
    g_variant_builder_unref(builder);

    return value;
}

static void apply_element_settings(GVariant *variant, GstElement *element)
{
    gboolean active, invert;
    gint filterType;
    gdouble margin;
    GVariantIter *labelArray;
    g_variant_get(variant, OBSTRUCT_SETTINGS_FORMAT, &active, &invert, &filterType, &margin, &labelArray);

    GValue labels = G_VALUE_INIT;
    gst_value_array_init(&labels, 0);
    const char *label;
    while (g_variant_iter_next(labelArray, "s", &label, NULL))
    {
        GValue labelValue = G_VALUE_INIT;
        g_value_init(&labelValue, G_TYPE_STRING);

        g_value_set_string(&labelValue, label);
        gst_value_array_append_value(&labels, &labelValue);

        g_value_unset(&labelValue);
        g_free((void *)label);
    }
    g_variant_iter_free(labelArray);

    // Configure element
    g_object_set(G_OBJECT(element), "active", active, "invert", invert, "filter-type", filterType, "margin", (gfloat)margin, NULL);
    g_object_set_property(G_OBJECT(element), "labels", &labels);

    // Free values
    g_value_unset(&labels);
}

static GstElement *create_element()
{
    GstElement *element = gst_element_factory_make("obstruct", NULL);

    // Set defaults
    GSettings *settings = sps_plugin_get_settings(self);
    GVariant *value = g_settings_get_value(settings, "obstruct");
    apply_element_settings(value, element);
    g_variant_unref(value);

    return element;
}

static GtkWidget *create_settings_gui(GstElement *element)
{
    return GTK_WIDGET(sps_plugin_base_gui_obstruct_new(element));
}

SpsPluginElementFactory *get_factory_obstruct()
{
    SpsPluginElementFactory *factory;
    factory = g_malloc(sizeof(SpsPluginElementFactory));
    factory->create_element = &create_element;
    factory->create_settings_gui = &create_settings_gui;
    factory->create_element_settings = &create_obstruct_settings;
    factory->apply_element_settings = &apply_element_settings;

    return factory;
}
