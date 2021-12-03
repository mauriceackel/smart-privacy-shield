#pragma once

typedef struct _GstInferenceUtil GstInferenceUtil;
typedef struct _GstInferenceUtilClass GstInferenceUtilClass;

#include "inferencedata.h"
#include "gstobjdetection.h"
#include <gst/gst.h>
#include <onnxruntime_c_api.h>
#ifdef WIN32
#include <Windows.h>
#include <gmodule.h>
#endif

G_BEGIN_DECLS

#define GST_TYPE_INFERENCE_UTIL (gst_inference_util_get_type())
#define GST_INFERENCE_UTIL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_INFERENCE_UTIL, GstInferenceUtil))
#define GST_INFERENCE_UTIL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_INFERENCE_UTIL, GstInferenceUtil))
#define GST_IS_INFERENCE_UTIL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_INFERENCE_UTIL))
#define GST_IS_INFERENCE_UTIL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_INFERENCE_UTIL))

GType gst_inference_util_get_type(void) G_GNUC_CONST;

struct _GstInferenceUtil
{
    GObject parent;

    gboolean initialized;

    guint sessionRefCount;
    gboolean sessionBlocked;
    GMutex mtxRefCount, mtxBlocked;
    GCond condRefCount, condBlocked;

    const OrtApi *ort;
    OrtEnv *environment;
    OrtSessionOptions *options;
    OrtSession *session;

    gint classCount;
    gint modelProportion;

#ifdef WIN32
    GModule *module;
#endif
};

struct _GstInferenceUtilClass
{
    GObjectClass parent_class;
};

G_END_DECLS

// Helpers
void inference_couple(GstInferenceData *source, GstInferenceData *target);
gboolean inference_apply(GstObjDetection *objDet, GstInferenceData *data);

// Methods
void gst_inference_util_initialize(GstInferenceUtil *self, GstObjDetection *objDet);
void gst_inference_util_reinitialize(GstInferenceUtil *self, GstObjDetection *objDet);
void gst_inference_util_finalize(GstInferenceUtil *self);
gboolean gst_inference_util_run_inference(GstInferenceUtil *self, GstObjDetection *objDet, GstInferenceData *data);
GstInferenceUtil *gst_inference_util_new();
