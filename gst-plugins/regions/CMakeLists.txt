cmake_minimum_required(VERSION 3.17)
project(gstregions VERSION 0.1.0)

# Set plugin version
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

# Ensure dependencies
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(GStreamer REQUIRED COMPONENTS base video)
find_package(GLib2 REQUIRED)

# Source and include specification
file(GLOB SOURCES gstregions.c)
add_library(gstregions SHARED ${SOURCES})

target_include_directories(gstregions PUBLIC . ${GLIB2_INCLUDE_DIRS} ${GSTREAMER_INCLUDE_DIRS} ${GSTREAMER_BASE_INCLUDE_DIRS} ${GSTREAMER_VIDEO_INCLUDE_DIRS})
target_link_directories(gstregions PUBLIC ${GLIB2_LIBRARY_DIRS} ${GSTREAMER_LIBRARY_DIRS} ${GSTREAMER_BASE_LIBRARY_DIRS} ${GSTREAMER_VIDEO_LIBRARY_DIRS})
target_link_libraries(gstregions ${GSTREAMER_LIBRARIES} ${GSTREAMER_BASE_LIBRARIES} ${GSTREAMER_VIDEO_LIBRARIES} gstspscommon)
