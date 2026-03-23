function(enable_static_analyzers target)
    if(NOT ENABLE_STATIC_ANALYZERS)
        return()
    endif()

    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CLANG_TIDY_CHECKS
            -*                                              # Disable all by default
            bugprone-*                                      # Likely bugs
            cert-*                                          # CERT secure coding
            clang-analyzer-*                                # Clang static analyser
            concurrency-*                                   # Thread safety
            cppcoreguidelines-*                             # C++ Core Guidelines
            misc-*
            modernize-*                                     # Suggest modern C++ idioms
            performance-*
            portability-*
            readability-*
            -readability-identifier-length                  
            -readability-magic-numbers                      
            -readability-function-cognitive-complexity      
            -cppcoreguidelines-avoid-magic-numbers
            -cppcoreguidelines-avoid-c-arrays               # std::array preferred but C arrays exist
            -modernize-use-trailing-return-type             # We use trailing return types already
            -cppcoreguidelines-pro-bounds-array-to-pointer-decay
            -cppcoreguidelines-pro-bounds-pointer-arithmetic
            -cppcoreguidelines-owning-memory                # RAII handles ownership; raw new/delete absent
            -modernize-use-nodiscard                        # Too noisy during tutorial
        )

        string(REPLACE ";" "," CLANG_TIDY_CHECKS_STR "${CLANG_TIDY_CHECKS}")

        set_target_properties(${target} PROPERTIES
            CXX_CLANG_TIDY
            "${CLANG_TIDY_EXE}\
            ;--checks=${CLANG_TIDY_CHECKS_STR}\
            ;--warnings-as-errors=*\
            ;--header-filter=${CMAKE_CURRENT_SOURCE_DIR}/.*\
            ;--use-color\
            ;-p=${CMAKE_BINARY_DIR}"
        )
        message(STATUS "clang-tidy: ${CLANG_TIDY_EXE}")
    else()
        message(STATUS "clang-tidy: not found — skipping")
    endif()

    find_program(CPPCHECK_EXE NAMES cppcheck)
    if(CPPCHECK_EXE)
        set(CPPCHECK_ARGS
            --enable=all
            --std=c++23           # Match project standard
            --language=c++
            --platform=win64
            --inconclusive
            --inline-suppr        # Honour // cppcheck-suppress comments in source
            --template=gcc
            --error-exitcode=0    # Don't break the build; treat as informational
            --quiet
            --suppress=*:*external/*
            --suppress=*:*vendor/*
            --suppress=unmatchedSuppression
            --suppress=missingIncludeSystem
        )

        set_target_properties(${target} PROPERTIES
            CXX_CPPCHECK "${CPPCHECK_EXE};${CPPCHECK_ARGS}"
        )
        message(STATUS "cppcheck: ${CPPCHECK_EXE}")
    else()
        message(STATUS "cppcheck: not found — skipping")
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE
           /analyze
           /analyze:WX-
           /analyze:stacksize65536
           /analyze:external-          # <-- do not analyse external headers
           /external:anglebrackets     # <-- anything included with <> is external
           /external:W0                # <-- zero warnings from external headers
        )
        message(STATUS "MSVC /analyze: enabled")
    endif()

endfunction()