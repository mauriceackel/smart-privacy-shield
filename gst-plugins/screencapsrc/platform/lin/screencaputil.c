#include "screencaputil.h"
#include "gstscreencapsrc.h"
#include "winanalyzerutils.h"

#include <windowmeta.h>
#include <windowlocationsmeta.h>
#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <gst/video/video.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

G_DEFINE_TYPE(GstScreenCapUtil, gst_screen_cap_util, G_TYPE_OBJECT);

struct _GstScreenCapUtilPrivate
{
    Display *display;
};

// -----------------------------------------------------------------------------------------------------
// ----------------------------------------- Private Helper --------------------------------------------
// -----------------------------------------------------------------------------------------------------

void get_frame(gulong windowId, Display *display, XImage **imageRef, gint *width, gint *height, gint *x, gint *y, size_t *stride, size_t *size)
{
    gboolean ret;

    XWindowAttributes windowAttributes;

    *imageRef = NULL;
    guint counter = 0;

    // Now, the issue is that there is a time difference between when we request the bounds of the window and the time we capture them.
    // As a result, when we make the window smaller, it is likely that the width & height we retrieve is larger than the actual image bounds
    // at the time we call XGetImage. If this is the case, a BadMatch error is thrown and *imageRef is set to NULL. To circumvent this, we
    // give a certain grace period in which we retry to get an image. If this does not work, we will fail and halt the pipeline.
    while (*imageRef == NULL && counter < 100)
    {
        ret = XGetWindowAttributes(display, windowId, &windowAttributes);
        if (!ret)
            return;

        *imageRef = XGetImage(display, windowId, 0, 0, windowAttributes.width, windowAttributes.height, AllPlanes, ZPixmap);
        counter++;
    }

    if (!*imageRef)
        return;

    if (width)
        *width = windowAttributes.width;

    if (height)
        *height = windowAttributes.height;

    if (x)
        *x = windowAttributes.x;

    if (y)
        *y = windowAttributes.y;

    size_t str = windowAttributes.width * BYTES_PER_PIXEL;
    if (stride)
        *stride = str;

    size_t s = str * windowAttributes.height;
    if (size)
        *size = s;
}

// -----------------------------------------------------------------------------------------------------
// ----------------------------------------- Object Methods --------------------------------------------
// -----------------------------------------------------------------------------------------------------

gboolean gst_screen_cap_util_get_dimensions(GstScreenCapUtil *self, GstScreenCapSrc *src, gint *width, gint *height, size_t *size)
{
    if (G_UNLIKELY(!self->initialized))
        return FALSE;

    XImage *imageRef = NULL;

    // Get dimensions
    get_frame(src->sourceId, self->priv->display, &imageRef, width, height, NULL, NULL, NULL, size);

    XFree((void *)imageRef);
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

    XImage *imageRef = NULL;
    gint width, height;
    gint x = 0, y = 0;
    size_t stride, size;

    if (src->sourceType == SOURCE_TYPE_DISPLAY && src->reportWindowLocations)
    {
        windowInfos = g_array_new(FALSE, FALSE, sizeof(WindowInfo));
        g_array_set_clear_func(windowInfos, window_info_clear);
        get_windows(src->sourceId, windowInfos);
    }

    // Decide on display vs. window capture and do the capture
    get_frame(src->sourceId, self->priv->display, &imageRef, &width, &height, &x, &y, &stride, &size);

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

    // Convert XImage to bitmap using memory of buffer
    memcpy(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0), imageRef->data, size);

out_frame:
    gst_video_frame_unmap(&frame);

out_buffer:
    gst_object_unref(pool);

out_capture:
    // Free memory
    if (imageRef)
        XDestroyImage(imageRef);

    GST_DEBUG_OBJECT(src, "Done creating buffer");
    return ret;
}

void gst_screen_cap_util_initialize(GstScreenCapUtil *self, GstScreenCapSrc *src)
{
    if (self->initialized)
        return;

    self->priv->display = XOpenDisplay(NULL);

    self->initialized = TRUE;
}

void gst_screen_cap_util_finalize(GstScreenCapUtil *self)
{
    if (!self->initialized)
        return;

    XCloseDisplay(self->priv->display);

    self->initialized = FALSE;
}

// Object instantiation
static void gst_screen_cap_util_init(GstScreenCapUtil *self)
{
    // Init private data
    self->priv = g_new0(GstScreenCapUtilPrivate, 1);

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
