#pragma once

#include <gtk/gtk.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define SPS_PLUGIN_BASE_TYPE_GUI_OBSTRUCT (sps_plugin_base_gui_obstruct_get_type())
#define SPS_PLUGIN_BASE_GUI_OBSTRUCT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_PLUGIN_BASE_TYPE_GUI_OBSTRUCT, SpsPluginBaseGuiObstruct))
#define SPS_PLUGIN_BASE_GUI_OBSTRUCT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_PLUGIN_BASE_TYPE_GUI_OBSTRUCT, SpsPluginBaseGuiObstruct))
#define SPS_PLUGIN_BASE_IS_GUI_OBSTRUCT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_PLUGIN_BASE_TYPE_GUI_OBSTRUCT))
#define SPS_PLUGIN_BASE_IS_GUI_OBSTRUCT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_PLUGIN_BASE_TYPE_GUI_OBSTRUCT))

typedef struct _SpsPluginBaseGuiObstruct SpsPluginBaseGuiObstruct;
typedef struct _SpsPluginBaseGuiObstructClass SpsPluginBaseGuiObstructClass;

GType sps_plugin_base_gui_obstruct_get_type(void) G_GNUC_CONST;

struct _SpsPluginBaseGuiObstruct
{
  GtkBox parent;

  GstElement *element;

  GtkWidget *lblHeading, lblDescription;

  GtkWidget *txtLabel;
  
  GtkWidget *btnAddLabel, *btnRemoveLabel;

  GtkWidget *cbActive, *cbInvert;

  GtkListStore *lstLabels;
  GtkWidget *tvLabels;

  GtkListStore *lstMaskingModes;
  GtkWidget *cbxMaskingMode;

  GtkWidget *btnStoreDefaults;
  
  GtkAdjustment *adjMargin;
};

struct _SpsPluginBaseGuiObstructClass
{
  GtkBoxClass parent_class;
};

G_END_DECLS

SpsPluginBaseGuiObstruct *sps_plugin_base_gui_obstruct_new(GstElement *element);
