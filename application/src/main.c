#include <gtk/gtk.h>
#include <gst/gst.h>
#include <sps/sps.h>
#include "application.h"

int main(int argc, char *argv[])
{
    // Init GStreamer
    gst_init(&argc, &argv);

    // Parse command line args
    const char *pluginPath = NULL, *gstElementsPath = NULL;
    GOptionContext *context = g_option_context_new("- run smart-privacy-shield");
    GError *error = NULL;
    GOptionEntry entries[] =
    {
        { "plugins", 'p', 0, G_OPTION_ARG_STRING, &pluginPath, "Path to SPS plugins directory", "plugin-path" },
        { "gst-elements", 'g', 0, G_OPTION_ARG_STRING, &gstElementsPath, "Path to GStreamer elements", "gstreamer-elements-path" },
        { NULL }
    };
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));
    if (!g_option_context_parse(context, &argc, &argv, &error))
    {
        g_error("Option parsing failed: %s\n", error->message);
        return 1;
    }
    g_option_context_free(context);

    // Add custom registry path for gstreamer plugins
    GstRegistry *gstRegistry = gst_registry_get();
    if(gstElementsPath == NULL)
    {
        g_error("Missing GStreamer elements path. Please specify a path. Use --help for more information.");
        return 1;
    }
    else
    {
        gst_registry_scan_path(gstRegistry, gstElementsPath);
        g_free((void *)gstElementsPath);
    }

    // Load sps plugins
    SpsRegistry *spsRegistry = sps_registry_get();
    if(pluginPath == NULL)
    {
        g_error("Missing plugin path. Please specify a path. Use --help for more information.");
        return 1;
    }
    else
    {
        sps_registry_scan_path(spsRegistry, pluginPath);
        g_free((void *)pluginPath);
    }

    SpsApplication *app = sps_application_new();
    gint status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
