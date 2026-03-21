/**
 * Xbox Dashboard - Manual function overrides and ICALL diagnostics
 *
 * Override specific Xbox VAs with hand-written code, or stub
 * problematic functions during bring-up.
 */

#include <stdio.h>
#include <stdint.h>

/* ── ICALL trace ring buffer ───────────────────────────────── */

volatile uint32_t g_icall_trace[16]  = {0};
volatile uint32_t g_icall_trace_idx  = 0;
volatile uint64_t g_icall_count      = 0;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

extern uint32_t g_eax;
extern ptrdiff_t g_xbox_mem_offset;

/* ── Manual function overrides ─────────────────────────────── */

recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /*
     * Dashboard-specific overrides go here.
     * During bring-up, use this to stub crashing functions:
     *
     * if (xbox_va == 0x000XXXXX) return stub_XXXXX;
     */

    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

void recomp_icall_fail_log(uint32_t va)
{
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (total calls: %llu)\n",
            va, (unsigned long long)g_icall_count);

    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
}
