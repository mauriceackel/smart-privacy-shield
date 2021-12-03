#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkscreen.h>
#include <cairo.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include "gui-regions.h"
#include "../spspluginbase.h"
#include "regions.h"
#include "regiondialog.h"
#include <stdlib.h>

G_DEFINE_TYPE(SpsPluginBaseGuiRegions, sps_plugin_base_gui_regions, GTK_TYPE_BOX)

enum
{
    PROP_0,
    PROP_ELEMENT,
};

static void get_regions(GtkListStore *store, GstElement *element)
{
    GValue regions = G_VALUE_INIT;
    g_object_get_property(G_OBJECT(element), "regions", &regions);
    guint size = gst_value_array_get_size(&regions);

    GtkTreeIter iter;
    gtk_list_store_clear(store);
    char **coordStrings;
    char label[100];
    gint x, y, width, height;

    for (gint i = 0; i < size; i++)
    {
        const GValue *region = gst_value_array_get_value(&regions, i);
        coordStrings = g_strsplit(g_value_get_string(region), ":", 4);

        x = atoi(coordStrings[0]);
        y = atoi(coordStrings[1]);
        width = atoi(coordStrings[2]);
        height = atoi(coordStrings[3]);

        g_snprintf(label, 100, "(%i, %i) -> (%ix%i)", x, y, width, height);

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, label, 1, x, 2, y, 3, width, 4, height, -1);

        g_strfreev(coordStrings);
    }

    // Free values
    g_value_unset(&regions);
}

static void set_regions(GtkTreeModel *model, GstElement *element)
{
    GtkTreeIter iter;
    gboolean valid;
    valid = gtk_tree_model_get_iter_first(model, &iter);

    GValue regions = G_VALUE_INIT;
    gst_value_array_init(&regions, 0);
    GValue region = G_VALUE_INIT;
    g_value_init(&region, G_TYPE_STRING);

    gint x, y, width, height;
    char buffer[100];

    // Convert tree model to value array
    while (valid)
    {
        gtk_tree_model_get(model, &iter, 1, &x, 2, &y, 3, &width, 4, &height, -1);
        g_snprintf(buffer, 100, "%i:%i:%i:%i", x, y, width, height);

        g_value_set_string(&region, buffer);
        gst_value_array_append_value(&regions, &region);

        valid = gtk_tree_model_iter_next(model, &iter);
    };

    // Configure element
    g_object_set_property(G_OBJECT(element), "regions", &regions);

    // Free values
    g_value_unset(&region);
    g_value_unset(&regions);
}

static void update_labels(SpsPluginBaseGuiRegions *gui) {
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

static void cb_active_toggled(GtkCheckButton *cb, SpsPluginBaseGuiRegions *gui)
{
    // Get state
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb));

    // Update element
    g_object_set(G_OBJECT(gui->element), "active", active, NULL);
}

static void txt_prefix_changed(GtkEntry *txt, SpsPluginBaseGuiRegions *gui)
{
    // Get prefix
    const char *prefix = gtk_entry_get_text(txt);

    // Update element
    g_object_set(G_OBJECT(gui->element), "prefix", prefix, NULL);
}

static void btn_add_region_handler(GtkButton *btn, SpsPluginBaseGuiRegions *gui)
{
    // Get values
    gint x, y, width, height;
    x = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gui->txtX));
    y = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gui->txtY));
    width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gui->txtWidth));
    height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gui->txtHeight));

    // Add new region
    GtkTreeIter iter;
    gtk_list_store_append(gui->lstRegions, &iter);
    char label[100];
    g_snprintf(label, 100, "(%i, %i) -> (%ix%i)", x, y, width, height);
    gtk_list_store_set(gui->lstRegions, &iter, 0, label, 1, x, 2, y, 3, width, 4, height, -1);

    // Set values to element
    set_regions(GTK_TREE_MODEL(gui->lstRegions), gui->element);
}

static void btn_remove_region_handler(GtkButton *btn, SpsPluginBaseGuiRegions *gui)
{
    // Get selected
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gui->tvRegions));
    if (gtk_tree_selection_count_selected_rows(selection) == 0)
        return;

    GtkTreeModel *model;
    GtkTreeIter iter;
    gtk_tree_selection_get_selected(selection, &model, &iter);

    // Remove region
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

    // Update element
    set_regions(model, gui->element);
}

static void btn_select_region_handler(GtkButton *btn, SpsPluginBaseGuiRegions *gui)
{
    GtkWindow *parentWindow = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(btn)));
    SpsPluginBaseRegionDialog *selectionDialog = sps_plugin_base_region_dialog_new(parentWindow, gui->appSink);
    BoundingBox bbox;

    if (!sps_plugin_base_region_dialog_run(selectionDialog, &bbox))
        return;

    // Add new region
    GtkTreeIter iter;
    gtk_list_store_append(gui->lstRegions, &iter);
    char label[100];
    g_snprintf(label, 100, "(%i, %i) -> (%ix%i)", bbox.x, bbox.y, bbox.width, bbox.height);
    gtk_list_store_set(gui->lstRegions, &iter, 0, label, 1, bbox.x, 2, bbox.y, 3, bbox.width, 4, bbox.height, -1);

    // Set values to element
    set_regions(GTK_TREE_MODEL(gui->lstRegions), gui->element);
}

static void sps_plugin_base_gui_regions_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseGuiRegions *gui = SPS_PLUGIN_BASE_GUI_REGIONS(object);

    switch (prop_id)
    {
    case PROP_ELEMENT:
    {
        GstElement *element = gst_bin_get_by_name(GST_BIN(g_value_get_pointer(value)), "regions");
        gui->element = gst_object_ref(element);

        GstElement *appSink = gst_bin_get_by_name(GST_BIN(g_value_get_pointer(value)), "appsink");
        gui->appSink = gst_object_ref(appSink);
    }
    break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void sps_plugin_base_gui_regions_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseGuiRegions *gui = SPS_PLUGIN_BASE_GUI_REGIONS(object);

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

static void btn_store_defaults_handler(GtkButton *btn, SpsPluginBaseGuiRegions *gui)
{
    GSettings *settings = sps_plugin_get_settings(self);

    GVariant *value = create_regions_settings(gui->element);
    g_settings_set_value(settings, "regions", value); // Consumes the reference
}

static void sps_plugin_base_gui_regions_constructed(GObject *object)
{
    SpsPluginBaseGuiRegions *gui = SPS_PLUGIN_BASE_GUI_REGIONS(object);

    // Get regions from element
    get_regions(gui->lstRegions, gui->element);

    // Get values
    const char *prefix;
    gboolean active;
    g_object_get(G_OBJECT(gui->element), "prefix", &prefix, "active", &active, NULL);

    // Set prefix
    gtk_entry_set_text(GTK_ENTRY(gui->txtPrefix), prefix);

    // Set active
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->cbActive), active);

    // Get labels
    update_labels(gui);

    // Free memory
    g_free((void *)prefix);

    G_OBJECT_CLASS(sps_plugin_base_gui_regions_parent_class)->constructed(object);
}

static void sps_plugin_base_gui_regions_init(SpsPluginBaseGuiRegions *gui)
{
    gtk_widget_init_template(GTK_WIDGET(gui));
}

static void sps_plugin_base_gui_regions_finalize(GObject *object)
{
    SpsPluginBaseGuiRegions *gui = SPS_PLUGIN_BASE_GUI_REGIONS(object);

    gst_object_unref(gui->element);
    gst_object_unref(gui->appSink);

    G_OBJECT_CLASS(sps_plugin_base_gui_regions_parent_class)->finalize(object);
}

static void sps_plugin_base_gui_regions_class_init(SpsPluginBaseGuiRegionsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->set_property = sps_plugin_base_gui_regions_set_property;
    object_class->get_property = sps_plugin_base_gui_regions_get_property;
    object_class->constructed = sps_plugin_base_gui_regions_constructed;
    object_class->finalize = sps_plugin_base_gui_regions_finalize;

    // Register properties
    g_object_class_install_property(object_class, PROP_ELEMENT,
                                    g_param_spec_pointer("element", "Element",
                                                         "The element this gui refers to",
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    // Load template from resources
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/plugins/base/gui/regions.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, btnStoreDefaults);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, lblHeading);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, lblDescription);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, lblAvailableLabels);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, tvRegions);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, lstRegions);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, btnAddRegion);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, btnRemoveRegion);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, btnSelectRegion);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, txtX);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, txtY);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, txtWidth);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, txtHeight);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, adjX);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, adjY);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, adjWidth);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, adjHeight);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, cbActive);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiRegions, txtPrefix);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, btn_add_region_handler);
    gtk_widget_class_bind_template_callback(widget_class, btn_remove_region_handler);
    gtk_widget_class_bind_template_callback(widget_class, btn_select_region_handler);
    gtk_widget_class_bind_template_callback(widget_class, cb_active_toggled);
    gtk_widget_class_bind_template_callback(widget_class, txt_prefix_changed);
    gtk_widget_class_bind_template_callback(widget_class, btn_store_defaults_handler);
}

SpsPluginBaseGuiRegions *sps_plugin_base_gui_regions_new(GstElement *element)
{
    return g_object_new(SPS_PLUGIN_BASE_TYPE_GUI_REGIONS, "element", element, NULL);
}
