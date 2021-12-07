#include "winanalyzerutils.h"

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CGWindow.h>
#import <CoreGraphics/CGDirectDisplay.h>

#define WINDOW_NAME ((NSString *)kCGWindowName)
#define WINDOW_BOUNDS ((NSString *)kCGWindowBounds)
#define WINDOW_NUMBER ((NSString *)kCGWindowNumber)
#define OWNER_NAME ((NSString *)kCGWindowOwnerName)
#define OWNER_PID ((NSString *)kCGWindowOwnerPID)
#define WINDOW_LAYER ((NSString *)kCGWindowLayer)

typedef enum {
    kCGSSpaceIncludesCurrent = 1 << 0,
    kCGSSpaceIncludesOthers = 1 << 1,
    kCGSSpaceIncludesUser = 1 << 2,

    kCGSAllSpacesMask = kCGSSpaceIncludesCurrent | kCGSSpaceIncludesOthers | kCGSSpaceIncludesUser
} CGSSpaceMask;
typedef guint32 CGSConnectionID;
typedef guint64 CGSSpaceID;

CGSConnectionID CGSMainConnectionID();
CGSSpaceID CGSManagedDisplayGetCurrentSpace(CGSConnectionID cid, CFStringRef displayUuid);
CFArrayRef CGSCopySpacesForWindows(CGSConnectionID cid, CGSSpaceMask mask, CFArrayRef wids);

CGRect get_dock_bounding_box(guint64 displayId, CGRect display)
{
    @autoreleasepool {
        gint dockPosition;

        NSArray *screens = [NSScreen screens];

        // Get screen belonging to selected display
        NSScreen *screen = NULL;
        for (NSScreen *s in screens)
        {
            NSNumber *dId = (NSNumber *)[s deviceDescription][@"NSScreenNumber"];
            gboolean isDisplay = dId.intValue == displayId;

            if (isDisplay)
            {
                screen = s;
                break;
            }
        }

        // No matching screen found
        if (!screen)
            return CGRectNull;

        // Compute dock position and bounding box
        CGRect frame = screen.frame;
        CGRect visibleFrame = screen.visibleFrame;

        if (visibleFrame.size.width < frame.size.width)
        {
            if (visibleFrame.origin.x == frame.origin.x)
                dockPosition = 2; // right
            else
                dockPosition = 0; // left
        }
        else
            dockPosition = 1; // bottom
        
        CGRect dock;
        switch (dockPosition)
        {
            case 0: // left
            {
                dock.origin.x = display.origin.x;
                dock.origin.y = display.origin.y + frame.size.height - visibleFrame.size.height;
                dock.size.width = frame.size.width - visibleFrame.size.width;
                dock.size.height = visibleFrame.size.height;
            }; break;
            case 1: // bottom
            {
                dock.origin.x = display.origin.x;
                dock.origin.y = display.origin.y + frame.size.height - (visibleFrame.origin.y - frame.origin.y);
                dock.size.width = frame.size.width;
                dock.size.height = visibleFrame.origin.y - frame.origin.y;
            }; break;
            case 2: // right
            {
                dock.origin.x = display.origin.x + visibleFrame.size.width;
                dock.origin.y = display.origin.y + frame.size.height - visibleFrame.size.height;
                dock.size.width = frame.size.width - visibleFrame.size.width;
                dock.size.height = visibleFrame.size.height;
            }; break;
        }

        return dock;
    }
}

void get_windows(guint64 displayId, GArray *winInfo)
{
    // Get info for all existing windows
    NSArray *windows = (NSArray *)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    UInt32 windowCount = [windows count];

    // Map raw data to window info
    CGRect displayRect = CGDisplayBounds(displayId);

    CFUUIDRef displayUuidRef = CGDisplayCreateUUIDFromDisplayID(displayId);
    CFStringRef displayUuid = CFUUIDCreateString(kCFAllocatorDefault, displayUuidRef);

    CGSConnectionID connectionId = CGSMainConnectionID();
    CGSSpaceID displaySpaceId = CGSManagedDisplayGetCurrentSpace(connectionId, displayUuid);

    for (gint i = 0; i < windowCount; i++)
    {
        NSDictionary *dict = windows[i];

        NSNumber *windowId = (NSNumber *)dict[WINDOW_NUMBER];
        NSString *windowNameBase = (NSString *)dict[WINDOW_NAME];
        NSNumber *ownerPid = (NSNumber *)dict[OWNER_PID];
        NSString *ownerNameBase = (NSString *)dict[OWNER_NAME];
        NSNumber *windowLayer = (NSNumber *)dict[WINDOW_LAYER];

        if (!windowNameBase.length || !ownerNameBase.length)
            continue;

        char *tmp;
        GString *windowName, *ownerName;

        @autoreleasepool {
            tmp = g_utf8_strdown(windowNameBase.UTF8String, -1);
            windowName = g_string_new(tmp);
            g_string_replace(windowName, " ", "_", 0);
            g_free(tmp);

            tmp = g_utf8_strdown(ownerNameBase.UTF8String, -1);
            ownerName = g_string_new(tmp);
            g_string_replace(ownerName, " ", "_", 0);
            g_free(tmp);
        }

        // Check if window is associated with current space of display
        gboolean isOnSameSpace = FALSE;
        CFArrayRef windowSpaces;
        @autoreleasepool {
            windowSpaces = CGSCopySpacesForWindows(connectionId, kCGSAllSpacesMask, (__bridge CFArrayRef) @[ @(windowId.intValue) ]);
        }
        CFIndex spaceCount = CFArrayGetCount(windowSpaces);

        if (spaceCount == 0)
        {
            // Unable to get space, pass it forward to check for display bounds
            isOnSameSpace = TRUE;
        }

        for(gint i = 0; i < spaceCount && !isOnSameSpace; i++)
        {
            NSNumber *windowSpaceId = (NSNumber*)CFArrayGetValueAtIndex(windowSpaces, i);

            if (windowSpaceId.intValue == displaySpaceId)
            {
                isOnSameSpace = TRUE;
                break;
            }
        }

        // TODO: There is an issue when a window is between two spaces and rendered transparently. Then, this logic will not consider the window although it is visible in the capture
        if (!isOnSameSpace)
            goto out;

        // Get window bounding box
        CGRect windowRect;
        CFDictionaryRef windowRectRef = (CFDictionaryRef)dict[WINDOW_BOUNDS];
        if (!CGRectMakeWithDictionaryRepresentation(windowRectRef, &windowRect))
            goto out;
        
        // Special treatment for dock as it covers the whole screen and would oclude everything behind it
        if (g_str_equal(ownerName->str, "dock"))
        {
            if (g_str_equal(windowName->str, "dock"))
                windowRect = get_dock_bounding_box(displayId, displayRect);
            else
                goto out; // Ignore all dock elements that are not the actual dock
        }
        
        CGRect intersection = CGRectIntersection(windowRect, displayRect);
        if (CGRectIsNull(intersection) || intersection.size.width <= 0 || intersection.size.height <= 0)
            goto out;

        // Align to full frame
        intersection.origin.x -= displayRect.origin.x;
        intersection.origin.y -= displayRect.origin.y;

        BoundingBox bbox = {
            .x = (gint)intersection.origin.x,
            .y = (gint)intersection.origin.y,
            .width = (gint)intersection.size.width,
            .height = (gint)intersection.size.height,
        };        

        gint zIndex = windowCount - i; // results are returned from high to low zorder, hence we need to invert the indices
        if (windowLayer.intValue >= 24)
            zIndex = windowCount; // make all elements on menuba and menu bar itself the same zIndex

        WindowInfo wInfo = {
            .ownerName = g_strdup(ownerName->str),
            .windowName = g_strdup(windowName->str),
            .bbox = bbox,
            .zIndex = zIndex,
            .id = (guintptr)windowId.intValue,
        };

        g_array_append_val(winInfo, wInfo);

    out:
        CFRelease(windowSpaces);
        g_string_free(ownerName, TRUE);
        g_string_free(windowName, TRUE);
    }

    CFRelease(displayUuidRef);
    CFRelease(displayUuid);
    [windows release];
}
