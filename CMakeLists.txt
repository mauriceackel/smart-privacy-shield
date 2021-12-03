# General project setup
cmake_minimum_required(VERSION 3.17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

project(smart-privacy-shield VERSION 0.1.0)
set(SPS_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(SPS_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(SPS_VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH})

# Main package descriptor
set(PACKAGE "de.mauriceackel.sps")

# Extend module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Start installation setup
include(cmake/InstallSetup.cmake)

# Add subprojects (keep order for target referencing)
add_subdirectory(sps-library)
add_subdirectory(application)
add_subdirectory(gst-plugins)
add_subdirectory(sps-plugins)

# Complete installation setup
include(cmake/Install.cmake)

# Setup packing
include(cmake/Pack.cmake)