#include <gst/gst.h>
#include <changemeta.h>
#include "gstobjdetection.h"
#include "inferencedata.h"
#include "inferenceutil.h"

#define LATENCY 2000000000 // nanoseconds processing latency = time for inference
#define THREAD_POOL_SIZE 4

GST_DEBUG_CATEGORY(gst_obj_detection_debug);

enum
{
    PROP_0,
    PROP_MODEL_PATH,
    PROP_PREFIX,
    PROP_ACTIVE,
    PROP_LABELS,
};

GstStaticPadTemplate obj_detection_src_template = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

GstStaticPadTemplate obj_detection_bypass_sink_template = GST_STATIC_PAD_TEMPLATE("bypass_sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

GstStaticPadTemplate obj_detection_model_sink_template = GST_STATIC_PAD_TEMPLATE("model_sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                                 GST_STATIC_CAPS("video/x-raw, format = (string) BGRA"));

#define gst_obj_detection_parent_class parent_class
G_DEFINE_TYPE(GstObjDetection, gst_obj_detection, GST_TYPE_ELEMENT);

// Reset detection when model changed
static void gst_obj_detection_reset(GstObjDetection *filter)
{
    // Reinit inference
    gst_inference_util_reinitialize(filter->inferenceUtil, filter);

    // Clear last detection -> forces redetection
    if (filter->lastInferenceData)
        g_object_unref(filter->lastInferenceData);
    filter->lastInferenceData = NULL;
}

// Property setter
static void gst_obj_detection_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstObjDetection *filter = GST_OBJ_DETECTION(object);

    switch (prop_id)
    {
    case PROP_MODEL_PATH:
        filter->modelPath = g_value_dup_string(value);
        gst_obj_detection_reset(filter);
        break;
    case PROP_PREFIX:
        filter->prefix = g_value_dup_string(value);
        break;
    case PROP_ACTIVE:
        filter->active = g_value_get_boolean(value);
        break;
    case PROP_LABELS:
    {
        // Clear array
        while (filter->labels->len)
            g_ptr_array_remove_index(filter->labels, 0);

        gint size = gst_value_array_get_size(value);
        for (gint i = 0; i < size; i++)
        {
            const GValue *val = gst_value_array_get_value(value, i);
            g_ptr_array_add(filter->labels, g_value_dup_string(val));
        }
    }
    break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

// Property setter
static void gst_obj_detection_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstObjDetection *filter = GST_OBJ_DETECTION(object);

    switch (prop_id)
    {
    case PROP_MODEL_PATH:
        g_value_set_string(value, filter->modelPath);
        break;
    case PROP_PREFIX:
        g_value_set_string(value, filter->prefix);
        break;
    case PROP_ACTIVE:
        g_value_set_boolean(value, filter->active);
        break;
    case PROP_LABELS:
    {
        for (gint i = 0; i < filter->labels->len; i++)
        {
            GValue val = G_VALUE_INIT;
            g_value_init(&val, G_TYPE_STRING);
            g_value_set_string(&val, g_ptr_array_index(filter->labels, i));

            gst_value_array_append_value(value, &val);
            g_value_unset(&val);
        }
    }
    break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

// Perform inference on a pair of model and bypass buffer
static void gst_obj_detection_inference(gpointer data, gpointer self)
{
    GstObjDetection *filter = GST_OBJ_DETECTION(self);
    GstInferenceData *inferenceData = (GstInferenceData *)data;
    gboolean success;

    GST_DEBUG_OBJECT(filter, "Starting inference thread");

    success = gst_inference_util_run_inference(filter->inferenceUtil, filter, inferenceData);

    inferenceData->error = !success;
    inferenceData->processed = TRUE;
    GST_DEBUG_OBJECT(filter, "Finished inference thread");
}

// Prepare to start processing -> open ressources, etc.
static gboolean gst_obj_detection_start(GstObjDetection *filter)
{
    gboolean ret = TRUE;

    if (filter->prefix == NULL)
    {
        GST_ELEMENT_ERROR(filter, RESOURCE, NOT_FOUND, ("Prefix has not been set"), (NULL));
        ret = FALSE;
        goto out;
    }

    // Init inference
    gst_inference_util_initialize(filter->inferenceUtil, filter);

    // Setup thread pool
    filter->threadPool = g_thread_pool_new(gst_obj_detection_inference, (gpointer)filter, THREAD_POOL_SIZE, FALSE, NULL);

    // Start collect pads
    gst_collect_pads_start(filter->collectPads);

out:
    return ret;
}

// Stop processing -> free ressources
static gboolean gst_obj_detection_stop(GstObjDetection *filter)
{
    // Stop collect pads
    gst_collect_pads_stop(filter->collectPads);

    // Stop all threads and wait for them to finish
    g_thread_pool_free(filter->threadPool, TRUE, TRUE);

    // Flush queue
    GstInferenceData *data;
    g_mutex_lock(&filter->mtxProcessingQueue);
    while ((data = (GstInferenceData *)g_queue_pop_head(filter->processingQueue)))
    {
        gst_buffer_unref(data->bypassBuffer);
        gst_buffer_unref(data->modelBuffer);
        g_object_unref(data);
    }
    g_mutex_unlock(&filter->mtxProcessingQueue);

    // Clear up last inference
    if (filter->lastInferenceData)
        g_object_unref(filter->lastInferenceData);
    filter->lastInferenceData = NULL;

    // Finalize inference
    gst_inference_util_finalize(filter->inferenceUtil);

    return TRUE;
}

// Called whenever both sinks have a buffer queued
static GstFlowReturn gst_obj_detection_aggregate_function_parallel(GstCollectPads *pads, gpointer self)
{
    GstObjDetection *filter = GST_OBJ_DETECTION(self);
    GstFlowReturn ret = GST_FLOW_OK;

    GstBuffer *modelBuffer, *bypassBuffer;
    GstInferenceData *data, *lastInferenceData = NULL;

    // Get queued buffers
    modelBuffer = gst_collect_pads_pop(pads, filter->modelSinkData);
    bypassBuffer = gst_collect_pads_pop(pads, filter->bypassSinkData);
    if (modelBuffer == NULL && bypassBuffer == NULL)
    {
        GST_DEBUG_OBJECT(filter, "Both buffers empty, pushing EOS");
        ret = GST_FLOW_EOS;
        goto out;
    }
    GST_DEBUG_OBJECT(filter, "Collected modelBuffer: %p and bypassBuffer: %p", modelBuffer, bypassBuffer);

    // Make writeable
    bypassBuffer = gst_buffer_make_writable(bypassBuffer);

    // Drop buffers if there are too many unprocessed tasks
    if (g_thread_pool_unprocessed(filter->threadPool) > 2)
    {
        // TODO: We should drop buffers from the front of the queue, as otherwise the delay will continue to be there (i.e. buffers will come late all the time)
        // TODO: We would need to abort the threads though -> bad
        GST_WARNING_OBJECT(filter, "Too many unprocessed frames. Dropping buffers.");
        gst_buffer_unref(modelBuffer);
        gst_buffer_unref(bypassBuffer);
        goto process_queue;
    }

    // Prepare for parallel processing
    data = gst_inference_data_new();
    data->bypassBuffer = bypassBuffer;
    data->modelBuffer = modelBuffer;
    data->processed = FALSE;

    // Get local reference to last inference data
    lastInferenceData = filter->lastInferenceData;
    filter->lastInferenceData = g_object_ref(data);

    // Add inference data to queue
    g_mutex_lock(&filter->mtxProcessingQueue);
    g_queue_push_tail(filter->processingQueue, (gpointer)data);
    g_mutex_unlock(&filter->mtxProcessingQueue);

    // Early return when not active
    if (!filter->active)
    {
        // Simply pass through data
        data->processed = TRUE;
        data->error = FALSE;
        goto process_queue;
    }

    // Check if inference is needed
    GstChangeMeta *changeMeta = GST_CHANGE_META_GET(bypassBuffer);
    if (changeMeta)
    {
        if (!changeMeta->changed && lastInferenceData)
        {
            GST_DEBUG_OBJECT(filter, "No change, reusing detections");
            inference_couple(lastInferenceData, data);
            data->processed = TRUE;
            goto process_queue;
        }
    }
    else
        GST_DEBUG_OBJECT(filter, "No change meta");

    // Start processing
    GST_DEBUG_OBJECT(filter, "Dispatching new processing thread");
    g_thread_pool_push(filter->threadPool, (gpointer)data, NULL);

process_queue:
    // Output processed buffers
    while (TRUE)
    {
        g_mutex_lock(&filter->mtxProcessingQueue);
        data = (GstInferenceData *)g_queue_peek_head(filter->processingQueue);

        // Stop if there is no more data in the buffer or the most recent data has not finished processing
        if (data == NULL || !data->processed)
        {
            g_mutex_unlock(&filter->mtxProcessingQueue);
            goto out;
        }

        // Remove from queue
        data = (GstInferenceData *)g_queue_pop_head(filter->processingQueue);
        g_mutex_unlock(&filter->mtxProcessingQueue);

        // Apply detections (i.e. add to metadata)
        data->error |= !inference_apply(filter, data);

        // Ignore buffers with errors
        if (data->error)
            continue;

        // Push processed bypass buffer
        GST_LOG_OBJECT(filter, "Returning processed data %" GST_PTR_FORMAT, data);
        gst_buffer_unref(data->modelBuffer);
        ret = gst_pad_push(filter->source, data->bypassBuffer);

        // Only free previous data as current data might be required by the next frame
        g_object_unref(data);

        // Abort on error
        if (G_UNLIKELY(ret != GST_FLOW_OK))
            goto out;
    }

out:
    if (lastInferenceData)
        g_object_unref(lastInferenceData);

    // Reached EOS send event downstream
    if (ret == GST_FLOW_EOS)
        gst_pad_push_event(filter->source, gst_event_new_eos());

    return ret;
}

// Called whenever both sinks have a buffer queued
static GstFlowReturn gst_obj_detection_aggregate_function(GstCollectPads *pads, gpointer self)
{
    GstObjDetection *filter = GST_OBJ_DETECTION(self);
    GstFlowReturn ret = GST_FLOW_OK;

    GstBuffer *modelBuffer, *bypassBuffer;
    GstInferenceData *data, *lastInferenceData = NULL;

    // Get queued buffers
    modelBuffer = gst_collect_pads_pop(pads, filter->modelSinkData);
    bypassBuffer = gst_collect_pads_pop(pads, filter->bypassSinkData);
    if (modelBuffer == NULL && bypassBuffer == NULL)
    {
        GST_DEBUG_OBJECT(filter, "Both buffers empty, pushing EOS");
        ret = GST_FLOW_EOS;
        goto out;
    }
    GST_DEBUG_OBJECT(filter, "Collected modelBuffer: %p and bypassBuffer: %p", modelBuffer, bypassBuffer);

    // Make writeable
    bypassBuffer = gst_buffer_make_writable(bypassBuffer);

    // Prepare for inference
    data = gst_inference_data_new();
    data->bypassBuffer = bypassBuffer;
    data->modelBuffer = modelBuffer;
    data->processed = FALSE;

    // Get local reference to last inference data
    lastInferenceData = filter->lastInferenceData;
    filter->lastInferenceData = g_object_ref(data);

    // Early return when not active
    if (!filter->active)
    {
        // Simply pass through data
        data->processed = TRUE;
        data->error = FALSE;
        goto output_buffer;
    }

    // Check if inference is needed
    GstChangeMeta *changeMeta = GST_CHANGE_META_GET(bypassBuffer);
    if (changeMeta)
    {
        if (!changeMeta->changed && lastInferenceData)
        {
            GST_DEBUG_OBJECT(filter, "No change, reusing detections");
            inference_couple(lastInferenceData, data);
            data->processed = TRUE;
            goto output_buffer;
        }
    }
    else
        GST_DEBUG_OBJECT(filter, "No change meta");

    // Start processing
    gboolean success = gst_inference_util_run_inference(filter->inferenceUtil, filter, data); // TODO: Think about only processing the last buffer on change
    data->error = !success;

output_buffer:
    // Apply detections (i.e. add to metadata)
    data->error |= !inference_apply(filter, data);

    // Ignore buffers with errors
    if (data->error)
        goto skip_processing;

    // Push processed bypass buffer
    GST_LOG_OBJECT(filter, "Returning processed data %" GST_PTR_FORMAT, data);
    gst_buffer_unref(data->modelBuffer);
    ret = gst_pad_push(filter->source, data->bypassBuffer);

skip_processing:
    // Free data
    g_object_unref(data);

    if (lastInferenceData)
        g_object_unref(lastInferenceData);

out:
    // Reached EOS send event downstream
    if (ret == GST_FLOW_EOS)
        gst_pad_push_event(filter->source, gst_event_new_eos());

    return ret;
}

// Forward event from source to sinks
static gboolean gst_obj_detection_src_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    GstObjDetection *filter = GST_OBJ_DETECTION(parent);

    return gst_collect_pads_src_event_default(filter->collectPads, pad, event);
}

// Forward event from bypass sink to source
static gboolean gst_obj_detection_sink_event(GstCollectPads *pads, GstCollectData *data, GstEvent *event, gpointer self)
{
    GstObjDetection *filter = GST_OBJ_DETECTION(self);
    gboolean ret;

    // Handle some of the events that are dropped by the collect pads default event handler but need to be forwarded
    if (data->pad == filter->bypassSink)
    {
        switch (event->type)
        {
        case GST_EVENT_STREAM_START:
        case GST_EVENT_CAPS:
        case GST_EVENT_SEGMENT:
        {
            gst_event_ref(event); // Ref the event so that it will still be available in collect pads handler
            gst_pad_event_default(data->pad, GST_OBJECT(filter), event);
        }
        break;
        default:
            break;
        }
    }

    // Invoke default handler
    ret = gst_collect_pads_event_default(filter->collectPads, data, event, FALSE);

out:
    return ret;
}

// Handle sink queries
static gboolean gst_obj_detection_sink_query(GstCollectPads *pads, GstCollectData *data, GstQuery *query, gpointer self)
{
    GstObjDetection *filter = GST_OBJ_DETECTION(self);
    gboolean ret = TRUE;

    if (data->pad == filter->modelSink && query->type == GST_QUERY_CAPS && filter->modelSinkCaps)
    {
        GstCaps *queryCaps, *resultCaps, *tmp;
        gst_query_parse_caps(query, &queryCaps);

        resultCaps = gst_caps_copy(filter->modelSinkCaps);

        // Filter against the query filter when needed
        if (queryCaps)
        {
            tmp = gst_caps_intersect(queryCaps, resultCaps);
            gst_caps_unref(resultCaps);
            resultCaps = tmp;
        }

        gst_query_set_caps_result(query, resultCaps);
        gst_caps_unref(resultCaps);
        goto out;
    }

    ret = gst_collect_pads_query_default(filter->collectPads, data, query, FALSE);

out:
    return ret;
}

// Manage queries on source pad
static gboolean gst_obj_detection_src_query(GstPad *pad, GstObject *parent, GstQuery *query)
{
    GstObjDetection *filter = GST_OBJ_DETECTION(parent);
    gboolean ret = TRUE;

    switch (query->type)
    {
    case GST_QUERY_LATENCY:
    {
        GST_DEBUG_OBJECT(filter, "Received latency query");
        GstPad *peer;
        if (!(peer = gst_pad_get_peer(filter->bypassSink)))
        {
            GST_ERROR_OBJECT(filter, "Unable to get peer pad to forward latency query.");
            ret = FALSE;
            goto out;
        }

        if (gst_pad_query(peer, query))
        {

            GstClockTime min, max;
            gboolean live;
            gst_query_parse_latency(query, &live, &min, &max);

            GST_DEBUG_OBJECT(filter, "Peer latency: min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT, GST_TIME_ARGS(min), GST_TIME_ARGS(max));
            GST_DEBUG_OBJECT(filter, "Our latency: %" GST_TIME_FORMAT, GST_TIME_ARGS(LATENCY));

            min += LATENCY;
            if (max != GST_CLOCK_TIME_NONE)
                max += LATENCY;

            GST_DEBUG_OBJECT(filter, "Calculated total latency : min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT, GST_TIME_ARGS(min), GST_TIME_ARGS(max));

            gst_query_set_latency(query, live, min, max);
        }
        else
        {
            GST_ERROR_OBJECT(filter, "Forwarded latency query failed on peer pad.");
            ret = FALSE;
        }

        gst_object_unref(peer);
    }
    break;
    default:
        ret = gst_pad_query_default(pad, parent, query);
    }

out:
    return ret;
}

// Handle state change
static GstStateChangeReturn gst_obj_detection_change_state(GstElement *element, GstStateChange transition)
{
    GstStateChangeReturn ret;
    GstObjDetection *filter = GST_OBJ_DETECTION(element);

    switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        if (!gst_obj_detection_start(filter))
        {
            GST_ERROR_OBJECT(filter, "Failed to start");
            ret = GST_STATE_CHANGE_FAILURE;
            goto out;
        }
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        if (!gst_obj_detection_stop(filter))
        {
            GST_ERROR_OBJECT(filter, "Failed to stop");
            ret = GST_STATE_CHANGE_FAILURE;
            goto out;
        }
        break;
    default:
        break;
    }

    ret = GST_ELEMENT_CLASS(gst_obj_detection_parent_class)->change_state(element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        GST_ERROR_OBJECT(filter, "Parent failed to change state");
        goto out;
    }

    GST_DEBUG_OBJECT(filter, "Changed state %i", transition);

out:
    return ret;
}

// Gets internally linked pads of source and bypass sink
static GstIterator *gst_obj_detection_iterate_internal_links(GstPad *pad, GstObject *parent)
{
    GstObjDetection *filter = GST_OBJ_DETECTION(parent);
    GstIterator *it = NULL;
    GstPad *peerPad;

    if (pad == filter->source)
    {
        peerPad = filter->bypassSink;
    }
    else if (pad == filter->bypassSink)
    {
        peerPad = filter->source;
    }
    else
    {
        return NULL;
    }

    GValue value = G_VALUE_INIT;

    g_value_init(&value, GST_TYPE_PAD);
    g_value_set_object(&value, peerPad);
    it = gst_iterator_new_single(GST_TYPE_PAD, &value);
    g_value_unset(&value);

    return it;
}

// Renegotiate sink caps
void gst_obj_detection_reconfigure_model_sink(GstObjDetection *filter, gint modelProportions)
{
    if (!filter->modelSinkCaps)
        return;

    GstCaps *newCaps;
    GstQuery *acceptCapsQuery;

    // Create new caps
    newCaps = gst_caps_copy(filter->modelSinkCaps);
    gst_caps_set_simple(newCaps, "width", G_TYPE_INT, modelProportions, "height", G_TYPE_INT, modelProportions, NULL);

    if (gst_caps_is_equal(filter->modelSinkCaps, newCaps))
        GST_DEBUG_OBJECT(filter, "New caps equal to old ones: %" GST_PTR_FORMAT, newCaps);
    else
        gst_caps_replace(&filter->modelSinkCaps, newCaps);

    // Check if upstream would accept new caps
    acceptCapsQuery = gst_query_new_accept_caps(newCaps);
    if (gst_pad_peer_query(filter->modelSink, acceptCapsQuery))
    {
        gboolean result;
        gst_query_parse_accept_caps_result(acceptCapsQuery, &result);
        if (!result)
        {
            GST_ERROR_OBJECT(filter, "Model sink peer pad did not handle accept caps query");
            goto out;
        }
    }
    else
    {
        GST_WARNING_OBJECT(filter, "Model sink peer pad did not handle accept caps query");
        goto out;
    }

    // Request upstream to reconfigure caps
    if (!gst_pad_push_event(filter->modelSink, gst_event_new_reconfigure()))
        GST_DEBUG_OBJECT(filter, "Model sink renegotiate event wasn't handled");

out:
    if (newCaps)
        gst_caps_unref(newCaps);
    if (acceptCapsQuery)
        gst_query_unref(acceptCapsQuery);
}

// Object constructor -> called for every instance
static void gst_obj_detection_init(GstObjDetection *filter)
{
    // Setup collect pads
    filter->collectPads = gst_collect_pads_new();
    gst_collect_pads_set_event_function(filter->collectPads, gst_obj_detection_sink_event, (gpointer)filter);
    gst_collect_pads_set_query_function(filter->collectPads, gst_obj_detection_sink_query, (gpointer)filter);
    gst_collect_pads_set_function(filter->collectPads, gst_obj_detection_aggregate_function, (gpointer)filter);

    // Setup sink pads
    filter->modelSink = gst_pad_new_from_static_template(&obj_detection_model_sink_template, "model_sink");
    gst_element_add_pad(GST_ELEMENT(filter), filter->modelSink);

    filter->bypassSink = gst_pad_new_from_static_template(&obj_detection_bypass_sink_template, "bypass_sink");
    gst_pad_set_iterate_internal_links_function(filter->bypassSink, gst_obj_detection_iterate_internal_links);
    gst_element_add_pad(GST_ELEMENT(filter), filter->bypassSink);

    // Setup source pad
    filter->source = gst_pad_new_from_static_template(&obj_detection_src_template, "src");
    gst_pad_set_iterate_internal_links_function(filter->source, gst_obj_detection_iterate_internal_links); // Required for proxy
    gst_pad_set_query_function(filter->source, gst_obj_detection_src_query);
    gst_element_add_pad(GST_ELEMENT(filter), filter->source);

    // Add sinks to collect pad. ORDER IMPORTANT as it dictates the order of the buffer_function callback
    filter->bypassSinkData = gst_collect_pads_add_pad(filter->collectPads, filter->bypassSink, sizeof(GstCollectData), NULL, TRUE);
    filter->modelSinkData = gst_collect_pads_add_pad(filter->collectPads, filter->modelSink, sizeof(GstCollectData), NULL, TRUE);

    // Set source and bypass sink to proxy mode
    GST_PAD_SET_PROXY_CAPS(filter->source);
    GST_PAD_SET_PROXY_ALLOCATION(filter->source);
    GST_PAD_SET_PROXY_SCHEDULING(filter->source);
    GST_PAD_SET_PROXY_CAPS(filter->bypassSink);
    GST_PAD_SET_PROXY_ALLOCATION(filter->bypassSink);
    GST_PAD_SET_PROXY_SCHEDULING(filter->bypassSink);

    // Set property initial value
    filter->modelPath = NULL;
    filter->prefix = NULL;
    filter->active = TRUE;

    filter->labels = g_ptr_array_new_with_free_func(g_free);

    filter->lastInferenceData = NULL;
    filter->processingQueue = g_queue_new();
    g_mutex_init(&filter->mtxProcessingQueue);

    // Init modelSinkCaps
    filter->modelSinkCaps = gst_static_pad_template_get_caps(&obj_detection_model_sink_template);

    // Init inference utils
    filter->inferenceUtil = gst_inference_util_new();
}

// Object destructor -> called if an object gets destroyed
static void gst_obj_detection_finalize(GObject *object)
{
    GstObjDetection *filter = GST_OBJ_DETECTION(object);

    g_free((void *)filter->modelPath);
    g_free((void *)filter->prefix);

    g_ptr_array_free(filter->labels, TRUE);

    g_mutex_clear(&filter->mtxProcessingQueue);
    g_queue_free(filter->processingQueue);

    g_clear_object(&(filter->collectPads));
    g_clear_object(&(filter->bypassSink));
    g_clear_object(&(filter->modelSink));
    g_clear_object(&(filter->source));

    g_object_unref(filter->inferenceUtil);

    // Clear modelSinkCaps
    if (filter->modelSinkCaps)
    {
        gst_caps_unref(filter->modelSinkCaps);
        filter->modelSinkCaps = NULL;
    }

    G_OBJECT_CLASS(gst_obj_detection_parent_class)->finalize(object);
}

// Class constructor -> called exactly once
static void gst_obj_detection_class_init(GstObjDetectionClass *klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    gobject_class = (GObjectClass *)klass;
    gstelement_class = (GstElementClass *)klass;

    // Set function pointers on this class
    gobject_class->finalize = gst_obj_detection_finalize;
    gobject_class->set_property = gst_obj_detection_set_property;
    gobject_class->get_property = gst_obj_detection_get_property;

    // Define properties of plugin
    g_object_class_install_property(gobject_class, PROP_MODEL_PATH,
                                    g_param_spec_string("model-path", "Model Path",
                                                        "Path to the model for detection", NULL,
                                                        G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_PREFIX,
                                    g_param_spec_string("prefix", "Prefix",
                                                        "The prefixed used to annotate detection labels", NULL,
                                                        G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_ACTIVE,
                                    g_param_spec_boolean("active", "Active",
                                                         "Whether detection is active or not",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_LABELS,
                                    gst_param_spec_array("labels", "Labels",
                                                         "List of the labels (in order) the specified model produces",
                                                         g_param_spec_string("label",
                                                                             "Label",
                                                                             "A label of the model",
                                                                             NULL,
                                                                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    // Set plugin metadata
    gst_element_class_set_static_metadata(gstelement_class,
                                          "Object detection filter", "Filter/Video",
                                          "Object detection filter", "Maurice Ackel <m.ackel@icloud.com>");

    // Set the pad templates
    gst_element_class_add_pad_template(gstelement_class,
                                       gst_static_pad_template_get(&obj_detection_src_template));
    gst_element_class_add_pad_template(gstelement_class,
                                       gst_static_pad_template_get(&obj_detection_model_sink_template));
    gst_element_class_add_pad_template(gstelement_class,
                                       gst_static_pad_template_get(&obj_detection_bypass_sink_template));

    // Set function pointers to implementation of base class
    gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_obj_detection_change_state);
}

// Plugin initializer that registers the plugin
static gboolean plugin_init(GstPlugin *plugin)
{
    gboolean ret;

    GST_DEBUG_CATEGORY_INIT(gst_obj_detection_debug, "objdetection", 0, "objdetection debug");

    ret = gst_element_register(plugin, "objdetection", GST_RANK_NONE, GST_TYPE_OBJ_DETECTION);

    return ret;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  objdetection,
                  "Object detection filter plugin",
                  plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);