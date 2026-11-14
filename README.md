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

- Create, open, save, and save as UTF-8 text files
- Prompts before discarding unsaved changes, including when closing the window
- Searches the current document and jumps directly to a line
- Offers to save, discard, or cancel before replacing/closing modified text
- Synchronized line numbers alongside the editor
- Wrapped text and a monospace editing surface

## Keyboard shortcuts

| Action | Shortcut |
| --- | --- |
| New document | Ctrl+N |
| Open | Ctrl+O |
| Save | Ctrl+S |
| Save as | Ctrl+Shift+S |
| Find | Ctrl+F |
| Go to line | Ctrl+L |

The application is split into `application`, `window`, `editor`,
`line_numbers`, and `file_manager` modules under `src/`.
