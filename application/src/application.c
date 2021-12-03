#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <sps/sps.h>
#include "application.h"
#include "gui/main-window.h"
#include "gui/video-window.h"
#include "gui/file-processor-window.h"
#include "gui/dialog-preferences.h"
#include "pipeline.h"

G_DEFINE_TYPE(SpsApplication, sps_application, GTK_TYPE_APPLICATION);

// Menu handlers
static void sps_application_update_menu(SpsApplication *app)
{
    GSimpleAction *startPipeline = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "start"));
    GSimpleAction *stopPipeline = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "stop"));

    GstState pipelineState = app->gstData.pipeline->current_state;

    // Enable/Disable
    g_simple_action_set_enabled(stopPipeline, pipelineState == GST_STATE_PLAYING || pipelineState == GST_STATE_PAUSED);
    g_simple_action_set_enabled(startPipeline, app->gstData.sources->len);
}

static void start_pipeline_handler(GSimpleAction *action, GVariant *parameter, gpointer application)
{
    SpsApplication *app = SPS_APPLICATION(application);
    pipeline_start(app->gstData.pipeline);
}

static void stop_pipeline_handler(GSimpleAction *action, GVariant *parameter, gpointer application)
{
    SpsApplication *app = SPS_APPLICATION(application);
    pipeline_stop(app->gstData.pipeline);
}

static void open_preferences_handler(GSimpleAction *action, GVariant *parameter, gpointer application)
{
    SpsApplication *app = SPS_APPLICATION(application);
    SpsDialogPreferences *dialog = sps_dialog_preferences_new(GTK_WINDOW(app->mainWindow));

    sps_dialog_preferences_run(dialog);
}

static void open_file_processor_handler(GSimpleAction *action, GVariant *parameter, gpointer application)
{
    SpsApplication *app = SPS_APPLICATION(application);

    SpsFileProcessorWindow *window = sps_file_processor_window_new(G_APPLICATION(app));
    gtk_window_present(GTK_WINDOW(window));
}

static void close_app_handler(GSimpleAction *action, GVariant *parameter, gpointer application)
{
    // Ensure all windows are closed and have performed their closing routine
    SpsApplication *app = SPS_APPLICATION(application);

    gtk_window_close(GTK_WINDOW(app->mainWindow)); // This will quit the application
}

static GActionEntry actions[] = {
    {"start", start_pipeline_handler, NULL, NULL, NULL},
    {"stop", stop_pipeline_handler, NULL, NULL, NULL},
    {"preferences", open_preferences_handler, NULL, NULL, NULL},
    {"file_processor", open_file_processor_handler, NULL, NULL, NULL},
    {"quit", close_app_handler, NULL, NULL, NULL},
};

// Pipeline callbacks
static void sps_application_pipeline_state_changed(GstBus *bus, GstMessage *msg, gpointer self)
{
    SpsApplication *app = SPS_APPLICATION(self);

    GstState oldState, newState, pendingState;
    gst_message_parse_state_changed(msg, &oldState, &newState, &pendingState);

    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(app->gstData.pipeline))
    {
        // Show video window when pipeline starts
        if (oldState < newState && newState > GST_STATE_READY)
            gtk_widget_show(GTK_WIDGET(app->videoWindow));

        // Hide video window when pipeline stops
        if (newState < oldState && newState < GST_STATE_PAUSED)
            gtk_widget_hide(GTK_WIDGET(app->videoWindow));

        sps_main_window_update_ui(app->mainWindow);
        sps_application_update_menu(app);
    }
}

// Setup main pipeline
static gboolean sps_application_pipeline_init(SpsApplication *app)
{
    if (app->gstData.pipeline)
        g_warning("Pipeline was already initialized");

    // Create main pipeline
    app->gstData.pipeline = pipeline_main_create();

    // Add gtk sink widget to video window
    GstElement *gtkSink = gst_bin_get_by_name(GST_BIN(app->gstData.pipeline), "sink");
    GtkWidget *pipelineWidget;
    g_object_get(gtkSink, "widget", &pipelineWidget, NULL);
    gtk_container_add(GTK_CONTAINER(app->videoWindow), pipelineWidget);
    gtk_widget_show(GTK_WIDGET(pipelineWidget));
    g_object_unref(pipelineWidget);

    // Register for important bus events
    GstBus *bus = gst_element_get_bus(app->gstData.pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)sps_application_pipeline_state_changed, (gpointer)app);
    gst_object_unref(bus);

    // Put pipeline to ready state
    GstStateChangeReturn ret = gst_element_set_state(app->gstData.pipeline, GST_STATE_READY);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set pipeline to ready state.\n");
        return FALSE;
    }

    return TRUE;
}

// Handle sources
SpsSourceData *sps_application_add_source(SpsApplication *app, SpsSourceInfo *sourceInfo, gboolean addDefaultElements)
{
    SpsSourceData *sourceData = sps_source_data_new();

    sps_source_info_copy(sourceInfo, &sourceData->sourceInfo);

    // Create and add row
    sps_main_window_add_row(app->mainWindow, sourceData);

    // Add subpipe to pipeline
    GstElement *subpipe = pipeline_subpipe_create(sourceData, addDefaultElements);
    if (!subpipe)
    {
        GST_ERROR("Unable to create subpipe");
        return NULL;
    }
    pipeline_main_add_subpipe(app->gstData.pipeline, subpipe);

    // Store new source data
    g_ptr_array_add(app->gstData.sources, sourceData);

    // Update UI
    sps_main_window_update_ui(app->mainWindow);
    sps_application_update_menu(app);

    return sourceData;
}

void sps_application_remove_source(SpsApplication *app, SpsSourceData *sourceData)
{
    sps_main_window_remove_row(app->mainWindow, sourceData->row);
    pipeline_main_remove_subpipe(app->gstData.pipeline, sourceData->pipeline);

    // Will decrease ref count of sourceData
    g_ptr_array_remove(app->gstData.sources, sourceData);

    // Update UI
    sps_main_window_update_ui(app->mainWindow);
    sps_application_update_menu(app);
}

// Handler that will be called whenever the app is opened, no matter the type
static void sps_application_startup(GApplication *parent)
{
    SpsApplication *app = SPS_APPLICATION(parent);

    // Ensure gtk is initialized
    G_APPLICATION_CLASS(sps_application_parent_class)->startup(parent);

    // Register app actions
    g_action_map_add_action_entries(G_ACTION_MAP(parent), actions, G_N_ELEMENTS(actions), parent);

    // Setup app menu and main menu
    GtkBuilder *builder = gtk_builder_new_from_resource("/sps/gui/main-menu.glade");
    GMenuModel *appMenu = G_MENU_MODEL(gtk_builder_get_object(builder, "appMenu"));
    GMenuModel *mainMenu = G_MENU_MODEL(gtk_builder_get_object(builder, "mainMenu"));

    gtk_application_set_app_menu(GTK_APPLICATION(app), appMenu);
    gtk_application_set_menubar(GTK_APPLICATION(app), mainMenu);
    g_object_unref(builder);
}

// App stopped
static void sps_application_shutdown(GApplication *parent)
{
    SpsApplication *app = SPS_APPLICATION(parent);

    gst_element_set_state(app->gstData.pipeline, GST_STATE_NULL);
    gst_object_unref(app->gstData.pipeline);

    G_APPLICATION_CLASS(sps_application_parent_class)->shutdown(parent);
}

// App opened by clicking on icon
static void sps_application_activate(GApplication *parent)
{
    SpsApplication *app = SPS_APPLICATION(parent);

    // Prepare video window so that we already have the handle for the video overlay
    SpsVideoWindow *videoWindow = sps_video_window_new(G_APPLICATION(app));
    app->videoWindow = videoWindow;

    // Initialize pipeline
    sps_application_pipeline_init(app);

    // This has to be initialized last, as it depends on the pipeline bing initialized
    SpsMainWindow *mainWindow = sps_main_window_new(parent);
    app->mainWindow = mainWindow;
    gtk_window_present(GTK_WINDOW(mainWindow));
}

static void sps_application_init(SpsApplication *app)
{
    // Init properties
    app->gstData.sources = g_ptr_array_new_with_free_func(g_object_unref);
}

static void sps_application_finalize(GObject *object)
{
    SpsApplication *app = SPS_APPLICATION(object);

    // Free memory
    g_ptr_array_free(app->gstData.sources, TRUE);

    G_OBJECT_CLASS(sps_application_parent_class)->finalize(object);
}

static void sps_application_class_init(SpsApplicationClass *klass)
{
    GObjectClass *object_class;
    GApplicationClass *g_application_class;
    GtkApplicationClass *gtk_application_class;
    object_class = G_OBJECT_CLASS(klass);
    g_application_class = G_APPLICATION_CLASS(klass);
    gtk_application_class = GTK_APPLICATION_CLASS(klass);

    object_class->finalize = sps_application_finalize;

    g_application_class->activate = sps_application_activate;
    g_application_class->startup = sps_application_startup;
    g_application_class->shutdown = sps_application_shutdown;
}

// Convenience constructor wrapper
SpsApplication *sps_application_new(void)
{
    return g_object_new(SPS_TYPE_APPLICATION,
                        "application-id", APPLICATION_ID,
                        "flags", G_APPLICATION_HANDLES_OPEN,
                        NULL);
}
