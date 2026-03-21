# Xbox Dashboard Recompiled

**The first-ever native PC port of the original Xbox Dashboard.**

Remember that green orb? The ambient hum of a spaceship control room? The satisfying *bwoom* when you scrolled through menus? We're bringing it back -- not through emulation, but by statically recompiling the original `xboxdash.xbe` into a native Windows executable.

This project uses [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) to translate the Xbox Dashboard's x86 machine code into C, then compiles it with MSVC against replacement runtime libraries (D3D8->D3D11, Xbox kernel->Win32, etc.). The result is a standalone `.exe` that runs the original dashboard code natively.

## Status

**Target:** Dashboard build 3944 (v1.0 - the one that shipped with launch consoles)

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Project setup & XBE extraction | DONE |
| 1 | XBE analysis (parse, disasm, func_id) | Up next |
| 2 | Lift to C & first build | Pending |
| 3 | Dashboard runtime (paths, EEPROM, stubs) | Pending |
| 4 | UI rendering (D3D8 init, 3D orb, fonts) | Pending |
| 5 | Polish (input, audio, settings) | Pending |

## What's Inside the Dashboard

The Xbox Dashboard isn't just any XBE. It's Microsoft's bespoke 3D system shell:

- **20 XIP archives** containing UI assets, 3D meshes, and XAP scripts
- **2 Xbox bitmap fonts** (Xbox.xtf, Xbox Book.xtf) totaling 31MB
- **71 WAV audio files** across 5 categories: ambient engine room sounds, menu clicks, transition swooshes, and comm chatter
- **Custom 3D UI** with the iconic green orb, flying panels, and particle effects
- **EEPROM settings** management (language, timezone, parental controls, video output)
- **No game engine** - this is hand-rolled Microsoft code, not RenderWare or Unreal

### Dashboard File Inventory

```
game/
  xboxdash.xbe              (1.39 MB - the main executable)
  Audio/
    AmbientAudio/            (39 files - engine room hums, comm chatter, steam hisses)
    MainAudio/               (11 files - button clicks, scroll beeps, error tones)
    MemoryAudio/             (3 files - controller/slot/game select sounds)
    MusicAudio/              (5 files - info screen transitions, CD select)
    SettingsAudio/           (2 files - language and parental submenu sounds)
    TransitionAudio/         (12 files - menu in/out swooshes per section)
  *.xip                      (20 archives - UI scenes, meshes, textures, scripts)
  Xbox.xtf, Xbox Book.xtf   (bitmap fonts)
```

## XBE Analysis

*Coming in Phase 1 - entry point, section map, kernel imports, function count*

## Building

### Prerequisites

- Windows 10/11 with Visual Studio 2022 (MSVC)
- CMake 3.20+
- Python 3.10+ (for the recomp pipeline)
- [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) cloned as a sibling directory

### Setup

```bash
# Clone repos side by side
git clone https://github.com/sp00nznet/xboxrecomp.git
git clone https://github.com/sp00nznet/xboxdashboard.git

# Extract dashboard files from an Xbox HDD image into dashboard/game/
# (You need: xboxdash.xbe, *.xip, *.xtf, Audio/)

# Build
cd xboxdashboard
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### Running

```bash
bin/Release/xboxdash.exe
```

The executable expects the original dashboard files in the `game/` subdirectory.

## How It Works

```
xboxdash.xbe (Xbox x86)
       |
   [xbe_parser] -- extract sections, kernel imports, entry point
       |
   [disassembler] -- discover functions, build call graph
       |
   [func_id] -- classify CRT, D3D8, XDK library functions
       |
   [lifter] -- translate each x86 instruction to C
       |
   recomp_*.c (millions of lines of mechanical C)
       |
   [MSVC] + xboxrecomp runtime libraries
       |
   xboxdash.exe (native Windows x86-64)
```

The runtime libraries replace Xbox hardware and OS:
- **xbox_kernel** - Xbox kernel calls -> Win32 API
- **xbox_d3d8** - Direct3D 8 -> Direct3D 11
- **xbox_dsound** - DirectSound -> Windows audio
- **xbox_apu** - MCPX APU emulation
- **xbox_nv2a** - NV2A GPU register handling
- **xbox_input** - Xbox gamepad -> XInput

## Why This Is Different

Unlike game recompilations, the dashboard is a **system application**:

1. It manages hardware settings (EEPROM, display modes, timezone)
2. It uses XIP archives with an embedded scripting engine (XAP)
3. The 3D UI is entirely custom - no game engine to lean on
4. It's the shell that launches everything else on the Xbox

This means we need dashboard-specific runtime support beyond what games require: EEPROM simulation, XIP-aware file I/O, and likely some creative solutions for the XAP scripting layer.

## Legal

This project does not include any copyrighted Xbox software. You must provide your own `xboxdash.xbe` and supporting files extracted from an original Xbox console.

## See Also

- [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) - The static recompilation toolkit powering this project
- [xboxdevwiki](https://xboxdevwiki.net/) - Community documentation for Xbox internals
