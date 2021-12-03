#include <gtk/gtk.h>
#include <gst/gst.h>
#include <sps/sps.h>
#include "spspluginbase.h"
#include "regions/regions.h"
#include "objdetection/objdetection.h"
#include "winanalyzer/winanalyzer.h"
#include "obstruct/obstruct.h"
#include "changedetector/changedetector.h"

#define GST_PLUGIN_PATH "gst-plugins"

// Define global variable
SpsPlugin *self;

static gboolean plugin_init(SpsPlugin *plugin)
{
    gboolean ret = TRUE;

    self = plugin;

    // Register local gst plugins
    GstRegistry *gstRegistry = gst_registry_get();
    const char *absoluteGstPluginPath = g_strconcat(plugin->filedir, "/", GST_PLUGIN_PATH, NULL);
    gst_registry_scan_path(gstRegistry, absoluteGstPluginPath);
    g_free((void *)absoluteGstPluginPath);

    SpsPluginElementFactory *factory;

    // Register preprocessor
    factory = get_factory_changedetector();
    g_hash_table_insert(plugin->preprocessorFactories, "change", factory);
    sps_register_preprocessor_factory(plugin, "change", factory);

    // Register detector factories
    factory = get_factory_regions();
    g_hash_table_insert(plugin->detectorFactories, "regions", factory);
    sps_register_detector_factory(plugin, "regions", factory);

    factory = get_factory_objdetection();
    g_hash_table_insert(plugin->detectorFactories, "objdetection", factory);
    sps_register_detector_factory(plugin, "objdetection", factory);

    factory = get_factory_winanalyzer();
    g_hash_table_insert(plugin->detectorFactories, "winanalyzer", factory);
    sps_register_detector_factory(plugin, "winanalyzer", factory);

    // Register postprocessor factories
    factory = get_factory_obstruct();
    g_hash_table_insert(plugin->postprocessorFactories, "obstruct", factory);
    sps_register_postprocessor_factory(plugin, "obstruct", factory);

    return ret;
}

SPS_PLUGIN_DEFINE(SPS_VERSION_MAJOR,
                  SPS_VERSION_MINOR,
                  PLUGIN_NAME,
                  "SPS Plugin with all base functionality",
                  plugin_init,
                  VERSION, SPS_LICENSE, SPS_PACKAGE_NAME, SPS_PACKAGE_ORIGIN);
