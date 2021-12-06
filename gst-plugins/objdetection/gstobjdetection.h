#pragma once

typedef struct _GstObjDetection GstObjDetection;
typedef struct _GstObjDetectionClass GstObjDetectionClass;

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>
#include "inferencedata.h"
#include "inferenceutil.h"

G_BEGIN_DECLS

#define GST_TYPE_OBJ_DETECTION (gst_obj_detection_get_type())
#define GST_OBJ_DETECTION(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_OBJ_DETECTION, GstObjDetection))
#define GST_OBJ_DETECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_OBJ_DETECTION, GstObjDetection))
#define GST_IS_OBJ_DETECTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_OBJ_DETECTION))
#define GST_IS_OBJ_DETECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_OBJ_DETECTION))

GType gst_obj_detection_get_type(void) G_GNUC_CONST;

struct _GstObjDetection
{
    GstElement element;

    GstPad *modelSink, *bypassSink, *source;
    GstCollectPads *collectPads;
    GstCollectData *modelSinkData, *bypassSinkData;

    GstCaps *modelSinkCaps;

    GstInferenceData *lastInferenceData;

    const char *modelPath;
    const char *prefix;
    GPtrArray *labels;
    gboolean active;

    GstInferenceUtil *inferenceUtil;
};

struct _GstObjDetectionClass
{
    GstElementClass parent_class;
};

void gst_obj_detection_reconfigure_model_sink(GstObjDetection *filter, gint modelProportions);

G_END_DECLS
