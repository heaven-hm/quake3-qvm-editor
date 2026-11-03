# Windows Packaging & Deployment

## Target Systems

- **Windows 10** (x64)
- **Windows 11** (x64)

## Included Artifacts

- `q3vm.exe` (CLI)
- `q3vm_gui.exe` (Qt 6 GUI)
- `tools/`
  - `q3lcc.exe`
  - `q3asm.exe`
  - `q3cpp.exe`
  - `q3rcc.exe`

## Deployment

Deploy Qt 6 runtime dependencies with:

```cmd
windeployqt.exe q3vm_gui.exe
```
