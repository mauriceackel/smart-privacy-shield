#include <gtk/gtk.h>
#include <gst/gst.h>
#include <sps/sps.h>
#include "gui-changedetector.h"
#include "changedetector.h"
#include "../spspluginbase.h"

G_DEFINE_TYPE(SpsPluginBaseGuiChangedetector, sps_plugin_base_gui_changedetector, GTK_TYPE_BOX)

enum
{
    PROP_0,
    PROP_ELEMENT,
};

static void sps_plugin_base_gui_changedetector_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseGuiChangedetector *gui = SPS_PLUGIN_BASE_GUI_CHANGEDETECTOR(object);

    switch (prop_id)
    {
    case PROP_ELEMENT:
        gui->element = gst_object_ref(g_value_get_pointer(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void sps_plugin_base_gui_changedetector_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseGuiChangedetector *gui = SPS_PLUGIN_BASE_GUI_CHANGEDETECTOR(object);

    switch (prop_id)
    {
    case PROP_ELEMENT:
        g_value_set_pointer(value, gui->element);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void cb_active_toggled(GtkCheckButton *cb, SpsPluginBaseGuiChangedetector *gui)
{
    // Get state
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb));

    // Update element
    g_object_set(G_OBJECT(gui->element), "active", active, NULL);
}

static void txt_threshold_changed(GtkSpinButton *txt, SpsPluginBaseGuiChangedetector *gui)
{
    // Get new value
    gint threshold = gtk_spin_button_get_value_as_int(txt);

    // Update element
    g_object_set(G_OBJECT(gui->element), "threshold", threshold, NULL);
}

static void btn_store_defaults_handler(GtkButton *btn, SpsPluginBaseGuiChangedetector *gui) {
    GSettings *settings = sps_plugin_get_settings(self);

    GVariant *value = create_changedetector_settings(gui->element);
    g_settings_set_value(settings, "changedetector", value); // Consumes the reference
}

static void sps_plugin_base_gui_changedetector_constructed(GObject *object)
{
    SpsPluginBaseGuiChangedetector *gui = SPS_PLUGIN_BASE_GUI_CHANGEDETECTOR(object);

    // Get values
    gboolean active;
    gint threshold;
    g_object_get(G_OBJECT(gui->element), "active", &active, "threshold", &threshold, NULL);

    // Set active
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->cbActive), active);

    // Set threshold
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui->txtThreshold), threshold);

    G_OBJECT_CLASS(sps_plugin_base_gui_changedetector_parent_class)->constructed(object);
}

static void sps_plugin_base_gui_changedetector_init(SpsPluginBaseGuiChangedetector *gui)
{
    gtk_widget_init_template(GTK_WIDGET(gui));
}

static void sps_plugin_base_gui_changedetector_finalize(GObject *object)
{
    SpsPluginBaseGuiChangedetector *gui = SPS_PLUGIN_BASE_GUI_CHANGEDETECTOR(object);

    gst_object_unref(gui->element);

    G_OBJECT_CLASS(sps_plugin_base_gui_changedetector_parent_class)->finalize(object);
}

static void sps_plugin_base_gui_changedetector_class_init(SpsPluginBaseGuiChangedetectorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->set_property = sps_plugin_base_gui_changedetector_set_property;
    object_class->get_property = sps_plugin_base_gui_changedetector_get_property;
    object_class->constructed = sps_plugin_base_gui_changedetector_constructed;
    object_class->finalize = sps_plugin_base_gui_changedetector_finalize;

    // Register properties
    g_object_class_install_property(object_class, PROP_ELEMENT,
                                    g_param_spec_pointer("element", "Element",
                                                         "The element this gui refers to",
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    // Load template from resources
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/plugins/base/gui/changedetector.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiChangedetector, btnStoreDefaults);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiChangedetector, lblHeading);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiChangedetector, lblDescription);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiChangedetector, cbActive);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiChangedetector, txtThreshold);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiChangedetector, adjThreshold);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, cb_active_toggled);
    gtk_widget_class_bind_template_callback(widget_class, txt_threshold_changed);
    gtk_widget_class_bind_template_callback(widget_class, btn_store_defaults_handler);
}

SpsPluginBaseGuiChangedetector *sps_plugin_base_gui_changedetector_new(GstElement *element)
{
    return g_object_new(SPS_PLUGIN_BASE_TYPE_GUI_CHANGEDETECTOR, "element", element, NULL);
}
