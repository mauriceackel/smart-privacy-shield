cmake_minimum_required(VERSION 3.17)
project(gstobjdetection VERSION 0.1.0)

# Set plugin version
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

# Ensure dependencies
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(GStreamer REQUIRED COMPONENTS base video)
if(WIN32)
# On Windows, add gmodule dependency for loading onnxruntime
find_package(GLib2 REQUIRED module)
else()
find_package(GLib2 REQUIRED)
endif()
find_package(ONNXRuntime REQUIRED)

# Source and include specification
file(GLOB SOURCES gstobjdetection.c objdetectionmeta.c inferencedata.c inferenceutil.c)
add_library(gstobjdetection SHARED ${SOURCES})

target_include_directories(gstobjdetection PUBLIC . ${GLIB2_COMBINED_INCLUDE_DIRS} ${GSTREAMER_COMBINED_INCLUDE_DIRS} ${ONNXRUNTIME_INCLUDE_DIRS})
target_link_directories(gstobjdetection PUBLIC ${GLIB2_COMBINED_LIBRARY_DIRS} ${GSTREAMER_COMBINED_LIBRARY_DIRS} ${ONNXRUNTIME_LIBRARY_DIRS})
target_link_libraries(gstobjdetection ${GLIB2_COMBINED_LIBRARIES} ${GSTREAMER_COMBINED_LIBRARIES} ${ONNXRUNTIME_LIBRARIES} gstspscommon)
