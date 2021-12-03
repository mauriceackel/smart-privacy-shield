#pragma once

#include <sps/sps.h>

#define OBSTRUCT_SETTINGS_FORMAT "(bbidas)"

SpsPluginElementFactory *get_factory_obstruct();

GVariant *create_obstruct_settings(GstElement *element);
