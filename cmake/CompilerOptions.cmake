# CompilerOptions.cmake
# Compiler flags and settings across platforms

if(MSVC)
    add_compile_options(
        /W3
        /permissive-
        /Zc:__cplusplus
        /EHsc
        /utf-8
        /bigobj
        /FS
    )
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(
            -Wno-nullability-completeness
        )
    endif()
endif()

# Position Independent Code for libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
