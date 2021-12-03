#include <sps/sps.h>
#include <gst/gst.h>
#include <gst/pbutils/encoding-profile.h>
#include "sourcedata.h"
#include "pipeline.h"

typedef struct _PipelineData PipelineData;
struct _PipelineData
{
    GstElement *pipeline, *predecessor, *successor, *element;
    GstPad *blockedPad;
    gulong blockingProbeId;
};

static void pipeline_file_pad_added(GstElement *element, GstPad *newPad, gpointer pipeline)
{
    // Get new pad's type
    GstCaps *newPadCaps = gst_pad_get_current_caps(newPad);
    GstStructure *newPadCapStruct = gst_caps_get_structure(newPadCaps, 0);
    const gchar *newPadType = gst_structure_get_name(newPadCapStruct);
    GST_DEBUG_OBJECT(element, "New detector pad with type %s\n", newPadType);

    if (g_str_has_prefix(newPadType, "audio/x-raw"))
    {
        // // Connect audio pad to audioconvert
        // GstElement *audioentry = gst_bin_get_by_name(GST_BIN(pipeline), "audioentry");
        // GstPad *audioentryPad = gst_element_get_static_pad(audioentry, "sink");

        // if (gst_pad_link(newPad, audioentryPad))
        // {
        //     GST_ERROR("Unable to link decoded audio to audioentry");
        // }

        // gst_object_unref(audioentry);
        // gst_object_unref(audioentryPad);

        goto out;
    }
    else if (g_str_has_prefix(newPadType, "video/x-raw"))
    {
        // Connect video pad to first video pipe entry element
        GstElement *videoentry = gst_bin_get_by_name(GST_BIN(pipeline), "videoentry");
        GstPad *videoentryPad = gst_element_get_static_pad(videoentry, "sink");

        if (gst_pad_link(newPad, videoentryPad))
        {
            GST_ERROR("Unable to link decoded video to videoentry");
        }

        gst_object_unref(videoentry);
        gst_object_unref(videoentryPad);

        goto out;
    }
    else
    {
        GST_ERROR_OBJECT(element, "Unexpected pad type from detector");
        goto out;
    }

out:
    // Unreference the new pad's caps
    if (newPadCaps != NULL)
        gst_caps_unref(newPadCaps);
}

static GstEncodingContainerProfile *create_ogg_profile(gboolean hasVideo, gboolean hasAudio)
{
    GstEncodingContainerProfile *containerProfile;

    GstCaps *profileCaps = gst_caps_from_string("application/ogg");
    containerProfile = gst_encoding_container_profile_new("ogg/vorbis", NULL, profileCaps, NULL);
    gst_caps_unref(profileCaps);

    if (hasAudio)
    {
        GstCaps *audioCaps = gst_caps_from_string("audio/x-vorbis");
        gst_encoding_container_profile_add_profile(containerProfile, (GstEncodingProfile *)gst_encoding_audio_profile_new(audioCaps, NULL, NULL, 0));
        gst_caps_unref(audioCaps);
    }

    if (hasVideo)
    {
        GstCaps *videoCaps = gst_caps_from_string("video/x-theora");
        gst_encoding_container_profile_add_profile(containerProfile, (GstEncodingProfile *)gst_encoding_video_profile_new(videoCaps, NULL, NULL, 0));
        gst_caps_unref(videoCaps);
    }

    return containerProfile;
}

// Create and manage file pipeline
GstElement *pipeline_file_create(SpsSourceData *sourceData, gboolean addDefaultElements)
{
    // Create pipeline
    GstElement *pipeline = gst_pipeline_new("file-pipeline");
    sourceData->pipeline = pipeline;

    // Add source
    GstElement *source = gst_element_factory_make("filesrc", "source");
    if (!source)
    {
        GST_ERROR("Unable to create source");
        return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), source);

    // Add decoder
    GstElement *decoder = gst_element_factory_make("decodebin", "decoder");
    if (!decoder)
    {
        GST_ERROR("Unable to create decoder");
        return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), decoder);
    g_signal_connect(decoder, "pad-added", G_CALLBACK(pipeline_file_pad_added), pipeline);
    if (!gst_element_link(source, decoder))
    {
        GST_ERROR("Unable to link source to decoder queue");
        return NULL;
    }

    // Add raw video converter
    GstElement *videoentry = gst_element_factory_make("videoconvert", "videoentry");
    if (!videoentry)
    {
        GST_ERROR("Unable to create videoentry");
        return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), videoentry);
    // Linking to decodebin happens after start

    // Preprocessors will be added here

    // Add decoupling queue (videoentry <-> preprocessors <-> preprocessorQueue)
    GstElement *preprocessorQueue = gst_element_factory_make("queue", "prepqueue");
    sourceData->preprocessorQueue = preprocessorQueue;
    if (!preprocessorQueue)
    {
        GST_ERROR("Unable to create preprocessor queue");
        return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), preprocessorQueue);
    if (!gst_element_link(videoentry, preprocessorQueue))
    {
        GST_ERROR("Unable to link video entry to preprocessorQueue");
        return NULL;
    }

    // Detectors will be added here

    // Add decoupling queue (preprocessorQueue <-> detectors <-> detectorQueue)
    GstElement *detectorQueue = gst_element_factory_make("queue", "detqueue");
    sourceData->detectorQueue = detectorQueue;
    if (!detectorQueue)
    {
        GST_ERROR("Unable to create detector queue");
        return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), detectorQueue);
    if (!gst_element_link(preprocessorQueue, detectorQueue))
    {
        GST_ERROR("Unable to link preprocessor to detector queue");
        return NULL;
    }

    // Postprocessors will be added here

    // Add decoupling queue (detectorQueue <-> postprocessors <-> postprocessorQueue)
    GstElement *postprocessorQueue = gst_element_factory_make("queue", "postpqueue");
    sourceData->postprocessorQueue = postprocessorQueue;
    if (!postprocessorQueue)
    {
        GST_ERROR("Unable to create postprocessor queue");
        return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), postprocessorQueue);
    if (!gst_element_link(detectorQueue, postprocessorQueue))
    {
        GST_ERROR("Unable to link detector to postprocessor queue");
        return NULL;
    }

    // Add encoder
    GstElement *encoder = gst_element_factory_make("encodebin", "encoder");
    if (!encoder)
    {
        GST_ERROR("Unable to create encoder");
        return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), encoder);
    GstEncodingContainerProfile *profile = create_ogg_profile(TRUE, FALSE);
    g_object_set(encoder, "profile", profile, NULL);

    // Link postprocessorQueue to encoder
    GstPad *postprocessorQueueSrcPad = gst_element_get_static_pad(postprocessorQueue, "src");
    GstPad *encoderVideoSinkPad = gst_element_get_request_pad(encoder, "video_%u");
    if (gst_pad_link(postprocessorQueueSrcPad, encoderVideoSinkPad))
    {
        GST_ERROR("Unable to link decoded audio to encoder");
    }
    gst_object_unref(encoderVideoSinkPad);
    gst_object_unref(postprocessorQueueSrcPad);

    // Add final output
    GstElement *sink = gst_element_factory_make("filesink", "sink");
    if (!sink)
    {
        GST_ERROR("Unable to create sink");
        return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), sink);
    if (!gst_element_link(encoder, sink))
    {
        GST_ERROR("Unable to link encoder to sink");
        return NULL;
    };

    if (!addDefaultElements)
        return pipeline;

    // Add default elements
    SpsRegistry *registry = sps_registry_get();
    GList *defaultPreprocessor = g_hash_table_get_values(registry->defaultPreprocessors);
    while (defaultPreprocessor != NULL)
    {
        pipeline_add_preprocessor((char *)defaultPreprocessor->data, sourceData);
        defaultPreprocessor = defaultPreprocessor->next;
    }

    GList *defaultDetector = g_hash_table_get_values(registry->defaultDetectors);
    while (defaultDetector != NULL)
    {
        pipeline_add_detector((char *)defaultDetector->data, sourceData);
        defaultDetector = defaultDetector->next;
    }

    GList *defaultPostprocessor = g_hash_table_get_values(registry->defaultPostprocessors);
    while (defaultPostprocessor != NULL)
    {
        pipeline_add_postprocessor((char *)defaultPostprocessor->data, sourceData);
        defaultPostprocessor = defaultPostprocessor->next;
    }

    return pipeline;
}

// Create and manage main pipe
GstElement *pipeline_main_create()
{
    // Create pipeline
    GstElement *pipeline = gst_pipeline_new("pipeline");

    // Add final compositor
    GstElement *compositor = gst_element_factory_make("autocompositor", "compositor");
    if (!compositor)
    {
        GST_ERROR("Unable to create final compositor");
        return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), compositor);

    // Add final output
    GstElement *output = gst_element_factory_make("gtksink", "sink");
    if (!output)
    {
        GST_ERROR("Unable to create output");
        return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), output);

    gst_element_link(compositor, output);

    return pipeline;
}

void pipeline_main_add_subpipe(GstElement *main, GstElement *subpipe)
{
    GstElement *compositor = gst_bin_get_by_name(GST_BIN(main), "compositor");

    // Add subpipe to pipeline
    gst_bin_add(GST_BIN(main), subpipe);

    // Link up
    if (!gst_element_link(subpipe, compositor))
        GST_ERROR("Unable to link subpipe to compositor");

    // Align states
    gst_element_set_state(subpipe, main->current_state);

    g_object_unref(compositor);
}

static gboolean cb_remove_subpipe(PipelineData *data)
{
    // Keep a reference so we can completely deinit
    gst_object_ref(data->element);

    // Prepare to release compositor's request pad
    GstPad *src, *srcPeer;
    src = gst_element_get_static_pad(data->element, "src");
    srcPeer = gst_pad_get_peer(src);

    // Unlink and remove request pad
    gst_element_unlink(data->element, data->successor);
    gst_element_release_request_pad(data->successor, srcPeer);

    // Destroy subpipe
    gst_bin_remove(GST_BIN(data->pipeline), data->element);
    gst_element_set_state(data->element, GST_STATE_NULL);

    gst_object_unref(src);
    gst_object_unref(srcPeer);
    gst_object_unref(data->element);
    g_free(data);

    return G_SOURCE_REMOVE;
}

static GstPadProbeReturn cb_event_probe_remove_subpipe(GstPad *pad, GstPadProbeInfo *info, PipelineData *data)
{
    // Only listen for EOS
    if (GST_EVENT_TYPE(GST_PAD_PROBE_INFO_DATA(info)) != GST_EVENT_EOS)
        return GST_PAD_PROBE_PASS;

    // Remove invoking probe
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));

    // Dispatch removal to the main thread to prevent a deadlock
    g_idle_add(G_SOURCE_FUNC(cb_remove_subpipe), (gpointer)data);

    return GST_PAD_PROBE_DROP;
}

void pipeline_main_remove_subpipe(GstElement *main, GstElement *subpipe)
{
    GstElement *compositor = gst_bin_get_by_name(GST_BIN(main), "compositor");

    // Get source pad of subpipe
    GstPad *src = gst_element_get_static_pad(subpipe, "src");
    PipelineData *data = g_malloc(sizeof(PipelineData));
    data->pipeline = main;
    data->element = subpipe;
    data->successor = compositor;

    if (data->pipeline->current_state < GST_STATE_PAUSED)
    {
        // Pad stopped, so we remove the element directly
        cb_remove_subpipe(data);
    }
    else
    {
        // Register EOS probe on the element that should be removed
        gst_pad_add_probe(src, GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, (GstPadProbeCallback)cb_event_probe_remove_subpipe, data, NULL); // Data freed in cb_remove_subpipe

        // Send EOS to element that should be removed (will be caught by the event probe before leaving)
        gst_element_send_event(subpipe, gst_event_new_eos());
    }

    gst_object_unref(src);
    gst_object_unref(compositor);
}

void pipeline_start(GstElement *pipeline)
{
    GstStateChangeReturn ret;

    switch (pipeline->current_state)
    {
    case GST_STATE_PLAYING:
    {
        ret = gst_element_set_state(pipeline, GST_STATE_PAUSED);
        if (ret == GST_STATE_CHANGE_FAILURE)
        {
            g_printerr("Unable to set the pipeline to paused state.\n");
            return;
        }
    }
    break;
    case GST_STATE_READY:
    case GST_STATE_PAUSED:
    {
        ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE)
        {
            g_printerr("Unable to set the pipeline to the playing state.\n");
            return;
        }
    }
    break;
    default:
        g_printerr("Unexpected element state %i\n", pipeline->current_state);
        break;
    }
}

void pipeline_stop(GstElement *pipeline)
{
    GstStateChangeReturn ret;

    ret = gst_element_set_state(pipeline, GST_STATE_READY);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to ready state.\n");
        return;
    }
}

// Create and manage subpipe
GstElement *pipeline_subpipe_create(SpsSourceData *sourceData, gboolean addDefaultElements)
{
    SpsRegistry *registry = sps_registry_get();

    // Create subpipeline
    GstElement *subpipe = gst_bin_new(NULL);
    sourceData->pipeline = subpipe;

    // Create new source and set properties
    GstElement *input = gst_element_factory_make("screencapsrc", NULL);
    if (!input)
    {
        GST_ERROR("Unable to create input");
        return NULL;
    }
    gst_bin_add(GST_BIN(subpipe), input);
    g_object_set(input, "source-id", sourceData->sourceInfo.id, "source-type", sourceData->sourceInfo.type, NULL);

    // Add decoupling queue (input <-> preprocessors <-> preprocessorQueue)
    GstElement *preprocessorQueue = gst_element_factory_make("queue", "prepqueue");
    sourceData->preprocessorQueue = preprocessorQueue;
    if (!preprocessorQueue)
    {
        GST_ERROR("Unable to create preprocessor queue");
        return NULL;
    }
    gst_bin_add(GST_BIN(subpipe), preprocessorQueue);
    g_object_set(preprocessorQueue, "leaky", 2, NULL); // Leak (old) buffers downstream
    if (!gst_element_link(input, preprocessorQueue))
    {
        GST_ERROR("Unable to link input to preprocessor queue");
        return NULL;
    }

    // Detectors will be added here

    // Add decoupling queue (preprocessorQueue <-> detectors <-> detectorQueue)
    GstElement *detectorQueue = gst_element_factory_make("queue", "detqueue");
    sourceData->detectorQueue = detectorQueue;
    if (!detectorQueue)
    {
        GST_ERROR("Unable to create detector queue");
        return NULL;
    }
    gst_bin_add(GST_BIN(subpipe), detectorQueue);
    g_object_set(detectorQueue, "leaky", 2, NULL); // Leak (old) buffers downstream
    if (!gst_element_link(preprocessorQueue, detectorQueue))
    {
        GST_ERROR("Unable to preprocessor to processor queue");
        return NULL;
    }

    // Postprocessors will be added here

    // Add final queue
    GstElement *postprocessorQueue = gst_element_factory_make("queue", "postpqueue");
    sourceData->postprocessorQueue = postprocessorQueue;
    if (!postprocessorQueue)
    {
        GST_ERROR("Unable to create postprocessor queue");
        return NULL;
    }
    gst_bin_add(GST_BIN(subpipe), postprocessorQueue);
    g_object_set(postprocessorQueue, "leaky", 2, NULL); // Leak (old) buffers downstream
    if (!gst_element_link(detectorQueue, postprocessorQueue))
    {
        GST_ERROR("Unable to preprocessor to processor queue");
        return NULL;
    }

    // Add ghost pad from identity to subpipe so we can use the subpipe as source element
    GstPad *pad = gst_element_get_static_pad(postprocessorQueue, "src");
    GstPad *ghostPad = gst_ghost_pad_new("src", pad);
    gst_element_add_pad(subpipe, ghostPad);
    gst_object_unref(pad);

    if (!addDefaultElements)
        return subpipe;

    // Add default elements
    GList *defaultPreprocessor = g_hash_table_get_values(registry->defaultPreprocessors);
    while (defaultPreprocessor != NULL)
    {
        pipeline_add_preprocessor((char *)defaultPreprocessor->data, sourceData);
        defaultPreprocessor = defaultPreprocessor->next;
    }

    GList *defaultDetector = g_hash_table_get_values(registry->defaultDetectors);
    while (defaultDetector != NULL)
    {
        pipeline_add_detector((char *)defaultDetector->data, sourceData);
        defaultDetector = defaultDetector->next;
    }

    GList *defaultPostprocessor = g_hash_table_get_values(registry->defaultPostprocessors);
    while (defaultPostprocessor != NULL)
    {
        pipeline_add_postprocessor((char *)defaultPostprocessor->data, sourceData);
        defaultPostprocessor = defaultPostprocessor->next;
    }

    return subpipe;
}

static gboolean cb_remove_element(PipelineData *data)
{
    // Remove element and fill the gap
    gst_object_ref(data->element);
    gst_element_unlink_many(data->predecessor, data->element, data->successor, NULL);
    gst_element_set_state(data->element, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(data->pipeline), data->element);
    gst_element_link(data->predecessor, data->successor);
    gst_object_unref(data->element);

    // Unblock predecessor's src pad
    gst_pad_remove_probe(data->blockedPad, data->blockingProbeId);
    GST_DEBUG_OBJECT(data->blockedPad, "Pad is unblocked now");

    g_free(data);

    return G_SOURCE_REMOVE;
}

static GstPadProbeReturn cb_event_probe_remove_element(GstPad *pad, GstPadProbeInfo *info, PipelineData *data)
{
    // Only listen for EOS
    if (GST_EVENT_TYPE(GST_PAD_PROBE_INFO_DATA(info)) != GST_EVENT_EOS)
        return GST_PAD_PROBE_PASS;

    // Remove invoking probe
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));

    // Dispatch removal to the main thread to prevent a deadlock
    g_idle_add(G_SOURCE_FUNC(cb_remove_element), (gpointer)data);

    return GST_PAD_PROBE_DROP;
}

static GstPadProbeReturn cb_pad_probe_remove_element(GstPad *pad, GstPadProbeInfo *info, PipelineData *data)
{
    GST_DEBUG_OBJECT(pad, "Pad is blocked now");

    // Unlink previous element and element that should be removed (no data flowing)
    gst_element_unlink(data->predecessor, data->element);

    // Prepare to flush and remove element
    GstPad *src, *sink;
    src = gst_element_get_static_pad(data->element, "src");
    sink = gst_element_get_static_pad(data->element, "sink");

    data->blockedPad = pad;
    data->blockingProbeId = GST_PAD_PROBE_INFO_ID(info);

    if (data->pipeline->current_state < GST_STATE_PAUSED)
    {
        // Pad stopped, so we remove the element directly
        cb_remove_element(data);
    }
    else
    {
        // Register EOS probe on the element that should be removed
        gst_pad_add_probe(src, GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, (GstPadProbeCallback)cb_event_probe_remove_element, data, NULL); // Data freed in cb_remove_element

        // Send EOS to element that should be removed (will be caught by the event probe before leaving)
        gst_pad_send_event(sink, gst_event_new_eos());
    }

    // Free memory
    gst_object_unref(src);
    gst_object_unref(sink);

    return GST_PAD_PROBE_OK;
}

void pipeline_remove_element(GstElement *element, SpsSourceData *sourceData)
{
    // Get element neighbors
    GstElement *predecessor, *successor;
    GstPad *sink, *src, *sinkPeer, *srcPeer;

    sink = gst_element_get_static_pad(element, "sink");
    sinkPeer = gst_pad_get_peer(sink);
    predecessor = gst_pad_get_parent_element(sinkPeer);

    src = gst_element_get_static_pad(element, "src");
    srcPeer = gst_pad_get_peer(src);
    successor = gst_pad_get_parent_element(srcPeer);

    PipelineData *data = g_malloc(sizeof(PipelineData));
    data->element = element;
    data->pipeline = sourceData->pipeline;
    data->successor = successor;
    data->predecessor = predecessor;

    // Block the predecessors source pad
    gst_pad_add_probe(sinkPeer, GST_PAD_PROBE_TYPE_BLOCKING | GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, (GstPadProbeCallback)cb_pad_probe_remove_element, data, NULL); // Data freed in cb_remove_element

    // Free memory
    gst_object_unref(sink);
    gst_object_unref(sinkPeer);
    gst_object_unref(predecessor);
    gst_object_unref(src);
    gst_object_unref(srcPeer);
    gst_object_unref(successor);
}

static GstPadProbeReturn cb_pad_probe_insert(GstPad *pad, GstPadProbeInfo *info, PipelineData *data)
{
    GST_DEBUG_OBJECT(pad, "Pad is blocked now");

    // Remove invoking probe
    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));

    // Unlink last element and target
    gst_element_unlink(data->predecessor, data->successor);

    // Add new element
    gst_bin_add(GST_BIN(data->pipeline), data->element);

    // Link at the insert position
    if (!gst_element_link_many(data->predecessor, data->element, data->successor, NULL))
        GST_ERROR("Unable to link new element to pipeline");

    // Match state
    gst_element_set_state(data->element, data->pipeline->current_state);

    // Free data
    g_free(data);

    // Unblock pad
    GST_DEBUG_OBJECT(pad, "Pad is unblocked now");
    return GST_PAD_PROBE_OK;
}

static void pipeline_insert_element(GstElement *newElement, GstElement *insertBefore, SpsSourceData *sourceData)
{
    GstElement *insertAfter;

    // Get element before target
    GstPad *sink, *sinkPeer;
    sink = gst_element_get_static_pad(insertBefore, "sink");
    sinkPeer = gst_pad_get_peer(sink);
    insertAfter = gst_pad_get_parent_element(sinkPeer);

    PipelineData *data = g_malloc(sizeof(PipelineData));
    data->pipeline = sourceData->pipeline;
    data->element = newElement;
    data->predecessor = insertAfter;
    data->predecessor = insertAfter;
    data->successor = insertBefore;

    // Block predecessor source pad and perform insertion
    gst_pad_add_probe(sinkPeer, GST_PAD_PROBE_TYPE_BLOCKING, (GstPadProbeCallback)cb_pad_probe_insert, data, NULL);

    gst_object_unref(sink);
    gst_object_unref(sinkPeer);
    gst_object_unref(insertAfter);
}

void pipeline_add_preprocessor(const char *factoryName, SpsSourceData *sourceData)
{
    // Retrieve factory
    SpsPluginElementFactory *factory = g_hash_table_lookup(sps_registry_get()->preprocessors, factoryName);
    if (!factory)
    {
        g_warning("No factory found");
        return;
    }

    // Create element and add to pipeline
    GstElement *processor = factory->create_element();
    if (!processor)
    {
        GST_ERROR("Unable to create preprocessor \"%s\"", factoryName);
        return;
    }

    // Insert element at the right position
    pipeline_insert_element(processor, sourceData->preprocessorQueue, sourceData);

    // Store element
    g_hash_table_insert(sourceData->activePreprocessors, g_strdup(factoryName), factory);
    g_hash_table_insert(sourceData->activePreprocessorElements, g_strdup(factoryName), processor);
}

void pipeline_add_detector(const char *factoryName, SpsSourceData *sourceData)
{
    // Retrieve factory
    SpsPluginElementFactory *factory = g_hash_table_lookup(sps_registry_get()->detectors, factoryName);
    if (!factory)
    {
        g_warning("No factory found");
        return;
    }

    // Create element and add to pipeline
    GstElement *detector = factory->create_element();
    if (!detector)
    {
        GST_ERROR("Unable to create detector \"%s\"", factoryName);
        return;
    }

    // Insert element at the right position
    pipeline_insert_element(detector, sourceData->detectorQueue, sourceData);

    // Store element
    g_hash_table_insert(sourceData->activeDetectors, g_strdup(factoryName), factory);
    g_hash_table_insert(sourceData->activeDetectorElements, g_strdup(factoryName), detector);
}

void pipeline_add_postprocessor(const char *factoryName, SpsSourceData *sourceData)
{
    // Retrieve factory
    SpsPluginElementFactory *factory = g_hash_table_lookup(sps_registry_get()->postprocessors, factoryName);
    if (!factory)
    {
        g_warning("No factory found");
        return;
    }

    // Create element and add to pipeline
    GstElement *processor = factory->create_element();
    if (!processor)
    {
        GST_ERROR("Unable to create postprocessor \"%s\"", factoryName);
        return;
    }

    // Insert element at the right position
    pipeline_insert_element(processor, sourceData->postprocessorQueue, sourceData);

    // Store element
    g_hash_table_insert(sourceData->activePostprocessors, g_strdup(factoryName), factory);
    g_hash_table_insert(sourceData->activePostprocessorElements, g_strdup(factoryName), processor);
}
