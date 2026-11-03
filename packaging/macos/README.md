# macOS Packaging & Deployment

## Target Systems

- macOS 12 Monterey and newer (Apple Silicon & Intel)

## Deployment

Package app bundle using `macdeployqt`:

```bash
macdeployqt bin/q3vm_gui.app -dmg
```
