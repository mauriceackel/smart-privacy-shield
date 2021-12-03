#pragma once

#include <glib.h>

typedef enum _SpsSourceType SpsSourceType;
enum _SpsSourceType
{
    SOURCE_TYPE_WINDOW,
    SOURCE_TYPE_DISPLAY,
};

typedef struct _SpsSourceInfo SpsSourceInfo;
struct _SpsSourceInfo
{
    const char *name;
    const char *internalName;
    guintptr id;
    SpsSourceType type;
};

void sps_source_info_copy(SpsSourceInfo *in, SpsSourceInfo *out);
void sps_source_info_clear(gpointer pointer);

void sps_sources_windows_list(GArray *sourceInfo);
gboolean sps_sources_window_id_exists(guintptr id);
gboolean sps_sources_recover_window(SpsSourceInfo *sourceInfo);

void sps_sources_displays_list(GArray *sourceInfo);
gboolean sps_sources_display_id_exists(guintptr id);
gboolean sps_sources_recover_display(SpsSourceInfo *sourceInfo);

void sps_sources_list(GArray *sourceInfo);
