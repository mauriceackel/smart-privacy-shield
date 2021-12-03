cmake_minimum_required(VERSION 3.17)
project(gstscreencapsrc VERSION 0.1.0)

# Set plugin version
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

# Ensure dependencies
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(GStreamer REQUIRED COMPONENTS base video)
find_package(GLib2 REQUIRED)

# Source and include specification
file(GLOB SOURCES gstscreencapsrc.c)
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
    find_package(CppWinRT REQUIRED)
    set(PLATFORM_INCLUDE_DIRS ${CPPWINRT_INCLUDE_DIRS})
endif()
add_library(gstscreencapsrc SHARED ${SOURCES} ${PLATFORM_SOURCES})

target_include_directories(gstscreencapsrc PUBLIC . ${GLIB2_COMBINED_INCLUDE_DIRS} ${GSTREAMER_COMBINED_INCLUDE_DIRS} ${PLATFORM_INCLUDE_DIRS} ${SPS_INCLUDE_DIRS})
target_link_directories(gstscreencapsrc PUBLIC ${GLIB2_COMBINED_LIBRARY_DIRS} ${GSTREAMER_COMBINED_LIBRARY_DIRS} ${SPS_LIBRARY_DIRS})
target_link_libraries(gstscreencapsrc ${GLIB2_COMBINED_LIBRARIES} ${GSTREAMER_COMBINED_LIBRARIES} ${PLATFORM_LIBRARIES} ${SPS_LIBRARIES} gstspscommon sps-library)
