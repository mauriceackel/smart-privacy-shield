#include <sps/spsversion.h>
#include <sps/plugins/spsplugin.h>
#include <sps/plugins/spsregistry.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gmodule.h>
#ifdef WIN32
#include <windows.h>
#endif

#define sps_registry_parent_class parent_class
G_DEFINE_TYPE(SpsRegistry, sps_registry, G_TYPE_OBJECT);

static gboolean sps_registry_check_version(gint major, gint minor)
{
    if (major != SPS_VERSION_MAJOR || minor > SPS_VERSION_MINOR)
        return FALSE;

    return TRUE;
}

// Initializes a new plugin
static gboolean sps_registry_initialize_plugin(SpsPlugin *plugin, const SpsPluginDesc *desc)
{
    g_message("Trying to load plugin \"%s\"", desc->name);

    if (!sps_registry_check_version(desc->major_version, desc->minor_version))
    {
        g_warning("Plugin target SPS version %i.%i did not match current SPS version %i.%i", desc->major_version, desc->minor_version, SPS_VERSION_MAJOR, SPS_VERSION_MINOR);
        return FALSE;
    }

    if (!desc->license || !desc->description || !desc->identifier || !desc->package || !desc->origin)
    {
        g_warning("Plugin has missing detail in description, not loading");
        return FALSE;
    }

    // Init default values
    sps_plugin_desc_copy(&plugin->description, desc);
    plugin->schemaSource = g_settings_schema_source_new_from_directory(plugin->filedir, g_settings_schema_source_get_default(), FALSE, NULL);

    // Enforce plugin is not unloaded
    if (plugin->module)
        g_module_make_resident(plugin->module);

    // Call plugin init function
    if (!((desc->plugin_init)(plugin)))
    {
        g_error("Plugin failed to initialize. Initialize function returned error.");
        return FALSE;
    }

    g_message("Plugin \"%s\" loaded successfully", desc->name);

    return TRUE;
}

// Stores a new plugin in the registry
static gboolean sps_registry_add_plugin(SpsRegistry *self, SpsPlugin *plugin)
{
    g_hash_table_insert(self->plugins, (gpointer)plugin->filename, plugin);

    return TRUE;
}

// Retruns the symbol to the function that returns the plugins description
static gchar *extract_plugin_description_symbol(const char *filepath)
{
    gchar *filename, *pluginname, *description_symbol;
    const gchar *dot;
    gsize prefix_len, len;
    int i;

    filename = g_path_get_basename(filepath);
    // Replace dash with underscore
    for (i = 0; filename[i]; ++i)
    {
        if (filename[i] == '-')
            filename[i] = '_';
    }

    // Determine suffix
    if (g_str_has_prefix(filename, "libsps"))
        prefix_len = 6;
    else if (g_str_has_prefix(filename, "lib"))
        prefix_len = 3;
    else if (g_str_has_prefix(filename, "sps"))
        prefix_len = 3;
    else
        prefix_len = 0; /* use whole name (minus suffix) as plugin name */

    dot = g_utf8_strchr(filename, -1, '.');
    if (dot)
        len = dot - filename - prefix_len; // Pointer math to get length w/o dot
    else
        len = strlen(filename + prefix_len); // Get length starting from prefix

    // The raw name of the plugin
    pluginname = g_strndup(filename + prefix_len, len);
    g_free(filename);

    description_symbol = g_strconcat("sps_plugin_", pluginname, "_get_desc", NULL);
    g_free(pluginname);

    return description_symbol;
}

// Try to load a dynamic library as sps plugin
static gboolean sps_registry_scan_plugin_file(SpsRegistry *self, const char *filepath)
{
    const SpsPluginDesc *desc;
    SpsPluginDesc *(*get_desc)(void);
    SpsPlugin *plugin;
    gchar *plugin_description_symbol;
    GModule *module;
    gboolean ret;
    GStatBuf file_status;

    g_return_val_if_fail(filepath != NULL, FALSE);

    if (!g_module_supported())
    {
        g_error("Dynamic loading not supported");
        ret = FALSE;
        goto out;
    }

    if (g_stat(filepath, &file_status))
    {
        g_warning("Unable to access library file");
        ret = FALSE;
        goto out;
    }

#ifdef WIN32
    // On windows, there is no nice way to load dynamic libraries relative to binary that is loading them
    // Instead, windows searches in specific locations for the file. It searches in all directories specified
    // in the PATH environment variable. So as a workaround, we add the current directory to the PATH
    // (for the current session only) and if it turns out we loaded a legit SPS plugin, we keep it in the PATH.
    // If we were unable to load the plugin, we remove it again
    wchar_t *wOldPath = _wgetenv(L"PATH");
    char *oldPath = g_utf16_to_utf8(wOldPath, -1, NULL, NULL, NULL);

    // Append plugin path to value of PATH
    GString *newPath = g_string_new(oldPath);
    const char *pluginDirName = g_path_get_dirname(filepath);
    g_string_append_printf(newPath, ";%s", pluginDirName);

    // Write out new value of PATH
    wchar_t *wNewPath = g_utf8_to_utf16(newPath->str, -1, NULL, NULL, NULL);
    _wputenv_s(L"PATH", wNewPath);

    g_free((void *)pluginDirName);
    g_free((void *)wNewPath);
    g_string_free(newPath, TRUE);

// Path has been updated, now try to load library
#endif

    // Load dynamic library
    module = g_module_open(filepath, G_MODULE_BIND_LOCAL);
    if (module == NULL)
    {
        g_warning("Unable to open module at %s, err: %s", filepath, g_module_error());
        ret = FALSE;
        goto out;
    }

    // Get pointer to plugin description function
    plugin_description_symbol = extract_plugin_description_symbol(filepath);
    ret = g_module_symbol(module, plugin_description_symbol, (void **)&get_desc);
    g_free(plugin_description_symbol);

    if (!ret)
    {
        g_warning("Unable to find plugin entry point for \"%s\". This might not be a SPS plugin.", filepath);
        g_module_close(module);
        goto out;
    }

    // Retrieve description and build new plugin
    desc = (const SpsPluginDesc *)get_desc();

    plugin = sps_plugin_new();
    plugin->filedir = g_path_get_dirname(filepath);
    plugin->filename = g_path_get_basename(filepath);
    plugin->module = module;

    g_message("Plugin %p for file \"%s\" prepared, initializing now.", plugin, filepath);

    if (!sps_registry_initialize_plugin(plugin, desc))
    {
        g_error("Unable to initialize plugin");
        ret = FALSE;
        goto out;
    }

    g_info("Plugin \"%s\" fully loaded and added", plugin->filename);

    // Reference the plugin and add it to the registry
    g_object_ref(plugin);
    sps_registry_add_plugin(self, plugin);

out:
#ifdef WIN32
    // Check if we were able to load the plugin and if not, restore PATH to its original value

    if (!ret)
    {
        wchar_t *wRecoveredPath = g_utf8_to_utf16(oldPath, -1, NULL, NULL, NULL);
        _wputenv_s(L"PATH", wRecoveredPath);
        g_free((void*)wRecoveredPath);
    }
    g_free((void*)oldPath);
#endif

    return ret;
}

// Returns a sps plugin for a filename or NULL if none is found. Caller will hold a reference to the plugin.
static SpsPlugin *sps_registry_lookup_filename(SpsRegistry *self, const char *filename)
{
    SpsPlugin *plugin;

    plugin = g_hash_table_lookup(self->plugins, filename);
    if (plugin)
        g_object_ref(plugin);

    return plugin;
}

// Internal method for traversing directory with subdirectories
static void sps_registry_scan_path_level(SpsRegistry *self, const char *path, gint level)
{
    GDir *directory;
    const gchar *filename;
    gchar *filepath;
    SpsPlugin *plugin;

    directory = g_dir_open(path, 0, NULL);
    if (!directory)
        return;

    // Iterate folder entries
    while ((filename = g_dir_read_name(directory)))
    {
        GStatBuf file_status;

        filepath = g_build_filename(path, filename, NULL);
        if (g_stat(filepath, &file_status) < 0)
        {
            // Error while retrieving file information
            g_free(filepath);
            continue;
        }

        // Check type of file
        if (file_status.st_mode & S_IFDIR)
        {
            // File is directory
            if (level > 0)
            {
                // Recurse into subdirectory only if max depth not reached yet
                sps_registry_scan_path_level(self, filepath, level - 1);
            }

            g_free(filepath);
            continue;
        }

        if (!(file_status.st_mode & S_IFREG))
        {
            // Unknown file type
            g_free(filepath);
            continue;
        }

        // Only normal files fom this point on
        if (!g_str_has_suffix(filename, "." G_MODULE_SUFFIX)
#ifdef SPS_EXTRA_MODULE_SUFFIX
            && !g_str_has_suffix(filename, "." SPS_EXTRA_MODULE_SUFFIX)
#endif
        )
        {
            // Ignore files that are no modules
            g_free(filepath);
            continue;
        }

        // Only possible plugins from this point

        // Check if plugin was already registered
        plugin = sps_registry_lookup_filename(self, filename);
        if (plugin)
        {
            g_error("Plugin with filename \"%s\" already registered, skipping plugin at \"%s\"", filename, filepath);
            g_object_unref(plugin);
        }
        else
        {
            // Plugin not yet loaded, check if it i an actual SPS plugin now
            sps_registry_scan_plugin_file(self, filepath);
        }

        g_free(filepath);
    }

    g_dir_close(directory);
}

// Recursively scan a path for sps plugins
void sps_registry_scan_path(SpsRegistry *self, const char *path)
{
    // Iterate folder with subfolders (max depth = 10)
    sps_registry_scan_path_level(self, path, 10);
}

// Add a detector factory
void sps_register_detector_factory(SpsPlugin *plugin, const char *name, SpsPluginElementFactory *factory)
{
    SpsRegistry *registry = sps_registry_get();

    if (g_hash_table_contains(registry->detectors, name))
        g_warning("Detector with name \"%s\" already registered. Skipping registration from plugin \"%s\".", name, plugin->filename);

    g_hash_table_insert(registry->detectors, (gpointer)name, factory);
}

// Add a preprocessor factory
void sps_register_preprocessor_factory(SpsPlugin *plugin, const char *name, SpsPluginElementFactory *factory)
{
    SpsRegistry *registry = sps_registry_get();

    if (g_hash_table_contains(registry->preprocessors, name))
        g_warning("Preprocessor with name \"%s\" already registered. Skipping registration from plugin \"%s\".", name, plugin->filename);

    g_hash_table_insert(registry->preprocessors, (gpointer)name, factory);
}

// Add a postprocessor factory
void sps_register_postprocessor_factory(SpsPlugin *plugin, const char *name, SpsPluginElementFactory *factory)
{
    SpsRegistry *registry = sps_registry_get();

    if (g_hash_table_contains(registry->postprocessors, name))
        g_warning("Postprocessor with name \"%s\" already registered. Skipping registration from plugin \"%s\".", name, plugin->filename);

    g_hash_table_insert(registry->postprocessors, (gpointer)name, factory);
}

// Get singleton of registry. No need to unref
SpsRegistry *sps_registry_get()
{
    static SpsRegistry *instance = NULL;

    if (instance == NULL)
    {
        instance = g_object_new(SPS_TYPE_REGISTRY, NULL);
    }

    return instance;
}

// Registry object initializer
static void sps_registry_init(SpsRegistry *self)
{
    self->plugins = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->preprocessors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->detectors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->postprocessors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->defaultPreprocessors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->defaultDetectors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->defaultPostprocessors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

// Registry object destructor
static void sps_registry_finalize(GObject *object)
{
    SpsRegistry *self = SPS_REGISTRY(object);

    GList *plugins = g_hash_table_get_values(self->plugins);
    while (plugins != NULL)
    {
        g_object_unref(plugins->data);
        plugins = plugins->next;
    }
    g_list_free(plugins);

    g_hash_table_destroy(self->plugins);
    g_hash_table_destroy(self->preprocessors);
    g_hash_table_destroy(self->detectors);
    g_hash_table_destroy(self->postprocessors);
    g_hash_table_destroy(self->defaultPreprocessors);
    g_hash_table_destroy(self->defaultDetectors);
    g_hash_table_destroy(self->defaultPostprocessors);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

// Registry class initializer
static void sps_registry_class_init(SpsRegistryClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = sps_registry_finalize;
}
