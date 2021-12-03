# Get GLIB compile resources program
if(WIN32 AND MSVC)
    if(NOT GLIB_COMPILE_RESOURCES)
        message(FATAL_ERROR "No path to glib-compile-resources.exe specified. Please specify a path to the tool using -DGLIB_COMPILE_RESOURCES=...")
    endif()
else()
    find_program(GLIB_COMPILE_RESOURCES NAMES glib-compile-resources REQUIRED)
endif()

set(GRESOURCE_C gresource.c) # GResource compiled output file name
set(GRESOURCE_XML gresource.xml) # GResource config input file name
set(INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gui") # Directory that holds the GResource XML and the resource files

# Get all glade files and convert them in an XML format
file(GLOB_RECURSE GRESOURCE_FILES ${INPUT_DIR}/*.glade)
set(GLADE_RESOURCES "")
foreach(GRESOURCE_FILE IN LISTS GRESOURCE_FILES)
    get_filename_component(FILE_NAME ${GRESOURCE_FILE} NAME)
    set(GLADE_RESOURCES "${GLADE_RESOURCES}<file>${FILE_NAME}</file>\n\t\t")
endforeach()

# Add files to GResource configuration XML
configure_file(gui/${GRESOURCE_XML}.in ${GRESOURCE_XML} @ONLY)

# Compile GResources
add_custom_command(
    OUTPUT ${GRESOURCE_C}
    WORKING_DIRECTORY ${INPUT_DIR}
    COMMAND ${GLIB_COMPILE_RESOURCES}
    ARGS
        --generate-source
        --target=${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_C}
        ${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_XML}
    VERBATIM
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_XML}
    DEPENDS ${GRESOURCE_FILES}
)

# Define a target with all the GResources, which we can include in the main target
add_custom_target(
    ${CMAKE_PROJECT_NAME}-resources
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_C} # Will be triggered when GResources are recompiled
)

# Export a variable with the path to the generated binary resources
set(GLIB_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_C}")

# Tell CMake that the compiled resources will be generated later, so there is not an error
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_C}
    PROPERTIES GENERATED TRUE
)
