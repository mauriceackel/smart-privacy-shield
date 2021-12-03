#pragma once

#include <gtk/gtk.h>
#include <sps/sps.h>
#include "../application.h"
#include "../sourcedata.h"

G_BEGIN_DECLS

#define SPS_TYPE_MAIN_WINDOW (sps_main_window_get_type())
#define SPS_MAIN_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_TYPE_MAIN_WINDOW, SpsMainWindow))
#define SPS_MAIN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_TYPE_MAIN_WINDOW, SpsMainWindow))
#define SPS_IS_MAIN_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_TYPE_MAIN_WINDOW))
#define SPS_IS_MAIN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_TYPE_MAIN_WINDOW))

typedef struct _SpsMainWindow SpsMainWindow;
typedef struct _SpsMainWindowClass SpsMainWindowClass;

GType sps_main_window_get_type(void) G_GNUC_CONST;

struct _SpsMainWindow
{
    GtkApplicationWindow parent;

    GtkWidget *btnAddSource, *btnStart, *btnStop;
    GtkWidget *boxSources;
};

struct _SpsMainWindowClass
{
    GtkApplicationWindowClass parent_class;
};

G_END_DECLS

#define SPS_MAIN_WINDOW_APP_GET(window) SPS_APPLICATION(gtk_window_get_application(GTK_WINDOW(window)))

SpsMainWindow *sps_main_window_new(GApplication *app);

void sps_main_window_open(SpsMainWindow *window, GFile *file);
void sps_main_window_update_ui(SpsMainWindow *window);

void sps_main_window_add_row(SpsMainWindow *window, SpsSourceData *sourceData);
void sps_main_window_remove_row(SpsMainWindow *window, SpsSourceRow *row);
