#include <gtk/gtk.h>
#include <sps/sps.h>
#include "dialog-preferences.h"
#include "../sourcedata.h"
#include "../pipeline.h"

G_DEFINE_TYPE(SpsDialogPreferences, sps_dialog_preferences, GTK_TYPE_DIALOG)

enum
{
    PROP_0,
};

static void add_preprocessor(GtkMenuItem *item, SpsDialogPreferences *dialog);
static void add_detector(GtkMenuItem *item, SpsDialogPreferences *dialog);
static void add_postprocessor(GtkMenuItem *item, SpsDialogPreferences *dialog);

// Updates the treeviews and menus according to the currently active plugins
static void sps_dialog_preferences_update_views(SpsDialogPreferences *dialog, GHashTable *active, GHashTable *available, GtkListStore *store, GtkMenu *menu, GCallback callback)
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

// Add a new preprocessor
static void add_preprocessor(GtkMenuItem *item, SpsDialogPreferences *dialog)
{
    const char *name = gtk_menu_item_get_label(item); // Owned by menu item

    // Check if factory is already active
    if (g_hash_table_contains(dialog->registry->defaultPreprocessors, name))
    {
        g_warning("Preprocessor already active");
        return;
    }

    // Add to list
    g_hash_table_add(dialog->registry->defaultPreprocessors, g_strdup(name));

    // Update view
    sps_dialog_preferences_update_views(dialog,
                                        dialog->registry->defaultPreprocessors,
                                        sps_registry_get()->preprocessors,
                                        dialog->lstActivePreprocessors,
                                        dialog->menuAvailablePreprocessors,
                                        (GCallback)add_preprocessor);
}

// Add a new detector
static void add_detector(GtkMenuItem *item, SpsDialogPreferences *dialog)
{
    const char *name = gtk_menu_item_get_label(item); // Owned by menu item

    // Check if factory is already active
    if (g_hash_table_contains(dialog->registry->defaultDetectors, name))
    {
        g_warning("Detector already active");
        return;
    }

    // Add to list
    g_hash_table_add(dialog->registry->defaultDetectors, g_strdup(name));

    // Update tree view model
    sps_dialog_preferences_update_views(dialog,
                                        dialog->registry->defaultDetectors,
                                        sps_registry_get()->detectors,
                                        dialog->lstActiveDetectors,
                                        dialog->menuAvailableDetectors,
                                        (GCallback)add_detector);
}

// Add a new postprocessor
static void add_postprocessor(GtkMenuItem *item, SpsDialogPreferences *dialog)
{
    const char *name = gtk_menu_item_get_label(item); // Owned by menu item

    // Check if factory is already active
    if (g_hash_table_contains(dialog->registry->defaultPostprocessors, name))
    {
        g_warning("Postprocessor already active");
        return;
    }

    // Add to list
    g_hash_table_add(dialog->registry->defaultPostprocessors, g_strdup(name));

    // Update view
    sps_dialog_preferences_update_views(dialog,
                                        dialog->registry->defaultPostprocessors,
                                        sps_registry_get()->postprocessors,
                                        dialog->lstActivePostprocessors,
                                        dialog->menuAvailablePostprocessors,
                                        (GCallback)add_postprocessor);
}

// Remove the selected preprocessor
static void btn_remove_preprocessor_handler(GtkButton *btn, SpsDialogPreferences *dialog)
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

    // Check if selected processor is active
    if (!g_hash_table_contains(dialog->registry->defaultPreprocessors, name))
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
    g_hash_table_remove(dialog->registry->defaultPreprocessors, name);

    // Update tree view model
    sps_dialog_preferences_update_views(dialog,
                                        dialog->registry->defaultPreprocessors,
                                        sps_registry_get()->preprocessors,
                                        dialog->lstActivePreprocessors,
                                        dialog->menuAvailablePreprocessors,
                                        (GCallback)add_preprocessor);

out:
    g_value_unset(&selectedName);
}

// Remove the selected detector
static void btn_remove_detector_handler(GtkButton *btn, SpsDialogPreferences *dialog)
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

    // Check if selected detector is active
    if (!g_hash_table_contains(dialog->registry->defaultDetectors, name))
        goto out;

    // Retrieve factory
    SpsPluginElementFactory *factory = g_hash_table_lookup(sps_registry_get()->detectors, name);
    if (!factory)
    {
        g_warning("No factory found");
        return;
    }

    // Remove factory
    g_hash_table_remove(dialog->registry->defaultDetectors, name);

    // Update tree view model
    sps_dialog_preferences_update_views(dialog,
                                        dialog->registry->defaultDetectors,
                                        sps_registry_get()->detectors,
                                        dialog->lstActiveDetectors,
                                        dialog->menuAvailableDetectors,
                                        (GCallback)add_detector);

out:
    g_value_unset(&selectedName);
}

// Remove the selected postprocessor
static void btn_remove_postprocessor_handler(GtkButton *btn, SpsDialogPreferences *dialog)
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

    // Check if selected processor is active
    if (!g_hash_table_contains(dialog->registry->defaultPostprocessors, name))
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
    g_hash_table_remove(dialog->registry->defaultPostprocessors, name);

    // Update tree view model
    sps_dialog_preferences_update_views(dialog,
                                        dialog->registry->defaultPostprocessors,
                                        sps_registry_get()->postprocessors,
                                        dialog->lstActivePostprocessors,
                                        dialog->menuAvailablePostprocessors,
                                        (GCallback)add_postprocessor);

out:
    g_value_unset(&selectedName);
}

static void sps_dialog_preferences_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    SpsDialogPreferences *dialog = SPS_DIALOG_PREFERENCES(object);

    switch (prop_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void sps_dialog_preferences_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    SpsDialogPreferences *dialog = SPS_DIALOG_PREFERENCES(object);

    switch (prop_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void sps_dialog_preferences_init(SpsDialogPreferences *dialog)
{
    gtk_widget_init_template(GTK_WIDGET(dialog));

    dialog->registry = sps_registry_get();

    // Init the listStores with the active detectors/processors
    sps_dialog_preferences_update_views(dialog,
                                        dialog->registry->defaultPreprocessors,
                                        sps_registry_get()->preprocessors,
                                        dialog->lstActivePreprocessors,
                                        dialog->menuAvailablePreprocessors,
                                        (GCallback)add_preprocessor);
    sps_dialog_preferences_update_views(dialog,
                                        dialog->registry->defaultDetectors,
                                        sps_registry_get()->detectors,
                                        dialog->lstActiveDetectors,
                                        dialog->menuAvailableDetectors,
                                        (GCallback)add_detector);
    sps_dialog_preferences_update_views(dialog,
                                        dialog->registry->defaultPostprocessors,
                                        sps_registry_get()->postprocessors,
                                        dialog->lstActivePostprocessors,
                                        dialog->menuAvailablePostprocessors,
                                        (GCallback)add_postprocessor);
}

static void sps_dialog_preferences_finalize(GObject *object)
{
    SpsDialogPreferences *dialog = SPS_DIALOG_PREFERENCES(object);

    G_OBJECT_CLASS(sps_dialog_preferences_parent_class)->finalize(object);
}

static void sps_dialog_preferences_class_init(SpsDialogPreferencesClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->set_property = sps_dialog_preferences_set_property;
    object_class->get_property = sps_dialog_preferences_get_property;
    object_class->finalize = sps_dialog_preferences_finalize;

    // Register properties

    // Load template from resources
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/gui/dialog-preferences.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, btnAddPreprocessor);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, btnRemovePreprocessor);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, btnAddDetector);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, btnRemoveDetector);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, btnAddPostprocessor);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, btnRemovePostprocessor);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, tvActivePreprocessors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, tvActiveDetectors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, tvActivePostprocessors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, lstActivePreprocessors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, lstActiveDetectors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, lstActivePostprocessors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, menuAvailablePreprocessors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, menuAvailableDetectors);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogPreferences, menuAvailablePostprocessors);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, btn_remove_preprocessor_handler);
    gtk_widget_class_bind_template_callback(widget_class, btn_remove_detector_handler);
    gtk_widget_class_bind_template_callback(widget_class, btn_remove_postprocessor_handler);
}

SpsDialogPreferences *sps_dialog_preferences_new(GtkWindow *parent)
{
    return g_object_new(SPS_TYPE_DIALOG_PREFERENCES, "transient-for", parent, NULL);
}

void sps_dialog_preferences_run(SpsDialogPreferences *dialog)
{
    gtk_dialog_run(GTK_DIALOG(dialog));

    // Store
    GSettings *settings = g_settings_new(APPLICATION_ID);
    GVariantBuilder *builder;
    GVariant *value;
    GList *elements;

    // Store default elements
    builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
    GList *defaultPreprocessor = g_hash_table_get_values(dialog->registry->defaultPreprocessors);
    while (defaultPreprocessor != NULL)
    {
        g_variant_builder_add(builder, "s", defaultPreprocessor->data);
        defaultPreprocessor = defaultPreprocessor->next;
    }
    value = g_variant_new("as", builder);
    g_variant_builder_unref(builder);
    g_settings_set_value(settings, "default-preprocessors", value);

    builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
    GList *defaultDetector = g_hash_table_get_values(dialog->registry->defaultDetectors);
    while (defaultDetector != NULL)
    {
        g_variant_builder_add(builder, "s", defaultDetector->data);
        defaultDetector = defaultDetector->next;
    }
    value = g_variant_new("as", builder);
    g_variant_builder_unref(builder);
    g_settings_set_value(settings, "default-detectors", value);

    builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
    GList *defaultPostprocessor = g_hash_table_get_values(dialog->registry->defaultPostprocessors);
    while (defaultPostprocessor != NULL)
    {
        g_variant_builder_add(builder, "s", defaultPostprocessor->data);
        defaultPostprocessor = defaultPostprocessor->next;
    }
    value = g_variant_new("as", builder);
    g_variant_builder_unref(builder);
    g_settings_set_value(settings, "default-postprocessors", value);

    gtk_widget_hide(GTK_WIDGET(dialog));
}
