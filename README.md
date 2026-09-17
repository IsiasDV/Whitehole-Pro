# Whitehole Pro
## The latest and greatest cutting-edge Whitehole and we can do this 'cause Whitehole Neo is open-source hahaha.
### Yes, AI is used in this project. So what? Oh are you complaining that it's not human-written code? Womp. WOMP!
Look at this image, look it makes us look so techy and advanced:
![Editing Flipswitch and Flip-Swap Galaxy](https://github.com/SMGCommunity/Whitehole-Neo/blob/master/ExampleImage.png)

> **Better than Whitehole Neo:** Whitehole Neo is cool and all that, right? Yeah, we're gonna take everything from Whitehole Neo ('cause it's open source) and make it BETTER with Claude. We'll overhaul the user interface, have it optimize the code, and make it incredibly user friendly. Might even include tutorials within the application.

**We're competent.** Unlike many of the BABIES that reside in Luma's Worshop, we are competent individuals. Yes, our language here seems very ironic. We know that!

What we plan to include:
- Something cool

Also don't worry about preinstalling anything weird like Java, old technology anyways, 'cause we're gonna have this install and work out of the box baby!

## Build the native C++ app

The active rewrite lives under `cpp/` and builds with CMake 3.16+ and a C++20 compiler. Java is not required.

```bat
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
build\Release\whitehole-neo.exe
```

On Windows, launching `whitehole-neo` with no arguments opens the editor. You can also open a bundled template map from the command line:

```bat
build\Release\whitehole-neo.exe map objects data\templates\SMG2BigGalaxyMap.arc
```

Full CLI usage and the migration plan are in [docs/CPP_REWRITE.md](docs/CPP_REWRITE.md).

## Controls
- Left Click: Select/Deselect object (hold <kbd>Shift</kbd> or <kbd>Ctrl</kbd> to select multiple)
- Left Click Drag: Pan camera, Move object
- Right Click Drag: Rotate camera
- Scroll Wheel: Move camera forward/backward, Move object forward/backward
- Arrow Keys + <kbd>PageUp</kbd>/<kbd>PageDown</kbd>: (Can switch to <kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> + <kbd>E</kbd>/<kbd>Q</kbd> in the settings)
  - Hold <kbd>G</kbd> to move selected objects (Letter can be changed in the settings)
  - Hold <kbd>R</kbd> to rotate selected objects (Letter can be changed in the settings)
  - Hold <kbd>S</kbd> to scale selected objects (Letter can be changed in the settings)

## Useful Keyboard Shortcuts
- <kbd>Ctrl</kbd>+<kbd>C</kbd>: Copy selected objects
- <kbd>Ctrl</kbd>+<kbd>V</kbd>: Paste copied objects (positioned at the mouse)
- <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>V</kbd>: Paste copied objects (positioned at the position in the copy data)
- <kbd>Ctrl</kbd>+<kbd>Z</kbd>: Undo previous action
- <kbd>Shift</kbd>+<kbd>A</kbd>: Add object quick access menu
- <kbd>Space</kbd>: Jump camera to selected object(s)
- <kbd>Shift</kbd>+<kbd>Space</kbd>: Jump camera to selected zone
- <kbd>H</kbd>: Hide/Unhide selected objects
- <kbd>Alt</kbd>+<kbd>H</kbd>: Unhide All hidden objects
- <kbd>Delete</kbd>: Delete selected objects
- <kbd>Ctrl</kbd>+<kbd>N</kbd>: Truncate an object's positional values to remove the decimal parts
- <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Alt</kbd>+<kbd>C</kbd>: Reset the selected path point control handles
- <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd>: Reverse the selected path points
- <kbd>L</kbd>: Link the selected worldmap points together
- <kbd>P</kbd>: Switch worldmap points and links between their Yellow and Pink variants

## Libraries
- **jogamp**: https://jogamp.org/
- **gluegen**: https://jogamp.org/gluegen/www/
- **org.json**: https://github.com/stleary/JSON-java
- **flatlaf**: https://github.com/JFormDesigner/FlatLaf
- **JWindowsFileDialog**: https://github.com/JacksonBrienen/JWindowsFileDialog
- **DiscordIPC**: https://github.com/LogicismDev/DiscordIPC

