#pragma once

#include <gtk/gtk.h>
#include <sps/sps.h>
#include "../application.h"

G_BEGIN_DECLS

#define SPS_TYPE_VIDEO_WINDOW (sps_video_window_get_type())
#define SPS_VIDEO_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_TYPE_VIDEO_WINDOW, SpsVideoWindow))
#define SPS_VIDEO_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_TYPE_VIDEO_WINDOW, SpsVideoWindow))
#define SPS_IS_VIDEO_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_TYPE_VIDEO_WINDOW))
#define SPS_IS_VIDEO_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_TYPE_VIDEO_WINDOW))

typedef struct _SpsVideoWindow SpsVideoWindow;
typedef struct _SpsVideoWindowClass SpsVideoWindowClass;

GType sps_video_window_get_type(void) G_GNUC_CONST;

struct _SpsVideoWindow
{
    GtkWindow parent;

    gboolean isFullScreen;
};

struct _SpsVideoWindowClass
{
    GtkWindowClass parent_class;
};

G_END_DECLS

#define SPS_VIDEO_WINDOW_APP_GET(window) SPS_APPLICATION(gtk_window_get_application(GTK_WINDOW(window)))

SpsVideoWindow *sps_video_window_new(GApplication *app);
