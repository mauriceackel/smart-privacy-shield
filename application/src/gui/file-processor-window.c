#include <gtk/gtk.h>
#include <gst/gst.h>
#include <sps/sps.h>

#include "../application.h"
#include "../pipeline.h"
#include "file-processor-window.h"
#include "dialog-edit-source.h"

#define FILE_PROCESSING_FORMAT "a{sa{sv}}"

G_DEFINE_TYPE(SpsFileProcessorWindow, sps_file_processor_window, GTK_TYPE_WINDOW)

static void sps_file_processor_window_update_ui(SpsFileProcessorWindow *window)
{
    GstState pipelineState = window->pipeline->current_state;

    gboolean stateChanged = (pipelineState == GST_STATE_PLAYING) != window->pipelineRunning;

    window->pipelineRunning = pipelineState == GST_STATE_PLAYING;

    if (stateChanged)
    {
        // Set start/stop button
        if (window->pipelineRunning)
            gtk_button_set_label(GTK_BUTTON(window->btnStart), "Abort");
        else
        {
            gtk_button_set_label(GTK_BUTTON(window->btnStart), "Start");
        }

        // Set stop button
        gtk_widget_set_sensitive(window->btnConfigure, !window->pipelineRunning);
        gtk_widget_set_sensitive(window->fcInput, !window->pipelineRunning);
        // Set start button
        gtk_widget_set_sensitive(window->btnStart, window->filePath != NULL);
    }

    // Update progress
    gint64 pos, len;
    if (window->pipelineRunning && gst_element_query_position(window->pipeline, GST_FORMAT_TIME, &pos) && gst_element_query_duration(window->pipeline, GST_FORMAT_TIME, &len))
    {
        gfloat percentage = 100.0f * pos / len;

        char progress[50];
        g_snprintf(progress, 50, "Progress: %.2f%%", percentage);

        gtk_label_set_text(GTK_LABEL(window->lblProgress), progress);
    }
}

static gboolean timer_callback(SpsFileProcessorWindow *window)
{
    if (window->pipeline->current_state != GST_STATE_PLAYING)
        return FALSE;

    sps_file_processor_window_update_ui(window);
    return TRUE;
}

// Pipeline callbacks
static void pipeline_state_changed(GstBus *bus, GstMessage *msg, gpointer self)
{
    SpsFileProcessorWindow *window = SPS_FILE_PROCESSOR_WINDOW(self);

    GstState oldState, newState, pendingState;
    gst_message_parse_state_changed(msg, &oldState, &newState, &pendingState);

    if (newState == GST_STATE_PLAYING)
    {
        g_timeout_add_seconds(1, (GSourceFunc)timer_callback, window);
    }

    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(window->pipeline))
    {
        sps_file_processor_window_update_ui(window);
    }
}

static void pipeline_eos(GstBus *bus, GstMessage *msg, gpointer self)
{
    SpsFileProcessorWindow *window = SPS_FILE_PROCESSOR_WINDOW(self);

    pipeline_stop(window->pipeline);

    gtk_label_set_text(GTK_LABEL(window->lblProgress), "Done");
}

static void sps_file_processor_window_pipeline_init(SpsFileProcessorWindow *window)
{
    window->pipeline = pipeline_file_create(window->sourceData, TRUE);

    // Register for important bus events
    GstBus *bus = gst_element_get_bus(window->pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)pipeline_state_changed, (gpointer)window);
    g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)pipeline_eos, (gpointer)window);
    gst_object_unref(bus);
}

static void btn_start_handler(GtkButton *btn, SpsFileProcessorWindow *window)
{
    GstElement *pipeline = window->pipeline;

    if (window->pipelineRunning)
    {
        pipeline_stop(pipeline);

        gtk_label_set_text(GTK_LABEL(window->lblProgress), "Aborted");
    }
    else
    {
        if (window->pipeline->current_state == GST_STATE_NULL)
        {
            // First start, try to get to ready state
            GstStateChangeReturn ret = gst_element_set_state(window->pipeline, GST_STATE_READY);
            if (ret == GST_STATE_CHANGE_FAILURE)
                g_printerr("Unable to set pipeline to ready state.\n");
        }

        pipeline_start(pipeline);
    }
}

static void btn_configure_handler(GtkButton *btn, SpsFileProcessorWindow *window)
{
    SpsDialogEditSource *dialog = sps_dialog_edit_source_new(GTK_WINDOW(window), window->sourceData);
    sps_dialog_edit_source_run(dialog);
}

static void fc_input_changed(GtkFileChooserButton *fc, SpsFileProcessorWindow *window)
{
    gtk_label_set_text(GTK_LABEL(window->lblProgress), "");

    window->filePath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
    GstElement *fileSrc = gst_bin_get_by_name(GST_BIN(window->pipeline), "source");
    GstElement *fileSink = gst_bin_get_by_name(GST_BIN(window->pipeline), "sink");

    // Set source path
    g_object_set(G_OBJECT(fileSrc), "location", window->filePath, NULL);

    const char *dirName = g_path_get_dirname(window->filePath);
    const char *fileName = g_path_get_basename(window->filePath);
    const char *dot = strrchr(fileName, '.');
    size_t index;
    if (dot)
        index = (dot - fileName);
    else
        index = strlen(fileName);
    char* fileNameNoExtension = g_malloc(index + 1);
    memcpy(fileNameNoExtension, fileName, index);
    fileNameNoExtension[index] = 0;
    
    size_t outFileNameSize = strlen(fileNameNoExtension) + 5; // dot + "ogv" + NULL
    char* outFileName = g_malloc(outFileNameSize);
    memcpy(outFileName, fileName, index);
    snprintf(outFileName, outFileNameSize, "%s.ogv", fileNameNoExtension);

    char *outPath = g_build_filename(dirName, outFileName, NULL);

    // Set sink path
    g_object_set(G_OBJECT(fileSink), "location", outPath, NULL);

    // Free memory
    g_free((void *)dirName);
    g_free((void *)fileName);
    g_free((void *)fileNameNoExtension);
    g_free((void *)outPath);
    g_free((void *)outFileName);

    // Update ui
    sps_file_processor_window_update_ui(window);
}

static gboolean window_close_handler(SpsFileProcessorWindow *window)
{
    // Persist
    GSettings *settings = g_settings_new(APPLICATION_ID);

    // Store sources
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE(FILE_PROCESSING_FORMAT));

    GList *factoryNames;
    SpsPluginElementFactory *factory;
    GstElement *element;

    // Preprocessors
    g_variant_builder_open(builder, G_VARIANT_TYPE("{sa{sv}}")); // Open new element in dict
    g_variant_builder_add(builder, "s", "preprocessors");        // Set key for the new category

    g_variant_builder_open(builder, G_VARIANT_TYPE("a{sv}")); // Open dictionary that holds all the factories with their properties
    factoryNames = g_hash_table_get_keys(window->sourceData->activePreprocessors);
    while (factoryNames != NULL)
    {
        factory = g_hash_table_lookup(window->sourceData->activePreprocessors, factoryNames->data);
        element = g_hash_table_lookup(window->sourceData->activePreprocessorElements, factoryNames->data);

        if (!factory || !element)
            continue;

        GVariant *elementSettings = factory->create_element_settings(element);

        GVariant *entry = g_variant_new("{sv}", factoryNames->data, elementSettings);
        g_variant_builder_add_value(builder, entry);

        factoryNames = factoryNames->next;
    }
    g_variant_builder_close(builder); // Now inside element of category dict
    g_variant_builder_close(builder); // Now inside category dict

    // Detectors
    g_variant_builder_open(builder, G_VARIANT_TYPE("{sa{sv}}")); // Open new element in dict
    g_variant_builder_add(builder, "s", "detectors");            // Set key for the new category

    g_variant_builder_open(builder, G_VARIANT_TYPE("a{sv}")); // Open dictionary that holds all the factories with their properties
    factoryNames = g_hash_table_get_keys(window->sourceData->activeDetectors);
    while (factoryNames != NULL)
    {
        factory = g_hash_table_lookup(window->sourceData->activeDetectors, factoryNames->data);
        element = g_hash_table_lookup(window->sourceData->activeDetectorElements, factoryNames->data);

        if (!factory || !element)
            continue;

        GVariant *elementSettings = factory->create_element_settings(element);

        GVariant *entry = g_variant_new("{sv}", factoryNames->data, elementSettings);
        g_variant_builder_add_value(builder, entry);

        factoryNames = factoryNames->next;
    }
    g_variant_builder_close(builder); // Now inside element of category dict
    g_variant_builder_close(builder); // Now inside category dict

    // Postprocessors
    g_variant_builder_open(builder, G_VARIANT_TYPE("{sa{sv}}")); // Open new element in dict
    g_variant_builder_add(builder, "s", "postprocessors");       // Set key for the new category

    g_variant_builder_open(builder, G_VARIANT_TYPE("a{sv}")); // Open dictionary that holds all the factories with their properties
    factoryNames = g_hash_table_get_keys(window->sourceData->activePostprocessors);
    while (factoryNames != NULL)
    {
        factory = g_hash_table_lookup(window->sourceData->activePostprocessors, factoryNames->data);
        element = g_hash_table_lookup(window->sourceData->activePostprocessorElements, factoryNames->data);

        if (!factory || !element)
            continue;

        GVariant *elementSettings = factory->create_element_settings(element);

        GVariant *entry = g_variant_new("{sv}", factoryNames->data, elementSettings);
        g_variant_builder_add_value(builder, entry);

        factoryNames = factoryNames->next;
    }
    g_variant_builder_close(builder); // Now inside element of category dict
    g_variant_builder_close(builder); // Now inside category dict

    g_settings_set(settings, "file-processing", FILE_PROCESSING_FORMAT, builder);
    g_variant_builder_unref(builder);

    return FALSE;
}

static void sps_file_processor_window_show(GtkWidget *parent)
{
    SpsFileProcessorWindow *window = SPS_FILE_PROCESSOR_WINDOW(parent);

    sps_file_processor_window_update_ui(window);

    GTK_WIDGET_CLASS(sps_file_processor_window_parent_class)->show(parent);
}

static void sps_file_processor_window_init(SpsFileProcessorWindow *window)
{
    gtk_widget_init_template(GTK_WIDGET(window));

    // Init properties
    window->filePath = NULL;
    window->pipelineRunning = FALSE;
    window->sourceData = sps_source_data_new();

    // Restore settings
    GSettings *settings = g_settings_new(APPLICATION_ID);
    GVariant *fileProcessingVariant = g_settings_get_value(settings, "file-processing");

    GVariantIter *iter = NULL;
    const char *factoryName;
    GVariant *settingsValue;
    SpsPluginElementFactory *factory;
    GstElement *element;

    // Get preprocessors
    g_variant_lookup(fileProcessingVariant, "preprocessors", "a{sv}", &iter);
    while (g_variant_iter_next(iter, "{sv}", &factoryName, &settingsValue))
    {
        // Try to add the element
        pipeline_add_preprocessor(factoryName, window->sourceData);

        // Get factory and element in order to apply stored settings
        factory = g_hash_table_lookup(window->sourceData->activePreprocessors, factoryName);
        element = g_hash_table_lookup(window->sourceData->activePreprocessorElements, factoryName);

        if (!factory || !element)
            continue;

        factory->apply_element_settings(settingsValue, element);

        g_free((void *)factoryName);
        g_variant_unref(settingsValue);
    }

    // Get detectors
    g_variant_lookup(fileProcessingVariant, "detectors", "a{sv}", &iter);
    while (g_variant_iter_next(iter, "{sv}", &factoryName, &settingsValue))
    {
        // Try to add the element
        pipeline_add_detector(factoryName, window->sourceData);

        // Get factory and element in order to apply stored settings
        factory = g_hash_table_lookup(window->sourceData->activeDetectors, factoryName);
        element = g_hash_table_lookup(window->sourceData->activeDetectorElements, factoryName);

        if (!factory || !element)
            continue;

        factory->apply_element_settings(settingsValue, element);

        g_free((void *)factoryName);
        g_variant_unref(settingsValue);
    }

    // Get postprocessors
    g_variant_lookup(fileProcessingVariant, "postprocessors", "a{sv}", &iter);
    while (g_variant_iter_next(iter, "{sv}", &factoryName, &settingsValue))
    {
        // Try to add the element
        pipeline_add_postprocessor(factoryName, window->sourceData);

        // Get factory and element in order to apply stored settings
        factory = g_hash_table_lookup(window->sourceData->activePostprocessors, factoryName);
        element = g_hash_table_lookup(window->sourceData->activePostprocessorElements, factoryName);

        if (!factory || !element)
            continue;

        factory->apply_element_settings(settingsValue, element);

        g_free((void *)factoryName);
        g_variant_unref(settingsValue);
    }

    g_variant_unref(fileProcessingVariant);

    sps_file_processor_window_pipeline_init(window);
}

static void sps_file_processor_window_finalize(GObject *object)
{
    SpsFileProcessorWindow *window = SPS_FILE_PROCESSOR_WINDOW(object);

    // Free
    if (window->filePath)
        g_free((void *)window->filePath);

    g_object_unref(window->sourceData);

    G_OBJECT_CLASS(sps_file_processor_window_parent_class)->finalize(object);
}

static void sps_file_processor_window_class_init(SpsFileProcessorWindowClass *klass)
{
    GObjectClass *object_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

    object_class->finalize = sps_file_processor_window_finalize;
    widget_class->show = sps_file_processor_window_show;

    // Load the main window from the GResource
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/gui/file-processor-window.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsFileProcessorWindow, fcInput);
    gtk_widget_class_bind_template_child(widget_class, SpsFileProcessorWindow, btnConfigure);
    gtk_widget_class_bind_template_child(widget_class, SpsFileProcessorWindow, btnStart);
    gtk_widget_class_bind_template_child(widget_class, SpsFileProcessorWindow, lblProgress);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, btn_configure_handler);
    gtk_widget_class_bind_template_callback(widget_class, btn_start_handler);
    gtk_widget_class_bind_template_callback(widget_class, fc_input_changed);
    gtk_widget_class_bind_template_callback(widget_class, window_close_handler);
}

SpsFileProcessorWindow *sps_file_processor_window_new(GApplication *app)
{
    SpsFileProcessorWindow *window = g_object_new(SPS_TYPE_FILE_PROCESSOR_WINDOW, "application", app, NULL);

    return window;
}
