function(enable_sanitizers target)
    if(NOT ENABLE_SANITIZERS AND NOT ENABLE_TSAN AND NOT ENABLE_MSAN)
        return()
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(WARNING "Sanitizers requested in a Release build — ignoring.")
        return()
    endif()

    if(ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            # MSVC only supports ASan; UBSan is not available.
            target_compile_options(${target} PRIVATE
                /fsanitize=address
                /Zi         # Full debug info required for ASan symbolisation
                /Oy-        # Disable frame-pointer omission for better stack traces
            )
            target_link_options(${target} PRIVATE
                /INCREMENTAL:NO  # Incompatible with ASan
                /DEBUG:FULL
            )
            # ASAN_OPTIONS is a *runtime* environment variable, not a macro.
            # Set it in your launch configuration or the VS debugger environment,
            # e.g.: ASAN_OPTIONS=windows_hook_rtl_allocators=true:detect_stack_use_after_return=1

        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # Clang supports the full sanitizer suite including UBSan sub-checks.
            set(_san_flags
                -fsanitize=address
                -fsanitize=undefined
                -fsanitize=leak
                -fsanitize=integer          # Clang-only: integer overflow checks
                -fsanitize=nullability      # Clang-only: _Nonnull annotation checks
                -fno-sanitize-recover=all   # Abort on first error rather than continue
                -fno-omit-frame-pointer
                -g
            )
            target_compile_options(${target} PRIVATE ${_san_flags})
            target_link_options(${target}    PRIVATE ${_san_flags})

        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            # GCC does not support -fsanitize=integer or -fsanitize=nullability.
            set(_san_flags
                -fsanitize=address
                -fsanitize=undefined
                -fsanitize=leak
                -fno-sanitize-recover=all
                -fno-omit-frame-pointer
                -g
            )
            target_compile_options(${target} PRIVATE ${_san_flags})
            target_link_options(${target}    PRIVATE ${_san_flags})
        endif()
    endif()

    # TSan is mutually exclusive with ASan. Don't enable both.
    if(ENABLE_TSAN)
        if(ENABLE_SANITIZERS)
            message(WARNING "ENABLE_TSAN and ENABLE_SANITIZERS (ASan) are mutually exclusive — TSan ignored.")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target} PRIVATE -fsanitize=thread -g -fno-omit-frame-pointer)
            target_link_options(${target}    PRIVATE -fsanitize=thread)
        else()
            message(WARNING "ThreadSanitizer is not available for MSVC.")
        endif()
    endif()

    # MSan is Clang-only and mutually exclusive with ASan/TSan.
    if(ENABLE_MSAN)
        if(ENABLE_SANITIZERS OR ENABLE_TSAN)
            message(WARNING "ENABLE_MSAN cannot be combined with ASan or TSan — MSan ignored.")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE
                -fsanitize=memory
                -fsanitize-memory-track-origins=2  # Full origin tracking
                -fno-optimize-sibling-calls         # Preserve full call stack
                -fno-omit-frame-pointer
                -g
            )
            target_link_options(${target} PRIVATE -fsanitize=memory)
        else()
            message(WARNING "MemorySanitizer is only available for Clang.")
        endif()
    endif()

endfunction()