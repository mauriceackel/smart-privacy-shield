#pragma once

#include <gtk/gtk.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define SPS_PLUGIN_BASE_TYPE_GUI_CHANGEDETECTOR (sps_plugin_base_gui_changedetector_get_type())
#define SPS_PLUGIN_BASE_GUI_CHANGEDETECTOR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_PLUGIN_BASE_TYPE_GUI_CHANGEDETECTOR, SpsPluginBaseGuiChangedetector))
#define SPS_PLUGIN_BASE_GUI_CHANGEDETECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_PLUGIN_BASE_TYPE_GUI_CHANGEDETECTOR, SpsPluginBaseGuiChangedetector))
#define SPS_PLUGIN_BASE_IS_GUI_CHANGEDETECTOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_PLUGIN_BASE_TYPE_GUI_CHANGEDETECTOR))
#define SPS_PLUGIN_BASE_IS_GUI_CHANGEDETECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_PLUGIN_BASE_TYPE_GUI_CHANGEDETECTOR))

typedef struct _SpsPluginBaseGuiChangedetector SpsPluginBaseGuiChangedetector;
typedef struct _SpsPluginBaseGuiChangedetectorClass SpsPluginBaseGuiChangedetectorClass;

GType sps_plugin_base_gui_changedetector_get_type(void) G_GNUC_CONST;

struct _SpsPluginBaseGuiChangedetector
{
  GtkBox parent;

  GstElement *element;

  GtkWidget *lblHeading, lblDescription;

  GtkWidget *txtThreshold;
  GtkAdjustment *adjThreshold;

  GtkWidget *btnStoreDefaults;

  GtkWidget *cbActive;
};

struct _SpsPluginBaseGuiChangedetectorClass
{
  GtkBoxClass parent_class;
};

G_END_DECLS

SpsPluginBaseGuiChangedetector *sps_plugin_base_gui_changedetector_new(GstElement *element);
