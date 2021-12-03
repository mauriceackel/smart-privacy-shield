#include <sps/utils/spssources.h>

void sps_source_info_clear(gpointer pointer)
{
    SpsSourceInfo *sInfo = (SpsSourceInfo *)pointer;
    g_free((void *)sInfo->name);
    g_free((void *)sInfo->internalName);
}

void sps_source_info_copy(SpsSourceInfo *in, SpsSourceInfo *out)
{
    out->name = g_strdup(in->name);
    out->internalName = g_strdup(in->internalName);
    out->id = in->id;
    out->type = in->type;
}

gboolean sps_sources_window_id_exists(guintptr windowId)
{
    GArray *sourceInfos = g_array_new(FALSE, FALSE, sizeof(SpsSourceInfo));
    g_array_set_clear_func(sourceInfos, sps_source_info_clear);
    sps_sources_windows_list(sourceInfos);

    gboolean exists = FALSE;
    for (gint i = 0; i < sourceInfos->len; i++)
    {
        SpsSourceInfo info = g_array_index(sourceInfos, SpsSourceInfo, i);
        if (info.id == windowId)
        {
            exists = TRUE;
            break;
        }
    }

    g_array_free(sourceInfos, TRUE);

    return exists;
}

gboolean sps_sources_display_id_exists(guintptr displayId)
{
    GArray *sourceInfos = g_array_new(FALSE, FALSE, sizeof(SpsSourceInfo));
    g_array_set_clear_func(sourceInfos, sps_source_info_clear);
    sps_sources_displays_list(sourceInfos);

    gboolean exists = FALSE;
    for (gint i = 0; i < sourceInfos->len; i++)
    {
        SpsSourceInfo info = g_array_index(sourceInfos, SpsSourceInfo, i);
        if (info.id == displayId)
        {
            exists = TRUE;
            break;
        }
    }

    g_array_free(sourceInfos, TRUE);

    return exists;
}
