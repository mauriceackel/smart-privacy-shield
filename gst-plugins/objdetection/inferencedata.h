#pragma once

typedef struct _GstInferenceData GstInferenceData;
typedef struct _GstInferenceDataClass GstInferenceDataClass;

#include <gst/gst.h>
#include <glib.h>

G_BEGIN_DECLS

#define GST_TYPE_INFERENCE_DATA (gst_inference_data_get_type())
#define GST_INFERENCE_DATA(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_INFERENCE_DATA, GstInferenceData))
#define GST_INFERENCE_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_INFERENCE_DATA, GstInferenceData))
#define GST_IS_INFERENCE_DATA(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_INFERENCE_DATA))
#define GST_IS_INFERENCE_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_INFERENCE_DATA))

GType gst_inference_data_get_type(void) G_GNUC_CONST;

struct _GstInferenceData
{
    GObject parent;

    GstBuffer *modelBuffer, *bypassBuffer;
    gboolean processed, error;
    GPtrArray *detections;
};

struct _GstInferenceDataClass
{
    GObjectClass parent_class;
};

G_END_DECLS

GstInferenceData *gst_inference_data_new();
