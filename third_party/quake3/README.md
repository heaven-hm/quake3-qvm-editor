# Quake III Arena Source Code & Toolchain

This directory contains source code and toolchain utilities from the official Quake III Arena repository by id Software:
https://github.com/id-Software/Quake-III-Arena

## Components

- **`lcc/`**: Fraser and Hanson's Little C Compiler retargeted for Quake III QVM bytecode output.
  - `cpp/`: The Quake 3 C preprocessor (`q3cpp`).
  - `src/`: The Quake 3 compiler intermediate code generator (`q3rcc`).
  - `etc/`: The driver (`q3lcc` / `lcc`) that orchestrates preprocessing and code generation.
- **`q3asm/`**: The Quake III assembler (`q3asm`), which translates assembly files into the `.qvm` binary format.
- **`code/`**: Official game modules and build scripts:
  - `game/`: Server-side game logic (`qagame`), including `g_main.c`, `bg_lib.c`, `g_syscalls.asm`, and `Conscript`.
  - `cgame/`: Client-side game logic (`cgame`), including `cg_main.c`, `cg_syscalls.asm`, and `Conscript`.
  - `q3_ui/`: Quake III user interface module (`ui`), including `ui_main.c`, `ui_syscalls.asm`, and `Conscript`.
  - `ui/`: Team Arena expanded UI module.
- **`bin/win32/`**: Original Win32 toolchain binaries (`lcc.exe`, `q3asm.exe`, `q3cpp.exe`, `q3rcc.exe`) bundled for Windows execution.

## Licensing

Quake III Arena source code is licensed under the GNU General Public License Version 2 (GPLv2).
See `COPYING.txt` for the full license text.
Copyright (C) 1999-2005 Id Software, Inc.
