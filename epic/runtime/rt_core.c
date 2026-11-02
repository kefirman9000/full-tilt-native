/* rt_core.c - CPU state, dispatcher, interrupts, PIC/PIT, keyboard controller, string instructions. */
#include "rt_internal.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

rt_cpu R;
uint8_t *M;
rt_task rt_tasks[4];
int rt_ntasks;
int rt_exit_code;
int rt_quit;

const uint8_t rt_parity[256] = {
#define P2(n) n, n ^ 1, n ^ 1, n
#define P4(n) P2(n), P2(n ^ 1), P2(n ^ 1), P2(n)
#define P6(n) P4(n), P4(n ^ 1), P4(n ^ 1), P4(n)
    P6(1), P6(0), P6(0), P6(1)
#undef P2
#undef P4
#undef P6
};

/* ------------------------------------------------------------------------ */
void rt_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

/* recent block history, printed on fatal errors */
#define HIST 32
static uint16_t hist_cs[HIST], hist_ip[HIST];
static unsigned hist_n;

void rt_fatal(const char *fmt, ...)
{
    va_list ap;
    unsigned k;
    fprintf(stderr, "recent blocks (oldest first):");
    for (k = hist_n >= HIST ? hist_n - HIST : 0; k < hist_n; k++)
        fprintf(stderr, " %04X:%04X", hist_cs[k % HIST], hist_ip[k % HIST]);
    fprintf(stderr, "\n");
    va_start(ap, fmt);
    fprintf(stderr, "EpicPinball: fatal: ");
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n  CS:IP=%04X:%04X AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X BP=%04X SP=%04X DS=%04X ES=%04X SS=%04X\n",
            CS, IP, AX, BX, CX, DX, SI, DI, BP, SP, DS, ES, SS);
    fflush(stderr);
    exit(2);
}

void rt_divide_error(void)
{
    rt_fatal("divide error");
}

/* ------------------------------------------------------------------------ */
/* PIC (8259) - just enough for IRQ0 (timer) and IRQ1 (keyboard)             */
/* ------------------------------------------------------------------------ */
static uint8_t pic_irr, pic_isr, pic_imr = 0xB8, pic_cmd_read_isr;

void rt_raise_irq(int irq)
{
    pic_irr |= (uint8_t)(1 << irq);
}

static int pic_pending(void)
{
    uint8_t req = (uint8_t)(pic_irr & ~pic_imr);
    int i;
    if (!req)
        return -1;
    for (i = 0; i < 8; i++)
    {
        if (pic_isr & (1 << i))
            return -1;      /* a higher (or same) priority IRQ is still in service */
        if (req & (1 << i))
            return i;
    }
    return -1;
}

static void pic_eoi(void)
{
    int i;
    for (i = 0; i < 8; i++)
        if (pic_isr & (1 << i))
        {
            pic_isr &= (uint8_t)~(1 << i);
            return;
        }
}

/* ------------------------------------------------------------------------ */
/* keyboard controller                                                       */
/* ------------------------------------------------------------------------ */
static uint8_t kbd_queue[64];
static int kbd_head, kbd_tail;
static uint8_t kbd_data;        /* value on port 60h */
static int kbd_full;            /* output buffer full (IRQ raised, not yet read) */

void rt_kbd_push(uint8_t sc)
{
    int n = (kbd_tail + 1) & 63;
    if (n == kbd_head)
        return;
    kbd_queue[kbd_tail] = sc;
    kbd_tail = n;
}

static void kbd_feed(void)
{
    /* hand the next scancode to the CPU once the previous IRQ1 has been serviced */
    if (kbd_head == kbd_tail || (pic_isr & 2) || (pic_irr & 2))
        return;
    kbd_data = kbd_queue[kbd_head];
    kbd_head = (kbd_head + 1) & 63;
    kbd_full = 1;
    rt_raise_irq(1);
}

/* BIOS keyboard buffer (filled by the built-in INT 9 handler) */
static uint16_t bios_keys[32];
static int bk_head, bk_tail;

static void bios_key_put(uint16_t k)
{
    int n = (bk_tail + 1) & 31;
    if (n != bk_head)
    {
        bios_keys[bk_tail] = k;
        bk_tail = n;
    }
}

int rt_kbd_bios_get(uint16_t *key, int remove)
{
    if (bk_head == bk_tail)
        return 0;
    *key = bios_keys[bk_head];
    if (remove)
        bk_head = (bk_head + 1) & 31;
    return 1;
}

/* XT set-1 make code -> ASCII (unshifted / shifted) for the BIOS buffer */
static const char sc_lo[0x3A] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 8, 9,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 13, 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '};
static const char sc_hi[0x3A] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 8, 9,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 13, 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '};
static int shift_down, e0_prefix;

void rt_bios_int9(void)
{
    uint8_t sc = rt_in8(0x60);
    if (sc == 0xE0)
    {
        e0_prefix = 1;
    }
    else
    {
        int brk = sc & 0x80, code = sc & 0x7F;
        if (code == 0x2A || code == 0x36)
            shift_down = !brk;
        else if (!brk)
        {
            uint8_t ascii = 0;
            if (!e0_prefix && code < 0x3A)
                ascii = (uint8_t)(shift_down ? sc_hi[code] : sc_lo[code]);
            else if (e0_prefix && code == 0x1C)
                ascii = 13;
            bios_key_put((uint16_t)((code << 8) | ascii));
        }
        e0_prefix = 0;
    }
    M[0x41A] = 0x1E; /* keep BDA head/tail plausible for programs that peek */
    pic_eoi();
}

/* ------------------------------------------------------------------------ */
/* PIT (8253) channel 0 and the time base                                    */
/* ------------------------------------------------------------------------ */
static uint16_t pit_div[3] = {0, 0, 0};
static int pit_lohi[3], pit_flip[3];
static uint64_t next_tick_us, tick_period_us = 54925; /* 18.2 Hz */
static uint64_t t0_us;

double rt_pit_hz(void)
{
    return 1e6 / (double)tick_period_us;
}

static void pit_write(int ch, uint8_t v)
{
    if (pit_lohi[ch] == 3)
    {
        if (!pit_flip[ch])
            pit_div[ch] = (uint16_t)((pit_div[ch] & 0xFF00) | v);
        else
            pit_div[ch] = (uint16_t)((pit_div[ch] & 0x00FF) | (v << 8));
        pit_flip[ch] ^= 1;
        if (pit_flip[ch])
            return;
    }
    else if (pit_lohi[ch] == 1)
        pit_div[ch] = (uint16_t)((pit_div[ch] & 0xFF00) | v);
    else
        pit_div[ch] = (uint16_t)((pit_div[ch] & 0x00FF) | (v << 8));
    if (ch == 0)
    {
        uint32_t d = pit_div[0] ? pit_div[0] : 65536;
        tick_period_us = (uint64_t)((double)d * 1e6 / 1193182.0);
        if (tick_period_us < 200)
            tick_period_us = 200;
        next_tick_us = rt_now_us() + tick_period_us;
    }
}

static int was_retrace;
void rt_timer_poll(void)
{
    uint64_t now = rt_now_us();
    int r;
    if (now >= next_tick_us)
    {
        rt_raise_irq(0);
        next_tick_us += tick_period_us;
        if (now > next_tick_us + 4 * tick_period_us) /* do not try to catch up after a stall */
            next_tick_us = now + tick_period_us;
    }
    r = rt_vga_retrace();
    if (r && !was_retrace)
    {
        rt_pump_events();
        rt_present();
    }
    was_retrace = r;
    kbd_feed();
}

/* ------------------------------------------------------------------------ */
/* I/O ports                                                                 */
/* ------------------------------------------------------------------------ */
static int idle_reads;

uint8_t rt_in8(uint16_t port)
{
    switch (port)
    {
    case 0x20: return pic_cmd_read_isr ? pic_isr : pic_irr;
    case 0x21: return pic_imr;
    case 0x40: case 0x41: case 0x42:
    {
        /* free-running counter readback derived from host time */
        uint32_t d = pit_div[port - 0x40] ? pit_div[port - 0x40] : 65536;
        uint32_t c = (uint32_t)(d - ((rt_now_us() * 1193182ull / 1000000ull) % d));
        int ch = port - 0x40;
        uint8_t v = (pit_lohi[ch] == 3 && pit_flip[ch]) ? (uint8_t)(c >> 8) : (uint8_t)c;
        if (pit_lohi[ch] == 3) pit_flip[ch] ^= 1;
        return v;
    }
    case 0x60:
        kbd_full = 0;
        return kbd_data;
    case 0x61: return 0x20;
    case 0x64: return (uint8_t)(0x14 | (kbd_full ? 1 : 0));
    case 0x201: return 0xFF;           /* no joystick */
    case 0x3DA:
        if (++idle_reads > 2000)
        {
            rt_timer_poll();
            rt_idle();
            idle_reads = 0;
        }
        return rt_vga_in(port);
    default:
        if (port >= 0x3C0 && port <= 0x3DF)
            return rt_vga_in(port);
        return 0xFF;                   /* nothing there (sound cards etc.) */
    }
}

void rt_out8(uint16_t port, uint8_t v)
{
    switch (port)
    {
    case 0x20:
        if ((v & 0xE0) == 0x20) pic_eoi();              /* (specific) EOI */
        else if (v == 0x0A) pic_cmd_read_isr = 0;       /* OCW3: read IRR */
        else if (v == 0x0B) pic_cmd_read_isr = 1;       /* OCW3: read ISR */
        return;
    case 0x21: pic_imr = v; return;
    case 0x40: case 0x41: case 0x42: pit_write(port - 0x40, v); return;
    case 0x43:
    {
        int ch = v >> 6;
        if (ch == 3) return;
        if (((v >> 4) & 3) == 0) return;                /* latch command */
        pit_lohi[ch] = (v >> 4) & 3;
        pit_flip[ch] = 0;
        return;
    }
    case 0x61: return;
    default:
        if (port >= 0x3C0 && port <= 0x3DF)
            rt_vga_out(port, v);
        return;
    }
}

/* ------------------------------------------------------------------------ */
/* string instructions                                                       */
/* ------------------------------------------------------------------------ */
void rt_str(int op, int size, uint16_t srcseg, int rep)
{
    int step = DF ? -size : size;
    for (;;)
    {
        if (rep && CX == 0)
            break;
        switch (op)
        {
        case RT_MOVS:
            if (size == 1) WR8(ES, DI, RD8(srcseg, SI));
            else if (size == 2) WR16(ES, DI, RD16(srcseg, SI));
            else WR32(ES, DI, RD32(srcseg, SI));
            SI = (uint16_t)(SI + step); DI = (uint16_t)(DI + step);
            break;
        case RT_STOS:
            if (size == 1) WR8(ES, DI, AL);
            else if (size == 2) WR16(ES, DI, AX);
            else WR32(ES, DI, EAX);
            DI = (uint16_t)(DI + step);
            break;
        case RT_LODS:
            if (size == 1) AL = RD8(srcseg, SI);
            else if (size == 2) AX = RD16(srcseg, SI);
            else EAX = RD32(srcseg, SI);
            SI = (uint16_t)(SI + step);
            break;
        case RT_CMPS:
            if (size == 1) (void)SUB8(RD8(srcseg, SI), RD8(ES, DI));
            else (void)SUB16(RD16(srcseg, SI), RD16(ES, DI));
            SI = (uint16_t)(SI + step); DI = (uint16_t)(DI + step);
            break;
        case RT_SCAS:
            if (size == 1) (void)SUB8(AL, RD8(ES, DI));
            else (void)SUB16(AX, RD16(ES, DI));
            DI = (uint16_t)(DI + step);
            break;
        }
        if (!rep)
            break;
        CX = (uint16_t)(CX - 1);
        if (rep == REPE && !ZF) break;
        if (rep == REPNE && ZF) break;
    }
}

/* ------------------------------------------------------------------------ */
/* interrupts                                                                */
/* ------------------------------------------------------------------------ */
void rt_int(uint8_t n, uint16_t next_ip)
{
    PUSH16(GETFLAGS());
    PUSH16(CS);
    PUSH16(next_ip);
    IFL = 0;
    R.tf = 0;
    IP = RD16(0, (uint16_t)(n * 4));
    CS = RD16(0, (uint16_t)(n * 4 + 2));
}

void rt_hlt(void)
{
    while (!rt_quit && pic_pending() < 0)
    {
        rt_timer_poll();
        rt_idle();
    }
}

void rt_sti(void) {}

void rt_bad(uint16_t cs, uint16_t ip)
{
    CS = cs; IP = ip;
    rt_fatal("reached code that was not recompiled (add it to the seeds file)");
}

static void deliver_irq(int irq)
{
    pic_irr &= (uint8_t)~(1 << irq);
    pic_isr |= (uint8_t)(1 << irq);
    rt_int((uint8_t)(8 + irq), IP);
}

/* ------------------------------------------------------------------------ */
/* dispatcher                                                                */
/* ------------------------------------------------------------------------ */
static rt_task *task_for_cs(uint16_t cs, uint16_t *rel)
{
    int i;
    for (i = rt_ntasks - 1; i >= 0; i--)
    {
        rt_task *t = &rt_tasks[i];
        uint32_t lo = t->load, hi = (uint32_t)t->psp + t->paras;
        if (cs >= lo && cs < hi)
        {
            *rel = (uint16_t)(cs - t->load);
            return t;
        }
    }
    return 0;
}

static rt_module modules[8];
static int nmodules;

void rt_module_loaded(const rt_program *prog, uint32_t base)
{
    int i;
    if (base & 15)
        rt_log("warning: %s loaded at unaligned address %05X", prog->name, base);
    for (i = 0; i < nmodules; i++)
        if (modules[i].prog == prog || modules[i].base == base)
            break;
    if (i == nmodules && nmodules < 8) nmodules++;
    modules[i].prog = prog;
    modules[i].base = base;
    *prog->load_seg = (uint16_t)(base >> 4);
    rt_log("module: %s at %04X:0000", prog->name, base >> 4);
}

static rt_block_fn module_lookup(void)
{
    uint32_t lin = ((uint32_t)CS << 4) + IP;
    int i;
    for (i = nmodules - 1; i >= 0; i--)
        if (lin >= modules[i].base && lin < modules[i].base + modules[i].prog->image_size)
        {
            rt_block_fn fn;
            if (((uint32_t)CS << 4) != modules[i].base)
                rt_fatal("call into %s with an unexpected segment", modules[i].prog->name);
            fn = modules[i].prog->lookup(0, IP);
            if (!fn)
            {
                FILE *mf = fopen("missing_seeds.txt", "a");
                if (mf) { fprintf(mf, "%s 0000:%04X\n", modules[i].prog->name, IP); fclose(mf); }
                rt_fatal("no recompiled code at %s 0000:%04X; add it to the seeds file", modules[i].prog->name, IP);
            }
            return fn;
        }
    return 0;
}

static void driver_stub(void)
{
    /* Runtime-loaded sound/music driver entry point. "No sound card" setup: report success and return. */
    AX = 0;
    CF = 0;
    IP = POP16();
    CS = POP16();
}

void rt_run(void)
{
    unsigned counter = 0;
    while (!rt_quit && rt_ntasks > 0)
    {
        rt_block_fn fn;
        uint16_t rel;
        rt_task *t;
        if ((++counter & 63) == 0)
            rt_timer_poll();
        if (IFL && pic_irr)
        {
            int irq = pic_pending();
            if (irq >= 0)
                deliver_irq(irq);
        }
        if (CS == HLE_SEG)
        {
            rt_hle((uint16_t)(IP - HLE_BASE));
            continue;
        }
        t = task_for_cs(CS, &rel);
        if (!t || (nmodules && !t->prog->lookup(rel, IP)))
        {
            /* driver modules live inside a program's memory: check them before giving up */
            rt_block_fn mfn = nmodules ? module_lookup() : 0;
            if (mfn) { hist_cs[hist_n % HIST] = CS; hist_ip[hist_n % HIST] = IP; hist_n++; mfn(); continue; }
            if (!t)
                rt_fatal("jumped outside any loaded program");
        }
        fn = t->prog->lookup(rel, IP);
        if (!fn)
        {
            const uint16_t *e;
            for (e = t->prog->extsegs; *e != 0xFFFF; e++)
                if (*e == rel)
                    break;
            if (*e == rel)
            {
                driver_stub();
                continue;
            }
            {
                FILE *mf = fopen("missing_seeds.txt", "a");
                if (mf) { fprintf(mf, "%s %04X:%04X\n", t->prog->name, rel, IP); fclose(mf); }
            }
            rt_fatal("no recompiled code at %s %04X:%04X (image-relative); add it to the seeds file",
                     t->prog->name, rel, IP);
        }
        hist_cs[hist_n % HIST] = CS; hist_ip[hist_n % HIST] = IP; hist_n++;
        fn();
    }
}

void rt_core_init(void)
{
    int i;
    M = (uint8_t *)calloc(1, 0x110000 + 16);
    if (!M)
        rt_fatal("out of memory");
    memset(&R, 0, sizeof R);
    /* every interrupt vector points at the built-in service stub F000:0100+n */
    for (i = 0; i < 256; i++)
    {
        M[i * 4 + 0] = (uint8_t)((HLE_BASE + i) & 0xFF);
        M[i * 4 + 1] = (uint8_t)((HLE_BASE + i) >> 8);
        M[i * 4 + 2] = (uint8_t)(HLE_SEG & 0xFF);
        M[i * 4 + 3] = (uint8_t)(HLE_SEG >> 8);
    }
    /* BIOS data area basics */
    M[0x410] = 0x21;         /* equipment: colour 80x25 */
    M[0x413] = 0x80; M[0x414] = 0x02;  /* 640 KB */
    M[0x449] = 3;            /* video mode */
    M[0x44A] = 80;
    M[0x484] = 24;
    t0_us = rt_now_us();
    next_tick_us = t0_us + tick_period_us;
    (void)t0_us;
}
