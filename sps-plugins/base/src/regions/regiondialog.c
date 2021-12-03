#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include "regiondialog.h"
#include "utils.h"
#include <math.h>

G_DEFINE_TYPE(SpsPluginBaseRegionDialog, sps_plugin_base_region_dialog, GTK_TYPE_DIALOG);

enum
{
    PROP_0,
    PROP_APP_SINK,
};

static GstSample *storedSample = NULL;

static void cb_mouse_down(GtkWindow *window, GdkEvent *event, SpsPluginBaseRegionDialog *gui)
{
    guint mouseButton;
    gdk_event_get_button(event, &mouseButton);

    gdouble x, y;
    gdk_event_get_coords(event, &x, &y);

    if (mouseButton == 1)
    {
        gui->mouseDown = TRUE;
        gui->startX = gui->endX = x;
        gui->startY = gui->endY = y;

        gtk_widget_queue_draw(gui->drawingArea);
    }
}

static void cb_mouse_up(GtkWindow *window, GdkEvent *event, SpsPluginBaseRegionDialog *gui)
{
    guint mouseButton;
    gdk_event_get_button(event, &mouseButton);

    gdouble x, y;
    gdk_event_get_coords(event, &x, &y);

    if (gui->mouseDown && mouseButton == 1)
    {
        gui->mouseDown = FALSE;
        gui->endX = x;
        gui->endY = y;

        gtk_widget_queue_draw(gui->drawingArea);
    }
}

static void cb_mouse_move(GtkWindow *window, GdkEvent *event, SpsPluginBaseRegionDialog *gui)
{
    gdouble x, y;
    gdk_event_get_coords(event, &x, &y);

    if (gui->mouseDown)
    {
        gui->endX = x;
        gui->endY = y;

        gtk_widget_queue_draw(gui->drawingArea);
    }
}

static gboolean cb_draw(GtkWidget *widget, cairo_t *cr, SpsPluginBaseRegionDialog *gui)
{
    // Draw the image in the middle of the drawing area, or (if the image is
    // larger than the drawing area) draw the middle part of the image.
    gint widgetWidth, widgetHeight, bufferWidth, bufferHeight;
    widgetWidth = gtk_widget_get_allocated_width(widget);
    widgetHeight = gtk_widget_get_allocated_height(widget);
    bufferWidth = gdk_pixbuf_get_width(gui->pixelBuffer);
    bufferHeight = gdk_pixbuf_get_height(gui->pixelBuffer);

    gdouble scale;
    scale = fmin(widgetWidth / (gdouble)bufferWidth, widgetHeight / (gdouble)bufferHeight);

    // Draw image
    cairo_scale(cr, scale, scale);
    gdk_cairo_set_source_pixbuf(cr, gui->pixelBuffer, 0, 0);
    cairo_paint(cr);

    // Draw selection
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_set_line_width(cr, 4);
    cairo_rectangle(cr,
                    gui->startX / scale, gui->startY / scale,
                    (gui->endX - gui->startX) / scale,
                    (gui->endY - gui->startY) / scale);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);

    return FALSE;
}

static gboolean sps_plugin_base_region_dialog_get_bbox(SpsPluginBaseRegionDialog *gui, BoundingBox *bbox)
{
    if (gui->startX == gui->endX && gui->startY == gui->endY)
        return FALSE;

    gint widgetWidth, widgetHeight, bufferWidth, bufferHeight;
    widgetWidth = gtk_widget_get_allocated_width(gui->drawingArea);
    widgetHeight = gtk_widget_get_allocated_height(gui->drawingArea);
    bufferWidth = gdk_pixbuf_get_width(gui->pixelBuffer);
    bufferHeight = gdk_pixbuf_get_height(gui->pixelBuffer);

    gdouble scale;
    scale = fmin(widgetWidth / (gdouble)bufferWidth, widgetHeight / (gdouble)bufferHeight);

    // Figure out top left and bottom right corner
    gdouble topLeftX, topLeftY, bottomRightX, bottomRightY;
    topLeftX = fmin(gui->startX, gui->endX);
    topLeftY = fmin(gui->startY, gui->endY);
    bottomRightX = fmax(gui->startX, gui->endX);
    bottomRightY = fmax(gui->startY, gui->endY);

    // Convert corner points to integer bounding box taking scale into account
    gint x, y, width, height;
    x = (int)(topLeftX / scale);
    x = MIN(MAX(x, 0), bufferWidth);
    y = (int)(topLeftY / scale);
    y = MIN(MAX(y, 0), bufferHeight);
    width = (int)((bottomRightX - topLeftX) / scale);
    width = MIN(MAX(width, 0), bufferWidth - x);
    height = (int)((bottomRightY - topLeftY) / scale);
    height = MIN(MAX(height, 0), bufferHeight - y);

    // Set bbox values
    bbox->x = x;
    bbox->y = y;
    bbox->width = width;
    bbox->height = height;

    return TRUE;
}

static void sps_plugin_base_region_dialog_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseRegionDialog *gui = SPS_PLUGIN_BASE_REGION_DIALOG(object);

    switch (prop_id)
    {
    case PROP_APP_SINK:
        gui->appSink = gst_object_ref(g_value_get_pointer(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void sps_plugin_base_region_dialog_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    SpsPluginBaseRegionDialog *gui = SPS_PLUGIN_BASE_REGION_DIALOG(object);

    switch (prop_id)
    {
    case PROP_APP_SINK:
        g_value_set_pointer(value, gui->appSink);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void sps_plugin_base_region_dialog_constructed(GObject *object)
{
    SpsPluginBaseRegionDialog *gui = SPS_PLUGIN_BASE_REGION_DIALOG(object);

    GstSample *prerollSample, *sample;
    GstBuffer *buffer;

    // Try to get current sample from running pipeline
    g_signal_emit_by_name(gui->appSink, "try-pull-preroll", 10, &prerollSample); // Blocking for x ns
    g_signal_emit_by_name(gui->appSink, "try-pull-sample", 10, &sample);         // Blocking for x ns
    if (prerollSample != NULL)
    {
        // Received a preroll buffer, we take ownership
        if (storedSample)
            gst_sample_unref(storedSample);
        storedSample = gst_sample_ref(prerollSample);
    }

    if (sample)
    {
        goto has_sample;
    }
    else if (storedSample)
    {
        sample = storedSample;
        goto has_sample;
    }

    // No sample available at all, abort
    return;

has_sample:
    // Get buffer from sample
    buffer = gst_buffer_ref(gst_sample_get_buffer(sample));
    gst_sample_unref(sample);

    // Get memory from buffer
    GstMapInfo info;
    gst_buffer_map(buffer, &info, GST_MAP_READ);

    // Get pixel data from memory and convert rgb to bgr
    GstVideoMeta *meta = (GstVideoMeta *)gst_buffer_get_meta(buffer, GST_VIDEO_META_API_TYPE);
    if (meta)
    {
        gint bufferWidth, bufferHeight, actionAreaHeight;
        bufferWidth = meta->width;
        bufferHeight = meta->height;
        gtk_widget_get_preferred_height(gtk_dialog_get_action_area(GTK_DIALOG(gui)), NULL, &actionAreaHeight);

        gtk_window_set_default_size(GTK_WINDOW(gui), 700, (int)(700.f * bufferHeight / bufferWidth + actionAreaHeight + 10));

        gui->pixelBuffer = gdk_pixbuf_new_from_data(bgra2rgba(info.data, info.size), GDK_COLORSPACE_RGB, TRUE, 8, bufferWidth, bufferHeight, 4 * bufferWidth, (GdkPixbufDestroyNotify)g_free, NULL);
    }

    // Add callbacks to drawing area
    gtk_widget_add_events(gui->drawingArea, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

    // Free memory
    gst_buffer_unmap(buffer, &info);
    gst_buffer_unref(buffer);

    G_OBJECT_CLASS(sps_plugin_base_region_dialog_parent_class)->constructed(object);
}

static void sps_plugin_base_region_dialog_init(SpsPluginBaseRegionDialog *gui)
{
    gtk_widget_init_template(GTK_WIDGET(gui));
}

static void sps_plugin_base_region_dialog_finalize(GObject *object)
{
    SpsPluginBaseRegionDialog *gui = SPS_PLUGIN_BASE_REGION_DIALOG(object);

    gst_object_unref(gui->appSink);
    if (storedSample)
    {
        gst_sample_unref(storedSample);
        storedSample = NULL;
    }

    G_OBJECT_CLASS(sps_plugin_base_region_dialog_parent_class)->finalize(object);
}

static void sps_plugin_base_region_dialog_class_init(SpsPluginBaseRegionDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->set_property = sps_plugin_base_region_dialog_set_property;
    object_class->get_property = sps_plugin_base_region_dialog_get_property;
    object_class->constructed = sps_plugin_base_region_dialog_constructed;
    object_class->finalize = sps_plugin_base_region_dialog_finalize;

    // Register properties
    g_object_class_install_property(object_class, PROP_APP_SINK,
                                    g_param_spec_pointer("appsink", "App Sink",
                                                         "The app sink to get the current buffer from",
                                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    // Load template from resources
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/plugins/base/gui/region-dialog.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseRegionDialog, btnAbort);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseRegionDialog, btnApply);
    gtk_widget_class_bind_template_child(widget_class, SpsPluginBaseRegionDialog, drawingArea);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, cb_draw);
    gtk_widget_class_bind_template_callback(widget_class, cb_mouse_up);
    gtk_widget_class_bind_template_callback(widget_class, cb_mouse_down);
    gtk_widget_class_bind_template_callback(widget_class, cb_mouse_move);
}

SpsPluginBaseRegionDialog *sps_plugin_base_region_dialog_new(GtkWindow *parent, GstElement *appSink)
{
    return g_object_new(SPS_PLUGIN_BASE_TYPE_REGION_DIALOG, "transient-for", parent, "appsink", appSink, NULL);
}

gboolean sps_plugin_base_region_dialog_run(SpsPluginBaseRegionDialog *dialog, BoundingBox *bbox)
{
    gint response;

    if (!dialog->pixelBuffer)
        return FALSE;

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(GTK_WIDGET(dialog));

    if (response == GTK_RESPONSE_OK)
        return sps_plugin_base_region_dialog_get_bbox(dialog, bbox);

    return FALSE;
}
