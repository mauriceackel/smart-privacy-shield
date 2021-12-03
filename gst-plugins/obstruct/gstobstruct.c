/**
 * SECTION:element-obstruct
 * @title: obstruct
 * @short_description: element for performing post processing of detections
 *
 * This element applies some processing on the image buffer based on existing detections on the buffer.
 * It uses and filters the metadata from objdetection elements.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 videotestsrc ! objdetection model="/path/to/model" prefix="example" ! obstruct labels="<prefixA:1,prefixA:2,prefixB:1>" ! autovideosink
 * ]| Runs inference on the testvideosrc.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstobstruct.h"
#include "obstruct.h"
#include <detectionmeta.h>

#include <gst/gst.h>
#include <gst/video/video.h>

GST_DEBUG_CATEGORY(gst_obstruct_debug);

enum
{
  PROP_0,
  PROP_LABELS,
  PROP_FILTER_TYPE,
  PROP_MARGIN,
  PROP_ACTIVE,
  PROP_INVERT,
};

// TODO: Possibly allow more formats in the future
GstStaticPadTemplate obstruct_src_template = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-raw, format = (string) BGRA"));

GstStaticPadTemplate obstruct_sink_template = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-raw, format = (string) BGRA"));

#define gst_obstruct_parent_class parent_class
G_DEFINE_TYPE(GstObstruct, gst_obstruct, GST_TYPE_BASE_TRANSFORM);

// Property setter
static void gst_obstruct_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GstObstruct *filter = GST_OBSTRUCT(object);

  switch (prop_id)
  {
  case PROP_LABELS:
  {
    g_hash_table_remove_all(filter->labels);
    gint size = gst_value_array_get_size(value);
    for (gint i = 0; i < size; i++)
    {
      const GValue *val = gst_value_array_get_value(value, i);
      g_hash_table_add(filter->labels, g_value_dup_string(val));
    }
  }
  break;
  case PROP_FILTER_TYPE:
    filter->filterType = g_value_get_uint(value);
    break;
  case PROP_MARGIN:
    filter->margin = g_value_get_float(value);
    break;
  case PROP_ACTIVE:
    filter->active = g_value_get_boolean(value);
    break;
  case PROP_INVERT:
    filter->invert = g_value_get_boolean(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

// Property setter
static void gst_obstruct_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GstObstruct *filter = GST_OBSTRUCT(object);

  switch (prop_id)
  {
  case PROP_LABELS:
  {
    GList *labels = g_hash_table_get_values(filter->labels);
    while (labels != NULL)
    {
      GValue val = G_VALUE_INIT;
      g_value_init(&val, G_TYPE_STRING);
      g_value_set_string(&val, labels->data);

      gst_value_array_append_value(value, &val);
      g_value_unset(&val);

      labels = labels->next;
    }
    g_list_free(labels);
  }
  break;
  case PROP_FILTER_TYPE:
    g_value_set_uint(value, filter->filterType);
    break;
  case PROP_MARGIN:
    g_value_set_float(value, filter->margin);
    break;
  case PROP_ACTIVE:
    g_value_set_boolean(value, filter->active);
    break;
  case PROP_INVERT:
    g_value_set_boolean(value, filter->invert);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

// Prepare to start proceassing -> open ressources, etc.
static gboolean gst_obstruct_start(GstObstruct *filter)
{
  gboolean ret = TRUE;

  if (g_hash_table_size(filter->labels) == 0)
  {
    GST_ELEMENT_ERROR(filter, RESOURCE, NOT_FOUND, ("Labels have not been set"), (NULL));
    ret = FALSE;
    goto out;
  }

out:
  return ret;
}

// Stop processing -> free ressources
static gboolean gst_obstruct_stop(GstObstruct *filter)
{
  return TRUE;
}

// Handle incoming buffer
static GstFlowReturn gst_obstruct_transform_buffer(GstBaseTransform *base, GstBuffer *buffer)
{
  GstObstruct *filter = GST_OBSTRUCT(base);
  GstDetectionMeta *detectionMeta;
  GstVideoMeta *videoMeta;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT(filter, "Received buffer");

  if (!filter->active)
  {
    GST_DEBUG_OBJECT(filter, "Filter disabled, no processing");
    goto out;
  }

  detectionMeta = GST_DETECTION_META_GET(buffer);
  if (detectionMeta == NULL)
  {
    // Ignore buffer
    GST_ERROR_OBJECT(filter, "No object detection meta on the buffer");
    goto out;
  }

  videoMeta = (GstVideoMeta *)gst_buffer_get_meta(buffer, GST_VIDEO_META_API_TYPE);
  if (videoMeta == NULL)
  {
    // Ignore buffer
    GST_ERROR_OBJECT(filter, "No video meta on the buffer");
    goto out;
  }

  GstMapInfo info;
  gst_buffer_map(buffer, &info, GST_MAP_READWRITE);

  for (gint i = 0; i < detectionMeta->detections->len; i++)
  {
    GstDetection *detection = g_ptr_array_index(detectionMeta->detections, i);

    GList *labels = g_hash_table_get_values(filter->labels);
    gboolean match = FALSE;
    while(labels != NULL)
    {
      match = g_str_has_prefix(detection->label, labels->data);
      if (match)
        break;

      labels = labels->next;
    }

    if (!filter->invert && !match || filter->invert && match)
      continue;

    GST_DEBUG_OBJECT(filter, "Found detection to process: %s", detection->label);

    // Apply filter on detection
    if (!apply_filter(videoMeta, info.data, detection, filter->filterType, filter->margin))
    {
      GST_ERROR_OBJECT(filter, "Error applying filter");
      ret = GST_FLOW_ERROR;
      goto unmap;
    }
  }

unmap:
  gst_buffer_unmap(buffer, &info);

out:
  return ret;
}

// Object constructor -> called for every instance
static void gst_obstruct_init(GstObstruct *filter)
{
  // Set property initial value
  filter->labels = g_hash_table_new_full(g_str_hash,  // Hash function
                                         g_str_equal, // Comparator
                                         g_free,      // Key & value destructor (as we only use the _add method)
                                         NULL);       // Value will be destructed by the key destructor

  filter->filterType = FILTER_TYPE_BLUR;
  filter->active = TRUE;
  filter->invert = FALSE;
}

// Object destructor -> called if an object gets destroyed
static void gst_obstruct_finalize(GObject *object)
{
  GstObstruct *filter = GST_OBSTRUCT(object);

  g_hash_table_destroy(filter->labels);

  G_OBJECT_CLASS(gst_obstruct_parent_class)->finalize(object);
}

// Class constructor -> called exactly once
static void gst_obstruct_class_init(GstObstructClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;
  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;
  gstbasetransform_class = (GstBaseTransformClass *)klass;

  // Set function pointers on this class
  gobject_class->finalize = gst_obstruct_finalize;
  gobject_class->set_property = gst_obstruct_set_property;
  gobject_class->get_property = gst_obstruct_get_property;

  // Define properties of plugin
  g_object_class_install_property(gobject_class, PROP_LABELS,
                                  gst_param_spec_array("labels", "Labels",
                                                       "List of the labels that should be processed",
                                                       g_param_spec_string("label",
                                                                           "Label",
                                                                           "A label that should be processed",
                                                                           NULL,
                                                                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_ACTIVE,
                                  g_param_spec_boolean("active", "Active",
                                                       "Whether preprocessing is active or not",
                                                       TRUE,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_INVERT,
                                  g_param_spec_boolean("invert", "Invert",
                                                       "If true, all labels that are not listed are occluded",
                                                       FALSE,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_FILTER_TYPE,
                                  g_param_spec_uint("filter-type", "Filter Type",
                                                    "Which type of filtering should be applied",
                                                    0, UINT_MAX,
                                                    FILTER_TYPE_BLUR,
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_MARGIN,
                                  g_param_spec_float("margin", "Margin",
                                                     "How much margin around the detection should be masked too",
                                                     -FLT_MAX, FLT_MAX,
                                                     0,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  // Set plugin metadata
  gst_element_class_set_static_metadata(gstelement_class,
                                        "Processor filter", "Filter/Video",
                                        "Processor filter", "Maurice Ackel <m.ackel@icloud.com>");

  // Set the pad templates
  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&obstruct_src_template));
  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&obstruct_sink_template));

  // Set function pointers to implementation of base class
  gstbasetransform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_obstruct_transform_buffer);
}

// Plugin initializer that registers the plugin
static gboolean plugin_init(GstPlugin *plugin)
{
  gboolean ret;

  GST_DEBUG_CATEGORY_INIT(gst_obstruct_debug, "obstruct", 0, "obstruct debug");

  ret = gst_element_register(plugin, "obstruct", GST_RANK_NONE, GST_TYPE_OBSTRUCT);

  return ret;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  obstruct,
                  "Detection obstruction plugin",
                  plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
