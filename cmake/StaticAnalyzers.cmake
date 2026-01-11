function(enable_static_analyzers target)
    if(NOT ENABLE_STATIC_ANALYZERS)
        return()
    endif()

    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CLANG_TIDY_CHECKS
            -*,
            bugprone-*,
            cert-*,
            clang-analyzer-*,
            concurrency-*,
            cppcoreguidelines-*,
            misc-*,
            modernize-*,
            performance-*,
            portability-*,
            readability-*,
            -readability-identifier-length,
            -readability-magic-numbers,
            -readability-function-cognitive-complexity,
            -cppcoreguidelines-avoid-magic-numbers,
            -cppcoreguidelines-avoid-c-arrays,
            -modernize-use-trailing-return-type,
            -cppcoreguidelines-pro-bounds-array-to-pointer-decay,
            -cppcoreguidelines-pro-bounds-pointer-arithmetic,
            -cppcoreguidelines-owning-memory
        )
        
        string(REPLACE ";" "," CLANG_TIDY_CHECKS_STR "${CLANG_TIDY_CHECKS}")
        
        set_target_properties(${target} PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--checks=${CLANG_TIDY_CHECKS_STR};--warnings-as-errors=*;--header-filter=.*;--use-color;-p=${CMAKE_BINARY_DIR}"
        )
    endif()

    find_program(CPPCHECK_EXE NAMES cppcheck)
    if(CPPCHECK_EXE)
        set(CPPCHECK_ARGS
            --enable=all
            --std=c++20
            --language=c++
            --platform=win64
            --inconclusive
            --inline-suppr
            --template=gcc
            --error-exitcode=0
            --quiet
            --suppress=*:*external/*
            --suppress=*:*imgui/*
            --suppress=*:*glfw/*
            --suppress=unmatchedSuppression
            --suppress=missingIncludeSystem
            -I${CMAKE_CURRENT_SOURCE_DIR}/src
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        
        set_target_properties(${target} PROPERTIES CXX_CPPCHECK "${CPPCHECK_EXE};${CPPCHECK_ARGS}")
        
        add_custom_target(${target}_cppcheck
            COMMAND ${CPPCHECK_EXE} ${CPPCHECK_ARGS} --project=${CMAKE_BINARY_DIR}/compile_commands.json
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /analyze
            /analyze:WX-
            /analyze:log${CMAKE_BINARY_DIR}/analysis.log
            /analyze:stacksize32768
        )
    endif()
endfunction()