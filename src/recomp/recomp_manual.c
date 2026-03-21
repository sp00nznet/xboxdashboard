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

/* Traced dashboard main (sub_00052A12) to find hang point */
extern void sub_000558D0(void);
extern void sub_00055A37(void);
extern void sub_000559DF(void);
extern void sub_0002A4FD(void);
extern void sub_0005586B(void);
extern void sub_0004F85A(void);
extern void sub_0002A4D4(void);
extern void sub_0002A40F(void);
extern void sub_00029777(void);
extern void sub_0002A4ED(void);

static void traced_dashboard_main(void)
{
    fprintf(stderr, "[DASH] Dashboard main entered\n"); fflush(stderr);

    fprintf(stderr, "[DASH] Calling sub_000558D0 (D3D/system init)...\n"); fflush(stderr);
    PUSH32(g_esp, 0); sub_000558D0();
    fprintf(stderr, "[DASH] sub_000558D0 returned (eax=0x%08X)\n", g_eax); fflush(stderr);

    /* Check fs:[0x20] for D3D cache - we set KPCR[0x20] to 0 */
    g_eax = MEM32(0x20);
    g_eax = MEM32(g_eax + 0x250);
    /* ecx = 0 since KPCR[0x20] is 0 */

    fprintf(stderr, "[DASH] Calling sub_00055A37...\n"); fflush(stderr);
    PUSH32(g_esp, 0); sub_00055A37();
    fprintf(stderr, "[DASH] sub_00055A37 returned\n"); fflush(stderr);

    fprintf(stderr, "[DASH] Calling sub_000559DF...\n"); fflush(stderr);
    PUSH32(g_esp, 0); sub_000559DF();
    fprintf(stderr, "[DASH] sub_000559DF returned\n"); fflush(stderr);

    /* Inline traced version of the full init chain */
    fprintf(stderr, "[DASH] Starting xapp init (sub_0002A40F)...\n"); fflush(stderr);

    /* sub_0002A4FD: call sub_0004F85A, then sub_0002A4D4 */
    fprintf(stderr, "[DASH]  [1] sub_0004F85A(0x33582)...\n"); fflush(stderr);
    PUSH32(g_esp, 0x33582);
    PUSH32(g_esp, 0); sub_0004F85A();
    fprintf(stderr, "[DASH]  [1] done\n"); fflush(stderr);

    /* sub_0002A40F init chain */
    uint32_t saved_ebp, saved_esp;
    PUSH32(g_esp, g_seh_ebp);
    saved_ebp = g_seh_ebp;
    saved_esp = g_esp;
    g_esp -= 0x208;
    PUSH32(g_esp, g_esi);
    g_esi = 0x121EF0; /* ecx = this */

    extern void sub_000345FA(void);
    extern void sub_0002A50F(void);
    extern void sub_0004FB15(void);
    extern void sub_00055C01(void);
    extern void sub_00032978(void);
    extern void sub_00029D34(void);

    fprintf(stderr, "[DASH]  [2] sub_000345FA...\n"); fflush(stderr);
    PUSH32(g_esp, 0); sub_000345FA();
    fprintf(stderr, "[DASH]  [2] done\n"); fflush(stderr);

    fprintf(stderr, "[DASH]  [3] sub_0002A50F...\n"); fflush(stderr);
    PUSH32(g_esp, 0); sub_0002A50F();
    fprintf(stderr, "[DASH]  [3] done\n"); fflush(stderr);

    fprintf(stderr, "[DASH]  [4] sub_0004FB15...\n"); fflush(stderr);
    PUSH32(g_esp, 0); sub_0004FB15();
    fprintf(stderr, "[DASH]  [4] done (eax=0x%08X)\n", g_eax); fflush(stderr);

    fprintf(stderr, "[DASH]  [5] sub_00055C01...\n"); fflush(stderr);
    PUSH32(g_esp, g_eax);
    PUSH32(g_esp, 0); sub_00055C01();
    POP32(g_esp, g_ecx);
    fprintf(stderr, "[DASH]  [5] done\n"); fflush(stderr);

    fprintf(stderr, "[DASH]  [6] sub_00032978...\n"); fflush(stderr);
    PUSH32(g_esp, 0); sub_00032978();
    fprintf(stderr, "[DASH]  [6] done\n"); fflush(stderr);

    fprintf(stderr, "[DASH]  [7] sub_00029D34 (this=0x%08X)...\n", g_esi); fflush(stderr);
    g_ecx = g_esi;
    PUSH32(g_esp, 0); sub_00029D34();
    fprintf(stderr, "[DASH]  [7] done (eax=0x%08X) - %s\n", g_eax,
            (g_eax & 0xFF) ? "SUCCESS" : "FAILED"); fflush(stderr);

    /* Return result */
    POP32(g_esp, g_esi);
    g_esp = saved_esp;
    POP32(g_esp, g_seh_ebp);
    /* eax already set by sub_00029D34 */
    g_esp += 4; /* ret from sub_0002A4FD */

    PUSH32(g_esp, 0);
    PUSH32(g_esp, 1);
    PUSH32(g_esp, 1);
    PUSH32(g_esp, 0); sub_0005586B();

    g_eax = 0;
    g_esp += 8; /* ret 4 */
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

/**
 * Hand-translated CRT _threadstart (sub_0004F8B5).
 *
 * This is the CRT thread initialization routine. The key difference from
 * the auto-generated version is that we re-read g_seh_ebp after calling
 * the SEH prolog, because the prolog sets up the caller's frame pointer.
 *
 * Original x86 flow:
 * 1. Call __SEH_prolog with local size 0x18 and handler at 0x1CA88
 * 2. Copy TLS data from template to thread-local storage
 * 3. Call _initterm (0x4F6BE) to run CRT initializers
 * 4. ICALL to [ebp+8] - the real app entry point (ctx1 from PsCreateSystemThreadEx)
 * 5. Call _initterm again for CRT terminators
 * 6. ICALL to [0x12068] - PsTerminateSystemThread with exit code
 */
extern void sub_000579F8(void);
extern void sub_0004F6BE(void);
extern recomp_func_t recomp_lookup(uint32_t xbox_va);
extern recomp_func_t recomp_lookup_kernel(uint32_t xbox_va);

static void fixed_sub_0004F8B5(void)
{
    /* Use ebp as a macro that always reads g_seh_ebp */
    #define EBP g_seh_ebp

    fprintf(stderr, "[CRT] _threadstart entered, esp=0x%08X\n", g_esp);
    fflush(stderr);

    /* call __SEH_prolog(0x18, 0x1CA88) */
    PUSH32(g_esp, 0x18);
    PUSH32(g_esp, 0x1CA88);
    PUSH32(g_esp, 0);
    fixed_seh_prolog(); /* This updates g_seh_ebp */

    fprintf(stderr, "[CRT] After SEH prolog, ebp=0x%08X esp=0x%08X\n", EBP, g_esp);
    fprintf(stderr, "[CRT]   [ebp+8]=0x%08X (app entry) [ebp+C]=0x%08X (ctx2)\n",
            MEM32(EBP + 8), MEM32(EBP + 0xC));
    fflush(stderr);

    /* [ebp-4] = 0 (set trylevel) */
    MEM32(EBP - 4) = 0;

    /* TLS setup: read TIB, copy TLS template data */
    g_eax = MEM32(0x28); /* fs:[0x28] = TLS pointer */
    MEM32(EBP - 28) = g_eax;
    g_edx = MEM32(g_eax + 0x28);
    g_edx = g_edx + 4;
    MEM32(EBP - 32) = g_edx;
    MEM32(g_edx - 4) = g_edx;

    /* Copy TLS template */
    g_ebx = MEM32(0x1CD58);
    g_esi = MEM32(0x1CD54);
    g_ebx = g_ebx - g_esi;
    MEM32(EBP - 36) = g_ebx;

    uint32_t copy_size = g_ebx;
    uint32_t dst = g_edx;
    uint32_t src = g_esi;

    if (copy_size > 0) {
        memcpy((void *)XBOX_PTR(dst), (void *)XBOX_PTR(src), copy_size);
    }

    /* Zero-fill BSS portion of TLS */
    g_ecx = MEM32(0x1CD64);
    if (g_ecx > 0) {
        memset((void *)XBOX_PTR(g_ebx + g_edx), 0, g_ecx);
    }

    /* Call _initterm (CRT initializers) */
    fprintf(stderr, "[CRT] Calling _initterm (initializers)\n");
    fflush(stderr);
    PUSH32(g_esp, 1);
    PUSH32(g_esp, 0);
    sub_0004F6BE();

    /* ICALL to app entry point at [ebp+8] */
    uint32_t app_entry = MEM32(EBP + 8);
    uint32_t app_ctx = MEM32(EBP + 0xC);
    fprintf(stderr, "[CRT] Calling app entry at 0x%08X with ctx=0x%08X\n", app_entry, app_ctx);
    fflush(stderr);

    {
        recomp_func_t fn = recomp_lookup_manual(app_entry);
        if (!fn) fn = recomp_lookup(app_entry);
        if (!fn) fn = recomp_lookup_kernel(app_entry);
        if (fn) {
            PUSH32(g_esp, app_ctx);
            PUSH32(g_esp, 0); /* dummy return address */
            fn();
        } else {
            fprintf(stderr, "[CRT] ERROR: app entry 0x%08X not found in dispatch!\n", app_entry);
            fflush(stderr);
        }
    }

    MEM32(EBP - 40) = g_eax; /* save app return code */

    /* Call _initterm (CRT terminators) */
    PUSH32(g_esp, 0);
    PUSH32(g_esp, 0);
    sub_0004F6BE();

    /* Call PsTerminateSystemThread via kernel thunk */
    uint32_t terminate_va = MEM32(0x12068);
    fprintf(stderr, "[CRT] Calling PsTerminateSystemThread (va=0x%08X, code=0x%08X)\n",
            terminate_va, MEM32(EBP - 40));
    fflush(stderr);

    {
        recomp_func_t fn = recomp_lookup_manual(terminate_va);
        if (!fn) fn = recomp_lookup(terminate_va);
        if (!fn) fn = recomp_lookup_kernel(terminate_va);
        if (fn) {
            PUSH32(g_esp, MEM32(EBP - 40));
            PUSH32(g_esp, 0);
            fn();
        }
    }

    #undef EBP
}

recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /* Fixed CRT _threadstart with proper g_seh_ebp handling */
    if (xbox_va == 0x0004F8B5) return fixed_sub_0004F8B5;
    /* Fixed __SEH_prolog that writes back to g_seh_ebp */
    if (xbox_va == 0x000579F8) return fixed_seh_prolog;
    /* Trace the dashboard main and init chain */
    if (xbox_va == 0x00052A12) return traced_dashboard_main;
    /* sub_0002A4FD is called directly from traced_dashboard_main, not via ICALL */

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
