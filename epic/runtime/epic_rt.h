/*
 * epic_rt.h - native runtime for the recompiled Epic Pinball code.
 *
 * The recompiler (tools/eprecomp.py) turns every basic block of the original
 * 16-bit x86 code into a C function that works on the state declared here.
 * This header is the whole "ABI" between generated code and the runtime:
 * registers, flags, memory access, the stack, ALU helpers with exact x86 flag
 * semantics, string instructions and the hooks for I/O ports and interrupts.
 *
 * Licence: same as the rest of the repository. Contains no game code or data.
 */
#ifndef EPIC_RT_H
#define EPIC_RT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* CPU state                                                                 */
/* ------------------------------------------------------------------------ */
typedef union { uint32_t e; uint16_t x; struct { uint8_t l, h; } b; } rt_reg;

typedef struct {
    rt_reg a, b, c, d;
    rt_reg si, di, bp, sp;
    uint16_t cs, ds, es, ss, fs, gs, ip;
    uint8_t cf, pf, af, zf, sf, of, df, ifl, tf;
} rt_cpu;

extern rt_cpu R;
extern uint8_t *M;              /* 1 MB + 64 KB of conventional memory */

#define EAX R.a.e
#define AX  R.a.x
#define AL  R.a.b.l
#define AH  R.a.b.h
#define EBX R.b.e
#define BX  R.b.x
#define BL  R.b.b.l
#define BH  R.b.b.h
#define ECX R.c.e
#define CX  R.c.x
#define CL  R.c.b.l
#define CH  R.c.b.h
#define EDX R.d.e
#define DX  R.d.x
#define DL  R.d.b.l
#define DH  R.d.b.h
#define ESI R.si.e
#define SI  R.si.x
#define EDI R.di.e
#define DI  R.di.x
#define EBP R.bp.e
#define BP  R.bp.x
#define ESP R.sp.e
#define SP  R.sp.x
#define CS  R.cs
#define DS  R.ds
#define ES  R.es
#define SS  R.ss
#define FS  R.fs
#define GS  R.gs
#define IP  R.ip
#define CF  R.cf
#define PF  R.pf
#define AF  R.af
#define ZF  R.zf
#define SF  R.sf
#define OF  R.of
#define DF  R.df
#define IFL R.ifl

/* ------------------------------------------------------------------------ */
/* memory                                                                    */
/* ------------------------------------------------------------------------ */
uint8_t rt_mmio_rd(uint32_t a);          /* A0000-AFFFF: VGA graphics aperture */
void    rt_mmio_wr(uint32_t a, uint8_t v);

static inline uint32_t LIN(uint16_t s, uint16_t o) { return ((uint32_t)s << 4) + o; }

static inline uint8_t RD8(uint16_t s, uint16_t o) {
    uint32_t a = LIN(s, o);
    if (a - 0xA0000u < 0x10000u) return rt_mmio_rd(a);
    return M[a];
}
static inline void WR8(uint16_t s, uint16_t o, uint8_t v) {
    uint32_t a = LIN(s, o);
    if (a - 0xA0000u < 0x10000u) { rt_mmio_wr(a, v); return; }
    if (a >= 0xC0000u) return;           /* ROM */
    M[a] = v;
}
static inline uint16_t RD16(uint16_t s, uint16_t o) {
    return (uint16_t)(RD8(s, o) | (RD8(s, (uint16_t)(o + 1)) << 8));
}
static inline void WR16(uint16_t s, uint16_t o, uint16_t v) {
    WR8(s, o, (uint8_t)v); WR8(s, (uint16_t)(o + 1), (uint8_t)(v >> 8));
}
static inline uint32_t RD32(uint16_t s, uint16_t o) {
    return RD16(s, o) | ((uint32_t)RD16(s, (uint16_t)(o + 2)) << 16);
}
static inline void WR32(uint16_t s, uint16_t o, uint32_t v) {
    WR16(s, o, (uint16_t)v); WR16(s, (uint16_t)(o + 2), (uint16_t)(v >> 16));
}

/* ------------------------------------------------------------------------ */
/* stack                                                                     */
/* ------------------------------------------------------------------------ */
static inline void PUSH16(uint16_t v) { SP = (uint16_t)(SP - 2); WR16(SS, SP, v); }
static inline uint16_t POP16(void) { uint16_t v = RD16(SS, SP); SP = (uint16_t)(SP + 2); return v; }
static inline void PUSH32(uint32_t v) { SP = (uint16_t)(SP - 4); WR32(SS, SP, v); }
static inline uint32_t POP32(void) { uint32_t v = RD32(SS, SP); SP = (uint16_t)(SP + 4); return v; }
static inline void PUSHA(void) {
    uint16_t sp = SP;
    PUSH16(AX); PUSH16(CX); PUSH16(DX); PUSH16(BX); PUSH16(sp); PUSH16(BP); PUSH16(SI); PUSH16(DI);
}
static inline void POPA(void) {
    DI = POP16(); SI = POP16(); BP = POP16(); (void)POP16(); BX = POP16(); DX = POP16(); CX = POP16(); AX = POP16();
}
static inline void ENTER(uint16_t sz, int lvl) {
    PUSH16(BP); { uint16_t fp = SP; int k;
    for (k = 1; k < lvl; k++) { BP = (uint16_t)(BP - 2); PUSH16(RD16(SS, BP)); }
    if (lvl > 0) PUSH16(fp);
    BP = fp; SP = (uint16_t)(SP - sz); }
}

/* ------------------------------------------------------------------------ */
/* flags                                                                     */
/* ------------------------------------------------------------------------ */
extern const uint8_t rt_parity[256];

static inline uint16_t GETFLAGS(void) {
    return (uint16_t)(0x0002 | (CF) | (PF << 2) | (AF << 4) | (ZF << 6) | (SF << 7) |
                      (R.tf << 8) | (IFL << 9) | (DF << 10) | (OF << 11) | 0xF000);
}
static inline void SETFLAGS(uint16_t f) {
    CF = f & 1; PF = (f >> 2) & 1; AF = (f >> 4) & 1; ZF = (f >> 6) & 1; SF = (f >> 7) & 1;
    R.tf = (f >> 8) & 1; IFL = (f >> 9) & 1; DF = (f >> 10) & 1; OF = (f >> 11) & 1;
}

/* ALU helpers. N = bits, T = type, H = sign bit */
#define RT_SZF(T, H, r) do { ZF = ((T)(r) == 0); SF = (((r) & (H)) != 0); PF = rt_parity[(uint8_t)(r)]; } while (0)

#define RT_ALU(N, T, H)                                                                      \
static inline T ADD##N(T a, T b) { T r = (T)(a + b); CF = r < a; OF = (((a ^ r) & (b ^ r) & (H)) != 0);  \
    AF = ((a ^ b ^ r) & 0x10) != 0; RT_SZF(T, H, r); return r; }                             \
static inline T ADC##N(T a, T b) { T c = CF; T r = (T)(a + b + c); CF = c ? (r <= a) : (r < a);          \
    OF = (((a ^ r) & (b ^ r) & (H)) != 0); AF = ((a ^ b ^ r) & 0x10) != 0; RT_SZF(T, H, r); return r; } \
static inline T SUB##N(T a, T b) { T r = (T)(a - b); CF = a < b; OF = (((a ^ b) & (a ^ r) & (H)) != 0);  \
    AF = ((a ^ b ^ r) & 0x10) != 0; RT_SZF(T, H, r); return r; }                             \
static inline T SBB##N(T a, T b) { T c = CF; T r = (T)(a - b - c); CF = c ? (a <= b) : (a < b);          \
    OF = (((a ^ b) & (a ^ r) & (H)) != 0); AF = ((a ^ b ^ r) & 0x10) != 0; RT_SZF(T, H, r); return r; } \
static inline T AND##N(T a, T b) { T r = (T)(a & b); CF = OF = AF = 0; RT_SZF(T, H, r); return r; }   \
static inline T OR##N(T a, T b)  { T r = (T)(a | b); CF = OF = AF = 0; RT_SZF(T, H, r); return r; }   \
static inline T XOR##N(T a, T b) { T r = (T)(a ^ b); CF = OF = AF = 0; RT_SZF(T, H, r); return r; }   \
static inline T INC##N(T a) { T r = (T)(a + 1); OF = (r == (T)(H)); AF = (r & 0xf) == 0; RT_SZF(T, H, r); return r; } \
static inline T DEC##N(T a) { T r = (T)(a - 1); OF = (a == (T)(H)); AF = (a & 0xf) == 0; RT_SZF(T, H, r); return r; } \
static inline T NEG##N(T a) { T r = (T)(0 - a); CF = a != 0; OF = (a == (T)(H)); AF = (a & 0xf) != 0; RT_SZF(T, H, r); return r; } \
static inline T NOT##N(T a) { return (T)~a; }                                                \
static inline T SHL##N(T a, unsigned c) { c &= 31; if (!c) return a; if (c > N) { CF = 0; a = 0; } else {     \
    CF = (T)(a >> (N - c)) & 1; a = (T)(a << c); } OF = ((a & (H)) != 0) ^ CF; RT_SZF(T, H, a); return a; }   \
static inline T SHR##N(T a, unsigned c) { c &= 31; if (!c) return a; OF = (a & (H)) != 0;    \
    if (c > N) { CF = 0; a = 0; } else { CF = (a >> (c - 1)) & 1; a = (T)(c == N ? 0 : a >> c); }             \
    if (c != 1) OF = 0; RT_SZF(T, H, a); return a; }                                         \
static inline T SAR##N(T a, unsigned c) { c &= 31; if (!c) return a; { int64_t s = (a & (H)) ? (int64_t)a - ((int64_t)(H) << 1) : (int64_t)a; \
    if (c >= N) { CF = s < 0; s = s < 0 ? -1 : 0; } else { CF = (s >> (c - 1)) & 1; s >>= c; }               \
    a = (T)s; } OF = 0; RT_SZF(T, H, a); return a; }                                         \
static inline T ROL##N(T a, unsigned c) { c &= 31; if (!c) return a; c %= N;                 \
    if (c) a = (T)((a << c) | (a >> (N - c))); CF = a & 1; OF = ((a & (H)) != 0) ^ CF; return a; }            \
static inline T ROR##N(T a, unsigned c) { c &= 31; if (!c) return a; c %= N;                 \
    if (c) a = (T)((a >> c) | (a << (N - c))); CF = (a & (H)) != 0; OF = CF ^ ((a & ((H) >> 1)) != 0); return a; } \
static inline T RCL##N(T a, unsigned c) { c &= 31; c %= (N + 1); while (c--) { T nc = (a & (H)) != 0;      \
    a = (T)((a << 1) | CF); CF = nc; } OF = ((a & (H)) != 0) ^ CF; return a; }               \
static inline T RCR##N(T a, unsigned c) { c &= 31; c %= (N + 1); if (c) OF = ((a & (H)) != 0) ^ CF;        \
    while (c--) { T nc = a & 1; a = (T)((a >> 1) | ((T)CF ? (T)(H) : 0)); CF = nc; } return a; }

RT_ALU(8, uint8_t, 0x80u)
RT_ALU(16, uint16_t, 0x8000u)
RT_ALU(32, uint32_t, 0x80000000u)

void rt_divide_error(void);

static inline void MUL8(uint8_t v)   { AX = (uint16_t)(AL * v); CF = OF = AH != 0; ZF = AX == 0; }
static inline void MUL16(uint16_t v) { uint32_t r = (uint32_t)AX * v; AX = (uint16_t)r; DX = (uint16_t)(r >> 16); CF = OF = DX != 0; ZF = r == 0; }
static inline void MUL32(uint32_t v) { uint64_t r = (uint64_t)EAX * v; EAX = (uint32_t)r; EDX = (uint32_t)(r >> 32); CF = OF = EDX != 0; }
static inline void IMUL8(uint8_t v)  { int16_t r = (int16_t)((int8_t)AL * (int8_t)v); AX = (uint16_t)r; CF = OF = r != (int8_t)r; }
static inline void IMUL16(uint16_t v){ int32_t r = (int32_t)(int16_t)AX * (int16_t)v; AX = (uint16_t)r; DX = (uint16_t)((uint32_t)r >> 16); CF = OF = r != (int16_t)r; }
static inline void IMUL32(uint32_t v){ int64_t r = (int64_t)(int32_t)EAX * (int32_t)v; EAX = (uint32_t)r; EDX = (uint32_t)((uint64_t)r >> 32); CF = OF = r != (int32_t)r; }
static inline uint16_t IMUL16x(uint16_t a, uint16_t b) { int32_t r = (int32_t)(int16_t)a * (int16_t)b; CF = OF = r != (int16_t)r; return (uint16_t)r; }
static inline uint32_t IMUL32x(uint32_t a, uint32_t b) { int64_t r = (int64_t)(int32_t)a * (int32_t)b; CF = OF = r != (int32_t)r; return (uint32_t)r; }
static inline void DIV8(uint8_t v) { if (!v || AX / v > 0xff) { rt_divide_error(); return; } { uint16_t q = AX / v, r = AX % v; AL = (uint8_t)q; AH = (uint8_t)r; } }
static inline void DIV16(uint16_t v) { uint32_t n = ((uint32_t)DX << 16) | AX; if (!v || n / v > 0xffff) { rt_divide_error(); return; } AX = (uint16_t)(n / v); DX = (uint16_t)(n % v); }
static inline void DIV32(uint32_t v) { uint64_t n = ((uint64_t)EDX << 32) | EAX; if (!v || n / v > 0xffffffffu) { rt_divide_error(); return; } EAX = (uint32_t)(n / v); EDX = (uint32_t)(n % v); }
static inline void IDIV8(uint8_t v) { int16_t n = (int16_t)AX; int8_t d = (int8_t)v; if (!d) { rt_divide_error(); return; }
    { int q = n / d, r = n % d; if (q > 127 || q < -128) { rt_divide_error(); return; } AL = (uint8_t)q; AH = (uint8_t)r; } }
static inline void IDIV16(uint16_t v) { int32_t n = (int32_t)(((uint32_t)DX << 16) | AX); int16_t d = (int16_t)v; if (!d) { rt_divide_error(); return; }
    { int32_t q = n / d, r = n % d; if (q > 32767 || q < -32768) { rt_divide_error(); return; } AX = (uint16_t)q; DX = (uint16_t)r; } }
static inline void IDIV32(uint32_t v) { int64_t n = (int64_t)(((uint64_t)EDX << 32) | EAX); int32_t d = (int32_t)v; if (!d) { rt_divide_error(); return; }
    { int64_t q = n / d, r = n % d; if (q > 2147483647LL || q < -2147483648LL) { rt_divide_error(); return; } EAX = (uint32_t)q; EDX = (uint32_t)r; } }

/* ------------------------------------------------------------------------ */
/* I/O ports                                                                 */
/* ------------------------------------------------------------------------ */
uint8_t rt_in8(uint16_t port);
void rt_out8(uint16_t port, uint8_t v);
#define IN8(p)  rt_in8((uint16_t)(p))
#define IN16(p) ((uint16_t)(rt_in8((uint16_t)(p)) | (rt_in8((uint16_t)((p) + 1)) << 8)))
#define OUT8(p, v)  rt_out8((uint16_t)(p), (uint8_t)(v))
#define OUT16(p, v) do { uint16_t _p = (uint16_t)(p), _v = (uint16_t)(v); rt_out8(_p, (uint8_t)_v); rt_out8((uint16_t)(_p + 1), (uint8_t)(_v >> 8)); } while (0)

/* ------------------------------------------------------------------------ */
/* string instructions                                                       */
/* ------------------------------------------------------------------------ */
enum { NONE = 0, REP = 1, REPE = 2, REPNE = 3 };
void rt_str(int op, int size, uint16_t srcseg, int rep);
enum { RT_MOVS, RT_STOS, RT_LODS, RT_CMPS, RT_SCAS };
#define STR_MOVS8(s, r)  rt_str(RT_MOVS, 1, s, r)
#define STR_MOVS16(s, r) rt_str(RT_MOVS, 2, s, r)
#define STR_MOVS32(s, r) rt_str(RT_MOVS, 4, s, r)
#define STR_STOS8(s, r)  rt_str(RT_STOS, 1, s, r)
#define STR_STOS16(s, r) rt_str(RT_STOS, 2, s, r)
#define STR_STOS32(s, r) rt_str(RT_STOS, 4, s, r)
#define STR_LODS8(s, r)  rt_str(RT_LODS, 1, s, r)
#define STR_LODS16(s, r) rt_str(RT_LODS, 2, s, r)
#define STR_LODS32(s, r) rt_str(RT_LODS, 4, s, r)
#define STR_CMPS8(s, r)  rt_str(RT_CMPS, 1, s, r)
#define STR_CMPS16(s, r) rt_str(RT_CMPS, 2, s, r)
#define STR_SCAS8(s, r)  rt_str(RT_SCAS, 1, s, r)
#define STR_SCAS16(s, r) rt_str(RT_SCAS, 2, s, r)

/* ------------------------------------------------------------------------ */
/* control transfer into the runtime                                         */
/* ------------------------------------------------------------------------ */
void rt_int(uint8_t n, uint16_t next_ip);   /* software interrupt */
void rt_hlt(void);
void rt_sti(void);
void rt_bad(uint16_t cs, uint16_t ip);      /* reached code the recompiler could not translate */

typedef void (*rt_block_fn)(void);
typedef rt_block_fn (*rt_lookup_fn)(uint16_t relseg, uint16_t ip);

typedef struct {
    const char *name;
    uint32_t image_crc;      /* CRC32 of the load image the code was generated from */
    uint32_t image_size;
    rt_lookup_fn lookup;
    const uint16_t *extsegs; /* image-relative segments of runtime-loaded drivers, 0xffff-terminated */
    uint16_t *load_seg;      /* where the runtime loaded this program's image (used by relocated constants) */
    uint16_t cs, ip, ss, sp, minalloc, maxalloc;
    int raw;                 /* 1: MASI driver module the game loads itself (MDRVnnnR.MUS) */
} rt_program;

#ifdef __cplusplus
}
#endif
#endif
