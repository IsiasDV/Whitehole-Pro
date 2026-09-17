# Whitehole Neo C++ rewrite

Whitehole Neo is moving from its Java 11/Swing implementation to a native C++20 application. The rewrite is incremental so each migrated format can be verified against real Super Mario Galaxy data before editor code depends on it.

## Current native milestone

The repository now builds `whitehole-neo`, a dependency-free command-line application backed by `whitehole_core`. The native core currently owns:

- bounds-checked endian-aware binary I/O;
- path-safe project filesystem access with complete create, read, rename, and delete operations;
- Yaz0 compression and decompression;
- big- and little-endian RARC reading, mutation, safe extraction, repacking, and Yaz0 recompression;
- big- and little-endian BCSV/JMap reading and writing, including packed fields and string tables;
- the SMG JMap and SuperFastHash algorithms;
- the vector and matrix operations used by the renderer.

The bundled `.arc` galaxy templates are part of the native test suite, so archive compatibility is checked against project data rather than only synthetic fixtures. The suite currently performs semantic write/read round trips over 179 real BCSV tables from the SMG1 and SMG2 templates.

## Build and use

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure

./build/whitehole-neo archive list data/templates/SMG2BigGalaxyMap.arc
./build/whitehole-neo archive extract data/templates/SMG2BigGalaxyMap.arc extracted
./build/whitehole-neo archive replace input.arc Stage/jmp/Placement/Common/ObjInfo edited.bcsv output.arc
./build/whitehole-neo bcsv inspect extracted/Stage/jmp/Placement/Common/ObjInfo
./build/whitehole-neo bcsv roundtrip input.bcsv output.bcsv
./build/whitehole-neo yaz0 decompress input.szs output.arc
./build/whitehole-neo hash Obj_arg0
```

Any CMake generator can be used if Ninja is unavailable.

## Migration boundary

The existing `src/` tree remains the behavioral reference until its equivalent is present and covered by native tests. New functionality belongs under `cpp/`; Java code should not gain new features.

The remaining migration is ordered by dependency:

1. BMD/BTI/KCL parsing and animation data.
2. Game, galaxy, stage, object, path, and undo models.
3. A native GPU renderer and asset caches.
4. Desktop project browser, property editor, galaxy editor, and world-map editor.
5. Cross-platform packaging, followed by removal of the Java/Ant build.

Each stage should replace a complete vertical slice; the Java implementation is removed only when its native replacement can open, edit, save, and reopen representative SMG1 and SMG2 data without loss.
