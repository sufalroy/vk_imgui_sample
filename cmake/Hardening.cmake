function(set_project_hardening target)
    if(NOT ENABLE_HARDENING)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${target} PRIVATE
            /sdl
            /guard:cf
            /GS
            $<$<CONFIG:Debug>:/RTC1>
            $<$<CONFIG:Debug>:/RTCs>
            $<$<CONFIG:Debug>:/RTCu>
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Release>:/Ob3>
            $<$<CONFIG:Release>:/Oi>
            $<$<CONFIG:Release>:/Ot>
            $<$<CONFIG:Release>:/Oy>
            $<$<CONFIG:Release>:/GT>
            $<$<CONFIG:Release>:/GL>
            $<$<CONFIG:Release>:/Gw>
            $<$<CONFIG:Release>:/Gy>
            $<$<CONFIG:Debug>:/Od>
            $<$<CONFIG:Debug>:/Ob0>
            $<$<CONFIG:Debug>:/JMC>
            $<$<CONFIG:Debug>:/Zi>
        )

        target_link_options(${target} PRIVATE
            /GUARD:CF
            /DYNAMICBASE
            /NXCOMPAT
            /HIGHENTROPYVA
            $<$<CONFIG:Release>:/LTCG>
            $<$<CONFIG:Release>:/OPT:REF>
            $<$<CONFIG:Release>:/OPT:ICF>
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
            -D_FORTIFY_SOURCE=2
            -fstack-protector-strong
            -fstack-clash-protection
            -fcf-protection=full
            -fPIE
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:Release>:-march=native>
            $<$<CONFIG:Release>:-mtune=native>
            $<$<CONFIG:Release>:-flto>
            $<$<CONFIG:Release>:-ffast-math>
            $<$<CONFIG:Debug>:-Og>
            $<$<CONFIG:Debug>:-g3>
            $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        )

        target_link_options(${target} PRIVATE
            -Wl,-z,relro
            -Wl,-z,now
            -Wl,-z,noexecstack
            -pie
            $<$<CONFIG:Release>:-flto>
        )
    endif()
endfunction()