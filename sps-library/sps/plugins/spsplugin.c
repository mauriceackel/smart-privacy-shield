#include <glib-object.h>
#include <sps/plugins/spsplugin.h>

#define sps_plugin_parent_class parent_class
G_DEFINE_TYPE(SpsPlugin, sps_plugin, G_TYPE_OBJECT);

GSettings *sps_plugin_get_settings(SpsPlugin *plugin)
{
    GSettingsSchema *schema;

    schema = g_settings_schema_source_lookup(plugin->schemaSource, plugin->description.identifier, FALSE);

    if (schema == NULL)
    {
        return NULL;
    }

    return g_settings_new_full(schema, NULL, NULL);
}

void sps_plugin_init(SpsPlugin *self)
{
    self->preprocessorFactories = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
    self->detectorFactories = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
    self->postprocessorFactories = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
}

void sps_plugin_finalize(GObject *object)
{
    SpsPlugin *self = SPS_PLUGIN(object);

    g_hash_table_destroy(self->preprocessorFactories);
    g_hash_table_destroy(self->detectorFactories);
    g_hash_table_destroy(self->postprocessorFactories);

    g_free((void *)self->filename);
    g_free((void *)self->filedir);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

void sps_plugin_class_init(SpsPluginClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = sps_plugin_finalize;
}

SpsPlugin *sps_plugin_new()
{
    return g_object_new(SPS_TYPE_PLUGIN, NULL);
}

void sps_plugin_desc_copy(SpsPluginDesc *target, const SpsPluginDesc *source)
{
    target->major_version = source->major_version;
    target->minor_version = source->minor_version;
    target->name = g_intern_string(source->name);
    target->description = g_intern_string(source->description);
    target->plugin_init = source->plugin_init;
    target->version = g_intern_string(source->version);
    target->license = g_intern_string(source->license);
    target->identifier = g_intern_string(source->identifier);
    target->package = g_intern_string(source->package);
    target->origin = g_intern_string(source->origin);
}
