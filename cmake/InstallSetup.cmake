# Configure installation

# ----------- Make cache variables for install destinations
include(GNUInstallDirs) # Include install dirs

if(UNIX AND NOT APPLE)
  # DEB and RPM packages do not run install them selfes and hence dont set this value.
  # Instead, they only copy the build tree. As we rely on CMAKE_INSTALL_PREFIX however
  # when we fix the bundle, we have to set it manually to the temporary directory of CPack.
  install(CODE [[ set(PACKING_DIRECTORY $ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}) ]])
else()
  install(CODE [[ set(PACKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}) ]])
endif()

set(LIB_DIR ${CMAKE_INSTALL_LIBDIR})
install(CODE "set(LIB_DIR \"\${PACKING_DIRECTORY}/${LIB_DIR}\")") # Make variable accessible inside install runtime

set(INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR})
install(CODE "set(INCLUDE_DIR \"\${PACKING_DIRECTORY}/${INCLUDE_DIR}\")") # Make variable accessible inside install runtime

set(BIN_DIR ${CMAKE_INSTALL_BINDIR})
install(CODE "set(BIN_DIR \"\${PACKING_DIRECTORY}/${BIN_DIR}\")") # Make variable accessible inside install runtime

set(DATA_DIR ${CMAKE_INSTALL_DATADIR})
install(CODE "set(DATA_DIR \"\${PACKING_DIRECTORY}/${DATA_DIR}\")") # Make variable accessible inside install runtime

set(SETTINGS_DIR "etc")
install(CODE "set(SETTINGS_DIR \"\${PACKING_DIRECTORY}/${SETTINGS_DIR}\")") # Make variable accessible inside install runtime

if(UNIX AND NOT APPLE)
  # On unix, as we install to the file system, place the plugins inside the lib folder in a special sps subfolder
  set(PLUGIN_DIR "${LIB_DIR}/sps")
else()
  set(PLUGIN_DIR "plugins")
endif()
install(CODE "set(PLUGIN_DIR \"\${PACKING_DIRECTORY}/${PLUGIN_DIR}\")") # Make variable accessible inside install runtime


