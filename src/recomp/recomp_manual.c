/**
 * Xbox Dashboard - Manual function overrides and ICALL diagnostics
 *
 * Override specific Xbox VAs with hand-written code, or stub
 * problematic functions during bring-up.
 */

#include <stdio.h>
#include <stdint.h>
#include "recomp_types.h"

/* ── ICALL trace ring buffer (defined in xbox_memory_layout.c) ── */

extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* Register state and memory offset provided by recomp_types.h */

/* ── Manual function overrides ─────────────────────────────── */

/* Forward declarations for traced functions */
extern void sub_0004F8B5(void);
extern uint32_t g_ecx, g_edx, g_ebx, g_esi, g_edi, g_esp;

/* Hook for sub_0004F6BE to trace CRT thread init flow */
extern void sub_0004F6BE(void);
static int s_f6be_count = 0;
static void traced_sub_0004F6BE(void)
{
    uint32_t arg = *(uint32_t *)((uint8_t *)g_xbox_mem_offset + g_esp + 4);
    fprintf(stderr, "[TRACE] sub_0004F6BE called (count=%d, arg=0x%08X)\n", ++s_f6be_count, arg);
    fflush(stderr);
    sub_0004F6BE();
}

extern uint32_t g_seh_ebp;

/**
 * Fixed __SEH_prolog that writes back to g_seh_ebp.
 *
 * The original SEH prolog sets up the caller's frame pointer (ebp),
 * but since it's a separate translated function with a local ebp,
 * the value is lost on return. We fix this by updating g_seh_ebp
 * so the caller can pick up the correct frame.
 */
static void fixed_seh_prolog(void)
{
    uint32_t ebp = g_seh_ebp;

    /* push __except_handler3 address */
    PUSH32(g_esp, 0x5BCD8);
    /* push fs:[0] (SEH chain head) */
    g_eax = MEM32(0);
    PUSH32(g_esp, g_eax);
    /* mov fs:[0], esp (register new SEH frame) */
    MEM32(0) = g_esp;
    /* eax = [esp+0x10] (the local-size parameter from caller's PUSH) */
    g_eax = MEM32(g_esp + 0x10);
    /* save old ebp at [esp+0x10] */
    MEM32(g_esp + 0x10) = ebp;
    /* ebp = esp + 0x10 (frame pointer points to saved ebp) */
    ebp = g_esp + 0x10;
    /* sub esp, eax (allocate locals) */
    g_esp = g_esp - g_eax;
    /* push ebx, esi, edi (callee-saved) */
    PUSH32(g_esp, g_ebx);
    PUSH32(g_esp, g_esi);
    PUSH32(g_esp, g_edi);
    /* eax = [ebp - 8] (trylevel) */
    g_eax = MEM32(ebp - 8);
    /* [ebp - 24] = esp */
    MEM32(ebp - 24) = g_esp;
    /* push eax */
    PUSH32(g_esp, g_eax);
    /* eax = [ebp - 4] */
    g_eax = MEM32(ebp - 4);
    /* [ebp - 4] = -1 (trylevel = unwind) */
    MEM32(ebp - 4) = 0xFFFFFFFF;
    /* [ebp - 8] = eax */
    MEM32(ebp - 8) = g_eax;

    /* CRITICAL: Write ebp back to g_seh_ebp so caller picks it up */
    g_seh_ebp = ebp;

    /* ret */
    g_esp += 4;
}

static void traced_sub_0004F8B5(void)
{
    fprintf(stderr, "[TRACE] sub_0004F8B5 entered, esp=0x%08X seh_ebp=0x%08X\n", g_esp, g_seh_ebp);
    fprintf(stderr, "  Stack: [esp]=0x%08X [esp+4]=0x%08X [esp+8]=0x%08X [esp+C]=0x%08X\n",
            *(uint32_t *)((uint8_t *)g_xbox_mem_offset + g_esp),
            *(uint32_t *)((uint8_t *)g_xbox_mem_offset + g_esp + 4),
            *(uint32_t *)((uint8_t *)g_xbox_mem_offset + g_esp + 8),
            *(uint32_t *)((uint8_t *)g_xbox_mem_offset + g_esp + 12));
    fflush(stderr);
    sub_0004F8B5();
    fprintf(stderr, "[TRACE] sub_0004F8B5 returned, eax=0x%08X esp=0x%08X\n", g_eax, g_esp);
    fflush(stderr);
}

recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /* Trace the thread start routine and dashboard main during bring-up */
    if (xbox_va == 0x0004F8B5) return traced_sub_0004F8B5;
    if (xbox_va == 0x0004F6BE) return traced_sub_0004F6BE;
    if (xbox_va == 0x000579F8) return fixed_seh_prolog;

    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

void recomp_icall_fail_log(uint32_t va)
{
    fprintf(stderr, "[ICALL FAIL] VA 0x%08X not in dispatch (call #%llu, esp=0x%08X)\n",
            va, (unsigned long long)g_icall_count, g_esp);

    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
}

/* Log ALL ICALL dispatches during bring-up */
static uint64_t s_icall_log_count = 0;
void recomp_icall_log(uint32_t va)
{
    if (s_icall_log_count < 50) {
        fprintf(stderr, "[ICALL] #%llu dispatch to VA 0x%08X\n",
                (unsigned long long)s_icall_log_count, va);
        fflush(stderr);
    }
    s_icall_log_count++;
}
