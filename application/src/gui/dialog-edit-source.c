#include <gtk/gtk.h>
#include <sps/sps.h>
#include "dialog-edit-source.h"
#include "../sourcedata.h"
#include "../pipeline.h"

G_DEFINE_TYPE(SpsDialogEditSource, sps_dialog_edit_source, GTK_TYPE_DIALOG)

enum
{
    PROP_0,
    PROP_SOURCE_DATA,
};

static void add_preprocessor(GtkMenuItem *item, SpsDialogEditSource *dialog);
static void add_detector(GtkMenuItem *item, SpsDialogEditSource *dialog);
static void add_postprocessor(GtkMenuItem *item, SpsDialogEditSource *dialog);

// Updates the treeviews and menus according to the currently active plugins
static void sps_dialog_edit_source_update_views(SpsDialogEditSource *dialog, GHashTable *active, GHashTable *available, GtkListStore *store, GtkMenu *menu, GCallback callback)
{
    GtkTreeIter iter;
    guint activeCount, availableCount;

    // Clear list store
    gtk_list_store_clear(store);

    // Add all active elements to store
    const char **activeKeys = (const char **)g_hash_table_get_keys_as_array(active, &activeCount);
    for (guint i = 0; i < activeCount; i++)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, activeKeys[i], -1);
    }

    // Clear menu
    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));
    while (children)
    {
        gtk_widget_destroy(children->data);
        children = children->next;
    }
    g_list_free(children);

    // Add available elements to menu
    const char **availableKeys = (const char **)g_hash_table_get_keys_as_array(available, &availableCount);
    for (guint i = 0; i < availableCount; i++)
    {
        // Filter out active
        for (guint j = 0; j < activeCount; j++)
        {
            if (g_str_equal(availableKeys[i], activeKeys[j]))
            {
                goto cnt; // Continue outer loop
            }
        }

        // Append available
        GtkWidget *menuItem = gtk_menu_item_new_with_label(availableKeys[i]);
        g_signal_connect(G_OBJECT(menuItem), "activate", (GCallback)callback, dialog);
        gtk_widget_show(menuItem);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);

    cnt:;
    }

    g_free(activeKeys);
    g_free(availableKeys);
}

// Clear the contents of the details view (i.e. destroy them and set new one)
static void sps_dialog_edit_source_set_details(SpsDialogEditSource *dialog, GtkWidget *details)
{
    // Remove old settings widget
    GList *children = gtk_container_get_children(GTK_CONTAINER(dialog->vpDetails));
    while (children)
    {
        gtk_widget_destroy(children->data);
        children = children->next;
    }
    g_list_free(children);

    if (!details)
        return;

    // Add new settings widget
    gtk_container_add(GTK_CONTAINER(dialog->vpDetails), details);
}

// Add a new preprocessor
static void add_preprocessor(GtkMenuItem *item, SpsDialogEditSource *dialog)
{
    const char *name = gtk_menu_item_get_label(item); // Owned by menu item

    // Check if factory is already active
    if (g_hash_table_contains(dialog->sourceData->activePreprocessors, name))
    {
        g_warning("Preprocessor already active");
        return;
    }

    // Create element and add to pipeline
    pipeline_add_preprocessor(name, dialog->sourceData);

    // Update view
    sps_dialog_edit_source_update_views(dialog,
                                        dialog->sourceData->activePreprocessors,
                                        sps_registry_get()->preprocessors,
                                        dialog->lstActivePreprocessors,
                                        dialog->menuAvailablePreprocessors,
                                        (GCallback)add_preprocessor);
}

// Add a new detector
static void add_detector(GtkMenuItem *item, SpsDialogEditSource *dialog)
{
    const char *name = gtk_menu_item_get_label(item); // Owned by menu item

    // Check if factory is already active
    if (g_hash_table_contains(dialog->sourceData->activeDetectors, name))
    {
        g_warning("Detector already active");
        return;
    }

    // Create element and add to pipeline
    pipeline_add_detector(name, dialog->sourceData);

    // Update tree view model
    sps_dialog_edit_source_update_views(dialog,
                                        dialog->sourceData->activeDetectors,
                                        sps_registry_get()->detectors,
                                        dialog->lstActiveDetectors,
                                        dialog->menuAvailableDetectors,
                                        (GCallback)add_detector);
}

// Add a new postprocessor
static void add_postprocessor(GtkMenuItem *item, SpsDialogEditSource *dialog)
{
    const char *name = gtk_menu_item_get_label(item); // Owned by menu item

    // Check if factory is already active
    if (g_hash_table_contains(dialog->sourceData->activePostprocessors, name))
    {
        g_warning("Postprocessor already active");
        return;
    }

    // Create element and add to pipeline
    pipeline_add_postprocessor(name, dialog->sourceData);

    // Update view
    sps_dialog_edit_source_update_views(dialog,
                                        dialog->sourceData->activePostprocessors,
                                        sps_registry_get()->postprocessors,
                                        dialog->lstActivePostprocessors,
                                        dialog->menuAvailablePostprocessors,
                                        (GCallback)add_postprocessor);
}

// Remove the selected preprocessor
static void btn_remove_preprocessor_handler(GtkButton *btn, SpsDialogEditSource *dialog)
{
    // Get selected processor
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActivePreprocessors));

    if (gtk_tree_selection_count_selected_rows(selection) == 0)
        return;

    GtkTreeModel *model;
    GtkTreeIter iter;
    GValue selectedName = G_VALUE_INIT;
    gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_tree_model_get_value(model, &iter, 0, &selectedName);
    const char *name = g_value_get_string(&selectedName); // Owned by GValue

    // Clear selection
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActivePreprocessors)));

    // Check if selected processor is active
    if (!g_hash_table_contains(dialog->sourceData->activePreprocessors, name))
    {
        g_warning("Processor is not active");
        goto out;
    }

    // Retrieve factory
    SpsPluginElementFactory *factory = g_hash_table_lookup(sps_registry_get()->preprocessors, name);
    if (!factory)
    {
        g_warning("No factory found");
        return;
    }

    // Remove factory
    g_hash_table_remove(dialog->sourceData->activePreprocessors, name);

    // Update tree view model
    sps_dialog_edit_source_update_views(dialog,
                                        dialog->sourceData->activePreprocessors,
                                        sps_registry_get()->preprocessors,
                                        dialog->lstActivePreprocessors,
                                        dialog->menuAvailablePreprocessors,
                                        (GCallback)add_preprocessor);

    // Remove the details page
    sps_dialog_edit_source_set_details(dialog, NULL);

    // Remove element from pipeline and destroy
    GstElement *element = g_hash_table_lookup(dialog->sourceData->activePreprocessorElements, name);
    if (!element)
    {
        GST_ERROR("No element found");
        goto out;
    }
    g_hash_table_remove(dialog->sourceData->activePreprocessorElements, name);

    pipeline_remove_element(element, dialog->sourceData);

out:
    g_value_unset(&selectedName);
}

// Remove the selected detector
static void btn_remove_detector_handler(GtkButton *btn, SpsDialogEditSource *dialog)
{
    // Get selected detector
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActiveDetectors));

    if (gtk_tree_selection_count_selected_rows(selection) == 0)
        return;

    GtkTreeModel *model;
    GtkTreeIter iter;
    GValue selectedName = G_VALUE_INIT;
    gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_tree_model_get_value(model, &iter, 0, &selectedName);
    const char *name = g_value_get_string(&selectedName); // Owned by GValue

    // Clear selection
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActiveDetectors)));

    // Check if selected detector is active
    if (!g_hash_table_contains(dialog->sourceData->activeDetectors, name))
        goto out;

    // Retrieve factory
    SpsPluginElementFactory *factory = g_hash_table_lookup(sps_registry_get()->detectors, name);
    if (!factory)
    {
        g_warning("No factory found");
        return;
    }

    // Remove factory
    g_hash_table_remove(dialog->sourceData->activeDetectors, name);

    // Update tree view model
    sps_dialog_edit_source_update_views(dialog,
                                        dialog->sourceData->activeDetectors,
                                        sps_registry_get()->detectors,
                                        dialog->lstActiveDetectors,
                                        dialog->menuAvailableDetectors,
                                        (GCallback)add_detector);

    // Remove the details page
    sps_dialog_edit_source_set_details(dialog, NULL);

    // Remove element from pipeline and destroy
    GstElement *element = g_hash_table_lookup(dialog->sourceData->activeDetectorElements, name);
    if (!element)
    {
        GST_ERROR("No element found");
        goto out;
    }
    g_hash_table_remove(dialog->sourceData->activeDetectorElements, name);

    pipeline_remove_element(element, dialog->sourceData);

out:
    g_value_unset(&selectedName);
}

// Remove the selected postprocessor
static void btn_remove_postprocessor_handler(GtkButton *btn, SpsDialogEditSource *dialog)
{
    // Get selected processor
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActivePostprocessors));

    if (gtk_tree_selection_count_selected_rows(selection) == 0)
        return;

    GtkTreeModel *model;
    GtkTreeIter iter;
    GValue selectedName = G_VALUE_INIT;
    gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_tree_model_get_value(model, &iter, 0, &selectedName);
    const char *name = g_value_get_string(&selectedName); // Owned by GValue

    // Clear selection
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActivePostprocessors)));

    // Check if selected processor is active
    if (!g_hash_table_contains(dialog->sourceData->activePostprocessors, name))
    {
        g_warning("Processor is not active");
        goto out;
    }

    // Retrieve factory
    SpsPluginElementFactory *factory = g_hash_table_lookup(sps_registry_get()->postprocessors, name);
    if (!factory)
    {
        g_warning("No factory found");
        return;
    }

    // Remove factory
    g_hash_table_remove(dialog->sourceData->activePostprocessors, name);

    // Update tree view model
    sps_dialog_edit_source_update_views(dialog,
                                        dialog->sourceData->activePostprocessors,
                                        sps_registry_get()->postprocessors,
                                        dialog->lstActivePostprocessors,
                                        dialog->menuAvailablePostprocessors,
                                        (GCallback)add_postprocessor);

    // Remove the details page
    sps_dialog_edit_source_set_details(dialog, NULL);

    // Remove elment from pipeline and destroy
    GstElement *element = g_hash_table_lookup(dialog->sourceData->activePostprocessorElements, name);
    if (!element)
    {
        GST_ERROR("No element found");
        goto out;
    }
    g_hash_table_remove(dialog->sourceData->activePostprocessorElements, name);

    pipeline_remove_element(element, dialog->sourceData);

out:
    g_value_unset(&selectedName);
}

// Handle change in selected detector (i.e. display settings page)
static void tv_active_detectors_selection_changed(GtkTreeSelection *selection, SpsDialogEditSource *dialog)
{
    if (gtk_tree_selection_count_selected_rows(selection) == 0)
        return;

    // If there was a selection made here, unselect all from peer
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActivePreprocessors)));
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActivePostprocessors)));

    // Get selected plugin
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    GValue selectedName = G_VALUE_INIT;
    gtk_tree_model_get_value(model, &iter, 0, &selectedName);
    const char *name = g_value_get_string(&selectedName); // Owned by GValue

    // Get factory of selected plugin
    SpsPluginElementFactory *factory = g_hash_table_lookup(dialog->sourceData->activeDetectors, name);
    if (!factory)
    {
        g_warning("Unable to get detector factory");
        goto out;
    }

    // Generate new settings widget
    GstElement *element = g_hash_table_lookup(dialog->sourceData->activeDetectorElements, name);
    if (!element)
    {
        g_warning("Unable to get element for detector");
        goto out;
    }
    GtkWidget *details = factory->create_settings_gui(element);
    sps_dialog_edit_source_set_details(dialog, details);

out:
    g_value_unset(&selectedName);
}

// Handle change in selected preprocessor (i.e. display settings page)
static void tv_active_preprocessors_selection_changed(GtkTreeSelection *selection, SpsDialogEditSource *dialog)
{
    if (gtk_tree_selection_count_selected_rows(selection) == 0)
        return;

    // If there was a selection made here, unselect all from peer
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActiveDetectors)));
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActivePostprocessors)));

    // Get selected plugin
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    GValue selectedName = G_VALUE_INIT;
    gtk_tree_model_get_value(model, &iter, 0, &selectedName);
    const char *name = g_value_get_string(&selectedName); // Owned by GValue

    // Get factory of selected plugin
    SpsPluginElementFactory *factory = g_hash_table_lookup(dialog->sourceData->activePreprocessors, name);
    if (!factory)
    {
        g_warning("Unable to get processor factory");
        goto out;
    }

    // Generate new settings widget
    GstElement *element = g_hash_table_lookup(dialog->sourceData->activePreprocessorElements, name);
    if (!element)
    {
        g_warning("Unable to get element for preprocessor");
        goto out;
    }
    GtkWidget *details = factory->create_settings_gui(element);
    sps_dialog_edit_source_set_details(dialog, details);

out:
    g_value_unset(&selectedName);
}

// Handle change in selected postprocessor (i.e. display settings page)
static void tv_active_postprocessors_selection_changed(GtkTreeSelection *selection, SpsDialogEditSource *dialog)
{
    if (gtk_tree_selection_count_selected_rows(selection) == 0)
        return;

    // If there was a selection made here, unselect all from peer
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActiveDetectors)));
    gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tvActivePreprocessors)));

    // Get selected plugin
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    GValue selectedName = G_VALUE_INIT;
    gtk_tree_model_get_value(model, &iter, 0, &selectedName);
    const char *name = g_value_get_string(&selectedName); // Owned by GValue

    // Get factory of selected plugin
    SpsPluginElementFactory *factory = g_hash_table_lookup(dialog->sourceData->activePostprocessors, name);
    if (!factory)
    {
        g_warning("Unable to get processor factory");
        goto out;
    }

    // Generate new settings widget
    GstElement *element = g_hash_table_lookup(dialog->sourceData->activePostprocessorElements, name);
    if (!element)
    {
        g_warning("Unable to get element for postprocessor");
        goto out;
    }
    GtkWidget *details = factory->create_settings_gui(element);
    sps_dialog_edit_source_set_details(dialog, details);

out:
    g_value_unset(&selectedName);
}

static void sps_dialog_edit_source_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    SpsDialogEditSource *dialog = SPS_DIALOG_EDIT_SOURCE(object);

    switch (prop_id)
    {
    case PROP_SOURCE_DATA:
        dialog->sourceData = gst_object_ref(g_value_get_pointer(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void sps_dialog_edit_source_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    SpsDialogEditSource *dialog = SPS_DIALOG_EDIT_SOURCE(object);

    switch (prop_id)
    {
    case PROP_SOURCE_DATA:
        g_value_set_pointer(value, dialog->sourceData);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void sps_dialog_edit_source_constructed(GObject *object)
{
    SpsDialogEditSource *dialog = SPS_DIALOG_EDIT_SOURCE(object);

    // Init the listStores with the active detectors/processors
    sps_dialog_edit_source_update_views(dialog,
                                        dialog->sourceData->activePreprocessors,
                                        sps_registry_get()->preprocessors,
                                        dialog->lstActivePreprocessors,
                                        dialog->menuAvailablePreprocessors,
                                        (GCallback)add_preprocessor);
    sps_dialog_edit_source_update_views(dialog,
                                        dialog->sourceData->activeDetectors,
                                        sps_registry_get()->detectors,
                                        dialog->lstActiveDetectors,
                                        dialog->menuAvailableDetectors,
                                        (GCallback)add_detector);
    sps_dialog_edit_source_update_views(dialog,
                                        dialog->sourceData->activePostprocessors,
                                        sps_registry_get()->postprocessors,
                                        dialog->lstActivePostprocessors,
                                        dialog->menuAvailablePostprocessors,
                                        (GCallback)add_postprocessor);

    G_OBJECT_CLASS(sps_dialog_edit_source_parent_class)->constructed(object);
}

static void sps_dialog_edit_source_init(SpsDialogEditSource *dialog)
{
    gtk_widget_init_template(GTK_WIDGET(dialog));
}

static void sps_dialog_edit_source_finalize(GObject *object)
{
    SpsDialogEditSource *dialog = SPS_DIALOG_EDIT_SOURCE(object);

    g_object_unref(dialog->sourceData);

    G_OBJECT_CLASS(sps_dialog_edit_source_parent_class)->finalize(object);
}

static void sps_dialog_edit_source_class_init(SpsDialogEditSourceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->set_property = sps_dialog_edit_source_set_property;
    object_class->get_property = sps_dialog_edit_source_get_property;
    object_class->constructed = sps_dialog_edit_source_constructed;
    object_class->finalize = sps_dialog_edit_source_finalize;

    // Register properties
    g_object_class_install_property(object_class, PROP_SOURCE_DATA,
                                    g_param_spec_pointer("source-data", "Source Data",
                                                         "The source data this dialog works on",
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    // Load template from resources
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/gui/dialog-edit-source.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, btnAddPreprocessor);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, btnRemovePreprocessor);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, btnAddDetector);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, btnRemoveDetector);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, btnAddPostprocessor);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, btnRemovePostprocessor);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, tvActivePreprocessors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, tvActiveDetectors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, tvActivePostprocessors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, lstActivePreprocessors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, lstActiveDetectors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, lstActivePostprocessors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, menuAvailablePreprocessors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, menuAvailableDetectors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, menuAvailablePostprocessors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogEditSource, vpDetails);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, btn_remove_preprocessor_handler);
    gtk_widget_class_bind_template_callback(widget_class, btn_remove_detector_handler);
    gtk_widget_class_bind_template_callback(widget_class, btn_remove_postprocessor_handler);
    gtk_widget_class_bind_template_callback(widget_class, tv_active_preprocessors_selection_changed);
    gtk_widget_class_bind_template_callback(widget_class, tv_active_detectors_selection_changed);
    gtk_widget_class_bind_template_callback(widget_class, tv_active_postprocessors_selection_changed);
}

SpsDialogEditSource *sps_dialog_edit_source_new(GtkWindow *parent, SpsSourceData *sourceData)
{
    return g_object_new(SPS_TYPE_DIALOG_EDIT_SOURCE, "transient-for", parent, "source-data", sourceData, NULL);
}

void sps_dialog_edit_source_run(SpsDialogEditSource *dialog)
{
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(GTK_WIDGET(dialog));
}
