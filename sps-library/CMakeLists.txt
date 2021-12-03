# General subproject setup
cmake_minimum_required(VERSION 3.17)
project(sps-library VERSION 0.1.0)

set(PACKAGE_LIBRARY "${PACKAGE}.library")

# Export all symbols to .dlls when building on windows
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS True)

# Extend module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Manage template files
configure_file(sps/spsversion.h.in sps/spsversion.h @ONLY)

# Ensure dependencies
find_package(GStreamer REQUIRED COMPONENTS video)
find_package(GLib2 REQUIRED COMPONENTS object module io)
find_package(Gtk REQUIRED)

# Source and include specification
file(GLOB_RECURSE LIB_SOURCES sps/*.c)
if(APPLE)
  file(GLOB_RECURSE PLATFORM_LIB_SOURCES platform/mac/*.c platform/mac/*.cpp platform/mac/*.m platform/mac/*.mm)
  find_library(FOUNDATION Foundation REQUIRED)
  find_library(CORE_GRAPHICS CoreGraphics REQUIRED)
  set(PLATFORM_LIBRARIES ${FOUNDATION} ${CORE_GRAPHICS})

  add_compile_definitions(SPS_EXTRA_MODULE_SUFFIX="dylib")
elseif(UNIX)
  file(GLOB_RECURSE PLATFORM_LIB_SOURCES platform/lin/*.c platform/lin/*.cpp)
  find_package(X11 REQUIRED)
  set(PLATFORM_LIBRARIES ${X11_LIBRARIES})
elseif(WIN32)
  file(GLOB_RECURSE PLATFORM_LIB_SOURCES platform/win/*.c platform/win/*.cpp)
endif()

# Define dynamic library
add_library(sps-library SHARED ${LIB_SOURCES} ${PLATFORM_LIB_SOURCES})
set_target_properties(sps-library PROPERTIES OUTPUT_NAME sps)
target_include_directories(sps-library PUBLIC
    ${GSTREAMER_COMBINED_INCLUDE_DIRS} ${GLIB2_COMBINED_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS} ${PLATFORM_INCLUDE_DIRS}
    # Include local directories at build time and install directories once we install the traget
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${INCLUDE_DIR}> 
    $<INSTALL_INTERFACE:${LIB_DIR}>
)
target_link_directories(sps-library PUBLIC ${GSTREAMER_COMBINED_LIBRARY_DIRS} ${GLIB2_COMBINED_LIBRARY_DIRS} ${GTK_LIBRARY_DIRS} ${PLATFORM_LIBRARY_DIRS})
target_link_libraries(sps-library ${GSTREAMER_COMBINED_LIBRARIES} ${GLIB2_COMBINED_LIBRARIES} ${GTK_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_LIBRARIES})

# Setup installation
include(cmake/Install.cmake)
