/**
 * Xbox Dashboard - Manual function overrides and ICALL diagnostics
 *
 * Override specific Xbox VAs with hand-written code, or stub
 * problematic functions during bring-up.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include "recomp_types.h"

/* ── ICALL trace ring buffer (defined in xbox_memory_layout.c) ── */

extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* D3D8 device (created in main.c) */
extern void *g_d3d_device; /* IDirect3DDevice8* */

/* Xbox heap allocator (from xbox_memory_layout.c) */
extern uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);

/* Forward declarations for D3D8 vtable methods we call */
typedef long (__stdcall *PFN_Clear)(void *dev, uint32_t count, void *rects,
                                     uint32_t flags, uint32_t color, float z, uint32_t stencil);
typedef long (__stdcall *PFN_Present)(void *dev, void *a, void *b, void *c, void *d);
typedef long (__stdcall *PFN_BeginScene)(void *dev);
typedef long (__stdcall *PFN_EndScene)(void *dev);
typedef long (__stdcall *PFN_SetRenderState)(void *dev, uint32_t state, uint32_t value);
typedef long (__stdcall *PFN_SetTransform)(void *dev, uint32_t state, const void *matrix);
typedef long (__stdcall *PFN_SetTexture)(void *dev, uint32_t stage, void *texture);
typedef long (__stdcall *PFN_SetTextureStageState)(void *dev, uint32_t stage, uint32_t type, uint32_t value);
typedef long (__stdcall *PFN_SetStreamSource)(void *dev, uint32_t stream, void *vb, uint32_t stride);
typedef long (__stdcall *PFN_SetVertexShader)(void *dev, uint32_t handle);
typedef long (__stdcall *PFN_DrawPrimitive)(void *dev, uint32_t type, uint32_t start, uint32_t count);
typedef long (__stdcall *PFN_DrawPrimitiveUP)(void *dev, uint32_t type, uint32_t count,
                                               const void *data, uint32_t stride);
typedef long (__stdcall *PFN_SetViewport)(void *dev, const void *vp);

/* Access vtable methods via COM interface */
#define D3D_VTBL(dev) (*(void***)(dev))

/* Correct vtable offsets from IDirect3DDevice8Vtbl in d3d8_xbox.h */
#define VTBL_Present              8
#define VTBL_BeginScene          10
#define VTBL_EndScene            11
#define VTBL_Clear               12
#define VTBL_SetTransform        13
#define VTBL_SetRenderState      15
#define VTBL_SetTextureStageState 17
#define VTBL_SetTexture          19
#define VTBL_SetStreamSource     21
#define VTBL_DrawPrimitive       25
#define VTBL_DrawPrimitiveUP     27
#define VTBL_SetViewport         37
#define VTBL_SetVertexShader     44

/* ── D3D helper call wrappers ──────────────────────────────── */

static inline long d3d_call_clear(uint32_t count, void *rects, uint32_t flags,
                                   uint32_t color, float z, uint32_t stencil) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_Clear)vt[VTBL_Clear])(g_d3d_device, count, rects, flags, color, z, stencil);
}

static inline long d3d_call_present(void) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_Present)vt[VTBL_Present])(g_d3d_device, NULL, NULL, NULL, NULL);
}

static inline long d3d_call_beginscene(void) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_BeginScene)vt[VTBL_BeginScene])(g_d3d_device);
}

static inline long d3d_call_endscene(void) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_EndScene)vt[VTBL_EndScene])(g_d3d_device);
}

static inline long d3d_call_setrenderstate(uint32_t state, uint32_t value) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_SetRenderState)vt[VTBL_SetRenderState])(g_d3d_device, state, value);
}

static inline long d3d_call_settransform(uint32_t type, const void *matrix) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_SetTransform)vt[VTBL_SetTransform])(g_d3d_device, type, matrix);
}

static inline long d3d_call_settexture(uint32_t stage, void *texture) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_SetTexture)vt[VTBL_SetTexture])(g_d3d_device, stage, texture);
}

static inline long d3d_call_settexturestagestate(uint32_t stage, uint32_t type, uint32_t value) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_SetTextureStageState)vt[VTBL_SetTextureStageState])(g_d3d_device, stage, type, value);
}

static inline long d3d_call_setstreamsource(uint32_t stream, void *vb, uint32_t stride) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_SetStreamSource)vt[VTBL_SetStreamSource])(g_d3d_device, stream, vb, stride);
}

static inline long d3d_call_setvertexshader(uint32_t handle) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_SetVertexShader)vt[VTBL_SetVertexShader])(g_d3d_device, handle);
}

static inline long d3d_call_drawprimitive(uint32_t type, uint32_t start, uint32_t count) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_DrawPrimitive)vt[VTBL_DrawPrimitive])(g_d3d_device, type, start, count);
}

static inline long d3d_call_drawprimitiveup(uint32_t type, uint32_t count,
                                              const void *data, uint32_t stride) {
    if (!g_d3d_device) return 0;
    void **vt = D3D_VTBL(g_d3d_device);
    return ((PFN_DrawPrimitiveUP)vt[VTBL_DrawPrimitiveUP])(g_d3d_device, type, count, data, stride);
}

/* ── Vertex buffer tracking ────────────────────────────────── */

/* Simple tracking for vertex buffers allocated in Xbox heap.
 * The generated code gets Xbox VAs; we track the size/stride
 * so DrawPrimitive can use DrawPrimitiveUP with the data. */
#define MAX_VB_SLOTS 32
static struct {
    uint32_t data_va;   /* Xbox VA of vertex data (from xbox_HeapAlloc) */
    uint32_t size;
    uint32_t fvf;
} g_vb_table[MAX_VB_SLOTS];
static int g_vb_count = 0;

/* Current stream source state */
static uint32_t g_cur_stream_va = 0;
static uint32_t g_cur_stream_stride = 0;
static uint32_t g_cur_stream_size = 0;

static int vb_find(uint32_t data_va) {
    for (int i = 0; i < g_vb_count; i++)
        if (g_vb_table[i].data_va == data_va) return i;
    return -1;
}

/* D3D bridge logging - throttled to avoid spam */
static uint64_t g_d3d_bridge_frame = 0;
static int g_d3d_log_enabled = 1;

static void d3d_log(const char *fmt, ...) {
    if (!g_d3d_log_enabled) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
}

/* ── D3D Bridge Functions ──────────────────────────────────── */

/**
 * sub_000AD8D0 - D3DDevice_SetRenderState_Simple (171 calls)
 * edx = render state enum, stack[+8] = value
 */
void bridge_SetRenderState(void)
{
    uint32_t value = MEM32(g_esp + 8);
    uint32_t state = g_edx;
    g_esp += 4; /* pop dummy return address */
    g_eax = (uint32_t)d3d_call_setrenderstate(state, value);
}

/**
 * sub_000AD820 - D3DDevice_SetRenderState (from render state dispatcher)
 * ecx = mapped state enum, edx = value
 */
void bridge_SetRenderState2(void)
{
    uint32_t state = g_ecx;
    uint32_t value = g_edx;
    g_esp += 4; /* pop dummy return address */
    g_eax = (uint32_t)d3d_call_setrenderstate(state, value);
}

/**
 * sub_000AF380 - D3DDevice_SetTransform (30 calls)
 * stack[+4] = transform type, stack[+8] = matrix VA
 */
void bridge_SetTransform(void)
{
    uint32_t type      = MEM32(g_esp + 4);
    uint32_t matrix_va = MEM32(g_esp + 8);
    g_esp += 4; /* pop dummy return address */
    if (matrix_va && matrix_va < 0x04000000) {
        g_eax = (uint32_t)d3d_call_settransform(type, (void *)XBOX_PTR(matrix_va));
    } else {
        g_eax = 0;
    }
}

/**
 * sub_000AF490 - D3DDevice_SetTransform variant (same convention)
 * stack[+4] = transform type, stack[+8] = matrix VA
 */
void bridge_SetTransform2(void)
{
    uint32_t type      = MEM32(g_esp + 4);
    uint32_t matrix_va = MEM32(g_esp + 8);
    g_esp += 4;
    if (matrix_va && matrix_va < 0x04000000) {
        g_eax = (uint32_t)d3d_call_settransform(type, (void *)XBOX_PTR(matrix_va));
    } else {
        g_eax = 0;
    }
}

/**
 * sub_000AF4C0 - D3DDevice_SetTransform (1-arg variant, likely projection)
 * stack[+4] = matrix VA (implicit type, probably D3DTS_PROJECTION=2)
 */
void bridge_SetTransform3(void)
{
    uint32_t matrix_va = MEM32(g_esp + 4);
    g_esp += 4;
    if (matrix_va && matrix_va < 0x04000000) {
        /* Assume projection transform (type 2) */
        g_eax = (uint32_t)d3d_call_settransform(2, (void *)XBOX_PTR(matrix_va));
    } else {
        g_eax = 0;
    }
}

/**
 * sub_000ADDC0 - D3DDevice_SetTexture
 * stack[+4] = stage, stack[+8] = texture ptr (0 = disable)
 */
void bridge_SetTexture(void)
{
    uint32_t stage   = MEM32(g_esp + 4);
    uint32_t tex_va  = MEM32(g_esp + 8);
    g_esp += 4;
    /* For now, always set NULL texture (we don't have texture bridging yet) */
    (void)tex_va;
    g_eax = (uint32_t)d3d_call_settexture(stage, NULL);
}

/**
 * sub_000B0520 - D3DDevice_CreateVertexBuffer
 * stack[+4]=size, stack[+8]=usage, stack[+12]=fvf, stack[+16]=pool, stack[+20]=ppVB_va
 * Allocates vertex data in Xbox heap so generated code can write to it.
 */
void bridge_CreateVertexBuffer(void)
{
    uint32_t size    = MEM32(g_esp + 4);
    uint32_t usage   = MEM32(g_esp + 8);
    uint32_t fvf     = MEM32(g_esp + 12);
    uint32_t pool    = MEM32(g_esp + 16);
    uint32_t out_va  = MEM32(g_esp + 20);
    g_esp += 4;
    (void)usage; (void)pool;

    d3d_log("[D3D BRIDGE] CreateVertexBuffer(size=%u, fvf=0x%X, out=0x%08X)\n",
            size, fvf, out_va);

    /* Allocate data in Xbox heap */
    uint32_t data_va = xbox_HeapAlloc(size, 16);
    if (!data_va) {
        d3d_log("[D3D BRIDGE]   FAILED: heap alloc returned 0\n");
        g_eax = 0x80004005u; /* E_FAIL */
        return;
    }

    /* Zero the buffer */
    memset((void *)XBOX_PTR(data_va), 0, size);

    /* Track it */
    if (g_vb_count < MAX_VB_SLOTS) {
        g_vb_table[g_vb_count].data_va = data_va;
        g_vb_table[g_vb_count].size = size;
        g_vb_table[g_vb_count].fvf = fvf;
        g_vb_count++;
    }

    /* Write the data VA as the "VB handle" to the output pointer */
    MEM32(out_va) = data_va;
    d3d_log("[D3D BRIDGE]   OK: data_va=0x%08X\n", data_va);
    g_eax = 0; /* S_OK */
}

/**
 * sub_000B0580 - D3DDevice_Lock / resource data setup
 * stack[+4]=vb_handle, stack[+8]=offset, stack[+12]=size,
 * stack[+16]=ppData_va, stack[+20]=flags
 * Returns data pointer so generated code can write vertex data.
 */
void bridge_LockVertexBuffer(void)
{
    uint32_t vb_handle = MEM32(g_esp + 4);
    uint32_t offset    = MEM32(g_esp + 8);
    uint32_t size      = MEM32(g_esp + 12);
    uint32_t pdata_va  = MEM32(g_esp + 16);
    uint32_t flags     = MEM32(g_esp + 20);
    g_esp += 4;
    (void)size; (void)flags;

    d3d_log("[D3D BRIDGE] Lock(vb=0x%08X, off=%u, size=%u, pdata=0x%08X)\n",
            vb_handle, offset, size, pdata_va);

    /* vb_handle IS the data VA (from our CreateVB bridge) */
    uint32_t data_ptr = vb_handle + offset;

    /* Write the data pointer to the output location */
    if (pdata_va && pdata_va < 0x04000000) {
        MEM32(pdata_va) = data_ptr;
    }

    d3d_log("[D3D BRIDGE]   data_ptr=0x%08X\n", data_ptr);
    g_eax = 0; /* S_OK */
}

/**
 * sub_000B14C0 - D3DDevice_SetStreamSource
 * stack[+4]=stream, stack[+8]=vb_handle, stack[+12]=stride
 */
void bridge_SetStreamSource(void)
{
    uint32_t stream   = MEM32(g_esp + 4);
    uint32_t vb_va    = MEM32(g_esp + 8);
    uint32_t stride   = MEM32(g_esp + 12);
    g_esp += 4;

    d3d_log("[D3D BRIDGE] SetStreamSource(stream=%u, vb=0x%08X, stride=%u)\n",
            stream, vb_va, stride);

    if (stream == 0) {
        g_cur_stream_va = vb_va;
        g_cur_stream_stride = stride;
        /* Look up size */
        int idx = vb_find(vb_va);
        g_cur_stream_size = (idx >= 0) ? g_vb_table[idx].size : 0;
    }
    g_eax = 0;
}

/**
 * sub_000B1860 - D3DDevice_SetVertexShader
 * stack[+4] = FVF handle (or shader handle)
 */
void bridge_SetVertexShader(void)
{
    uint32_t handle = MEM32(g_esp + 4);
    g_esp += 4;

    d3d_log("[D3D BRIDGE] SetVertexShader(0x%X)\n", handle);
    g_eax = (uint32_t)d3d_call_setvertexshader(handle);
}

/**
 * sub_000B0B50 - D3DDevice_DrawVertices (Xbox-specific)
 * stack[+4]=primtype, stack[+8]=start_vertex, stack[+12]=vertex_count
 * Uses DrawPrimitiveUP with vertex data from the current stream source.
 */
void bridge_DrawVertices(void)
{
    uint32_t prim_type    = MEM32(g_esp + 4);
    uint32_t start_vertex = MEM32(g_esp + 8);
    uint32_t vertex_count = MEM32(g_esp + 12);
    g_esp += 4;

    d3d_log("[D3D BRIDGE] DrawVertices(type=%u, start=%u, count=%u)\n",
            prim_type, start_vertex, vertex_count);

    if (!g_cur_stream_va || !g_cur_stream_stride || vertex_count == 0) {
        d3d_log("[D3D BRIDGE]   SKIP: no stream source or zero count\n");
        g_eax = 0;
        return;
    }

    /* Convert vertex count to primitive count */
    uint32_t prim_count = 0;
    switch (prim_type) {
        case 1: prim_count = vertex_count;     break; /* POINTLIST */
        case 2: prim_count = vertex_count / 2; break; /* LINELIST */
        case 3: prim_count = vertex_count - 1; break; /* LINESTRIP */
        case 4: prim_count = vertex_count / 3; break; /* TRIANGLELIST */
        case 5: prim_count = vertex_count - 2; break; /* TRIANGLESTRIP */
        case 6: prim_count = vertex_count - 2; break; /* TRIANGLEFAN */
        default: prim_count = vertex_count;    break;
    }

    if (prim_count == 0) {
        g_eax = 0;
        return;
    }

    /* Get pointer to vertex data in Xbox memory */
    uint32_t data_va = g_cur_stream_va + (start_vertex * g_cur_stream_stride);
    void *data_ptr = (void *)XBOX_PTR(data_va);

    d3d_log("[D3D BRIDGE]   DrawPrimitiveUP: prims=%u, data=0x%08X, stride=%u\n",
            prim_count, data_va, g_cur_stream_stride);

    /* Dump vertex data on first draw */
    {
        static int _dumped = 0;
        if (!_dumped && g_cur_stream_stride == 16) {
            _dumped = 1;
            float *vf = (float *)data_ptr;
            uint32_t *vi = (uint32_t *)data_ptr;
            for (uint32_t i = 0; i < vertex_count; i++) {
                fprintf(stderr, "  Vert[%u]: pos=(%.2f, %.2f, %.2f) color=0x%08X\n",
                        i, vf[i*4], vf[i*4+1], vf[i*4+2], vi[i*4+3]);
            }
            fflush(stderr);
        }
    }

    g_eax = (uint32_t)d3d_call_drawprimitiveup(prim_type, prim_count,
                                                data_ptr, g_cur_stream_stride);
}

/**
 * sub_000AF110 - D3DDevice_Swap / Present
 * stack[+4..+16] = 4 args (typically all 0)
 */
void bridge_Swap(void)
{
    g_esp += 4;
    /* Don't actually present - dashboard_end_frame handles it */
    g_eax = 0;
}

/**
 * Throttle D3D logging after first few frames
 */
void d3d_bridge_new_frame(void)
{
    g_d3d_bridge_frame++;
    if (g_d3d_bridge_frame > 3) g_d3d_log_enabled = 0;
}

/* ── CRT heap bridge ──────────────────────────────────────── */

/**
 * sub_00055A60 - operator new(size_t)
 * stack[+4] = size
 * The CRT heap (RtlAllocateHeap chain) hangs because the heap handle
 * at 0x12DED0 isn't valid. Bridge to xbox_HeapAlloc instead.
 */
static int g_new_count = 0;
void bridge_operator_new(void)
{
    uint32_t size = MEM32(g_esp + 4);
    g_esp += 4; /* pop dummy return */

    if (size == 0) size = 1;
    uint32_t va = xbox_HeapAlloc(size, 16);
    if (va) {
        memset((void *)XBOX_PTR(va), 0, size);
    }
    g_new_count++;
    if (g_new_count <= 50) {
        fprintf(stderr, "[NEW] #%d size=%u → 0x%08X\n", g_new_count, size, va);
        fflush(stderr);
    }
    g_eax = va;
    /* original does POP32 ecx twice + ret to clean 1 arg, but since
     * callers typically don't clean (cdecl with frame restore), this is fine */
}

/* ── Minimal scene graph ───────────────────────────────────── */

/* Synthetic Xbox VAs for custom scene functions.
 * These are in an unused VA range (0x00F00xxx) and registered
 * in recomp_lookup_manual so ICALL dispatch finds them. */
/* Must be below 0x00400000 to pass the RECOMP_ICALL_SAFE range check */
#define VA_SCENE_NOOP       0x00000101
#define VA_SCENE_RENDER     0x00000102

#include <math.h>

/**
 * No-op scene method. Used for device context vtable entries
 * that need valid targets but don't need to do anything.
 */
static void scene_noop(void)
{
    g_esp += 4; /* pop dummy return */
    g_eax = 0;
}

/**
 * Scene root Render method - draws a green sphere (the Xbox orb).
 * Called from render path via ICALL on scene_root vtable[+0x40].
 *
 * Convention: ecx = this (scene root), pushed before ICALL.
 */
static void scene_render(void)
{
    g_esp += 4; /* pop dummy return */
    if (!g_d3d_device) { g_eax = 0; return; }

    /* Draw a green disc using triangle list (avoids fan winding issues).
     * 24 segments, each segment = 1 triangle = 3 vertices. */
    #define ORB_SEGS 24
    struct { float x, y, z; uint32_t color; } verts[ORB_SEGS * 3];

    float cx = 0.0f, cy = 0.0f;
    float radius = 0.35f;         /* NDC units */
    uint32_t center_color = 0xFF00CC00;
    uint32_t edge_color   = 0xFF004400;

    for (int i = 0; i < ORB_SEGS; i++) {
        float a0 = (float)i / (float)ORB_SEGS * 6.28318f;
        float a1 = (float)(i + 1) / (float)ORB_SEGS * 6.28318f;
        int base = i * 3;
        /* Center */
        verts[base].x = cx; verts[base].y = cy;
        verts[base].z = 0.0f; verts[base].color = center_color;
        /* Edge vertex 1 */
        verts[base+1].x = cx + radius * cosf(a0);
        verts[base+1].y = cy + radius * sinf(a0);
        verts[base+1].z = 0.0f; verts[base+1].color = edge_color;
        /* Edge vertex 2 */
        verts[base+2].x = cx + radius * cosf(a1);
        verts[base+2].y = cy + radius * sinf(a1);
        verts[base+2].z = 0.0f; verts[base+2].color = edge_color;
    }

    /* All identity transforms — vertices in NDC space (-1 to 1) */
    float identity[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };
    d3d_call_settransform(0, identity);  /* world */
    d3d_call_settransform(1, identity);  /* view */
    d3d_call_settransform(2, identity);  /* projection */

    /* Render state: PC D3D8 enum values */
    d3d_call_setrenderstate(22, 1);  /* D3DRS_CULLMODE = D3DCULL_NONE */
    d3d_call_setrenderstate(27, 0);  /* D3DRS_ALPHABLENDENABLE = FALSE */
    d3d_call_setrenderstate(137, 0); /* D3DRS_LIGHTING = FALSE */
    d3d_call_setrenderstate(7, 0);   /* D3DRS_ZENABLE = D3DZB_FALSE */
    d3d_call_settexture(0, NULL);

    /* Set FVF = XYZ | DIFFUSE (0x042) */
    d3d_call_setvertexshader(0x042);

    /* Draw as triangle list */
    {
        d3d_call_drawprimitiveup(4, /* D3DPT_TRIANGLELIST */
                                  ORB_SEGS, /* primitive count */
                                  verts, sizeof(verts[0]));
    }

    g_eax = 0;
    #undef ORB_SEGS
    #undef ORB_VERTS
}

/**
 * Set up minimal scene graph objects in Xbox heap memory.
 * Creates device context + scene root with vtables pointing to
 * our custom functions registered via synthetic VAs.
 */
void dashboard_setup_scene(void)
{
    /* Allocate vtable for device context (needs entries at +0x0C, +0x10, +0x14) */
    uint32_t dev_vtbl_va = xbox_HeapAlloc(64, 16); /* 16 uint32 entries */
    if (!dev_vtbl_va) return;
    memset((void *)XBOX_PTR(dev_vtbl_va), 0, 64);
    MEM32(dev_vtbl_va + 0x0C) = VA_SCENE_NOOP;  /* vtable[3]: cleanup */
    MEM32(dev_vtbl_va + 0x10) = VA_SCENE_NOOP;  /* vtable[4]: begin */
    MEM32(dev_vtbl_va + 0x14) = VA_SCENE_NOOP;  /* vtable[5]: prepare */

    /* Allocate device context object (vtable ptr at offset 0) */
    uint32_t dev_ctx_va = xbox_HeapAlloc(64, 16);
    if (!dev_ctx_va) return;
    memset((void *)XBOX_PTR(dev_ctx_va), 0, 64);
    MEM32(dev_ctx_va) = dev_vtbl_va; /* vtable pointer */

    /* Store at XApp+0x4C (0x121F3C) */
    MEM32(0x121EF0 + 0x4C) = dev_ctx_va;

    /* Allocate vtable for scene root (needs entry at +0x40 = vtable[16]) */
    uint32_t root_vtbl_va = xbox_HeapAlloc(128, 16); /* 32 entries */
    if (!root_vtbl_va) return;
    memset((void *)XBOX_PTR(root_vtbl_va), 0, 128);
    MEM32(root_vtbl_va + 0x40) = VA_SCENE_RENDER; /* vtable[16]: Render */

    /* Allocate scene root object */
    uint32_t root_va = xbox_HeapAlloc(128, 16);
    if (!root_va) return;
    memset((void *)XBOX_PTR(root_va), 0, 128);
    MEM32(root_va) = root_vtbl_va; /* vtable pointer */

    /* Store at XApp+0x64 only if not already set by real init */
    if (!MEM32(0x121EF0 + 0x64))
        MEM32(0x121EF0 + 0x64) = root_va;

    fprintf(stderr, "[SCENE] dev_ctx=0x%08X root=0x%08X\n", dev_ctx_va, root_va);
    fprintf(stderr, "[SCENE] XApp+4C=0x%08X XApp+64=0x%08X\n",
            MEM32(0x121EF0 + 0x4C), MEM32(0x121EF0 + 0x64));
    fflush(stderr);
}

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
    /* D3D8 method bridges */
    if (xbox_va == 0x000AD8D0) return bridge_SetRenderState;
    if (xbox_va == 0x000AD820) return bridge_SetRenderState2;
    if (xbox_va == 0x000AF380) return bridge_SetTransform;
    if (xbox_va == 0x000AF490) return bridge_SetTransform2;
    if (xbox_va == 0x000AF4C0) return bridge_SetTransform3;
    if (xbox_va == 0x000ADDC0) return bridge_SetTexture;
    if (xbox_va == 0x000B0520) return bridge_CreateVertexBuffer;
    if (xbox_va == 0x000B0580) return bridge_LockVertexBuffer;
    if (xbox_va == 0x000B14C0) return bridge_SetStreamSource;
    if (xbox_va == 0x000B1860) return bridge_SetVertexShader;
    if (xbox_va == 0x000B0B50) return bridge_DrawVertices;
    if (xbox_va == 0x000AF110) return bridge_Swap;

    /* Minimal scene graph methods */
    if (xbox_va == VA_SCENE_NOOP)   return scene_noop;
    if (xbox_va == VA_SCENE_RENDER) return scene_render;

    /* CRT heap: operator new → xbox_HeapAlloc */
    if (xbox_va == 0x00055A60) return bridge_operator_new;

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
