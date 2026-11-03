# Quake 3 QVM Studio

A modern, cross-platform C++20 and Qt 6 desktop application and standalone command-line interface (CLI) for compiling, editing, inspecting, disassembling, and decompiling Quake III Arena virtual machine (`.qvm`) bytecode.

---

## Overview

Quake 3 QVM Studio brings Quake III Arena QVM development and reverse engineering into a single unified workflow supporting both directions:

- **C Source Code &rarr; Compile &rarr; QVM Bytecode**
- **QVM Bytecode &rarr; Disassemble / Decompile &rarr; Reconstructed C-Like Pseudocode**

The application integrates the official id Software Quake III Arena toolchain (**LCC** compiler and **Q3ASM** assembler) alongside a native C++20 bytecode parser, control-flow reconstructor, and stack-based expression decompilation engine.

---

## Technology Stack

| Component | Technology | Description |
|---|---|---|
| **Language** | C++20 | Native, clean, modern standard C++ |
| **GUI Framework** | Qt 6 Widgets | Fast, responsive native desktop user interface |
| **CLI Tool** | Standard C++20 | Standalone command-line tool with zero Qt dependency |
| **Build System** | CMake 3.16+ | Modern CMake with multi-platform presets |
| **Compiler Toolchain** | Official Quake III LCC | Retargeted Fraser & Hanson C compiler |
| **Assembler** | Official Quake III Q3ASM | Bytecode packaging and symbol relocation |
| **Decompiler Engine** | Native C++20 | Bytecode analysis, control-flow reconstruction, and expression stack engine |
| **Testing** | GoogleTest 1.14 | Automated unit testing suite |
| **Supported Platforms** | Windows 10/11, macOS, Linux | Cross-platform native binaries |

---

## Project Architecture

```text
quake-3-qvm-studio/
├── CMakeLists.txt              # Root CMake build configuration
├── CMakePresets.json           # Presets for Release, Debug, and CLI-only builds
├── README.md                   # Project documentation
├── LICENSE                     # GNU General Public License v2
│
├── cmake/
│   ├── Dependencies.cmake      # Qt 6 and GoogleTest resolution
│   ├── CompilerOptions.cmake   # Warning levels and MSVC/Clang flags
│   └── Packaging.cmake         # CPack distribution setup
│
├── third_party/
│   └── quake3/                 # Official id Software Quake III Arena source
│       ├── lcc/                # LCC C compiler (cpp, src, etc)
│       ├── q3asm/              # Q3ASM bytecode assembler
│       ├── code/               # Game modules (game, cgame, q3_ui, ui)
│       └── bin/win32/          # Precompiled Win32 toolchain binaries
│
├── include/qvm/
│   ├── common/                 # Common utilities (Error, FileUtils, ProcessRunner, Version)
│   ├── format/                 # Binary format structures (QvmHeader, QvmInstruction, QvmSections)
│   ├── compiler/               # Compilation pipeline (CompilerConfig, ModuleConfig, ToolchainRunner, BuildPipeline)
│   └── decompiler/             # Decompiler engine (QvmParser, InstructionDecoder, FunctionAnalyzer, ControlFlowAnalyzer, StringExtractor, PseudocodeGenerator, Exporter)
│
├── source/
│   ├── common/                 # Common implementations
│   ├── format/                 # QVM structures implementations
│   ├── compiler/               # Compiler implementations
│   ├── decompiler/             # Decompiler implementations
│   ├── cli/main.cpp            # Standalone CLI entry point (NO Qt)
│   └── gui/                    # Qt 6 desktop GUI (MainWindow, CompilerPage, DecompilerPage, models, widgets)
│
├── resources/                  # Styles and Qt resources
├── tests/                      # GoogleTest automated test suite
└── packaging/                  # Platform deployment notes
```

---

## Building from Source

### Prerequisites

- **CMake 3.16** or newer
- **C++20 compliant compiler** (MSVC 2019+, GCC 11+, or Clang 13+)
- **Qt 6.2+** with Widgets (optional if building CLI only)

### Standard Build (GUI + CLI + Tests)

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build all targets in parallel
cmake --build build --parallel

# Run tests
ctest --test-dir build --output-on-failure
```

### Building CLI Only (Zero Qt Dependency)

```bash
cmake -S . -B build-cli -DQVM_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-cli --parallel
```

---

## Command-Line Interface (`q3vm`)

The standalone command-line tool `q3vm` provides complete access to compiler and decompiler functionality without requiring a graphical environment:

```bash
# 1. Compile Quake III C source code into a QVM binary
q3vm compile --module qagame --source ./code/game --output ./build
q3vm compile --module cgame --source ./code/cgame --output ./build
q3vm compile --module ui --source ./code/q3_ui --output ./build

# 2. Inspect QVM binary header and section layout
q3vm info ./build/qagame.qvm

# 3. Disassemble bytecode into readable assembly
q3vm disassemble ./build/qagame.qvm --output ./build/qagame.asm

# 4. Reconstruct C-like pseudocode from bytecode
q3vm pseudocode ./build/qagame.qvm --output ./build/qagame.c

# 5. Export both assembly and pseudocode simultaneously
q3vm export ./build/qagame.qvm --assembly ./build/qagame.asm --pseudocode ./build/qagame.c

# 6. Full decompile pipeline (prints info and exports .asm and .c files)
q3vm decompile ./build/qagame.qvm

# 7. Help and version
q3vm --help
q3vm --version
```

---

## Graphical User Interface (`q3vm_gui`)

The Qt 6 desktop interface is focused on two main workflows:

### 1. Compiler Page
- **Module Selection:** Choose from standard Quake III modules (`qagame`, `cgame`, `ui`) or `custom`.
- **Source & Output Pickers:** Select project folders with native directory dialogs.
- **Compiler Options:** Toggle debug symbols (`-Wf-g`), verbose assembler output (`-v`), and intermediate cleanup.
- **Non-Blocking Background Builds:** Compilations execute in a worker `QThread`, keeping the interface smooth and responsive.
- **Categorized Log Tabs:** Inspect combined logs, compiler stage output, assembler stage output, or isolated warnings/errors.
- **Direct Workflow Integration:** Upon successful compilation, a single click immediately opens the generated `.qvm` in the decompiler.

### 2. Decompiler Page
- **Drag-and-Drop:** Drag any `.qvm` file directly into the application window.
- **Header Information Card:** Instant visibility into magic numbers, instruction counts, code/data/literal/BSS segment offsets and sizes.
- **Decoded Instruction Table:** High-performance table view displaying instruction index, byte offset, mnemonic, decimal/hex operand, and resolved targets/symbols.
- **Function Navigation:** Automatically partitions bytecode into functions, displays stack frame sizes, computes bytecode hashes, and identifies stock Quake III functions (e.g. `vmMain`, `G_Printf`, `G_InitGame`). Double-clicking a function instantly jumps to its bytecode.
- **Reconstructed C Pseudocode:** Built-in editor displays formatted C-like pseudocode reconstructed using expression stack lowering.
- **Extracted Strings & Constants:** Dedicated table displaying all string literals found in the LIT and DATA sections.
- **Hex View:** Integrated hex inspection view for raw binary analysis.
- **Export Actions:** One-click export to standard `.asm` and `.c` files.

---

## Decompilation Notice & Disclaimer

Bytecode decompilation reconstructs program logic through control-flow and expression-stack analysis. Due to compiler optimizations, register reuse, and compiler type erasure, original variable names, macros, struct types, and source comments cannot be completely recovered.

Reconstructed pseudocode is intended for inspection, reverse engineering, and mod analysis. It is clearly labeled and should not be confused with original C source code.

---

## Licensing

- **Quake 3 QVM Studio** codebase is licensed under the [GNU General Public License Version 2 (GPLv2)](LICENSE).
- **Quake III Arena** original source code, LCC, and Q3ASM are Copyright &copy; 1999-2005 Id Software, Inc. and licensed under GPLv2.
- **Qt 6** is licensed under LGPLv3 / GPLv3 / Commercial licenses.
