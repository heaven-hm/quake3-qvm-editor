# Changelog

All notable changes to **Quake 3 QVM Editor** are documented in this file.

## [0.2.0] - 2026-11-03

### Features
- **Module-Aware Single-File Compilation**: Compiling a single module source (for example `cgame.c`) now auto-detects the target module and links the whole module (`cg_main.c`, shared `bg_*`/`q_*` sources, the matching syscall table, and `bg_lib.c`). The supplied file becomes the custom `vmMain` entry point; a generated forward-entry stub guarantees a valid QVM entry at code offset 0, and the stock entry's `vmMain` is renamed to avoid duplicate symbols. `q3vm compile <file>.c` therefore produces a complete, loadable module QVM.
- **Automatic Module Detection From Source File Name**: `cgame.c`/`cg_*.c` map to `cgame`, `game.c`/`g_*.c` to `qagame`, and `ui.c`/`ui_*.c` to `ui`, selecting the correct syscall table and default output name.
- **Enforced Per-Module Mapping in the GUI Editor**: Added a Target Module selector to the Interactive C Editor that fixes the `source`/`syscall`/`output` mapping, auto-selects the mapped output name, and emits `--module` in the live CLI command preview.
- **Relocatable Particle System Resolution**: Module builds automatically include `cg_particles.c` when the resolved tree does not already define the particle symbols in `cg_marks.c`, supporting both stock Q3 and ioq3/Team-Arena-derived trees.
- **`qcommon` Source Resolution**: Source resolution now searches `qcommon`/`code/qcommon`, so `q_shared.c` and `q_math.c` link from source trees that keep them there.

### Major Bug Fixes
- **Per-Module Syscall Enforcement**: Single module files are compiled against their mapped syscall table (`cg_syscalls.asm`/`g_syscalls.asm`/`ui_syscalls.asm`) instead of always `g_syscalls.asm`, and produce mapped output names (`cgame.qvm`/`qagame.qvm`/`ui.qvm`).
- **Token-Boundary Module Deduction**: Output-name deduction no longer misclassifies names such as `build.qvm`, `guide.qvm`, or `circuit.qvm` as the `ui` module.
- **Standalone Module C Runtime Linking**: Full module builds now link `bg_lib.c`, resolving `vsprintf`, `strcmp`, `atoi`, `rand`, and related C runtime references.
- **CLI Explicit Module Respect**: `q3vm compile <file.c> --module <mod>` no longer force-overrides the module to `custom`.
- **Extraneous Namespace Brace Fix**: Removed a duplicated `} // namespace qvm` in `Compiler.cpp` that prevented the compiler core from building.

## [0.1.1] - 2026-11-03

### Features
- **Selective Source File Compilation**: The compiler and UI no longer compile every `.c` file found in a source directory. Added support for `config.selectedFiles` to compile strictly designated source files and their configured dependencies.
- **Dedicated GUI Source File Selector**: Added a "Source Files" selector and file browser dialog to the Project Directory Build tab in the GUI to allow picking specific `.c` and `.asm` files.
- **CLI Multi-File Option (`--files`)**: Extended `q3vm compile` with `--files <file1> <file2> ...` to compile and link explicit file sets.
- **Detailed Q3ASM Command Logging**: The exact Q3ASM invocation command is now captured in `BuildReport::q3asmCommand`, logged to the compilation output console, and printed in error diagnostics.

### Major Bug Fixes
- **Removal of Automatic `bg_lib.c` Linking**: Eliminated unconditional inclusion and copying of `bg_lib.c` across `QaGame`, `CGame`, `Ui`, and custom scratch builds, preventing duplicate symbol collisions (`swapfunc`, `med3`, `qsort`, `strlen`, `strcpy`, `strcmp`, `vmMain`, etc.) when user code defines custom routines or standalone entry points.
- **Prevention of Duplicate Assembly Passes**: Pruned and isolated intermediate scratch directories before each build and deduplicated `.asm` files before assembling with Q3ASM, resolving duplicate symbol definitions caused by re-traversing stale intermediate files.
- **Accurate Engine Trap Linking**: Preserved automatic companion syscall stubs (`g_syscalls.asm`, `cg_syscalls.asm`, `ui_syscalls.asm`) for engine traps without dragging in extraneous C library code.

## [0.1.0] - 2026-11-03

### Features
- **Interactive C Editor**: Integrated C code editor with syntax highlighting for C keywords, types, preprocessor directives, and `trap_*` engine syscalls.
- **One-Click & Shortcut Compilation**: Compile in-editor C code directly to `.qvm` bytecode on the fly via button or <kbd>Ctrl+Enter</kbd> (<kbd>Cmd+Enter</kbd>).
- **Automated SDK Include Resolution**: Dedicated "Select Quake 3 SDK..." button and automatic detection/resolution of Quake III Arena game headers (`qcommon` for `q_shared.h`, `cgame` for `cg_public.h`, `game` for `g_local.h`/`bg_lib.h`, `ui`, `q3_ui`).
- **Official Toolchain Integration**: Integrated authentic Fraser & Hanson LCC bytecode compiler and Q3ASM assembler supporting `qagame`, `cgame`, `ui`, and `custom` mod targets with `--sdk` option.
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
- **Scratch Directory Isolation**: Relocated editor scratch compilation from user output directories to system temporary directory (`temp_directory_path() / "quake3_qvm_editor_scratch"`), completely eliminating directory pollution in output paths.
- **Auto-Resolution of Sibling Quake 3 SDK Directories**: Added automatic discovery and expansion for sibling module headers (`qcommon`, `cgame`, `game`, `ui`, `q3_ui`) when any project SDK include directory is specified, seamlessly resolving `#include "q_shared.h"` and `#include "cg_public.h"`.
- **Embedded Quake 3 Headers in Release Bundles**: Bundled complete Quake 3 Arena headers in `Contents/Resources/code` on macOS and `code/` on Windows release distributions.
- **macOS DMG Standalone Deployment**: Self-contained all Qt 6 frameworks, platform plugins (`libqcocoa`), and native tools inside `quake3-qvm-editor.app` with code signing and direct `/Applications` drag-and-drop link, resolving dynamic linker crashes on macOS disk images.
- **Windows Desktop Application Support**: Provided native Windows executable `quake3-qvm-editor.exe` with GUI workspace and CLI dual-mode execution, bundling toolchain binaries with zero external DLL dependencies.
- **Standardized Executable Naming & Branding**: Unified executable and application names to `quake3-qvm-editor.app` (macOS), `quake3-qvm-editor.exe` (Windows), and `quake3-qvm-editor` (Linux), updating all UI branding titles to "QVM Editor".
- **Dedicated SDK Selection in GUI**: Added "Select Quake 3 SDK..." button to both the Interactive C Editor and Project Directory tabs in both macOS and Windows applications, automatically configuring include flags and resolving cross-module headers.
- **Single-File Compilation Intermediate Cleanup**: Intermediate assembly files are now strictly isolated to temporary directories and cleanly pruned when building single files, ensuring only the target `.qvm` is placed in the destination folder.
