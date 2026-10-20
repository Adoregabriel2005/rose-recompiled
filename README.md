# Rose Recompiled

Rose Recompiled is an experimental native Windows recompilation project built with [nesrecomp](https://github.com/mstan/nesrecomp). The repository contains the host integration, Rose configuration menu, build tools, launcher and verification routes. It does not contain a ROM or generated game code.

## Current beta

The current Windows x64 beta provides:

- a portable graphical launcher that validates and remembers a compatible user-supplied ROM;
- the Rose Recompiled menu with sound, video, controls and system pages;
- original 4:3, stretched 16:9 and experimental expanded 16:9 presentation;
- keyboard remapping and persistent configuration;
- ROM-specific native recompilation profiles with interpreter and independent-oracle checks.

Expanded 16:9 reconstructs compatible horizontal rooms at 426x240 and extends ordinary object activation to the wider viewport. Special screens and unsupported room layouts can fall back to a centered presentation.

Only the tested ROM revision is currently supported. Other regions and revisions require an independent profile and validation. The project is incomplete and does not claim full-game coverage.

## Build

Requirements: PowerShell 7, Git, CMake, Python 3.11+, Clang/LLVM-MinGW and `mingw32-make` in `PATH`.

```powershell
./tools/setup.ps1
python tools/import_rom.py 'C:/path/to/compatible-rom.zip'
$rom = (Get-ChildItem private/roms/*/game.nes | Select-Object -First 1).FullName
cmake -S . -B build/game -G 'MinGW Makefiles' `
  -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release "-DROSE_ROM=$rom"
cmake --build build/game --parallel 4
./Start.ps1
```

The imported ROM stays under `private/`, which Git ignores. The executable accepts only the cartridge payload identity used for its build.

For the complete profiling and verification pipeline:

```powershell
./build-local.ps1 -RomPath 'C:/path/to/compatible-rom.nes' -Verify
```

This records a private per-ROM execution profile, rebuilds native blocks, compares every tested frame with the interpreter and, with `-Verify`, checks four CPU/PPU alignments against the independent oracle.

## Tests

```powershell
ctest --test-dir build/game --output-on-failure
python tests/verify_game_state.py --exe build/game/RoseRecompiled.exe --rom $rom
```

The current gameplay route covers 6,000 frames through the opening and first playable area. It verifies that the tested trace matches across backends; it is not proof of complete-game compatibility. Detailed results and current limits are documented in [docs/validation.md](docs/validation.md).

## Controls

Default keyboard bindings:

| Action | Key |
|---|---|
| Move | Arrow keys |
| A | Z |
| B | X |
| Start | Enter |
| Select | Backslash |
| Rose menu | Escape |

Inside the menu, left and right change pages, down enters the options, Enter activates an item and Backspace returns to the page header.

## Distribution

Do not commit or distribute ROMs, saves, captured RAM code or generated game sources. Portable packages contain only the launcher, runtime, SDL2, notices and the hidden optional tips file. Users supply their own compatible ROM.

The recompilation framework is kept outside this repository and pinned by `engine.lock`. Dependency notices are collected when a portable beta is packaged.
