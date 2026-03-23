function(set_project_hardening target)
    if(NOT ENABLE_HARDENING)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${target} PRIVATE
            /GS         # Buffer security check (stack cookies)
            /sdl        # Additional SDL security checks (superset of /GS)
            /guard:cf   # Control Flow Guard — hardens indirect calls/jumps
            # Runtime checks are Debug-only; they are incompatible with optimised builds.
            $<$<CONFIG:Debug>:/RTC1>    # /RTC1 = /RTCs + /RTCu: stack frame + uninit variable
        )
        target_link_options(${target} PRIVATE
            /GUARD:CF       # CFG must also be set at link time
            /DYNAMICBASE    # ASLR — randomise base address at load
            /NXCOMPAT       # Mark as compatible with DEP (Data Execution Prevention)
            /HIGHENTROPYVA  # 64-bit high-entropy ASLR
        )

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_definitions(${target} PRIVATE
            # _FORTIFY_SOURCE replaces unsafe libc calls with checked variants.
            # Must be a definition, not a -D flag passed via compile_options,
            # and requires optimisation ≥ -O1 to take effect (guaranteed in Release).
            $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
        )
        target_compile_options(${target} PRIVATE
            -fstack-protector-strong    # Protect stack frames with canaries
            # -fstack-clash-protection and -fcf-protection are x86/x86-64 only.
            $<$<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},AMD64>:-fstack-clash-protection>
            $<$<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},x86_64>:-fstack-clash-protection>
            $<$<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},AMD64>:-fcf-protection=full>
            $<$<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},x86_64>:-fcf-protection=full>
        )
        target_link_options(${target} PRIVATE
            -Wl,-z,relro        # Mark GOT read-only after relocations (RELRO)
            -Wl,-z,now          # Resolve all symbols at load (full RELRO)
            -Wl,-z,noexecstack  # Mark stack as non-executable
        )
    endif()

endfunction()