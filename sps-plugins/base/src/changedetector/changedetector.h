#pragma once

#include <sps/sps.h>

#define CHANGEDETECTOR_SETTINGS_FORMAT "(bi)"

SpsPluginElementFactory *get_factory_changedetector();

GVariant *create_changedetector_settings(GstElement *element);
