/**
 * SECTION:element-screencapsrc
 * @title: screencapsrc
 * @short_description: Screen and window capture source
 *
 * This element captures a desktop or a selected application window and creates raw RGB video.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 screencapsrc window-id=1234 ! video/x-raw,framerate=5/1 ! videoconvert ! autovideosink
 * ]| Displays the window with given id.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstscreencapsrc.h"
#include "screencaputil.h"

#include <string.h>
#include <stdlib.h>

#include <gst/gst.h>
#include <gst/video/video.h>

GST_DEBUG_CATEGORY(gst_screen_cap_src_debug);

enum
{
  PROP_0,
  PROP_SOURCE_ID,
  PROP_SOURCE_TYPE,
  PROP_REPORT_LOCATIONS,
};

static GstStaticPadTemplate video_src_template =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("video/x-raw, "
                                            "format = BGRA, "
                                            "framerate = (fraction) [ 0, MAX ], "
                                            "width = (int) [ 1, MAX ], "
                                            "height = (int) [ 1, MAX ]"));

#define gst_screen_cap_src_parent_class parent_class
G_DEFINE_TYPE(GstScreenCapSrc, gst_screen_cap_src, GST_TYPE_PUSH_SRC);

// Property setter
static void gst_screen_cap_src_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(object);

  switch (prop_id)
  {
  case PROP_SOURCE_ID:
    src->sourceId = g_value_get_uint64(value);
    break;
  case PROP_SOURCE_TYPE:
    src->sourceType = g_value_get_uint(value);
    break;
  case PROP_REPORT_LOCATIONS:
    src->reportWindowLocations = g_value_get_boolean(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

// Property getter
static void gst_screen_cap_src_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(object);

  switch (prop_id)
  {
  case PROP_SOURCE_ID:
    g_value_set_uint64(value, src->sourceId);
    break;
  case PROP_SOURCE_TYPE:
    g_value_set_uint(value, src->sourceType);
    break;
  case PROP_REPORT_LOCATIONS:
    g_value_set_boolean(value, src->reportWindowLocations);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

// Prepare to start capturing -> open ressources, etc.
static gboolean gst_screen_cap_src_start(GstBaseSrc *parent)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(parent);

  // Init capture
  gst_screen_cap_util_initialize(src->captureUtil, src);

  // Get dimensions for capture
  gint width, height;
  gboolean success = gst_screen_cap_util_get_dimensions(src->captureUtil, src, &width, &height, NULL);
  src->caps = gst_caps_new_simple("video/x-raw",
                                  "format", G_TYPE_STRING, "BGRA",
                                  "width", G_TYPE_INT, width,
                                  "height", G_TYPE_INT, height,
                                  "framerate", GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1, NULL);

  src->initialized = success;
  return success;
}

// Stop processing -> free ressources
static gboolean gst_screen_cap_src_stop(GstBaseSrc *parent)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(parent);

  if (src->caps)
  {
    gst_caps_unref(src->caps);
    src->caps = NULL;
  }

  src->initialized = FALSE;

  // Finalize capture
  gst_screen_cap_util_initialize(src->captureUtil, src);

  return TRUE;
}

// Intercept outgoing events
static gboolean gst_screen_cap_src_send_event(GstElement *parent, GstEvent *event)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(parent);
  gboolean ret;

  switch (GST_EVENT_TYPE(event))
  {
  default:
    ret = GST_ELEMENT_CLASS(parent_class)->send_event(parent, event);
    break;
  }

  return ret;
}

// Handler for incoming queries
static gboolean gst_screen_cap_src_query(GstBaseSrc *parent, GstQuery *query)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(parent);
  gboolean ret;

  switch (GST_QUERY_TYPE(query))
  {
  case GST_QUERY_CUSTOM:
  {
    const GstStructure *payload = gst_query_get_structure(query); // No need to free
    const gchar *messageName = gst_structure_get_name(payload);

    // Enable/Disable window reporting
    if (g_str_equal(messageName, "report_window_locations"))
    {
      gboolean active;
      if (gst_structure_get_boolean(payload, "active", &active))
      {
        GST_DEBUG_OBJECT(src, "Received 'report_window_locations' query. Setting reporting state to %i", active);
        src->reportWindowLocations = active;
      }
    }
  }; // No break to handle query in default way;
  default:
    ret = GST_BASE_SRC_CLASS(parent_class)->query(parent, query);
    break;
  }

  return ret;
}

// Handler for incoming events
static gboolean gst_screen_cap_src_event(GstBaseSrc *parent, GstEvent *event)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(parent);
  gboolean ret;

  switch (GST_EVENT_TYPE(event))
  {
  default:
    ret = GST_BASE_SRC_CLASS(parent_class)->event(parent, event);
    break;
  }

  return ret;
}

// Get capabilities of this source
static GstCaps *gst_screen_cap_src_get_caps(GstBaseSrc *bsrc, GstCaps *filter)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(bsrc);
  GstCaps *caps;

  // If we are not initialized, we can only return the template capabilities
  if (!src->initialized)
  {
    caps = gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(bsrc));
    goto done;
  }

  // We are capturing so we report the actual dimensions, etc.
  caps = gst_caps_ref(src->caps);

done:
  GST_DEBUG_OBJECT(src, "get_caps returning %" GST_PTR_FORMAT, caps);
  return caps;
}

// Set this elements capabilities
static gboolean gst_screen_cap_src_set_caps(GstBaseSrc *bsrc, GstCaps *caps)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(bsrc);
  GstStructure *structure;
  GstVideoInfo info;

  // If not yet initialized, disallow setcaps until later
  if (!src->initialized)
    return FALSE;

  structure = gst_caps_get_structure(caps, 0);
  if (gst_structure_has_name(structure, "video/x-raw"))
  {
    if (!gst_video_info_from_caps(&info, caps))
      goto parse_failed;
  }
  else
  {
    goto unsupported_caps;
  }

  src->info = info;
  gst_caps_replace(&src->caps, caps);
  GST_DEBUG_OBJECT(src, "New caps set: %p, %" GST_PTR_FORMAT, caps, caps);

  return TRUE;

parse_failed:
  GST_DEBUG_OBJECT(src, "Failed to parse caps");
  return FALSE;

unsupported_caps:
  GST_DEBUG_OBJECT(src, "Unsupported caps: %" GST_PTR_FORMAT, caps);
  return FALSE;
}

// Called during caps negotiation if caps need fixating
static GstCaps *gst_screen_cap_src_fixate(GstBaseSrc *bsrc, GstCaps *caps)
{
  gint i;
  GstStructure *structure;

  caps = gst_caps_make_writable(caps);

  for (i = 0; i < gst_caps_get_size(caps); i++)
  {
    structure = gst_caps_get_structure(caps, i);

    gst_structure_fixate_field_nearest_fraction(structure, "framerate", 25, 1);
  }

  caps = GST_BASE_SRC_CLASS(parent_class)->fixate(bsrc, caps);

  return caps;
}

// Decide on the bufferpool allocation
static gboolean gst_screen_cap_src_decide_allocation(GstBaseSrc *bsrc, GstQuery *query)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(bsrc);
  GstBufferPool *pool;
  guint size, minBuffers, maxBuffers;
  GstStructure *config;
  GstCaps *caps = NULL;
  gboolean ret = TRUE;

  gboolean update = gst_query_get_n_allocation_pools(query) > 0 ? TRUE : FALSE;

  if (update)
  {
    // Received configuration from peer
    gst_query_parse_nth_allocation_pool(query, 0, &pool, &size, &minBuffers, &maxBuffers);
  }
  else
  {
    // Need to set own configuration
    pool = NULL;

    // Get caps from query
    gst_query_parse_allocation(query, &caps, NULL);

    // Get dimensions from caps
    GstStructure *capsStructure = gst_caps_get_structure(caps, 0);
    gint width, height;

    gboolean res = gst_structure_get_int(capsStructure, "width", &width);
    res |= gst_structure_get_int(capsStructure, "height", &height);
    if (!res)
    {
      GST_DEBUG_OBJECT(src, "Unable to get dimensions from caps %" GST_PTR_FORMAT, caps);
    }

    size = BYTES_PER_PIXEL * width * height;
    minBuffers = 0; // No lower limit
    maxBuffers = 0; // Unlimited buffers = 0
  }

  if (pool == NULL)
  {
    // No pool from downstream, we need to create one ourselfs
    // Old pool in BaseSrc will be automatically deactivated
    GST_DEBUG_OBJECT(src, "Creating new bufferpool");
    pool = gst_video_buffer_pool_new();
  }

  // Configure buffer pool
  config = gst_buffer_pool_get_config(pool);
  gst_buffer_pool_config_set_params(config, caps, size, minBuffers, maxBuffers);
  gst_buffer_pool_config_add_option(config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_set_config(pool, config);

  // Set pool on query
  if (update)
  {
    gst_query_set_nth_allocation_pool(query, 0, pool, size, minBuffers, maxBuffers);
  }
  else
  {
    gst_query_add_allocation_pool(query, pool, size, minBuffers, maxBuffers);
  }

  // Call parent allocation handler
  ret = GST_BASE_SRC_CLASS(parent_class)->decide_allocation(bsrc, query);

  // Activate new bufferpool
  if (ret && !gst_buffer_pool_set_active(pool, TRUE))
  {
    GST_ELEMENT_ERROR(src, RESOURCE, SETTINGS, ("Failed to allocate required memory."), ("Buffer pool activation failed"));
    ret = FALSE;
  }

out:
  if (pool)
  {
    gst_object_unref(pool);
  }

  return ret;
}

// Main capturing event -> creates a new buffer
static GstFlowReturn gst_screen_cap_src_create(GstPushSrc *parent, GstBuffer **buffer)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(parent);
  GstClockTime baseTime, nextFrameTime;

  if (!src->lastFrameTime)
  {
    src->lastFrameTime = gst_clock_get_time(GST_ELEMENT_CLOCK(src));
    goto create_buffer;
  }

  // Determine next frame time from last frame time
  baseTime = GST_ELEMENT_CAST(src)->base_time;
  nextFrameTime = src->lastFrameTime + src->info.fps_d / src->info.fps_n * GST_SECOND;

  // Block until next frame time arrives to prevent unnecessary captures
  GstClockID clockId = gst_clock_new_single_shot_id(GST_ELEMENT_CLOCK(src), nextFrameTime);
  GST_TRACE_OBJECT(src, "Waiting for next frame time %" G_GUINT64_FORMAT, nextFrameTime);
  GstClockReturn ret = gst_clock_id_wait(clockId, NULL);
  GST_TRACE_OBJECT(src, "Done waiting for frame time %" G_GUINT64_FORMAT, nextFrameTime);
  gst_clock_id_unref(clockId);

  src->lastFrameTime = nextFrameTime;

create_buffer:
  return gst_screen_cap_util_create_buffer(src->captureUtil, src, buffer);
}

// Object constructor -> called for every instance
static void gst_screen_cap_src_init(GstScreenCapSrc *src)
{
  gst_base_src_set_format(GST_BASE_SRC(src), GST_FORMAT_TIME);
  gst_base_src_set_live(GST_BASE_SRC(src), TRUE);
  gst_base_src_set_do_timestamp(GST_BASE_SRC(src), TRUE);

  src->sourceId = 0;
  src->sourceType = SOURCE_TYPE_DISPLAY;
  src->reportWindowLocations = FALSE;

  // Will be initialized in the start function
  src->initialized = FALSE;

  // Create capture util
  src->captureUtil = gst_screen_cap_util_new();
}

// Object destructor -> called if an object gets destroyed
static void gst_screen_cap_src_finalize(GObject *object)
{
  GstScreenCapSrc *src = GST_SCREEN_CAP_SRC(object);

  // If necessary, members of the object can be freed here
  g_object_unref(src->captureUtil);

  G_OBJECT_CLASS(gst_screen_cap_src_parent_class)->finalize(object);
}

// Class constructor -> called exactly once
static void gst_screen_cap_src_class_init(GstScreenCapSrcClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *basesrc_class;
  GstPushSrcClass *pushsrc_class;
  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;
  basesrc_class = (GstBaseSrcClass *)klass;
  pushsrc_class = (GstPushSrcClass *)klass;

  // Set function pointers on this class
  gobject_class->finalize = gst_screen_cap_src_finalize;
  gobject_class->set_property = gst_screen_cap_src_set_property;
  gobject_class->get_property = gst_screen_cap_src_get_property;

  // Define properties of plugin
  g_object_class_install_property(gobject_class, PROP_SOURCE_ID,
                                  g_param_spec_uint64("source-id", "Source ID",
                                                      "The ID of the source (i.e. window/display) that should be captured", 0,
                                                      G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_SOURCE_TYPE,
                                  g_param_spec_uint("source-type", "Source Type",
                                                    "The type of the source that should be captured", 0,
                                                    G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_REPORT_LOCATIONS,
                                  g_param_spec_boolean("report-locations", "Report Locations",
                                                       "If true, metadata with all window locations at capture time is added",
                                                       FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  // Set plugin metadata
  gst_element_class_set_static_metadata(gstelement_class,
                                        "Mac screen capture source", "Source/Video",
                                        "Mac screen capture source", "Maurice Ackel <m.ackel@icloud.com>");

  // Set the pad template
  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&video_src_template));

  // Set function pointers to implementation of base class
  basesrc_class->start = GST_DEBUG_FUNCPTR(gst_screen_cap_src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR(gst_screen_cap_src_stop);
  basesrc_class->get_caps = GST_DEBUG_FUNCPTR(gst_screen_cap_src_get_caps);
  basesrc_class->set_caps = GST_DEBUG_FUNCPTR(gst_screen_cap_src_set_caps);
  basesrc_class->fixate = GST_DEBUG_FUNCPTR(gst_screen_cap_src_fixate);
  basesrc_class->event = GST_DEBUG_FUNCPTR(gst_screen_cap_src_event);
  basesrc_class->query = GST_DEBUG_FUNCPTR(gst_screen_cap_src_query);
  basesrc_class->decide_allocation = GST_DEBUG_FUNCPTR(gst_screen_cap_src_decide_allocation);
  gstelement_class->send_event = GST_DEBUG_FUNCPTR(gst_screen_cap_src_send_event);
  pushsrc_class->create = GST_DEBUG_FUNCPTR(gst_screen_cap_src_create);
}

// Plugin initializer that registers the plugin
static gboolean plugin_init(GstPlugin *plugin)
{
  gboolean ret;

  GST_DEBUG_CATEGORY_INIT(gst_screen_cap_src_debug, "screencapsrc", 0, "screencapsrc debug");

  ret = gst_element_register(plugin, "screencapsrc", GST_RANK_NONE, GST_TYPE_SCREEN_CAP_SRC);

  return ret;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  screencapsrc,
                  "Mac screen capture video input plugin",
                  plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
