#include <gtk/gtk.h>
#include <gst/gst.h>
#include "gui-obstruct.h"
#include "obstruct.h"
#include "../spspluginbase.h"

G_DEFINE_TYPE(SpsPluginBaseGuiObstruct, sps_plugin_base_gui_obstruct, GTK_TYPE_BOX)

enum
{
    PROP_0,
    PROP_ELEMENT,
};

static void get_labels(GtkListStore *store, GstElement *element)
{
    GValue labels = G_VALUE_INIT;
    g_object_get_property(G_OBJECT(element), "labels", &labels);
    guint size = gst_value_array_get_size(&labels);

    GtkTreeIter iter;
    gtk_list_store_clear(store);

    for (gint i = 0; i < size; i++)
    {
        const GValue *label = gst_value_array_get_value(&labels, i);

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, g_value_get_string(label), -1);
    }

    // Free values
    g_value_unset(&labels);
}

static void set_labels(GtkTreeModel *model, GstElement *element)
{
    GtkTreeIter iter;
    gboolean valid;
    valid = gtk_tree_model_get_iter_first(model, &iter);

    GValue labels = G_VALUE_INIT;
    gst_value_array_init(&labels, 0);

    // Convert tree model to value array
    while (valid)
    {
        GValue label = G_VALUE_INIT;

        gtk_tree_model_get_value(model, &iter, 0, &label);
        gst_value_array_append_value(&labels, &label);

        valid = gtk_tree_model_iter_next(model, &iter);

        g_value_unset(&label);
    };

    // Configure element
    g_object_set_property(G_OBJECT(element), "labels", &labels);

    // Free values
    g_value_unset(&labels);
}

static void cb_active_toggled(GtkCheckButton *cb, SpsPluginBaseGuiObstruct *gui)
{
    // Get state
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb));

    // Update element
    g_object_set(G_OBJECT(gui->element), "active", active, NULL);
}

static void cb_invert_toggled(GtkCheckButton *cb, SpsPluginBaseGuiObstruct *gui)
{
    // Get state
    gboolean invert = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb));

    // Update element
    g_object_set(G_OBJECT(gui->element), "invert", invert, NULL);
}

static void cbx_masking_mode_selection_changed(GtkComboBox *cbx, SpsPluginBaseGuiObstruct *gui)
{
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_combo_box_get_model(cbx);

    if (!gtk_combo_box_get_active_iter(cbx, &iter))
        return;

    // Get selected value
    gint filterType;
    gtk_tree_model_get(model, &iter, 1, &filterType, -1);

    // Update element
    g_object_set(G_OBJECT(gui->element), "filter-type", filterType, NULL);
}

static void btn_add_label_handler(GtkButton *btn, SpsPluginBaseGuiObstruct *gui)
{
    // Get values
    const char *label = gtk_entry_get_text(GTK_ENTRY(gui->txtLabel));

    if (strlen(label) == 0)
        return;

    // Add new region
    GtkTreeIter iter;
    gtk_list_store_append(gui->lstLabels, &iter);
    gtk_list_store_set(gui->lstLabels, &iter, 0, label, -1);

    // Clear textbox
    gtk_entry_set_text(GTK_ENTRY(gui->txtLabel), "");

    // Set values to element
    set_labels(GTK_TREE_MODEL(gui->lstLabels), gui->element);
}

static void btn_remove_label_handler(GtkButton *btn, SpsPluginBaseGuiObstruct *gui)
{
    // Get selected
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gui->tvLabels));
    if (gtk_tree_selection_count_selected_rows(selection) == 0)
        return;

    GtkTreeModel *model;
    GtkTreeIter iter;
    gtk_tree_selection_get_selected(selection, &model, &iter);

    // Remove region
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

    // Update element
    set_labels(model, gui->element);
}

static void txt_label_enter_handler(GtkEntry *txt, SpsPluginBaseGuiObstruct *gui)
{
    btn_add_label_handler(GTK_BUTTON(gui->btnAddLabel), gui);
}

static void adj_margin_changed(GtkAdjustment *adj, SpsPluginBaseGuiObstruct *gui)
{
    g_object_set(gui->element, "margin", gtk_adjustment_get_value(adj), NULL);
}

static void sps_plugin_base_gui_obstruct_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseGuiObstruct *gui = SPS_PLUGIN_BASE_GUI_OBSTRUCT(object);

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

static void sps_plugin_base_gui_obstruct_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseGuiObstruct *gui = SPS_PLUGIN_BASE_GUI_OBSTRUCT(object);

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

static void btn_store_defaults_handler(GtkButton *btn, SpsPluginBaseGuiObstruct *gui)
{
    GSettings *settings = sps_plugin_get_settings(self);

    GVariant *value = create_obstruct_settings(gui->element);
    g_settings_set_value(settings, "obstruct", value); // Consumes the reference
}

static void sps_plugin_base_gui_obstruct_constructed(GObject *object)
{
    SpsPluginBaseGuiObstruct *gui = SPS_PLUGIN_BASE_GUI_OBSTRUCT(object);

    // Get regions from element
    get_labels(gui->lstLabels, gui->element);

    // Get values
    gboolean active, invert;
    guint elementMaskingMode;
    gfloat margin;
    g_object_get(gui->element, "active", &active, "invert", &invert, "filter-type", &elementMaskingMode, "margin", &margin, NULL);

    // Set active
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->cbActive), active);

    // Set invert
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->cbInvert), invert);
    
    // Set margin
    gtk_adjustment_set_value(gui->adjMargin, margin);

    // Set masking mode
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(gui->lstMaskingModes), &iter);

    guint modelMaskingMode;
    while (valid)
    {
        gtk_tree_model_get(GTK_TREE_MODEL(gui->lstMaskingModes), &iter, 1, &modelMaskingMode, -1);

        if (elementMaskingMode == modelMaskingMode)
        {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(gui->cbxMaskingMode), &iter);
            break;
        }

        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(gui->lstMaskingModes), &iter);
    }

    

    G_OBJECT_CLASS(sps_plugin_base_gui_obstruct_parent_class)->constructed(object);
}

static void sps_plugin_base_gui_obstruct_init(SpsPluginBaseGuiObstruct *gui)
{
    gtk_widget_init_template(GTK_WIDGET(gui));
}

static void sps_plugin_base_gui_obstruct_finalize(GObject *object)
{
    SpsPluginBaseGuiObstruct *gui = SPS_PLUGIN_BASE_GUI_OBSTRUCT(object);

    gst_object_unref(gui->element);

    G_OBJECT_CLASS(sps_plugin_base_gui_obstruct_parent_class)->finalize(object);
}

static void sps_plugin_base_gui_obstruct_class_init(SpsPluginBaseGuiObstructClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->set_property = sps_plugin_base_gui_obstruct_set_property;
    object_class->get_property = sps_plugin_base_gui_obstruct_get_property;
    object_class->constructed = sps_plugin_base_gui_obstruct_constructed;
    object_class->finalize = sps_plugin_base_gui_obstruct_finalize;

    // Register properties
    g_object_class_install_property(object_class, PROP_ELEMENT,
                                    g_param_spec_pointer("element", "Element",
                                                         "The element this gui refers to",
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    // Load template from resources
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/plugins/base/gui/obstruct.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, btnStoreDefaults);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, lblHeading);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, lblDescription);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, tvLabels);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, lstLabels);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, btnAddLabel);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, btnRemoveLabel);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, txtLabel);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, lstMaskingModes);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, cbxMaskingMode);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, adjMargin);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, cbActive);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseGuiObstruct, cbInvert);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, btn_add_label_handler);
    gtk_widget_class_bind_template_callback(widget_class, btn_remove_label_handler);
    gtk_widget_class_bind_template_callback(widget_class, cbx_masking_mode_selection_changed);
    gtk_widget_class_bind_template_callback(widget_class, adj_margin_changed);
    gtk_widget_class_bind_template_callback(widget_class, btn_store_defaults_handler);
    gtk_widget_class_bind_template_callback(widget_class, cb_active_toggled);
    gtk_widget_class_bind_template_callback(widget_class, cb_invert_toggled);
    gtk_widget_class_bind_template_callback(widget_class, txt_label_enter_handler);
}

SpsPluginBaseGuiObstruct *sps_plugin_base_gui_obstruct_new(GstElement *element)
{
    return g_object_new(SPS_PLUGIN_BASE_TYPE_GUI_OBSTRUCT, "element", element, NULL);
}
