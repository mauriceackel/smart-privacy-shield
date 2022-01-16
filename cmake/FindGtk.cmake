# FindGtk
# ===============
#
# Find a GTK installation.
#
# Output variable
# ---------------
# 
#   GTK_FOUND           True if headers and requested libraries were found
#   GTK_LIBRARIES       Component libraries to be linked.
#   GTK_LIBRARY_DIRS    Directories of component libraries to be linked.
#   GTK_INCLUDE_DIRS    Include directories.
#

set(OUTPUT_VARS)

if(WIN32 AND MSVC)
    set(GTK_ROOT_DIR "C:/gtk")

    set(GTK_INCLUDE_DIRS
        "${GTK_ROOT_DIR}/include/gtk-3.0"
        "${GTK_ROOT_DIR}/include/pango-1.0"
        "${GTK_ROOT_DIR}/include/harfbuzz"
        "${GTK_ROOT_DIR}/include/cairo"
        "${GTK_ROOT_DIR}/include/fribidi"
        "${GTK_ROOT_DIR}/include/atk-1.0"
        "${GTK_ROOT_DIR}/include/pixman-1"
        "${GTK_ROOT_DIR}/include/libpng16"
        "${GTK_ROOT_DIR}/include/freetype2"
        "${GTK_ROOT_DIR}/include/gdk-pixbuf-2.0"
        "${GTK_ROOT_DIR}/include/harfbuzz"
    )
    set(GTK_LIBRARY_DIRS "${GTK_ROOT_DIR}/lib")

    find_library(GTK_LIBRARY NAME gtk-3.0 HINTS ${GTK_LIBRARY_DIRS})
    find_library(GDK_LIBRARY NAME gdk-3.0 HINTS ${GTK_LIBRARY_DIRS})
    find_library(ATK_LIBRARY NAME atk-1.0 HINTS ${GTK_LIBRARY_DIRS})
    find_library(PANGO_LIBRARY NAME pango-1.0 HINTS ${GTK_LIBRARY_DIRS})
    find_library(PANGOCAIRO_LIBRARY NAME pangocairo-1.0 HINTS ${GTK_LIBRARY_DIRS})
    find_library(CAIRO_LIBRARY NAME cairo HINTS ${GTK_LIBRARY_DIRS})
    find_library(CAIRO_GOBJECT_LIBRARY NAME cairo-gobject HINTS ${GTK_LIBRARY_DIRS})
    find_library(HARFBUZZ_LIBRARY NAME harfbuzz HINTS ${GTK_LIBRARY_DIRS})
    find_library(GDK_PIXBUF_LIBRARY NAME gdk_pixbuf-2.0 HINTS ${GTK_LIBRARY_DIRS})
    find_library(GIO_LIBRARY NAME gio-2.0 HINTS ${GTK_LIBRARY_DIRS})
    find_library(GOBJECT_LIBRARY NAME gobject-2.0 HINTS ${GTK_LIBRARY_DIRS})
    find_library(GLIB_LIBRARY NAME glib-2.0 HINTS ${GTK_LIBRARY_DIRS})
    find_library(INTL_LIBRARY NAME intl HINTS ${GTK_LIBRARY_DIRS})
    list(APPEND GTK_LIBRARIES
        ${GTK_LIBRARY}
        ${GDK_LIBRARY}
        ${ATK_LIBRARY}
        ${PANGO_LIBRARY}
        ${PANGOCAIRO_LIBRARY}
        ${CAIRO_LIBRARY}
        ${CAIRO_GOBJECT_LIBRARY}
        ${HARFBUZZ_LIBRARY}
        ${GDK_PIXBUF_LIBRARY}
        ${GIO_LIBRARY}
        ${GOBJECT_LIBRARY}
        ${GLIB_LIBRARY}
        ${INTL_LIBRARY}
    )

    list(APPEND OUTPUT_VARS GTK_INCLUDE_DIRS GTK_LIBRARIES)
else()
    find_package(PkgConfig)
    pkg_check_modules(GTK REQUIRED gtk+-3.0)
    list(APPEND OUTPUT_VARS GTK_INCLUDE_DIRS GTK_LIBRARIES)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Gtk DEFAULT_MSG ${OUTPUT_VARS})

mark_as_advanced(${OUTPUT_VARS})

unset(OUTPUT_VARS)
