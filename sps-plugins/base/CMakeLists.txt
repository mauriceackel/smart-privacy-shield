# General project setup
cmake_minimum_required(VERSION 3.17)
project(spsbase VERSION 0.1.0)

# Set plugin options
set(PLUGIN_NAME "base")
set(IDENTIFIER "${PACKAGE_SPS_PLUGINS}.${PLUGIN_NAME}")
add_compile_definitions(
    VERSION="${CMAKE_PROJECT_VERSION}"
    PLUGIN_NAME=${PLUGIN_NAME}
    IDENTIFIER="${IDENTIFIER}"
)

# Include extensions
include(cmake/GlibResources.cmake)
include(cmake/GlibSettings.cmake)

# Ensure dependencies
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(GStreamer REQUIRED COMPONENTS video)
find_package(GLib2 REQUIRED object io)
find_package(Gtk REQUIRED)

# Source and include specification
file(GLOB_RECURSE SOURCES src/*.c)
add_library(spsbase SHARED ${SOURCES} ${GLIB_RESOURCES})
add_dependencies(spsbase ${CMAKE_PROJECT_NAME}-resources ${CMAKE_PROJECT_NAME}-gsettings-schema)

target_include_directories(spsbase PUBLIC ./src ${GLIB2_COMBINED_INCLUDE_DIRS} ${GSTREAMER_COMBINED_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS})
target_link_directories(spsbase PUBLIC ${GLIB2_COMBINED_LIBRARY_DIRS} ${GSTREAMER_COMBINED_LIBRARY_DIRS} ${GTK_LIBRARY_DIRS})
target_link_libraries(spsbase ${GLIB2_COMBINED_LIBRARIES} ${GSTREAMER_COMBINED_LIBRARIES} ${GTK_LIBRARIES} sps-library)
set_target_properties(spsbase PROPERTIES RESOURCE "${GSCHEMA_RESOURCES}")

# Setup installation
include(cmake/Install.cmake)
