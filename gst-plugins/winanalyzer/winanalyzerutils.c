#include "winanalyzerutils.h"

// Sort by zIndex descending (i.e. from front to back)
gint sort_window_info_zindex(WindowInfo *a, WindowInfo *b)
{
    return b->zIndex - a->zIndex;
}
