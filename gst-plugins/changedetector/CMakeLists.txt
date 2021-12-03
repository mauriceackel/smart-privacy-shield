cmake_minimum_required(VERSION 3.17)
project(gstchangedetector VERSION 0.1.0)

# Set plugin version
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

# Ensure dependencies
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(GStreamer REQUIRED COMPONENTS base video)
find_package(GLib2 REQUIRED)
find_package(OpenCV REQUIRED MODULE)

# Source and include specification
file(GLOB SOURCES gstchangedetector.c changedetection.cpp)
add_library(gstchangedetector SHARED ${SOURCES})

target_include_directories(gstchangedetector PUBLIC . ${GLIB2_INCLUDE_DIRS} ${GSTREAMER_INCLUDE_DIRS} ${GSTREAMER_BASE_INCLUDE_DIRS} ${GSTREAMER_VIDEO_INCLUDE_DIRS} ${OPENCV_INCLUDE_DIRS})
target_link_directories(gstchangedetector PUBLIC ${GLIB2_LIBRARY_DIRS} ${GSTREAMER_LIBRARY_DIRS} ${GSTREAMER_BASE_LIBRARY_DIRS} ${GSTREAMER_VIDEO_LIBRARY_DIRS} ${OPENCV_LIBRARY_DIRS})
target_link_libraries(gstchangedetector ${GSTREAMER_LIBRARIES} ${GSTREAMER_BASE_LIBRARIES} ${GSTREAMER_VIDEO_LIBRARIES} ${OPENCV_LIBRARIES} gstspscommon)
