# General subproject setup
cmake_minimum_required(VERSION 3.17)
project(sps-application VERSION 0.1.0)

set(PACKAGE_APPLICATION "${PACKAGE}.application")

# Extend module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Add extensions
include(cmake/GlibResources.cmake)

# Add compile definitions
add_compile_definitions(APPLICATION_ID="${PACKAGE_APPLICATION}")

# Ensure dependencies
find_package(GStreamer REQUIRED COMPONENTS video)
find_package(GLib2 REQUIRED COMPONENTS object io)
find_package(Gtk REQUIRED)

# Define executable
file(GLOB_RECURSE APP_SOURCES src/*.c)
add_executable(smart-privacy-shield ${APP_SOURCES} ${GLIB_RESOURCES})
target_include_directories(smart-privacy-shield PUBLIC
    ${GSTREAMER_COMBINED_INCLUDE_DIRS} ${GLIB2_COMBINED_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS} ${PLATFORM_INCLUDE_DIRS}
    # Include local directories at build time and install directories once we install the traget
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${INCLUDE_DIR}> 
    $<INSTALL_INTERFACE:${LIB_DIR}>
)
add_dependencies(smart-privacy-shield sps-gui-resources)
target_link_directories(smart-privacy-shield PUBLIC ${GSTREAMER_COMBINED_LIBRARY_DIRS} ${GLIB2_COMBINED_LIBRARY_DIRS} ${GTK_LIBRARY_DIRS} ${PLATFORM_LIBRARY_DIRS})
target_link_libraries(smart-privacy-shield ${GSTREAMER_COMBINED_LIBRARIES} ${GLIB2_COMBINED_LIBRARIES} ${GTK_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_LIBRARIES} sps-library gstpbutils-1.0)

# Make windows application instead of console app
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(smart-privacy-shield PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
endif()

# Setup installation
include(cmake/Install.cmake)
