#pragma once

#include <gst/gstelement.h>
#include <glib.h>
#include <glib-object.h>
#include <sps/plugins/spsplugin.h>

G_BEGIN_DECLS

#define SPS_TYPE_REGISTRY (sps_registry_get_type())
#define SPS_REGISTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_TYPE_REGISTRY, SpsRegistry))
#define SPS_REGISTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_TYPE_REGISTRY, SpsRegistry))
#define SPS_IS_REGISTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_TYPE_REGISTRY))
#define SPS_IS_REGISTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_TYPE_REGISTRY))

typedef struct _SpsRegistry SpsRegistry;
typedef struct _SpsRegistryClass SpsRegistryClass;

GType sps_registry_get_type(void) G_GNUC_CONST;

struct _SpsRegistry
{
    GObject parent;

    GHashTable *plugins;
    
    GHashTable *preprocessors, *detectors, *postprocessors;

    GHashTable *defaultPreprocessors, *defaultDetectors, *defaultPostprocessors;
};

struct _SpsRegistryClass
{
    GObjectClass parent_class;
};

SpsRegistry *sps_registry_get(void);
void sps_registry_scan_path(SpsRegistry *self, const char *path);
void sps_register_preprocessor_factory(SpsPlugin *plugin, const char* name, SpsPluginElementFactory *factory);
void sps_register_detector_factory(SpsPlugin *plugin, const char* name, SpsPluginElementFactory *factory);
void sps_register_postprocessor_factory(SpsPlugin *plugin, const char* name, SpsPluginElementFactory *factory);

G_END_DECLS
