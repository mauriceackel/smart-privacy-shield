#include "winanalyzerutils.h"
#include "gstwinanalyzer.h"

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CGWindow.h>
#import <CoreGraphics/CGDirectDisplay.h>

#define WINDOW_NAME ((NSString *)kCGWindowName)
#define WINDOW_BOUNDS ((NSString *)kCGWindowBounds)
#define WINDOW_NUMBER ((NSString *)kCGWindowNumber)
#define OWNER_NAME ((NSString *)kCGWindowOwnerName)
#define OWNER_PID ((NSString *)kCGWindowOwnerPID)
#define WINDOW_LAYER ((NSString *)kCGWindowLayer)

void get_all_windows(GstWinAnalyzer *filter, GArray *winInfo)
{
    if(filter->displayId == 0)
        return;

    NSArray *windows = (NSArray *)CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID);
    [windows autorelease];
    UInt32 windowCount = [windows count];

    NSError *error = NULL;
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:@"_+" options:NSRegularExpressionCaseInsensitive error:&error];

    // Map raw data to window info
    for (NSDictionary *dict in windows)
    {
        NSNumber *windowId = (NSNumber *)dict[WINDOW_NUMBER];
        NSString *windowName = (NSString *)dict[WINDOW_NAME];
        NSNumber *ownerPid = (NSNumber *)dict[OWNER_PID];
        NSString *ownerName = (NSString *)dict[OWNER_NAME];

        if (!ownerName.length || !windowName.length)
            continue;

        windowName = [windowName lowercaseString];
        windowName = [windowName stringByReplacingOccurrencesOfString:@" " withString:@"_"];
        windowName = [regex stringByReplacingMatchesInString:windowName options:0 range:NSMakeRange(0, [windowName length]) withTemplate:@"_"]; // combine multiple underscores
        ownerName = [ownerName lowercaseString];
        ownerName = [ownerName stringByReplacingOccurrencesOfString:@" " withString:@"_"];
        ownerName = [regex stringByReplacingMatchesInString:ownerName options:0 range:NSMakeRange(0, [ownerName length]) withTemplate:@"_"]; // combine multiple underscores

        CGRect displayRect = CGDisplayBounds(filter->displayId);
        
        // Ignore all dock elements except dock itself
        if ([ownerName isEqualToString:@"dock"] && ![windowName isEqualToString:@"dock"])
            continue;

        CGRect windowRect;
        CFDictionaryRef windowRectRef = (CFDictionaryRef)dict[WINDOW_BOUNDS];
        if(!CGRectMakeWithDictionaryRepresentation(windowRectRef, &windowRect))
            continue;

        CGRect intersection = CGRectIntersection(windowRect, displayRect);
        if(CGRectIsNull(intersection) || intersection.size.width <= 0 || intersection.size.height <= 0)
            continue;

        BoundingBox bbox = {
            .x = (gint)windowRect.origin.x,
            .y = (gint)windowRect.origin.y,
            .width = (gint)windowRect.size.width,
            .height = (gint)windowRect.size.height,
        };

        WindowInfo wInfo = {
            .ownerName = g_strdup(ownerName.UTF8String),
            .windowName = g_strdup(windowName.UTF8String),
            .bbox = bbox,
            .id = (guintptr)windowId.intValue,
        };

        g_array_append_val(winInfo, wInfo);
    }
}

CGRect get_dock_bounding_box(gint displayId, CGRect display)
{
    gint dockPosition;

    NSArray *screens = [NSScreen screens];

    // Get screen belonging to selected display
    NSScreen *screen = NULL;
    for (NSScreen *s in screens)
    {
        NSNumber *dId = (NSNumber *)[s deviceDescription][@"NSScreenNumber"];
        if (dId.intValue == displayId)
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

void get_visible_windows(GstWinAnalyzer *filter, GArray *winInfo)
{
    // Get info for all existing windows
    NSArray *windows = (NSArray *)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    [windows autorelease];
    UInt32 windowCount = [windows count];

    NSError *error = NULL;
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:@"_+" options:NSRegularExpressionCaseInsensitive error:&error];
    
    // Map raw data to window info
    for (gint i = 0; i < windowCount; i++)
    {
        NSDictionary *dict = windows[i];

        NSNumber *windowId = (NSNumber *)dict[WINDOW_NUMBER];
        NSString *windowName = (NSString *)dict[WINDOW_NAME];
        NSNumber *ownerPid = (NSNumber *)dict[OWNER_PID];
        NSString *ownerName = (NSString *)dict[OWNER_NAME];
        NSNumber *windowLayer = (NSNumber *)dict[WINDOW_LAYER];

        if (!ownerName.length || !windowName.length)
            continue;

        windowName = [windowName lowercaseString]; // to lowercase
        windowName = [windowName stringByReplacingOccurrencesOfString:@" " withString:@"_"]; // replace space with dash
        windowName = [regex stringByReplacingMatchesInString:windowName options:0 range:NSMakeRange(0, [windowName length]) withTemplate:@"_"]; // combine multiple underscores

        ownerName = [ownerName lowercaseString]; // to lowercase
        ownerName = [ownerName stringByReplacingOccurrencesOfString:@" " withString:@"_"]; // replace space with dash
        ownerName = [regex stringByReplacingMatchesInString:ownerName options:0 range:NSMakeRange(0, [ownerName length]) withTemplate:@"_"]; // combine multiple underscores

        CGRect windowRect;
        CFDictionaryRef windowRectRef = (CFDictionaryRef)dict[WINDOW_BOUNDS];
        if(!CGRectMakeWithDictionaryRepresentation(windowRectRef, &windowRect))
            continue;
        
        CGRect displayRect = CGDisplayBounds(filter->displayId);
        
        // Special treatment for dock as it covers the whole screen and would oclude everything behind it
        if ([ownerName isEqualToString:@"dock"])
        {
            if ([windowName isEqualToString:@"dock"])
                windowRect = get_dock_bounding_box(filter->displayId, displayRect);
            else
                continue; // Ignore all dock elements that are not the actual dock
        }
        
        CGRect intersection = CGRectIntersection(windowRect, displayRect);
        if(CGRectIsNull(intersection) || intersection.size.width <= 0 || intersection.size.height <= 0)
            continue;

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
        if(windowLayer.intValue >= 24)
            zIndex = windowCount; // make all elements on menuba and menu bar itself the same zIndex

        WindowInfo wInfo = {
            .ownerName = g_strdup(ownerName.UTF8String),
            .windowName = g_strdup(windowName.UTF8String),
            .bbox = bbox,
            .zIndex = zIndex,
            .id = (guintptr)windowId.intValue,
        };

        GST_DEBUG_OBJECT(filter, "WindowDetection: owner=%s, window=%s, bounds=(%i, %i, %i, %i), layer=%i", wInfo.ownerName, wInfo.windowName, wInfo.bbox.x, wInfo.bbox.y, wInfo.bbox.width, wInfo.bbox.height, windowLayer.intValue);

        g_array_append_val(winInfo, wInfo);
    }
}
