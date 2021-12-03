/**
 * SECTION:element-regions
 * @title: regions
 * @short_description: element for performing post processing of detections
 *
 * This element marks specific regions on a buffer for exclude.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 videotestsrc ! regions prefix="example" regions="<10:20:40:60>" ! obstruct labels="<example:region>" ! autovideosink
 * ]| Blocks specified region on the testvideosrc.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstregions.h"
#include <detectionmeta.h>
#include <math.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY(gst_regions_debug);

enum
{
  PROP_0,
  PROP_PREFIX,
  PROP_ACTIVE,
  PROP_REGIONS,
  PROP_LABELS,
};

GstStaticPadTemplate regions_src_template = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

GstStaticPadTemplate regions_sink_template = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

#define gst_regions_parent_class parent_class
G_DEFINE_TYPE(GstRegions, gst_regions, GST_TYPE_BASE_TRANSFORM);

// Property setter
static void gst_regions_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GstRegions *filter = GST_REGIONS(object);

  switch (prop_id)
  {
  case PROP_REGIONS:
  {
    g_array_remove_range(filter->regions, 0, filter->regions->len);
    gint size = gst_value_array_get_size(value);
    for (gint i = 0; i < size; i++)
    {
      const GValue *val = gst_value_array_get_value(value, i);
      const char *regionString = g_value_get_string(val);

      char **coordStrings = g_strsplit(regionString, ":", 4);
      BoundingBox bbox = {
          .x = atoi(coordStrings[0]),
          .y = atoi(coordStrings[1]),
          .width = atoi(coordStrings[2]),
          .height = atoi(coordStrings[3]),
      };

      g_array_insert_val(filter->regions, i, bbox);
      g_strfreev(coordStrings);
    }
  }
  break;
  case PROP_PREFIX:
    filter->prefix = g_value_dup_string(value);
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
static void gst_regions_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GstRegions *filter = GST_REGIONS(object);

  switch (prop_id)
  {
  case PROP_REGIONS:
  {
    for (guint i = 0; i < filter->regions->len; i++)
    {
      GValue val = G_VALUE_INIT;
      g_value_init(&val, G_TYPE_STRING);

      BoundingBox *bbox = &g_array_index(filter->regions, BoundingBox, i);

      GString *coordinateString = g_string_new("");
      g_string_append_printf(coordinateString, "%i:%i:%i:%i", bbox->x, bbox->y, bbox->width, bbox->height);
      g_value_set_string(&val, coordinateString->str);
      g_string_free(coordinateString, TRUE);

      gst_value_array_append_value(value, &val);
      g_value_unset(&val);
    }
  }
  break;
  case PROP_PREFIX:
    g_value_set_string(value, filter->prefix);
    break;
  case PROP_ACTIVE:
    g_value_set_boolean(value, filter->active);
    break;
  case PROP_LABELS:
    {
        for (gint i = 0; i < filter->labels->len; i++)
        {
            GValue val = G_VALUE_INIT;
            g_value_init(&val, G_TYPE_STRING);
            g_value_set_string(&val, g_ptr_array_index(filter->labels, i));

            gst_value_array_append_value(value, &val);
            g_value_unset(&val);
        }
    }
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

// Prepare to start proceassing -> open ressources, etc.
static gboolean gst_regions_start(GstRegions *filter)
{
  return TRUE;
}

// Stop processing -> free ressources
static gboolean gst_regions_stop(GstRegions *filter)
{
  return TRUE;
}

// Handle incoming buffer
static GstFlowReturn gst_regions_transform_buffer(GstBaseTransform *base, GstBuffer *buffer)
{
  GstRegions *filter = GST_REGIONS(base);
  GstDetectionMeta *detectionMeta;
  GstVideoMeta *videoMeta;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT(filter, "Received buffer");

  if (!filter->active)
  {
    GST_DEBUG_OBJECT(filter, "Filter disabled, no processing");
    goto out;
  }

  videoMeta = (GstVideoMeta *)gst_buffer_get_meta(buffer, GST_VIDEO_META_API_TYPE);
  if (videoMeta == NULL)
  {
    // Ignore buffer
    GST_ERROR_OBJECT(filter, "No video meta on the buffer");
    goto out;
  }

  detectionMeta = GST_DETECTION_META_GET(buffer);
  if (detectionMeta == NULL)
  {
    // Add new detection meta
    GST_DEBUG_OBJECT(filter, "No object detection meta on buffer. Adding new meta.");
    detectionMeta = GST_DETECTION_META_ADD(buffer);
  }

  for (guint i = 0; i < filter->regions->len; i++)
  {
    BoundingBox bbox = g_array_index(filter->regions, BoundingBox, i);

    // Ignore bounding boxes that start outside the frame
    if (bbox.x > videoMeta->width || bbox.height > videoMeta->height)
      continue;

    // Clip bounding box
    bbox.x = MAX(bbox.x, 0);
    bbox.y = MAX(bbox.y, 0);
    bbox.width = MIN(bbox.width, videoMeta->width - bbox.x);
    bbox.height = MIN(bbox.height, videoMeta->height - bbox.y);

    GString *label = g_string_new(filter->prefix);
    g_string_append(label, ":regions");

    GstDetection *detection = gst_detection_new(label->str, 1, bbox);

    g_ptr_array_add(detectionMeta->detections, detection);

    g_string_free(label, TRUE);
  }

out:
  return ret;
}

// Object constructor -> called for every instance
static void gst_regions_init(GstRegions *filter)
{
  // Set property initial value
  filter->regions = g_array_new(FALSE, FALSE, sizeof(BoundingBox));
  filter->prefix = NULL;
  filter->active = TRUE;

  filter->labels = g_ptr_array_new_with_free_func(g_free);
  g_ptr_array_add(filter->labels, g_strdup("regions"));
}

// Object destructor -> called if an object gets destroyed
static void gst_regions_finalize(GObject *object)
{
  GstRegions *filter = GST_REGIONS(object);

  g_array_free(filter->regions, TRUE);
  g_free((void *)filter->prefix);
  g_ptr_array_free(filter->labels, TRUE);

  G_OBJECT_CLASS(gst_regions_parent_class)->finalize(object);
}

// Class constructor -> called exactly once
static void gst_regions_class_init(GstRegionsClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;
  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;
  gstbasetransform_class = (GstBaseTransformClass *)klass;

  // Set function pointers on this class
  gobject_class->finalize = gst_regions_finalize;
  gobject_class->set_property = gst_regions_set_property;
  gobject_class->get_property = gst_regions_get_property;

  // Define properties of plugin
  g_object_class_install_property(gobject_class, PROP_REGIONS,
                                  gst_param_spec_array("regions", "Regions",
                                                       "List of the regions that should be blocked",
                                                       g_param_spec_string("region",
                                                                           "Region",
                                                                           "A region definition that should be processed",
                                                                           NULL,
                                                                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_ACTIVE,
                                  g_param_spec_boolean("active", "Active",
                                                       "Whether preprocessing is active or not",
                                                       TRUE,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_PREFIX,
                                  g_param_spec_string("prefix", "Prefix",
                                                      "The prefixed used to annotate region labels", NULL,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_LABELS,
                                  gst_param_spec_array("labels", "Labels",
                                                       "List of the labels this detector provides",
                                                       g_param_spec_string("label",
                                                                           "Label",
                                                                           "Name of a label this detector provides",
                                                                           NULL,
                                                                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS),
                                                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  // Set plugin metadata
  gst_element_class_set_static_metadata(gstelement_class,
                                        "Regions filter", "Filter/Video",
                                        "Regions filter", "Maurice Ackel <m.ackel@icloud.com>");

  // Set the pad templates
  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&regions_src_template));
  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&regions_sink_template));

  // Set function pointers to implementation of base class
  gstbasetransform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_regions_transform_buffer);
}

// Plugin initializer that registers the plugin
static gboolean plugin_init(GstPlugin *plugin)
{
  gboolean ret;

  GST_DEBUG_CATEGORY_INIT(gst_regions_debug, "regions", 0, "regions debug");

  ret = gst_element_register(plugin, "regions", GST_RANK_NONE, GST_TYPE_REGIONS);

  return ret;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  regions,
                  "Region filter plugin",
                  plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
