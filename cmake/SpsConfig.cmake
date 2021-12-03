get_filename_component(SELF_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(${SELF_DIR}/SpsTargets.cmake)

get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
if(_IMPORT_PREFIX STREQUAL "/")
  set(_IMPORT_PREFIX "")
endif()

set(SPS_INCLUDE_DIRS "${_IMPORT_PREFIX}/include")
set(SPS_LIBRARY_DIRS "${_IMPORT_PREFIX}/lib")
set(SPS_LIBRARIES "sps")

# Cleanup temporary variables.
set(_IMPORT_PREFIX)
