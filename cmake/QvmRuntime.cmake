# QvmRuntime.cmake — embed the q3vm interpreter (third_party/q3vm/vm.c) and the
# Q3 syscall-host shim (source/runtime/QvmRuntime.cpp) into one static library that
# the CLI and the GUI link against.

set(QVM_RUNTIME_SOURCES
    third_party/q3vm/vm.c
    source/runtime/QvmRuntime.cpp
)

add_library(qvm_runtime_core STATIC ${QVM_RUNTIME_SOURCES})

target_include_directories(qvm_runtime_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/q3vm"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# vm.h is kept in third_party/q3vm/ and included as "third_party/q3vm/vm.h".
# Make that lookup directory visible to any target that links qvm_runtime_core.
target_include_directories(qvm_runtime_core INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
)
target_include_directories(qvm_runtime_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# vm.c is pure C; keep it compiled as C so the setjmp/longjmp used for trap_Error
# / fatal-VM failure is well-defined and the C text symbols remain C-linkage.
set_target_properties(qvm_runtime_core PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# The host shim uses longjmp to unwind on trap_Error / fatal VM errors.
# longjmp back into the C++ call site is safe on GNU/Clang when the C++ frame
# has unwind tables; ensure exceptions are enabled for C++ so the RunWorker /
# CLI code can catch the unwind path if it ever needs to.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(qvm_runtime_core PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>
    )
endif()

# qvm_runtime_core has no external library dependencies.
target_link_libraries(qvm_runtime_core PUBLIC
    $<$<PLATFORM_ID:Darwin>:m>
    $<$<PLATFORM_ID:Linux>:m>
)
