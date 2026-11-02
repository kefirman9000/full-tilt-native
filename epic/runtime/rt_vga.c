/* rt_vga.c - VGA registers, planar video memory and the renderer, including the full-table view.
 *
 * Epic Pinball runs in "Mode X" (unchained 256 colours) with a tweaked 320x240 CRTC: every table row is
 * scan-doubled to 480 lines. Each of the two video pages holds the WHOLE 320x400 table (pages at 0640h and
 * 8340h, 7D00h bytes = 400 rows of 80 bytes), and the 20-row score panel lives at address 0 and is shown below
 * the table through the CRTC line-compare split. Scrolling only moves the CRTC start address inside a page.
 *
 * So the full-table view needs no game change at all: it draws all 400 rows of the page being displayed, plus
 * the score panel underneath, with square pixels. The original view renders exactly what the CRTC describes.
 */
#include "rt_internal.h"
#include <string.h>

static uint8_t vram[4][65536];
static uint8_t latch[4];
static uint8_t seq[8], gc[9], crtc[25], attr[21];
static uint8_t seq_i, gc_i, crtc_i, attr_i, attr_ff, misc = 0x63;
static uint8_t dac[256][3], dac_wi, dac_wc, dac_ri, dac_rc, dac_mask = 0xFF;
static int mode = 3;
int rt_view = RT_VIEW_FULL;

extern const uint8_t rt_font8x8[256][8];

/* ------------------------------------------------------------------------ */
/* retrace timing: 70 Hz like a real VGA in these modes                     */
/* ------------------------------------------------------------------------ */
#define FRAME_US 14286u
#define VRETRACE_US 1100u
int rt_vga_retrace(void)
{
    return (rt_now_us() % FRAME_US) >= FRAME_US - VRETRACE_US;
}

/* ------------------------------------------------------------------------ */
/* memory                                                                    */
/* ------------------------------------------------------------------------ */
static int chain4(void) { return (seq[4] & 0x08) != 0; }

uint8_t rt_mmio_rd(uint32_t a)
{
    uint16_t off = (uint16_t)(a - 0xA0000u);
    int p;
    if (chain4())
    {
        uint16_t o = (uint16_t)(off & ~3u);
        for (p = 0; p < 4; p++) latch[p] = vram[p][o];
        return vram[off & 3][o];
    }
    for (p = 0; p < 4; p++) latch[p] = vram[p][off];
    if (gc[5] & 0x08)
    {
        /* read mode 1: colour compare */
        uint8_t r = 0xFF;
        for (p = 0; p < 4; p++)
            if (gc[7] & (1 << p))
                r &= (uint8_t)((gc[2] & (1 << p)) ? latch[p] : ~latch[p]);
        return r;
    }
    return vram[gc[4] & 3][off];
}

static uint8_t logic(uint8_t d, uint8_t l)
{
    switch ((gc[3] >> 3) & 3)
    {
    case 1: return (uint8_t)(d & l);
    case 2: return (uint8_t)(d | l);
    case 3: return (uint8_t)(d ^ l);
    default: return d;
    }
}

void rt_mmio_wr(uint32_t a, uint8_t v)
{
    uint16_t off = (uint16_t)(a - 0xA0000u);
    uint8_t mask = seq[2] & 0x0F, bm = gc[8], rot;
    int p, wm = gc[5] & 3;
    if (chain4())
    {
        p = off & 3;
        if (mask & (1 << p))
            vram[p][off & ~3u] = v;
        return;
    }
    rot = (uint8_t)(gc[3] & 7);
    if (rot) v = (uint8_t)((v >> rot) | (v << (8 - rot)));
    for (p = 0; p < 4; p++)
    {
        uint8_t d;
        if (!(mask & (1 << p))) continue;
        switch (wm)
        {
        case 0:
            d = (gc[1] & (1 << p)) ? ((gc[0] & (1 << p)) ? 0xFF : 0x00) : v;
            d = logic(d, latch[p]);
            vram[p][off] = (uint8_t)((d & bm) | (latch[p] & ~bm));
            break;
        case 1:
            vram[p][off] = latch[p];
            break;
        case 2:
            d = (v & (1 << p)) ? 0xFF : 0x00;
            d = logic(d, latch[p]);
            vram[p][off] = (uint8_t)((d & bm) | (latch[p] & ~bm));
            break;
        case 3:
        {
            uint8_t m = (uint8_t)(v & bm);
            d = (gc[0] & (1 << p)) ? 0xFF : 0x00;
            vram[p][off] = (uint8_t)((d & m) | (latch[p] & ~m));
            break;
        }
        }
    }
}

/* ------------------------------------------------------------------------ */
/* ports                                                                     */
/* ------------------------------------------------------------------------ */
uint8_t rt_vga_in(uint16_t port)
{
    switch (port)
    {
    case 0x3C0: return attr_i;
    case 0x3C1: return attr_i < 21 ? attr[attr_i] : 0;
    case 0x3C4: return seq_i;
    case 0x3C5: return seq_i < 8 ? seq[seq_i] : 0;
    case 0x3C6: return dac_mask;
    case 0x3C7: return 3;
    case 0x3C8: return dac_wi;
    case 0x3C9:
    {
        uint8_t v = dac[dac_ri][dac_rc];
        if (++dac_rc == 3) { dac_rc = 0; dac_ri++; }
        return v;
    }
    case 0x3CC: return misc;
    case 0x3CE: return gc_i;
    case 0x3CF: return gc_i < 9 ? gc[gc_i] : 0;
    case 0x3D4: return crtc_i;
    case 0x3D5: return crtc_i < 25 ? crtc[crtc_i] : 0;
    case 0x3DA:
    {
        static uint8_t h;
        int vr = rt_vga_retrace();
        attr_ff = 0;
        h ^= 1;                                  /* horizontal blank toggles between reads */
        return (uint8_t)((vr ? 0x09 : 0) | h);
    }
    default: return 0xFF;
    }
}

void rt_vga_out(uint16_t port, uint8_t v)
{
    switch (port)
    {
    case 0x3C0:
        if (!attr_ff) attr_i = v & 0x1F;
        else if (attr_i < 21) attr[attr_i] = v;
        attr_ff ^= 1;
        break;
    case 0x3C2: misc = v; break;
    case 0x3C4: seq_i = v & 7; break;
    case 0x3C5: seq[seq_i] = v; break;
    case 0x3C6: dac_mask = v; break;
    case 0x3C7: dac_ri = v; dac_rc = 0; break;
    case 0x3C8: dac_wi = v; dac_wc = 0; break;
    case 0x3C9:
        dac[dac_wi][dac_wc] = v & 0x3F;
        if (++dac_wc == 3) { dac_wc = 0; dac_wi++; }
        break;
    case 0x3CE: gc_i = v & 0x0F; break;
    case 0x3CF: if (gc_i < 9) gc[gc_i] = v; break;
    case 0x3D4: crtc_i = v & 0x1F; break;
    case 0x3D5:
        if (crtc_i < 25)
        {
            if (crtc_i < 8 && (crtc[0x11] & 0x80) && crtc_i != 7) break;  /* write-protected */
            if (crtc_i == 7 && (crtc[0x11] & 0x80)) { crtc[7] = (uint8_t)((crtc[7] & ~0x10) | (v & 0x10)); break; }
            crtc[crtc_i] = v;
        }
        break;
    default: break;
    }
}

/* ------------------------------------------------------------------------ */
/* modes                                                                     */
/* ------------------------------------------------------------------------ */
static const uint8_t crtc_text[25] = {0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F, 0x00, 0x4F, 0x0D, 0x0E, 0x00,
                                      0x00, 0x00, 0x00, 0x9C, 0x8E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF};
static const uint8_t crtc_13h[25] = {0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F, 0x00, 0x41, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x9C, 0x8E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF};
static const uint8_t ega16[16][3] = {{0, 0, 0}, {0, 0, 42}, {0, 42, 0}, {0, 42, 42}, {42, 0, 0}, {42, 0, 42},
    {42, 21, 0}, {42, 42, 42}, {21, 21, 21}, {21, 21, 63}, {21, 63, 21}, {21, 63, 63}, {63, 21, 21},
    {63, 21, 63}, {63, 63, 21}, {63, 63, 63}};

int rt_vga_mode(void) { return mode; }

void rt_vga_set_mode(int m)
{
    int i;
    if (m < 0) return;
    mode = m;
    M[0x449] = (uint8_t)m;
    memset(seq, 0, sizeof seq);
    memset(gc, 0, sizeof gc);
    seq[2] = 0x0F;
    gc[5] = 0x00; gc[6] = 0x05; gc[8] = 0xFF; gc[7] = 0x0F;
    for (i = 0; i < 16; i++) attr[i] = (uint8_t)i;
    if (m == 0x13)
    {
        memcpy(crtc, crtc_13h, sizeof crtc);
        seq[4] = 0x0E;
        gc[5] = 0x40;
        memset(vram, 0, sizeof vram);
        /* default 256 colour palette: EGA colours then a grey ramp is enough; games load their own */
        for (i = 0; i < 256; i++)
        {
            if (i < 16) memcpy(dac[i], ega16[i], 3);
            else dac[i][0] = dac[i][1] = dac[i][2] = (uint8_t)((i & 63));
        }
    }
    else
    {
        mode = 3;
        memcpy(crtc, crtc_text, sizeof crtc);
        seq[4] = 0x02;
        gc[5] = 0x10; gc[6] = 0x0E;
        for (i = 0; i < 16; i++) memcpy(dac[i], ega16[i], 3);
        for (i = 0; i < 80 * 25; i++) { M[0xB8000 + i * 2] = ' '; M[0xB8000 + i * 2 + 1] = 0x07; }
    }
}

void rt_vga_init(void)
{
    rt_vga_set_mode(3);
}

/* ------------------------------------------------------------------------ */
/* rendering                                                                 */
/* ------------------------------------------------------------------------ */
static uint32_t rgb(uint8_t i)
{
    i &= dac_mask;
    return 0xFF000000u | ((uint32_t)(dac[i][0] * 255 / 63) << 16) | ((uint32_t)(dac[i][1] * 255 / 63) << 8) |
           (uint32_t)(dac[i][2] * 255 / 63);
}

static void render_text(uint32_t *out, int *w, int *h)
{
    int r, c, y, x;
    *w = 640; *h = 400;
    for (r = 0; r < 25; r++)
        for (c = 0; c < 80; c++)
        {
            uint8_t ch = M[0xB8000 + (r * 80 + c) * 2], at = M[0xB8000 + (r * 80 + c) * 2 + 1];
            uint32_t fg = rgb(attr[at & 15]), bg = rgb(attr[(at >> 4) & 7]);
            for (y = 0; y < 16; y++)
            {
                uint8_t bits = rt_font8x8[ch][y / 2];
                /* line-drawing characters reach the cell edge; stretch them over the doubled rows */
                for (x = 0; x < 8; x++)
                    out[(r * 16 + y) * 640 + c * 8 + x] = (bits >> x) & 1 ? fg : bg;
            }
        }
}

static int vde(void) { return crtc[0x12] | ((crtc[7] & 0x02) << 7) | ((crtc[7] & 0x40) << 3); }
static int line_compare(void) { return crtc[0x18] | ((crtc[7] & 0x10) << 4) | ((crtc[9] & 0x40) << 3); }

/* one row of 256-colour pixels starting at CRTC address addr */
static void draw_row(uint32_t *dst, uint16_t addr, int width, int dword, const uint32_t *pal)
{
    int x;
    for (x = 0; x < width; x++)
    {
        uint16_t unit = (uint16_t)(addr + (x >> 2));
        uint16_t o = dword ? (uint16_t)(unit << 2) : unit;
        dst[x] = pal[vram[x & 3][o]];
    }
}

/* Epic Pinball's in-game layout (see the comment at the top) */
#define EP_PAGE0 0x0640u
#define EP_PAGE1 0x8340u
#define EP_PAGE_BYTES 0x7D00u
#define EP_ROWS 400
#define EP_PANEL_ROWS 20

static int is_epic_table_screen(int lines, int spr, int stride, int dword)
{
    uint16_t start = (uint16_t)((crtc[0x0C] << 8) | crtc[0x0D]);
    int lc = line_compare();
    if (dword || stride != 80 || lines != 480 || spr != 2) return 0;
    if (lc < 400 || lc > 480) return 0;
    /* the start address must lie inside one of the two pages, within the scroll range */
    if (start >= EP_PAGE0 && start <= EP_PAGE0 + (EP_ROWS - 200) * 80) return 1;
    if (start >= EP_PAGE1 && start <= EP_PAGE1 + (EP_ROWS - 200) * 80) return 1;
    return 0;
}

int rt_vga_render(uint32_t *out, int maxw, int maxh, int *w, int *h, double *aspect)
{
    uint32_t pal[256];
    int i, lines, spr, stride, width, dword, lc, y;
    uint16_t start;
    if (mode == 3)
    {
        render_text(out, w, h);
        *aspect = 4.0 / 3.0;
        return 1;
    }
    for (i = 0; i < 256; i++) pal[i] = rgb((uint8_t)i);
    lines = vde() + 1;
    spr = ((crtc[9] & 0x1F) + 1) * ((crtc[9] & 0x80) ? 2 : 1);
    stride = crtc[0x13] * 2;
    width = (crtc[1] + 1) * 4;
    dword = (crtc[0x14] & 0x40) != 0;
    if (width > maxw) width = maxw;
    if (lines > maxh) lines = maxh;
    start = (uint16_t)((crtc[0x0C] << 8) | crtc[0x0D]);
    lc = line_compare();

    if (rt_view == RT_VIEW_FULL && is_epic_table_screen(lines, spr, stride, dword))
    {
        /* whole table of the displayed page, then the score panel from address 0, square pixels */
        uint16_t page = start >= EP_PAGE1 ? EP_PAGE1 : EP_PAGE0;
        int panel = (lines - 1 - lc) / spr;      /* rows of score panel visible right now (it slides) */
        if (panel < 0) panel = 0;
        if (panel > EP_PANEL_ROWS) panel = EP_PANEL_ROWS;
        for (y = 0; y < EP_ROWS; y++)
            draw_row(out + y * width, (uint16_t)(page + y * 80), width, 0, pal);
        for (y = 0; y < EP_PANEL_ROWS; y++)
        {
            if (y < panel)
                draw_row(out + (EP_ROWS + y) * width, (uint16_t)(y * 80), width, 0, pal);
            else
                memset(out + (EP_ROWS + y) * width, 0, (size_t)width * 4);
        }
        *w = width;
        *h = EP_ROWS + EP_PANEL_ROWS;
        *aspect = (double)width / (EP_ROWS + EP_PANEL_ROWS);
        return 1;
    }

    /* original view: exactly what the CRTC shows, one output line per scanline */
    {
        uint16_t row_addr = start;
        int sub = 0;
        for (y = 0; y < lines; y++)
        {
            draw_row(out + y * width, row_addr, width, dword, pal);
            if (y == lc)
            {
                row_addr = 0;
                sub = 0;
                continue;
            }
            if (++sub >= spr)
            {
                sub = 0;
                row_addr = (uint16_t)(row_addr + stride);
            }
        }
        *w = width;
        *h = lines;
        *aspect = 4.0 / 3.0;
    }
    return 1;
}
