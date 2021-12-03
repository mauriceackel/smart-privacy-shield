# Get input files path
set(GSCHEMA_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/data/plugin.gschema.in)
set(GSCHEMA_FILE ${CMAKE_CURRENT_BINARY_DIR}/plugin.gschema.xml)
set(GSCHEMA_COMPILED ${CMAKE_CURRENT_BINARY_DIR}/gschemas.compiled)

# Get schema compiler
if(WIN32 AND MSVC)
    if(NOT GLIB_COMPILE_SCHEMAS)
        message(FATAL_ERROR "No path to glib-compile-schemas.exe specified. Please specify a path to the tool using -DGLIB_COMPILE_SCHEMAS=...")
    endif()
else()
    find_program(GLIB_COMPILE_SCHEMAS NAMES glib-compile-schemas REQUIRED)
endif()

# Precompile schema
configure_file(${GSCHEMA_TEMPLATE} ${GSCHEMA_FILE})

add_custom_command(
    OUTPUT ${GSCHEMA_COMPILED}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${GLIB_COMPILE_SCHEMAS}
    ARGS
        "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM
    MAIN_DEPENDENCY ${GSCHEMA_FILE}
)

# Define a target with all the GSchemas, which we can include in the main target
add_custom_target(
    ${CMAKE_PROJECT_NAME}-gsettings-schema
    DEPENDS ${GSCHEMA_COMPILED} # Will be triggered when GSchemas are recompiled
)

# Export a variable with the path to all required resource files at runtime
set(GSCHEMA_RESOURCES ${GSCHEMA_FILE} ${GSCHEMA_COMPILED})
