# FindCppWinRT
# ===============
#
# Generates the C++/WinRT for the current platform and palces them in the binary directory.
#
# Output variable
# ---------------
# 
#   CPPWINRT_FOUND              True if headers were generated correctly
#   CPPWINRT_INCLUDE_DIRS      Include directories.
#

if(NOT WIN32)
    message(FATAL_ERROR "Trying to get C++/WinRT but not on a Windows system!")
endif()

if(NOT CPPWINRT_PATH)
    message(FATAL_ERROR "No path to cppwinrt.exe specified. Please specify a path to the tool (version >= v2.0.210930.14) using -DCPPWINRT_PATH=...")
endif()

# Define location of C++/WinRT headers
set(CPPWINRT_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/cppwinrt")

# Generate C++/WinRT headers
exec_program(${CPPWINRT_PATH} ARGS -input local -output ${CPPWINRT_INCLUDE_DIRS})

# Check compatibility
if(NOT (CMAKE_CXX_STANDARD EQUAL 20))
    message(FATAL_ERROR "C++/WinRT requires C++20")
endif()

# Enable coroutines in GCC
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fcoroutines>")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # No arguments needed
else()
    message(FATAL_ERROR "C++ compiler ${CMAKE_CXX_COMPILER_ID} may not have C++20 coroutine support. Please manually check and adapt this file.")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CppWinRT DEFAULT_MSG CPPWINRT_INCLUDE_DIRS)

mark_as_advanced(CPPWINRT_INCLUDE_DIRS)
