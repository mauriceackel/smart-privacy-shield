#include <gtk/gtk.h>
#include <gst/gst.h>
#include "gui-winanalyzer.h"
#include "../spspluginbase.h"
#include "winanalyzer.h"

G_DEFINE_TYPE(SpsPluginBaseGuiWinanalyzer, sps_plugin_base_gui_winanalyzer, GTK_TYPE_BOX)

enum
{
    PROP_0,
    PROP_ELEMENT,
};

int compare_labels(const char **a, const char **b)
{
    return strcmp(*a, *b);
}

static void update_labels(SpsPluginBaseGuiWinanalyzer *gui)
{
    // Get labels
    GValue labels = G_VALUE_INIT;
    g_object_get_property(G_OBJECT(gui->element), "labels", &labels);
    guint size = gst_value_array_get_size(&labels);

    GPtrArray *sortedLabels = g_ptr_array_new_with_free_func(g_free);

    // Fill array
    for (gint i = 0; i < size; i++)
    {
        const GValue *label = gst_value_array_get_value(&labels, i);
        g_ptr_array_add(sortedLabels, g_value_dup_string(label));
    }

    // Sort array
    g_ptr_array_sort(sortedLabels, (GCompareFunc)compare_labels);

    // Prepare output string
    GString *availableLabels = g_string_new("");

    // Convert sorted array to string
    for (gint i = 0; i < sortedLabels->len; i++)
    {
        g_string_append_printf(availableLabels, "- %s%s", g_ptr_array_index(sortedLabels, i), (i < size - 1 ? "\n" : ""));
    }

    // Update label
    gtk_label_set_text(GTK_LABEL(gui->lblAvailableLabels), availableLabels->str);

    // Free memory
    g_ptr_array_free(sortedLabels, TRUE);
    g_value_unset(&labels);
    g_string_free(availableLabels, TRUE);
}

static void cb_active_toggled(GtkCheckButton *cb, SpsPluginBaseGuiWinanalyzer *gui)
{
    // Get state
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb));

    // Update element
    g_object_set(G_OBJECT(gui->element), "active", active, NULL);
}

static void txt_prefix_changed(GtkEntry *txt, SpsPluginBaseGuiWinanalyzer *gui)
{
    // Get prefix
    const char *prefix = gtk_entry_get_text(txt);

    // Update element
    g_object_set(G_OBJECT(gui->element), "prefix", prefix, NULL);
}

static void cbx_display_changed(GtkComboBox *cbx, SpsPluginBaseGuiWinanalyzer *gui)
{
    gint selectedIndex = gtk_combo_box_get_active(cbx);

    if (selectedIndex == -1)
        return;

    SpsSourceInfo *sourceInfo = &g_array_index(gui->sourceInfos, SpsSourceInfo, selectedIndex);

    g_object_set(G_OBJECT(gui->element), "display-id", sourceInfo->id, NULL);

    // Update labels
    update_labels(gui);
}

static void sps_plugin_base_gui_winanalyzer_load_sources(SpsPluginBaseGuiWinanalyzer *gui)
{
    // Get source info
    sps_sources_displays_list(gui->sourceInfos);

    // Fill dropdown with sources
    GtkTreeIter iter;
    for (gint i = 0; i < gui->sourceInfos->len; i++)
    {
        SpsSourceInfo *info = &g_array_index(gui->sourceInfos, SpsSourceInfo, i);
        gtk_list_store_append(gui->lstDisplays, &iter);
        gtk_list_store_set(gui->lstDisplays, &iter, 0, info->name, -1);
    }
}

static void sps_plugin_base_gui_winanalyzer_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseGuiWinanalyzer *gui = SPS_PLUGIN_BASE_GUI_WINANALYZER(object);

    switch (prop_id)
    {
    case PROP_ELEMENT:
    {
        GstElement *element = gst_object_ref(g_value_get_pointer(value));
        gui->element = gst_object_ref(element);
    }
    break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void sps_plugin_base_gui_winanalyzer_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseGuiWinanalyzer *gui = SPS_PLUGIN_BASE_GUI_WINANALYZER(object);

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

static void btn_store_defaults_handler(GtkButton *btn, SpsPluginBaseGuiWinanalyzer *gui)
{
    GSettings *settings = sps_plugin_get_settings(self);

    GVariant *value = create_winanalyzer_settings(gui->element);
    g_settings_set_value(settings, "winanalyzer", value); // Consumes the reference
}

static void sps_plugin_base_gui_winanalyzer_constructed(GObject *object)
{
    SpsPluginBaseGuiWinanalyzer *gui = SPS_PLUGIN_BASE_GUI_WINANALYZER(object);

    // Get values
    const char *prefix;
    guint64 displayId;
    gboolean active;
    g_object_get(G_OBJECT(gui->element), "prefix", &prefix, "active", &active, "display-id", &displayId, NULL);

    // Set prefix
    gtk_entry_set_text(GTK_ENTRY(gui->txtPrefix), prefix);

    // Set active
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->cbActive), active);

    // Set selected display
    for (gint i = 0; i < gui->sourceInfos->len; i++)
    {
        if (g_array_index(gui->sourceInfos, SpsSourceInfo, i).id == displayId)
        {
            gtk_combo_box_set_active(GTK_COMBO_BOX(gui->cbxDisplay), i);
            break;
        }
    }

    // Set labels
    update_labels(gui);

    // Free memory
    g_free((void *)prefix);

    G_OBJECT_CLASS(sps_plugin_base_gui_winanalyzer_parent_class)->constructed(object);
}

static void sps_plugin_base_gui_winanalyzer_init(SpsPluginBaseGuiWinanalyzer *gui)
{
    gtk_widget_init_template(GTK_WIDGET(gui));

    gui->sourceInfos = g_array_new(FALSE, FALSE, sizeof(SpsSourceInfo));
    g_array_set_clear_func(gui->sourceInfos, sps_source_info_clear);

    sps_plugin_base_gui_winanalyzer_load_sources(gui);
}

static void sps_plugin_base_gui_winanalyzer_finalize(GObject *object)
{
    SpsPluginBaseGuiWinanalyzer *gui = SPS_PLUGIN_BASE_GUI_WINANALYZER(object);

    gst_object_unref(gui->element);
    g_array_free(gui->sourceInfos, TRUE);

    G_OBJECT_CLASS(sps_plugin_base_gui_winanalyzer_parent_class)->finalize(object);
}

static void sps_plugin_base_gui_winanalyzer_class_init(SpsPluginBaseGuiWinanalyzerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->set_property = sps_plugin_base_gui_winanalyzer_set_property;
    object_class->get_property = sps_plugin_base_gui_winanalyzer_get_property;
    object_class->constructed = sps_plugin_base_gui_winanalyzer_constructed;
    object_class->finalize = sps_plugin_base_gui_winanalyzer_finalize;

    // Register properties
    g_object_class_install_property(object_class, PROP_ELEMENT,
                                    g_param_spec_pointer("element", "Element",
                                                         "The element this gui refers to",
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    // Load template from resources
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/plugins/base/gui/winanalyzer.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiWinanalyzer, btnStoreDefaults);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiWinanalyzer, lblHeading);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiWinanalyzer, lblDescription);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiWinanalyzer, lblAvailableLabels);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiWinanalyzer, cbActive);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiWinanalyzer, cbxDisplay);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiWinanalyzer, lstDisplays);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiWinanalyzer, txtPrefix);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, cb_active_toggled);
    gtk_widget_class_bind_template_callback(widget_class, txt_prefix_changed);
    gtk_widget_class_bind_template_callback(widget_class, btn_store_defaults_handler);
    gtk_widget_class_bind_template_callback(widget_class, cbx_display_changed);
}

SpsPluginBaseGuiWinanalyzer *sps_plugin_base_gui_winanalyzer_new(GstElement *element)
{
    return g_object_new(SPS_PLUGIN_BASE_TYPE_GUI_WINANALYZER, "element", element, NULL);
}
