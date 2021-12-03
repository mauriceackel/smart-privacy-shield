#pragma once

#include <gtk/gtk.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define SPS_PLUGIN_BASE_TYPE_GUI_WINANALYZER (sps_plugin_base_gui_winanalyzer_get_type())
#define SPS_PLUGIN_BASE_GUI_WINANALYZER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_PLUGIN_BASE_TYPE_GUI_WINANALYZER, SpsPluginBaseGuiWinanalyzer))
#define SPS_PLUGIN_BASE_GUI_WINANALYZER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_PLUGIN_BASE_TYPE_GUI_WINANALYZER, SpsPluginBaseGuiWinanalyzer))
#define SPS_PLUGIN_BASE_IS_GUI_WINANALYZER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_PLUGIN_BASE_TYPE_GUI_WINANALYZER))
#define SPS_PLUGIN_BASE_IS_GUI_WINANALYZER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_PLUGIN_BASE_TYPE_GUI_WINANALYZER))

typedef struct _SpsPluginBaseGuiWinanalyzer SpsPluginBaseGuiWinanalyzer;
typedef struct _SpsPluginBaseGuiWinanalyzerClass SpsPluginBaseGuiWinanalyzerClass;

GType sps_plugin_base_gui_winanalyzer_get_type(void) G_GNUC_CONST;

struct _SpsPluginBaseGuiWinanalyzer
{
  GtkBox parent;

  GstElement *element;

  GtkWidget *lblHeading, lblDescription;

  GtkWidget *lblAvailableLabels;

  GtkWidget *cbxDisplay;

  GtkWidget *cbActive;

  GtkWidget *btnStoreDefaults;

  GtkWidget *txtPrefix;

  GtkListStore *lstDisplays;

  GArray *sourceInfos;
};

struct _SpsPluginBaseGuiWinanalyzerClass
{
  GtkBoxClass parent_class;
};

G_END_DECLS

SpsPluginBaseGuiWinanalyzer *sps_plugin_base_gui_winanalyzer_new(GstElement *element);
