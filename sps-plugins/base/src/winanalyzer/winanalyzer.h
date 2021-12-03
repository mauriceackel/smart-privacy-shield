#pragma once

#include <sps/sps.h>

#define WINANALYZER_SETTINGS_FORMAT "(bs(ssxx))"

SpsPluginElementFactory *get_factory_winanalyzer();

GVariant *create_winanalyzer_settings(GstElement *element);
