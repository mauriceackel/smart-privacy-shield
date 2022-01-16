extern "C"
{
#include "winanalyzerutils.h"
#include <glib.h>
}
#include <Windows.h>
#include <psapi.h>
#include <string>

#define ENTRY_COUNT 3

struct _EnumData
{
    GArray *winInfo;
    guint64 displayId;
};
typedef struct _EnumData EnumData;

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
    EnumData *data = (EnumData *)param;

    // Ignore windows that aare not captureable
    if (!is_capturable(hWnd))
        return TRUE;

    char *windowName, *ownerName;

    // Get window name
    gint windowNameLength = GetWindowTextLength(hWnd);
    windowName = (char *)g_malloc(windowNameLength + 1);
    windowName[0] = '\0';
    GetWindowText(hWnd, windowName, windowNameLength + 1);

    // Get owner process name
    gulong parentPid;
    GetWindowThreadProcessId(hWnd, &parentPid);
    char processName[MAX_PATH];
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, parentPid); // Open process to be able to retrieve the name
    if (hProc != NULL)
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if (EnumProcessModules(hProc, &hMod, sizeof(hMod), &cbNeeded))
            GetModuleBaseNameA(hProc, hMod, processName, sizeof(processName));

        CloseHandle(hProc);
    }

    // If owner name does not exist, use default
    if (strlen(processName) == 0)
        ownerName = g_strdup("application");
    else
    {

        if (g_str_has_suffix(processName, ".exe") || g_str_has_suffix(processName, ".EXE"))
            processName[strlen(processName) - 4] = '\0'; // trim end
        ownerName = g_utf8_strdown(processName, -1);
    }

    // Get window bounding box
    RECT windowRect;
    GetWindowRect(hWnd, &windowRect);

    MONITORINFO monitorInfo = {sizeof(monitorInfo)};
    GetMonitorInfo((HMONITOR)data->displayId, (LPMONITORINFO)&monitorInfo);
    RECT displayRect = monitorInfo.rcMonitor;

    RECT intersection;
    gboolean intersected = IntersectRect(&intersection, &windowRect, &displayRect);
    
    BoundingBox bbox;
    WindowInfo wInfo;

    if (!intersected)
        goto out;

    bbox = {
        .x = (gint)intersection.left,
        .y = (gint)intersection.top,
        .width = (gint)(intersection.right - intersection.left),
        .height = (gint)(intersection.bottom - intersection.top),
    };

    wInfo = {
        .ownerName = g_utf8_strdown(ownerName, -1),
        .windowName = g_utf8_strdown(windowName, -1),
        .zIndex = -1, // Has to be updated once all windows are retrieved
        .bbox = bbox,
        .id = (guintptr)hWnd,
    };

    // Add window to list
    g_array_append_val(data->winInfo, wInfo);

    // Free memory
out:
    g_free(windowName);
    g_free(ownerName);

    return TRUE;
}

BoundingBox get_taskbar_bounding_box(HMONITOR display)
{
    gint taskbarPosition;

    MONITORINFO monitorInfo = {sizeof(monitorInfo)};
    GetMonitorInfo(display, (LPMONITORINFO)&monitorInfo);

    // Compute dock position and bounding box
    RECT frame = monitorInfo.rcMonitor;
    RECT visibleFrame = monitorInfo.rcWork;

    if ((visibleFrame.right - visibleFrame.left) < (frame.right - frame.left))
    {
        if (visibleFrame.left == frame.left)
            taskbarPosition = 2; // right
        else
            taskbarPosition = 0; // left
    }
    else
    {
        if (visibleFrame.top == frame.top)
            taskbarPosition = 1; // bottom
        else
            taskbarPosition = 3; // top
    }

    RECT taskbar;
    switch (taskbarPosition)
    {
    case 0: // left
    {
        taskbar.left = frame.left;
        taskbar.top = frame.top;
        taskbar.right = visibleFrame.left;
        taskbar.bottom = frame.bottom;
    };
    break;
    case 1: // bottom
    {
        taskbar.left = frame.left;
        taskbar.top = visibleFrame.bottom;
        taskbar.right = frame.right;
        taskbar.bottom = frame.bottom;
    };
    break;
    case 2: // right
    {
        taskbar.left = visibleFrame.right;
        taskbar.top = frame.top;
        taskbar.right = frame.right;
        taskbar.bottom = frame.bottom;
    };
    break;
    case 3: // top
    {
        taskbar.left = frame.left;
        taskbar.top = frame.top;
        taskbar.right = frame.right;
        taskbar.bottom = visibleFrame.top;
    };
    break;
    }

    BoundingBox taskbarBoundingBox = {
        .x = taskbar.left,
        .y = taskbar.top,
        .width = taskbar.right - taskbar.left,
        .height = taskbar.bottom - taskbar.top,
    };

    return taskbarBoundingBox;
}

void get_windows(guint64 displayId, GArray *winInfo)
{
    EnumData data = {
        .winInfo = winInfo,
        .displayId = displayId,
    };

    // First, we only extract the window info with undefined zIndex
    EnumWindows(enum_window_callback, (LPARAM)&data);

    // Compute and update zIndex
    for (gint i = 0; i < winInfo->len; i++)
    {
        WindowInfo *wInfo = &g_array_index(winInfo, WindowInfo, i);

        gint zIndex = winInfo->len - i; // results are returned from high to low zorder, hence we need to invert the indices
        wInfo->zIndex = zIndex;
    }

    // Add taskbar
    WindowInfo taskbar = {
        .ownerName = g_strdup("taskbar"),
        .windowName = g_strdup("taskbar"),
        .zIndex = (gint)(winInfo->len + 1), // Highest zIndex
        .bbox = get_taskbar_bounding_box((HMONITOR)displayId),
        .id = 0,
    };

    g_array_append_val(winInfo, taskbar);
}
