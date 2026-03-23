function(set_project_warnings target warnings_as_errors)

    set(MSVC_WARNINGS
        /W4             # High warning level (not /Wall — that warns on STL internals)
        /w14242         # Conversion: possible loss of data (int → char)
        /w14254         # Larger bitfield → smaller bitfield, possible data loss
        /w14263         # Member function doesn't override base virtual function
        /w14265         # Class with virtual functions but no virtual destructor
        /w14287         # Unsigned / negative constant mismatch
        /we4289         # Non-standard extension: loop control variable used outside loop
        /w14296         # Expression always true/false
        /w14311         # Pointer truncation from 'type' to 'type'
        /w14545         # Expression before comma evaluates to a function missing arg list
        /w14546         # Function call before comma missing argument list
        /w14547         # Operator before comma has no effect
        /w14549         # Operator before comma has no effect; did you intend '!='?
        /w14555         # Expression has no effect; expected expression with side-effect
        /w14619         # #pragma warning: there is no warning number 'XXXX'
        /w14640         # Enable warning on thread un-safe static member initialisation
        /w14826         # Conversion from 'type1' to 'type2' is sign-extended
        /w14905         # Wide string literal cast to 'LPSTR'
        /w14906         # String literal cast to 'LPWSTR'
        /w14928         # Illegal copy-initialisation; more than one user conversion applied
        /permissive-    # Standards-conformance mode (disables MSVC extensions)
        /Zc:__cplusplus # Report correct __cplusplus value instead of 199711L
        /Zc:preprocessor # Use conformant preprocessor (required for __VA_OPT__ etc.)
        /utf-8          # Source and execution charset = UTF-8
    )

    set(CLANG_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow                # Variable shadows another
        -Wnon-virtual-dtor      # Class with virtual functions but non-virtual dtor
        -Wold-style-cast        # C-style casts
        -Wcast-align            # Potential alignment issues on pointer cast
        -Wunused
        -Woverloaded-virtual    # Hides virtual method from base
        -Wconversion            # Implicit type conversions
        -Wsign-conversion       # Implicit sign conversions
        -Wnull-dereference
        -Wdouble-promotion      # float → double implicit promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Wno-unknown-pragmas    # Don't warn on MSVC pragmas in shared headers
    )

    set(GCC_WARNINGS
        ${CLANG_WARNINGS}
        -Wlogical-op            # Suspicious use of logical operators
        -Wuseless-cast          # Cast to the same type
        -Wno-unknown-pragmas
    )

    if(warnings_as_errors)
        if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            list(APPEND MSVC_WARNINGS /WX)
        else()
            list(APPEND CLANG_WARNINGS -Werror)
            list(APPEND GCC_WARNINGS   -Werror)
        endif()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${target} PRIVATE ${MSVC_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE ${CLANG_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(${target} PRIVATE ${GCC_WARNINGS})
    endif()

endfunction()