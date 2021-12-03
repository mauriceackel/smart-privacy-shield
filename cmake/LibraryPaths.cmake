# Gets all link directories for all specified targets
function(lp_get_dependency_paths dependency_paths_var) #pass TARGETS as additional paramerters
    # Get all library paths for passed targets
    math(EXPR ARGC "${ARGC}-1")
    foreach(INDEX RANGE 1 ${ARGC})
        set(TARGET ${ARGV${INDEX}})
        get_target_property(LIBRARY_PATH ${TARGET} LINK_DIRECTORIES)
        list(APPEND LIBRARY_PATHS ${LIBRARY_PATH})
    endforeach()

    if(WIN32)
        # On windows, required dll often reside in bin folder
        foreach(LIBRARY_PATH ${LIBRARY_PATHS})
            get_filename_component(ROOT_PATH ${LIBRARY_PATH} DIRECTORY)
            list(APPEND DEPENDENCY_PATHS "${ROOT_PATH}/bin")
        endforeach()
    else()
        set(DEPENDENCY_PATHS ${LIBRARY_PATHS})
    endif()

    set(${dependency_paths_var} ${DEPENDENCY_PATHS} PARENT_SCOPE)
endfunction()