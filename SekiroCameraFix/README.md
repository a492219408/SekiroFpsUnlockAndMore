# SekiroCameraFix DLL

This is a standalone DLL that disables camera auto-rotate on movement in Sekiro: Shadows Die Twice.

## Features

- **Disable camera auto rotate on movement**: Prevents the camera from automatically adjusting when the player moves.

## Usage with ModEngine

1. Build the DLL (see Build Instructions below)
2. Copy `SekiroCameraFix.dll` to your ModEngine mods folder
3. Configure ModEngine to load the DLL
4. Start the game through ModEngine

### ModEngine Configuration

Add the following to your `config_sekiro.toml` or equivalent ModEngine configuration:

```toml
[modengine]
external_dlls = ["SekiroCameraFix.dll"]
```

Or if using ModEngine2:

```toml
[extension]
dll_path = "SekiroCameraFix.dll"
```

## Build Instructions

### Using Visual Studio

1. Open `SekiroFpsUnlockAndMore.sln` in Visual Studio
2. Select `Release|x64` configuration
3. Build the `SekiroCameraFix` project
4. The DLL will be in `bin\x64\Release\SekiroCameraFix.dll`

### Using CMake

```bash
cd SekiroCameraFix
mkdir build
cd build
cmake -A x64 ..
cmake --build . --config Release
```

## Notes

- The DLL is designed for **x64** only (Sekiro is a 64-bit game)
- The camera pitch XY adjustment is disabled by default as it can cause issues with controller input
- If you use a mouse, you can uncomment the pitch XY code in `dllmain.cpp` and rebuild

## Technical Details

The DLL works by:
1. Creating a new thread when loaded
2. Waiting for the game to initialize
3. Scanning game memory for specific patterns
4. Creating code caves that redirect camera calculation functions to preserve the current camera position

## Credits

Based on the camera adjustment code from [SekiroFpsUnlockAndMore](https://github.com/uberhalit/SekiroFpsUnlockAndMore) by uberhalit.
