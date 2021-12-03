#pragma once

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_REGIONS (gst_regions_get_type())
#define GST_REGIONS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_REGIONS, GstRegions))
#define GST_REGIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_REGIONS, GstRegions))
#define GST_IS_REGIONS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_REGIONS))
#define GST_IS_REGIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_REGIONS))

typedef struct _GstRegions GstRegions;
typedef struct _GstRegionsClass GstRegionsClass;

GType gst_regions_get_type(void) G_GNUC_CONST;

struct _GstRegions
{
  GstBaseTransform parent;

  GArray *regions;
  const char *prefix;
  gboolean active;
  GPtrArray *labels;
};

struct _GstRegionsClass
{
  GstBaseTransformClass parent_class;
};

G_END_DECLS
