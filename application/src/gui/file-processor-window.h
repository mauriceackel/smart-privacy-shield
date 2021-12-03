#pragma once

#include <gtk/gtk.h>
#include <sps/sps.h>
#include "../application.h"

G_BEGIN_DECLS

#define SPS_TYPE_FILE_PROCESSOR_WINDOW (sps_file_processor_window_get_type())
#define SPS_FILE_PROCESSOR_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_TYPE_FILE_PROCESSOR_WINDOW, SpsFileProcessorWindow))
#define SPS_FILE_PROCESSOR_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_TYPE_FILE_PROCESSOR_WINDOW, SpsFileProcessorWindow))
#define SPS_IS_FILE_PROCESSOR_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_TYPE_FILE_PROCESSOR_WINDOW))
#define SPS_IS_FILE_PROCESSOR_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_TYPE_FILE_PROCESSOR_WINDOW))

typedef struct _SpsFileProcessorWindow SpsFileProcessorWindow;
typedef struct _SpsFileProcessorWindowClass SpsFileProcessorWindowClass;

GType sps_file_processor_window_get_type(void) G_GNUC_CONST;

struct _SpsFileProcessorWindow
{
    GtkWindow parent;

    GtkWidget *lblProgress;
    GtkWidget *btnStart;
    GtkWidget *btnConfigure;
    GtkWidget *fcInput;

    GstElement *pipeline;

    char* filePath;
    gboolean pipelineRunning;
    SpsSourceData *sourceData;
};

struct _SpsFileProcessorWindowClass
{
    GtkWindowClass parent_class;
};

G_END_DECLS

#define SPS_FILE_PROCESSOR_WINDOW_APP_GET(window) SPS_APPLICATION(gtk_window_get_application(GTK_WINDOW(window)))

SpsFileProcessorWindow *sps_file_processor_window_new(GApplication *app);
