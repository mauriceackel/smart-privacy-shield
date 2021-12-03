#include <glib.h>
#include <sps/utils/spssources.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

void sps_sources_windows_list(GArray *sourceInfo)
{
    Display *display = XOpenDisplay(NULL);
    Window rootWindowId = RootWindow(display, DefaultScreen(display));
    Window rootWindowIdReturn, parentWindowIdReturn, *windowIds;
    guint windowCount;

    // Get all X windows
    XQueryTree(display, rootWindowId, &rootWindowIdReturn, &parentWindowIdReturn, &windowIds, &windowCount);

    // Map raw data to window info
    for (guint i = 0; i < windowCount; i++)
    {
        Window windowId = windowIds[i];
        char *windowName;
        XFetchName(display, windowId, &windowName);

        // Get window attributes for filtering
        XWindowAttributes windowAttributes;
        XGetWindowAttributes(display, windowId, &windowAttributes);

        // Only include windows that are viewable
        if (windowAttributes.map_state != 2)
            continue;

        // Check if window has children
        Window r, p, *childWindowIds;
        guint childCount;
        XQueryTree(display, windowId, &r, &p, &childWindowIds, &childCount);

        GString *displayName = g_string_new("");

        // If there already is a window name or n children we could query, directly return name
        if (windowName || childCount == 0)
        {
            g_string_append_printf(displayName, "[Application] %s", (windowName != NULL && strlen(windowName) != 0) ? windowName : "Unknown");
        }
        else
        {
            // Try to get the name from a child window
            char *childName;

            // Iterate over children to check if a child has a useful name
            for (gint j = 0; j < childCount; j++)
            {
                Window childWindowId = childWindowIds[j];
                XFetchName(display, childWindowId, &childName);

                // Ignore nameless children
                if (childName != NULL)
                {
                    g_string_append_printf(displayName, "[Application] %s", childName);
                    XFree(childName);
                    break;
                }
            }
        }

        // Add the actual source info
        SpsSourceInfo sInfo = {
            .name = g_strdup(displayName->str),
            .internalName = g_strdup(displayName->str),
            .id = (guintptr)windowId,
            .type = SOURCE_TYPE_WINDOW,
        };

        g_array_append_val(sourceInfo, sInfo);

        // Free memory
        g_string_free(displayName, TRUE);
        XFree(childWindowIds);
        XFree(windowName);
    }

    // Free memory
    XFree(windowIds);
    XCloseDisplay(display);
}

gboolean sps_sources_recover_window(SpsSourceInfo *sourceInfo)
{
    if (sourceInfo->type != SOURCE_TYPE_WINDOW)
        return FALSE;

    gboolean matchFound = FALSE;

    // Get all current windows
    GArray *sourceInfos = g_array_new(FALSE, FALSE, sizeof(SpsSourceInfo));
    g_array_set_clear_func(sourceInfos, sps_source_info_clear);
    sps_sources_windows_list(sourceInfos);

    // Check if window id exists in list
    for (gint i = 0; i < sourceInfos->len; i++)
    {
        SpsSourceInfo info = g_array_index(sourceInfos, SpsSourceInfo, i);
        if (info.id == sourceInfo->id)
        {
            matchFound = TRUE;
            sourceInfo->name = g_strdup(info.name);
            sourceInfo->internalName = g_strdup(info.internalName);
            // Early return if we found the window id
            goto out;
        }
    }

    matchFound = FALSE;

    // Try to recover window id from name
    for (gint i = 0; i < sourceInfos->len; i++)
    {
        SpsSourceInfo info = g_array_index(sourceInfos, SpsSourceInfo, i);

        if (strcmp(sourceInfo->internalName, info.internalName) == 0)
        {
            // Match found, store new id
            sourceInfo->id = info.id;
            sourceInfo->name = g_strdup(info.name);
            sourceInfo->internalName = g_strdup(info.internalName);
            matchFound = TRUE;
            goto out;
        }
    }

out:
    g_array_free(sourceInfos, TRUE);

    return matchFound;
}

void sps_sources_displays_list(GArray *sourceInfo)
{
    Display *display = XOpenDisplay(NULL);
    Screen *screen;

    gint screenCount = XScreenCount(display);
    for (gint i = 0; i < screenCount; i++)
    {
        screen = XScreenOfDisplay(display, i);
        Window windowId = XRootWindow(display, i);

        GString *displayName = g_string_new("");
        g_string_append_printf(displayName, "[Screen] %i x %i", screen->width, screen->height);

        SpsSourceInfo sInfo = {
            .name = g_strdup(displayName->str),
            .internalName = g_strdup(displayName->str),
            .id = (guintptr)windowId,
            .type = SOURCE_TYPE_DISPLAY,
        };

        g_array_append_val(sourceInfo, sInfo);

        g_string_free(displayName, TRUE);
    }

    XCloseDisplay(display);
}

gboolean sps_sources_recover_display(SpsSourceInfo *sourceInfo)
{
    if (sourceInfo->type != SOURCE_TYPE_DISPLAY)
        return FALSE;

    gboolean matchFound = FALSE;

    // Get all current displays
    GArray *sourceInfos = g_array_new(FALSE, FALSE, sizeof(SpsSourceInfo));
    g_array_set_clear_func(sourceInfos, sps_source_info_clear);
    sps_sources_displays_list(sourceInfos);

    // Check if display id exists in list
    for (gint i = 0; i < sourceInfos->len; i++)
    {
        SpsSourceInfo info = g_array_index(sourceInfos, SpsSourceInfo, i);
        if (info.id == sourceInfo->id)
        {
            matchFound = TRUE;
            sourceInfo->name = g_strdup(info.name);
            sourceInfo->internalName = g_strdup(info.internalName);
            // Early return if we found the display id
            goto out;
        }
    }

    matchFound = FALSE;

    // Try to recover display id from name
    for (gint i = 0; i < sourceInfos->len; i++)
    {
        SpsSourceInfo info = g_array_index(sourceInfos, SpsSourceInfo, i);

        if (strcmp(sourceInfo->internalName, info.internalName) == 0)
        {
            // Match found, store new id
            sourceInfo->id = info.id;
            sourceInfo->name = g_strdup(info.name);
            sourceInfo->internalName = g_strdup(info.internalName);
            matchFound = TRUE;
            goto out;
        }
    }

out:
    g_array_free(sourceInfos, TRUE);

    return matchFound;
}

void sps_sources_list(GArray *sourceInfo)
{
    sps_sources_windows_list(sourceInfo);
    sps_sources_displays_list(sourceInfo);
}
