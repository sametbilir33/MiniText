<h1>
  <img src="data/icons/hicolor/scalable/apps/io.github.sametbilir33.MiniText_Title.svg"
       width="520" height="256"
       alt="MiniText Title" />
</h1>

# MiniText

MiniText is a lightweight GTK 4 and libadwaita text editor for Linux.

## Requirements

- CMake 3.16 or newer
- GCC or another C11 compiler
- GTK 4.10 or newer
- libadwaita 1.5 or newer
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
`cmake --build build --target test-ui` runs the GTK window, tab, and drag/drop
interaction tests and requires an active graphical session.

## Features

- UTF-8 text editing with file open, save, and save-as support
- Multiple documents in independent tabs, with duplicate-file detection
- Multiple-file drag and drop on the window, tab bar, or editor
- Per-tab unsaved-change indicators and save/discard/cancel close prompts
- Find and replace, including case-sensitive search and replace all
- Undo and redo, with bulk replacements grouped as one action
- Line navigation, synchronized line numbers, and wrapped monospace text
- Checks every modified tab before closing the window

## Keyboard shortcuts

| Action | Shortcut |
| --- | --- |
| New tab | Ctrl+N |
| Open | Ctrl+O |
| Save | Ctrl+S |
| Save as | Ctrl+Shift+S |
| Find | Ctrl+F |
| Find and replace | Ctrl+H |
| Undo | Ctrl+Z |
| Redo | Ctrl+Shift+Z or Ctrl+Y |
| Go to line | Ctrl+L |
| Close active tab | Ctrl+W |
| Next tab | Ctrl+Tab |
| Previous tab | Ctrl+Shift+Tab |
| Select tab 1-9 | Ctrl+1 through Ctrl+9 |
| Keyboard shortcuts | Ctrl+? |
| About | Ctrl+, |

When the search field is active, Enter moves to the next match, Shift+Enter
moves to the previous match, and Escape closes the search bar.