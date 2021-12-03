#!/bin/sh

BUNDLE_DIR="`echo "$0" | sed -e 's/\/Contents\/MacOS\/.*//'`"
RESOURCE_DIR="$BUNDLE_DIR/Contents/Resources"

# Setup environment variables
export "PATH=$RESOURCE_DIR/bin:$PATH"
# Make glib find schemas and GTK find settings & themes
export "XDG_DATA_DIRS=$RESOURCE_DIR/share:$XDG_DATA_DIRS"
export "XDG_CONFIG_DIRS=$RESOURCE_DIR/etc:$XDG_CONFIG_DIRS"
# Prevent gstreamer from loading system plugins
export "GST_PLUGIN_SYSTEM_PATH=''"
# Prevent gstreamer from loading plugins via the child process gst-plugin-scanner (which causes issues with @executable_path)
export "GST_REGISTRY_FORK=no"

# Run binary
exec "$RESOURCE_DIR/bin/smart-privacy-shield" "--plugins=$RESOURCE_DIR/plugins" "--gst-elements=$RESOURCE_DIR/lib/gstreamer"
