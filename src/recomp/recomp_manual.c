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
extern void sub_00053DCE(void);
extern void sub_00051F15(void);
extern void sub_000559D3(void);
extern void sub_0005591E(void);
extern void sub_00055A37(void);
extern void sub_000559DF(void);
extern void sub_0002A4FD(void);
extern void sub_0005586B(void);
extern void sub_0004F85A(void);
extern void sub_0002A4D4(void);
extern void sub_0002A40F(void);
extern void sub_00029777(void);
extern void sub_0002A4ED(void);

static void traced_sub_000558D0(void); /* forward decl */

extern void sub_00052A12(void); /* original dashboard main (generated) */

/* Public entry point for the traced sub_000558D0, called from generated code */
void traced_sub_000558D0_entry(void)
{
    traced_sub_000558D0();
}

static void traced_dashboard_main(void)
{
    fprintf(stderr, "[DASH] Dashboard main entered, calling original sub_00052A12\n"); fflush(stderr);

    /* Call the original generated dashboard main.
     * sub_000558D0 is redirected to traced_sub_000558D0_entry (see recomp_0001.c)
     * which has critical g_seh_ebp fixes. Everything else runs through generated code. */
    PUSH32(g_esp, 0); sub_00052A12();

    fprintf(stderr, "[DASH] sub_00052A12 returned\n"); fflush(stderr);
    g_esp += 4; /* ret */
}

extern uint32_t g_seh_ebp;

/**
 * Stub Direct3DCreate8/Direct3D_CreateDevice (sub_000B1FD0).
 *
 * The D3D section was misinterpreted as code by the lifter. This function
 * is called from the xapp init to create the D3D device. We return a fake
 * device pointer so the init chain can continue.
 */
static void stub_Direct3DCreate(void)
{
    uint32_t flags = MEM32(g_esp + 4); /* first param */
    g_esp += 4; /* pop dummy return address */

    fprintf(stderr, "[D3D8] Direct3DCreate stub called (flags=0x%08X)\n", flags);
    fflush(stderr);

    /* Return the NV2A device we already created in sub_00053DCE */
    g_eax = MEM32(0x12DED0); /* D3D device pointer stored by sub_000558D0 */
    if (!g_eax) {
        /* Fallback: return a dummy non-zero pointer */
        g_eax = 0x00F80000;
    }

    fprintf(stderr, "[D3D8] Returning device=0x%08X\n", g_eax);
    fflush(stderr);

    g_esp += 4; /* caller cleans 1 arg (cdecl) */
}

/**
 * Fixed memcpy (sub_00055E90).
 *
 * The generated version uses RECOMP_ITAIL to jump to cleanup code
 * that was not lifted (0x55FE8, 0x55FF0, etc.), causing esi/edi
 * to never be restored. This properly saves/restores callee-saved regs.
 */
static void fixed_memcpy(void)
{
    /* Save callee-saved registers */
    uint32_t saved_esi = g_esi;
    uint32_t saved_edi = g_edi;

    /* Read params: dest=[esp+4], src=[esp+8], count=[esp+12] */
    uint32_t dest  = MEM32(g_esp + 4);
    uint32_t src   = MEM32(g_esp + 8);
    uint32_t count = MEM32(g_esp + 12);

    /* Pop return address (cdecl: caller cleans args) */
    g_esp += 4;

    if (count > 0) {
        memcpy((void *)XBOX_PTR(dest), (void *)XBOX_PTR(src), count);
    }

    g_eax = dest; /* memcpy returns dest */

    /* Restore callee-saved registers */
    g_esi = saved_esi;
    g_edi = saved_edi;

    /* cdecl: caller cleans the 3 args. We only popped the dummy ret. */
}

/**
 * Traced sub_000558D0 (D3D/system init wrapper)
 */
static void traced_sub_000558D0(void)
{
    uint32_t ebp;
    int _flags = 0;

    uint32_t saved_g_seh_ebp = g_seh_ebp; /* save for restoration after sub_0005591E */
    fprintf(stderr, "[TRACE] sub_000558D0 entered\n"); fflush(stderr);

    PUSH32(g_esp, ebp);
    ebp = g_esp;
    g_esp = g_esp - 0x30;
    PUSH32(g_esp, g_esi);
    PUSH32(g_esp, g_edi);
    g_esi = 0;

    fprintf(stderr, "[TRACE] Calling sub_00051F15 (timer/DPC init)...\n"); fflush(stderr);
    PUSH32(g_esp, 0); sub_00051F15();
    fprintf(stderr, "[TRACE] sub_00051F15 returned\n"); fflush(stderr);

    /* Clear 48 bytes of stack buffer */
    PUSH32(g_esp, 0xC);
    POP32(g_esp, g_ecx);
    g_eax = 0;
    g_edi = ebp + (uint32_t)(-48);
    { uint32_t _i; for (_i = 0; _i < g_ecx; _i++) MEM32(g_edi + _i*4) = g_eax; }
    g_edi += g_ecx * 4; g_ecx = 0;

    /* Set up parameters for sub_00053DCE */
    g_eax = ebp + (uint32_t)(-48);
    PUSH32(g_esp, g_eax);       /* param 7: &present_params */
    PUSH32(g_esp, g_esi);       /* param 6: 0 */
    MEM32(ebp + (uint32_t)(-48)) = 0x30;  /* present_params.size = 0x30 */
    uint32_t val_10138 = MEM32(0x10138);
    uint32_t val_10134 = MEM32(0x10134);
    PUSH32(g_esp, val_10138);   /* param 5 */
    PUSH32(g_esp, val_10134);   /* param 4 */
    PUSH32(g_esp, g_esi);       /* param 3: 0 */
    PUSH32(g_esp, 2);           /* param 2: adapter=2? */
    POP32(g_esp, g_edi);
    PUSH32(g_esp, g_edi);       /* param 1: 2 */

    fprintf(stderr, "[TRACE] Calling sub_00053DCE (D3D init) with params:\n"); fflush(stderr);
    fprintf(stderr, "[TRACE]   [0x10134]=0x%08X [0x10138]=0x%08X esi=%u\n",
            val_10134, val_10138, g_esi); fflush(stderr);

    PUSH32(g_esp, 0); sub_00053DCE();

    fprintf(stderr, "[TRACE] sub_00053DCE returned! eax=0x%08X\n", g_eax); fflush(stderr);

    MEM32(0x12DED0) = g_eax;
    if (g_eax != 0) {
        /* Non-zero = D3D device created successfully */
        fprintf(stderr, "[TRACE] D3D init success (device=0x%08X), calling sub_0005591E\n", g_eax); fflush(stderr);
        g_seh_ebp = ebp; /* CRITICAL: sub_0005591E is fpo_leaf */
        sub_0005591E();
        /* sub_0005591E's epilog uses g_seh_ebp (our ebp) to clean up.
         * Restore g_seh_ebp for subsequent functions. */
        g_seh_ebp = saved_g_seh_ebp;
        fprintf(stderr, "[TRACE] sub_0005591E returned, esp=0x%08X\n", g_esp); fflush(stderr);
        return;
    }

    /* Zero = D3D init failed, take error/reboot path */
    fprintf(stderr, "[TRACE] D3D init failed (eax=0), calling sub_000559D3 (error path)\n"); fflush(stderr);
    g_ecx = 0;
    g_ecx++;
    g_eax = g_ecx;
    g_seh_ebp = ebp;
    sub_000559D3();
    return;
}

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
    /* Fixed memcpy that properly saves/restores esi/edi */
    if (xbox_va == 0x00055E90) return fixed_memcpy;
    /* Stub D3D8 Direct3DCreate - return fake device pointer */
    if (xbox_va == 0x000B1FD0) return stub_Direct3DCreate;
    /* Traced D3D/system init wrapper */
    if (xbox_va == 0x000558D0) return traced_sub_000558D0;
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
