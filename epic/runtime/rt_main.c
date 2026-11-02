/* rt_main.c - SDL2 front end for the recompiled Epic Pinball.
 *
 *   EpicPinball [--game DIR] [--save DIR] [--view full|original] [--script FILE]
 *
 * --game   folder with the original game files (default: the current folder)
 * --save   where settings and high scores are written (default: <game>/save)
 * --view   full (whole table, no scrolling; default) or original (the 1993 scrolling window)
 * --script test automation: lines "<ms> key <SDL key name>", "<ms> shot <file.bmp>", "<ms> view full|original",
 *          "<ms> quit". Times are from start-up.
 *
 * Keys: F11 switches between the full-table view and the original scrolling view, Alt+Enter toggles fullscreen,
 * F12 closes the game (back to the table library).
 * Everything else goes to the game exactly like a DOS keyboard.
 */
#include "rt_internal.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static int tex_w, tex_h;
static uint32_t frame[640 * 480];
static int frame_w, frame_h;
static double frame_aspect = 4.0 / 3.0;
static int headless;
static uint64_t perf_freq, perf_t0;

uint64_t rt_now_us(void)
{
    return (SDL_GetPerformanceCounter() - perf_t0) * 1000000ull / perf_freq;
}

void rt_idle(void)
{
    SDL_Delay(1);
}

/* ------------------------------------------------------------------------ */
/* keyboard: SDL scancode -> XT set 1                                        */
/* ------------------------------------------------------------------------ */
static int xt_code(SDL_Scancode s, int *ext)
{
    static const struct { SDL_Scancode s; uint8_t c; uint8_t e; } map[] = {
        {SDL_SCANCODE_ESCAPE, 0x01, 0}, {SDL_SCANCODE_1, 0x02, 0}, {SDL_SCANCODE_2, 0x03, 0}, {SDL_SCANCODE_3, 0x04, 0},
        {SDL_SCANCODE_4, 0x05, 0}, {SDL_SCANCODE_5, 0x06, 0}, {SDL_SCANCODE_6, 0x07, 0}, {SDL_SCANCODE_7, 0x08, 0},
        {SDL_SCANCODE_8, 0x09, 0}, {SDL_SCANCODE_9, 0x0A, 0}, {SDL_SCANCODE_0, 0x0B, 0}, {SDL_SCANCODE_MINUS, 0x0C, 0},
        {SDL_SCANCODE_EQUALS, 0x0D, 0}, {SDL_SCANCODE_BACKSPACE, 0x0E, 0}, {SDL_SCANCODE_TAB, 0x0F, 0},
        {SDL_SCANCODE_Q, 0x10, 0}, {SDL_SCANCODE_W, 0x11, 0}, {SDL_SCANCODE_E, 0x12, 0}, {SDL_SCANCODE_R, 0x13, 0},
        {SDL_SCANCODE_T, 0x14, 0}, {SDL_SCANCODE_Y, 0x15, 0}, {SDL_SCANCODE_U, 0x16, 0}, {SDL_SCANCODE_I, 0x17, 0},
        {SDL_SCANCODE_O, 0x18, 0}, {SDL_SCANCODE_P, 0x19, 0}, {SDL_SCANCODE_LEFTBRACKET, 0x1A, 0},
        {SDL_SCANCODE_RIGHTBRACKET, 0x1B, 0}, {SDL_SCANCODE_RETURN, 0x1C, 0}, {SDL_SCANCODE_LCTRL, 0x1D, 0},
        {SDL_SCANCODE_A, 0x1E, 0}, {SDL_SCANCODE_S, 0x1F, 0}, {SDL_SCANCODE_D, 0x20, 0}, {SDL_SCANCODE_F, 0x21, 0},
        {SDL_SCANCODE_G, 0x22, 0}, {SDL_SCANCODE_H, 0x23, 0}, {SDL_SCANCODE_J, 0x24, 0}, {SDL_SCANCODE_K, 0x25, 0},
        {SDL_SCANCODE_L, 0x26, 0}, {SDL_SCANCODE_SEMICOLON, 0x27, 0}, {SDL_SCANCODE_APOSTROPHE, 0x28, 0},
        {SDL_SCANCODE_GRAVE, 0x29, 0}, {SDL_SCANCODE_LSHIFT, 0x2A, 0}, {SDL_SCANCODE_BACKSLASH, 0x2B, 0},
        {SDL_SCANCODE_Z, 0x2C, 0}, {SDL_SCANCODE_X, 0x2D, 0}, {SDL_SCANCODE_C, 0x2E, 0}, {SDL_SCANCODE_V, 0x2F, 0},
        {SDL_SCANCODE_B, 0x30, 0}, {SDL_SCANCODE_N, 0x31, 0}, {SDL_SCANCODE_M, 0x32, 0}, {SDL_SCANCODE_COMMA, 0x33, 0},
        {SDL_SCANCODE_PERIOD, 0x34, 0}, {SDL_SCANCODE_SLASH, 0x35, 0}, {SDL_SCANCODE_RSHIFT, 0x36, 0},
        {SDL_SCANCODE_KP_MULTIPLY, 0x37, 0}, {SDL_SCANCODE_LALT, 0x38, 0}, {SDL_SCANCODE_SPACE, 0x39, 0},
        {SDL_SCANCODE_CAPSLOCK, 0x3A, 0}, {SDL_SCANCODE_F1, 0x3B, 0}, {SDL_SCANCODE_F2, 0x3C, 0},
        {SDL_SCANCODE_F3, 0x3D, 0}, {SDL_SCANCODE_F4, 0x3E, 0}, {SDL_SCANCODE_F5, 0x3F, 0}, {SDL_SCANCODE_F6, 0x40, 0},
        {SDL_SCANCODE_F7, 0x41, 0}, {SDL_SCANCODE_F8, 0x42, 0}, {SDL_SCANCODE_F9, 0x43, 0}, {SDL_SCANCODE_F10, 0x44, 0},
        {SDL_SCANCODE_KP_7, 0x47, 0}, {SDL_SCANCODE_KP_8, 0x48, 0}, {SDL_SCANCODE_KP_9, 0x49, 0},
        {SDL_SCANCODE_KP_MINUS, 0x4A, 0}, {SDL_SCANCODE_KP_4, 0x4B, 0}, {SDL_SCANCODE_KP_5, 0x4C, 0},
        {SDL_SCANCODE_KP_6, 0x4D, 0}, {SDL_SCANCODE_KP_PLUS, 0x4E, 0}, {SDL_SCANCODE_KP_1, 0x4F, 0},
        {SDL_SCANCODE_KP_2, 0x50, 0}, {SDL_SCANCODE_KP_3, 0x51, 0}, {SDL_SCANCODE_KP_0, 0x52, 0},
        {SDL_SCANCODE_KP_PERIOD, 0x53, 0}, {SDL_SCANCODE_F12, 0x58, 0},
        {SDL_SCANCODE_KP_ENTER, 0x1C, 1}, {SDL_SCANCODE_RCTRL, 0x1D, 1}, {SDL_SCANCODE_RALT, 0x38, 1},
        {SDL_SCANCODE_HOME, 0x47, 1}, {SDL_SCANCODE_UP, 0x48, 1}, {SDL_SCANCODE_PAGEUP, 0x49, 1},
        {SDL_SCANCODE_LEFT, 0x4B, 1}, {SDL_SCANCODE_RIGHT, 0x4D, 1}, {SDL_SCANCODE_END, 0x4F, 1},
        {SDL_SCANCODE_DOWN, 0x50, 1}, {SDL_SCANCODE_PAGEDOWN, 0x51, 1}, {SDL_SCANCODE_INSERT, 0x52, 1},
        {SDL_SCANCODE_DELETE, 0x53, 1},
    };
    size_t i;
    for (i = 0; i < sizeof map / sizeof map[0]; i++)
        if (map[i].s == s)
        {
            *ext = map[i].e;
            return map[i].c;
        }
    return 0;
}

static void key(SDL_Scancode s, int down)
{
    int ext = 0, c = xt_code(s, &ext);
    if (!c) return;
    if (ext) rt_kbd_push(0xE0);
    rt_kbd_push((uint8_t)(down ? c : (c | 0x80)));
}

/* ------------------------------------------------------------------------ */
/* test script                                                               */
/* ------------------------------------------------------------------------ */
typedef struct { uint32_t ms; char cmd[8]; char arg[200]; } script_ev;
static script_ev *script;
static int nscript, iscript;
static int pending_up[64], npending;  /* keys released one frame later */
static uint32_t pending_up_ms[64];

static void load_script(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    if (!f) { rt_log("cannot open script %s", path); return; }
    script = (script_ev *)calloc(512, sizeof *script);
    while (fgets(line, sizeof line, f) && nscript < 512)
    {
        script_ev *e = &script[nscript];
        if (line[0] == '#' || sscanf(line, "%u %7s %199[^\n]", &e->ms, e->cmd, e->arg) < 2) continue;
        nscript++;
    }
    fclose(f);
}

static void save_shot(const char *name)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(frame, frame_w, frame_h, 32, frame_w * 4, SDL_PIXELFORMAT_ARGB8888);
    if (s) { SDL_SaveBMP(s, name); SDL_FreeSurface(s); rt_log("screenshot %s (%dx%d)", name, frame_w, frame_h); }
}

static void run_script(void)
{
    uint32_t ms = (uint32_t)(rt_now_us() / 1000);
    int i;
    for (i = 0; i < npending;)
    {
        if (ms >= pending_up_ms[i])
        {
            key((SDL_Scancode)pending_up[i], 0);
            pending_up[i] = pending_up[--npending];
            pending_up_ms[i] = pending_up_ms[npending];
        }
        else i++;
    }
    while (iscript < nscript && ms >= script[iscript].ms)
    {
        script_ev *e = &script[iscript++];
        if (!strcmp(e->cmd, "key") || !strcmp(e->cmd, "hold"))
        {
            SDL_Scancode s = SDL_GetScancodeFromName(e->arg);
            uint32_t hold = 80;
            if (!strcmp(e->cmd, "hold"))
            {
                char nm[64];
                unsigned h;
                if (sscanf(e->arg, "%63s %u", nm, &h) == 2) { s = SDL_GetScancodeFromName(nm); hold = h; }
            }
            if (s == SDL_SCANCODE_UNKNOWN) { rt_log("script: unknown key %s", e->arg); continue; }
            key(s, 1);
            if (npending < 64) { pending_up[npending] = s; pending_up_ms[npending++] = ms + hold; }
        }
        else if (!strcmp(e->cmd, "shot")) save_shot(e->arg);
        else if (!strcmp(e->cmd, "view")) rt_view = !strcmp(e->arg, "original") ? RT_VIEW_ORIGINAL : RT_VIEW_FULL;
        else if (!strcmp(e->cmd, "quit")) rt_quit = 1;
    }
}

/* ------------------------------------------------------------------------ */
/* presentation                                                              */
/* ------------------------------------------------------------------------ */
static void fit_window_to_view(void)
{
    /* size the window for the view: 4:3 for the original, a tall window for the whole table */
    int dw, dh;
    SDL_DisplayMode dm;
    if (headless || (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP)) return;
    if (SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(win), &dm) != 0) { dm.w = 1280; dm.h = 720; }
    dh = dm.h * 85 / 100;
    dw = (int)(dh * frame_aspect);
    if (dw > dm.w * 9 / 10) { dw = dm.w * 9 / 10; dh = (int)(dw / frame_aspect); }
    SDL_SetWindowSize(win, dw, dh);
}

void rt_present(void)
{
    int w, h;
    double aspect;
    static double last_aspect;
    if (!rt_vga_render(frame, 640, 480, &w, &h, &aspect)) return;
    frame_w = w; frame_h = h; frame_aspect = aspect;
    if (script) run_script();
    if (headless) return;
    if (!tex || tex_w != w || tex_h != h)
    {
        if (tex) SDL_DestroyTexture(tex);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        tex_w = w; tex_h = h;
    }
    if (aspect != last_aspect)
    {
        last_aspect = aspect;
        fit_window_to_view();
    }
    SDL_UpdateTexture(tex, NULL, frame, w * 4);
    {
        int ow, oh;
        SDL_Rect dst;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        if ((double)ow / oh > aspect) { dst.h = oh; dst.w = (int)(oh * aspect); }
        else { dst.w = ow; dst.h = (int)(ow / aspect); }
        dst.x = (ow - dst.w) / 2;
        dst.y = (oh - dst.h) / 2;
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, &dst);
        SDL_RenderPresent(ren);
    }
}

void rt_pump_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_QUIT: rt_quit = 1; break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (e.key.keysym.scancode == SDL_SCANCODE_F12)
            {
                rt_quit = 1;               /* back to the table library, like the Full Tilt tables */
                break;
            }
            if (e.key.keysym.scancode == SDL_SCANCODE_F11)
            {
                if (e.type == SDL_KEYDOWN && !e.key.repeat)
                {
                    rt_view = rt_view == RT_VIEW_FULL ? RT_VIEW_ORIGINAL : RT_VIEW_FULL;
                    rt_log("view: %s", rt_view == RT_VIEW_FULL ? "full table" : "original scrolling");
                }
                break;
            }
            if (e.key.keysym.scancode == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT))
            {
                if (e.type == SDL_KEYDOWN && !e.key.repeat)
                {
                    int fs = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
                break;
            }
            if (e.key.repeat) break;   /* the game sees typematic repeat only as held keys */
            key(e.key.keysym.scancode, e.type == SDL_KEYDOWN);
            break;
        default: break;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Sound card selection without the DOS SETUP program: PINBALL.EXE only needs SOUNDCRD.INF, which names the
   MASI driver to load. It is the driver's 24-byte signature followed by the driver's file name (length byte +
   name + NUL). We write it to the save folder (searched before the game folder), so the original files are
   never changed. */
extern int rt_find_game_file(const char *dosname, char *out, size_t outsz);

static void ensure_sound_config(const char *save, const char *driver)
{
    char path[1200], drv[1200];
    unsigned char hdr[24], out[64];
    FILE *f;
    size_t n = strlen(driver);
    if (rt_find_game_file("SOUNDCRD.INF", path, sizeof path))
    {
        /* only keep one we wrote ourselves (in the save folder); a SETUP-made one in the game folder is ignored */
        if (strncmp(path, save, strlen(save)) == 0) return;
    }
    if (!rt_find_game_file(driver, drv, sizeof drv) || n > 30) { rt_log("sound driver %s not found", driver); return; }
    f = fopen(drv, "rb");
    if (!f || fread(hdr, 1, sizeof hdr, f) != sizeof hdr) { if (f) fclose(f); return; }
    fclose(f);
    memcpy(out, hdr, 24);
    out[24] = (unsigned char)n;
    memcpy(out + 25, driver, n + 1);
    snprintf(path, sizeof path, "%s/SOUNDCRD.INF", save);
    f = fopen(path, "wb");
    if (f) { fwrite(out, 1, 25 + n + 1, f); fclose(f); rt_log("sound: %s", driver); }

    /* SETUP also marks the driver itself as configured. The header dword at 0x30 points at the driver's
       settings block: byte 0 is the "configured" flag and bytes 0x11-0x14 the port/IRQ/DMA choices (FF = none).
       Write a configured copy to the save folder. */
    {
        FILE *in = fopen(drv, "rb");
        long sz;
        unsigned char *buf;
        uint32_t cfg;
        if (!in) return;
        fseek(in, 0, SEEK_END); sz = ftell(in); fseek(in, 0, SEEK_SET);
        buf = (unsigned char *)malloc((size_t)sz);
        if (!buf || fread(buf, 1, (size_t)sz, in) != (size_t)sz) { fclose(in); free(buf); return; }
        fclose(in);
        cfg = (uint32_t)buf[0x30] | ((uint32_t)buf[0x31] << 8);
        if (cfg && cfg + 0x15 <= (uint32_t)sz)
        {
            buf[cfg] = 1;
            memset(buf + cfg + 0x11, 0xFF, 4);
            snprintf(path, sizeof path, "%s/%s", save, driver);
            f = fopen(path, "wb");
            if (f) { fwrite(buf, 1, (size_t)sz, f); fclose(f); }
        }
        free(buf);
    }
}

/* ------------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    const char *game = ".", *save = NULL, *scriptfile = NULL;
    char savebuf[1100];
    int i;
    for (i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--game") && i + 1 < argc) game = argv[++i];
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
        else if (!strcmp(argv[i], "--view") && i + 1 < argc) rt_view = !strcmp(argv[++i], "original") ? RT_VIEW_ORIGINAL : RT_VIEW_FULL;
        else if (!strcmp(argv[i], "--script") && i + 1 < argc) scriptfile = argv[++i];
        else if (!strcmp(argv[i], "--headless")) headless = 1;
    }
    if (!save) { snprintf(savebuf, sizeof savebuf, "%s/save", game); save = savebuf; }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    perf_freq = SDL_GetPerformanceFrequency();
    perf_t0 = SDL_GetPerformanceCounter();
    if (!headless)
    {
        win = SDL_CreateWindow("Epic Pinball", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 720,
                               SDL_WINDOW_RESIZABLE);
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    }
    if (scriptfile) load_script(scriptfile);

    rt_core_init();
    rt_vga_init();
    rt_dos_init(game, save);
    ensure_sound_config(save, "MDRV001R.MUS");   /* "no sound card" until native sound is in */
    if (rt_exec("PINBALL.EXE", "", 0) != 0)
    {
        const char *msg = "Could not start PINBALL.EXE. Point --game at your Epic Pinball folder.";
        fprintf(stderr, "%s\n", msg);
        if (!headless) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Epic Pinball", msg, win);
        return 1;
    }
    rt_run();
    SDL_Quit();
    return rt_exit_code;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* GUI program on Windows: no console window */
int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR c, int d)
{
    (void)a; (void)b; (void)c; (void)d;
    SDL_SetMainReady();
    return main(__argc, __argv);
}
#endif
