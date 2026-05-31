# Xbox Dashboard Recompiled

**The first-ever native PC port of the original Xbox Dashboard.**

Remember that green orb? The ambient hum of a spaceship control room? The satisfying *bwoom* when you scrolled through menus? We're bringing it back -- not through emulation, but by statically recompiling the original `xboxdash.xbe` into a native Windows executable.

This project uses [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) to translate the Xbox Dashboard's x86 machine code into C, then compiles it with MSVC against replacement runtime libraries (D3D8->D3D11, Xbox kernel->Win32, etc.). The result is a standalone `.exe` that runs the original dashboard code natively.

## Status

**Target:** Dashboard build 3944 (v1.0 - the one that shipped with launch consoles)

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Project setup & XBE extraction | DONE |
| 1 | XBE analysis (parse, disasm, func_id) | DONE - 6,323 functions, 134 kernel imports |
| 2 | Lift to C & first build | DONE - 628K lines of C, ~6MB native exe |
| 3 | Dashboard runtime (paths, EEPROM, stubs) | DONE - 50+ kernel calls, full init chain |
| 4 | UI rendering (D3D8 init, 3D orb, fonts) | DONE - green orb @ 60fps, 12 D3D8→D3D11 bridges |
| 4.5 | XAP/XIP scene engine bring-up | IN PROGRESS - engine fully mapped; real XIP loader now runs (see below) |
| 5 | Polish (input, audio, settings) | Pending |

### Phase 4.5 — XAP/XIP scene engine (current focus)

The dashboard UI is driven by **`default.xip`**, which contains the scene as **VRML97-style
text** (`DEF theScreen Screen { ... }`) plus **JavaScript** behaviour scripts. The dashboard
is effectively a small **VRML + ECMAScript engine**: text is compiled to a **stack-machine
bytecode** which a VM (`sub_00031DDE`) executes against a **node-class reflection registry**
(C-prefixed classes `CScreenSaver`/`CTextNode`/`CLevel`/… with `{name, member-offset, type}`
field tables) to build the scene graph. This is far more than a "XAP parser" and has no analog
in the sibling recomp projects.

Reverse-engineered with **Ghidra** (FidDb name recovery) and **headless IDA Pro 9.1**
(Hex-Rays). Real load path: `sub_0003534B` (XIP load) → worker/sync → `sub_00035176` (opens
`y:\default.xip`, reads header/entry-table/string-table/data, `sub_0003503E` processes
resources by type) → resource provider → compiler → bytecode VM → reflection → render.

**Fixes landed this phase** (see `docs/GEN_PATCHES.md` for the gitignored gen-file patches):
- **`Y:` drive mapping** — the dashboard opens `y:\default.xip`; the kernel path layer had no
  `Y:` rule. Added `Y:\ → game/` (xboxrecomp `kernel_path.c`).
- **CRT allocator redirect** — the CRT heap (handle `0x12DED0`) is invalid; `nh_malloc`
  (the funnel for `malloc`/`operator new`) now routes to the working `xbox_HeapAlloc`, so the
  real loader's allocations (e.g. the filename buffer) are valid for both direct and indirect calls.
- **Forced synchronous XIP load** — the loader was running on a `PsCreateSystemThreadEx`
  worker thread whose simulated stack corrupts FPO stack-locals; forcing the sync/main-thread
  path (which the original takes) fixed that class of corruption.

**Current blocker:** a runtime ESP/frame desync deep in the FPO call chain
(`sub_00035176 → sub_0002A90B → sub_2A66D`) yields an empty filename to `CreateFile`. The
recompiler's *output is verified correct* (regen is byte-identical), so this is a runtime
simulation issue to debug — not a codegen one.

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
Initializing kernel bridge... 134/134 resolved (62 bridged, 72 stubbed)
NV2A GPU initialized: VRAM=64MB RAMIN=1024KB
Starting dashboard...
  PsCreateSystemThreadEx -> CRT _threadstart -> SEH prolog -> TLS copy
  _initterm (CRT initializers) -> OK
  Dashboard main (sub_00052A12) entered
  D3D/system init (sub_000558D0):
    Timer/DPC init -> KeSetTimer + DPC dispatch OK
    NV2A device creation (sub_00053DCE) -> SUCCESS (1MB at 0x00F80000)
    D3D device setup (sub_0005387F) -> OK
  File system:
    C:\tdata, C:\tdata\fffe0000, C:\tdata\fffe0000\music -> opened OK
    XIP archive paths ready (C:\ -> game/)
  Xapp init chain (7 steps):
    [1] Heap setup -> OK
    [2] CRT lock init -> OK
    [3] File/path init -> OK (NtCreateFile, RtlInitAnsiString)
    [4] Random seed -> OK
    [5] Display config -> OK
    [6] Settings/EEPROM -> OK (NtReadFile, ExQueryPoolBlockSize)
    [7] App init (sub_00029D34) -> FAILS (needs D3D8 device with vtable)
  Cleanup -> DPC dispatch -> HalReturnToFirmware (stubbed)
```

**44+ kernel calls executing correctly** including:
- `NtAllocateVirtualMemory` (1MB GPU memory), `NtCreateFile` (directory creation)
- `RtlInitAnsiString`, `RtlEnter/LeaveCriticalSection` (CRT locking)
- `KeSetTimer`, `HalRequestSoftwareInterrupt` (DPC dispatch)
- `ExAllocatePoolWithTag`, `ExFreePool` (heap management)
- NV2A MMIO hook active for GPU register access at 0xFD000000

**WINDOW RENDERING AT 33 FPS:** A 640x480 window displays with D3D11 swap chain via the xboxrecomp D3D8-to-D3D11 translation layer. The main loop runs tick+render continuously. D3D bridge functions route SetRenderState (171 call sites) and SetTransform (30 call sites) to the real D3D11 backend. Clear+Present runs each frame. Next: bridge remaining D3D methods (BeginScene, EndScene, DrawPrimitive, CreateTexture) and load XIP archives for dashboard UI assets.

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

### Key Milestones So Far

- **FATX extraction** - Wrote custom QCOW2 + FATX filesystem tools to pull dashboard from Xbox HDD image
- **XBE analysis** - 19 sections, 134 kernel imports, internal codename "xapp" (Xbox Application)
- **Function recovery** - Recovered 3,087 "split-tail" functions the disassembler missed by scanning for stubbed call targets
- **CRT thread init** - Hand-translated `_threadstart` to fix `g_seh_ebp` frame pointer sharing between SEH prolog and caller
- **DPC dispatch** - Implemented `HalRequestSoftwareInterrupt` with deferred procedure call queue, shutdown watchdog timer runs
- **SEH prolog fix** - Fixed generated `__SEH_prolog` to write back `g_seh_ebp`, plus post-processed 48 call sites
- **NV2A device creation** - GPU instance memory (1MB) allocated and initialized, device setup succeeds
- **CRT memcpy fix** - Replaced broken `sub_00055E90` (had unresolved jump-table targets clobbering esi/edi)
- **CRT lock table** - Pre-initialized 36 critical section entries to prevent infinite recursion in `_mtinitlocknum`
- **Full init chain** - All 7 xapp initialization steps execute: heap, locks, files, RNG, display, settings, app init
- **File system** - `C:\` drive mapped to `game/`, `RtlInitAnsiString` bridge for proper path handling
- **D3D8 window** - 640x480 window with D3D11 swap chain, presenting at ~33 FPS with VSync
- **D3D bridges** - SetRenderState and SetTransform routed to D3D8-to-D3D11 layer
- **Main loop** - Dashboard tick+render loop running continuously with frame presentation
- **Scene graph** - Green orb drawing via ICALL dispatch; real scene root + scene manager created
- **Symbol recovery** - Ghidra FidDb pass recovered 133 CRT/XDK names (incl. LZX/XIP decompressors); applied to the recompiled C
- **XAP/XIP engine mapped** - identified as a VRML97 + JavaScript engine: text→bytecode compiler + stack-machine VM (`sub_00031DDE`) + node-class reflection registry (via headless IDA Pro Hex-Rays)
- **Real XIP loader running** - `Y:` drive mapping + CRT-heap allocator redirect + forced-sync load bring the dashboard's *own* `sub_0003534B`→`sub_00035176` load path online (replacing the hand-rolled loader)

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
