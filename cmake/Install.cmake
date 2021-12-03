# Configure installation

include(LibraryPaths)

# ----------- Installation of required runtime libraries of main application and all bundeled gstreamer elements
lp_get_dependency_paths(DEPENDENCY_PATHS smart-privacy-shield sps-library gstautocompositor gstscreencapsrc)
install(CODE "set(DEPENDENCY_PATHS \"${DEPENDENCY_PATHS}\")") # Make variable accessible inside install runtime

install(CODE [[
    include(BundleUtilities)

    # Overwrite default library location with /lib on macOS
    function(gp_item_default_embedded_path_override item default_embedded_path_var)
      string(REPLACE "Frameworks" "lib" path "${${default_embedded_path_var}}")
      set(${default_embedded_path_var} "${path}" PARENT_SCOPE)
    endfunction ()

    # Get main executable and additional gstreamer libraries
    file(GLOB EXECUTABLE ${BIN_DIR}/smart-privacy-shield*)
    file(GLOB ADDITIONAL_LIBS ${LIB_DIR}/gstreamer/*)

    # Copy all runtime dependencies of main executable and gstreamer plugins to /lib
    # Change the install dir inside the binaries to @executable/../lib/<libname>
    fixup_bundle("${EXECUTABLE}" "${ADDITIONAL_LIBS}" "${DEPENDENCY_PATHS};${LIB_DIR}")
  ]]
)

# ----------- Installation of required runtime libraries of "spsbase" plugin and all bundeled gstreamer elements
lp_get_dependency_paths(DEPENDENCY_PATHS spsbase gstregions gstchangedetector gstobjdetection gstobstruct gstwinanalyzer)
install(CODE "set(DEPENDENCY_PATHS \"${DEPENDENCY_PATHS}\")") # Make variable accessible inside install runtime

install(CODE "list(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH})") # Make build tree module path available in install module path
install(CODE [[
    include(PluginBundleUtilities)

    # Overwrite default library location with /lib on macOS
    function(gp_item_default_embedded_path_override item default_embedded_path_var)
      set(path "${${default_embedded_path_var}}")
      string(REPLACE "@executable_path/../" "@executable_path/" path ${path})
      if(APPLE)
        string(REPLACE "Frameworks" "lib" path ${path})
      elseif(UNIX)
        set(path "${path}/lib")
      endif()
      set(${default_embedded_path_var} "${path}" PARENT_SCOPE)
    endfunction ()

    # Ignore all dependencies that are already part of the main lib(/bin) folder
    if(WIN32)
        file(GLOB IGNORED_DEPENDENCIES RELATIVE ${BIN_DIR} ${BIN_DIR}/*)
    else()    
        file(GLOB IGNORED_DEPENDENCIES RELATIVE ${LIB_DIR} ${LIB_DIR}/*)
    endif()
    
    # Get main plugin file and additional gstreamer libraries
    file(GLOB PLUGIN_LIB 
      ${PLUGIN_DIR}/spsbase/*.dylib
      ${PLUGIN_DIR}/spsbase/*.dll
      ${PLUGIN_DIR}/spsbase/*.so
    )
    file(GLOB ADDITIONAL_LIBS
      ${PLUGIN_DIR}/spsbase/gst-plugins/*.dylib
      ${PLUGIN_DIR}/spsbase/gst-plugins/*.dll
      ${PLUGIN_DIR}/spsbase/gst-plugins/*.so
    )

    # Copy all runtime dependencies of plugin and gstreamer plugins to /lib
    # Change the install dir inside the binaries to @rpath/<libname>
    # Set rpath to "@executable_path/../lib", "@loader_path", "@loader_path/../lib"
    list(APPEND CUSTOM_RPATHS "@executable_path/../lib" "@loader_path/lib" "@loader_path/../lib")
    fixup_plugin("${PLUGIN_LIB}" "${ADDITIONAL_LIBS}" "${DEPENDENCY_PATHS};${LIB_DIR}" IGNORE_ITEM "${IGNORED_DEPENDENCIES}")
  ]]
)

# ----------- Installation of Cmake package
install(FILES cmake/SpsConfig.cmake DESTINATION "${LIB_DIR}/cmake/sps")
install(EXPORT SpsTargets
    FILE SpsTargets.cmake
    NAMESPACE Sps::
    DESTINATION "${LIB_DIR}/cmake/sps"
)
