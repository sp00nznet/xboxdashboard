/**
 * Manual function overrides and ICALL diagnostics
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 *
 * Common reasons to add manual overrides:
 *   - Trace a function to understand call flow (wrap the generated version)
 *   - Fix a function the lifter translated incorrectly
 *   - Stub out a function that crashes (return early, set eax to a safe value)
 *   - Redirect a function to a native implementation (e.g., skip CRT init)
 *   - Intercept D3D/audio calls for custom rendering or sound
 */

#include "recomp_types.h"
#include <stdio.h>
#include <stdint.h>

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns these: xbox_kernel defines them in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- defining them here too is a duplicate symbol and
 * the link fails with LNK2005 on all three.
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

extern uint32_t g_eax;
extern RECOMP_TLS uint32_t g_ebx, g_esi, g_edi;
extern ptrdiff_t g_xbox_mem_offset;

/* ── Manual function overrides ─────────────────────────────── */

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */

/* ── XIP block integrity check (sub_00034924) ────────────────
 *
 * The dashboard streams every XIP archive through a 64 KB buffered reader
 * (sub_00034A1B) and hashes each block, comparing the digest against a table
 * indexed by block number. A mismatch is not an error return -- it calls
 * HalReturnToFirmware(4), which is the console rebooting itself. This is a
 * system application checking the machine it is running on.
 *
 * The table lives at [this+4] and the block index at [this+0x14]:
 *
 *     if (!table)                        reboot
 *     if (index >= table[0x208])         reboot
 *     if (memcmp(digest, table+0x20C + index*20, 20))  reboot
 *
 * This does NOT skip the check silently. It reports what the check was about
 * to compare and lets the title continue, because a reboot three threads away
 * from the render loop is the least informative possible outcome -- it kills
 * the process mid-frame and says nothing about why. Whether the mismatch is
 * "the digest table is in flash and we have no BIOS" or "our archive read
 * returns the wrong bytes" is the thing worth knowing, and those look
 * identical from a dead process.
 *
 * Not scaffolding: this draws nothing and invents no content. It is the same
 * category as reporting a disc in the tray or an AC'97 codec as ready --
 * hardware the host does not have, substituted so the title can proceed.
 *
 * Named sub_00034924 deliberately. The caller reaches it by a *direct* call,
 * which the generated code emits as a plain C call to the symbol -- it never
 * consults recomp_lookup_manual. --exclude-manual scans this file for the
 * sub_ names it defines and omits those bodies from codegen, so this
 * definition is the one that links.
 */
void sub_00034924(void)
{
    uint32_t this_va = g_ecx;
    uint32_t buf     = MEM32(g_esp + 4);
    uint32_t len     = MEM32(g_esp + 8);
    uint32_t table   = MEM32(this_va + 4);
    uint32_t index   = MEM32(this_va + 0x14);
    static int reported;

    if (reported < 8) {
        reported++;
        fprintf(stderr, "[XIP VERIFY] block %u (%u bytes at 0x%08X): "
                        "digest table 0x%08X", index, len, buf, table);
        if (!table) {
            fprintf(stderr, " -- NULL, would reboot\n");
        } else {
            uint32_t count = MEM32(table + 0x208);
            uint32_t slot  = table + 0x20C + index * 20;
            int i, all_zero = 1;

            fprintf(stderr, ", %u entries", count);
            if (index >= count) {
                fprintf(stderr, " -- index out of range, would reboot\n");
            } else {
                fprintf(stderr, "\n    expected ");
                for (i = 0; i < 20; i++) {
                    uint8_t b = MEM8(slot + i);
                    if (b) all_zero = 0;
                    fprintf(stderr, "%02X", b);
                }
                if (all_zero)
                    fprintf(stderr, "  (all zero -- unset table)");
                fprintf(stderr, "\n");

                /* And what the title's own digest function makes of the same
                 * bytes. The table is verifiable from outside -- SHA1 over the
                 * 4-byte length followed by the block reproduces every entry
                 * exactly -- so a mismatch here is the guest-side hash being
                 * wrong, not the archive. Printing both is what tells those
                 * apart. */
                {
                    extern uint32_t xbox_HeapAlloc(uint32_t, uint32_t);
                    extern void sub_0005F7C1(void);
                    static uint32_t scratch;
                    uint32_t s_esp = g_esp, s_ebx = g_ebx,
                             s_esi = g_esi, s_edi = g_edi;

                    if (!scratch) scratch = xbox_HeapAlloc(32, 16);
                    if (scratch) {
                        for (i = 0; i < 20; i++) MEM8(scratch + i) = 0;
                        PUSH32(g_esp, scratch);
                        PUSH32(g_esp, len);
                        PUSH32(g_esp, buf);
                        PUSH32(g_esp, 0);        /* dummy return address */
                        sub_0005F7C1();
                        g_esp = s_esp; g_ebx = s_ebx;
                        g_esi = s_esi; g_edi = s_edi;

                        fprintf(stderr, "    computed ");
                        for (i = 0; i < 20; i++)
                            fprintf(stderr, "%02X", MEM8(scratch + i));
                        fprintf(stderr, "\n");
                    }
                }
            }
        }
        fflush(stderr);
    }

    /* thiscall, `ret 8`: pop the dummy return address and both stack args. */
    g_esp += 12;
    g_eax = 0;
}

recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * During early bring-up you will see many of these. Most are harmless
 * (the ICALL macro pops the dummy return address and continues).
 * Focus on the ones that cause crashes or incorrect behavior.
 */
void recomp_icall_fail_log(uint32_t va)
{
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (total calls: %llu)\n",
            va, (unsigned long long)g_icall_count);

    /* Dump last 16 call targets from the ring buffer */
    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
}

/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va)
{
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped %llu time(s) "
                    "(null or wild function pointer, at call #%llu)\n",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    fflush(stderr);
}

