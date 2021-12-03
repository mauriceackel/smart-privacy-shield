#include <glib.h>
#include <sps/utils/spssources.h>

#import <Foundation/Foundation.h>

#define WINDOW_NAME ((NSString *)kCGWindowName)
#define WINDOW_NUMBER ((NSString *)kCGWindowNumber)
#define OWNER_NAME ((NSString *)kCGWindowOwnerName)
#define OWNER_PID ((NSNumber *)kCGWindowOwnerPID)

void sps_sources_windows_list(GArray *sourceInfo)
{
    // Get info for all existing windows
    NSArray *windows = (NSArray *)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    [windows autorelease];
    UInt32 windowCount = [windows count];

    // Map raw data to window info
    for (NSDictionary *dict in windows)
    {
        NSNumber *windowId = (NSNumber *)dict[WINDOW_NUMBER];
        NSString *ownerName = (NSString *)dict[OWNER_NAME];
        NSString *windowName = (NSString *)dict[WINDOW_NAME];

        if (
            !ownerName.length ||
            [ownerName isEqualToString:@"SystemUIServer"] ||     // Exlude system internals
            [ownerName isEqualToString:@"Dock"] ||               // Exclude dock and desktop
            [ownerName isEqualToString:@"Window Server"] ||      // Exclude menu bar
            [ownerName isEqualToString:@"TextInputMenuAgent"] || // Exclude menu bar
            [windowName isEqualToString:@""] ||                  // Exclude empty names
            [windowName hasPrefix:@"Item-"])                     // Exclude menubar icons
        {
            continue;
        }

        NSString *displayName = [NSString stringWithFormat:@"[%@] %@", ownerName, windowName];

        SpsSourceInfo sInfo = {
            .name = g_strdup(displayName.UTF8String),
            .internalName = g_strdup(ownerName.UTF8String),
            .id = (guintptr)windowId.intValue,
            .type = SOURCE_TYPE_WINDOW,
        };

        g_array_append_val(sourceInfo, sInfo);
    }
}

gboolean sps_sources_recover_window(SpsSourceInfo *sourceInfo) {
    if (sourceInfo->type != SOURCE_TYPE_WINDOW)
        return FALSE;
    
    gboolean matchFound = FALSE;

    // Get all current windows
    GArray *sourceInfos = g_array_new(FALSE, FALSE, sizeof(SpsSourceInfo));
    g_array_set_clear_func(sourceInfos, sps_source_info_clear);
    sps_sources_windows_list(sourceInfos);
    
    // Check if window id exists in list
    for (gint i = 0; i < sourceInfos->len; i++) {
        SpsSourceInfo info = g_array_index(sourceInfos, SpsSourceInfo, i);
        if (info.id == sourceInfo->id) {
            matchFound = TRUE;
            sourceInfo->name = g_strdup(info.name);
            sourceInfo->internalName = g_strdup(info.internalName);
            // Early return if we found the window id
            goto out;
        }
    }

    matchFound = FALSE;

    // Try to recover window id from name
    for (gint i = 0; i < sourceInfos->len; i++) {
        SpsSourceInfo info = g_array_index(sourceInfos, SpsSourceInfo, i);

        if (strcmp(sourceInfo->internalName, info.internalName) == 0) {
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
    UInt32 maxDisplays = 16;
    CGDirectDisplayID onlineDisplays[16];
    UInt32 displayCount = 0;

    CGGetOnlineDisplayList(maxDisplays, onlineDisplays, &displayCount);

    for(int i = 0; i < displayCount; i++) {
        const CGDirectDisplayID displayId = onlineDisplays[i];
        const bool isInternal = CGDisplayIsBuiltin(displayId);
        const UInt32 width = CGDisplayPixelsWide(displayId);
        const UInt32 height = CGDisplayPixelsHigh(displayId);

        const UInt32 serialNumber = CGDisplaySerialNumber(displayId);
        NSString *recoverName = [NSString stringWithFormat:@"%@-%u-%u-%u", isInternal ? @"i" : @"e", serialNumber, width, height];

        NSString *displayName = [NSString stringWithFormat:@"[%@] Screen %i (%u x %u)", isInternal ? @"Internal" : @"External", i + 1, width, height];

        SpsSourceInfo sInfo = {
            .name = g_strdup(displayName.UTF8String),
            .internalName = g_strdup(recoverName.UTF8String),
            .id = (guintptr)displayId,
            .type = SOURCE_TYPE_DISPLAY,
        };

        g_array_append_val(sourceInfo, sInfo);
    }
}

gboolean sps_sources_recover_display(SpsSourceInfo *sourceInfo) {
    if (sourceInfo->type != SOURCE_TYPE_DISPLAY)
        return FALSE;
    
    gboolean matchFound = FALSE;

    // Get all current displays
    GArray *sourceInfos = g_array_new(FALSE, FALSE, sizeof(SpsSourceInfo));
    g_array_set_clear_func(sourceInfos, sps_source_info_clear);
    sps_sources_displays_list(sourceInfos);
    
    // Check if display id exists in list
    for (gint i = 0; i < sourceInfos->len; i++) {
        SpsSourceInfo info = g_array_index(sourceInfos, SpsSourceInfo, i);
        if (info.id == sourceInfo->id) {
            matchFound = TRUE;
            sourceInfo->name = g_strdup(info.name);
            sourceInfo->internalName = g_strdup(info.internalName);
            // Early return if we found the display id
            goto out;
        }
    }

    matchFound = FALSE;

    // Try to recover display id from name
    for (gint i = 0; i < sourceInfos->len; i++) {
        SpsSourceInfo info = g_array_index(sourceInfos, SpsSourceInfo, i);

        if (strcmp(sourceInfo->internalName, info.internalName) == 0) {
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
