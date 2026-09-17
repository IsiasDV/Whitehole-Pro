# Whitehole C++ rewrite

Whitehole is moving from its Java 11/Swing implementation to a native C++20 application. The rewrite is incremental so each migrated format can be verified against real Super Mario Galaxy data before the remaining editor surfaces depend on it.

## Current native milestone

The repository builds `whitehole-neo` from `whitehole_core`. The native app currently owns:

- bounds-checked endian-aware binary I/O;
- path-safe project filesystem access;
- Yaz0 compression and decompression;
- big- and little-endian RARC reading, mutation, extraction, repacking, and Yaz0 recompression;
- big- and little-endian BCSV/JMap reading and writing;
- SMG1/SMG2 game, galaxy, and stage archives;
- placement object loading (name, layer, type, position, rotation, scale) with round-trip save;
- galaxy/zone display names from `data/galaxies.json` and `data/zones.json`;
- a Windows desktop editor that can open a game folder or a map archive, list objects, edit transforms, and save;
- a command-line interface on every platform.

The bundled `.arc` galaxy templates are part of the native test suite.

## Build

You need CMake 3.16+ and a C++20 compiler (Visual Studio 2019 16.11+, Visual Studio 2022, or a recent GCC/Clang). No Java install is required for the native app.

### Windows (Visual Studio)

```bat
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The editor is `build\Release\whitehole-neo.exe` (or `build\Debug\whitehole-neo.exe` for a Debug build). Launching it with no arguments opens the desktop editor.

### Other generators

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

```sh
# Windows desktop editor
build\Release\whitehole-neo.exe
build\Release\whitehole-neo.exe gui

# List objects inside a bundled template map
whitehole-neo map objects data/templates/SMG2BigGalaxyMap.arc

# Open a real SMG1/SMG2 workspace (the folder that contains StageData)
whitehole-neo game list path\to\extracted\files
whitehole-neo galaxy inspect path\to\extracted\files HoneyBeeKingdomGalaxy
whitehole-neo zone objects path\to\extracted\files HoneyBeeKingdomGalaxy

# Archive / BCSV / Yaz0 tools
whitehole-neo archive list data/templates/SMG2BigGalaxyMap.arc
whitehole-neo archive extract data/templates/SMG2BigGalaxyMap.arc extracted
whitehole-neo bcsv inspect extracted/Stage/jmp/Placement/Common/ObjInfo
whitehole-neo yaz0 decompress input.szs output.arc
whitehole-neo hash Obj_arg0
```

In the Windows editor: **File > Open Map Archive...** and choose `data/templates/SMG2BigGalaxyMap.arc` to load objects without a full game dump. Use **File > Open Game Directory...** for an extracted SMG workspace. Edit name/position/rotation/scale, click **Apply**, then **File > Save Zone**.

## Migration boundary

The existing `src/` Java tree remains the behavioral reference until its equivalent is present and covered by native tests. New functionality belongs under `cpp/`; Java code should not gain new features.

Still to migrate:

1. BMD/BTI/KCL parsing and animation data.
2. Full per-class object models, paths, and undo.
3. A native GPU renderer and asset caches.
4. The remaining desktop editors (BCSV spreadsheet, world map, galaxy creation).
5. Cross-platform GUI packaging, then removal of the Java/Ant build.

Each stage should replace a complete vertical slice. Java is removed only when the native replacement can open, edit, save, and reopen representative SMG1 and SMG2 data without loss.
