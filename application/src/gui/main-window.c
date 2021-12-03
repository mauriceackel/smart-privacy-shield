#include <gtk/gtk.h>
#include <gst/gst.h>
#include "../sourcedata.h"
#include "main-window.h"
#include "source-row.h"
#include "../application.h"
#include "../pipeline.h"
#include "dialog-add-source.h"

#define SOURCES_ELEMENT_FORMAT "((ssxx)a{sa{sv}})"
#define SOURCES_FORMAT "a" SOURCES_ELEMENT_FORMAT

G_DEFINE_TYPE(SpsMainWindow, sps_main_window, GTK_TYPE_APPLICATION_WINDOW)

void sps_main_window_remove_row(SpsMainWindow *window, SpsSourceRow *row)
{
    // Remove from parent (will be destroyed if there is no more reference)
    gtk_container_remove(GTK_CONTAINER(window->boxSources), GTK_WIDGET(row));
}

void sps_main_window_add_row(SpsMainWindow *window, SpsSourceData *sourceData)
{
    g_object_ref(sourceData);

    SpsSourceRow *sourceRow = sps_source_row_new(sourceData);
    sourceData->row = sourceRow;

    gtk_label_set_text(GTK_LABEL(sourceRow->lblName), sourceData->sourceInfo.name);

    gtk_container_add(GTK_CONTAINER(window->boxSources), GTK_WIDGET(sourceRow));
    gtk_box_set_child_packing(GTK_BOX(window->boxSources), GTK_WIDGET(sourceRow), FALSE, TRUE, 0, GTK_PACK_START);

    g_object_unref(sourceData);
}

static void btn_add_source_handler(GtkButton *btn, SpsMainWindow *window)
{
    SpsSourceInfo *result;
    SpsDialogAddSource *dialog = sps_dialog_add_source_new(GTK_WINDOW(window));

    result = sps_dialog_add_source_run(dialog);

    if (result == NULL)
        goto out;

    // Add the source
    sps_application_add_source(SPS_MAIN_WINDOW_APP_GET(window), result, TRUE);

out:
    gtk_widget_destroy(GTK_WIDGET(dialog)); // Will free "result" as well
}

static void btn_start_handler(GtkButton *btn, SpsMainWindow *window)
{
    GstElement *pipeline = SPS_MAIN_WINDOW_APP_GET(window)->gstData.pipeline;
    pipeline_start(pipeline);
}

static void btn_stop_handler(GtkButton *btn, SpsMainWindow *window)
{
    GstElement *pipeline = SPS_MAIN_WINDOW_APP_GET(window)->gstData.pipeline;
    pipeline_stop(pipeline);
}

static gboolean window_close_handler(SpsMainWindow *window)
{
    SpsApplication *app = SPS_MAIN_WINDOW_APP_GET(window);

    // Persist
    GSettings *settings = g_settings_new(APPLICATION_ID);

    // Store position and size
    gint posX, posY, windowWidth, windowHeight;
    gtk_window_get_position(GTK_WINDOW(window), &posX, &posY);
    gtk_window_get_size(GTK_WINDOW(window), &windowWidth, &windowHeight);

    g_settings_set_int(settings, "pos-x", posX);
    g_settings_set_int(settings, "pos-y", posY);
    g_settings_set_int(settings, "window-width", windowWidth);
    g_settings_set_int(settings, "window-height", windowHeight);

    // Store sources
    GVariantBuilder *sourceListBuilder = g_variant_builder_new(G_VARIANT_TYPE(SOURCES_FORMAT));
    for (gint i = 0; i < app->gstData.sources->len; i++)
    {
        SpsSourceData *sourceData = g_ptr_array_index(app->gstData.sources, i);

        // Open a single source entry
        g_variant_builder_open(sourceListBuilder, G_VARIANT_TYPE(SOURCES_ELEMENT_FORMAT));

        // Add SourceInfo
        g_variant_builder_add(sourceListBuilder, "(ssxx)", sourceData->sourceInfo.name, sourceData->sourceInfo.internalName, (gint64)sourceData->sourceInfo.id, (gint64)sourceData->sourceInfo.type);

        GList *factoryNames;
        SpsPluginElementFactory *factory;
        GstElement *element;

        // Enter dict
        g_variant_builder_open(sourceListBuilder, G_VARIANT_TYPE("a{sa{sv}}"));

        // Preprocessors
        g_variant_builder_open(sourceListBuilder, G_VARIANT_TYPE("{sa{sv}}")); // Open new element in dict
        g_variant_builder_add(sourceListBuilder, "s", "preprocessors");        // Set key for the new category

        g_variant_builder_open(sourceListBuilder, G_VARIANT_TYPE("a{sv}")); // Open dictionary that holds all the factories with their properties
        factoryNames = g_hash_table_get_keys(sourceData->activePreprocessors);
        while (factoryNames != NULL)
        {
            factory = g_hash_table_lookup(sourceData->activePreprocessors, factoryNames->data);
            element = g_hash_table_lookup(sourceData->activePreprocessorElements, factoryNames->data);

            if (!factory || !element)
                continue;

            GVariant *elementSettings = factory->create_element_settings(element);

            GVariant *entry = g_variant_new("{sv}", factoryNames->data, elementSettings);
            g_variant_builder_add_value(sourceListBuilder, entry);

            factoryNames = factoryNames->next;
        }
        g_variant_builder_close(sourceListBuilder); // Now inside element of category dict
        g_variant_builder_close(sourceListBuilder); // Now inside category dict

        // Detectors
        g_variant_builder_open(sourceListBuilder, G_VARIANT_TYPE("{sa{sv}}")); // Open new element in dict
        g_variant_builder_add(sourceListBuilder, "s", "detectors");            // Set key for the new category

        g_variant_builder_open(sourceListBuilder, G_VARIANT_TYPE("a{sv}")); // Open dictionary that holds all the factories with their properties
        factoryNames = g_hash_table_get_keys(sourceData->activeDetectors);
        while (factoryNames != NULL)
        {
            factory = g_hash_table_lookup(sourceData->activeDetectors, factoryNames->data);
            element = g_hash_table_lookup(sourceData->activeDetectorElements, factoryNames->data);

            if (!factory || !element)
                continue;

            GVariant *elementSettings = factory->create_element_settings(element);

            GVariant *entry = g_variant_new("{sv}", factoryNames->data, elementSettings);
            g_variant_builder_add_value(sourceListBuilder, entry);

            factoryNames = factoryNames->next;
        }
        g_variant_builder_close(sourceListBuilder); // Now inside element of category dict
        g_variant_builder_close(sourceListBuilder); // Now inside category dict

        // Postprocessors
        g_variant_builder_open(sourceListBuilder, G_VARIANT_TYPE("{sa{sv}}")); // Open new element in dict
        g_variant_builder_add(sourceListBuilder, "s", "postprocessors");       // Set key for the new category

        g_variant_builder_open(sourceListBuilder, G_VARIANT_TYPE("a{sv}")); // Open dictionary that holds all the factories with their properties
        factoryNames = g_hash_table_get_keys(sourceData->activePostprocessors);
        while (factoryNames != NULL)
        {
            factory = g_hash_table_lookup(sourceData->activePostprocessors, factoryNames->data);
            element = g_hash_table_lookup(sourceData->activePostprocessorElements, factoryNames->data);

            if (!factory || !element)
                continue;

            GVariant *elementSettings = factory->create_element_settings(element);

            GVariant *entry = g_variant_new("{sv}", factoryNames->data, elementSettings);
            g_variant_builder_add_value(sourceListBuilder, entry);

            factoryNames = factoryNames->next;
        }
        g_variant_builder_close(sourceListBuilder); // Now inside element of category dict
        g_variant_builder_close(sourceListBuilder); // Now inside category dict

        // Finish source entry
        g_variant_builder_close(sourceListBuilder); // Now inside source element
        g_variant_builder_close(sourceListBuilder); // Now inside source list
    }
    g_settings_set(settings, "sources", SOURCES_FORMAT, sourceListBuilder);
    g_variant_builder_unref(sourceListBuilder);

    // Stop whole app as main window gets closed
    g_application_quit(G_APPLICATION(app));

    return FALSE;
}

static gboolean window_realize_handler(SpsMainWindow *window)
{
    SpsApplication *app = SPS_MAIN_WINDOW_APP_GET(window);

    // Restore position and size
    GSettings *settings = g_settings_new(APPLICATION_ID);
    gint posX = g_settings_get_int(settings, "pos-x");
    gint posY = g_settings_get_int(settings, "pos-y");
    gint windowWidth = g_settings_get_int(settings, "window-width");
    gint windowHeight = g_settings_get_int(settings, "window-height");

    gtk_window_move(GTK_WINDOW(window), posX, posY);
    gtk_window_resize(GTK_WINDOW(window), windowWidth, windowHeight);

    // Restore default elements
    SpsRegistry *registry = sps_registry_get();
    const char *name;

    GVariant *preprocessors = g_settings_get_value(settings, "default-preprocessors");
    for (gint i = 0; i < g_variant_n_children(preprocessors); i++)
    {
        GVariant *preprocessor = g_variant_get_child_value(preprocessors, i);
        name = g_variant_get_string(preprocessor, NULL);

        g_hash_table_add(registry->defaultPreprocessors, g_strdup(name));
    }

    GVariant *detectors = g_settings_get_value(settings, "default-detectors");
    for (gint i = 0; i < g_variant_n_children(detectors); i++)
    {
        GVariant *detector = g_variant_get_child_value(detectors, i);
        name = g_variant_get_string(detector, NULL);

        g_hash_table_add(registry->defaultDetectors, g_strdup(name));
    }

    GVariant *postprocessors = g_settings_get_value(settings, "default-postprocessors");
    for (gint i = 0; i < g_variant_n_children(postprocessors); i++)
    {
        GVariant *postprocessor = g_variant_get_child_value(postprocessors, i);
        name = g_variant_get_string(postprocessor, NULL);

        g_hash_table_add(registry->defaultPostprocessors, g_strdup(name));
    }

    // Restore sources
    GVariant *sources = g_settings_get_value(settings, "sources");
    for (gint i = 0; i < g_variant_n_children(sources); i++)
    {
        GVariant *source = g_variant_get_child_value(sources, i);

        GVariant *sourceInfoVariant, *elementsVariant;
        sourceInfoVariant = g_variant_get_child_value(source, 0);
        elementsVariant = g_variant_get_child_value(source, 1);

        if (!g_variant_check_format_string(sourceInfoVariant, "(ssxx)", FALSE))
        {
            continue;
        }

        // Get source info
        const char *name, *internalName;
        gint64 id, type;
        g_variant_get(sourceInfoVariant, "(ssxx)", &name, &internalName, &id, &type);
        SpsSourceInfo sourceInfo = {
            .name = g_strdup(name),
            .internalName = g_strdup(internalName),
            .id = (guintptr)id,
            .type = (SpsSourceType)type,
        };

        // Recover source info
        if (type == SOURCE_TYPE_WINDOW)
        {
            if (!sps_sources_recover_window(&sourceInfo))
                continue; // No match found, skip
        }
        else if (type == SOURCE_TYPE_DISPLAY)
        {
            if (!sps_sources_recover_display(&sourceInfo))
                continue; // No match found, skip
        }

        // Create new source
        SpsSourceData *sourceData = sps_application_add_source(app, &sourceInfo, FALSE);

        GVariantIter *iter = NULL;
        const char *factoryName;
        GVariant *settingsValue;
        SpsPluginElementFactory *factory;
        GstElement *element;

        // Get preprocessors
        g_variant_lookup(elementsVariant, "preprocessors", "a{sv}", &iter);
        while (g_variant_iter_next(iter, "{sv}", &factoryName, &settingsValue))
        {
            // Try to add the element
            pipeline_add_preprocessor(factoryName, sourceData);

            // Get factory and element in order to apply stored settings
            factory = g_hash_table_lookup(sourceData->activePreprocessors, factoryName);
            element = g_hash_table_lookup(sourceData->activePreprocessorElements, factoryName);

            if (!factory || !element)
                continue;

            factory->apply_element_settings(settingsValue, element);

            g_free((void *)factoryName);
            g_variant_unref(settingsValue);
        }

        // Get detectors
        g_variant_lookup(elementsVariant, "detectors", "a{sv}", &iter);
        while (g_variant_iter_next(iter, "{sv}", &factoryName, &settingsValue))
        {
            // Try to add the element
            pipeline_add_detector(factoryName, sourceData);

            // Get factory and element in order to apply stored settings
            factory = g_hash_table_lookup(sourceData->activeDetectors, factoryName);
            element = g_hash_table_lookup(sourceData->activeDetectorElements, factoryName);

            if (!factory || !element)
                continue;

            factory->apply_element_settings(settingsValue, element);

            g_free((void *)factoryName);
            g_variant_unref(settingsValue);
        }

        // Get postprocessors
        g_variant_lookup(elementsVariant, "postprocessors", "a{sv}", &iter);
        while (g_variant_iter_next(iter, "{sv}", &factoryName, &settingsValue))
        {
            // Try to add the element
            pipeline_add_postprocessor(factoryName, sourceData);

            // Get factory and element in order to apply stored settings
            factory = g_hash_table_lookup(sourceData->activePostprocessors, factoryName);
            element = g_hash_table_lookup(sourceData->activePostprocessorElements, factoryName);

            if (!factory || !element)
                continue;

            factory->apply_element_settings(settingsValue, element);

            g_free((void *)factoryName);
            g_variant_unref(settingsValue);
        }

        g_variant_unref(sourceInfoVariant);
        g_variant_unref(elementsVariant);
    }

    return FALSE;
}

// Update GUI elements based on app state
void sps_main_window_update_ui(SpsMainWindow *window)
{
    SpsApplication *app = SPS_MAIN_WINDOW_APP_GET(window);
    GstState pipelineState = app->gstData.pipeline->current_state;

    // Set start/pause button
    if (pipelineState == GST_STATE_PLAYING)
        gtk_button_set_label(GTK_BUTTON(window->btnStart), "Pause");
    else
        gtk_button_set_label(GTK_BUTTON(window->btnStart), "Start");

    // Set stop button
    gtk_widget_set_sensitive(window->btnStop, pipelineState == GST_STATE_PLAYING || pipelineState == GST_STATE_PAUSED);
    // Set start button
    gtk_widget_set_sensitive(window->btnStart, app->gstData.sources->len);
}

// Init GUI elements
static void sps_main_window_show(GtkWidget *parent)
{
    SpsMainWindow *window = SPS_MAIN_WINDOW(parent);

    sps_main_window_update_ui(window);

    GTK_WIDGET_CLASS(sps_main_window_parent_class)->show(parent);
}

static void sps_main_window_init(SpsMainWindow *window)
{
    gtk_widget_init_template(GTK_WIDGET(window));
}

static void sps_main_window_finalize(GObject *object)
{
    SpsMainWindow *window = SPS_MAIN_WINDOW(object);

    G_OBJECT_CLASS(sps_main_window_parent_class)->finalize(object);
}

static void sps_main_window_class_init(SpsMainWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->finalize = sps_main_window_finalize;
    widget_class->show = sps_main_window_show;

    // Load the main window from the GResource
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/gui/main-window.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsMainWindow, btnAddSource);
    gtk_widget_class_bind_template_child(widget_class, SpsMainWindow, btnStart);
    gtk_widget_class_bind_template_child(widget_class, SpsMainWindow, btnStop);
    gtk_widget_class_bind_template_child(widget_class, SpsMainWindow, boxSources);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, btn_add_source_handler);
    gtk_widget_class_bind_template_callback(widget_class, btn_start_handler);
    gtk_widget_class_bind_template_callback(widget_class, btn_stop_handler);
    gtk_widget_class_bind_template_callback(widget_class, window_realize_handler);
    gtk_widget_class_bind_template_callback(widget_class, window_close_handler);
}

// Handler for when a file should be opened by the app
void sps_main_window_open(SpsMainWindow *window, GFile *file)
{
}

// Convenience constructor wrapper
SpsMainWindow *sps_main_window_new(GApplication *app)
{
    return g_object_new(SPS_TYPE_MAIN_WINDOW, "application", app, NULL);
}
