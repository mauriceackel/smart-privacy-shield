#include <gst/gst.h>
#include <detectionmeta.h>
#include <windowlocationsmeta.h>
#include "gstwinanalyzer.h"

void get_all_windows(GstWinAnalyzer *filter, GArray *winInfo);

void get_visible_windows(GstWinAnalyzer *filter, GArray *winInfo);

gint sort_window_info_zindex(WindowInfo *a, WindowInfo *b);
