#pragma once

#include <sps/sps.h>
#include <gst/gst.h>

#define REGIONS_SETTINGS_FORMAT "(bsas)"

SpsPluginElementFactory *get_factory_regions();

GVariant *create_regions_settings(GstElement *element);
