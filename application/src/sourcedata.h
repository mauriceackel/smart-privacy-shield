#pragma once

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <sps/sps.h>

// Forward declarations to prevent circular dependency
typedef struct _SpsMainWindow SpsMainWindow;
typedef struct _SpsSourceRow SpsSourceRow;

G_BEGIN_DECLS

#define SPS_TYPE_SOURCE_DATA (sps_source_data_get_type())
#define SPS_SOURCE_DATA(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_TYPE_SOURCE_DATA, SpsSourceData))
#define SPS_SOURCE_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_TYPE_SOURCE_DATA, SpsSourceData))
#define SPS_IS_SOURCE_DATA(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_TYPE_SOURCE_DATA))
#define SPS_IS_SOURCE_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_TYPE_SOURCE_DATA))

typedef struct _SpsSourceData SpsSourceData;
typedef struct _SpsSourceDataClass SpsSourceDataClass;

GType sps_source_data_get_type(void) G_GNUC_CONST;

struct _SpsSourceData
{
    GObject parent;

    SpsSourceInfo sourceInfo;

    SpsSourceRow *row;

    GstElement *pipeline;
    GstElement *preprocessorQueue, *detectorQueue, *postprocessorQueue;
    GHashTable *activePreprocessorElements, *activeDetectorElements, *activePostprocessorElements; // Store the actual, instantiated GstElements

    GHashTable *activePreprocessors, *activeDetectors, *activePostprocessors; // Store the SpsPluginElementFactories
};

struct _SpsSourceDataClass
{
    GObjectClass parent_class;
};

G_END_DECLS

SpsSourceData *sps_source_data_new();
