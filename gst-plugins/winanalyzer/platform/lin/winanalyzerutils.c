#include "winanalyzerutils.h"
#include "gstwinanalyzer.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

void get_all_windows(GstWinAnalyzer *filter, GArray *winInfo)
{
    get_visible_windows(filter, winInfo);
}

void get_visible_windows(GstWinAnalyzer *filter, GArray *winInfo)
{
    Display *display = XOpenDisplay(NULL);
    Window rootWindowId = RootWindow(display, DefaultScreen(display));
    Window rootWindowIdReturn, parentWindowIdReturn, *windowIds;
    char *tmp;
    guint windowCount;

    // Get all X windows
    XQueryTree(display, rootWindowId, &rootWindowIdReturn, &parentWindowIdReturn, &windowIds, &windowCount);

    // Map raw data to window info
    for (guint i = 0; i < windowCount; i++)
    {
        Window windowId = windowIds[i];

        // Get window attributes for filtering
        XWindowAttributes windowAttributes;
        XGetWindowAttributes(display, windowId, &windowAttributes);

        // Only include windows that are viewable
        if (windowAttributes.map_state != 2)
            continue;

        // Get window name
        char *windowName;
        XFetchName(display, windowId, &windowName);
        tmp = g_strdup(windowName);
        XFree(windowName);
        windowName = tmp;

        if (!windowName)
        {
            // Check if window has children from which to get a name
            Window r, p, *childWindowIds;
            guint childCount;
            XQueryTree(display, windowId, &r, &p, &childWindowIds, &childCount);

            if (childCount == 0)
            {
                // No name can be found
                windowName = g_strdup("unknown");
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
                        windowName = g_strdup(childName);
                        XFree(childName);
                        break;
                    }
                }
            }

            XFree(childWindowIds);
        }

        // Get owner name
        char *ownerName = g_strdup("unknown"); // This is not really possible via the XClient

        // Check bounding boxes
        BoundingBox windowRect = {
            .x = windowAttributes.x,
            .y = windowAttributes.y,
            .width = windowAttributes.width,
            .height = windowAttributes.height
        };

        XWindowAttributes displayAttributes;
        XGetWindowAttributes(display, filter->displayId, &displayAttributes);
        BoundingBox displayRect = {
            .x = displayAttributes.x,
            .y = displayAttributes.y,
            .width = displayAttributes.width,
            .height = displayAttributes.height
        };

        BoundingBox intersection = gst_bounding_box_intersect(&windowRect, &displayRect);
        if (intersection.width <= 0 || intersection.height <= 0)
            continue;

        gint zIndex = i; // results are returned from low to high zorder (i.e. background first)

        // Add the actual window info
        WindowInfo wInfo = {
            .ownerName = g_utf8_strdown(ownerName, -1),
            .windowName = g_utf8_strdown(windowName, -1),
            .zIndex = zIndex,
            .bbox = intersection,
            .id = (guintptr)windowId,
        };

        GST_DEBUG_OBJECT(filter, "WindowDetection: owner=%s, window=%s, bounds=(%i, %i, %i, %i)", wInfo.ownerName, wInfo.windowName, wInfo.bbox.x, wInfo.bbox.y, wInfo.bbox.width, wInfo.bbox.height);

        g_array_append_val(winInfo, wInfo);

        // Free memory
        g_free((void *)windowName);
        g_free((void *)ownerName);
    }

    // Free memory
    XFree(windowIds);
    XCloseDisplay(display);
}
