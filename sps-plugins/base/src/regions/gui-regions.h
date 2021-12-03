#pragma once

#include <gtk/gtk.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define SPS_PLUGIN_BASE_TYPE_GUI_REGIONS (sps_plugin_base_gui_regions_get_type())
#define SPS_PLUGIN_BASE_GUI_REGIONS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_PLUGIN_BASE_TYPE_GUI_REGIONS, SpsPluginBaseGuiRegions))
#define SPS_PLUGIN_BASE_GUI_REGIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_PLUGIN_BASE_TYPE_GUI_REGIONS, SpsPluginBaseGuiRegions))
#define SPS_PLUGIN_BASE_IS_GUI_REGIONS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_PLUGIN_BASE_TYPE_GUI_REGIONS))
#define SPS_PLUGIN_BASE_IS_GUI_REGIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_PLUGIN_BASE_TYPE_GUI_REGIONS))

typedef struct _SpsPluginBaseGuiRegions SpsPluginBaseGuiRegions;
typedef struct _SpsPluginBaseGuiRegionsClass SpsPluginBaseGuiRegionsClass;

GType sps_plugin_base_gui_regions_get_type(void) G_GNUC_CONST;

struct _SpsPluginBaseGuiRegions
{
  GtkBox parent;

  GstElement *element, *appSink;

  GtkWidget *lblHeading, lblDescription;

  GtkWidget *lblAvailableLabels;

  GtkWidget *txtX, *txtY, *txtWidth, *txtHeight;
  GtkAdjustment *adjX, *adjY, *adjWidth, *adjHeight;

  GtkWidget *btnAddRegion, *btnRemoveRegion, *btnSelectRegion;
  GtkListStore *lstRegions;
  GtkWidget *tvRegions;

  GtkWidget *cbActive;

  GtkWidget *btnStoreDefaults;

  GtkWidget *txtPrefix;
};

struct _SpsPluginBaseGuiRegionsClass
{
  GtkBoxClass parent_class;
};

G_END_DECLS

SpsPluginBaseGuiRegions *sps_plugin_base_gui_regions_new(GstElement *element);
