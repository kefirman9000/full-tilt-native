/*
 * rt_cpu.c - CPU side of the Sierra runtime: dispatcher, flags, string instructions, x87 helpers,
 * and calls between native code and recompiled guest code.
 */
#include "rt_internal.h"
#include <stdio.h>
#include <stdlib.h>

rt_cpu R;
uint8_t *M;

static const rt_program *prog;
static rt_block_fn *direct;            /* one entry per byte of the code range */
static uint32_t code_lo, code_hi;
int rt_async_depth;
static uint32_t async_tick;

/* ------------------------------------------------------------------------ */
uint32_t GETFLAGS(void)
{
    return 0x202u | CF | (PF << 2) | (AF << 4) | (ZF << 6) | (SF << 7) | (DF << 10) | (OF << 11);
}

void SETFLAGS(uint32_t f)
{
    CF = f & 1; PF = (f >> 2) & 1; AF = (f >> 4) & 1; ZF = (f >> 6) & 1; SF = (f >> 7) & 1;
    DF = (f >> 10) & 1; OF = (f >> 11) & 1;
}

void DAA(void) { uint8_t a = AL, c = CF; if ((a & 15) > 9 || AF) { AL += 6; AF = 1; } else AF = 0;
    if (a > 0x99 || c) { AL += 0x60; CF = 1; } else CF = 0; ZF = AL == 0; SF = AL >> 7; PF = rt_parity(AL); }
void DAS(void) { uint8_t a = AL, c = CF; if ((a & 15) > 9 || AF) { AL -= 6; AF = 1; } else AF = 0;
    if (a > 0x99 || c) { AL -= 0x60; CF = 1; } else CF = 0; ZF = AL == 0; SF = AL >> 7; PF = rt_parity(AL); }
void AAA(void) { if ((AL & 15) > 9 || AF) { AX += 0x106; AF = CF = 1; } else AF = CF = 0; AL &= 15; }
void AAS(void) { if ((AL & 15) > 9 || AF) { AX -= 6; AH--; AF = CF = 1; } else AF = CF = 0; AL &= 15; }
void AAM(uint8_t b) { if (!b) { rt_divide_error(); return; } AH = AL / b; AL = AL % b; ZF = AL == 0; SF = AL >> 7; PF = rt_parity(AL); }
void AAD(uint8_t b) { AL = (uint8_t)(AL + AH * b); AH = 0; ZF = AL == 0; SF = AL >> 7; PF = rt_parity(AL); }

void rt_cpuid(void) { EAX = 0; EBX = 0x756e6547; EDX = 0x49656e69; ECX = 0x6c65746e; }
void rt_rdtsc(void) { uint64_t t = (uint64_t)rt_ticks_us() * 100; EAX = (uint32_t)t; EDX = (uint32_t)(t >> 32); }

/* ------------------------------------------------------------------------ */
/* string instructions                                                       */
/* ------------------------------------------------------------------------ */
static inline uint32_t rdn(uint32_t a, int n) { return n == 1 ? RD8(a) : n == 2 ? RD16(a) : RD32(a); }
static inline void wrn(uint32_t a, int n, uint32_t v) { if (n == 1) WR8(a, (uint8_t)v); else if (n == 2) WR16(a, (uint16_t)v); else WR32(a, v); }

void STR_MOVS(int n, int rep)
{
    int32_t d = DF ? -n : n;
    if (rep == REP_NONE) { wrn(EDI, n, rdn(ESI, n)); ESI += d; EDI += d; return; }
    uint32_t c = ECX;
    if (!DF && c && ESI + c * n <= GUEST_SIZE && EDI + c * n <= GUEST_SIZE &&
            (EDI >= ESI + c * n || EDI + c * n <= ESI || EDI <= ESI)) {
        /* forward, non-overlapping or source ahead of destination: same result as byte-by-byte */
        if (EDI + c * n <= ESI || EDI >= ESI + c * n) memcpy(M + EDI, M + ESI, (size_t)c * n);
        else for (uint32_t k = 0; k < c * n; k++) M[EDI + k] = M[ESI + k];
        ESI += c * n; EDI += c * n; ECX = 0; return;
    }
    while (ECX) { wrn(EDI, n, rdn(ESI, n)); ESI += d; EDI += d; ECX--; }
}

void STR_STOS(int n, int rep)
{
    int32_t d = DF ? -n : n;
    uint32_t v = n == 1 ? AL : n == 2 ? AX : EAX;
    if (rep == REP_NONE) { wrn(EDI, n, v); EDI += d; return; }
    if (!DF && n == 1 && EDI + ECX <= GUEST_SIZE) { memset(M + EDI, AL, ECX); EDI += ECX; ECX = 0; return; }
    while (ECX) { wrn(EDI, n, v); EDI += d; ECX--; }
}

void STR_LODS(int n, int rep)
{
    int32_t d = DF ? -n : n;
    do {
        if (rep != REP_NONE && !ECX) break;
        uint32_t v = rdn(ESI, n);
        if (n == 1) AL = (uint8_t)v; else if (n == 2) AX = (uint16_t)v; else EAX = v;
        ESI += d;
        if (rep != REP_NONE) ECX--;
    } while (rep != REP_NONE);
}

static void cmp_n(uint32_t a, uint32_t b, int n)
{
    if (n == 1) (void)SUB8((uint8_t)a, (uint8_t)b);
    else if (n == 2) (void)SUB16((uint16_t)a, (uint16_t)b);
    else (void)SUB32(a, b);
}

void STR_CMPS(int n, int rep)
{
    int32_t d = DF ? -n : n;
    if (rep == REP_NONE) { cmp_n(rdn(ESI, n), rdn(EDI, n), n); ESI += d; EDI += d; return; }
    while (ECX) {
        cmp_n(rdn(ESI, n), rdn(EDI, n), n); ESI += d; EDI += d; ECX--;
        if (rep == REP_REPE && !ZF) break;
        if (rep == REP_REPNE && ZF) break;
    }
}

void STR_SCAS(int n, int rep)
{
    int32_t d = DF ? -n : n;
    uint32_t v = n == 1 ? AL : n == 2 ? AX : EAX;
    if (rep == REP_NONE) { cmp_n(v, rdn(EDI, n), n); EDI += d; return; }
    while (ECX) {
        cmp_n(v, rdn(EDI, n), n); EDI += d; ECX--;
        if (rep == REP_REPE && !ZF) break;
        if (rep == REP_REPNE && ZF) break;
    }
}

/* ------------------------------------------------------------------------ */
/* x87                                                                       */
/* ------------------------------------------------------------------------ */
void FNINIT(void)
{
    R.fcw = 0x37f; R.fsw = 0; R.ftop = 0;
    for (int k = 0; k < 8; k++) { R.ftag[k] = 1; R.st[k] = 0; }
}

double RDF80(uint32_t a)
{
    uint64_t m = RD64(a); uint16_t se = RD16(a + 8);
    int e = se & 0x7fff;
    if (!e && !m) return (se & 0x8000) ? -0.0 : 0.0;
    if (e == 0x7fff) return (m << 1) ? NAN : ((se & 0x8000) ? -INFINITY : INFINITY);
    double v = ldexp((double)m, e - 16383 - 63);
    return (se & 0x8000) ? -v : v;
}

void WRF80(uint32_t a, double d)
{
    uint16_t se = signbit(d) ? 0x8000 : 0; uint64_t m = 0;
    if (isnan(d)) { se |= 0x7fff; m = 0xc000000000000000ull; }
    else if (isinf(d)) { se |= 0x7fff; m = 0x8000000000000000ull; }
    else if (d != 0) {
        int e; double f = frexp(fabs(d), &e);          /* f in [0.5, 1) */
        m = (uint64_t)ldexp(f, 64); se |= (uint16_t)(e - 1 + 16383);
    }
    WR64(a, m); WR16(a + 8, se);
}

double rt_frndint(double d)
{
    switch ((R.fcw >> 10) & 3) {
    case 0: return nearbyint(d);       /* host default is round-to-nearest-even */
    case 1: return floor(d);
    case 2: return ceil(d);
    default: return trunc(d);
    }
}

int64_t rt_fround(double d)
{
    if (isnan(d) || d >= 9.2e18 || d <= -9.2e18) return INT64_MIN;
    return (int64_t)rt_frndint(d);
}

void FCOM(double a, double b)
{
    R.fsw &= ~0x4500;
    if (isnan(a) || isnan(b)) R.fsw |= 0x4500;
    else if (a < b) R.fsw |= 0x0100;
    else if (a == b) R.fsw |= 0x4000;
}

void FCOMI(double a, double b)
{
    if (isnan(a) || isnan(b)) { ZF = PF = CF = 1; }
    else { ZF = a == b; PF = 0; CF = a < b; }
    OF = SF = AF = 0;
}

void rt_fpu_misc(int op)
{
    double a = ST(0);
    switch (op) {
    case 0xe5: {                                    /* fxam */
        uint16_t c = 0;
        if (R.ftag[R.ftop]) c = 0x4100;             /* empty */
        else if (isnan(a)) c = 0x0100;
        else if (isinf(a)) c = 0x0500;
        else if (a == 0) c = 0x4000;
        else c = 0x0400;
        if (signbit(a)) c |= 0x0200;
        R.fsw = (R.fsw & ~0x4700) | c;
        return; }
    case 0xf0: ST(0) = exp2(a) - 1; return;         /* f2xm1 */
    case 0xf1: ST(1) = ST(1) * log2(a); FPOP(); return;           /* fyl2x */
    case 0xf2: ST(0) = tan(a); FPUSH(1.0); R.fsw &= ~0x400; return; /* fptan */
    case 0xf3: ST(1) = atan2(ST(1), a); FPOP(); return;           /* fpatan */
    case 0xf4: { int e; double f = frexp(a, &e); ST(0) = (double)(e - 1); FPUSH(f * 2); return; } /* fxtract */
    case 0xf5: ST(0) = remainder(a, ST(1)); R.fsw &= ~0x400; return; /* fprem1 */
    case 0xf6: R.ftop = (R.ftop - 1) & 7; return;
    case 0xf7: R.ftop = (R.ftop + 1) & 7; return;
    case 0xf8: ST(0) = fmod(a, ST(1)); R.fsw &= ~0x400; return;   /* fprem */
    case 0xf9: ST(1) = ST(1) * log2(a + 1); FPOP(); return;       /* fyl2xp1 */
    case 0xfb: { double s = sin(a), c = cos(a); ST(0) = s; FPUSH(c); R.fsw &= ~0x400; return; }
    case 0xfd: ST(0) = ldexp(a, (int)trunc(ST(1))); return;       /* fscale */
    default:
        rt_trap(EIP, "x87");
    }
}

/* ------------------------------------------------------------------------ */
/* dispatcher                                                                */
/* ------------------------------------------------------------------------ */
void rt_set_program(const rt_program *p)
{
    prog = p;
    code_lo = p->code_lo; code_hi = p->code_hi;
    direct = (rt_block_fn *)calloc(code_hi - code_lo, sizeof(rt_block_fn));
    for (uint32_t k = 0; k < p->nblocks; k++)
        if (p->addrs[k] >= code_lo && p->addrs[k] < code_hi)
            direct[p->addrs[k] - code_lo] = p->fns[k];
}

static void note_missing(uint32_t eip)
{
    static uint32_t seen[256]; static int nseen;
    for (int k = 0; k < nseen; k++) if (seen[k] == eip) return;
    if (nseen < 256) seen[nseen++] = eip;
    FILE *f = rt_open_save("missing_seeds.txt", "a");
    if (f) { fprintf(f, "%08x\n", eip); fclose(f); }
    rt_log("reached code that was not recompiled: %08x (added to missing_seeds.txt)\n", eip);
}

void rt_bad(uint32_t eip)
{
    note_missing(eip);
    rt_fatal("The game reached code at %08x that was not recompiled.\n"
             "Rebuild with the address in sierra/tools/seeds (see missing_seeds.txt).", eip);
}

void rt_trap(uint32_t eip, const char *what)
{
    rt_fatal("Unsupported instruction '%s' at %08x.", what, eip);
}

void rt_divide_error(void)
{
    rt_fatal("Divide error at %08x.", EIP);
}

void rt_int(uint8_t n, uint32_t next)
{
    if (n == 3) { rt_log("int3 at %08x ignored\n", next - 1); EIP = next; return; }
    rt_fatal("Software interrupt %02x at %08x.", n, next);
}

/* run until EIP == stop (a return address on the stack) */
static void run_until(uint32_t stop)
{
    for (;;) {
        uint32_t e = EIP;
        if (e == stop) return;
        if (e - code_lo < code_hi - code_lo) {
            rt_block_fn f = direct[e - code_lo];
            if (!f) {
                note_missing(e);
                rt_fatal("The game jumped to %08x, which was not recompiled.\n"
                         "The address was written to missing_seeds.txt; rebuild to include it.", e);
            }
            f();
            if (++async_tick >= 4096) { async_tick = 0; if (!rt_async_depth) rt_poll_async(); }
            continue;
        }
        if (e - THUNK_BASE < THUNK_SLOTS * 4) { rt_call_thunk((e - THUNK_BASE) / 4); continue; }
        if (e == RET_MAGIC) rt_fatal("Guest returned to the runtime unexpectedly.");
        rt_fatal("The game jumped to %08x, outside its code (from %08x).", e, RD32(ESP));
    }
}

uint32_t rt_call_guest(uint32_t addr, int cdecl_, int nargs, const uint32_t *args)
{
    rt_cpu save = R;
    for (int k = nargs - 1; k >= 0; k--) PUSH32(args[k]);
    uint32_t sp_before_ret = ESP;
    PUSH32(RET_MAGIC);
    EIP = addr;
    run_until(RET_MAGIC);
    uint32_t r = EAX;
    /* a callee that cleans up (stdcall) leaves ESP above the arguments; cdecl leaves them */
    (void)cdecl_; (void)sp_before_ret;
    double st[8]; uint8_t tag[8]; int top = R.ftop; uint16_t fsw = R.fsw, fcw = R.fcw;
    memcpy(st, R.st, sizeof st); memcpy(tag, R.ftag, sizeof tag);
    R = save;
    /* keep FPU results (functions may return in ST0) */
    memcpy(R.st, st, sizeof st); memcpy(R.ftag, tag, sizeof tag); R.ftop = top; R.fsw = fsw; R.fcw = fcw;
    return r;
}

void rt_run(void)
{
    run_until(RET_MAGIC);
}
