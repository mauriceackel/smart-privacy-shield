#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gtk/gtk.h>
#include "gui/main-window.h"
#include "gui/video-window.h"

G_BEGIN_DECLS

#define SPS_TYPE_APPLICATION (sps_application_get_type())
#define SPS_APPLICATION(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_TYPE_APPLICATION, SpsApplication))
#define SPS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_TYPE_APPLICATION, SpsApplication))
#define SPS_IS_APPLICATION(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_TYPE_APPLICATION))
#define SPS_IS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_TYPE_APPLICATION))

typedef struct _SpsApplication SpsApplication;
typedef struct _SpsApplicationClass SpsApplicationClass;

GType sps_application_get_type(void) G_GNUC_CONST;

typedef struct _GstData GstData;
struct _GstData
{
    GstElement *pipeline; // Main pipeline
    GPtrArray *sources;   // Stores all added sources in a SourceData struct
};

struct _SpsApplication
{
    GtkApplication parent;

    SpsVideoWindow *videoWindow;
    SpsMainWindow *mainWindow;

    GstData gstData;
};

struct _SpsApplicationClass
{
    GtkApplicationClass parent_class;
};

G_END_DECLS

SpsApplication *sps_application_new(void);

SpsSourceData *sps_application_add_source(SpsApplication *app, SpsSourceInfo *sourceInfo, gboolean addDefaultElements);
void sps_application_remove_source(SpsApplication *app, SpsSourceData *sourceData);
