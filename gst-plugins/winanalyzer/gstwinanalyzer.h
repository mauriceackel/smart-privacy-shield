#pragma once

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_WIN_ANALYZER (gst_win_analyzer_get_type())
#define GST_WIN_ANALYZER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_WIN_ANALYZER, GstWinAnalyzer))
#define GST_WIN_ANALYZER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_WIN_ANALYZER, GstWinAnalyzer))
#define GST_IS_WIN_ANALYZER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_WIN_ANALYZER))
#define GST_IS_WIN_ANALYZER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_WIN_ANALYZER))

typedef struct _GstWinAnalyzer GstWinAnalyzer;
typedef struct _GstWinAnalyzerClass GstWinAnalyzerClass;

GType gst_win_analyzer_get_type(void) G_GNUC_CONST;

struct _GstWinAnalyzer
{
  GstBaseTransform parent;

  guint64 displayId;
  const char *prefix;
  gboolean active;
  GPtrArray *labels;
};

struct _GstWinAnalyzerClass
{
  GstBaseTransformClass parent_class;
};

G_END_DECLS
