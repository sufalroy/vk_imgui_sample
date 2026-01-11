function(enable_sanitizers target)
    if(NOT ENABLE_SANITIZERS AND NOT ENABLE_TSAN AND NOT ENABLE_MSAN)
        return()
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(WARNING "Sanitizers should not be enabled in Release builds")
        return()
    endif()

    if(ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            target_compile_options(${target} PRIVATE /fsanitize=address /Zi /Oy-)
            target_link_options(${target} PRIVATE /INCREMENTAL:NO /DEBUG:FULL)
            target_compile_definitions(${target} PRIVATE
                ASAN_OPTIONS=windows_hook_rtl_allocators=true:detect_stack_use_after_return=1
            )
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            set(SANITIZER_FLAGS
                -fsanitize=address
                -fsanitize=undefined
                -fsanitize=leak
                -fsanitize=integer
                -fsanitize=nullability
                -fno-sanitize-recover=all
                -fno-omit-frame-pointer
                -g
            )
            target_compile_options(${target} PRIVATE ${SANITIZER_FLAGS})
            target_link_options(${target} PRIVATE ${SANITIZER_FLAGS})
        endif()
    endif()

    if(ENABLE_TSAN)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target} PRIVATE -fsanitize=thread -g)
            target_link_options(${target} PRIVATE -fsanitize=thread)
        else()
            message(WARNING "ThreadSanitizer not available for MSVC")
        endif()
    endif()

    if(ENABLE_MSAN)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE
                -fsanitize=memory
                -fsanitize-memory-track-origins=2
                -fno-optimize-sibling-calls
                -g
            )
            target_link_options(${target} PRIVATE -fsanitize=memory)
        else()
            message(WARNING "MemorySanitizer only available for Clang")
        endif()
    endif()
endfunction()