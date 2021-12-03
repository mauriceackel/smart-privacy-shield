# FindOpenCV
# ===============
#
# Find a OpenCV installation.
#
# Output variable
# ---------------
# 
#   OPENCV_FOUND           True if headers and requested libraries were found
#   OPENCV_LIBRARIES       Component libraries to be linked.
#   OPENCV_LIBRARY_DIRS    Directories of component libraries to be linked.
#   OPENCV_INCLUDE_DIRS    Include directories.
#

set(OUTPUT_VARS)

if(WIN32 AND MSVC)
    find_package(OpenCV REQUIRED CONFIG)
    set(OPENCV_LIBRARIES ${OpenCV_LIBS})
    set(OPENCV_LIBRARY_DIRS ${OpenCV_CONFIG_PATH})
    set(OPENCV_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS})

    list(APPEND OUTPUT_VARS OPENCV_INCLUDE_DIRS OPENCV_LIBRARIES)
else()
    find_library(OPENCV_LIBRARIES NAME opencv_world REQUIRED)
    get_filename_component(OPENCV_LIBRARY_DIRS ${OPENCV_LIBRARIES} DIRECTORY)

    # Check long path
    find_path(OPENCV_INCLUDE_DIRS "opencv4/opencv2/core.hpp" CMAKE_FIND_ROOT_PATH_BOTH)
    if(OPENCV_INCLUDE_DIRS)
        # Path mangling for easier include
        if(NOT (OPENCV_INCLUDE_DIRS MATCHES "^.*/opencv4.*$"))
            set(OPENCV_INCLUDE_DIRS "${OPENCV_INCLUDE_DIRS}/opencv4" CACHE PATH "Path to opencv headers" FORCE)
        endif()
    else()
        # Long path does not exist, check short path 
        find_path(OPENCV_INCLUDE_DIRS "opencv2/core.hpp")
    endif()

    list(APPEND OUTPUT_VARS OPENCV_INCLUDE_DIRS OPENCV_LIBRARIES)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCV DEFAULT_MSG ${OUTPUT_VARS})

mark_as_advanced(${OUTPUT_VARS})

unset(OUTPUT_VARS)
