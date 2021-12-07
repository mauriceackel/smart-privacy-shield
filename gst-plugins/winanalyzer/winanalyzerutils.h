#include <gst/gst.h>
#include <detectionmeta.h>
#include <windowlocationsmeta.h>
#include "gstwinanalyzer.h"

void get_windows(guint64 displayId, GArray *winInfo, gboolean onlyVisible);

gint sort_window_info_zindex(WindowInfo *a, WindowInfo *b);
