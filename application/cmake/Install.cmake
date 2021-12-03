# Install SPS application

# ----------- Basic installation rules for binary and library
install (TARGETS smart-privacy-shield
  EXPORT SpsTargets DESTINATION ${LIB_DIR}
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR}
)

# ----------- Installation of GLib settings
set(GSCHEMAS_INPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data/gschemas)
file(GLOB GSCHEMA_FILES ${GSCHEMAS_INPUT_DIR}/*.gschema.xml)

if(APPLE OR WIN32)
  # For mac and windows we need to include the filechooser gschema for safety
  file(GLOB AUXILIARY_GSCHEMA_FILES ${GSCHEMAS_INPUT_DIR}/auxiliary/*.gschema.xml)
  list(APPEND GSCHEMA_FILES ${AUXILIARY_GSCHEMA_FILES})
endif()

# Get path for schema installation
set(SETTINGS_PATH ${DATA_DIR}/glib-2.0/schemas)

# Get schema compiler
if(WIN32 AND MSVC)
    if(NOT GLIB_COMPILE_SCHEMAS)
        message(FATAL_ERROR "No path to glib-compile-schemas.exe specified. Please specify a path to the tool using -DGLIB_COMPILE_SCHEMAS=...")
    endif()
else()
    find_program(GLIB_COMPILE_SCHEMAS NAMES glib-compile-schemas REQUIRED)
endif()

# Install schema and compile
install(FILES ${GSCHEMA_FILES} DESTINATION ${SETTINGS_PATH})
install(CODE "execute_process(COMMAND ${GLIB_COMPILE_SCHEMAS} \"\${PACKING_DIRECTORY}/${SETTINGS_PATH}\")") # Expand prefix at runtime for bundling

# ----------- Installation of additional data

# Install theme
if(APPLE)
  install(FILES "${CMAKE_SOURCE_DIR}/application/data/themes/mac/settings.ini" DESTINATION "${SETTINGS_DIR}/gtk-3.0")
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/application/data/themes/mac/content/" DESTINATION "${DATA_DIR}/themes/mac/gtk-3.0")
elseif(UNIX)
  # Not required
elseif(WIN32)
  install(FILES "${CMAKE_SOURCE_DIR}/application/data/themes/win/settings.ini" DESTINATION "${SETTINGS_DIR}/gtk-3.0")
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/application/data/themes/win/content/" DESTINATION "${DATA_DIR}/themes/win/gtk-3.0")
endif()

# Install icons
if(APPLE)
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/application/data/icons/" DESTINATION "${DATA_DIR}/icons")
elseif(UNIX)
 # Not required
elseif(WIN32)
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/application/data/icons/" DESTINATION "${DATA_DIR}/icons")
endif()

# Check main install file for runtime dependency install
