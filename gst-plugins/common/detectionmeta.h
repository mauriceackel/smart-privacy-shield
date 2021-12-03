#pragma once

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _BoundingBox BoundingBox;
struct _BoundingBox
{
    gint x, y;
    gint width, height;
};

#define GST_TYPE_DETECTION (gst_detection_get_type())
#define GST_DETECTION(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DETECTION, GstDetection))
#define GST_DETECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DETECTION, GstDetection))
#define GST_IS_DETECTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DETECTION))
#define GST_IS_DETECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DETECTION))

GType gst_detection_get_type(void) G_GNUC_CONST;

typedef struct _GstDetection GstDetection;
typedef struct _GstDetectionClass GstDetectionClass;

struct _GstDetection
{
    GObject parent;

    const char *label;
    gfloat confidence;

    BoundingBox bbox;
};

struct _GstDetectionClass
{
    GObjectClass parent_class;
};

GstDetection *gst_detection_new(const char *label, gfloat confidence, BoundingBox bbox);
GstDetection *gst_detection_copy(GstDetection *detection);

typedef struct _GstDetectionMeta GstDetectionMeta;
struct _GstDetectionMeta
{
    GstMeta meta;

    GPtrArray *detections;
};

GType gst_detection_meta_api_get_type(void);
const GstMetaInfo *gst_detection_meta_get_info(void);
#define GST_DETECTION_META_GET(buf) ((GstDetectionMeta *)gst_buffer_get_meta(buf, gst_detection_meta_api_get_type()))
#define GST_DETECTION_META_ADD(buf) ((GstDetectionMeta *)gst_buffer_add_meta(buf, gst_detection_meta_get_info(), NULL))

// Bounding boxes
gboolean gst_bounding_box_do_intersect(BoundingBox *b1, BoundingBox *b2);
BoundingBox gst_bounding_box_intersect(BoundingBox *b1, BoundingBox *b2);
gboolean gst_bounding_box_contains(BoundingBox *box, BoundingBox *test);
GArray *gst_bounding_box_subtract_from_box(BoundingBox *rect, BoundingBox *subtract);
GArray *gst_bounding_box_subtract_from_boxes(GArray *inRects, BoundingBox *subtract);

G_END_DECLS
