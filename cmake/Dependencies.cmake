# Dependencies.cmake
# Manages third-party and framework dependencies

# Qt 6 Widgets for GUI
if(QVM_BUILD_GUI)
    find_package(Qt6 COMPONENTS Core Gui Widgets Svg REQUIRED)
    message(STATUS "Found Qt6: ${Qt6_VERSION} (Widgets, Gui, Core, Svg)")
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
