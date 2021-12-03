#pragma once

typedef struct _GstScreenCapUtil GstScreenCapUtil;
typedef struct _GstScreenCapUtilPrivate GstScreenCapUtilPrivate;
typedef struct _GstScreenCapUtilClass GstScreenCapUtilClass;

#include "gstscreencapsrc.h"
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_SCREEN_CAP_UTIL (gst_screen_cap_util_get_type())
#define GST_SCREEN_CAP_UTIL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_SCREEN_CAP_UTIL, GstScreenCapUtil))
#define GST_SCREEN_CAP_UTIL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_SCREEN_CAP_UTIL, GstScreenCapUtil))
#define GST_IS_SCREEN_CAP_UTIL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_SCREEN_CAP_UTIL))
#define GST_IS_SCREEN_CAP_UTIL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_SCREEN_CAP_UTIL))

GType gst_screen_cap_util_get_type(void) G_GNUC_CONST;

struct _GstScreenCapUtil
{
    GObject parent;

    gboolean initialized;

    GstScreenCapUtilPrivate *priv;
};

struct _GstScreenCapUtilClass
{
    GObjectClass parent_class;
};

G_END_DECLS

void gst_screen_cap_util_initialize(GstScreenCapUtil *self, GstScreenCapSrc *src);
void gst_screen_cap_util_finalize(GstScreenCapUtil *self);
GstFlowReturn gst_screen_cap_util_create_buffer(GstScreenCapUtil *self, GstScreenCapSrc *src, GstBuffer **buffer);
gboolean gst_screen_cap_util_get_dimensions(GstScreenCapUtil *self, GstScreenCapSrc *src, gint *width, gint *height, size_t *size);
GstScreenCapUtil *gst_screen_cap_util_new();
