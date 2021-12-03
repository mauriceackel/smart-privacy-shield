cmake_minimum_required(VERSION 3.17)
project(gstautocompositor VERSION 0.1.0)

# Set plugin version
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

# Ensure dependencies
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(GStreamer REQUIRED COMPONENTS base video)
find_package(GLib2 REQUIRED)

# Source and include specification
file(GLOB SOURCES gstautocompositor.c compose.c)
add_library(gstautocompositor SHARED ${SOURCES})

target_include_directories(gstautocompositor PUBLIC . ${GLIB2_COMBINED_INCLUDE_DIRS} ${GSTREAMER_COMBINED_INCLUDE_DIRS})
target_link_directories(gstautocompositor PUBLIC ${GLIB2_COMBINED_LIBRARY_DIRS} ${GSTREAMER_COMBINED_LIBRARY_DIRS})
target_link_libraries(gstautocompositor ${GLIB2_COMBINED_LIBRARIES} ${GSTREAMER_COMBINED_LIBRARIES} gstspscommon)
