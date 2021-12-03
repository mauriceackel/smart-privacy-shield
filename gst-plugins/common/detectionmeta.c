#include <gst/gst.h>
#include "detectionmeta.h"

// Define detection type
G_DEFINE_TYPE(GstDetection, gst_detection, G_TYPE_OBJECT);

void gst_detection_init(GstDetection *detection)
{
}

void gst_detection_finalize(GObject *object)
{
    GstDetection *detection = GST_DETECTION(object);

    g_free((void *)detection->label);

    G_OBJECT_CLASS(gst_detection_parent_class)->finalize(object);
}

void gst_detection_class_init(GstDetectionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = gst_detection_finalize;
}

GstDetection *gst_detection_new(const char *label, gfloat confidence, BoundingBox bbox)
{
    GstDetection *detection = g_object_new(GST_TYPE_DETECTION, NULL);

    detection->label = g_strdup(label);
    detection->confidence = confidence;
    detection->bbox = bbox;

    return detection;
}

GstDetection *gst_detection_copy(GstDetection *detection)
{
    GstDetection *newDetection = g_object_new(GST_TYPE_DETECTION, NULL);

    newDetection->label = g_strdup(detection->label);
    newDetection->confidence = detection->confidence;
    newDetection->bbox = detection->bbox;

    return newDetection;
}

// Define the new metadata type
GType gst_detection_meta_api_get_type(void)
{
    static GType type;
    static const gchar *tags[] = {"memory", NULL};

    if (g_once_init_enter(&type))
    {
        GType _type = gst_meta_api_type_register("GstDetectionMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }

    return type;
}

// Set the initial values of the metadata
static gboolean gst_detection_meta_init(GstMeta *basemeta, gpointer params, GstBuffer *buffer)
{
    GstDetectionMeta *meta = (GstDetectionMeta *)basemeta;

    meta->detections = g_ptr_array_new_with_free_func(g_object_unref);

    return TRUE;
}

// Free meta object
static void gst_detection_meta_free(GstMeta *basemeta, GstBuffer *buffer)
{
    GstDetectionMeta *meta = (GstDetectionMeta *)basemeta;

    // Free array including element data
    g_ptr_array_free(meta->detections, TRUE);
}

// Transform meta object
static gboolean gst_detection_meta_transform(GstBuffer *dest, GstMeta *meta, GstBuffer *src, GQuark type, gpointer data)
{
    GstDetectionMeta *oldMeta, *newMeta;
    oldMeta = (GstDetectionMeta *)meta;

    if (GST_META_TRANSFORM_IS_COPY(type))
    {
        // Create new meta on destination
        newMeta = GST_DETECTION_META_ADD(dest);

        if (!newMeta)
            return FALSE;

        // Copy over content
        g_ptr_array_free(newMeta->detections, TRUE);
        newMeta->detections = g_ptr_array_copy(oldMeta->detections, (GCopyFunc)gst_detection_copy, NULL);

        return TRUE;
    }
    else
    {
        // transform type not supported
        return FALSE;
    }
}

// Build a metadata info object
const GstMetaInfo *gst_detection_meta_get_info(void)
{
    static const GstMetaInfo *meta_objdetection_info = NULL;

    if (g_once_init_enter(&meta_objdetection_info))
    {
        const GstMetaInfo *meta =
            gst_meta_register(gst_detection_meta_api_get_type(), "GstDetectionMeta",
                              sizeof(GstDetectionMeta), (GstMetaInitFunction)gst_detection_meta_init,
                              (GstMetaFreeFunction)gst_detection_meta_free, (GstMetaTransformFunction)gst_detection_meta_transform);
        g_once_init_leave(&meta_objdetection_info, meta);
    }

    return meta_objdetection_info;
}

// Bounding boxes

// Check if bounding boxes intersect
gboolean gst_bounding_box_do_intersect(BoundingBox *b1, BoundingBox *b2)
{
    return !(
        (b1->x + b1->width <= b2->x) ||
        (b1->y + b1->height <= b2->y) ||
        (b1->x >= b2->x + b2->width) ||
        (b1->y >= b2->y + b2->height));
}

BoundingBox gst_bounding_box_intersect(BoundingBox *b1, BoundingBox *b2)
{
    if (!gst_bounding_box_do_intersect(b1, b2))
    {
        BoundingBox empty = {0, 0, 0, 0};
        return empty;
    }

    BoundingBox intersection;
    intersection.x = MAX(b1->x, b2->x);
    intersection.y = MAX(b1->y, b2->y);
    intersection.width = MIN((b1->x + b1->width), (b2->x + b2->width)) - intersection.x;
    intersection.height = MIN((b1->y + b1->height), (b2->y + b2->height)) - intersection.y;

    return intersection;
}

// Check of first bounding box contains second
gboolean gst_bounding_box_contains(BoundingBox *box, BoundingBox *test)
{
    return (test->x >= box->x) &&
           (test->y >= box->y) &&
           (test->x + test->width <= box->x + box->width) &&
           (test->y + test->height <= box->y + box->height);
}

// Subtracts the second bounding box from the first
GArray *gst_bounding_box_subtract_from_box(BoundingBox *rect, BoundingBox *subtract)
{
    GArray *outRects = g_array_new(FALSE, FALSE, sizeof(BoundingBox));

    // Rectangle is fully contained, return no bounding box
    if (gst_bounding_box_contains(subtract, rect))
        return outRects;

    // Nothing to subtract, return rect as is
    if (!gst_bounding_box_do_intersect(rect, subtract))
    {
        g_array_append_vals(outRects, rect, 1);
        return outRects;
    }

    // There can be four resulting rectangles at max

    // Compute the top rectangle
    gint rtHeight = subtract->y - rect->y; // Difference between top corners
    if (rtHeight > 0)
    {
        // Top edge of subtract is below top edge of rect

        BoundingBox top = {
            .x = rect->x,
            .y = rect->y,
            .width = rect->width,
            .height = rtHeight,
        };
        g_array_append_val(outRects, top);
    }

    // Compute the bottom rectangle
    gint rectBottom = rect->y + rect->height;             // bottom edge of rect
    gint subtractBottom = subtract->y + subtract->height; // y value of subtract bottom edge
    gint rbHeight = rectBottom - subtractBottom;          // subtract bottom edges
    if (rbHeight > 0)
    {
        // Bottom edge of subtract is above bottom edge of rect and

        BoundingBox bottom = {
            .x = rect->x,
            .y = subtractBottom,
            .width = rect->width,
            .height = rbHeight,
        };
        g_array_append_val(outRects, bottom);
    }

    gint y1 = subtract->y > rect->y ? subtract->y : rect->y;             // largest top edge value
    gint y2 = subtractBottom < rectBottom ? subtractBottom : rectBottom; // smallest bottom edge value
    gint rsHeight = y2 - y1;                                             // height of rects at the sides

    // Compute the left rectangle
    gint rlWidth = subtract->x - rect->x; // Difference between left corners
    if (rlWidth > 0 && rsHeight > 0)
    {
        // Left edge of subtract to the right of left edge of rect and height of side rectangles > 0

        BoundingBox left = {
            .x = rect->x,
            .y = y1,
            .width = rlWidth,
            .height = rsHeight,
        };
        g_array_append_val(outRects, left);
    }

    // Compute the right rectangle
    gint subtractRight = subtract->x + subtract->width;   // right edge of subtract
    gint rrWidth = rect->x + rect->width - subtractRight; // distance between left edges
    if (rrWidth > 0 && rsHeight > 0)
    {
        // Right edge of subtract to the left of right edge of rect and height of side rectangles > 0

        BoundingBox right = {
            .x = subtractRight,
            .y = y1,
            .width = rrWidth,
            .height = rsHeight,
        };
        g_array_append_val(outRects, right);
    }

    return outRects;
}

GArray *gst_bounding_box_subtract_from_boxes(GArray *inRects, BoundingBox *subtract)
{
    GArray *outRects = g_array_new(FALSE, FALSE, sizeof(BoundingBox));

    for (gint i = 0; i < inRects->len; i++)
    {
        BoundingBox *rect = &g_array_index(inRects, BoundingBox, i);

        GArray *result = gst_bounding_box_subtract_from_box(rect, subtract);
        g_array_append_vals(outRects, result->data, result->len);
        g_array_free(result, TRUE);
    }

    return outRects;
}
