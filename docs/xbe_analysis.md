# Xbox Dashboard XBE Analysis

## Overview

- **File**: xboxdash.xbe (1,394,036 bytes)
- **Title**: Xbox Dashboard
- **Title ID**: 0xFFFE0000 (system application)
- **Build**: 3944 (v1.0, shipped with launch consoles)
- **Build Date**: 2001-10-25 16:40:15 UTC
- **Debug Path**: `d:\xboxret\private\ui\xapp\obj\i386\xboxdash.exe`
- **Internal Name**: "xapp" (Xbox Application)
- **Region**: North America, Japan, Rest of World
- **Media**: HDD only

## Memory Layout

| Field | Value |
|-------|-------|
| Base Address | 0x00010000 |
| Image Size | 0x00160640 (1.38 MB) |
| Entry Point | 0x00052A81 |
| Kernel Thunks | 0x00012000 |
| Stack Commit | 0x10000 (64 KB) |
| Heap Reserve | 0x100000 (1 MB) |
| Heap Commit | 0x1000 (4 KB) |
| TLS Index | 0x0012D798 |
| TLS Zero Fill | 144 bytes |

## Sections (19)

| # | Name | VirtAddr | Size | Flags | Description |
|---|------|----------|------|-------|-------------|
| 0 | .text | 0x00012000 | 637 KB | X, HRO | Main executable code |
| 1 | D3D | 0x000AD720 | 72 KB | W, X | Direct3D 8 library (LTCG) |
| 2 | D3DX | 0x000BF220 | 150 KB | W, X | D3DX utility library |
| 3 | XGRPH | 0x000E49E0 | 8 KB | W, X | XGraphics library |
| 4 | DSOUND | 0x000E67C0 | 31 KB | W, X | DirectSound library |
| 5 | WMADECXM | 0x000EE240 | 5 KB | W, X | WMA decoder (extra) |
| 6 | WMADEC | 0x000EF580 | 101 KB | W, X | WMA decoder |
| 7 | DVDTHUNK | 0x001082E0 | 1 KB | X | DVD thunk code |
| 8 | XPP | 0x00108740 | 31 KB | W, X | Xbox Platform Plugin (input) |
| 9 | .data | 0x00110220 | 137 KB | W, X | Data section |
| 10 | DOLBY | 0x001326A0 | 27 KB | X | Dolby audio codec |
| 11 | .data1 | 0x00139440 | 7 KB | W, X | Additional data |
| 12 | XIPS | 0x0013B1C0 | 20 KB | INS | XIP support code |
| 13 | EnglishXlate | 0x001402E0 | 32 KB | W, INS | English translations |
| 14 | JapaneseXlate | 0x001481A0 | 27 KB | W, INS | Japanese translations |
| 15 | GermanXlate | 0x0014EB60 | 33 KB | W, INS | German translations |
| 16 | FrenchXlate | 0x001570E0 | 34 KB | W, INS | French translations |
| 17 | SpanishXlate | 0x0015F8C0 | 34 KB | W, INS | Spanish translations |
| 18 | ItalianXlate | 0x001680C0 | 33 KB | W, INS | Italian translations |

Total code sections: ~1.1 MB executable code

## Libraries (XDK 3944)

- XAPILIB 1.0.3944 - Xbox API Library
- XAPILBP 1.0.3944 - Xbox API Library (backup)
- LIBC 1.0.3944 - C Runtime Library
- XBOXKRNL 1.0.3944 - Xbox Kernel
- D3D8 1.0.3944 - Direct3D 8
- D3DX8 1.0.3944 - D3DX Utility Library
- XGRAPHC 1.0.3944 - Xbox Graphics
- DSOUND 1.0.3944 - DirectSound
- LIBCPMT 1.0.3944 - C++ Standard Library

## Kernel Imports (134)

### Notable Dashboard-Specific Imports
- `ExQueryNonVolatileSetting` / `ExSaveNonVolatileSetting` - EEPROM settings read/write
- `HalIsResetOrShutdownPending` / `HalInitiateShutdown` - System power management
- `HalEnableSecureTrayEject` - DVD tray control
- `NtSetSystemTime` - Clock setting
- `NtQueryDirectoryFile` - Directory enumeration (saves, content)
- `WRITE_PORT_BUFFER_USHORT/ULONG` - Direct hardware I/O
- `XcSHAInit`, `XcRC4Key`, `XcPKDecPrivate` - Crypto for content verification

### Import Categories
- Av (display): 4
- Ex (executive): 5
- Hal (hardware): 7
- Io (I/O manager): 8
- Ke (kernel): 25
- Kf (fast kernel): 2
- Mm (memory): 14
- Nt (system calls): 32
- Ob (object manager): 2
- Ps (process/thread): 3
- Rtl (runtime library): 13
- Xbox (system data): 7
- Xc (crypto): 3
- Other: 9

## Key Observations

1. **"xapp" internal name** - The dashboard is internally called "xapp" (Xbox Application), built from `d:\xboxret\private\ui\xapp\`
2. **XIPS section** (20KB) - Dedicated section for XIP support code, confirming the dashboard has built-in XIP archive parsing
3. **6 language sections** - English, Japanese, German, French, Spanish, Italian translation tables as separate loadable sections
4. **DVDTHUNK** - Small thunk section for DVD operations
5. **WMA decoder** - Two WMA sections (5KB + 101KB) for soundtrack playback
6. **DOLBY** - Dolby audio codec for surround sound
7. **.text is 637KB** - The actual dashboard logic. Expect ~3000-5000 functions
8. **134 kernel imports** - Heavy use of file I/O (32 Nt* calls), memory management (14 Mm*), and threading (25 Ke*)
