#include <gtk/gtk.h>
#include <gst/gst.h>
#include "gui-objdetection.h"
#include "../spspluginbase.h"
#include "objdetection.h"

G_DEFINE_TYPE(SpsPluginBaseGuiObjdetection, sps_plugin_base_gui_objdetection, GTK_TYPE_BOX)

enum
{
    PROP_0,
    PROP_ELEMENT,
};

static void update_labels(SpsPluginBaseGuiObjdetection *gui) {
    // Get labels
    GValue labels = G_VALUE_INIT;
    g_object_get_property(G_OBJECT(gui->element), "labels", &labels);
    guint size = gst_value_array_get_size(&labels);

    // Prepare output string
    GString *availableLabels = g_string_new("");

    for (gint i = 0; i < size; i++)
    {
        const GValue *label = gst_value_array_get_value(&labels, i);
        g_string_append_printf(availableLabels, "- %s%s", g_value_get_string(label), (i < size - 1 ? "\n" : ""));
    }

    // Update label
    gtk_label_set_text(GTK_LABEL(gui->lblAvailableLabels), availableLabels->str);

    // Free memory
    g_value_unset(&labels);
    g_string_free(availableLabels, TRUE);
}

static void fc_model_changed(GtkFileChooserButton *fc, SpsPluginBaseGuiObjdetection *gui)
{
    const char *modelPath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));

    g_object_set(G_OBJECT(gui->element), "model-path", modelPath, NULL);

    // Get updated labels
    update_labels(gui);

    g_free((void *)modelPath);
}

static void cb_active_toggled(GtkCheckButton *cb, SpsPluginBaseGuiObjdetection *gui)
{
    // Get state
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb));

    // Update element
    g_object_set(G_OBJECT(gui->element), "active", active, NULL);
}

static void txt_prefix_changed(GtkEntry *txt, SpsPluginBaseGuiObjdetection *gui)
{
    // Get prefix
    const char *prefix = gtk_entry_get_text(txt);

    // Update element
    g_object_set(G_OBJECT(gui->element), "prefix", prefix, NULL);
}

static void sps_plugin_base_gui_objdetection_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseGuiObjdetection *gui = SPS_PLUGIN_BASE_GUI_OBJDETECTION(object);

    switch (prop_id)
    {
    case PROP_ELEMENT:
    {
        GstElement *element = gst_bin_get_by_name(GST_BIN(g_value_get_pointer(value)), "objdetection");
        gui->element = gst_object_ref(element);
    }
    break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void sps_plugin_base_gui_objdetection_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseGuiObjdetection *gui = SPS_PLUGIN_BASE_GUI_OBJDETECTION(object);

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

static void btn_store_defaults_handler(GtkButton *btn, SpsPluginBaseGuiObjdetection *gui)
{
    GSettings *settings = sps_plugin_get_settings(self);

    GVariant *value = create_objdetection_settings(gui->element);
    g_settings_set_value(settings, "objdetection", value); // Consumes the reference
}

static void sps_plugin_base_gui_objdetection_constructed(GObject *object)
{
    SpsPluginBaseGuiObjdetection *gui = SPS_PLUGIN_BASE_GUI_OBJDETECTION(object);

    // Get values
    const char *prefix, *modelPath;
    gboolean active;
    g_object_get(G_OBJECT(gui->element), "model-path", &modelPath, "prefix", &prefix, "active", &active, NULL);

    // Set file
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gui->fcModel), modelPath);

    // Set prefix
    gtk_entry_set_text(GTK_ENTRY(gui->txtPrefix), prefix);

    // Set active
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->cbActive), active);

    // Set labels
    update_labels(gui);

    // Free memory
    g_free((void *)prefix);
    g_free((void *)modelPath);

    G_OBJECT_CLASS(sps_plugin_base_gui_objdetection_parent_class)->constructed(object);
}

static void sps_plugin_base_gui_objdetection_init(SpsPluginBaseGuiObjdetection *gui)
{
    gtk_widget_init_template(GTK_WIDGET(gui));
}

static void sps_plugin_base_gui_objdetection_finalize(GObject *object)
{
    SpsPluginBaseGuiObjdetection *gui = SPS_PLUGIN_BASE_GUI_OBJDETECTION(object);

    gst_object_unref(gui->element);

    G_OBJECT_CLASS(sps_plugin_base_gui_objdetection_parent_class)->finalize(object);
}

static void sps_plugin_base_gui_objdetection_class_init(SpsPluginBaseGuiObjdetectionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->set_property = sps_plugin_base_gui_objdetection_set_property;
    object_class->get_property = sps_plugin_base_gui_objdetection_get_property;
    object_class->constructed = sps_plugin_base_gui_objdetection_constructed;
    object_class->finalize = sps_plugin_base_gui_objdetection_finalize;

    // Register properties
    g_object_class_install_property(object_class, PROP_ELEMENT,
                                    g_param_spec_pointer("element", "Element",
                                                         "The element this gui refers to",
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    // Load template from resources
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/plugins/base/gui/objdetection.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObjdetection, btnStoreDefaults);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObjdetection, lblHeading);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObjdetection, lblDescription);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObjdetection, lblAvailableLabels);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObjdetection, fcModel);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObjdetection, cbActive);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObjdetection, txtPrefix);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, fc_model_changed);
    gtk_widget_class_bind_template_callback(widget_class, cb_active_toggled);
    gtk_widget_class_bind_template_callback(widget_class, txt_prefix_changed);
    gtk_widget_class_bind_template_callback(widget_class, btn_store_defaults_handler);
}

SpsPluginBaseGuiObjdetection *sps_plugin_base_gui_objdetection_new(GstElement *element)
{
    return g_object_new(SPS_PLUGIN_BASE_TYPE_GUI_OBJDETECTION, "element", element, NULL);
}
