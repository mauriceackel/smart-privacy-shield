# General subproject setup
cmake_minimum_required(VERSION 3.17)
project(sps-plugins VERSION 0.1.0)

set(PACKAGE_SPS_PLUGINS "${PACKAGE}.plugins")

add_compile_definitions(
    SPS_LICENSE="LGPL"
    PACKAGE="${PACKAGE_SPS_PLUGINS}"
    SPS_PACKAGE_NAME="SPS Plugins"
    SPS_PACKAGE_ORIGIN="Maurice Ackel"
)

# Extend module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Export all symbols to .dlls when building on windows
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS True)

# Add each plugin
add_subdirectory(base)
