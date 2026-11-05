/*
 * sierra_rt.h - native runtime for recompiled 32-bit Windows (Win32s) games:
 * Sierra's 3-D Ultra Pinball series.
 *
 * The recompiler (sierra/tools/sprecomp.py) turns every basic block of the
 * original flat 32-bit x86 code into a C function over the state declared here.
 * This header is the whole ABI between generated code and the runtime:
 * registers, flags, guest memory, the stack, ALU helpers with x86 flag
 * semantics, string instructions, the x87 FPU and the dispatcher hooks.
 *
 * Licence: same as the rest of the repository. Contains no game code or data.
 */
#ifndef SIERRA_RT_H
#define SIERRA_RT_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* CPU state                                                                 */
/* ------------------------------------------------------------------------ */
typedef union { uint32_t e; uint16_t x; struct { uint8_t l, h; } b; } rt_reg;

typedef struct {
    rt_reg a, b, c, d, si, di, bp, sp;
    uint32_t eip;
    uint32_t fsbase;                       /* thread information block */
    uint8_t cf, pf, af, zf, sf, of, df;
    /* x87: values held as double; top-of-stack index and the status/control words */
    double st[8];
    uint8_t ftag[8];                       /* 1 = empty */
    int ftop;
    uint16_t fsw, fcw;
} rt_cpu;

extern rt_cpu R;
extern uint8_t *M;                         /* guest address space, GUEST_SIZE bytes */

#define GUEST_SIZE   0x20000000u           /* 512 MB of guest addresses */
#define GUEST_MASK   (GUEST_SIZE - 1)
#define THUNK_BASE   0x1ff00000u           /* imported functions live here (never code) */
#define THUNK_SLOTS  0x4000u
#define RET_MAGIC    0x1ffffff0u           /* return address that ends a native->guest call */

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
#define EIP R.eip
#define CF  R.cf
#define PF  R.pf
#define AF  R.af
#define ZF  R.zf
#define SF  R.sf
#define OF  R.of
#define DF  R.df

/* ------------------------------------------------------------------------ */
/* guest memory (little-endian host assumed: x86-64, ARM64)                  */
/* ------------------------------------------------------------------------ */
static inline uint8_t  RD8 (uint32_t a) { return M[a & GUEST_MASK]; }
static inline uint16_t RD16(uint32_t a) { uint16_t v; memcpy(&v, M + (a & GUEST_MASK), 2); return v; }
static inline uint32_t RD32(uint32_t a) { uint32_t v; memcpy(&v, M + (a & GUEST_MASK), 4); return v; }
static inline uint64_t RD64(uint32_t a) { uint64_t v; memcpy(&v, M + (a & GUEST_MASK), 8); return v; }
static inline void WR8 (uint32_t a, uint8_t  v) { M[a & GUEST_MASK] = v; }
static inline void WR16(uint32_t a, uint16_t v) { memcpy(M + (a & GUEST_MASK), &v, 2); }
static inline void WR32(uint32_t a, uint32_t v) { memcpy(M + (a & GUEST_MASK), &v, 4); }
static inline void WR64(uint32_t a, uint64_t v) { memcpy(M + (a & GUEST_MASK), &v, 8); }
#define GPTR(a) ((void *)(M + ((uint32_t)(a) & GUEST_MASK)))

/* ------------------------------------------------------------------------ */
/* stack                                                                     */
/* ------------------------------------------------------------------------ */
static inline void PUSH32(uint32_t v) { ESP -= 4; WR32(ESP, v); }
static inline void PUSH16(uint16_t v) { ESP -= 2; WR16(ESP, v); }
static inline uint32_t POP32(void) { uint32_t v = RD32(ESP); ESP += 4; return v; }
static inline uint16_t POP16(void) { uint16_t v = RD16(ESP); ESP += 2; return v; }

/* ------------------------------------------------------------------------ */
/* flags                                                                     */
/* ------------------------------------------------------------------------ */
static inline uint8_t rt_parity(uint8_t v) { v ^= v >> 4; v ^= v >> 2; v ^= v >> 1; return !(v & 1); }

#define DEF_ALU(N, T, BITS)                                                                         \
static inline T ADD##N(T a, T b) { T r = (T)(a + b); CF = r < a; OF = ((~(a ^ b) & (a ^ r)) >> (BITS-1)) & 1; \
    AF = ((a ^ b ^ r) >> 4) & 1; ZF = r == 0; SF = (r >> (BITS-1)) & 1; PF = rt_parity((uint8_t)r); return r; } \
static inline T ADC##N(T a, T b) { unsigned c = CF; T r = (T)(a + b + c); CF = c ? r <= a : r < a;      \
    OF = ((~(a ^ b) & (a ^ r)) >> (BITS-1)) & 1; AF = ((a ^ b ^ r) >> 4) & 1; ZF = r == 0;            \
    SF = (r >> (BITS-1)) & 1; PF = rt_parity((uint8_t)r); return r; }                                  \
static inline T SUB##N(T a, T b) { T r = (T)(a - b); CF = a < b; OF = (((a ^ b) & (a ^ r)) >> (BITS-1)) & 1; \
    AF = ((a ^ b ^ r) >> 4) & 1; ZF = r == 0; SF = (r >> (BITS-1)) & 1; PF = rt_parity((uint8_t)r); return r; } \
static inline T SBB##N(T a, T b) { unsigned c = CF; T r = (T)(a - b - c); CF = c ? a <= b : a < b;      \
    OF = (((a ^ b) & (a ^ r)) >> (BITS-1)) & 1; AF = ((a ^ b ^ r) >> 4) & 1; ZF = r == 0;             \
    SF = (r >> (BITS-1)) & 1; PF = rt_parity((uint8_t)r); return r; }                                  \
static inline T LOGIC##N(T r) { CF = 0; OF = 0; AF = 0; ZF = r == 0; SF = (r >> (BITS-1)) & 1; PF = rt_parity((uint8_t)r); return r; } \
static inline T AND##N(T a, T b) { return LOGIC##N((T)(a & b)); }                                    \
static inline T OR##N (T a, T b) { return LOGIC##N((T)(a | b)); }                                    \
static inline T XOR##N(T a, T b) { return LOGIC##N((T)(a ^ b)); }                                    \
static inline T INC##N(T a) { uint8_t c = CF; T r = ADD##N(a, 1); CF = c; return r; }                \
static inline T DEC##N(T a) { uint8_t c = CF; T r = SUB##N(a, 1); CF = c; return r; }                \
static inline T NEG##N(T a) { T r = SUB##N(0, a); CF = a != 0; return r; }                          \
static inline T NOT##N(T a) { return (T)~a; }                                                        \
static inline T SHL##N(T a, unsigned c) { c &= 31; if (!c) return a; if (c > BITS) { CF = 0; } else CF = (T)(a >> (BITS - c)) & 1; \
    T r = c >= BITS ? 0 : (T)(a << c); OF = ((r >> (BITS-1)) & 1) ^ CF; ZF = r == 0; SF = (r >> (BITS-1)) & 1; \
    PF = rt_parity((uint8_t)r); return r; }                                                           \
static inline T SHR##N(T a, unsigned c) { c &= 31; if (!c) return a; CF = c > BITS ? 0 : (T)(a >> (c - 1)) & 1; \
    T r = c >= BITS ? 0 : (T)(a >> c); OF = (a >> (BITS-1)) & 1; ZF = r == 0; SF = (r >> (BITS-1)) & 1; \
    PF = rt_parity((uint8_t)r); return r; }                                                           \
static inline T SAR##N(T a, unsigned c) { c &= 31; if (!c) return a; int64_t s = (int64_t)(int##BITS##_t)a; \
    if (c > BITS) c = BITS; CF = (s >> (c - 1)) & 1; T r = (T)(s >> c); OF = 0; ZF = r == 0;         \
    SF = (r >> (BITS-1)) & 1; PF = rt_parity((uint8_t)r); return r; }                                  \
static inline T ROL##N(T a, unsigned c) { c &= 31; if (!c) return a; c %= BITS; T r = c ? (T)((a << c) | (a >> (BITS - c))) : a; \
    CF = r & 1; OF = ((r >> (BITS-1)) & 1) ^ CF; return r; }                                         \
static inline T ROR##N(T a, unsigned c) { c &= 31; if (!c) return a; c %= BITS; T r = c ? (T)((a >> c) | (a << (BITS - c))) : a; \
    CF = (r >> (BITS-1)) & 1; OF = CF ^ ((r >> (BITS-2)) & 1); return r; }                           \
static inline T RCL##N(T a, unsigned c) { c &= 31; c %= (BITS + 1); while (c--) { uint8_t o = (a >> (BITS-1)) & 1; a = (T)((a << 1) | CF); CF = o; } \
    OF = ((a >> (BITS-1)) & 1) ^ CF; return a; }                                                     \
static inline T RCR##N(T a, unsigned c) { c &= 31; c %= (BITS + 1); OF = ((a >> (BITS-1)) & 1) ^ CF; \
    while (c--) { uint8_t o = a & 1; a = (T)((a >> 1) | ((T)CF << (BITS-1))); CF = o; } return a; }

DEF_ALU(8, uint8_t, 8)
DEF_ALU(16, uint16_t, 16)
DEF_ALU(32, uint32_t, 32)

uint32_t GETFLAGS(void);
void SETFLAGS(uint32_t f);

/* multiply / divide (accumulator forms); division by zero reports and stops */
void rt_divide_error(void);
static inline void MUL8(uint8_t v)  { uint16_t r = (uint16_t)(AL * v); AX = r; CF = OF = AH != 0; }
static inline void MUL16(uint16_t v){ uint32_t r = (uint32_t)AX * v; AX = (uint16_t)r; DX = (uint16_t)(r >> 16); CF = OF = DX != 0; }
static inline void MUL32(uint32_t v){ uint64_t r = (uint64_t)EAX * v; EAX = (uint32_t)r; EDX = (uint32_t)(r >> 32); CF = OF = EDX != 0; }
static inline void IMUL8(uint8_t v) { int16_t r = (int16_t)((int8_t)AL * (int8_t)v); AX = (uint16_t)r; CF = OF = r != (int8_t)r; }
static inline void IMUL16(uint16_t v){ int32_t r = (int32_t)(int16_t)AX * (int16_t)v; AX = (uint16_t)r; DX = (uint16_t)(r >> 16); CF = OF = r != (int16_t)r; }
static inline void IMUL32(uint32_t v){ int64_t r = (int64_t)(int32_t)EAX * (int32_t)v; EAX = (uint32_t)r; EDX = (uint32_t)((uint64_t)r >> 32); CF = OF = r != (int32_t)r; }
static inline uint16_t IMUL16x(uint16_t a, uint16_t b) { int32_t r = (int32_t)(int16_t)a * (int16_t)b; CF = OF = r != (int16_t)r; return (uint16_t)r; }
static inline uint32_t IMUL32x(uint32_t a, uint32_t b) { int64_t r = (int64_t)(int32_t)a * (int32_t)b; CF = OF = r != (int32_t)r; return (uint32_t)r; }
static inline void DIV8(uint8_t v)  { if (!v) { rt_divide_error(); return; } uint16_t n = AX; AL = (uint8_t)(n / v); AH = (uint8_t)(n % v); }
static inline void DIV16(uint16_t v){ if (!v) { rt_divide_error(); return; } uint32_t n = ((uint32_t)DX << 16) | AX; AX = (uint16_t)(n / v); DX = (uint16_t)(n % v); }
static inline void DIV32(uint32_t v){ if (!v) { rt_divide_error(); return; } uint64_t n = ((uint64_t)EDX << 32) | EAX; EAX = (uint32_t)(n / v); EDX = (uint32_t)(n % v); }
static inline void IDIV8(uint8_t v) { if (!v) { rt_divide_error(); return; } int16_t n = (int16_t)AX; AL = (uint8_t)(n / (int8_t)v); AH = (uint8_t)(n % (int8_t)v); }
static inline void IDIV16(uint16_t v){ if (!v) { rt_divide_error(); return; } int32_t n = (int32_t)(((uint32_t)DX << 16) | AX); AX = (uint16_t)(n / (int16_t)v); DX = (uint16_t)(n % (int16_t)v); }
static inline void IDIV32(uint32_t v){ if (!v) { rt_divide_error(); return; } int64_t n = (int64_t)(((uint64_t)EDX << 32) | EAX); int64_t d = (int32_t)v;
    if (d == -1 && n == INT64_MIN) { rt_divide_error(); return; } EAX = (uint32_t)(n / d); EDX = (uint32_t)(n % d); }

/* double-precision shifts and bit tests */
static inline uint32_t SHLD32(uint32_t d, uint32_t s, unsigned c) { c &= 31; if (!c) return d; uint32_t r = (d << c) | (s >> (32 - c));
    CF = (d >> (32 - c)) & 1; ZF = r == 0; SF = r >> 31; PF = rt_parity((uint8_t)r); OF = ((r ^ d) >> 31) & 1; return r; }
static inline uint32_t SHRD32(uint32_t d, uint32_t s, unsigned c) { c &= 31; if (!c) return d; uint32_t r = (d >> c) | (s << (32 - c));
    CF = (d >> (c - 1)) & 1; ZF = r == 0; SF = r >> 31; PF = rt_parity((uint8_t)r); OF = ((r ^ d) >> 31) & 1; return r; }
static inline uint32_t BSWAP32(uint32_t v) { return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24); }
static inline uint32_t BSF32(uint32_t v, uint32_t old) { if (!v) { ZF = 1; return old; } ZF = 0; unsigned i = 0; while (!(v & 1)) { v >>= 1; i++; } return i; }
static inline uint32_t BSR32(uint32_t v, uint32_t old) { if (!v) { ZF = 1; return old; } ZF = 0; unsigned i = 31; while (!(v >> i)) i--; return i; }

/* BCD adjust (rare) */
void DAA(void); void DAS(void); void AAA(void); void AAS(void); void AAM(uint8_t b); void AAD(uint8_t b);

/* ------------------------------------------------------------------------ */
/* string instructions (flat: DS = ES = SS)                                  */
/* ------------------------------------------------------------------------ */
enum { REP_NONE, REP_REP, REP_REPE, REP_REPNE };
void STR_MOVS(int size, int rep);
void STR_STOS(int size, int rep);
void STR_LODS(int size, int rep);
void STR_CMPS(int size, int rep);
void STR_SCAS(int size, int rep);

/* ------------------------------------------------------------------------ */
/* x87                                                                       */
/* ------------------------------------------------------------------------ */
#define ST(i) R.st[(R.ftop + (i)) & 7]
static inline void FPUSH(double v) { R.ftop = (R.ftop - 1) & 7; R.st[R.ftop] = v; R.ftag[R.ftop] = 0; }
static inline double FPOP(void) { double v = R.st[R.ftop]; R.ftag[R.ftop] = 1; R.ftop = (R.ftop + 1) & 7; return v; }
static inline float  RDF32(uint32_t a) { float f; uint32_t v = RD32(a); memcpy(&f, &v, 4); return f; }
static inline double RDF64(uint32_t a) { double f; uint64_t v = RD64(a); memcpy(&f, &v, 8); return f; }
static inline void WRF32(uint32_t a, double d) { float f = (float)d; uint32_t v; memcpy(&v, &f, 4); WR32(a, v); }
static inline void WRF64(uint32_t a, double d) { uint64_t v; memcpy(&v, &d, 8); WR64(a, v); }
double RDF80(uint32_t a);
void WRF80(uint32_t a, double d);
int64_t rt_fround(double d);                    /* rounding per FPU control word */
void FCOM(double a, double b);                  /* sets C0/C2/C3 in R.fsw */
void FCOMI(double a, double b);                 /* sets ZF/PF/CF */
double rt_frndint(double d);
void rt_fpu_misc(int op);                       /* fxtract, fprem, fscale ... see rt_fpu.c */
void FNINIT(void);
static inline uint16_t FNSTSW(void) { return (uint16_t)((R.fsw & ~0x3800) | ((R.ftop & 7) << 11)); }

/* ------------------------------------------------------------------------ */
/* dispatcher                                                                */
/* ------------------------------------------------------------------------ */
typedef void (*rt_block_fn)(void);
typedef struct {
    const char *name;
    uint32_t crc;                 /* CRC-32 of the original executable's code section */
    uint32_t image_base, code_lo, code_hi;
    const uint32_t *addrs;        /* sorted block addresses */
    const rt_block_fn *fns;
    uint32_t nblocks;
} rt_program;

void rt_bad(uint32_t eip);                      /* reached untranslated code */
void rt_trap(uint32_t eip, const char *what);   /* unsupported instruction */
void rt_int(uint8_t n, uint32_t next);
void rt_cpuid(void);
void rt_rdtsc(void);

/* call guest code from native code (window procedures, timers, sound callbacks);
 * stdcall or cdecl: args pushed right to left, returns EAX; all registers are preserved */
uint32_t rt_call_guest(uint32_t addr, int cdecl_, int nargs, const uint32_t *args);
void rt_run(void);

#ifdef __cplusplus
}
#endif
#endif
