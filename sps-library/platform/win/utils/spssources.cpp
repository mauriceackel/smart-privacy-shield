extern "C"
{
#include <glib.h>
#include <sps/utils/spssources.h>
}
#include <Windows.h>
#include <string>

#define ENTRY_COUNT 3

static gboolean is_blacklisted(HWND hWnd)
{
    const char *entries[ENTRY_COUNT][2] = {
        {"Task View", "Windows.UI.Core.CoreWindow"},
        {"DesktopWindowXamlSource", "Windows.UI.Core.CoreWindow"},
        {"PopupHost", "Xaml_WindowedPopupClass"}};

    char className[256];
    for (gint i = 0; i < ENTRY_COUNT; i++)
    {
        gint windowNameLength = GetWindowTextLength(hWnd);
        std::string windowName(windowNameLength + 1, '\0');
        GetWindowText(hWnd, &windowName[0], windowName.size());
        // Ignore if name does not match already
        if (strcmp(windowName.c_str(), entries[i][0]) != 0)
            continue;

        GetClassName(hWnd, className, sizeof(className));
        // Name and class match, window blacklisted
        if (strcmp(className, entries[i][1]) == 0)
            return TRUE;
    }

    return FALSE;
}

static gboolean is_capturable(HWND hWnd)
{
    gint windowNameLength = GetWindowTextLength(hWnd);

    // Ignore windows without name, if they are the shell's desktop window, if they are not visible and if they are not parent windows
    if (windowNameLength == 0 || hWnd == GetShellWindow() || !IsWindowVisible(hWnd) || GetAncestor(hWnd, GA_ROOT) != hWnd)
        return FALSE;

    glong style = GetWindowLong(hWnd, GWL_STYLE);
    // Ignore disabled windows
    if (style & WS_DISABLED)
        return FALSE;

    glong exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
    // Ignore tooltips
    if (exStyle & WS_EX_TOOLWINDOW)
        return FALSE;

    // Ignore some known known windows
    if (is_blacklisted(hWnd))
        return FALSE;

    return TRUE;
}

static gboolean enum_window_callback(HWND hWnd, LPARAM param)
{
    // Ignore windows that are not captureable
    if (!is_capturable(hWnd))
        return TRUE;

    // Get window name
    gint windowNameLength = GetWindowTextLength(hWnd);
    std::string windowName(windowNameLength + 1, '\0');
    GetWindowText(hWnd, &windowName[0], windowName.size());

    GString *displayName = g_string_new("");
    g_string_append_printf(displayName, "[Application] %s", windowNameLength != 0 ? windowName.c_str() : "Unknown");

    // Get name of window's process
    gulong parentPid;
    GetWindowThreadProcessId(hWnd, &parentPid);
    char processName[100];
    gulong processNameLength = sizeof(processName);
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parentPid); // Open process to be able to retrieve the name
    if (hProc != NULL)
    {
        QueryFullProcessImageName(hProc, 0, processName, &processNameLength);
        CloseHandle(hProc);
    }

    GString *recoverName = g_string_new("");
    g_string_append_printf(recoverName, "%s", processName);

    SpsSourceInfo sInfo = {
        .name = g_strdup(displayName->str),
        .internalName = g_strdup(recoverName->str),
        .id = (guintptr)hWnd,
        .type = SOURCE_TYPE_WINDOW,
    };

    // Add window to list
    GArray *sourceInfo = (GArray *)param;
    g_array_append_val(sourceInfo, sInfo);

    // Free memory
    g_string_free(displayName, TRUE);
    g_string_free(recoverName, TRUE);

    return TRUE;
}

void sps_sources_windows_list(GArray *sourceInfo)
{
    EnumWindows(enum_window_callback, (LPARAM)sourceInfo);
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

static gboolean enum_display_callback(HMONITOR hMon, HDC hdc, LPRECT bounds, LPARAM param)
{
    // Get display info
    MONITORINFOEX monitorInfo = {sizeof(monitorInfo)};
    GetMonitorInfo(hMon, (LPMONITORINFO)&monitorInfo);
    gint width = bounds->right - bounds->left;
    gint height = bounds->bottom - bounds->top;

    GString *displayName = g_string_new("");
    g_string_append_printf(displayName, "[Screen] %s %ix%i", monitorInfo.szDevice, width, height);

    GString *recoverName = g_string_new("");
    g_string_append_printf(recoverName, "%s-%i-%i", monitorInfo.szDevice, width, height);

    SpsSourceInfo sInfo = {
        .name = g_strdup(displayName->str),
        .internalName = g_strdup(recoverName->str),
        .id = (guintptr)hMon,
        .type = SOURCE_TYPE_DISPLAY,
    };

    // Add dispaly to list
    GArray *sourceInfo = (GArray *)param;
    g_array_append_val(sourceInfo, sInfo);

    // Free memory
    g_string_free(displayName, TRUE);
    g_string_free(recoverName, TRUE);

    return TRUE;
}

void sps_sources_displays_list(GArray *sourceInfo)
{
    EnumDisplayMonitors(NULL, NULL, enum_display_callback, (LPARAM)sourceInfo);
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
