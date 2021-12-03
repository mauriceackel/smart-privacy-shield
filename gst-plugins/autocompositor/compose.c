#include <gst/gst.h>
#include <gst/video/video.h>
#include <math.h>
#include "compose.h"

typedef struct _PlacementPoint PlacementPoint;
struct _PlacementPoint
{
    gint x, y;
    gint maxWidth, maxHeight;
};

static gint sort_frames_area_desc(CompositionData *a, CompositionData *b)
{
    return b->width * b->height - a->width * a->height;
}

static void update_max_bounds(PlacementPoint *point, CompositionData *frame)
{
    // Update maxWidth, if frame is to the right of the point and the frame's bottom edge is below the point
    if (frame->x > point->x && frame->y + frame->height > point->y)
    {
        point->maxWidth = MIN(point->maxWidth, frame->x - point->x);
    }
    // Update maxHeight, if frame is below the point and the frame's right edge is to the right of the point
    if (frame->y > point->y && frame->x + frame->width > point->x)
    {
        point->maxHeight = MIN(point->maxHeight, frame->y - point->y);
    }
}

static void update_generated_placement_points(GArray *placementPoints, GArray *placedFrames)
{
    for (gint i = 0; i < placementPoints->len; i++)
    {
        PlacementPoint point = g_array_index(placementPoints, PlacementPoint, i);

        for (gint j = 0; j < placedFrames->len; j++)
        {
            CompositionData frame = g_array_index(placedFrames, CompositionData, j);

            update_max_bounds(&point, &frame);
        }
    }
}

static void filter_placement_points(GArray *placementPoints, GArray *placedFrames)
{
    for (gint i = 0; i < placementPoints->len; i++)
    {
        PlacementPoint point = g_array_index(placementPoints, PlacementPoint, i);
        gboolean pointInvalid = FALSE;

        for (gint j = 0; j < placedFrames->len; j++)
        {
            CompositionData frame = g_array_index(placedFrames, CompositionData, j);

            // If the point collides with some placed frame's top edge it is invalid
            if (point.y == frame.y && point.x >= frame.x && point.x < frame.x + frame.width)
            {
                pointInvalid = TRUE;
                break;
            }

            // If the point collides with some placed frame's left edge or left edge it is invalid
            if (point.x == frame.x && point.y >= frame.y && point.y < frame.y + frame.height)
            {
                pointInvalid = TRUE;
                break;
            }

            // In any other way, update bounds
            update_max_bounds(&point, &frame);
        }

        // Remove if invalid
        if (pointInvalid)
        {
            g_array_remove_index(placementPoints, i);
            i--; // Reduce index by one so we don't skip the next point
        }
    }
}

static gint compute_overlap(PlacementPoint *placementPoint, CompositionData *frame, GArray *placedFrames)
{
    gint overlap = 0;

    for (gint i = 0; i < placedFrames->len; i++)
    {
        CompositionData placedFrame = g_array_index(placedFrames, CompositionData, i);

        // Check if placement point is on bottom edge of rectangle
        if (placedFrame.y + placedFrame.height == placementPoint->y)
        {
            // distance placed frame right edge to point - distance placed frame right edge to end of frame
            overlap += MAX(placedFrame.x + placedFrame.width - placementPoint->x, 0) - MAX(placedFrame.x + placedFrame.width - placementPoint->x - frame->width, 0);
        }

        // Check if placement point is on right edge of rectangle
        if (placedFrame.x + placedFrame.width == placementPoint->x)
        {
            // distance placed frame bottom edge to point - distance placed frame bottom edge to end of frame
            overlap += MAX(placedFrame.y + placedFrame.height - placementPoint->y, 0) - MAX(placedFrame.y + placedFrame.height - placementPoint->y - frame->height, 0);
        }
    }

    return overlap;
}

// Compute the best position of each frame given a heuristic
void compute_positioning(GArray *frames, gdouble targetRatio)
{
    gfloat finalWidth, finalHeight;
    finalWidth = targetRatio;
    finalHeight = 1;

    gint maxX, maxY; // Current max x/y
    maxX = maxY = INT_MIN;

    GArray *placementPoints = g_array_new(FALSE, FALSE, sizeof(PlacementPoint));
    PlacementPoint initial = {
        .x = 0,
        .y = 0,
        .maxWidth = INT_MAX,
        .maxHeight = INT_MAX,
    };
    g_array_append_val(placementPoints, initial);
    GArray *placedFrames = g_array_new(FALSE, FALSE, sizeof(CompositionData));

    // Perform placement
    g_array_sort(frames, (GCompareFunc)sort_frames_area_desc);

    // Run through all rectangles in a greedy manner
    for (gint i = 0; i < frames->len; i++)
    {
        CompositionData *frame = &g_array_index(frames, CompositionData, i);

        PlacementPoint *bestPlacementPoint;
        gfloat bestScaleFactor;
        gint bestEdgeOverlap;
        bestScaleFactor = FLT_MAX;
        bestEdgeOverlap = INT_MIN;

        // Check at which point to place the rectangle
        for (gint j = 0; j < placementPoints->len; j++)
        {
            PlacementPoint *point = &g_array_index(placementPoints, PlacementPoint, j);

            // Ignore points that can't fit the rectangle
            if (point->maxHeight < frame->height || point->maxWidth < frame->width)
                continue;

            // Compute the worst-case scale factor for placing at this placement point
            gfloat combinedWidth = point->x + frame->width;
            gfloat combinedHeight = point->y + frame->height;
            gfloat scaleFactor = fmax(1, fmax(combinedWidth / finalWidth, combinedHeight / finalHeight));

            // Decide whether this point is better than the current best
            if (scaleFactor < bestScaleFactor)
            {
                // Better scale factor always outrules the rest
                bestPlacementPoint = point;
                bestScaleFactor = scaleFactor;
                bestEdgeOverlap = compute_overlap(point, frame, placedFrames);
            }
            else if (scaleFactor == bestScaleFactor)
            {
                // In case two points are similar in scale factor:
                if (point->x < bestPlacementPoint->x && point->y <= bestPlacementPoint->y)
                {
                    // Clearly better as no compromise in y needs to be made
                    bestPlacementPoint = point;
                    bestEdgeOverlap = compute_overlap(point, frame, placedFrames);
                }
                else if (point->y < bestPlacementPoint->y && point->x <= bestPlacementPoint->x)
                {
                    // Clearly better as no compromise in x needs to be made
                    bestPlacementPoint = point;
                    bestEdgeOverlap = compute_overlap(point, frame, placedFrames);
                }
                else if (point->x < bestPlacementPoint->x || point->y < bestPlacementPoint->y)
                {
                    // Better in one than the over, check for better edge overlap
                    gint edgeOverlap = compute_overlap(point, frame, placedFrames);
                    if (edgeOverlap > bestEdgeOverlap)
                    {
                        bestPlacementPoint = point;
                        bestEdgeOverlap = edgeOverlap;
                    }
                }
            }
        }

        // Best point is found, apply to frame
        frame->x = bestPlacementPoint->x;
        frame->y = bestPlacementPoint->y;

        // Rescale target
        finalWidth *= bestScaleFactor;
        finalHeight *= bestScaleFactor;

        // Generate new placement points
        GArray *generatedPoints = g_array_new(FALSE, FALSE, sizeof(PlacementPoint));
        gint combinedX = frame->x + frame->width;
        gint combinedY = frame->y + frame->height;

        // Add the default three corners of the new rectangle
        g_array_append_val(generatedPoints, ((PlacementPoint){.x = frame->x, .y = combinedY, .maxWidth = INT_MAX, .maxHeight = INT_MAX}));  // Bottom left
        g_array_append_val(generatedPoints, ((PlacementPoint){.x = combinedX, .y = frame->y, .maxWidth = INT_MAX, .maxHeight = INT_MAX}));  // Top right
        g_array_append_val(generatedPoints, ((PlacementPoint){.x = combinedX, .y = combinedY, .maxWidth = INT_MAX, .maxHeight = INT_MAX})); // Bottom right
        // If the placed rectangle exceeds the current max width/height, add additional placement points at the intersection with the borders
        if (combinedX > maxX)
        {
            maxX = combinedX;
            if (frame->y != 0) // Prevent adding a point we added above already
                g_array_append_val(generatedPoints, ((PlacementPoint){.x = combinedX, .y = 0, .maxHeight = INT_MAX, .maxWidth = INT_MAX}));
        }
        if (combinedY > maxY)
        {
            maxY = combinedY;
            if (frame->x != 0) // Prevent adding a point we added above already
                g_array_append_val(generatedPoints, ((PlacementPoint){.x = 0, .y = combinedY, .maxHeight = INT_MAX, .maxWidth = INT_MAX}));
        }

        // Filter and update placement points
        update_generated_placement_points(generatedPoints, placedFrames); // Placed frames without current frame so the points are not affected by the rectangle they belong to
        g_array_append_vals(placedFrames, frame, 1);
        filter_placement_points(placementPoints, placedFrames); // Placed frames with current frame to update all placement points given the status quo
        g_array_append_vals(placementPoints, generatedPoints->data, generatedPoints->len);
    }

    // Free memory
    g_array_free(placementPoints, TRUE);
    g_array_free(placedFrames, TRUE);
}
