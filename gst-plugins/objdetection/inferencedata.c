#include "inferencedata.h"

G_DEFINE_TYPE(GstInferenceData, gst_inference_data, G_TYPE_OBJECT);

void gst_inference_data_init(GstInferenceData *inferenceData)
{
    inferenceData->detections = g_ptr_array_new_with_free_func(g_object_unref);
}

void gst_inference_data_finalize(GObject *object)
{
    GstInferenceData *inferenceData = GST_INFERENCE_DATA(object);

    g_ptr_array_unref(inferenceData->detections);

    G_OBJECT_CLASS(gst_inference_data_parent_class)->finalize(object);
}

void gst_inference_data_class_init(GstInferenceDataClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = gst_inference_data_finalize;
}

GstInferenceData *gst_inference_data_new()
{
    return g_object_new(GST_TYPE_INFERENCE_DATA, NULL);
}
