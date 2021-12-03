# FindGLib2
# ===============
#
# Find a GLib2 installation.
# In addition, components can be added to the find_package call to require additional plugins like "object" and "module"
#
# Output variable
# ---------------
# 
#   GLIB2_FOUND           True if headers and requested libraries were found
#   GLIB2_LIBRARIES       Component libraries to be linked.
#   GLIB2_LIBRARY_DIRS    Directories of component libraries to be linked.
#   GLIB2_INCLUDE_DIRS    Include directories.
#
#   GLIB2_COMBINED_LIBRARIES       Combined libraries of base glib and selected components
#   GLIB2_COMBINED_LIBRARY_DIRS    Combined library dirs of base glib and selected components
#   GLIB2_COMBINED_INCLUDE_DIRS    Combined include dirs of base glib and selected components
#
#   <component_name>_LIBRARIES       Component libraries to be linked.
#   <component_name>_LIBRARY_DIRS    Directories of component libraries to be linked.
#   <component_name>_INCLUDE_DIRS    Include directories.
#

set(OUTPUT_VARS)

if(WIN32 AND MSVC)
    set(GLIB2_ROOT_DIR "C:/gtk/3.24.30")

    set(GLIB2_INCLUDE_DIRS "${GLIB2_ROOT_DIR}/include/glib-2.0" "${GLIB2_ROOT_DIR}/lib/glib-2.0/include")
    set(GLIB2_LIBRARY_DIRS "${GLIB2_ROOT_DIR}/lib")
    find_library(GLIB2_LIBRARIES NAME glib-2.0 HINTS ${GLIB2_LIBRARY_DIRS})

    list(APPEND OUTPUT_VARS GLIB2_INCLUDE_DIRS GLIB2_LIBRARY_DIRS GLIB2_LIBRARIES)
    list(APPEND GLIB2_COMBINED_LIBRARIES ${GLIB2_LIBRARIES})
    list(APPEND GLIB2_COMBINED_LIBRARY_DIRS ${GLIB2_LIBRARY_DIRS})
    list(APPEND GLIB2_COMBINED_INCLUDE_DIRS ${GLIB2_INCLUDE_DIRS})

    if("object" IN_LIST GLib2_FIND_COMPONENTS)
        set(GOBJECT_INCLUDE_DIRS "${GLIB2_ROOT_DIR}/include/glib-2.0" "${GLIB2_ROOT_DIR}/lib/glib-2.0/include")
        set(GOBJECT_LIBRARY_DIRS "${GLIB2_ROOT_DIR}/lib")
        find_library(GOBJECT_LIBRARIES NAME gobject-2.0 HINTS ${GOBJECT_LIBRARY_DIRS})

        list(APPEND OUTPUT_VARS GOBJECT_INCLUDE_DIRS GOBJECT_LIBRARIES)
        list(APPEND GLIB2_COMBINED_LIBRARIES ${GOBJECT_LIBRARIES})
        list(APPEND GLIB2_COMBINED_LIBRARY_DIRS ${GOBJECT_LIBRARY_DIRS})
        list(APPEND GLIB2_COMBINED_INCLUDE_DIRS ${GOBJECT_INCLUDE_DIRS})
    endif()

    if("module" IN_LIST GLib2_FIND_COMPONENTS)
        set(GMODULE_INCLUDE_DIRS "${GLIB2_ROOT_DIR}/include/glib-2.0" "${GLIB2_ROOT_DIR}/lib/glib-2.0/include")
        set(GMODULE_LIBRARY_DIRS "${GLIB2_ROOT_DIR}/lib")
        find_library(GMODULE_LIBRARIES NAME gmodule-2.0 HINTS ${GMODULE_LIBRARY_DIRS})

        list(APPEND OUTPUT_VARS GMODULE_INCLUDE_DIRS GMODULE_LIBRARIES)
        list(APPEND GLIB2_COMBINED_LIBRARIES ${GMODULE_LIBRARIES})
        list(APPEND GLIB2_COMBINED_LIBRARY_DIRS ${GMODULE_LIBRARY_DIRS})
        list(APPEND GLIB2_COMBINED_INCLUDE_DIRS ${GMODULE_INCLUDE_DIRS})
    endif()

    if("io" IN_LIST GLib2_FIND_COMPONENTS)
        set(GIO_INCLUDE_DIRS "${GLIB2_ROOT_DIR}/include/glib-2.0" "${GLIB2_ROOT_DIR}/include/gio-win32-2.0" "${GLIB2_ROOT_DIR}/lib/glib-2.0/include")
        set(GIO_LIBRARY_DIRS "${GLIB2_ROOT_DIR}/lib")
        find_library(GIO_LIBRARIES NAME gio-2.0 HINTS ${GIO_LIBRARY_DIRS})

        list(APPEND OUTPUT_VARS GIO_INCLUDE_DIRS GIO_LIBRARIES)
        list(APPEND GLIB2_COMBINED_LIBRARIES ${GIO_LIBRARIES})
        list(APPEND GLIB2_COMBINED_LIBRARY_DIRS ${GIO_LIBRARY_DIRS})
        list(APPEND GLIB2_COMBINED_INCLUDE_DIRS ${GIO_INCLUDE_DIRS})
    endif()
else()
    find_package(PkgConfig)
    pkg_check_modules(GLIB2 REQUIRED glib-2.0)

    list(APPEND OUTPUT_VARS GLIB2_INCLUDE_DIRS GLIB2_LIBRARIES)
    list(APPEND GLIB2_COMBINED_LIBRARIES ${GLIB2_LIBRARIES})
    list(APPEND GLIB2_COMBINED_LIBRARY_DIRS ${GLIB2_LIBRARY_DIRS})
    list(APPEND GLIB2_COMBINED_INCLUDE_DIRS ${GLIB2_INCLUDE_DIRS})

    if("object" IN_LIST GLib2_FIND_COMPONENTS)
        pkg_check_modules(GOBJECT REQUIRED gobject-2.0)

        list(APPEND OUTPUT_VARS GOBJECT_INCLUDE_DIRS GOBJECT_LIBRARIES)
        list(APPEND GLIB2_COMBINED_LIBRARIES ${GOBJECT_LIBRARIES})
        list(APPEND GLIB2_COMBINED_LIBRARY_DIRS ${GOBJECT_LIBRARY_DIRS})
        list(APPEND GLIB2_COMBINED_INCLUDE_DIRS ${GOBJECT_INCLUDE_DIRS})
    endif()

    if("module" IN_LIST GLib2_FIND_COMPONENTS)
        pkg_check_modules(GMODULE REQUIRED gmodule-2.0)

        list(APPEND OUTPUT_VARS GMODULE_INCLUDE_DIRS GMODULE_LIBRARIES)
        list(APPEND GLIB2_COMBINED_LIBRARIES ${GMODULE_LIBRARIES})
        list(APPEND GLIB2_COMBINED_LIBRARY_DIRS ${GMODULE_LIBRARY_DIRS})
        list(APPEND GLIB2_COMBINED_INCLUDE_DIRS ${GMODULE_INCLUDE_DIRS})
    endif()

    if("io" IN_LIST GLib2_FIND_COMPONENTS)
        pkg_check_modules(GIO REQUIRED gio-2.0)

        list(APPEND OUTPUT_VARS GIO_INCLUDE_DIRS GIO_LIBRARIES)
        list(APPEND GLIB2_COMBINED_LIBRARIES ${GIO_LIBRARIES})
        list(APPEND GLIB2_COMBINED_LIBRARY_DIRS ${GIO_LIBRARY_DIRS})
        list(APPEND GLIB2_COMBINED_INCLUDE_DIRS ${GIO_INCLUDE_DIRS})
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLib2 DEFAULT_MSG ${OUTPUT_VARS})

mark_as_advanced(${OUTPUT_VARS})

unset(OUTPUT_VARS)
