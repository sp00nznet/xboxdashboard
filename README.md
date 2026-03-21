# Xbox Dashboard Recompiled

**The first-ever native PC port of the original Xbox Dashboard.**

Remember that green orb? The ambient hum of a spaceship control room? The satisfying *bwoom* when you scrolled through menus? We're bringing it back -- not through emulation, but by statically recompiling the original `xboxdash.xbe` into a native Windows executable.

This project uses [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) to translate the Xbox Dashboard's x86 machine code into C, then compiles it with MSVC against replacement runtime libraries (D3D8->D3D11, Xbox kernel->Win32, etc.). The result is a standalone `.exe` that runs the original dashboard code natively.

## Status

**Target:** Dashboard build 3944 (v1.0 - the one that shipped with launch consoles)

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Project setup & XBE extraction | DONE |
| 1 | XBE analysis (parse, disasm, func_id) | DONE - 3,209 functions, 134 kernel imports |
| 2 | Lift to C & first build | DONE - 248K lines of C, 3MB native exe |
| 3 | Dashboard runtime (paths, EEPROM, stubs) | IN PROGRESS - app main executing! |
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

| Property | Value |
|----------|-------|
| Title | Xbox Dashboard |
| Title ID | 0xFFFE0000 (system application) |
| Build | 3944 (v1.0) |
| Build Date | 2001-10-25 |
| Internal Name | "xapp" (Xbox Application) |
| Debug Path | `d:\xboxret\private\ui\xapp\obj\i386\xboxdash.exe` |
| Entry Point | 0x00052A81 |
| Image Size | 1.38 MB |
| Sections | 19 (.text 637KB, D3D, D3DX, XGRPH, DSOUND, WMA, XPP, DOLBY, XIPS, 6 language tables) |
| Kernel Imports | 134 (32 Nt* file I/O, 25 Ke* threading, 14 Mm* memory, 7 Hal* hardware) |
| Libraries | D3D8, D3DX8, XGRAPHC, DSOUND, LIBC, LIBCPMT (all XDK 3944) |
| Functions | 6,323 discovered, 6,258 translated to C (98.97% success) |
| Generated Code | 628,227 lines of C across 7 source files |
| Executable | ~6 MB native x86-64 Windows .exe |

### Current Boot Status

```
=== Xbox Dashboard - Static Recompilation ===
Loading XBE... 1,394,036 bytes
Initializing Xbox memory layout... 19 sections, 27/28 RAM mirrors
Initializing kernel bridge... 134/134 resolved (61 bridged, 73 stubbed)
Starting dashboard...
  PsCreateSystemThreadEx -> CRT _threadstart -> SEH prolog -> TLS copy
  _initterm (CRT initializers) -> OK
  App entry (sub_00052A12) -> Dashboard main ENTERED!
  sub_000558D0 (D3D/system init):
    KeInitializeDpc -> KeInitializeTimerEx -> KeSetTimer(DPC)
    NtAllocateVirtualMemory (x2) -> RtlInitializeCriticalSection
    HalRequestSoftwareInterrupt -> DPC dispatched and completed!
  -> Hangs deeper in D3D/display init (sub_00053DCE)
```

The dashboard's real initialization code is executing natively on Windows:
- CRT thread init with proper TLS copy and SEH frame setup
- Dashboard `main()` at 0x00052A12 called successfully
- DPC/timer system working (shutdown watchdog timer dispatches and completes)
- Memory allocation, critical sections, and kernel timing all functional
- 12+ kernel calls executing correctly

Current blocker: hang in D3D/display initialization after the DPC system is set up. Need to trace deeper into the rendering init path.

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
