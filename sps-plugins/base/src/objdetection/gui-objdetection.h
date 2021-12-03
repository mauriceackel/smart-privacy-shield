#pragma once

#include <gtk/gtk.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define SPS_PLUGIN_BASE_TYPE_GUI_OBJDETECTION (sps_plugin_base_gui_objdetection_get_type())
#define SPS_PLUGIN_BASE_GUI_OBJDETECTION(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_PLUGIN_BASE_TYPE_GUI_OBJDETECTION, SpsPluginBaseGuiObjdetection))
#define SPS_PLUGIN_BASE_GUI_OBJDETECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_PLUGIN_BASE_TYPE_GUI_OBJDETECTION, SpsPluginBaseGuiObjdetection))
#define SPS_PLUGIN_BASE_IS_GUI_OBJDETECTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_PLUGIN_BASE_TYPE_GUI_OBJDETECTION))
#define SPS_PLUGIN_BASE_IS_GUI_OBJDETECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_PLUGIN_BASE_TYPE_GUI_OBJDETECTION))

typedef struct _SpsPluginBaseGuiObjdetection SpsPluginBaseGuiObjdetection;
typedef struct _SpsPluginBaseGuiObjdetectionClass SpsPluginBaseGuiObjdetectionClass;

GType sps_plugin_base_gui_objdetection_get_type(void) G_GNUC_CONST;

struct _SpsPluginBaseGuiObjdetection
{
  GtkBox parent;

  GstElement *element;

  GtkWidget *lblHeading, lblDescription;

  GtkWidget *lblAvailableLabels;

  GtkWidget *fcModel;

  GtkWidget *cbActive;

  GtkWidget *btnStoreDefaults;

  GtkWidget *txtPrefix;
};

struct _SpsPluginBaseGuiObjdetectionClass
{
  GtkBoxClass parent_class;
};

G_END_DECLS

SpsPluginBaseGuiObjdetection *sps_plugin_base_gui_objdetection_new(GstElement *element);
