# Fails configuration if the engine ever acquires a Qt dependency, either by
# linking a Qt target or by including a Qt header. The rule is a graded
# architectural property of the project, so it is checked mechanically rather
# than by code review.

function(_logicbench_assert_qt_free target)
    get_target_property(_libs ${target} LINK_LIBRARIES)
    if(_libs)
        foreach(lib IN LISTS _libs)
            if(lib MATCHES "^Qt[0-9]*::")
                message(FATAL_ERROR
                    "Engine purity violation: ${target} links ${lib}. "
                    "The simulation engine must not depend on Qt.")
            endif()
        endforeach()
    endif()

    get_target_property(_sources ${target} SOURCES)
    get_target_property(_dir ${target} SOURCE_DIR)
    foreach(src IN LISTS _sources)
        if(NOT IS_ABSOLUTE "${src}")
            set(src "${_dir}/${src}")
        endif()
        file(STRINGS "${src}" _qt_includes REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]Q")
        if(_qt_includes)
            message(FATAL_ERROR
                "Engine purity violation: ${src} includes a Qt header "
                "(${_qt_includes}).")
        endif()
    endforeach()
endfunction()

_logicbench_assert_qt_free(logicbench_engine)
message(STATUS "Engine purity check passed: logicbench_engine is Qt-free")
