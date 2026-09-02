# Xbox Dashboard Recompiled

Statically recompiling the original Xbox Dashboard (`xboxdash.xbe`, build 3944) into a native
Windows executable — not emulation. [xboxrecomp](https://github.com/sp00nznet/xboxrecomp)
translates its x86 machine code to C, which MSVC compiles against replacement runtime
libraries (Xbox kernel → Win32, D3D8 → D3D11, NV2A, DirectSound).

> ### It does not render anything yet.
>
> There is no picture. The window is black, and that is the honest state of the project.
>
> What does work: the dashboard boots, runs its full init chain, brings up its *own* D3D8,
> sizes and allocates its own 640x480 surfaces, submits NV2A commands, and reads its own
> `default.xip` scene archive off disk. It then fails to build a scene from it and returns to
> firmware. Nothing has ever been drawn by the dashboard on the PC.
>
> An earlier version of this README claimed a "green orb at 60fps". That orb was ours — a
> hand-written disc drawn by scaffolding in this repo, not by the dashboard. It has been
> deleted, along with the fake scene root, hand-rolled asset loader and 2,204 return-zero
> stubs that surrounded it. Treat any screenshot from before 2026-09-02 as retired.

## Status

**Target:** Dashboard build 3944 (v1.0 - the one that shipped with launch consoles)

Nothing below claims a working UI. Phases 4 and 4.5 are where the project actually is.

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Project setup & XBE extraction | DONE |
| 1 | XBE analysis (parse, disasm, func_id) | DONE - 3,873 functions, 134 kernel imports |
| 2 | Lift to C & first build | DONE - 475K lines of C, ~6MB native exe |
| 3 | Dashboard runtime (paths, EEPROM, stubs) | DONE - full init chain, 424 kernel calls |
| 4 | UI rendering | IN PROGRESS - the dashboard's own D3D8 runs and submits a pushbuffer; nothing on screen yet |
| 4.5 | XAP/XIP scene engine bring-up | IN PROGRESS - `default.xip` opens and streams in through the dashboard's own loader |
| 5 | Polish (input, audio, settings) | Pending |

### The rule this project runs on

**The dashboard drives the rendering; the toolkit provides the framebuffer.** Nothing here
draws content the dashboard did not ask for. Until 2026-09-02 that rule was broken: the
"green orb at 60fps" in earlier versions of this README was *our* 24-segment disc, drawn by a
hand-written `scene_render()` hooked onto a fabricated scene root, next to a hand-rolled XIP
parser and a fake `Direct3DCreate` — the same scaffolding trap the Burnout 3 bring-up had to
back out of. All of it is deleted. `src/recomp/recomp_manual.c` now holds no overrides at all.

What replaces it is the toolkit's observation path: the dashboard's own statically-linked
D3D8 (262 recompiled functions in the `D3D` section) builds an NV2A pushbuffer in guest RAM,
and `RECOMP_PB_SCAN` / `RECOMP_PB_EXEC` / `RECOMP_FB_WINDOW` survey it, execute what can be
executed honestly, and put the guest's own framebuffer on screen. A black window is then a
fact about the dashboard, not about our renderer.

### Phase 4.5 — XAP/XIP scene engine

The dashboard UI is driven by **`default.xip`**, which contains the scene as **VRML97-style
text** (`DEF theScreen Screen { ... }`) plus **JavaScript** behaviour scripts. The dashboard
is effectively a small **VRML + ECMAScript engine**: text is compiled to a **stack-machine
bytecode** which a VM (`sub_00031DDE`) executes against a **node-class reflection registry**
(C-prefixed classes `CScreenSaver`/`CTextNode`/`CLevel`/… with `{name, member-offset, type}`
field tables) to build the scene graph. This is far more than a "XAP parser" and has no analog
in the sibling recomp projects.

Reverse-engineered with **Ghidra** (FidDb name recovery) and **headless IDA Pro 9.1**
(Hex-Rays). Real load path: `sub_0003534B` (XIP load) → `sub_00035176` (opens the archive,
reads header/entry-table/string-table/data, `sub_0003503E` processes resources by type) →
resource provider → compiler → bytecode VM → reflection → render.

**Fixes landed this phase:**
- **Function detection was truncating `__heap_init`** — seeding `tools.disasm` with
  `func_id`'s `identified_functions.json` (which the tool's own help recommends) injects
  addresses that land *inside* already-detected bodies, and the containing function is then
  cut short at that address. `sub_0005A771` lost its `pop esi; ret`, so the CRT heap was never
  initialised and every allocation after it was garbage. Seed only from measured
  indirect-branch targets (`tools.recomp.icall_feedback`), never from the classifier's list.
- **`\Device\Harddisk0\Partition2\` now maps to the game dir** (xboxrecomp `kernel_path.c`).
  Partition2 is C:, the system partition, which on a console is where the dashboard's own
  assets live — it opens them by device path as well as through `Y:`. They were being sent to
  the save directory. With this, `default.xip` opens and reads.
- **Indirect-call feedback dumps on the firmware-exit path** — that path ends in
  `ExitProcess`, which does not run `atexit`, so a title that gave up during boot never wrote
  the target set that would explain why.

**Current blocker:** having read `default.xip`, the dashboard probes for a loose
`default.xap`, does not find one (it lives *inside* the archive), and returns to firmware.
Three functions in the xapp init chain — `sub_0002A40F`, `sub_0002A4D4`, `sub_0002A4FD` —
return with `ebx` and `edi` changed (caught by `-DRECOMP_ABI_CHECK`), which is the shape of
bug that silently truncates a caller's loop. Whether that is what breaks the in-archive
fallback is the next thing to establish.

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
| Functions | 3,873 discovered, 3,873 translated to C (0 failed) |
| Generated Code | 474,831 lines of C across 4 source files |
| Executable | ~6 MB native x86-64 Windows .exe |

The function count went *down* from the 6,323 quoted before 2026-09-02. That earlier figure
came from a hand-rolled "split-tail" scan that manufactured entry points; most were not
functions, and 2,204 of them were hand-stubbed to `return 0`. The current number is what the
disassembler actually finds, plus the vtable targets the title is measured reaching.

### Current Boot Status

Everything below is the dashboard's own code, running unmodified. No overrides.

```
=== Xbox Dashboard (build 3944) - Static Recompilation ===
Xbox memory mapped, 19 sections, 27/28 RAM mirrors
Kernel thunk bridge: 134/134 resolved (89 bridged, 45 stub)
Starting dashboard...
  PsCreateSystemThreadEx -> CRT _threadstart -> SEH prolog -> TLS copy -> _initterm
  Dashboard main (sub_00052A12) entered
  D3D/system init (sub_000558D0):
    NV2A device creation (sub_00053DCE) -> OK (1 MB at 0x00F80000)
    D3D device setup (sub_0005387F) -> OK
  The dashboard's own D3D8 (sub_000B2630):
    present params -> 640x480, format 7, 1 backbuffer   <- read from its own state
    surface sizing (sub_000B4070) -> 1280x960 = 0x4B0000, 640x480 = 0x12C000
    MmAllocateContiguousMemoryEx -> pushbuffer 520 KB, surfaces in contiguous RAM
    NV2A DMA_PUT advances -> the dashboard is submitting GPU commands
  File system:
    partition1\tdata, \tdata\fffe0000, \tdata\fffe0000\music -> opened OK
    partition2\default.xip -> OPENED, 1.5 MB read in 64 KB chunks
      ("XIP0" magic, then VRML/JS scene text streaming in)
  Then: probes for a loose partition2\default.xap, does not find one,
        and calls HalReturnToFirmware
```

**424 kernel calls** execute before it gives up, including `NtAllocateVirtualMemory`,
`MmAllocateContiguousMemoryEx`, `NtCreateFile`/`NtReadFile` (the archive read),
`RtlEnter/LeaveCriticalSection`, `KeSetTimer` and `HalRequestSoftwareInterrupt`.

**Not yet on screen.** The pushbuffer survey reports the surface as unset and no draws, so
there is nothing for the framebuffer window to show. That is expected while the scene fails
to load: with no scene graph there is nothing to draw. The framebuffer window
(`RECOMP_FB_WINDOW=1`) is on so that the moment there *is*, it appears without anyone here
writing a renderer.

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

### Regenerating the recompiled C

From the `xboxrecomp` clone (that is where `tools/` lives):

```bash
py -3 -m tools.xbe_parser   ../dashboard/game/xboxdash.xbe --json ../dashboard/game/xboxdash_analysis.json
py -3 -m tools.disasm       ../dashboard/game/xboxdash.xbe --extra-sections XIPS,DOLBY --force \
                            --seed-functions ../dashboard/icall_seeds.json
py -3 -m tools.func_id      ../dashboard/game/xboxdash.xbe
py -3 -m tools.abi_analysis ../dashboard/game/xboxdash.xbe
py -3 -m tools.recomp       ../dashboard/game/xboxdash.xbe --all --split 1000 \
                            --gen-dir ../dashboard/src/recomp/gen \
                            --trace-functions ../dashboard/trace_funcs.json
```

`icall_seeds.json` holds indirect-branch targets the dashboard was *measured* reaching
(`tools.recomp.icall_feedback merge <dump>` then `seeds --out ... --xbe ...`, from the
`icall_targets.dump` a run leaves behind).

**Do not seed from `tools/func_id/output/identified_functions.json`.** The disassembler's own
help suggests it; it contains addresses inside existing function bodies, and each one
truncates the function containing it. That is how `__heap_init` lost its epilogue.

### Running

```bash
bin/Release/xboxdash.exe
```

The executable expects the original dashboard files in the `game/` subdirectory. Useful
during bring-up:

| Variable | What it gives you |
|----------|-------------------|
| `RECOMP_FB_WINDOW=1` | A window on the guest's own framebuffer. Nothing else scans it out |
| `RECOMP_PB_SCAN=1` | Survey of the pushbuffer the dashboard submits, ranked by what is unimplemented |
| `RECOMP_PB_EXEC=1` | Executes the surface/clear methods and screen-space geometry from that pushbuffer |
| `RECOMP_TRACE_ARGS=12` | Stack arguments at each `--trace-functions` entry |
| `RECOMP_WATCHDOG_SECS=20` | Guest call stack when it stops making progress |

`-DRECOMP_ABI_CHECK` (on by default in `CMakeLists.txt`) makes every call verify the callee
restored `ebx`/`esi`/`edi` and popped its return address. It costs three compares per call and
it is how the truncated `__heap_init` was found.

### Key Milestones So Far

- **FATX extraction** - Wrote custom QCOW2 + FATX filesystem tools to pull dashboard from Xbox HDD image
- **XBE analysis** - 19 sections, 134 kernel imports, internal codename "xapp" (Xbox Application)
- **DPC dispatch** - `HalRequestSoftwareInterrupt` with a deferred procedure call queue; the shutdown watchdog timer runs
- **NV2A device creation** - GPU instance memory (1MB) allocated and initialized, device setup succeeds
- **CRT lock table** - 36 critical-section entries pre-initialised so `_mtinitlocknum` cannot recurse forever
- **Full init chain** - all seven xapp initialisation steps execute: heap, locks, files, RNG, display, settings, app init
- **Symbol recovery** - Ghidra FidDb pass recovered 133 CRT/XDK names (incl. LZX/XIP decompressors)
- **XAP/XIP engine mapped** - identified as a VRML97 + JavaScript engine: text→bytecode compiler + stack-machine VM (`sub_00031DDE`) + node-class reflection registry (via headless IDA Pro Hex-Rays)
- **Scaffolding removed (2026-09-02)** - the hand-written orb, fake scene root, hand-rolled XIP parser, fake `Direct3DCreate`, and 2,204 return-zero stubs are gone; `recomp_manual.c` holds no overrides
- **The CRT heap actually initialises** - found by `-DRECOMP_ABI_CHECK`: `__heap_init` had been truncated by a bad function-detection seed and never ran its epilogue. Every allocator workaround built on top of that is now unnecessary
- **The dashboard's own D3D8 runs** - it reads its own 640x480 present parameters, sizes its own surfaces, allocates its own pushbuffer, and advances `DMA_PUT`; nothing in this repo tells it what to draw
- **The dashboard's own loader reads `default.xip`** - 1.5 MB of VRML/JavaScript scene source streamed in through `sub_0003534B` → `sub_00035176`, after mapping partition2 to the game dir

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
