#pragma once

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS

#define SPS_TYPE_PLUGIN (sps_plugin_get_type())
#define SPS_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_TYPE_PLUGIN, SpsPlugin))
#define SPS_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_TYPE_PLUGIN, SpsPlugin))
#define SPS_IS_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_TYPE_PLUGIN))
#define SPS_IS_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_TYPE_PLUGIN))

typedef struct _SpsPlugin SpsPlugin;
typedef struct _SpsPluginClass SpsPluginClass;
typedef gboolean (*SpsPluginInitFunc)(SpsPlugin *plugin);
typedef struct _SpsPluginDesc SpsPluginDesc;
typedef struct _SpsPluginElementFactory SpsPluginElementFactory;

GType sps_plugin_get_type(void) G_GNUC_CONST;

struct _SpsPluginElementFactory
{
    GstElement *(*create_element)(void);
    GtkWidget *(*create_settings_gui)(GstElement *element);
    GVariant *(*create_element_settings)(GstElement *element);
    void (*apply_element_settings)(GVariant *variant, GstElement *element);
};

struct _SpsPluginDesc
{
    gint major_version;
    gint minor_version;
    const gchar *name;
    const gchar *description;
    SpsPluginInitFunc plugin_init;
    const gchar *version;
    const gchar *license;
    const gchar *identifier;
    const gchar *package;
    const gchar *origin;
};

SpsPlugin *sps_plugin_new();
GSettings *sps_plugin_get_settings(SpsPlugin *plugin);
void sps_plugin_desc_copy(SpsPluginDesc *target, const SpsPluginDesc *source);

struct _SpsPlugin
{
    GObject parent;

    SpsPluginDesc description;

    const gchar *filename;
    const gchar *filedir;

    GSettingsSchemaSource *schemaSource;

    GModule *module;

    GHashTable *preprocessorFactories, *detectorFactories, *postprocessorFactories;
};

struct _SpsPluginClass
{
    GObjectClass parent_class;
};

#define SPS_PLUGIN_DEFINE(major, minor, name, description, init, version, license, package, origin) \
    G_BEGIN_DECLS                                                                                   \
    G_MODULE_EXPORT const SpsPluginDesc *G_PASTE(sps_plugin_, G_PASTE(name, _get_desc))(void);      \
                                                                                                    \
    static const SpsPluginDesc sps_plugin_desc = {                                                  \
        major,                                                                                      \
        minor,                                                                                      \
        G_STRINGIFY(name),                                                                          \
        (gchar *)description,                                                                       \
        init,                                                                                       \
        version,                                                                                    \
        license,                                                                                    \
        IDENTIFIER,                                                                                 \
        package,                                                                                    \
        origin,                                                                                     \
    };                                                                                              \
                                                                                                    \
    const SpsPluginDesc *G_PASTE(sps_plugin_, G_PASTE(name, _get_desc))(void)                       \
    {                                                                                               \
        return &sps_plugin_desc;                                                                    \
    }                                                                                               \
    G_END_DECLS

G_END_DECLS
