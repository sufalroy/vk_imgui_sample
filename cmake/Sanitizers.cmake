function(enable_sanitizers target)
    if(NOT ENABLE_SANITIZERS AND NOT ENABLE_TSAN AND NOT ENABLE_MSAN)
        return()
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(WARNING "Sanitizers requested in a Release build — ignoring.")
        return()
    endif()

    # ── ASan + UBSan + LSan ────────────────────────────────────────────────
    if(ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            # MSVC only supports ASan; UBSan is not available.
            target_compile_options(${target} PRIVATE
                    /fsanitize=address
                    /Zi          # Full debug info required for ASan symbolisation
                    /Oy-         # Disable frame-pointer omission for better stack traces
            )
            target_link_options(${target} PRIVATE
                    /INCREMENTAL:NO  # Incompatible with ASan
                    /DEBUG:FULL
            )

        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # Clang supports the full sanitizer suite including UBSan sub-checks.
            #
            # unsigned-shift-base and unsigned-integer-overflow are Clang
            # extensions to -fsanitize=integer that flag mathematically valid
            # unsigned arithmetic (wrapping / shifting is defined for unsigned
            # types). They produce false positives inside libc++ and third-party
            # headers, so we opt them back out after enabling the integer group.
            set(_san_flags
                    -fsanitize=address
                    -fsanitize=undefined
                    -fsanitize=leak
                    -fsanitize=integer          # Clang-only: integer overflow checks
                    -fsanitize=nullability      # Clang-only: _Nonnull annotation checks
                    -fno-sanitize=unsigned-shift-base        # FP in libc++ hash / bitops
                    -fno-sanitize=unsigned-integer-overflow  # Defined behaviour for unsigned
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

    # ── TSan — mutually exclusive with ASan ────────────────────────────────
    if(ENABLE_TSAN)
        if(ENABLE_SANITIZERS)
            message(WARNING
                    "ENABLE_TSAN and ENABLE_SANITIZERS (ASan) are mutually exclusive — TSan ignored.")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target} PRIVATE
                    -fsanitize=thread
                    -fno-omit-frame-pointer
                    -g
            )
            target_link_options(${target} PRIVATE -fsanitize=thread)
        else()
            message(WARNING "ThreadSanitizer is not available for MSVC.")
        endif()
    endif()

    # ── MSan — Clang-only, mutually exclusive with ASan / TSan ────────────
    if(ENABLE_MSAN)
        if(ENABLE_SANITIZERS OR ENABLE_TSAN)
            message(WARNING
                    "ENABLE_MSAN cannot be combined with ASan or TSan — MSan ignored.")
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