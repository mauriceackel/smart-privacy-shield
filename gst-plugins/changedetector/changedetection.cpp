#include <gst/gst.h>
#include <gst/video/video.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "changedetection.h"
#include "changemeta.h"
#include <vector>

#define PIXEL_THRESHOLD 5

using namespace cv;

static void add_metadata(GstChangeDetector *filter, GstBuffer *buffer, gboolean changed, GArray *regions = NULL)
{
    GstChangeMeta *changeMeta;

    if (!gst_buffer_is_writable(buffer))
        return;

    changeMeta = GST_CHANGE_META_ADD(buffer);
    changeMeta->changed = changed;
    GST_DEBUG_OBJECT(filter, "Adding change meta to buffer %p. Changed: %i", buffer, changed);

    if (!regions)
        return;

    g_array_free(changeMeta->regions, TRUE);
    changeMeta->regions = regions;
}

void detect_changes(GstChangeDetector *filter, GstBuffer *currentBuffer, GstBuffer *lastBuffer)
{
    // Get video meta of both buffers
    GArray *regions;
    GstVideoMeta *videoMetaCurrent, *videoMetaLast;
    GstMapInfo mapInfoCurrent, mapInfoLast;
    Mat currentFrame, lastFrame, difference, channels[4], out;
    gfloat changedPixels, changedPixelRatio;

    videoMetaCurrent = (GstVideoMeta *)gst_buffer_get_meta(currentBuffer, GST_VIDEO_META_API_TYPE);
    if (videoMetaCurrent == NULL)
    {
        // Ignore buffer
        GST_ERROR_OBJECT(filter, "No video meta on current buffer");
        return;
    }

    if (!lastBuffer)
    {
        regions = g_array_new(FALSE, FALSE, sizeof(BoundingBox));
        BoundingBox fullScreen = {
            .x = 0,
            .y = 0,
            .width = (int)videoMetaCurrent->width,
            .height = (int)videoMetaCurrent->height,
        };
        g_array_append_val(regions, fullScreen);
        add_metadata(filter, currentBuffer, TRUE, regions);
        return;
    }

    videoMetaLast = (GstVideoMeta *)gst_buffer_get_meta(lastBuffer, GST_VIDEO_META_API_TYPE);
    if (videoMetaLast == NULL)
    {
        // Ignore buffer
        GST_ERROR_OBJECT(filter, "No video meta on last buffer");
        return;
    }

    // Check if dimensions are similar.
    if (videoMetaCurrent->width != videoMetaLast->width || videoMetaCurrent->height != videoMetaLast->height)
    {
        regions = g_array_new(FALSE, FALSE, sizeof(BoundingBox));
        BoundingBox fullScreen = {
            .x = 0,
            .y = 0,
            .width = (int)videoMetaCurrent->width,
            .height = (int)videoMetaCurrent->height,
        };
        g_array_append_val(regions, fullScreen);
        add_metadata(filter, currentBuffer, TRUE, regions);
        return;
    }

    // Map buffer memory
    gst_buffer_map(currentBuffer, &mapInfoCurrent, GST_MAP_READ);
    gst_buffer_map(lastBuffer, &mapInfoLast, GST_MAP_READ);

    // Create Mats from buffer memory
    currentFrame = Mat(Size(videoMetaCurrent->width, videoMetaCurrent->height), CV_8UC4, mapInfoCurrent.data);
    lastFrame = Mat(Size(videoMetaLast->width, videoMetaLast->height), CV_8UC4, mapInfoLast.data);

    // Computes per channel difference
    difference = Mat(Size(videoMetaCurrent->width, videoMetaCurrent->height), CV_8UC4);
    absdiff(currentFrame, lastFrame, difference);

    // Convert to single channel and merge to single array using max
    split(difference, channels);

    out = Mat(Size(videoMetaCurrent->width, videoMetaCurrent->height), CV_8UC1);
    max(channels[0], channels[1], out);
    max(out, channels[2], out);
    max(out, channels[3], out);

    // Perform thresholding
    threshold(out, out, PIXEL_THRESHOLD, 255, THRESH_BINARY);

    // Compute percentage of changed pixels
    changedPixels = (float)countNonZero(out);
    changedPixelRatio = changedPixels / (out.rows * out.cols);

    gst_buffer_unmap(currentBuffer, &mapInfoCurrent);
    gst_buffer_unmap(lastBuffer, &mapInfoLast);

    // No change detected. Return instantly
    if (changedPixelRatio <= filter->threshold)
    {
        add_metadata(filter, currentBuffer, FALSE);
        return;
    }

    // Get bounding boxes of regions
    regions = g_array_new(FALSE, FALSE, sizeof(BoundingBox));

    // Fill gaps
    Mat kernel = getStructuringElement(0, Size(21, 21), Point(10, 10));
    dilate(out, out, kernel);
    erode(out, out, kernel);

    // Extract contours
    std::vector<std::vector<Point>> contours;
    findContours(out, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    // Filter regions
    gfloat meanRegionArea = changedPixels / contours.size();
    for (size_t i = 0; i < contours.size(); i++)
    {
        // Ignore areas smaller than the theoretical average given the amount of contours
        if (contourArea(contours[i]) < meanRegionArea)
            continue;

        Rect rect = boundingRect(contours[i]);
        BoundingBox bbox = {
            .x = rect.x,
            .y = rect.y,
            .width = rect.width,
            .height = rect.height,
        };
        g_array_append_val(regions, bbox);
    }

    add_metadata(filter, currentBuffer, TRUE, regions);
out:
    return;
}