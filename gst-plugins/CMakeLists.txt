# General subproject setup
cmake_minimum_required(VERSION 3.17)
project(sps-gst-plugins VERSION 0.1.0)

set(PACKAGE_SPS_GST_PLUGINS "${PACKAGE}.gst-plugins")

add_compile_definitions(
    GST_LICENSE="LGPL"
    PACKAGE="${PACKAGE_SPS_GST_PLUGINS}"
    GST_PACKAGE_NAME="SPS GST Plugins"
    GST_PACKAGE_ORIGIN="Maurice Ackel"
)

# Extend module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Export all symbols to .dlls when building on windows
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS True)

add_subdirectory(screencapsrc)
add_subdirectory(autocompositor)
add_subdirectory(changedetector)
add_subdirectory(objdetection)
add_subdirectory(winanalyzer)
add_subdirectory(common)
add_subdirectory(regions)
add_subdirectory(obstruct)

# Setup installation
include(cmake/Install.cmake)
