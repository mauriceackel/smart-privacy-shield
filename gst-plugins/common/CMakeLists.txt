cmake_minimum_required(VERSION 3.17)
project(gstspscommon VERSION 0.1.0)

# Set plugin version
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

# Ensure dependencies
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(GStreamer REQUIRED COMPONENTS base video)
find_package(GLib2 REQUIRED COMPONENTS object)

# Source and include specification
file(GLOB SOURCES detectionmeta.c changemeta.c windowmeta.c windowlocationsmeta.c)
add_library(gstspscommon SHARED ${SOURCES})

target_include_directories(gstspscommon PUBLIC
    ${GLIB2_COMBINED_INCLUDE_DIRS} ${GSTREAMER_COMBINED_INCLUDE_DIRS}
    # Include local directories at build time and install directories once we install the traget
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${INCLUDE_DIR}>
)
target_link_directories(gstspscommon PUBLIC ${GLIB2_COMBINED_LIBRARY_DIRS} ${GSTREAMER_COMBINED_LIBRARY_DIRS})
target_link_libraries(gstspscommon ${GLIB2_COMBINED_LIBRARIES} ${GSTREAMER_COMBINED_LIBRARIES})
