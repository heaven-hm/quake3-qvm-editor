set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER "/opt/homebrew/bin/zig")
set(CMAKE_C_COMPILER_ARG1 "cc -target x86_64-linux-musl")

set(CMAKE_CXX_COMPILER "/opt/homebrew/bin/zig")
set(CMAKE_CXX_COMPILER_ARG1 "c++ -target x86_64-linux-musl")
