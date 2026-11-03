# Dependencies.cmake
# Manages third-party and framework dependencies

# Qt 6 Widgets for GUI
if(QVM_BUILD_GUI)
    if(CMAKE_CROSSCOMPILING AND WIN32)
        set(QVM_HAVE_QT6 OFF)
        message(STATUS "Cross-compiling for Windows; enabling native Win32 GUI editor")
    elseif(WIN32)
        find_package(Qt6 COMPONENTS Core Gui Widgets Svg QUIET)
        if(Qt6_FOUND)
            set(QVM_HAVE_QT6 ON)
            message(STATUS "Found Qt6: ${Qt6_VERSION} (Widgets, Gui, Core, Svg)")
        else()
            set(QVM_HAVE_QT6 OFF)
            message(STATUS "Qt6 not found on Windows; enabling native Win32 GUI editor")
        endif()
    else()
        find_package(Qt6 COMPONENTS Core Gui Widgets Svg REQUIRED)
        set(QVM_HAVE_QT6 ON)
        message(STATUS "Found Qt6: ${Qt6_VERSION} (Widgets, Gui, Core, Svg)")
    endif()
endif()

# GoogleTest for unit tests
if(QVM_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # Prevent overriding parent project's compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    message(STATUS "GoogleTest v1.14.0 configured")
endif()
