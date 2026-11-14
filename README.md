# MiniText

MiniText is a lightweight GTK 4 and libadwaita text editor for Linux.

## Requirements

- CMake 3.16 or newer
- GCC or another C11 compiler
- GTK 4
- libadwaita
- `pkg-config`

## Build and run

```sh
cmake -S . -B build
cmake --build build
./build/minitext
```

All generated files and executables are placed in `build/`. Use
`cmake --build build --target clean` to remove build outputs.

Run `ctest --test-dir build --output-on-failure` for file I/O tests.
`cmake --build build --target test-ui` runs a GTK window creation/teardown
smoke test and requires an active graphical session.

## Features

- UTF-8 text editing with file open, save, and save-as support
- Find and replace, including case-sensitive search and replace all
- Undo and redo, with bulk replacements grouped as one action
- Line navigation, synchronized line numbers, and wrapped monospace text
- Prompts before discarding unsaved changes

## Keyboard shortcuts

| Action | Shortcut |
| --- | --- |
| New document | Ctrl+N |
| Open | Ctrl+O |
| Save | Ctrl+S |
| Save as | Ctrl+Shift+S |
| Find | Ctrl+F |
| Find and replace | Ctrl+H |
| Undo | Ctrl+Z |
| Redo | Ctrl+Shift+Z or Ctrl+Y |
| Go to line | Ctrl+L |

When the search field is active, Enter moves to the next match, Shift+Enter
moves to the previous match, and Escape closes the search bar.

The application is split into `application`, `window`, `editor`,
`line_numbers`, and `file_manager` modules under `src/`.
