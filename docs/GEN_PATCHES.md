# Generated-code patches (`src/recomp/gen/`)

`src/recomp/gen/` is **git-ignored** (regenerable via the xboxrecomp pipeline). Any manual
edit to the generated C is therefore lost on regen. This file is the authoritative list of
such patches so they can be re-applied. Prefer moving fixes into `recomp_manual.c` (tracked,
address-dispatched) when feasible; the patches below are direct gen edits made during
bring-up because they are awkward to express as ICALL overrides (they fix *direct*-call paths).

> Verified (2026-05-31): regenerating the affected functions with the current toolkit produces
> **byte-identical** C, so the recompiler's output is correct — these patches address **runtime**
> behaviour (broken CRT heap, worker-thread stack), not codegen.

## Active patches in `recomp_all.c`

### 1. CRT allocator → Xbox heap  (`crt_nh_malloc`, orig `sub_000572C8`)
The CRT heap (handle at `0x12DED0`) is invalid in the recomp, so the original alloc chain
(`sub_000572A1` → CRT heap) returns garbage. `nh_malloc` is the funnel for `malloc`,
`operator new` (`_2_YAPAXI_Z` / `_2_YAPAXI_Z_00055AB9`) and `nh_malloc` itself, for **both
direct and indirect** calls (the `operator_new` ICALL bridge does NOT catch the direct calls
used by the XIP name-setup `sub_000346F6`). At the top of `crt_nh_malloc`, before the original
body:
```c
{
    extern uint32_t xbox_HeapAlloc(uint32_t size, uint32_t align);
    uint32_t _sz = MEM32(esp + 4);
    if (_sz == 0) _sz = 1;
    eax = xbox_HeapAlloc(_sz, 16);
    esp += 4; return;
}
```
Caveat: CRT `free()` now mismatches (frees Xbox-heap ptrs from the broken CRT heap) — acceptable
for bring-up (leaks), consistent with the existing operator-new bridge. Proper fix later: init
the real CRT heap handle at `0x12DED0`, or move this to a `recomp_manual.c` bridge.

### 2. Force synchronous XIP load  (`sub_0003537D` and `sub_00035384`, two tail-copies)
The real XIP loader is invoked async via `PsCreateSystemThreadEx` (`sub_00035259` worker), whose
simulated worker-thread stack corrupts FPO stack-locals (e.g. the wide→ascii path buffer in
`sub_0002A90B`), so `CreateFile` gets a garbage name. The original goes **sync** when its 2nd arg
!= 0 (init passes 1) but the recomp mis-evaluated it and went async. In the entry-create block
(`loc_00035450`) disable the async branch so it always falls through to the sync `sub_00035176`:
```c
/* if (CMP_EQ(MEM8(ebp + 0xC), LO8(ebx))) goto loc_00035464; */  /* async disabled */
```

### 3. Diagnostics (optional, can be removed)
- `[XIP WORKER]` log in `sub_00035259` (entry/flag/name+4).
- `[XIP LOADER]` log in `sub_00035176` (~entry: esi, name[+4], first wide chars).

## Known remaining bug (not yet patched)
`sub_00035176 → sub_0002A90B → sub_2A66D`: with everything correct at `sub_00035176` (esi =
entry `0x124860`, name[+4] = valid wide `"y:\default.xip"`), the wide→ascii conversion still
yields an empty path → `CreateFile` err 3. Lifted code is correct; this is a runtime ESP/frame
desync in this chain. Next step: log `esi` at `sub_2A66D` entry to confirm, then fix the
frame/ESP handling. See `memory/project_dashboard_state.md` for full analysis.
