/**
 * SECTION:element-winanalyzer
 * @title: winanalyzer
 * @short_description: element for detecting windows
 *
 * This element detects windows on screen.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 videotestsrc ! winanalyzer prefix="example" display-id=12424 ! obstruct labels="<example:winname1>" ! autovideosink
 * ]| Blocks specified window in the testvideosrc.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstwinanalyzer.h"
#include "winanalyzerutils.h"
#include <detectionmeta.h>
#include <windowlocationsmeta.h>
#include <math.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY(gst_win_analyzer_debug);

enum
{
  PROP_0,
  PROP_PREFIX,
  PROP_ACTIVE,
  PROP_DISPLAY_ID,
  PROP_LABELS,
};

GstStaticPadTemplate win_analyzer_src_template = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

GstStaticPadTemplate win_analyzer_sink_template = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

#define gst_win_analyzer_parent_class parent_class
G_DEFINE_TYPE(GstWinAnalyzer, gst_win_analyzer, GST_TYPE_BASE_TRANSFORM);

// Property setter
static void gst_win_analyzer_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GstWinAnalyzer *filter = GST_WIN_ANALYZER(object);

  switch (prop_id)
  {
  case PROP_DISPLAY_ID:
  {
    filter->displayId = g_value_get_uint64(value);
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
static void gst_win_analyzer_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GstWinAnalyzer *filter = GST_WIN_ANALYZER(object);

  switch (prop_id)
  {
  case PROP_DISPLAY_ID:
    g_value_set_uint64(value, filter->displayId);
    break;
  case PROP_PREFIX:
    g_value_set_string(value, filter->prefix);
    break;
  case PROP_ACTIVE:
    g_value_set_boolean(value, filter->active);
    break;
  case PROP_LABELS:
  {
    GArray *winInfos = g_array_new(FALSE, FALSE, sizeof(WindowInfo));
    g_array_set_clear_func(winInfos, window_info_clear);
    get_windows(filter->displayId, winInfos, FALSE);

    GHashTable *labelSet = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    for (gint i = 0; i < winInfos->len; i++)
    {
      WindowInfo *winInfo = &g_array_index(winInfos, WindowInfo, i);
      GString *label;

      // Add detection with full label
      label = g_string_new("");
      g_string_append_printf(label, "%s/%s", winInfo->ownerName, winInfo->windowName);
      g_hash_table_add(labelSet, g_strdup(label->str));
      g_string_free(label, TRUE);

      // Add detection with only owner
      g_hash_table_add(labelSet, g_strdup(winInfo->ownerName));
    }

    GList *labels = g_hash_table_get_values(labelSet);
    for (; labels != NULL; labels = labels->next)
    {
      GValue val = G_VALUE_INIT;
      g_value_init(&val, G_TYPE_STRING);
      g_value_set_string(&val, labels->data);

      gst_value_array_append_value(value, &val);
      g_value_unset(&val);
    }

    g_hash_table_destroy(labelSet);
  }
  break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

// Prepare to start proceassing -> open ressources, etc.
static gboolean gst_win_analyzer_start(GstBaseTransform *base)
{
  GstWinAnalyzer *filter = GST_WIN_ANALYZER(base);

  GstStructure *payload = gst_structure_new_from_string("report_window_locations, active=true");
  GstQuery *query = gst_query_new_custom(GST_QUERY_CUSTOM, payload);

  GstPad *sinkPad = GST_BASE_TRANSFORM_SINK_PAD(base);
  gst_pad_peer_query(sinkPad, query);
  gst_query_unref(query);

  return TRUE;
}

// Stop processing -> free ressources
static gboolean gst_win_analyzer_stop(GstBaseTransform *base)
{
  GstWinAnalyzer *filter = GST_WIN_ANALYZER(base);

  GstStructure *payload = gst_structure_new_from_string("report_window_locations, active=false");
  GstQuery *query = gst_query_new_custom(GST_QUERY_CUSTOM, payload);

  GstPad *sinkPad = GST_BASE_TRANSFORM_SINK_PAD(base);
  gst_pad_peer_query(sinkPad, query);
  gst_query_unref(query);

  return TRUE;
}

// Handle incoming buffer
static GstFlowReturn gst_win_analyzer_transform_buffer(GstBaseTransform *base, GstBuffer *buffer)
{
  GstWinAnalyzer *filter = GST_WIN_ANALYZER(base);
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

  // Get windows from meta if possible
  GArray *winInfos = g_array_new(FALSE, FALSE, sizeof(WindowInfo));
  g_array_set_clear_func(winInfos, window_info_clear);

  GstWindowLocationsMeta *windowLocationsMeta = GST_WINDOW_LOCATIONS_META_GET(buffer);
  if (windowLocationsMeta)
  {
    // Copy for local usage
    WindowInfo newInfo;
    for (gint i = 0; i < windowLocationsMeta->windowInfos->len; i++)
    {
      window_info_copy(&g_array_index(windowLocationsMeta->windowInfos, WindowInfo, i), &newInfo);
      g_array_append_val(winInfos, newInfo);
    }
  }
  else
  {
    // No metadata, capture now
    get_windows(filter->displayId, winInfos, TRUE);
  }

  // Sort by zIndex from front to back (i.e. descending)
  g_array_sort(winInfos, (GCompareFunc)sort_window_info_zindex);

  for (guint i = 0; i < winInfos->len; i++)
  {
    WindowInfo *winInfo = &g_array_index(winInfos, WindowInfo, i);

    GArray *resultingBoundingBoxes = g_array_new(FALSE, FALSE, sizeof(BoundingBox));
    g_array_append_val(resultingBoundingBoxes, winInfo->bbox);

    // For each window that has a higher zorder, compute overlap
    // Start from element and go towards front
    for (gint j = i - 1; j >= 0; j--)
    {
      WindowInfo *higherWinInfo = &g_array_index(winInfos, WindowInfo, j);

      if (higherWinInfo->zIndex <= winInfo->zIndex)
        continue;

      GArray *result = gst_bounding_box_subtract_from_boxes(resultingBoundingBoxes, &higherWinInfo->bbox);
      g_array_free(resultingBoundingBoxes, TRUE);
      resultingBoundingBoxes = result;
    }

    // Preprocessing finished, now produce detections

    GString *fullLabel;
    GstDetection *detection;

    // Add detection with full label
    fullLabel = g_string_new(filter->prefix);
    g_string_append_printf(fullLabel, ":%s/%s", winInfo->ownerName, winInfo->windowName);

    // Create a detection for each resulting bounding box to be able to represent complex overlapping shapes
    for (gint k = 0; k < resultingBoundingBoxes->len; k++)
    {
      BoundingBox bbox = g_array_index(resultingBoundingBoxes, BoundingBox, k);

      // Ignore bounding boxes that start completely outside the frame
      if (bbox.x > videoMeta->width || bbox.height > videoMeta->height)
        continue;

      // Clip bounding box
      bbox.x = MAX(bbox.x, 0);
      bbox.y = MAX(bbox.y, 0);
      bbox.width = MIN(bbox.width, videoMeta->width - bbox.x);
      bbox.height = MIN(bbox.height, videoMeta->height - bbox.y);

      detection = gst_detection_new(fullLabel->str, 1, bbox);
      g_ptr_array_add(detectionMeta->detections, detection);
    }

    // Free memory
    g_array_free(resultingBoundingBoxes, TRUE);
    g_string_free(fullLabel, TRUE);
  }

out:
  g_array_free(winInfos, TRUE);

  return ret;
}

// Object constructor -> called for every instance
static void gst_win_analyzer_init(GstWinAnalyzer *filter)
{
  // Set property initial value
  filter->prefix = NULL;
  filter->active = TRUE;
  filter->displayId = 0;

  filter->labels = g_ptr_array_new_with_free_func(g_free);
  g_ptr_array_add(filter->labels, g_strdup("regions"));
}

// Object destructor -> called if an object gets destroyed
static void gst_win_analyzer_finalize(GObject *object)
{
  GstWinAnalyzer *filter = GST_WIN_ANALYZER(object);

  g_free((void *)filter->prefix);
  g_ptr_array_free(filter->labels, TRUE);

  G_OBJECT_CLASS(gst_win_analyzer_parent_class)->finalize(object);
}

// Class constructor -> called exactly once
static void gst_win_analyzer_class_init(GstWinAnalyzerClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;
  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;
  gstbasetransform_class = (GstBaseTransformClass *)klass;

  // Set function pointers on this class
  gobject_class->finalize = gst_win_analyzer_finalize;
  gobject_class->set_property = gst_win_analyzer_set_property;
  gobject_class->get_property = gst_win_analyzer_get_property;

  // Define properties of plugin
  g_object_class_install_property(gobject_class, PROP_DISPLAY_ID,
                                  g_param_spec_uint64("display-id", "Display ID",
                                                      "The id of the display which should be monitored for windows",
                                                      0, G_MAXUINT64, 0,
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
                                        "Exclude filter", "Filter/Video",
                                        "Exclude filter", "Maurice Ackel <m.ackel@icloud.com>");

  // Set the pad templates
  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&win_analyzer_src_template));
  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&win_analyzer_sink_template));

  // Set function pointers to implementation of base class
  gstbasetransform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_win_analyzer_transform_buffer);
  gstbasetransform_class->start = GST_DEBUG_FUNCPTR(gst_win_analyzer_start);
  gstbasetransform_class->stop = GST_DEBUG_FUNCPTR(gst_win_analyzer_stop);
}

// Plugin initializer that registers the plugin
static gboolean plugin_init(GstPlugin *plugin)
{
  gboolean ret;

  GST_DEBUG_CATEGORY_INIT(gst_win_analyzer_debug, "winanalyzer", 0, "win_analyzer debug");

  ret = gst_element_register(plugin, "winanalyzer", GST_RANK_NONE, GST_TYPE_WIN_ANALYZER);

  return ret;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  winanalyzer,
                  "Window analyzer filter plugin",
                  plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
