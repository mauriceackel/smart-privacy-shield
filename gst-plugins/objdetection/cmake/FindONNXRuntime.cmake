# FindONNXRuntime
# ===============
#
# Find an ONNX Runtime installation.
# ONNX Runtime is a cross-platform inference and training machine-learning
# accelerator.
#
# Output variable
# ---------------
# 
#   ONNXRUNTIME_FOUND           True if headers and requested libraries were found
#   ONNXRUNTIME_LIBRARIES       Component libraries to be linked.
#   ONNXRUNTIME_LIBRARY_DIRS    Directories of component libraries to be linked.
#   ONNXRUNTIME_INCLUDE_DIRS    Include directories.
#

MACRO(SUBDIRLIST result curdir)
    file(GLOB children RELATIVE ${curdir} ${curdir}/*)
    set(dirlist "")
    foreach(child ${children})
    if(IS_DIRECTORY ${curdir}/${child})
        list(APPEND dirlist ${child})
    endif()
    endforeach()
    set(${result} ${dirlist})
ENDMACRO()

MACRO(GET_ARCHITECTURE result)
    if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
        set(${result} "x64")
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64")
        set(${result} "x64")
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
        # cmake reports AMD64 on Windows, but we might be building for 32-bit.
        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(${result} "x64")
        else()
            set(${result} "x86")
        endif()
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86")
        set(${result} "x86")
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i386")
        set(${result} "x86")
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i686")
        set(${result} "x86")
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm")
        if (CMAKE_SIZEOF_VOID_P EQUAL 64)
            set(${result} "arm64")
        else()
            set(${result} "arm")
        endif()
    else()
        message(FATAL_ERROR "Unknown processor:" ${CMAKE_SYSTEM_PROCESSOR})
    endif()
ENDMACRO()

if(WIN32)
    # Try to find a nuget package
    string(REGEX REPLACE "\\\\" "/" ONNXRUNTIME_BASE_DIR "$ENV{HOMEPATH}/.nuget/packages/microsoft.ml.onnxruntime") 
    subdirlist(ONNXRUNTIME_VERSION_DIRS ${ONNXRUNTIME_BASE_DIR})
    
    if(NOT ONNXRUNTIME_VERSION_DIRS)
        message(WARNING "No onnxruntime nuget package found. Please install a package!")
    else()
        # Sort by version descending
        list(SORT ONNXRUNTIME_VERSION_DIRS COMPARE NATURAL ORDER DESCENDING)
        # Get latest version
        list(GET ONNXRUNTIME_VERSION_DIRS 0 ONNXRUNTIME_VERSION)
        
        # Get include dir
        set(ONNXRUNTIME_INCLUDE_DIRS "${ONNXRUNTIME_BASE_DIR}/${ONNXRUNTIME_VERSION}/build/native/include")

        get_architecture(ARCH)
        # Get include dir
        set(ONNXRUNTIME_LIBRARY_DIRS "${ONNXRUNTIME_BASE_DIR}/${ONNXRUNTIME_VERSION}/runtimes/win-${ARCH}/native")
        # Get libraries
        find_library(ONNXRUNTIME_LIBRARIES onnxruntime libonnxruntime PATHS "${ONNXRUNTIME_LIBRARY_DIRS}" CMAKE_FIND_ROOT_PATH_BOTH)
    endif()
else()
    # Check long path
    find_path(ONNXRUNTIME_INCLUDE_DIRS onnxruntime/core/session/onnxruntime_c_api.h CMAKE_FIND_ROOT_PATH_BOTH)
    if(ONNXRUNTIME_INCLUDE_DIRS)
        get_filename_component(ORT_INCLUDE_ROOT ${ONNXRUNTIME_INCLUDE_DIRS} DIRECTORY)

        # Path mangling for easier include
        if(NOT (ONNXRUNTIME_INCLUDE_DIRS MATCHES "^.*/onnxruntime/core/session.*$"))
            set(ONNXRUNTIME_INCLUDE_DIRS "${ONNXRUNTIME_INCLUDE_DIRS}/onnxruntime/core/session/" CACHE PATH "Path to onnxruntime headers" FORCE)
        endif()
    else()
        # Long path does not exist, check short path 
        find_path(ONNXRUNTIME_INCLUDE_DIRS onnxruntime_c_api.h CMAKE_FIND_ROOT_PATH_BOTH)
        get_filename_component(ORT_INCLUDE_ROOT ${ONNXRUNTIME_INCLUDE_DIRS} DIRECTORY)
    endif()

    # Get library
    find_library(ONNXRUNTIME_LIBRARIES onnxruntime libonnxruntime PATHS "${ORT_INCLUDE_ROOT}" CMAKE_FIND_ROOT_PATH_BOTH)
    # Get library dir
    get_filename_component(ONNXRUNTIME_LIBRARY_DIRS ${ONNXRUNTIME_LIBRARIES} DIRECTORY)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ONNXRuntime DEFAULT_MSG ONNXRUNTIME_INCLUDE_DIRS ONNXRUNTIME_LIBRARY_DIRS ONNXRUNTIME_LIBRARIES)

mark_as_advanced(ONNXRUNTIME_INCLUDE_DIRS ONNXRUNTIME_LIBRARY_DIRS ONNXRUNTIME_LIBRARIES)
