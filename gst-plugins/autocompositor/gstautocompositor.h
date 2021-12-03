#pragma once

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_AUTO_COMPOSITOR (gst_auto_compositor_get_type())
#define GST_AUTO_COMPOSITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AUTO_COMPOSITOR, GstAutoCompositor))
#define GST_AUTO_COMPOSITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AUTO_COMPOSITOR, GstAutoCompositor))
#define GST_IS_AUTO_COMPOSITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AUTO_COMPOSITOR))
#define GST_IS_AUTO_COMPOSITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AUTO_COMPOSITOR))

typedef struct _GstAutoCompositor GstAutoCompositor;
typedef struct _GstAutoCompositorClass GstAutoCompositorClass;

GType gst_auto_compositor_get_type(void) G_GNUC_CONST;

struct _GstAutoCompositor
{
  GstBin parent;

  GstElement *compositor;
};

struct _GstAutoCompositorClass
{
  GstBinClass parent_class;
};

G_END_DECLS
