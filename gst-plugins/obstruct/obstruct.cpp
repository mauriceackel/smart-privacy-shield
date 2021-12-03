
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "obstruct.h"

extern "C"
{
#include <gst/gst.h>
#include <gst/video/video.h>
#include <detectionmeta.h>
}

using namespace cv;

gboolean apply_filter(GstVideoMeta *meta, guint8 *data, GstDetection *detection, FilterType filterType, gfloat margin)
{
    Mat frame;

    // Create Mat from the raw data
    switch (meta->format)
    {
    case GST_VIDEO_FORMAT_BGRA:
        frame = Mat(meta->height, meta->width, CV_8UC4, data);
        break;
    default:
        GST_DEBUG("Unsupported video format");
        return FALSE;
    }

    // Region to be filtered
    Rect region = Rect(detection->bbox.x + margin, detection->bbox.y + margin, detection->bbox.width - 2 * margin, detection->bbox.height - 2 * margin);
    region = region & cv::Rect(0, 0, meta->width, meta->height);
    if (region.width == 0 || region.height == 0)
    {
        GST_DEBUG("Invalid region");
        return TRUE; // Don't abort stream
    }

    // Apply filter effect in place
    switch (filterType)
    {
    case FILTER_TYPE_BLACK:
        frame(region).setTo(Scalar(0, 0, 0, 255));
        break;
    case FILTER_TYPE_WHITE:
        frame(region).setTo(Scalar(255, 255, 255, 255));
        break;
    case FILTER_TYPE_BLUR:
        cv::blur(
            frame(region),
            frame(region),
            Size(320, 320)
        );
        break;
    default:
        GST_DEBUG("Unsupported filter type");
        return FALSE;
    }

    return TRUE;
}
