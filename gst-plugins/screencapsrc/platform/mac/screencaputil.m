#include "screencaputil.h"
#include "gstscreencapsrc.h"
#include "winanalyzerutils.h"

#include <windowmeta.h>
#include <windowlocationsmeta.h>
#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <gst/video/video.h>

#include <Foundation/Foundation.h>
#include <CoreGraphics/CGWindow.h>
#include <CoreGraphics/CGDirectDisplay.h>

G_DEFINE_TYPE(GstScreenCapUtil, gst_screen_cap_util, G_TYPE_OBJECT);

struct _GstScreenCapUtilPrivate {};

// -----------------------------------------------------------------------------------------------------
// ----------------------------------------- Private Helper --------------------------------------------
// -----------------------------------------------------------------------------------------------------

static void get_window_frame(gulong windowId, CGImageRef *imageRef, gint *width, gint *height, gint *x, gint *y, size_t *stride, size_t *size)
{
    // Check window existence
    NSArray *arr = (NSArray *)CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, windowId);
    [arr autorelease];

    if (!arr.count)
    {
        // No window found
        return;
    }

    // Make snapshot of window at nominal resolution (i.e. not double the resolution)
    *imageRef = CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, windowId, kCGWindowImageNominalResolution | kCGWindowImageBoundsIgnoreFraming); //kCGWindowImageDefault

    if (!*imageRef)
    {
        return;
    }

    // Get image dimensions
    NSDictionary *windowData = arr[0];
    NSDictionary *bounds = (NSDictionary *)windowData[(NSString *)kCGWindowBounds];

    gint w = CGImageGetWidth(*imageRef);
    if (width)
        *width = w;

    gint h = CGImageGetHeight(*imageRef);
    if (height)
        *height = h;

    NSNumber *xPosNum = (NSNumber *)bounds[@"X"];
    gint xPos = (gint)xPosNum.intValue;
    if (x)
        *x = xPos;

    NSNumber *yPosNum = (NSNumber *)bounds[@"Y"];
    gint yPos = (gint)yPosNum.intValue;
    if (y)
        *y = yPos;

    size_t str = w * BYTES_PER_PIXEL;
    if (stride)
        *stride = str;

    size_t s = str * h;
    if (size)
        *size = s;
}

static void get_display_frame(gulong displayId, CGImageRef *imageRef, gint *width, gint *height, size_t *stride, size_t *size)
{
    // Make snapshot of screen
    // On retina screens, this image will be huge. Hence, we don't report back the captured screenshot size, but the logical screen dimensions.
    // Scaling will be handled when we draw this cgImageRef in the calling method.
    *imageRef = CGDisplayCreateImage((CGDirectDisplayID)displayId);

    if (!*imageRef)
    {
        return;
    }

    // Get logical screen dimensions
    CGDisplayModeRef m = CGDisplayCopyDisplayMode(displayId);

    gint w = CGDisplayModeGetWidth(m);
    if (width)
        *width = w;

    gint h = CGDisplayModeGetHeight(m);
    if (height)
        *height = h;

    size_t str = w * BYTES_PER_PIXEL;
    if (stride)
        *stride = str;

    size_t s = str * h;
    if (size)
        *size = s;

    // Clear references
    CGDisplayModeRelease(m);
}

// -----------------------------------------------------------------------------------------------------
// ----------------------------------------- Object Methods --------------------------------------------
// -----------------------------------------------------------------------------------------------------

gboolean gst_screen_cap_util_get_dimensions(GstScreenCapUtil *self, GstScreenCapSrc *src, gint *width, gint *height, size_t *size)
{
    if (G_UNLIKELY(!self->initialized))
        return FALSE;

    CGImageRef imageRef = NULL;

    // Decide on display vs. window capture and get dimensions
    if (src->sourceType == SOURCE_TYPE_WINDOW)
    {
        get_window_frame(src->sourceId, &imageRef, width, height, NULL, NULL, NULL, size);
    }
    else if (src->sourceType == SOURCE_TYPE_DISPLAY)
    {
        get_display_frame(src->sourceId, &imageRef, width, height, NULL, size);
    }
    else
        return FALSE;

    CGImageRelease(imageRef);
    return TRUE;
}

GstFlowReturn gst_screen_cap_util_create_buffer(GstScreenCapUtil *self, GstScreenCapSrc *src, GstBuffer **buffer)
{
    if (G_UNLIKELY(!self->initialized))
        return GST_FLOW_ERROR;

    GST_DEBUG_OBJECT(src, "Creating buffer");

    GstFlowReturn ret = GST_FLOW_OK;

    if (G_UNLIKELY(GST_VIDEO_INFO_FORMAT(&src->info) == GST_VIDEO_FORMAT_UNKNOWN))
        return GST_FLOW_NOT_NEGOTIATED;

    GstBufferPool *pool;
    GstVideoFrame frame;
    GstWindowMeta *windowMeta;
    GstWindowLocationsMeta *windowLocationsMeta;
    GArray *windowInfos = NULL;

    CGImageRef imageRef = NULL;
    CGContextRef cgContext = NULL;
    gint width, height;
    gint x = 0, y = 0;
    size_t stride, size;

    // Decide on display vs. window capture and do the capture
    if (src->sourceType == SOURCE_TYPE_WINDOW)
    {
        get_window_frame(src->sourceId, &imageRef, &width, &height, &x, &y, &stride, &size);
    }
    else if (src->sourceType == SOURCE_TYPE_DISPLAY)
    {
        // If requested, add position of windows
        if (src->reportWindowLocations)
        {
            windowInfos = g_array_new(FALSE, FALSE, sizeof(WindowInfo));
            g_array_set_clear_func(windowInfos, window_info_clear);
            get_visible_windows(src->sourceId, windowInfos);
        }

        get_display_frame(src->sourceId, &imageRef, &width, &height, &stride, &size);
    }
    else
    {
        ret = GST_FLOW_ERROR;
        goto out_capture;
    }

    // Unable to capture frame
    if (!imageRef)
    {
        ret = GST_FLOW_ERROR;
        goto out_capture;
    }

    if (src->info.width != width || src->info.height != height)
    {
        // Set new caps according to current frame dimensions
        GST_WARNING("Output frame size has changed %dx%d -> %dx%d, updating caps", src->info.width, src->info.height, width, height);

        GstCaps *newCaps = gst_caps_copy(src->caps);
        gst_caps_set_simple(newCaps, "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
        gst_base_src_set_caps(GST_BASE_SRC(src), newCaps);
        gst_base_src_negotiate(GST_BASE_SRC(src));
    }

    // Get buffer from bufferpool
    pool = gst_base_src_get_buffer_pool(GST_BASE_SRC(src));
    ret = gst_buffer_pool_acquire_buffer(pool, buffer, NULL);
    if (G_UNLIKELY(ret != GST_FLOW_OK))
        goto out_buffer;

    // Add metadata
    windowMeta = GST_WINDOW_META_ADD(*buffer);
    windowMeta->width = width;
    windowMeta->height = height;
    windowMeta->x = x;
    windowMeta->y = y;

    if (windowInfos)
    {
        windowLocationsMeta = GST_WINDOW_LOCATIONS_META_ADD(*buffer);
        g_array_free(windowLocationsMeta->windowInfos, TRUE);
        windowLocationsMeta->windowInfos = windowInfos;
    }

    // Get videoframe from buffer
    if (!gst_video_frame_map(&frame, &src->info, *buffer, GST_MAP_WRITE))
        goto out_buffer;

    // Check if buffer has correct size for captured image
    if (frame.info.size != size)
    {
        GST_DEBUG_OBJECT(src, "Frame has wrong size (%zu instead of %zu)", frame.info.size, size);
        goto out_frame;
    }

    GST_DEBUG_OBJECT(src, "Captured frame. Width: %i, Height: %i, Stride: %zu, Size: %zu for buffer of size: %zu!", width, height, stride, size, frame.info.size);

    // Convert cgImage to bitmap using memory of buffer
    cgContext = CGBitmapContextCreate(
        GST_VIDEO_FRAME_PLANE_DATA(&frame, 0),
        width,
        height,
        8,
        stride,
        CGColorSpaceCreateDeviceRGB(),
        kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
    CGContextSetBlendMode(cgContext, kCGBlendModeCopy);
    CGContextSetInterpolationQuality(cgContext, kCGInterpolationHigh);
    CGRect boundingRect = {{0, 0}, {(CGFloat)width, (CGFloat)height}};
    CGContextDrawImage(cgContext, boundingRect, imageRef);

out_frame:
    gst_video_frame_unmap(&frame);

out_buffer:
    gst_object_unref(pool);

out_capture:
    // Free memory
    CGContextRelease(cgContext);
    CGImageRelease(imageRef);

    GST_DEBUG_OBJECT(src, "Done creating buffer");
    return ret;
}

void gst_screen_cap_util_initialize(GstScreenCapUtil *self, GstScreenCapSrc *src)
{
    if (self->initialized)
        return;

    self->initialized = TRUE;
}

void gst_screen_cap_util_finalize(GstScreenCapUtil *self)
{
    if (!self->initialized)
        return;

    self->initialized = FALSE;
}

// Object instantiation
static void gst_screen_cap_util_init(GstScreenCapUtil *self)
{
    // Init private data
    self->priv = g_new0(GstScreenCapUtilPrivate,1);

    // Set default values
    self->initialized = FALSE;
}

// Object finalization
static void gst_screen_cap_util_object_finalize(GObject *object)
{
    GstScreenCapUtil *self = GST_SCREEN_CAP_UTIL(object);

    // Default finalize routine
    gst_screen_cap_util_finalize(self);

    // Free private data
    g_free((void *)self->priv);

    G_OBJECT_CLASS(gst_screen_cap_util_parent_class)->finalize(object);
}

static void gst_screen_cap_util_class_init(GstScreenCapUtilClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = gst_screen_cap_util_object_finalize;
}

GstScreenCapUtil *gst_screen_cap_util_new()
{
    return g_object_new(GST_TYPE_SCREEN_CAP_UTIL, NULL);
}