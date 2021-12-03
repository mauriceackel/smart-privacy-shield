#pragma once

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "obstruct.h"

G_BEGIN_DECLS

#define GST_TYPE_OBSTRUCT (gst_obstruct_get_type())
#define GST_OBSTRUCT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_OBSTRUCT, GstObstruct))
#define GST_OBSTRUCT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_OBSTRUCT, GstObstruct))
#define GST_IS_OBSTRUCT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_OBSTRUCT))
#define GST_IS_OBSTRUCT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_OBSTRUCT))

typedef struct _GstObstruct GstObstruct;
typedef struct _GstObstructClass GstObstructClass;

GType gst_obstruct_get_type(void) G_GNUC_CONST;

struct _GstObstruct
{
  GstBaseTransform parent;

  GHashTable *labels;
  FilterType filterType;
  gfloat margin;
  gboolean invert;
  gboolean active;
};

struct _GstObstructClass
{
  GstBaseTransformClass parent_class;
};

G_END_DECLS
