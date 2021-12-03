#pragma once

#include <sps/sps.h>

#define OBJDETECTION_SETTINGS_FORMAT "(bss)"

SpsPluginElementFactory *get_factory_objdetection();

GVariant *create_objdetection_settings(GstElement *element);
