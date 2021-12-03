#pragma once

typedef struct _GstScreenCapSrc GstScreenCapSrc;
typedef struct _GstScreenCapSrcClass GstScreenCapSrcClass;

#include <sps/utils/spssources.h>
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>
#include "screencaputil.h"

G_BEGIN_DECLS

#define BYTES_PER_PIXEL 4 // BGRA

#define GST_TYPE_SCREEN_CAP_SRC (gst_screen_cap_src_get_type())
#define GST_SCREEN_CAP_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_SCREEN_CAP_SRC, GstScreenCapSrc))
#define GST_SCREEN_CAP_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_SCREEN_CAP_SRC, GstScreenCapSrc))
#define GST_IS_SCREEN_CAP_SRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_SCREEN_CAP_SRC))
#define GST_IS_SCREEN_CAP_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_SCREEN_CAP_SRC))

GType gst_screen_cap_src_get_type(void) G_GNUC_CONST;

struct _GstScreenCapSrc
{
  GstPushSrc parent;

  GstCaps *caps;

  gboolean initialized;
  guintptr sourceId;
  SpsSourceType sourceType;
  gboolean reportWindowLocations;

  GstVideoInfo info;

  GstClockTime lastFrameTime;

  GstScreenCapUtil *captureUtil;
};

struct _GstScreenCapSrcClass
{
  GstPushSrcClass parent_class;
};

G_END_DECLS
