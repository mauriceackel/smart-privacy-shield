#pragma once

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_CHANGE_DETECTOR (gst_change_detector_get_type())
#define GST_CHANGE_DETECTOR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CHANGE_DETECTOR, GstChangeDetector))
#define GST_CHANGE_DETECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CHANGE_DETECTOR, GstChangeDetector))
#define GST_IS_CHANGE_DETECTOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CHANGE_DETECTOR))
#define GST_IS_CHANGE_DETECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CHANGE_DETECTOR))

typedef struct _GstChangeDetector GstChangeDetector;
typedef struct _GstChangeDetectorClass GstChangeDetectorClass;

GType gst_change_detector_get_type(void) G_GNUC_CONST;

struct _GstChangeDetector
{
  GstBaseTransform parent;

  GstBuffer *lastBuffer;
  
  float threshold;
  gboolean active;
};

struct _GstChangeDetectorClass
{
  GstBaseTransformClass parent_class;
};

G_END_DECLS
