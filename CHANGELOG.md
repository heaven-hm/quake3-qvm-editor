# Changelog

All notable changes to **Quake 3 QVM Editor** are documented in this file.

## [0.1.0] - 2026-11-03

### Features
- **Interactive C Editor**: Integrated C code editor with syntax highlighting for C keywords, types, preprocessor directives, and `trap_*` engine syscalls.
- **One-Click & Shortcut Compilation**: Compile in-editor C code directly to `.qvm` bytecode on the fly via button or <kbd>Ctrl+Enter</kbd> (<kbd>Cmd+Enter</kbd>).
- **Automated SDK Include Resolution**: Automatic detection and resolution of bundled official Quake III Arena game headers (`g_local.h`, `bg_lib.h`, `q_shared.h`, `bg_public.h`, `g_public.h`).
- **Official Toolchain Integration**: Integrated authentic Fraser & Hanson LCC bytecode compiler and Q3ASM assembler supporting `qagame`, `cgame`, `ui`, and `custom` mod targets.
- **Bytecode Decompiler & Disassembler**: Full 32-bit QVM instruction decoding, function discovery, stack frame size calculation, entry/exit point detection, and C-like pseudocode reconstruction.
- **Single-Function Focus Mode**: Isolate and inspect assembly instructions, stack frame details, and pseudocode for any individual selected function.
- **Extracted Strings & Hex View**: Dedicated table displaying all string literals from LIT and DATA segments alongside raw hexadecimal bytecode inspector.
- **Multiple UI Themes**: Added Tactical Dark (`#b3e878` accent), Midnight Navy (`#7ecdf3` accent), and Clean Light themes.
- **Standalone C++20 CLI (`q3vm`)**: Zero-dependency standard C++20 command-line interface for compiling, disassembling, decompiling, and exporting QVM files.

### Major Bug Fixes
- **LCC Preprocessor Include Path Prepending**: Fixed a critical bug in `q3cpp` (`include.c`) that prepended the source file's directory to absolute and cwd-relative include paths, causing `#include "g_local.h"` failures.
- **Header Guard Idempotency in `bg_lib.h`**: Added missing `#ifndef BG_LIB_H` guards to prevent redeclaration errors (`size_t`, `va_list`, `cmp_t`) when both `g_local.h` and `bg_lib.h` are included.
- **Source Trailing Newline Sanitation**: Fixed compiler error `No newline at end of file` by automatically ensuring trailing newlines in in-editor source buffers.
- **Syscall Linking Resolution**: Added bidirectional aliases for `trap_Printf` and `trap_Print` to syscall `-1` in `g_syscalls.asm` to resolve linker errors across game and client modules.
