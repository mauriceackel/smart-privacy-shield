/**
 * SECTION:element-changedetector
 * @title: changedetector
 * @short_description: element for identifying change between frames
 *
 * This element detects regions that have change in the frame compared to the previous one
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 videotestsrc ! changedetector threshold=50 ! autovideosink
 * ]| Identifies regions that have changed.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstchangedetector.h"
#include "changedetection.h"
#include <math.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#define LATENCY 50000000 // 50ms

GST_DEBUG_CATEGORY(gst_change_detector_debug);

enum
{
    PROP_0,
    PROP_ACTIVE,
    PROP_THRESHOLD,
};

// TODO: Possibly allow more formats in the future
GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-raw, format = (string) BGRA"));

GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-raw, format = (string) BGRA"));

#define gst_change_detector_parent_class parent_class
G_DEFINE_TYPE(GstChangeDetector, gst_change_detector, GST_TYPE_BASE_TRANSFORM);

// Property setter
static void gst_change_detector_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstChangeDetector *filter = GST_CHANGE_DETECTOR(object);

    switch (prop_id)
    {
    case PROP_THRESHOLD:
        filter->threshold = g_value_get_int(value) / 100.f;
        break;
    case PROP_ACTIVE:
        filter->active = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

// Property setter
static void gst_change_detector_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstChangeDetector *filter = GST_CHANGE_DETECTOR(object);

    switch (prop_id)
    {
    case PROP_THRESHOLD:
        g_value_set_int(value, (int)(filter->threshold * 100));
        break;
    case PROP_ACTIVE:
        g_value_set_boolean(value, filter->active);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

// Prepare to start proceassing -> open ressources, etc.
static gboolean gst_change_detector_start(GstChangeDetector *filter)
{
    return TRUE;
}

// Stop processing -> free ressources
static gboolean gst_change_detector_stop(GstChangeDetector *filter)
{
    if (filter->lastBuffer)
        gst_buffer_unref(filter->lastBuffer);

    filter->lastBuffer = NULL;

    return TRUE;
}

// Handle queries
static gboolean gst_change_detector_query(GstBaseTransform *parent, GstPadDirection direction, GstQuery *query)
{
    GstPad *pad;
    GstChangeDetector *filter = GST_CHANGE_DETECTOR(parent);

    if (direction == GST_PAD_SRC)
        pad = GST_BASE_TRANSFORM_SRC_PAD(parent);
    else
        pad = GST_BASE_TRANSFORM_SINK_PAD(parent);

    if (direction == GST_PAD_SRC && query->type == GST_QUERY_LATENCY)
    {
        GST_DEBUG_OBJECT(filter, "Received latency query");
        GstPad *peer;
        if (!(peer = gst_pad_get_peer(GST_BASE_TRANSFORM_SINK_PAD(parent))))
        {
            GST_ERROR_OBJECT(filter, "Unable to get peer pad to forward latency query.");
            return FALSE;
        }

        if (gst_pad_query(peer, query))
        {
            GstClockTime min, max;
            gboolean live;
            gst_query_parse_latency(query, &live, &min, &max);

            GST_DEBUG_OBJECT(filter, "Peer latency: min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT, GST_TIME_ARGS(min), GST_TIME_ARGS(max));
            GST_DEBUG_OBJECT(filter, "Our latency: %" GST_TIME_FORMAT, GST_TIME_ARGS(LATENCY));

            min += LATENCY;
            if (max != GST_CLOCK_TIME_NONE)
                max += LATENCY;

            GST_DEBUG_OBJECT(filter, "Calculated total latency : min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT, GST_TIME_ARGS(min), GST_TIME_ARGS(max));

            gst_query_set_latency(query, live, min, max);
        }
        else
        {
            GST_ERROR_OBJECT(filter, "Forwarded latency query failed on peer pad.");
            return FALSE;
        }

        gst_object_unref(peer);
        return TRUE;
    }

    return GST_BASE_TRANSFORM_CLASS(parent_class)->query(parent, direction, query);
}

// Handle incoming buffer
static GstFlowReturn gst_change_detector_transform_buffer(GstBaseTransform *base, GstBuffer *buffer)
{
    GstChangeDetector *filter = GST_CHANGE_DETECTOR(base);
    GstFlowReturn ret = GST_FLOW_OK;

    GST_DEBUG_OBJECT(filter, "Received buffer %p", buffer);

    if (!filter->active)
    {
        GST_DEBUG_OBJECT(filter, "Filter disabled, no processing");
        goto out;
    }

    // Detect and add meta to buffer
    detect_changes(filter, buffer, filter->lastBuffer);

    // Save new buffer
    if (filter->lastBuffer)
    {
        gst_buffer_unref(filter->lastBuffer);
    }
    filter->lastBuffer = gst_buffer_ref(buffer);

out:
    return ret;
}

// Object constructor -> called for every instance
static void gst_change_detector_init(GstChangeDetector *filter)
{
    // Set property initial value
    filter->threshold = 0;
    filter->active = TRUE;
    filter->lastBuffer = NULL;
}

// Object destructor -> called if an object gets destroyed
static void gst_change_detector_finalize(GObject *object)
{
    GstChangeDetector *filter = GST_CHANGE_DETECTOR(object);

    G_OBJECT_CLASS(gst_change_detector_parent_class)->finalize(object);
}

// Class constructor -> called exactly once
static void gst_change_detector_class_init(GstChangeDetectorClass *klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    GstBaseTransformClass *gstbasetransform_class;
    gobject_class = (GObjectClass *)klass;
    gstelement_class = (GstElementClass *)klass;
    gstbasetransform_class = (GstBaseTransformClass *)klass;

    // Set function pointers on this class
    gobject_class->finalize = gst_change_detector_finalize;
    gobject_class->set_property = gst_change_detector_set_property;
    gobject_class->get_property = gst_change_detector_get_property;

    // Define properties of plugin
    g_object_class_install_property(gobject_class, PROP_ACTIVE,
                                    g_param_spec_boolean("active", "Active",
                                                         "Whether preprocessing is active or not",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_THRESHOLD,
                                    g_param_spec_int("threshold", "Threshold",
                                                     "The threshold to define the detection sensitivity",
                                                     0, 100, 0,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    // Set plugin metadata
    gst_element_class_set_static_metadata(gstelement_class,
                                          "Change detector filter", "Filter/Video",
                                          "Change detector filter", "Maurice Ackel <m.ackel@icloud.com>");

    // Set the pad templates
    gst_element_class_add_pad_template(gstelement_class,
                                       gst_static_pad_template_get(&src_template));
    gst_element_class_add_pad_template(gstelement_class,
                                       gst_static_pad_template_get(&sink_template));

    // Set function pointers to implementation of base class
    gstbasetransform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_change_detector_transform_buffer);
    gstbasetransform_class->query = GST_DEBUG_FUNCPTR(gst_change_detector_query);
}

// Plugin initializer that registers the plugin
static gboolean plugin_init(GstPlugin *plugin)
{
    gboolean ret;

    GST_DEBUG_CATEGORY_INIT(gst_change_detector_debug, "changedetector", 0, "changedetector debug");

    ret = gst_element_register(plugin, "changedetector", GST_RANK_NONE, GST_TYPE_CHANGE_DETECTOR);

    return ret;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  changedetector,
                  "Change detector filter plugin",
                  plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
