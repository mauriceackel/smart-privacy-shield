#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gdk/gdk.h>
#include "../application.h"
#include "video-window.h"

G_DEFINE_TYPE(SpsVideoWindow, sps_video_window, GTK_TYPE_WINDOW)

static gboolean window_close_handler()
{
    return TRUE; // Cancel event
}

static void mouse_press_handler(GtkWidget *widget, GdkEventButton *event, SpsVideoWindow *window)
{
    GdkWindow *gdkWindow = gtk_widget_get_window(GTK_WIDGET(window));
    gdk_window_begin_move_drag(gdkWindow, event->button, event->x_root, event->y_root, event->time);
}

static void key_press_handler(GtkWidget *widget, GdkEventKey *event, SpsVideoWindow *window)
{
    if (!strcmp("f", event->string))
    {
        // "f" was pressed, toggle full screen
        if (window->isFullScreen)
        {
            gtk_window_unfullscreen(GTK_WINDOW(window));
            gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
            window->isFullScreen = FALSE;
        }
        else
        {
            gtk_window_set_decorated(GTK_WINDOW(window), TRUE);
            gtk_window_fullscreen(GTK_WINDOW(window));
            window->isFullScreen = TRUE;
        }
    }
}

static void sps_video_window_show(GtkWidget *widget)
{
    SpsVideoWindow *window = SPS_VIDEO_WINDOW(widget);
    SpsApplication *app = SPS_VIDEO_WINDOW_APP_GET(window);

    GTK_WIDGET_CLASS(sps_video_window_parent_class)->show(widget);
}

static void sps_video_window_init(SpsVideoWindow *window)
{
    gtk_widget_init_template(GTK_WIDGET(window));

    gtk_widget_add_events(GTK_WIDGET(window), GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
    g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(mouse_press_handler), window);
    g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(key_press_handler), window);

    window->isFullScreen = FALSE;
}

static void sps_video_window_finalize(GObject *object)
{
    SpsVideoWindow *window = SPS_VIDEO_WINDOW(object);

    G_OBJECT_CLASS(sps_video_window_parent_class)->finalize(object);
}

static void sps_video_window_class_init(SpsVideoWindowClass *klass)
{
    GObjectClass *object_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

    object_class->finalize = sps_video_window_finalize;

    // Load the main window from the GResource
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/gui/video-window.glade");

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, window_close_handler);

    widget_class->show = sps_video_window_show;
}

SpsVideoWindow *sps_video_window_new(GApplication *app)
{
    SpsVideoWindow *window = g_object_new(SPS_TYPE_VIDEO_WINDOW, "application", app, NULL);

    return window;
}
