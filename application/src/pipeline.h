#pragma once

#include <gst/gst.h>
#include "sourcedata.h"

GstElement *pipeline_file_create(SpsSourceData *sourceData, gboolean addDefaultElements);

GstElement *pipeline_main_create();

void pipeline_main_add_subpipe(GstElement *main, GstElement *subpipe);

void pipeline_main_remove_subpipe(GstElement *main, GstElement *subpipe);

void pipeline_start(GstElement *pipeline);

void pipeline_stop(GstElement *pipeline);

GstElement *pipeline_subpipe_create(SpsSourceData *sourceData, gboolean addDefaultElements);

void pipeline_add_preprocessor(const char *factoryName, SpsSourceData *sourceData);

void pipeline_add_detector(const char *factoryName, SpsSourceData *sourceData);

void pipeline_add_postprocessor(const char *factoryName, SpsSourceData *sourceData);

void pipeline_remove_element(GstElement *element, SpsSourceData *sourceData);
