/*
 * rt_main.c - SDL2 front end for the recompiled 3-D Ultra Pinball: start-up, the window the game draws into,
 * keyboard and mouse as Win32 messages, timing, error boxes and test automation.
 *
 *   UltraPinball [--game DIR] [--save DIR] [--script FILE] [--headless]
 *
 * --game     folder with the original game files (PINBALL.EXE, RESOURCE.MAP, RESOURCE.00n; default: current folder)
 * --save     where settings and high scores are written (default: <game>/save)
 * --script   test automation, one event per line, times in ms from start-up:
 *              "<ms> key <SDL key name>"           press and release (80 ms)
 *              "<ms> hold <SDL key name> <ms>"     press, release after the given time
 *              "<ms> down|up <SDL key name>"       press or release only
 *              "<ms> click <x> <y>"                left click at game coordinates
 *              "<ms> shot <file.bmp>"              save what the game shows
 *              "<ms> quit"
 * --headless no window (with --script, for automated tests)
 *
 * Keys: Alt+Enter fullscreen, F12 closes the game (back to the table library). Everything else goes to the game.
 */
#include "rt_internal.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifndef SIERRA_PROGRAM
#define SIERRA_PROGRAM upb_program
#endif
extern const rt_program SIERRA_PROGRAM;
extern void rt_mem_init(void);
extern void rt_win32_init(void);
extern void rt_present_if_dirty(int force);

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static int tex_w, tex_h;
static uint32_t *frame; static int frame_w, frame_h;   /* last picture the game showed */
static int game_w = 640, game_h = 480;                 /* the game's window (client) size */
static int headless;
static uint64_t perf_freq, perf_t0;
static uint8_t keys[256];                              /* by virtual-key code */

/* ------------------------------------------------------------------------ */
/* time, logging, errors                                                     */
/* ------------------------------------------------------------------------ */
uint64_t rt_ticks_us(void) { return (SDL_GetPerformanceCounter() - perf_t0) * 1000000ull / perf_freq; }
uint32_t rt_ticks_ms(void) { return (uint32_t)(rt_ticks_us() / 1000) + 10000; }   /* never 0 near start-up */

void rt_exit(int code)
{
    rt_log("exit %d\n", code);
    SDL_Quit();
    exit(code);
}

void rt_fatal(const char *fmt, ...)
{
    char msg[2048];
    va_list ap;
    va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    rt_log("FATAL: %s\n  eip %08x esp %08x eax %08x ebx %08x ecx %08x edx %08x esi %08x edi %08x ebp %08x\n",
           msg, EIP, ESP, EAX, EBX, ECX, EDX, ESI, EDI, EBP);
    fprintf(stderr, "3-D Ultra Pinball: %s\n", msg);
    if (!headless) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "3-D Ultra Pinball", msg, win);
    SDL_Quit();
    exit(1);
}

void rt_message_box(const char *title, const char *text)
{
    fprintf(stderr, "%s: %s\n", title, text);
    if (!headless) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title && *title ? title : "3-D Ultra Pinball", text, win);
}

void rt_show_cursor(int on) { if (!headless) SDL_ShowCursor(on ? SDL_ENABLE : SDL_DISABLE); }

/* ------------------------------------------------------------------------ */
/* presentation: the game's 8-bit window, converted to RGB by rt_win32.c     */
/* ------------------------------------------------------------------------ */
static SDL_Rect dest_rect(void)
{
    int ow = game_w, oh = game_h;
    SDL_Rect d = { 0, 0, game_w, game_h };
    if (!ren) return d;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    double a = (double)game_w / game_h;
    if ((double)ow / oh > a) { d.h = oh; d.w = (int)(oh * a + 0.5); } else { d.w = ow; d.h = (int)(ow / a + 0.5); }
    d.x = (ow - d.w) / 2; d.y = (oh - d.h) / 2;
    return d;
}

void rt_video_init(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (w == game_w && h == game_h && (headless || win)) return;
    game_w = w; game_h = h;
    rt_log("video %dx%d\n", w, h);
    if (headless || !win) return;
    SDL_RenderSetLogicalSize(ren, 0, 0);
    if (!(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP)) {
        /* as big as fits, keeping whole multiples where possible */
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(win), &dm) != 0) { dm.w = 1280; dm.h = 720; }
        int s = 1;
        while ((s + 1) * w <= dm.w * 9 / 10 && (s + 1) * h <= dm.h * 85 / 100) s++;
        SDL_SetWindowSize(win, w * s, h * s);
        SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}

void rt_video_frame(const uint32_t *rgb, int w, int h)
{
    if (frame_w != w || frame_h != h) { free(frame); frame = (uint32_t *)malloc((size_t)w * h * 4); frame_w = w; frame_h = h; }
    memcpy(frame, rgb, (size_t)w * h * 4);
    if (headless || !ren) return;
    if (!tex || tex_w != w || tex_h != h) {
        if (tex) SDL_DestroyTexture(tex);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        tex_w = w; tex_h = h;
    }
    SDL_UpdateTexture(tex, NULL, rgb, w * 4);
    SDL_Rect d = dest_rect();
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, &d);
    SDL_RenderPresent(ren);
}

void rt_video_present(const uint8_t *pix8, int w, int h, int pitch, const uint32_t *pal, int dx, int dy, int dw, int dh)
{
    (void)pix8; (void)w; (void)h; (void)pitch; (void)pal; (void)dx; (void)dy; (void)dw; (void)dh;
}

/* ------------------------------------------------------------------------ */
/* input                                                                     */
/* ------------------------------------------------------------------------ */
static int vk_of(SDL_Scancode s)
{
    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) return 'A' + (s - SDL_SCANCODE_A);
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_9) return '1' + (s - SDL_SCANCODE_1);
    if (s == SDL_SCANCODE_0) return '0';
    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F12) return 0x70 + (s - SDL_SCANCODE_F1);
    if (s >= SDL_SCANCODE_KP_1 && s <= SDL_SCANCODE_KP_9) return 0x61 + (s - SDL_SCANCODE_KP_1);
    switch (s) {
    case SDL_SCANCODE_KP_0: return 0x60;
    case SDL_SCANCODE_BACKSPACE: return 0x08; case SDL_SCANCODE_TAB: return 0x09;
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return 0x0d;
    case SDL_SCANCODE_LSHIFT: case SDL_SCANCODE_RSHIFT: return 0x10;
    case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_RCTRL: return 0x11;
    case SDL_SCANCODE_LALT: case SDL_SCANCODE_RALT: return 0x12;
    case SDL_SCANCODE_PAUSE: return 0x13; case SDL_SCANCODE_CAPSLOCK: return 0x14;
    case SDL_SCANCODE_ESCAPE: return 0x1b; case SDL_SCANCODE_SPACE: return 0x20;
    case SDL_SCANCODE_PAGEUP: return 0x21; case SDL_SCANCODE_PAGEDOWN: return 0x22;
    case SDL_SCANCODE_END: return 0x23; case SDL_SCANCODE_HOME: return 0x24;
    case SDL_SCANCODE_LEFT: return 0x25; case SDL_SCANCODE_UP: return 0x26;
    case SDL_SCANCODE_RIGHT: return 0x27; case SDL_SCANCODE_DOWN: return 0x28;
    case SDL_SCANCODE_INSERT: return 0x2d; case SDL_SCANCODE_DELETE: return 0x2e;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x6a; case SDL_SCANCODE_KP_PLUS: return 0x6b;
    case SDL_SCANCODE_KP_MINUS: return 0x6d; case SDL_SCANCODE_KP_PERIOD: return 0x6e; case SDL_SCANCODE_KP_DIVIDE: return 0x6f;
    case SDL_SCANCODE_SEMICOLON: return 0xba; case SDL_SCANCODE_EQUALS: return 0xbb; case SDL_SCANCODE_COMMA: return 0xbc;
    case SDL_SCANCODE_MINUS: return 0xbd; case SDL_SCANCODE_PERIOD: return 0xbe; case SDL_SCANCODE_SLASH: return 0xbf;
    case SDL_SCANCODE_GRAVE: return 0xc0; case SDL_SCANCODE_LEFTBRACKET: return 0xdb; case SDL_SCANCODE_BACKSLASH: return 0xdc;
    case SDL_SCANCODE_RIGHTBRACKET: return 0xdd; case SDL_SCANCODE_APOSTROPHE: return 0xde;
    default: return 0;
    }
}

/* PC scan code (set 1) for the lParam of key messages */
static int scan_of(SDL_Scancode s, int *ext)
{
    static const uint8_t az[26] = { 0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,0x19,0x10,0x13,0x1f,0x14,0x16,0x2f,0x11,0x2d,0x15,0x2c };
    *ext = 0;
    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) return az[s - SDL_SCANCODE_A];
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_0) return 2 + (s - SDL_SCANCODE_1);
    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F10) return 0x3b + (s - SDL_SCANCODE_F1);
    switch (s) {
    case SDL_SCANCODE_ESCAPE: return 0x01; case SDL_SCANCODE_RETURN: return 0x1c; case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_LSHIFT: return 0x2a; case SDL_SCANCODE_RSHIFT: return 0x36;
    case SDL_SCANCODE_LCTRL: return 0x1d; case SDL_SCANCODE_RCTRL: *ext = 1; return 0x1d;
    case SDL_SCANCODE_LALT: return 0x38; case SDL_SCANCODE_RALT: *ext = 1; return 0x38;
    case SDL_SCANCODE_SLASH: return 0x35; case SDL_SCANCODE_PERIOD: return 0x34; case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_TAB: return 0x0f; case SDL_SCANCODE_BACKSPACE: return 0x0e;
    case SDL_SCANCODE_UP: *ext = 1; return 0x48; case SDL_SCANCODE_DOWN: *ext = 1; return 0x50;
    case SDL_SCANCODE_LEFT: *ext = 1; return 0x4b; case SDL_SCANCODE_RIGHT: *ext = 1; return 0x4d;
    default: return 0;
    }
}

/* left/right modifier state, for GetAsyncKeyState(VK_LSHIFT ...) */
static void side_keys(SDL_Scancode s, int down)
{
    int vk = 0;
    switch (s) {
    case SDL_SCANCODE_LSHIFT: vk = 0xa0; break; case SDL_SCANCODE_RSHIFT: vk = 0xa1; break;
    case SDL_SCANCODE_LCTRL: vk = 0xa2; break; case SDL_SCANCODE_RCTRL: vk = 0xa3; break;
    case SDL_SCANCODE_LALT: vk = 0xa4; break; case SDL_SCANCODE_RALT: vk = 0xa5; break;
    default: return;
    }
    keys[vk] = (uint8_t)down;
}

static void key(SDL_Scancode s, int down, int repeat)
{
    int vk = vk_of(s), ext = 0, sc = scan_of(s, &ext);
    if (!vk) return;
    side_keys(s, down);
    if (vk == 0x10 || vk == 0x11 || vk == 0x12) {         /* generic modifier: down while either side is */
        int l = vk == 0x10 ? 0xa0 : vk == 0x11 ? 0xa2 : 0xa4;
        keys[vk] = keys[l] | keys[l + 1];
    } else keys[vk] = (uint8_t)down;
    uint32_t lp = 1u | ((uint32_t)sc << 16) | (ext ? 1u << 24 : 0);
    if (!down) lp |= 0xc0000000u; else if (repeat) lp |= 0x40000000u;
    int sys = vk == 0x12 || (keys[0x12] && vk != 0x12);
    rt_post_input(down ? (sys ? 0x104 : 0x100) : (sys ? 0x105 : 0x101), (uint32_t)vk, lp);
}

static int mouse_buttons;                              /* bit 0 left, bit 1 right (real and scripted) */
int rt_key_down(int vk)
{
    if (vk == 1) return (mouse_buttons & 1) != 0;
    if (vk == 2) return (mouse_buttons & 2) != 0;
    return vk >= 0 && vk < 256 && keys[vk];
}

static void to_game(int wx, int wy, int *gx, int *gy)
{
    SDL_Rect d = dest_rect();
    float sx = 1, sy = 1;
    if (win && ren) {
        int ww, wh, ow, oh;
        SDL_GetWindowSize(win, &ww, &wh); SDL_GetRendererOutputSize(ren, &ow, &oh);
        if (ww && wh) { sx = (float)ow / ww; sy = (float)oh / wh; }
    }
    *gx = d.w ? (int)((wx * sx - d.x) * game_w / d.w) : wx;
    *gy = d.h ? (int)((wy * sy - d.y) * game_h / d.h) : wy;
}
static void mouse_msg(uint32_t msg, int gx, int gy)
{
    uint32_t wp = (mouse_buttons & 1 ? 1 : 0) | (mouse_buttons & 2 ? 2 : 0) | (keys[0x10] ? 4 : 0) | (keys[0x11] ? 8 : 0);
    rt_post_input(msg, wp, (uint32_t)(gx & 0xffff) | ((uint32_t)(gy & 0xffff) << 16));
}

void rt_warp_mouse(int x, int y)
{
    if (headless || !win) return;
    SDL_Rect d = dest_rect();
    int ww, wh, ow, oh;
    SDL_GetWindowSize(win, &ww, &wh); SDL_GetRendererOutputSize(ren, &ow, &oh);
    float sx = ow ? (float)ww / ow : 1, sy = oh ? (float)wh / oh : 1;
    SDL_WarpMouseInWindow(win, (int)((d.x + x * d.w / game_w) * sx), (int)((d.y + y * d.h / game_h) * sy));
}

/* ------------------------------------------------------------------------ */
/* test script                                                               */
/* ------------------------------------------------------------------------ */
typedef struct { uint32_t ms; char cmd[8]; char arg[200]; } script_ev;
static script_ev *script; static int nscript, iscript;
static int pend_key[64]; static uint32_t pend_ms[64]; static int npend;   /* key releases; -1 - x*4096 - y: mouse up */

static void load_script(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    if (!f) { fprintf(stderr, "cannot open script %s\n", path); return; }
    script = (script_ev *)calloc(2048, sizeof *script);
    while (fgets(line, sizeof line, f) && nscript < 2048) {
        script_ev *e = &script[nscript];
        e->arg[0] = 0;
        if (line[0] == '#' || sscanf(line, "%u %7s %199[^\n]", &e->ms, e->cmd, e->arg) < 2) continue;
        nscript++;
    }
    fclose(f);
}

static void save_shot(const char *name)
{
    extern void rt_debug_screen(void);
    rt_debug_screen();
    rt_present_if_dirty(1);
    if (!frame) { rt_log("screenshot %s: nothing shown yet\n", name); return; }
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(frame, frame_w, frame_h, 32, frame_w * 4, SDL_PIXELFORMAT_ARGB8888);
    if (s) { SDL_SaveBMP(s, name); SDL_FreeSurface(s); rt_log("screenshot %s (%dx%d)\n", name, frame_w, frame_h); }
}

static void run_script(void)
{
    uint32_t ms = (uint32_t)(rt_ticks_us() / 1000);
    for (int i = 0; i < npend;) {
        if (ms >= pend_ms[i]) {
            if (pend_key[i] < 0) { int v = -1 - pend_key[i]; mouse_buttons &= ~1; mouse_msg(0x202, v >> 12, v & 4095); }
            else key((SDL_Scancode)pend_key[i], 0, 0);
            pend_key[i] = pend_key[--npend]; pend_ms[i] = pend_ms[npend];
        }
        else i++;
    }
    while (iscript < nscript && ms >= script[iscript].ms) {
        script_ev *e = &script[iscript++];
        char nm[64]; unsigned hold = 80; int x, y;
        if (!strcmp(e->cmd, "key") || !strcmp(e->cmd, "hold") || !strcmp(e->cmd, "down") || !strcmp(e->cmd, "up")) {
            if (sscanf(e->arg, "%63s %u", nm, &hold) < 1) continue;
            if (strcmp(e->cmd, "hold")) hold = 80;
            SDL_Scancode s = SDL_GetScancodeFromName(nm);
            if (s == SDL_SCANCODE_UNKNOWN) { rt_log("script: unknown key %s\n", nm); continue; }
            if (!strcmp(e->cmd, "up")) { key(s, 0, 0); continue; }
            key(s, 1, 0);
            if (!strcmp(e->cmd, "down")) continue;
            if (npend < 64) { pend_key[npend] = s; pend_ms[npend++] = ms + hold; }
        } else if (!strcmp(e->cmd, "click") && sscanf(e->arg, "%d %d", &x, &y) == 2) {
            /* held for 150 ms: the game also polls the button with GetAsyncKeyState */
            mouse_msg(0x200, x, y);
            mouse_buttons |= 1; mouse_msg(0x201, x, y);
            if (npend < 64 && x >= 0 && y >= 0 && x < 4096 && y < 4096) { pend_key[npend] = -1 - (x * 4096 + y); pend_ms[npend++] = ms + 150; }
        } else if (!strcmp(e->cmd, "move") && sscanf(e->arg, "%d %d", &x, &y) == 2) {
            mouse_msg(0x200, x, y);
        } else if (!strcmp(e->cmd, "shot")) save_shot(e->arg);
        else if (!strcmp(e->cmd, "quit")) { rt_log("script: quit\n"); rt_exit(0); }
    }
}

/* ------------------------------------------------------------------------ */
/* events                                                                    */
/* ------------------------------------------------------------------------ */
void rt_pump_events(void)
{
    static uint32_t last;
    uint32_t now = rt_ticks_ms();
    if (now == last && !script) return;                  /* at most once per millisecond */
    last = now;
    if (script) run_script();
    rt_present_if_dirty(0);
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: rt_quit_requested = 1; break;
        case SDL_KEYDOWN: case SDL_KEYUP:
            if (e.key.keysym.scancode == SDL_SCANCODE_F12) { if (e.type == SDL_KEYDOWN) rt_quit_requested = 1; break; }
            if (e.key.keysym.scancode == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
                if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                    int fs = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
                break;
            }
            key(e.key.keysym.scancode, e.type == SDL_KEYDOWN, e.key.repeat);
            break;
        case SDL_MOUSEMOTION: { int x, y; to_game(e.motion.x, e.motion.y, &x, &y); mouse_msg(0x200, x, y); break; }
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: {
            int x, y, b = e.button.button == SDL_BUTTON_LEFT ? 1 : e.button.button == SDL_BUTTON_RIGHT ? 2 : 0;
            if (!b) break;
            to_game(e.button.x, e.button.y, &x, &y);
            if (e.type == SDL_MOUSEBUTTONDOWN) mouse_buttons |= b; else mouse_buttons &= ~b;
            uint32_t m = b == 1 ? (e.type == SDL_MOUSEBUTTONDOWN ? (e.button.clicks == 2 ? 0x203 : 0x201) : 0x202)
                                : (e.type == SDL_MOUSEBUTTONDOWN ? (e.button.clicks == 2 ? 0x206 : 0x204) : 0x205);
            mouse_msg(m, x, y);
            break; }
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_EXPOSED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                if (frame) rt_video_frame(frame, frame_w, frame_h);
            }
            break;
        default: break;
        }
    }
}

void rt_idle_wait(int ms)
{
    rt_present_if_dirty(1);
    if (script) { run_script(); }
    SDL_Delay((Uint32)(ms > 0 ? ms : 1));
}

/* ------------------------------------------------------------------------ */
/* start-up                                                                  */
/* ------------------------------------------------------------------------ */
static int file_exists_ci(const char *dir, const char *name, char *out, size_t n)
{
    snprintf(out, n, "%s/%s", dir, name);
    FILE *f = fopen(out, "rb");
    if (f) { fclose(f); return 1; }
    return rt_resolve_path(name, out, n, 0);           /* other spellings of the name */
}

int main(int argc, char **argv)
{
    const char *game = ".", *save = NULL, *scriptfile = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--game") && i + 1 < argc) game = argv[++i];
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
        else if (!strcmp(argv[i], "--script") && i + 1 < argc) scriptfile = argv[++i];
        else if (!strcmp(argv[i], "--headless")) headless = 1;
    }
    snprintf(rt_game_dir, sizeof rt_game_dir, "%s", game);
    if (save) snprintf(rt_save_dir, sizeof rt_save_dir, "%s", save);
    else snprintf(rt_save_dir, sizeof rt_save_dir, "%s/save", game);

    if (headless) SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0 && SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    perf_freq = SDL_GetPerformanceFrequency();
    perf_t0 = SDL_GetPerformanceCounter();
    if (!headless) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        win = SDL_CreateWindow("3-D Ultra Pinball", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 960, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); SDL_RenderClear(ren); SDL_RenderPresent(ren);
    }
    if (scriptfile) load_script(scriptfile);

    rt_mem_init();
    rt_win32_init();
    rt_sos_init();
    char exe[2048];
    if (!file_exists_ci(game, "PINBALL.EXE", exe, sizeof exe))
        rt_fatal("PINBALL.EXE was not found in %s. Point --game at your 3-D Ultra Pinball folder.", game);
    rt_set_program(&SIERRA_PROGRAM);
    uint32_t entry = rt_load_image(exe, &SIERRA_PROGRAM);
    rt_log("loaded %s, entry %08x\n", exe, entry);

    /* thread information block: SEH chain end, stack limits, self pointer */
    uint32_t tib = rt_low_alloc(0x100);
    WR32(tib + 0x00, 0xffffffffu);
    WR32(tib + 0x04, STACK_TOP);
    WR32(tib + 0x08, STACK_LO);
    WR32(tib + 0x18, tib);
    WR32(tib + 0x20, 0x50); WR32(tib + 0x24, 0x51);   /* process / thread ids */
    R.fsbase = tib;
    ESP = STACK_TOP - 0x10;
    FNINIT();
    PUSH32(RET_MAGIC);
    EIP = entry;
    rt_run();
    rt_log("program returned %u\n", EAX);
    SDL_Quit();
    return (int)EAX;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR c, int d)
{
    (void)a; (void)b; (void)c; (void)d;
    SDL_SetMainReady();
    return main(__argc, __argv);
}
#endif
