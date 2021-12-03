cmake_minimum_required(VERSION 3.17)
project(gstwinanalyzer VERSION 0.1.0)

# Set plugin version
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

# Ensure dependencies
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(GStreamer REQUIRED COMPONENTS base video)
find_package(GLib2 REQUIRED)

# Source and include specification
file(GLOB SOURCES gstwinanalyzer.c winanalyzerutils.c)
if(APPLE)
    file(GLOB_RECURSE PLATFORM_SOURCES platform/mac/*)
    set(PLATFORM_LIBRARIES "-framework Foundation -framework AppKit -framework CoreGraphics")
elseif(UNIX)
    file(GLOB_RECURSE PLATFORM_SOURCES platform/lin/*)
    find_package(X11 REQUIRED)
    set(PLATFORM_LIBRARIES ${X11_LIBRARIES})
elseif(WIN32)
    file(GLOB_RECURSE PLATFORM_SOURCES platform/win/*)
    set(PLATFORM_LIBRARIES "windowsapp")
endif()
add_library(gstwinanalyzer SHARED ${SOURCES} ${PLATFORM_SOURCES})

target_include_directories(gstwinanalyzer PUBLIC . ${PLATFORM_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS} ${GSTREAMER_INCLUDE_DIRS} ${GSTREAMER_BASE_INCLUDE_DIRS} ${GSTREAMER_VIDEO_INCLUDE_DIRS})
target_link_directories(gstwinanalyzer PUBLIC ${GLIB2_LIBRARY_DIRS} ${GSTREAMER_LIBRARY_DIRS} ${GSTREAMER_BASE_LIBRARY_DIRS} ${GSTREAMER_VIDEO_LIBRARY_DIRS})
target_link_libraries(gstwinanalyzer ${PLATFORM_LIBRARIES} ${GSTREAMER_LIBRARIES} ${GSTREAMER_BASE_LIBRARIES} ${GSTREAMER_VIDEO_LIBRARIES} gstspscommon)
