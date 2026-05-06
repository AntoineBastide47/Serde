# serde_generate(<target> PATHS <path...> [COMPILER_ARGS <arg...>])
#
# Runs HeaderForge on the given paths as a PRE_BUILD step for <target>.
# HeaderForge generates .gen.hpp files and injects SERIALIZE_* macros into the
# matching headers. Only changed files are reprocessed (HeaderForge is incremental).
#
# Arguments:
#   PATHS         One or more files or directories to process (required).
#   COMPILER_ARGS Extra flags forwarded to HeaderForge via --compilerArgs
#                 (e.g. -I${CMAKE_CURRENT_SOURCE_DIR}/include).
#
# Example:
#   add_subdirectory(vendor/Serde)
#
#   add_executable(MyGame src/main.cpp)
#   target_link_libraries(MyGame PRIVATE serde)
#
#   serde_generate(MyGame
#       PATHS ${CMAKE_CURRENT_SOURCE_DIR}/src
#       COMPILER_ARGS -I${CMAKE_CURRENT_SOURCE_DIR}/include
#   )
function(serde_generate TARGET)
    cmake_parse_arguments(ARG "" "" "PATHS;COMPILER_ARGS" ${ARGN})

    if(NOT ARG_PATHS)
        message(FATAL_ERROR "serde_generate: PATHS is required")
    endif()

    # Ensure HeaderForge is built before the target that needs generated code.
    add_dependencies(${TARGET} HeaderForge)

    set(HF_CMD $<TARGET_FILE:HeaderForge> --parse ${ARG_PATHS})
    if(ARG_COMPILER_ARGS)
        list(APPEND HF_CMD --compilerArgs ${ARG_COMPILER_ARGS})
    endif()

    add_custom_command(
        TARGET ${TARGET}
        PRE_BUILD
        COMMAND ${HF_CMD}
        COMMENT "HeaderForge: generating serialization code for ${TARGET}"
        VERBATIM
    )
endfunction()
