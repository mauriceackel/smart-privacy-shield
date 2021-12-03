#pragma once

#include <gtk/gtk.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define SPS_PLUGIN_BASE_TYPE_REGION_DIALOG (sps_plugin_base_region_dialog_get_type())
#define SPS_PLUGIN_BASE_REGION_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_PLUGIN_BASE_TYPE_REGION_DIALOG, SpsPluginBaseRegionDialog))
#define SPS_PLUGIN_BASE_REGION_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_PLUGIN_BASE_TYPE_REGION_DIALOG, SpsPluginBaseRegionDialog))
#define SPS_PLUGIN_BASE_IS_REGION_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_PLUGIN_BASE_TYPE_REGION_DIALOG))
#define SPS_PLUGIN_BASE_IS_REGION_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_PLUGIN_BASE_TYPE_REGION_DIALOG))

typedef struct _BoundingBox BoundingBox;
struct _BoundingBox
{
    gint x, y;
    gint width, height;
};

typedef struct _SpsPluginBaseRegionDialog SpsPluginBaseRegionDialog;
typedef struct _SpsPluginBaseRegionDialogClass SpsPluginBaseRegionDialogClass;

GType sps_plugin_base_region_dialog_get_type(void) G_GNUC_CONST;

struct _SpsPluginBaseRegionDialog
{
    GtkDialog parent;

    GstElement *appSink;

    gboolean mouseDown;
    gdouble startX, startY, endX, endY;

    GtkWidget *btnAbort, *btnApply;

    GdkPixbuf *pixelBuffer;
    GtkWidget *drawingArea;
};

struct _SpsPluginBaseRegionDialogClass
{
    GtkDialogClass parent_class;
};

G_END_DECLS

SpsPluginBaseRegionDialog *sps_plugin_base_region_dialog_new(GtkWindow *parent, GstElement *appSink);

gboolean sps_plugin_base_region_dialog_run(SpsPluginBaseRegionDialog *dialog, BoundingBox *bbox);
