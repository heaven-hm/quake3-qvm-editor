cmake_minimum_required(VERSION 3.16)

project(quake3_qvm_editor
    VERSION 0.1.1
    DESCRIPTION "Quake III Arena QVM Compiler & Decompiler Editor"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type" FORCE)
endif()

# Global build options
option(QVM_BUILD_GUI "Build Qt 6 Widgets Desktop GUI" ON)
option(QVM_BUILD_TESTS "Build GoogleTest automated unit tests" ON)

# Global output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Dependencies)

# ==============================================================================
# Native Quake III Arena Toolchain (LCC & Q3ASM)
# ==============================================================================

# Tools output directory
set(QVM_TOOLS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tools")

# 1. Q3ASM Assembler
set(Q3ASM_SOURCES
    third_party/quake3/q3asm/q3asm.c
    third_party/quake3/q3asm/cmdlib.c
)
add_executable(q3asm_tool ${Q3ASM_SOURCES})
set_target_properties(q3asm_tool PROPERTIES
    OUTPUT_NAME "q3asm"
    RUNTIME_OUTPUT_DIRECTORY "${QVM_TOOLS_OUTPUT_DIR}"
)
target_include_directories(q3asm_tool PRIVATE third_party/quake3/q3asm)
if(NOT MSVC)
    target_compile_options(q3asm_tool PRIVATE -Wno-implicit-function-declaration -Wno-format-security)
endif()

# 2. Q3CPP Preprocessor
set(Q3CPP_SOURCES
    third_party/quake3/lcc/cpp/cpp.c
    third_party/quake3/lcc/cpp/cpplex.c
    third_party/quake3/lcc/cpp/eval.c
    third_party/quake3/lcc/cpp/getopt.c
    third_party/quake3/lcc/cpp/hideset.c
    third_party/quake3/lcc/cpp/include.c
    third_party/quake3/lcc/cpp/macro.c
    third_party/quake3/lcc/cpp/nlist.c
    third_party/quake3/lcc/cpp/tokens.c
    third_party/quake3/lcc/cpp/unix.c
)
add_executable(q3cpp_tool ${Q3CPP_SOURCES})
set_target_properties(q3cpp_tool PROPERTIES
    OUTPUT_NAME "q3cpp"
    RUNTIME_OUTPUT_DIRECTORY "${QVM_TOOLS_OUTPUT_DIR}"
)
target_include_directories(q3cpp_tool PRIVATE third_party/quake3/lcc/cpp)
if(NOT MSVC)
    target_compile_options(q3cpp_tool PRIVATE -Wno-implicit-function-declaration -Wno-unused-result)
endif()

# 3. Q3RCC Intermediate Compiler
set(Q3RCC_SOURCES
    third_party/quake3/lcc/src/alloc.c
    third_party/quake3/lcc/src/bind.c
    third_party/quake3/lcc/src/bytecode.c
    third_party/quake3/lcc/src/dag.c
    third_party/quake3/lcc/src/dagcheck.c
    third_party/quake3/lcc/src/decl.c
    third_party/quake3/lcc/src/enode.c
    third_party/quake3/lcc/src/error.c
    third_party/quake3/lcc/src/event.c
    third_party/quake3/lcc/src/expr.c
    third_party/quake3/lcc/src/gen.c
    third_party/quake3/lcc/src/init.c
    third_party/quake3/lcc/src/inits.c
    third_party/quake3/lcc/src/input.c
    third_party/quake3/lcc/src/lex.c
    third_party/quake3/lcc/src/list.c
    third_party/quake3/lcc/src/main.c
    third_party/quake3/lcc/src/null.c
    third_party/quake3/lcc/src/output.c
    third_party/quake3/lcc/src/prof.c
    third_party/quake3/lcc/src/profio.c
    third_party/quake3/lcc/src/simp.c
    third_party/quake3/lcc/src/stmt.c
    third_party/quake3/lcc/src/string.c
    third_party/quake3/lcc/src/sym.c
    third_party/quake3/lcc/src/symbolic.c
    third_party/quake3/lcc/src/trace.c
    third_party/quake3/lcc/src/tree.c
    third_party/quake3/lcc/src/types.c
)
add_executable(q3rcc_tool ${Q3RCC_SOURCES})
set_target_properties(q3rcc_tool PROPERTIES
    OUTPUT_NAME "q3rcc"
    RUNTIME_OUTPUT_DIRECTORY "${QVM_TOOLS_OUTPUT_DIR}"
)
target_include_directories(q3rcc_tool PRIVATE third_party/quake3/lcc/src)
if(NOT MSVC)
    target_compile_options(q3rcc_tool PRIVATE -Wno-implicit-function-declaration -Wno-unused-result)
    target_link_libraries(q3rcc_tool PRIVATE m)
endif()

# 4. Q3LCC Compiler Driver
set(Q3LCC_SOURCES
    third_party/quake3/lcc/etc/lcc.c
    third_party/quake3/lcc/etc/bytecodelcc.c
)
add_executable(q3lcc_tool ${Q3LCC_SOURCES})
set_target_properties(q3lcc_tool PROPERTIES
    OUTPUT_NAME "q3lcc"
    RUNTIME_OUTPUT_DIRECTORY "${QVM_TOOLS_OUTPUT_DIR}"
)
target_include_directories(q3lcc_tool PRIVATE third_party/quake3/lcc/etc)
if(NOT MSVC)
    target_compile_options(q3lcc_tool PRIVATE -Wno-implicit-function-declaration -Wno-unused-result)
endif()

# Also build standard named "lcc" binary alias in tools dir
add_executable(lcc_tool ${Q3LCC_SOURCES})
set_target_properties(lcc_tool PROPERTIES
    OUTPUT_NAME "lcc"
    RUNTIME_OUTPUT_DIRECTORY "${QVM_TOOLS_OUTPUT_DIR}"
)
target_include_directories(lcc_tool PRIVATE third_party/quake3/lcc/etc)
if(NOT MSVC)
    target_compile_options(lcc_tool PRIVATE -Wno-implicit-function-declaration -Wno-unused-result)
endif()

# Copy prebuilt win32 binaries if on Windows
if(WIN32)
    add_custom_command(TARGET q3lcc_tool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/quake3/bin/win32"
            "${QVM_TOOLS_OUTPUT_DIR}/win32"
    )
endif()

# ==============================================================================
# QVM Compiler Core Library
# ==============================================================================
set(COMPILER_CORE_SOURCES
    source/common/Error.cpp
    source/common/FileUtils.cpp
    source/common/ProcessRunner.cpp
    source/format/QvmHeader.cpp
    source/compiler/CompilerConfig.cpp
    source/compiler/ModuleConfig.cpp
    source/compiler/ToolchainRunner.cpp
    source/compiler/BuildPipeline.cpp
    source/compiler/Compiler.cpp
)

add_library(qvm_compiler_core STATIC ${COMPILER_CORE_SOURCES})
target_include_directories(qvm_compiler_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
add_dependencies(qvm_compiler_core q3asm_tool q3cpp_tool q3rcc_tool q3lcc_tool lcc_tool)

# ==============================================================================
# QVM Decompiler Core Library
# ==============================================================================
set(DECOMPILER_CORE_SOURCES
    source/common/Error.cpp
    source/common/FileUtils.cpp
    source/format/QvmHeader.cpp
    source/format/QvmInstruction.cpp
    source/format/QvmSections.cpp
    source/decompiler/InstructionDecoder.cpp
    source/decompiler/QvmParser.cpp
    source/decompiler/StringExtractor.cpp
    source/decompiler/FunctionAnalyzer.cpp
    source/decompiler/ControlFlowAnalyzer.cpp
    source/decompiler/PseudocodeGenerator.cpp
    source/decompiler/Exporter.cpp
)

add_library(qvm_decompiler_core STATIC ${DECOMPILER_CORE_SOURCES})
target_include_directories(qvm_decompiler_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ==============================================================================
# Standalone CLI Executable (q3vm - C++20, NO Qt dependency)
# ==============================================================================
add_executable(q3vm source/cli/main.cpp)
target_link_libraries(q3vm PRIVATE qvm_compiler_core qvm_decompiler_core)
target_include_directories(q3vm PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Copy tools to CLI output directory after build
add_custom_command(TARGET q3vm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${QVM_TOOLS_OUTPUT_DIR}"
        "$<TARGET_FILE_DIR:q3vm>/tools"
)

# ==============================================================================
# Qt 6 Desktop GUI Executable (q3vm_gui)
# ==============================================================================
if(QVM_BUILD_GUI)
    if(QVM_HAVE_QT6)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTORCC ON)
        set(CMAKE_AUTOUIC ON)

        set(GUI_SOURCES
            source/gui/main.cpp
            source/gui/MainWindow.hpp
            source/gui/MainWindow.cpp
            source/gui/CompilerPage.hpp
            source/gui/CompilerPage.cpp
            source/gui/DecompilerPage.hpp
            source/gui/DecompilerPage.cpp
            source/gui/models/InstructionTableModel.hpp
            source/gui/models/InstructionTableModel.cpp
            source/gui/models/StringsTableModel.hpp
            source/gui/models/StringsTableModel.cpp
            source/gui/models/FunctionsTreeModel.hpp
            source/gui/models/FunctionsTreeModel.cpp
            source/gui/widgets/CodeEditor.hpp
            source/gui/widgets/CodeEditor.cpp
            source/gui/widgets/CSyntaxHighlighter.hpp
            source/gui/widgets/CSyntaxHighlighter.cpp
            source/gui/widgets/AsmSyntaxHighlighter.hpp
            source/gui/widgets/AsmSyntaxHighlighter.cpp
            source/gui/widgets/HexViewWidget.hpp
            source/gui/widgets/HexViewWidget.cpp
            resources/resources.qrc
        )

        if(WIN32)
            add_executable(q3vm_gui WIN32 ${GUI_SOURCES})
            set_target_properties(q3vm_gui PROPERTIES OUTPUT_NAME "quake3-qvm-editor")
        elseif(APPLE)
            add_executable(q3vm_gui MACOSX_BUNDLE ${GUI_SOURCES})
            set_target_properties(q3vm_gui PROPERTIES
                OUTPUT_NAME "quake3-qvm-editor"
                MACOSX_BUNDLE_BUNDLE_NAME "quake3-qvm-editor"
                MACOSX_BUNDLE_GUI_IDENTIFIER "com.quake3.qvmeditor"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
                MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            )
        else()
            add_executable(q3vm_gui ${GUI_SOURCES})
            set_target_properties(q3vm_gui PROPERTIES OUTPUT_NAME "quake3-qvm-editor")
        endif()

        target_link_libraries(q3vm_gui PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Svg
            qvm_compiler_core
            qvm_decompiler_core
        )

        target_include_directories(q3vm_gui PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
        )
    else()
        # Native Win32 GUI without Qt6 dependency
        add_executable(q3vm_gui WIN32 source/win32_gui/main_win32.cpp)
        set_target_properties(q3vm_gui PROPERTIES OUTPUT_NAME "quake3-qvm-editor")

        target_link_libraries(q3vm_gui PRIVATE
            qvm_decompiler_core
            qvm_compiler_core
            comctl32
            comdlg32
            shell32
        )

        target_include_directories(q3vm_gui PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
        )
    endif()

    # Ensure tools directory is copied adjacent to GUI executable
    add_custom_command(TARGET q3vm_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${QVM_TOOLS_OUTPUT_DIR}"
            "$<TARGET_FILE_DIR:q3vm_gui>/tools"
    )

    # Ensure SDK headers are bundled with GUI executable
    if(APPLE)
        add_custom_command(TARGET q3vm_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/quake3/code"
                "$<TARGET_FILE_DIR:q3vm_gui>/../Resources/code"
        )
    else()
        add_custom_command(TARGET q3vm_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/quake3/code"
                "$<TARGET_FILE_DIR:q3vm_gui>/code"
        )
    endif()
endif()

# ==============================================================================
# Automated Testing Suite (GoogleTest)
# ==============================================================================
if(QVM_BUILD_TESTS)
    set(TEST_SOURCES
        tests/test_qvm_parser.cpp
        tests/test_instruction_decoder.cpp
        tests/test_compiler.cpp
        tests/test_decompiler.cpp
    )

    add_executable(q3vm_tests ${TEST_SOURCES})
    target_link_libraries(q3vm_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        qvm_compiler_core
        qvm_decompiler_core
    )
    target_include_directories(q3vm_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

    include(GoogleTest)
    gtest_discover_tests(q3vm_tests WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# ==============================================================================
# Installation Targets
# ==============================================================================
include(GNUInstallDirs)

install(TARGETS q3vm
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QVM_BUILD_GUI)
    install(TARGETS q3vm_gui
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(TARGETS q3lcc_tool q3asm_tool q3cpp_tool q3rcc_tool lcc_tool
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/tools
)

install(FILES README.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/quake3-qvm-editor
)

# ==============================================================================
# CPack Packaging
# ==============================================================================
include(Packaging)
