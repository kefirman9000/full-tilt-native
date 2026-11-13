/*
 * rt_win32.c - the Windows API that 3-D Ultra Pinball uses, implemented natively:
 * KERNEL32 (files, INI files, memory, process), USER32 (one top-level window, messages, timers, input),
 * GDI32 (8-bit DIB sections, palettes, BitBlt/StretchBlt to the window) and WINMM (time, MCI).
 *
 * The game believes it runs on Windows 95, installed in C:\SIERRA\UPINBALL with its CD in D:.
 */
#include "rt_internal.h"
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#define unlink_ _unlink
#else
#include <unistd.h>
#define unlink_ unlink
#endif

#define INSTALL_DIR "C:\\SIERRA\\UPINBALL"

/* ======================================================================== */
/* handles                                                                   */
/* ======================================================================== */
enum { H_NONE, H_FILE, H_MUTEX, H_DC, H_BITMAP, H_PALETTE, H_BRUSH, H_STOCK, H_MENU, H_ICON, H_CURSOR, H_ACCEL };
typedef struct {
    int type;
    /* file */
    FILE *f;
    /* dc */
    uint32_t hwnd, bitmap, palette, brush;
    /* bitmap (DIB section) */
    int w, h, bpp, pitch, topdown, palcolors;
    uint32_t bits;
    uint32_t ctab[256];            /* RGB colour table */
    uint16_t pidx[256];            /* DIB_PAL_COLORS indices */
    /* palette */
    uint32_t pal[256]; int npal;
} gobj;

#define HBASE 0x4000u
#define MAXH 4096
static gobj H[MAXH];

static uint32_t h_new(int type)
{
    for (uint32_t k = 1; k < MAXH; k++) if (!H[k].type) { memset(&H[k], 0, sizeof H[k]); H[k].type = type; return HBASE + k * 4; }
    rt_fatal("Out of handles.");
    return 0;
}
static gobj *h_get(uint32_t h, int type)
{
    uint32_t k = (h - HBASE) / 4;
    if (h < HBASE || (h - HBASE) % 4 || k >= MAXH || !H[k].type) return NULL;
    if (type && H[k].type != type) return NULL;
    return &H[k];
}
static void h_free(uint32_t h) { gobj *g = h_get(h, 0); if (g) g->type = H_NONE; }

static uint32_t last_error;
static int trace_input = -1;
#define ITRACE(...) do { if (trace_input < 0) trace_input = getenv("SIERRA_TRACE_INPUT") != NULL; if (trace_input) rt_log(__VA_ARGS__); } while (0)

/* ======================================================================== */
/* KERNEL32                                                                  */
/* ======================================================================== */
#define K(name, n) static uint32_t k_##name(const uint32_t *a)
#define UNUSED (void)a

K(GetVersion, 0) { UNUSED; return 0xc3b60004u; }    /* Windows 95 */
K(SetLastError, 1) { last_error = a[0]; return 0; }
K(GetLastError, 0) { UNUSED; return last_error; }
K(GetLogicalDrives, 0) { UNUSED; return 0x0c; }      /* C: and D: */
K(SetHandleCount, 1) { return a[0]; }
K(GetCurrentThreadId, 0) { UNUSED; return 0x51; }
K(SetConsoleCtrlHandler, 2) { UNUSED; return 1; }
K(UnhandledExceptionFilter, 1) { UNUSED; return 1; }
K(WaitForSingleObject, 2) { UNUSED; return 0; }
K(ReleaseMutex, 1) { UNUSED; return 1; }
K(CreateMutexA, 3) { UNUSED; last_error = 0; return h_new(H_MUTEX); }

K(ExitProcess, 1)
{
    rt_log("ExitProcess(%u)\n", a[0]);
    extern void rt_exit(int code);
    rt_exit((int)a[0]);
    return 0;
}

K(RaiseException, 4)
{
    rt_fatal("The game raised exception %08x (flags %x) - it hit an internal error.", a[0], a[1]);
    return 0;
}

K(RtlUnwind, 4)
{
    /* unwind the SEH chain down to the target frame (handlers are not called for the frames skipped) */
    if (a[0]) WR32(R.fsbase, a[0]);
    return 0;
}

K(GetModuleHandleA, 1)
{
    if (!a[0]) return 0x400000;
    const char *n = GSTR(a[0]);
    rt_log("GetModuleHandleA(%s)\n", n);
    if (!strncasecmp(n, "kernel32", 8)) return 0x1ff80000u;
    return 0;
}

K(GetModuleFileNameA, 3) { rt_put_str(a[1], INSTALL_DIR "\\PINBALL.EXE", a[2]); return (uint32_t)strlen(INSTALL_DIR "\\PINBALL.EXE"); }

static uint32_t cmdline, envblock;
K(GetCommandLineA, 0) { UNUSED; if (!cmdline) cmdline = rt_guest_strdup(INSTALL_DIR "\\PINBALL.EXE"); return cmdline; }
K(GetEnvironmentStrings, 0)
{
    UNUSED;
    if (!envblock) { static const char e[] = "PATH=C:\\WINDOWS\0WINDIR=C:\\WINDOWS\0\0"; envblock = rt_low_alloc(sizeof e); memcpy(M + envblock, e, sizeof e); }
    return envblock;
}
K(GetEnvironmentVariableA, 3) { UNUSED; return 0; }
K(GetStartupInfoA, 1) { memset(GPTR(a[0]), 0, 68); WR32(a[0], 68); return 0; }
K(GetStdHandle, 1) { return 0x10 + (a[0] & 3); }
K(GetFileType, 1) { return h_get(a[0], H_FILE) ? 1 : 2; }

K(GetLocalTime, 1)
{
    time_t t = time(NULL); struct tm *tm = localtime(&t);
    uint32_t p = a[0];
    WR16(p, (uint16_t)(tm->tm_year + 1900)); WR16(p + 2, (uint16_t)(tm->tm_mon + 1)); WR16(p + 4, (uint16_t)tm->tm_wday);
    WR16(p + 6, (uint16_t)tm->tm_mday); WR16(p + 8, (uint16_t)tm->tm_hour); WR16(p + 10, (uint16_t)tm->tm_min);
    WR16(p + 12, (uint16_t)tm->tm_sec); WR16(p + 14, 0);
    return 0;
}
K(GetTimeZoneInformation, 1) { memset(GPTR(a[0]), 0, 172); return 0; }

/* ---- memory ---- */
K(GlobalAlloc, 2) { return rt_alloc(a[1], 16); }
K(GlobalLock, 1) { return a[0]; }
K(GlobalUnlock, 1) { UNUSED; return 1; }
K(GlobalHandle, 1) { return a[0]; }
K(GlobalFree, 1) { rt_free(a[0]); return 0; }

K(VirtualAlloc, 4)
{
    uint32_t addr = a[0], size = a[1];
    if (addr) return addr & ~0xfffu;          /* committing pages inside a reservation: memory is always there */
    return rt_alloc(size, 0x10000);
}
K(VirtualFree, 3)
{
    if (a[2] & 0x8000) rt_free(a[0]);         /* MEM_RELEASE */
    return 1;
}

/* ---- files ---- */
static uint32_t open_file(const char *guest, int write, int create, int trunc)
{
    char host[2048];
    FILE *f = NULL;
    if (write || create) {
        if (rt_resolve_path(guest, host, sizeof host, 1)) {
            /* copy-on-write: a game file opened for writing is copied into the save folder first */
            if (strncmp(host, rt_save_dir, strlen(rt_save_dir))) {
                char src[2048]; snprintf(src, sizeof src, "%s", host);
                const char *base = strrchr(src, '/') ? strrchr(src, '/') + 1 : src;
                snprintf(host, sizeof host, "%s/%s", rt_save_dir, base);
                if (!trunc) { FILE *s = fopen(src, "rb"), *d = s ? rt_open_save(base, "wb") : NULL;
                    if (s && d) { char b[65536]; size_t n; while ((n = fread(b, 1, sizeof b, s))) fwrite(b, 1, n, d); }
                    if (s) fclose(s); if (d) fclose(d); }
            }
            struct stat st;
            int ex = stat(host, &st) == 0;
            if (trunc || (create && !ex)) f = fopen(host, "w+b");
            else if (ex) f = fopen(host, write ? "r+b" : "rb");
        }
    } else if (rt_resolve_path(guest, host, sizeof host, 0)) {
        f = fopen(host, "rb");
    }
    rt_log("open %s (%s) -> %s\n", guest, write ? "w" : "r", f ? host : "NOT FOUND");
    if (!f) { last_error = 2; return 0xffffffffu; }
    uint32_t h = h_new(H_FILE);
    h_get(h, H_FILE)->f = f;
    return h;
}

K(CreateFileA, 7)
{
    uint32_t acc = a[1], disp = a[4];
    int write = (acc & 0x40000000u) != 0;
    /* 1 CREATE_NEW 2 CREATE_ALWAYS 3 OPEN_EXISTING 4 OPEN_ALWAYS 5 TRUNCATE_EXISTING */
    return open_file(GSTR(a[0]), write, disp == 1 || disp == 2 || disp == 4, disp == 2 || disp == 5);
}
K(_lopen, 2) { uint32_t m = a[1] & 3; uint32_t h = open_file(GSTR(a[0]), m != 0, 0, 0); return h; }
K(CloseHandle, 1) { gobj *g = h_get(a[0], 0); if (g && g->type == H_FILE) { fclose(g->f); } h_free(a[0]); return 1; }
K(_lclose, 1) { gobj *g = h_get(a[0], H_FILE); if (g) { fclose(g->f); h_free(a[0]); return 0; } return 0xffffffffu; }

K(ReadFile, 5)
{
    gobj *g = h_get(a[0], H_FILE);
    if (!g) { if (a[3]) WR32(a[3], 0); return 0; }
    size_t n = fread(GPTR(a[1]), 1, a[2], g->f);
    if (a[3]) WR32(a[3], (uint32_t)n);
    return 1;
}
K(_hread, 3) { gobj *g = h_get(a[0], H_FILE); if (!g) return 0xffffffffu; return (uint32_t)fread(GPTR(a[1]), 1, a[2], g->f); }
K(WriteFile, 5)
{
    gobj *g = h_get(a[0], H_FILE);
    if (!g) { if (a[3]) WR32(a[3], a[2]); return 1; }   /* stdout/stderr: discard */
    size_t n = fwrite(GPTR(a[1]), 1, a[2], g->f);
    if (a[3]) WR32(a[3], (uint32_t)n);
    return 1;
}
K(FlushFileBuffers, 1) { gobj *g = h_get(a[0], H_FILE); if (g) fflush(g->f); return 1; }
K(SetFilePointer, 4)
{
    gobj *g = h_get(a[0], H_FILE);
    if (!g) return 0xffffffffu;
    fseek(g->f, (long)(int32_t)a[1], a[3] == 0 ? SEEK_SET : a[3] == 1 ? SEEK_CUR : SEEK_END);
    if (a[2]) WR32(a[2], 0);
    return (uint32_t)ftell(g->f);
}
K(_llseek, 3)
{
    gobj *g = h_get(a[0], H_FILE);
    if (!g) return 0xffffffffu;
    fseek(g->f, (long)(int32_t)a[1], a[2] == 0 ? SEEK_SET : a[2] == 1 ? SEEK_CUR : SEEK_END);
    return (uint32_t)ftell(g->f);
}
K(GetFileSize, 2)
{
    gobj *g = h_get(a[0], H_FILE);
    if (!g) return 0xffffffffu;
    long p = ftell(g->f); fseek(g->f, 0, SEEK_END); long n = ftell(g->f); fseek(g->f, p, SEEK_SET);
    if (a[1]) WR32(a[1], 0);
    return (uint32_t)n;
}
K(GetFileAttributesA, 1)
{
    char host[2048];
    if (!rt_resolve_path(GSTR(a[0]), host, sizeof host, 0)) { rt_log("attributes %s: not found\n", GSTR(a[0])); last_error = 2; return 0xffffffffu; }
    struct stat st; stat(host, &st);
    return (st.st_mode & 0170000) == 0040000 ? 0x10 : 0x20;   /* directory : archive */
}
K(DeleteFileA, 1)
{
    char host[2048];
    if (rt_resolve_path(GSTR(a[0]), host, sizeof host, 1) && !strncmp(host, rt_save_dir, strlen(rt_save_dir))) unlink_(host);
    return 1;
}
K(MoveFileA, 2)
{
    char s[2048], d[2048];
    if (!rt_resolve_path(GSTR(a[0]), s, sizeof s, 1) || !rt_resolve_path(GSTR(a[1]), d, sizeof d, 1)) return 0;
    return rename(s, d) == 0;
}
static char curdir[260] = INSTALL_DIR;
K(SetCurrentDirectoryA, 1) { rt_log("SetCurrentDirectory(%s)\n", GSTR(a[0])); snprintf(curdir, sizeof curdir, "%s", GSTR(a[0])); return 1; }
K(GetCurrentDirectoryA, 2) { rt_put_str(a[1], curdir, a[0]); return (uint32_t)strlen(curdir); }
K(CreateProcessA, 10) { rt_log("CreateProcessA(%s, %s) refused\n", GSTR(a[0]), GSTR(a[1])); return 0; }

/* ---- INI files ---- */
static char *ini_load(const char *file)
{
    char host[2048];
    if (!rt_resolve_path(file, host, sizeof host, 0)) return NULL;
    FILE *f = fopen(host, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *s = (char *)malloc(n + 1); n = (long)fread(s, 1, n, f); s[n] = 0; fclose(f);
    return s;
}

/* finds key in section; returns malloc'd value or NULL */
static char *ini_get(const char *txt, const char *sec, const char *key)
{
    if (!txt) return NULL;
    int in = 0;
    const char *p = txt;
    while (*p) {
        const char *e = p; while (*e && *e != '\n') e++;
        char line[1024]; size_t n = (size_t)(e - p); if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, p, n); line[n] = 0;
        while (n && (line[n-1] == '\r' || line[n-1] == ' ')) line[--n] = 0;
        char *l = line; while (*l == ' ' || *l == '\t') l++;
        if (*l == '[') { char *c = strchr(l, ']'); if (c) *c = 0; in = !strcasecmp(l + 1, sec); }
        else if (in) {
            char *eq = strchr(l, '=');
            if (eq) { *eq = 0; char *k = l; size_t kl = strlen(k); while (kl && k[kl-1] == ' ') k[--kl] = 0;
                if (!strcasecmp(k, key)) { char *v = eq + 1; while (*v == ' ') v++; return strdup(v); } }
        }
        p = *e ? e + 1 : e;
    }
    return NULL;
}

K(GetPrivateProfileStringA, 6)
{
    const char *sec = GSTR(a[0]), *file = GSTR(a[5]);
    if (!a[1]) { rt_put_str(a[3], "", a[4]); return 0; }   /* key enumeration: not needed */
    const char *key = GSTR(a[1]), *def = GSTR(a[2]);
    char *txt = ini_load(file);
    char *v = ini_get(txt, sec, key);
    if (!v && !strcasecmp(file, "sierra.ini") && !strcasecmp(key, "installpath")) v = strdup(INSTALL_DIR);
    const char *out = v ? v : def;
    rt_put_str(a[3], out, a[4]);
    rt_log("GetPrivateProfileString([%s] %s, %s) = %s\n", sec, key, file, out);
    uint32_t n = (uint32_t)strlen(out); if (n >= a[4]) n = a[4] ? a[4] - 1 : 0;
    free(v); free(txt);
    return n;
}

K(WritePrivateProfileStringA, 4)
{
    const char *sec = GSTR(a[0]), *file = GSTR(a[3]);
    const char *key = a[1] ? GSTR(a[1]) : NULL, *val = a[2] ? GSTR(a[2]) : NULL;
    char *txt = ini_load(file);
    /* rebuild the file with the one key changed (simple, files are tiny) */
    char host[2048]; rt_resolve_path(file, host, sizeof host, 1);
    if (strncmp(host, rt_save_dir, strlen(rt_save_dir))) { const char *b = strrchr(file, '\\'); snprintf(host, sizeof host, "%s/%s", rt_save_dir, b ? b + 1 : file); }
    FILE *o = fopen(host, "wb");
    if (!o) { free(txt); return 0; }
    int in = 0, done = 0, any_sec = 0;
    const char *p = txt ? txt : "";
    while (*p) {
        const char *e = p; while (*e && *e != '\n') e++;
        char line[1024]; size_t n = (size_t)(e - p); if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, p, n); line[n] = 0; while (n && line[n-1] == '\r') line[--n] = 0;
        int skip = 0;
        if (line[0] == '[') {
            if (in && !done && key && val) { fprintf(o, "%s=%s\r\n", key, val); done = 1; }
            char s2[1024]; snprintf(s2, sizeof s2, "%s", line + 1); char *c = strchr(s2, ']'); if (c) *c = 0;
            in = !strcasecmp(s2, sec); if (in) any_sec = 1;
            if (in && !key) skip = 1;
        } else if (in) {
            if (!key) skip = 1;
            else { char *eq = strchr(line, '='); if (eq) { size_t kl = (size_t)(eq - line);
                if (kl == strlen(key) && !strncasecmp(line, key, kl)) { skip = 1; if (val && !done) { fprintf(o, "%s=%s\r\n", key, val); done = 1; } } } }
        }
        if (!skip) fprintf(o, "%s\r\n", line);
        p = *e ? e + 1 : e;
    }
    if (key && val && !done) { if (!in) fprintf(o, "[%s]\r\n", sec); else if (!any_sec) fprintf(o, "[%s]\r\n", sec); fprintf(o, "%s=%s\r\n", key, val); }
    fclose(o); free(txt);
    return 1;
}

/* ---- DLLs ---- */
#define HMOD_SOS   0x1ff90000u
#define HMOD_K32   0x1ff80000u
K(LoadLibraryA, 1)
{
    const char *n = GSTR(a[0]);
    rt_log("LoadLibraryA(%s)\n", n);
    if (!strncasecmp(n, "sos", 3)) return HMOD_SOS;
    return 0;                                         /* WING32.DLL etc.: not present */
}
K(FreeLibrary, 1) { UNUSED; return 1; }
K(GetProcAddress, 2)
{
    const char *n = a[1] < 0x10000 ? "#ordinal" : GSTR(a[1]);
    uint32_t t = 0;
    if (a[0] == HMOD_SOS) t = rt_thunk_for("sos9502", n);
    else if (a[0] == HMOD_K32) t = rt_thunk_for("kernel32", n);
    if (!t) rt_log("GetProcAddress(%08x, %s): not available\n", a[0], n);
    return t;
}

/* ======================================================================== */
/* USER32: windows and messages                                              */
/* ======================================================================== */
typedef struct { char name[64]; uint32_t proc, style, extra, cextra; } wclass;
typedef struct {
    int used; int cls; uint32_t proc, parent, style, exstyle, id, user;
    int x, y, w, h, visible, invalid;
    uint32_t dc;                   /* its window DC */
    char text[128];
    uint8_t extra[64];
    uint32_t dlgproc; int dlg_ended; uint32_t dlg_result;   /* dialog boxes */
} wnd;
#define MAXW 32
#define HWND_BASE 0x2000u
static wclass classes[32]; static int nclasses;
static wnd W[MAXW];
static uint32_t main_hwnd, focus_hwnd, capture_hwnd;

static wnd *w_get(uint32_t h) { uint32_t k = h - HWND_BASE; if (h < HWND_BASE || k >= MAXW || !W[k].used) return NULL; return &W[k]; }

/* non-client edges of a child window (the main window is shown without a frame: its client is the whole picture).
 * Windows 95 metrics: WS_BORDER 1, WS_DLGFRAME 3 (no border), WS_CAPTION = border 1 + 18 caption, WS_THICKFRAME 4. */
static void nc_insets(const wnd *w, int *l, int *t, int *r, int *b)
{
    int e = 0, cap = 0;
    *l = *t = *r = *b = 0;
    if (!w || !w->parent) return;
    uint32_t st = w->style;
    if ((st & 0x00c00000u) == 0x00c00000u) { e = 1; cap = 18; }
    else if (st & 0x00400000u) e = 3;
    else if (st & 0x00800000u) e = 1;
    if (st & 0x00040000u) e = 4;
    *l = *r = *b = e; *t = e + cap;
    if (cap) *t = cap;                                 /* the caption's height includes the top border line */
}
static void client_size(const wnd *w, int *cw, int *ch)
{
    int l, t, r, b; nc_insets(w, &l, &t, &r, &b);
    *cw = w->w - l - r; *ch = w->h - t - b;
    if (*cw < 0) *cw = 0;
    if (*ch < 0) *ch = 0;
}
/* where a window's client area is in the main window's client area */
static void client_origin(uint32_t hwnd, int *ox, int *oy)
{
    *ox = *oy = 0;
    for (int guard = 0; guard < 16; guard++) {
        wnd *w = w_get(hwnd);
        if (!w || !w->parent || hwnd == main_hwnd) return;
        int l, t, r, b; nc_insets(w, &l, &t, &r, &b);
        *ox += w->x + l; *oy += w->y + t;
        hwnd = w->parent;
    }
}

typedef struct { uint32_t hwnd, msg, wp, lp, time; int32_t x, y; } qmsg;
#define QMAX 512
static qmsg Q[QMAX]; static int qhead, qtail;
static int quit_posted; static uint32_t quit_code;
int rt_quit_requested;
static int mouse_x, mouse_y;

#define MOUSE_PENDING 0xffffffffu                        /* queued mouse input, not yet given a window */
static void q_post(uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp)
{
    int n = (qtail + 1) % QMAX;
    if (n == qhead) return;                          /* full: drop */
    Q[qtail].hwnd = hwnd; Q[qtail].msg = msg; Q[qtail].wp = wp; Q[qtail].lp = lp;
    Q[qtail].time = rt_ticks_ms(); Q[qtail].x = mouse_x; Q[qtail].y = mouse_y;
    qtail = n;
}

static uint32_t def_window_proc(uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp);

static uint32_t send(uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp)
{
    wnd *w = w_get(hwnd);
    if (!w) return 0;
    uint32_t args[4] = { hwnd, msg, wp, lp };
    if (msg == 0x121) { static int n; if (n++ < 6) ITRACE("WM_ENTERIDLE -> %x (dialog %d)\n", hwnd, w->dlgproc != 0); }
    if (w->dlgproc) {
        /* the dialog manager's window procedure: the dialog procedure first, then the defaults */
        uint32_t r = rt_call_guest(w->dlgproc, 0, 4, args);
        if (msg == 0x110) return r;                     /* WM_INITDIALOG */
        if (r) return r;
        if (msg == 0x10) { w->dlg_ended = 1; w->dlg_result = 2; return 0; }   /* WM_CLOSE -> IDCANCEL */
        return w_get(hwnd) ? def_window_proc(hwnd, msg, wp, lp) : 0;
    }
    if (!w->proc) return 0;
    return rt_call_guest(w->proc, 0, 4, args);
}

/* timers */
typedef struct { uint32_t hwnd, id, ms, proc, next; int used; } wtimer;
static wtimer T[32];

static void client_origin(uint32_t hwnd, int *ox, int *oy);
static void client_size(const wnd *w, int *cw, int *ch);

static int shown(uint32_t h)
{
    for (int guard = 0; guard < 16 && h; guard++) { wnd *w = w_get(h); if (!w || !w->visible) return 0; h = w->parent; }
    return 1;
}
/* the deepest visible window whose client area holds the point (newest first: it is on top) */
static uint32_t window_at(int x, int y)
{
    uint32_t best = main_hwnd; int depth = -1;
    for (int k = MAXW - 1; k >= 0; k--) {
        uint32_t h = HWND_BASE + k;
        if (!W[k].used || !W[k].parent || !shown(h)) continue;
        int ox, oy, cw, ch; client_origin(h, &ox, &oy); client_size(&W[k], &cw, &ch);
        if (x < ox || y < oy || x >= ox + cw || y >= oy + ch) continue;
        int d = 0; for (uint32_t p = W[k].parent; p && p != main_hwnd && d < 16; p = w_get(p) ? w_get(p)->parent : 0) d++;
        if (d > depth) { depth = d; best = h; }
    }
    return best;
}

void rt_post_input(uint32_t msg, uint32_t wp, uint32_t lp)
{
    if (msg >= 0x200 && msg <= 0x20a) {
        mouse_x = (int16_t)(lp & 0xffff); mouse_y = (int16_t)(lp >> 16);
        /* coalesce moves like Windows does: only the newest WM_MOUSEMOVE waits in the queue */
        if (msg == 0x200) for (int i = qhead; i != qtail; i = (i + 1) % QMAX)
            if (Q[i].hwnd == MOUSE_PENDING && Q[i].msg == 0x200) { Q[i].wp = wp; Q[i].lp = lp; Q[i].x = mouse_x; Q[i].y = mouse_y; return; }
        q_post(MOUSE_PENDING, msg, wp, lp);            /* the target window is found when the game takes it */
        return;
    }
    uint32_t h = focus_hwnd ? focus_hwnd : main_hwnd;
    if (h) q_post(h, msg, wp, lp);
}

#define U(name, n) static uint32_t u_##name(const uint32_t *a)

U(RegisterClassA, 1)
{
    uint32_t c = a[0];
    if (nclasses >= 32) return 0;
    wclass *k = &classes[nclasses];
    k->style = RD32(c); k->proc = RD32(c + 4); k->cextra = RD32(c + 8); k->extra = RD32(c + 12);
    uint32_t nm = RD32(c + 36);
    if (nm < 0x10000) snprintf(k->name, sizeof k->name, "#%u", nm); else snprintf(k->name, sizeof k->name, "%s", GSTR(nm));
    rt_log("RegisterClass %s proc %08x\n", k->name, k->proc);
    return 0xc000 + nclasses++;
}

static int find_class(uint32_t nm)
{
    char b[64];
    if (nm < 0x10000) { if (nm >= 0xc000 && nm < 0xc000u + nclasses) return (int)(nm - 0xc000); snprintf(b, sizeof b, "#%u", nm); }
    else snprintf(b, sizeof b, "%s", GSTR(nm));
    for (int k = 0; k < nclasses; k++) if (!strcasecmp(classes[k].name, b)) return k;
    return -1;
}

U(CreateWindowExA, 12)
{
    int c = find_class(a[1]);
    int k; for (k = 0; k < MAXW && W[k].used; k++) ;
    if (k == MAXW) return 0;
    wnd *w = &W[k]; memset(w, 0, sizeof *w);
    w->used = 1; w->cls = c; w->proc = c >= 0 ? classes[c].proc : 0;
    w->exstyle = a[0]; w->style = a[3];
    w->x = (int32_t)a[4]; w->y = (int32_t)a[5]; w->w = (int32_t)a[6]; w->h = (int32_t)a[7];
    if (w->x == (int32_t)0x80000000) w->x = 0;
    if (w->y == (int32_t)0x80000000) w->y = 0;
    if (w->w == (int32_t)0x80000000 || w->w <= 0) w->w = 640;
    if (w->h == (int32_t)0x80000000 || w->h <= 0) w->h = 480;
    w->parent = a[8]; w->id = a[9];
    if (a[2]) snprintf(w->text, sizeof w->text, "%s", GSTR(a[2]));
    uint32_t h = HWND_BASE + k;
    rt_log("CreateWindowEx class=%s title=%s style=%08x %d,%d %dx%d parent=%x -> %x\n",
           c >= 0 ? classes[c].name : "?", w->text, w->style, w->x, w->y, w->w, w->h, w->parent, h);
    if (!w->parent && !main_hwnd) main_hwnd = h;
    if (c < 0) { rt_log("  unknown window class\n"); }
    /* CREATESTRUCT */
    uint32_t cs = rt_alloc(48, 16);
    uint32_t v[12] = { a[11], a[10], a[9], a[8], (uint32_t)w->h, (uint32_t)w->w, (uint32_t)w->y, (uint32_t)w->x, a[3], a[2], a[1], a[0] };
    for (int i = 0; i < 12; i++) WR32(cs + 4 * i, v[i]);
    if (w->proc) {
        send(h, 0x81, 0, cs);                         /* WM_NCCREATE */
        if ((int32_t)send(h, 0x01, 0, cs) == -1) { rt_free(cs); w->used = 0; return 0; }   /* WM_CREATE */
        send(h, 0x05, 0, (uint32_t)(w->w & 0xffff) | ((uint32_t)w->h << 16));            /* WM_SIZE */
        send(h, 0x03, 0, (uint32_t)(w->x & 0xffff) | ((uint32_t)w->y << 16));            /* WM_MOVE */
    }
    rt_free(cs);
    if (w->style & 0x10000000) { w->visible = 1; w->invalid = 1; }                     /* WS_VISIBLE */
    return h;
}

U(ShowWindow, 2)
{
    wnd *w = w_get(a[0]);
    if (!w) return 0;
    int was = w->visible;
    w->visible = a[1] != 0;
    if (a[1] == 3 && w->parent) {                      /* SW_MAXIMIZE a child: its client area fills the parent's */
        wnd *pw = w_get(w->parent);
        int pcw = 640, pch = 480, l, t, r, b;
        if (pw) client_size(pw, &pcw, &pch);
        nc_insets(w, &l, &t, &r, &b);
        w->x = -l; w->y = -t; w->w = pcw + l + r; w->h = pch + t + b;
        send(a[0], 0x05, 2, (uint32_t)(pcw & 0xffff) | ((uint32_t)pch << 16));   /* WM_SIZE, SIZE_MAXIMIZED */
        w->invalid = 1;
    }
    rt_log("ShowWindow(%x, %u)\n", a[0], a[1]);
    if (a[0] == main_hwnd && w->visible) {
        rt_video_init(w->w, w->h);
        send(a[0], 0x18, 1, 0);                       /* WM_SHOWWINDOW */
        send(a[0], 0x1c, 1, 0);                       /* WM_ACTIVATEAPP */
        send(a[0], 0x06, 1, 0);                       /* WM_ACTIVATE */
        focus_hwnd = a[0];
        send(a[0], 0x07, 0, 0);                       /* WM_SETFOCUS */
        send(a[0], 0x30f, 0, 0);                      /* WM_QUERYNEWPALETTE */
        w->invalid = 1;
    }
    return (uint32_t)was;
}
U(UpdateWindow, 1) { wnd *w = w_get(a[0]); if (w && w->invalid) { w->invalid = 0; send(a[0], 0x0f, 0, 0); } return 1; }
U(InvalidateRect, 3) { wnd *w = w_get(a[0] ? a[0] : main_hwnd); if (w) w->invalid = 1; return 1; }
U(DestroyWindow, 1)
{
    wnd *w = w_get(a[0]);
    if (!w) return 0;
    rt_log("DestroyWindow(%x)\n", a[0]);
    send(a[0], 0x02, 0, 0);                           /* WM_DESTROY */
    send(a[0], 0x82, 0, 0);                           /* WM_NCDESTROY */
    w->used = 0;
    if (a[0] == main_hwnd) main_hwnd = 0;
    return 1;
}
U(MoveWindow, 6)
{
    wnd *w = w_get(a[0]);
    if (!w) return 0;
    w->x = (int32_t)a[1]; w->y = (int32_t)a[2]; w->w = (int32_t)a[3]; w->h = (int32_t)a[4];
    rt_log("MoveWindow(%x) %d,%d %dx%d\n", a[0], w->x, w->y, w->w, w->h);
    if (a[0] == main_hwnd && w->visible) rt_video_init(w->w, w->h);
    send(a[0], 0x05, 0, (uint32_t)(w->w & 0xffff) | ((uint32_t)w->h << 16));
    if (a[5]) w->invalid = 1;
    return 1;
}
U(GetWindowRect, 2) { wnd *w = w_get(a[0]); if (!w) return 0; WR32(a[1], w->x); WR32(a[1] + 4, w->y); WR32(a[1] + 8, w->x + w->w); WR32(a[1] + 12, w->y + w->h); return 1; }
U(GetClientRect, 2) { wnd *w = w_get(a[0]); if (!w) return 0; int cw, ch; client_size(w, &cw, &ch); WR32(a[1], 0); WR32(a[1] + 4, 0); WR32(a[1] + 8, cw); WR32(a[1] + 12, ch); return 1; }
U(GetWindowPlacement, 2)
{
    wnd *w = w_get(a[0]); if (!w) return 0;
    uint32_t p = a[1]; WR32(p, 44); WR32(p + 4, 0); WR32(p + 8, 1);
    for (int i = 12; i < 28; i += 4) WR32(p + i, 0);
    WR32(p + 28, w->x); WR32(p + 32, w->y); WR32(p + 36, w->x + w->w); WR32(p + 40, w->y + w->h);
    return 1;
}
U(SetWindowPlacement, 2)
{
    wnd *w = w_get(a[0]); if (!w) return 0;
    uint32_t p = a[1];
    int x0 = (int32_t)RD32(p + 28), y0 = (int32_t)RD32(p + 32), x1 = (int32_t)RD32(p + 36), y1 = (int32_t)RD32(p + 40);
    if (x1 > x0 && y1 > y0) { w->x = x0; w->y = y0; w->w = x1 - x0; w->h = y1 - y0; }
    rt_log("SetWindowPlacement(%x) %d,%d %dx%d show %u\n", a[0], w->x, w->y, w->w, w->h, RD32(p + 8));
    return 1;
}
U(GetWindowTextA, 3) { wnd *w = w_get(a[0]); rt_put_str(a[1], w ? w->text : "", a[2]); return w ? (uint32_t)strlen(w->text) : 0; }
U(SetWindowTextA, 2) { wnd *w = w_get(a[0]); if (w) snprintf(w->text, sizeof w->text, "%s", GSTR(a[1])); return 1; }
static uint32_t dlg_class_proc(void);
U(GetWindowLongA, 2)
{
    wnd *w = w_get(a[0]); if (!w) return 0;
    switch ((int32_t)a[1]) {
    case -4: return w->dlgproc ? dlg_class_proc() : w->proc;
    case 4: if (w->dlgproc) return w->dlgproc; break;      /* DWL_DLGPROC */
    case -6: return 0x400000;
    case -8: return w->parent;
    case -12: return w->id;
    case -16: return w->style | (w->visible ? 0x10000000 : 0);
    case -20: return w->exstyle;
    case -21: return w->user;
    default: break;
    }
    if ((int32_t)a[1] >= 0 && a[1] + 4 <= sizeof w->extra) { uint32_t v; memcpy(&v, w->extra + a[1], 4); return v; }
    return 0;
}
/* the dialog class's window procedure (DefDlgProc): the dialog procedure, then the defaults */
U(DefDlgProcA, 4) { return send(a[0], a[1], a[2], a[3]); }
static uint32_t dlg_class_proc(void) { static uint32_t t; if (!t) t = rt_thunk_for("user32", "DefDlgProcA"); return t; }

U(GetClassLongA, 2)
{
    wnd *w = w_get(a[0]);
    if (w && w->dlgproc && (int32_t)a[1] == -24) return dlg_class_proc();   /* GCL_WNDPROC */
    if (!w || w->cls < 0) return 0;
    switch ((int32_t)a[1]) { case -24: return classes[w->cls].proc; case -26: return classes[w->cls].style; default: return 0; }
}
U(GetParent, 1) { wnd *w = w_get(a[0]); return w ? w->parent : 0; }
U(IsChild, 2) { wnd *w = w_get(a[1]); return w && w->parent == a[0]; }
U(IsIconic, 1) { UNUSED; return 0; }
U(GetWindow, 2) { UNUSED; return 0; }
U(GetTopWindow, 1) { UNUSED; return main_hwnd; }
U(GetActiveWindow, 0) { UNUSED; return main_hwnd; }
U(GetLastActivePopup, 1) { return a[0]; }
U(GetFocus, 0) { UNUSED; return focus_hwnd; }
U(SetFocus, 1) { uint32_t o = focus_hwnd; focus_hwnd = a[0]; ITRACE("SetFocus(%x)\n", a[0]); return o; }
U(SetForegroundWindow, 1) { UNUSED; return 1; }
U(BringWindowToTop, 1) { UNUSED; return 1; }
U(EnableWindow, 2) { UNUSED; return 0; }
U(FindWindowA, 2) { UNUSED; return 0; }
U(GetDlgCtrlID, 1) { wnd *w = w_get(a[0]); return w ? w->id : 0; }
U(ChildWindowFromPoint, 3)
{
    /* POINT by value (x, y) in the parent's client coordinates: the direct child that contains it (hidden ones
     * too, like Windows), else the parent itself if the point is inside it, else NULL */
    int x = (int32_t)a[1], y = (int32_t)a[2];
    wnd *pw = w_get(a[0]);
    if (!pw) return 0;
    int pcw, pch; client_size(pw, &pcw, &pch);
    for (int k = MAXW - 1; k >= 0; k--) {
        if (!W[k].used || W[k].parent != a[0] || W[k].dlgproc) continue;
        if (x >= W[k].x && y >= W[k].y && x < W[k].x + W[k].w && y < W[k].y + W[k].h) return HWND_BASE + k;
    }
    return (x >= 0 && y >= 0 && x < pcw && y < pch) ? a[0] : 0;
}
U(GetDialogBaseUnits, 0) { UNUSED; return 0x00100008; }
U(GetSystemMetrics, 1)
{
    switch (a[0]) {
    case 0: return 640; case 1: return 480;            /* SM_CXSCREEN, SM_CYSCREEN */
    case 4: return 18;                                 /* SM_CYCAPTION */
    case 5: case 6: return 1;                          /* borders */
    case 7: case 8: return 3;                          /* dialog frame */
    case 15: return 18;                                /* SM_CYMENU */
    case 32: case 33: return 4;                        /* SM_CXFRAME */
    case 2: case 3: return 16;                         /* scroll bars */
    case 11: case 12: return 32;                       /* icons */
    case 13: case 14: return 32;                       /* cursor */
    case 16: return 640; case 17: return 480 - 18;     /* full screen client */
    default: return 0;
    }
}
U(EnumThreadWindows, 3)
{
    for (int k = 0; k < MAXW; k++) {
        if (!W[k].used || W[k].parent) continue;
        uint32_t args[2] = { HWND_BASE + k, a[2] };
        if (!rt_call_guest(a[1], 0, 2, args)) break;
    }
    return 1;
}

static uint32_t u_DestroyWindow(const uint32_t *a);
static uint32_t def_window_proc(uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp)
{
    uint32_t a[4] = { hwnd, msg, wp, lp };
    switch (a[1]) {
    case 0x81: return 1;                               /* WM_NCCREATE */
    case 0x10: u_DestroyWindow(a); return 0;          /* WM_CLOSE */
    case 0x0f: { wnd *w = w_get(a[0]); if (w) w->invalid = 0; return 0; }
    case 0x14: return 1;                               /* WM_ERASEBKGND */
    case 0x112: if ((a[2] & 0xfff0) == 0xf060) q_post(a[0], 0x10, 0, 0); return 0; /* SC_CLOSE */
    case 0x20: return 0;                               /* WM_SETCURSOR */
    default: return 0;
    }
}
U(DefWindowProcA, 4) { return def_window_proc(a[0], a[1], a[2], a[3]); }
U(PostMessageA, 4) { q_post(a[0], a[1], a[2], a[3]); return 1; }
int rt_mouse_x(void) { return mouse_x; }
int rt_mouse_y(void) { return mouse_y; }
void rt_post_message(uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp) { q_post(hwnd, msg, wp, lp); }
U(SendMessageA, 4) { return send(a[0], a[1], a[2], a[3]); }
U(PostQuitMessage, 1) { quit_posted = 1; quit_code = a[0]; return 0; }

static void put_msg(uint32_t p, const qmsg *m)
{
    WR32(p, m->hwnd); WR32(p + 4, m->msg); WR32(p + 8, m->wp); WR32(p + 12, m->lp);
    WR32(p + 16, m->time); WR32(p + 20, (uint32_t)m->x); WR32(p + 24, (uint32_t)m->y);
}

static uint32_t last_msg_time;

/* one message if any is ready: queued input/posts, then quit, then paint, then timers */
static int fetch(uint32_t p, uint32_t hfilter, uint32_t lo, uint32_t hi, int remove)
{
    (void)hfilter;
    rt_pump_events();
    rt_poll_async();
    if (rt_quit_requested && main_hwnd) { rt_quit_requested = 0; q_post(main_hwnd, 0x10, 0, 0); }
    for (int i = qhead; i != qtail; i = (i + 1) % QMAX) {
        if ((lo || hi) && (Q[i].msg < lo || Q[i].msg > hi)) continue;
        if (Q[i].hwnd == MOUSE_PENDING) {
            int x = (int16_t)(Q[i].lp & 0xffff), y = (int16_t)(Q[i].lp >> 16);
            uint32_t h = capture_hwnd ? capture_hwnd : window_at(x, y);
            int ox, oy; client_origin(h, &ox, &oy);
            if (Q[i].msg != 0x200) ITRACE("mouse %x at %d,%d -> %x (capture %x)\n", Q[i].msg, x, y, h, capture_hwnd);
            if (remove) { Q[i].hwnd = h; Q[i].lp = (uint32_t)((x - ox) & 0xffff) | ((uint32_t)((y - oy) & 0xffff) << 16); }
            else { qmsg m = Q[i]; m.hwnd = h; m.lp = (uint32_t)((x - ox) & 0xffff) | ((uint32_t)((y - oy) & 0xffff) << 16); put_msg(p, &m); last_msg_time = m.time; return 1; }
        }
        put_msg(p, &Q[i]); last_msg_time = Q[i].time;
        if (remove) {
            /* remove entry i, keep order */
            for (int j = i; j != qtail; ) { int n = (j + 1) % QMAX; if (n == qtail) break; Q[j] = Q[n]; j = n; }
            qtail = (qtail - 1 + QMAX) % QMAX;
        }
        return 1;
    }
    if (quit_posted) {
        qmsg m = { 0, 0x12, quit_code, 0, rt_ticks_ms(), mouse_x, mouse_y };
        put_msg(p, &m);
        if (remove) quit_posted = 0;
        return 1;
    }
    for (int k = 0; k < MAXW; k++) if (W[k].used && W[k].visible && W[k].invalid && (!(lo || hi) || (lo <= 0x0f && hi >= 0x0f))) {
        qmsg m = { HWND_BASE + k, 0x0f, 0, 0, rt_ticks_ms(), mouse_x, mouse_y };
        put_msg(p, &m);
        return 1;
    }
    uint32_t now = rt_ticks_ms();
    for (int k = 0; k < 32; k++) if (T[k].used && (int32_t)(now - T[k].next) >= 0 && (!(lo || hi) || (lo <= 0x113 && hi >= 0x113))) {
        qmsg m = { T[k].hwnd, 0x113, T[k].id, T[k].proc, now, mouse_x, mouse_y };
        put_msg(p, &m);
        if (remove) T[k].next = now + T[k].ms;
        return 1;
    }
    return 0;
}

static uint32_t stat_peeks, stat_empty, stat_blits, stat_t0;
static void stats(void)
{
    static int on = -1;
    if (on < 0) on = getenv("SIERRA_STATS") != NULL;
    if (!on) return;
    uint32_t now = rt_ticks_ms();
    if (!stat_t0) stat_t0 = now;
    if (now - stat_t0 >= 2000) {
        rt_log("stats: %u peeks/s (%u empty), %u window blits/s\n", stat_peeks * 1000 / (now - stat_t0),
               stat_empty * 1000 / (now - stat_t0), stat_blits * 1000 / (now - stat_t0));
        stat_peeks = stat_empty = stat_blits = 0; stat_t0 = now;
    }
}
U(PeekMessageA, 5)
{
    int r = fetch(a[0], a[1], a[2], a[3], a[4] & 1);
    stat_peeks++; if (!r) stat_empty++;
    stats();
    /* The game waits for its next frame by calling PeekMessage in a loop (about two million times a second). Its
     * timing comes from timeGetTime, so after a run of empty peeks it can give the CPU back for a millisecond. */
    static uint32_t idle;
    if (r) idle = 0;
    else if (++idle >= 256) {
        idle = 0;
        extern void rt_idle_wait(int ms);
        rt_idle_wait(1);
    }
    return (uint32_t)r;
}
U(GetMessageA, 4)
{
    while (!fetch(a[0], a[1], a[2], a[3], 1)) {
        extern void rt_idle_wait(int ms);
        rt_idle_wait(2);
    }
    return RD32(a[0] + 4) != 0x12;
}
U(TranslateMessage, 1)
{
    uint32_t m = RD32(a[0] + 4), vk = RD32(a[0] + 8);
    if (m == 0x100) {
        int ch = 0;
        int shift = rt_key_down(0x10);
        if (vk >= 'A' && vk <= 'Z') ch = shift ? (int)vk : (int)vk + 32;
        else if (vk >= '0' && vk <= '9') ch = (int)vk;
        else if (vk == 0x20 || vk == 0x0d || vk == 0x08 || vk == 0x1b) ch = (int)vk;
        else if (vk == 0xbd) ch = shift ? '_' : '-';
        else if (vk == 0xbe) ch = '.';
        if (ch) q_post(RD32(a[0]), 0x102, (uint32_t)ch, RD32(a[0] + 12));
    }
    return 1;
}
U(DispatchMessageA, 1)
{
    uint32_t h = RD32(a[0]), m = RD32(a[0] + 4), wp = RD32(a[0] + 8), lp = RD32(a[0] + 12);
    if ((m >= 0x100 && m <= 0x105) || (m >= 0x201 && m <= 0x206)) ITRACE("dispatch %x msg %x wp %x lp %x\n", h, m, wp, lp);
    if (m == 0x113 && lp) { uint32_t args[4] = { h, m, wp, rt_ticks_ms() }; return rt_call_guest(lp, 0, 4, args); }
    if (m == 0x0f) { wnd *w = w_get(h); if (w) { uint32_t r = send(h, m, wp, lp); w->invalid = 0; return r; } }
    return send(h, m, wp, lp);
}
U(GetMessageTime, 0) { UNUSED; return last_msg_time; }
U(TranslateAcceleratorA, 3) { UNUSED; return 0; }

U(SetTimer, 4)
{
    for (int k = 0; k < 32; k++) if (T[k].used && T[k].hwnd == a[0] && T[k].id == a[1]) T[k].used = 0;
    for (int k = 0; k < 32; k++) if (!T[k].used) {
        T[k].used = 1; T[k].hwnd = a[0]; T[k].id = a[0] ? a[1] : 0x100 + k; T[k].ms = a[2] ? a[2] : 1; T[k].proc = a[3];
        T[k].next = rt_ticks_ms() + T[k].ms;
        rt_log("SetTimer(%x, %u, %u ms, proc %08x)\n", a[0], T[k].id, a[2], a[3]);
        return T[k].id;
    }
    return 0;
}
U(KillTimer, 2) { for (int k = 0; k < 32; k++) if (T[k].used && T[k].hwnd == a[0] && T[k].id == a[1]) T[k].used = 0; return 1; }

U(BeginPaint, 2)
{
    wnd *w = w_get(a[0]);
    extern uint32_t g_window_dc(uint32_t hwnd);
    uint32_t dc = g_window_dc(a[0]);
    uint32_t p = a[1];
    WR32(p, dc); WR32(p + 4, 0); WR32(p + 8, 0); WR32(p + 12, 0);
    int cw = 640, ch = 480; if (w) client_size(w, &cw, &ch);
    WR32(p + 16, (uint32_t)cw); WR32(p + 20, (uint32_t)ch);
    if (w) w->invalid = 0;
    return dc;
}
U(EndPaint, 2) { UNUSED; return 1; }

U(MessageBoxA, 4)
{
    extern void rt_message_box(const char *title, const char *text);
    rt_log("MessageBox: %s: %s\n", GSTR(a[2]), GSTR(a[1]));
    rt_message_box(GSTR(a[2]), GSTR(a[1]));
    return 1;                                          /* IDOK */
}
U(DialogBoxParamA, 5) { rt_log("DialogBoxParamA(template %08x) not supported\n", a[1]); return (uint32_t)-1; }

/* skip a template string/ordinal field (WORD array): 0x0000 none, 0xFFFF + ordinal, or a NUL-terminated wide string */
static uint32_t dlg_skip_sz(uint32_t p)
{
    if (RD16(p) == 0xffff) return p + 4;
    while (RD16(p)) p += 2;
    return p + 2;
}


/* modal dialog from an in-memory DLGTEMPLATE. 3-D Ultra Pinball's dialogs have no controls: the dialog
 * procedure paints a picture and handles clicks itself. Controls in a template are logged and ignored. */
U(DialogBoxIndirectParamA, 5)
{
    uint32_t t = a[1];
    uint32_t style = RD32(t), ex = RD32(t + 4);
    int n = RD16(t + 8);
    int x = (int16_t)RD16(t + 10), y = (int16_t)RD16(t + 12), cx = (int16_t)RD16(t + 14), cy = (int16_t)RD16(t + 16);
    /* dialog units -> pixels with the base units GetDialogBaseUnits reports (8 x 16) */
    int px = x * 8 / 4, py = y * 16 / 8, pw = cx * 8 / 4, ph = cy * 16 / 8;
    uint32_t q = dlg_skip_sz(dlg_skip_sz(t + 18));     /* menu, class */
    char title[128] = ""; { int k = 0; while (RD16(q) && k < 127) { title[k++] = (char)RD16(q); q += 2; } title[k] = 0; }
    rt_log("dialog %s: style %08x %d,%d %dx%d px, %d controls, proc %08x\n", title, style, px, py, pw, ph, n, a[3]);
    if (n) rt_log("  dialog controls are not created\n");
    int k; for (k = 0; k < MAXW && W[k].used; k++) ;
    if (k == MAXW) return (uint32_t)-1;
    wnd *w = &W[k]; memset(w, 0, sizeof *w);
    uint32_t h = HWND_BASE + k;
    w->used = 1; w->cls = -1; w->dlgproc = a[3]; w->style = style & ~0x00c00000u; w->exstyle = ex;
    w->parent = a[2] ? a[2] : main_hwnd;              /* the owner; popups are placed in screen (= main client) coordinates */
    if (style & 0x0800) { px = (640 - pw) / 2; py = (480 - ph) / 2; }   /* DS_CENTER */
    w->x = px; w->y = py; w->w = pw; w->h = ph;
    snprintf(w->text, sizeof w->text, "%s", title);
    uint32_t prev_focus = focus_hwnd, prev_capture = capture_hwnd;
    if (send(h, 0x110, 0, a[4])) focus_hwnd = h;       /* WM_INITDIALOG */
    if (w_get(h) && (style & 0x10000000u)) { w->visible = 1; w->invalid = 1; }
    focus_hwnd = h;
    static uint32_t msgbuf;                            /* MSG for the dialog's own message loop */
    if (!msgbuf) msgbuf = rt_low_alloc(28);
    while (w_get(h) && !W[k].dlg_ended) {
        if (fetch(msgbuf, 0, 0, 0, 1)) {
            if (RD32(msgbuf + 4) == 0x12) { quit_posted = 1; break; }   /* WM_QUIT: leave it for the main loop */
            uint32_t mp[1] = { msgbuf };
            u_TranslateMessage(mp);
            u_DispatchMessageA(mp);
        } else {
            /* queue empty: the owner gets WM_ENTERIDLE (MSGF_DIALOGBOX), which is where the game animates */
            uint32_t owner = W[k].parent;
            if (w_get(owner)) send(owner, 0x121, 0, h);
            extern void rt_idle_wait(int ms);
            rt_idle_wait(1);
        }
    }
    uint32_t r = w_get(h) ? W[k].dlg_result : 2;
    rt_log("dialog %s ended: %d\n", title, (int)r);
    if (w_get(h)) {
        send(h, 0x02, 0, 0);                           /* WM_DESTROY */
        W[k].used = 0;
    }
    focus_hwnd = prev_focus; capture_hwnd = prev_capture;
    wnd *ow = w_get(main_hwnd); if (ow) ow->invalid = 1;
    return r;
}
U(EndDialog, 2)
{
    wnd *w = w_get(a[0]);
    if (!w || !w->dlgproc) return 0;
    w->dlg_ended = 1; w->dlg_result = a[1];
    return 1;
}

/* menus, icons, cursors, scroll bars: accepted and ignored */
static uint32_t dummy_obj(int t) { return h_new(t); }
U(LoadMenuA, 2) { UNUSED; return dummy_obj(H_MENU); }
U(GetMenu, 1) { UNUSED; return 0; }
U(DestroyMenu, 1) { h_free(a[0]); return 1; }
U(EnableMenuItem, 3) { UNUSED; return 0; }
U(CheckMenuItem, 3) { UNUSED; return 0; }
U(LoadIconA, 2) { UNUSED; return dummy_obj(H_ICON); }
U(LoadCursorA, 2) { UNUSED; static uint32_t c; if (!c) c = dummy_obj(H_CURSOR); return c; }
U(LoadAcceleratorsA, 2) { UNUSED; return dummy_obj(H_ACCEL); }
static uint32_t cur_cursor = 1;
U(SetCursor, 1)
{
    extern void rt_show_cursor(int on);
    uint32_t o = cur_cursor; cur_cursor = a[0]; rt_show_cursor(a[0] != 0); return o;
}
U(GetScrollRange, 4) { WR32(a[2], 0); WR32(a[3], 0); return 1; }
U(SetScrollRange, 5) { UNUSED; return 1; }
U(SetScrollPos, 4) { UNUSED; return 0; }

/* input */
U(GetAsyncKeyState, 1) { static uint32_t seen[8]; if (!(seen[(a[0] & 255) >> 5] & (1u << (a[0] & 31)))) { seen[(a[0] & 255) >> 5] |= 1u << (a[0] & 31); ITRACE("GetAsyncKeyState(%02x) first use\n", a[0]); }
    extern int rt_key_polled(int vk); return rt_key_polled((int)a[0]) ? 0x8000 : 0; }
U(GetKeyState, 1) { static uint32_t seen[8]; if (!(seen[(a[0] & 255) >> 5] & (1u << (a[0] & 31)))) { seen[(a[0] & 255) >> 5] |= 1u << (a[0] & 31); ITRACE("GetKeyState(%02x) first use\n", a[0]); }
    extern int rt_key_polled(int vk); return rt_key_polled((int)a[0]) ? 0xff80 : 0; }
U(GetCursorPos, 1) { WR32(a[0], (uint32_t)mouse_x); WR32(a[0] + 4, (uint32_t)mouse_y); return 1; }
U(SetCursorPos, 2)
{
    extern void rt_warp_mouse(int x, int y);
    mouse_x = (int32_t)a[0]; mouse_y = (int32_t)a[1]; rt_warp_mouse(mouse_x, mouse_y); return 1;
}
U(ClientToScreen, 2) { int x, y; client_origin(a[0], &x, &y); WR32(a[1], RD32(a[1]) + x); WR32(a[1] + 4, RD32(a[1] + 4) + y); return 1; }
U(ScreenToClient, 2) { int x, y; client_origin(a[0], &x, &y); WR32(a[1], RD32(a[1]) - x); WR32(a[1] + 4, RD32(a[1] + 4) - y); return 1; }
U(SetCapture, 1) { uint32_t o = capture_hwnd; capture_hwnd = a[0]; ITRACE("SetCapture(%x)\n", a[0]); return o; }
U(ReleaseCapture, 0) { UNUSED; ITRACE("ReleaseCapture\n"); capture_hwnd = 0; return 1; }
U(GetCapture, 0) { UNUSED; return capture_hwnd; }

/* wsprintfA (cdecl): a[0] buffer, a[1] format, a[2..] arguments */
U(wsprintfA, 0)
{
    const char *f = GSTR(a[1]);
    char out[1024]; size_t o = 0;
    uint32_t argp = ESP + 8;                           /* the third argument (thunk: ret popped, a[] at ESP) */
    while (*f && o < sizeof out - 1) {
        if (*f != '%') { out[o++] = *f++; continue; }
        f++;
        if (*f == '%') { out[o++] = '%'; f++; continue; }
        char spec[32] = "%"; size_t s = 1;
        while (*f && strchr("-+ #0", *f) && s < 20) spec[s++] = *f++;
        while (*f && isdigit((unsigned char)*f) && s < 26) spec[s++] = *f++;
        if (*f == '.') { spec[s++] = *f++; while (*f && isdigit((unsigned char)*f) && s < 28) spec[s++] = *f++; }
        while (*f == 'l' || *f == 'h') f++;
        char c = *f ? *f++ : 'd';
        char tmp[512];
        uint32_t v = RD32(argp); argp += 4;
        if (c == 's') { spec[s++] = 's'; spec[s] = 0; snprintf(tmp, sizeof tmp, spec, GSTR(v)); }
        else if (c == 'c') { spec[s++] = 'c'; spec[s] = 0; snprintf(tmp, sizeof tmp, spec, (int)(v & 0xff)); }
        else if (c == 'd' || c == 'i') { spec[s++] = 'd'; spec[s] = 0; snprintf(tmp, sizeof tmp, spec, (int32_t)v); }
        else if (c == 'u') { spec[s++] = 'u'; spec[s] = 0; snprintf(tmp, sizeof tmp, spec, v); }
        else if (c == 'x' || c == 'X') { spec[s++] = c; spec[s] = 0; snprintf(tmp, sizeof tmp, spec, v); }
        else if (c == 'p') { snprintf(tmp, sizeof tmp, "%08X", v); }
        else { tmp[0] = 0; }
        for (char *t = tmp; *t && o < sizeof out - 1; t++) out[o++] = *t;
    }
    out[o] = 0;
    memcpy(GPTR(a[0]), out, o + 1);
    return (uint32_t)o;
}

/* ======================================================================== */
/* GDI32                                                                     */
/* ======================================================================== */
static int trace_gdi = -1;
#define GTRACE(...) do { if (trace_gdi < 0) trace_gdi = getenv("SIERRA_TRACE_GDI") != NULL; if (trace_gdi) rt_log(__VA_ARGS__); } while (0)

/* The display is a 256-colour one, like the Windows 95 setups the game was made for: the screen holds palette
 * indices and shows them through the realized palette, so realizing a new palette (fades, flashes) recolours
 * what is already on screen without redrawing it. */
static uint32_t realized[256];                         /* system palette, 0x00RRGGBB */
static uint8_t *screen8; static int sw, sh;            /* the main window's client area, palette indices */
static uint32_t *screen; static int screen_dirty;      /* converted for presentation */
static uint32_t present_at;

uint32_t g_window_dc(uint32_t hwnd)
{
    wnd *w = w_get(hwnd ? hwnd : main_hwnd);
    if (!w) { static uint32_t desk; if (!desk) desk = h_new(H_DC); return desk; }   /* desktop DC */
    if (!w->dc || !h_get(w->dc, H_DC)) { w->dc = h_new(H_DC); }
    h_get(w->dc, H_DC)->hwnd = hwnd ? hwnd : main_hwnd;
    return w->dc;
}
static int is_window_dc(const gobj *d) { return d && d->hwnd != 0; }

static void ensure_screen(void)
{
    wnd *w = w_get(main_hwnd);
    int nw = w ? w->w : 640, nh = w ? w->h : 480;
    if (nw != sw || nh != sh || !screen8) {
        free(screen8); free(screen); sw = nw; sh = nh;
        screen8 = (uint8_t *)calloc((size_t)sw * sh, 1); screen = (uint32_t *)calloc((size_t)sw * sh, 4);
    }
}

void rt_debug_screen(void)
{
    int nz = 0, pz = 0;
    if (screen8) for (size_t k = 0, n = (size_t)sw * sh; k < n; k++) nz += screen8[k] != 0;
    for (int k = 0; k < 256; k++) pz += realized[k] != 0;
    rt_log("screen %dx%d: %d non-zero pixels, %d non-black palette entries, t=%u\n", sw, sh, nz, pz, rt_ticks_ms());
}

void rt_present_if_dirty(int force)
{
    uint32_t now = rt_ticks_ms();
    if (!screen8 || !screen_dirty) return;
    if (!force && now - present_at < 8) return;
    extern void rt_video_frame(const uint32_t *rgb, int w, int h);
    for (size_t k = 0, n = (size_t)sw * sh; k < n; k++) screen[k] = realized[screen8[k]] | 0xff000000u;
    rt_video_frame(screen, sw, sh);
    screen_dirty = 0; present_at = now;
}

U(GetDC, 1) { return g_window_dc(a[0]); }
U(ReleaseDC, 2) { UNUSED; return 1; }

static uint32_t stock[20];
static uint32_t g_GetStockObject(const uint32_t *a) { uint32_t i = a[0] % 20; if (!stock[i]) stock[i] = h_new(H_STOCK); return stock[i]; }
static uint32_t g_CreateSolidBrush(const uint32_t *a) { UNUSED; return h_new(H_BRUSH); }
static uint32_t g_SetTextColor(const uint32_t *a) { UNUSED; return 0; }
static uint32_t g_SetBkColor(const uint32_t *a) { UNUSED; return 0xffffff; }
static uint32_t g_CreateCompatibleDC(const uint32_t *a) { UNUSED; return h_new(H_DC); }
static uint32_t g_DeleteDC(const uint32_t *a) { gobj *d = h_get(a[0], H_DC); if (d && !is_window_dc(d)) h_free(a[0]); return 1; }
static uint32_t g_DeleteObject(const uint32_t *a)
{
    gobj *g = h_get(a[0], 0);
    if (!g || g->type == H_STOCK) return 1;
    if (g->type == H_BITMAP && g->bits) rt_free(g->bits);
    h_free(a[0]);
    return 1;
}

static uint32_t g_CreateDIBSection(const uint32_t *a)
{
    uint32_t bi = a[1];
    int w = (int32_t)RD32(bi + 4), h = (int32_t)RD32(bi + 8), bpp = RD16(bi + 14);
    uint32_t hsz = RD32(bi), ncol = RD32(bi + 32);
    if (bpp != 8) rt_log("CreateDIBSection: %d bpp not supported yet\n", bpp);
    uint32_t hb = h_new(H_BITMAP);
    gobj *g = h_get(hb, H_BITMAP);
    g->w = w; g->h = h < 0 ? -h : h; g->topdown = h < 0; g->bpp = bpp;
    g->pitch = ((w * bpp + 31) / 32) * 4;
    g->bits = rt_alloc((uint32_t)(g->pitch * g->h), 16);
    g->palcolors = a[2] == 1;
    if (!ncol) ncol = 256;
    for (uint32_t k = 0; k < 256 && k < ncol; k++) {
        if (g->palcolors) g->pidx[k] = RD16(bi + hsz + 2 * k);
        else { uint32_t q = RD32(bi + hsz + 4 * k); g->ctab[k] = q & 0xffffff; }
    }
    if (a[3]) WR32(a[3], g->bits);
    rt_log("CreateDIBSection %dx%d %dbpp %s -> %x bits %08x\n", g->w, g->h, bpp, g->palcolors ? "PAL" : "RGB", hb, g->bits);
    return hb;
}

static uint32_t g_SelectObject(const uint32_t *a)
{
    gobj *dc = h_get(a[0], H_DC), *o = h_get(a[1], 0);
    if (!dc || !o) return 0;
    uint32_t old = 0;
    if (o->type == H_BITMAP) { old = dc->bitmap; dc->bitmap = a[1]; }
    else if (o->type == H_BRUSH || o->type == H_STOCK) { old = dc->brush; dc->brush = a[1]; }
    if (!old) old = g_GetStockObject((const uint32_t[]){ 0 });
    return old;
}

static uint32_t g_CreatePalette(const uint32_t *a)
{
    uint32_t lp = a[0];
    uint32_t hp = h_new(H_PALETTE);
    gobj *g = h_get(hp, H_PALETTE);
    g->npal = RD16(lp + 2); if (g->npal > 256) g->npal = 256;
    for (int k = 0; k < g->npal; k++) { uint32_t e = RD32(lp + 4 + 4 * k); g->pal[k] = ((e & 0xff) << 16) | (e & 0xff00) | ((e >> 16) & 0xff); }
    return hp;
}
static uint32_t g_SelectPalette(const uint32_t *a)
{
    gobj *dc = h_get(a[0], H_DC); if (!dc) return 0;
    uint32_t old = dc->palette; dc->palette = a[1];
    return old ? old : g_GetStockObject((const uint32_t[]){ 15 });
}
static uint32_t g_RealizePalette(const uint32_t *a)
{
    gobj *dc = h_get(a[0], H_DC); if (!dc) return 0;
    gobj *p = h_get(dc->palette, H_PALETTE);
    if (!p) return 0;
    if (is_window_dc(dc)) { for (int k = 0; k < p->npal; k++) realized[k] = p->pal[k]; screen_dirty = 1; }
    GTRACE("RealizePalette dc %x pal %x n %d [1]=%06x [128]=%06x t=%u\n", a[0], dc->palette, p->npal, p->pal[1], p->pal[128 % (p->npal ? p->npal : 1)], rt_ticks_ms());
    return (uint32_t)p->npal;
}
static uint32_t g_GetSystemPaletteEntries(const uint32_t *a)
{
    if (!a[3]) return 256;
    for (uint32_t k = 0; k < a[2] && a[1] + k < 256; k++) {
        uint32_t c = realized[a[1] + k];
        WR32(a[3] + 4 * k, ((c >> 16) & 0xff) | (c & 0xff00) | ((c & 0xff) << 16));
    }
    return a[2];
}
static uint32_t g_GetSystemPaletteUse(const uint32_t *a) { UNUSED; return 1; }
static uint32_t g_SetDIBColorTable(const uint32_t *a)
{
    gobj *dc = h_get(a[0], H_DC); gobj *b = dc ? h_get(dc->bitmap, H_BITMAP) : NULL;
    if (!b) return 0;
    for (uint32_t k = 0; k < a[2] && a[1] + k < 256; k++) b->ctab[a[1] + k] = RD32(a[3] + 4 * k) & 0xffffff;
    b->palcolors = 0;
    return a[2];
}
static uint32_t g_GetDIBColorTable(const uint32_t *a)
{
    gobj *dc = h_get(a[0], H_DC); gobj *b = dc ? h_get(dc->bitmap, H_BITMAP) : NULL;
    if (!b) return 0;
    for (uint32_t k = 0; k < a[2] && a[1] + k < 256; k++) WR32(a[3] + 4 * k, b->ctab[a[1] + k]);
    return a[2];
}

static inline uint8_t *dib_row(gobj *b, int y)
{
    int r = b->topdown ? y : b->h - 1 - y;
    return (uint8_t *)GPTR(b->bits + (uint32_t)(r * b->pitch));
}

/* source colours -> system palette indices */
static void src_to_index(gobj *b, gobj *dc, uint8_t *map)
{
    if (b->palcolors) {
        /* DIB_PAL_COLORS: entries are indices into the logical palette, which the game builds to match the
         * system palette entry for entry (PC_NOCOLLAPSE) */
        for (int k = 0; k < 256; k++) map[k] = (uint8_t)b->pidx[k];
        (void)dc;
        return;
    }
    for (int k = 0; k < 256; k++) {                    /* RGB colour table: nearest system colour */
        uint32_t c = b->ctab[k]; int best = 0; long bd = 1L << 30;
        int r = (c >> 16) & 255, g = (c >> 8) & 255, bl = c & 255;
        for (int j = 0; j < 256; j++) {
            uint32_t q = realized[j];
            long dr = r - (int)((q >> 16) & 255), dg = g - (int)((q >> 8) & 255), db = bl - (int)(q & 255);
            long d = dr * dr + dg * dg + db * db;
            if (d < bd) { bd = d; best = j; if (!d) break; }
        }
        map[k] = (uint8_t)best;
    }
}

/* blit (with optional stretch) from a DIB-section DC to a window DC or another DIB section */
static uint32_t blit(uint32_t ddc, int dx, int dy, int dw, int dh, uint32_t sdc, int sx, int sy, int sw_, int sh_)
{
    gobj *s = h_get(sdc, H_DC), *d = h_get(ddc, H_DC);
    gobj *sb = s ? h_get(s->bitmap, H_BITMAP) : NULL;
    GTRACE("blit dc %x (%s, hwnd %x) %d,%d %dx%d <- dc %x bmp %x %d,%d %dx%d\n", ddc, is_window_dc(d) ? "window" : "memory",
           d ? d->hwnd : 0, dx, dy, dw, dh, sdc, s ? s->bitmap : 0, sx, sy, sw_, sh_);
    if (!d || dw <= 0 || dh <= 0) return d != NULL;
    if (!sb) return 1;                                 /* fills from the window DC etc.: ignore */
    if (sb->bpp != 8 || sw_ == 0 || sh_ == 0) return 1;
    if (is_window_dc(d)) {
        ensure_screen();
        uint8_t map[256]; src_to_index(sb, s, map);
        if (trace_gdi > 0) { const uint8_t *r0 = dib_row(sb, sb->h / 2);
            rt_log("  map %d %d %d %d .. pix %d %d %d %d, realized[pix]=%06x palcolors %d\n", map[0], map[1], map[2], map[200],
                   r0[0], r0[sb->w / 3], r0[sb->w / 2], r0[sb->w - 1], realized[map[r0[sb->w / 2]]], sb->palcolors); }
        int ox, oy, cw = sw, ch = sh;
        client_origin(d->hwnd, &ox, &oy);
        wnd *w = w_get(d->hwnd);
        if (w && d->hwnd != main_hwnd) client_size(w, &cw, &ch);
        for (int y = 0; y < dh; y++) {
            int cy = dy + y; if (cy < 0 || cy >= ch) continue;
            int ty = oy + cy; if (ty < 0 || ty >= sh) continue;
            int syy = sy + (int)((int64_t)y * sh_ / dh); if (syy < 0 || syy >= sb->h) continue;
            const uint8_t *row = dib_row(sb, syy);
            uint8_t *o = screen8 + (size_t)ty * sw;
            for (int x = 0; x < dw; x++) {
                int cx = dx + x; if (cx < 0 || cx >= cw) continue;
                int tx = ox + cx; if (tx < 0 || tx >= sw) continue;
                int sxx = sx + (int)((int64_t)x * sw_ / dw); if (sxx < 0 || sxx >= sb->w) continue;
                o[tx] = map[row[sxx]];
            }
        }
        screen_dirty = 1;
        stat_blits++;
        rt_present_if_dirty(0);
        return 1;
    }
    gobj *db = h_get(d->bitmap, H_BITMAP);
    if (!db || db->bpp != 8) return 1;
    for (int y = 0; y < dh; y++) {
        int ty = dy + y; if (ty < 0 || ty >= db->h) continue;
        int syy = sy + (int)((int64_t)y * sh_ / dh); if (syy < 0 || syy >= sb->h) continue;
        const uint8_t *srow = dib_row(sb, syy); uint8_t *drow = dib_row(db, ty);
        for (int x = 0; x < dw; x++) {
            int tx = dx + x; if (tx < 0 || tx >= db->w) continue;
            int sxx = sx + (int)((int64_t)x * sw_ / dw); if (sxx < 0 || sxx >= sb->w) continue;
            drow[tx] = srow[sxx];
        }
    }
    return 1;
}
static uint32_t g_BitBlt(const uint32_t *a)
{
    return blit(a[0], (int32_t)a[1], (int32_t)a[2], (int32_t)a[3], (int32_t)a[4], a[5], (int32_t)a[6], (int32_t)a[7], (int32_t)a[3], (int32_t)a[4]);
}
static uint32_t g_StretchBlt(const uint32_t *a)
{
    return blit(a[0], (int32_t)a[1], (int32_t)a[2], (int32_t)a[3], (int32_t)a[4], a[5], (int32_t)a[6], (int32_t)a[7], (int32_t)a[8], (int32_t)a[9]);
}

/* ======================================================================== */
/* WINMM                                                                     */
/* ======================================================================== */
static uint32_t w_timeGetTime(const uint32_t *a) { UNUSED; return rt_ticks_ms(); }
static uint32_t w_mciSendStringA(const uint32_t *a)
{
    const char *cmd = GSTR(a[0]);
    rt_log("mciSendString(%s)\n", cmd);
    if (a[1] && a[2]) rt_put_str(a[1], "", a[2]);
    return 263;                                        /* MCIERR_INVALID_DEVICE_NAME: no CD audio, no AVI player */
}

/* ======================================================================== */
#define NK(n, c) { "kernel32", #n, k_##n, c, 0 }
#define NU(n, c) { "user32", #n, u_##n, c, 0 }
#define NG(n, c) { "gdi32", #n, g_##n, c, 0 }
static const rt_native natives[] = {
    NK(GetVersion, 0), NK(SetLastError, 1), NK(GetLastError, 0), NK(GetLogicalDrives, 0), NK(SetHandleCount, 1),
    NK(GetCurrentThreadId, 0), NK(SetConsoleCtrlHandler, 2), NK(UnhandledExceptionFilter, 1), NK(WaitForSingleObject, 2),
    NK(ReleaseMutex, 1), NK(CreateMutexA, 3), NK(ExitProcess, 1), NK(RaiseException, 4), NK(RtlUnwind, 4),
    NK(GetModuleHandleA, 1), NK(GetModuleFileNameA, 3), NK(GetCommandLineA, 0), NK(GetEnvironmentStrings, 0),
    NK(GetEnvironmentVariableA, 3), NK(GetStartupInfoA, 1), NK(GetStdHandle, 1), NK(GetFileType, 1),
    NK(GetLocalTime, 1), NK(GetTimeZoneInformation, 1), NK(GlobalAlloc, 2), NK(GlobalLock, 1), NK(GlobalUnlock, 1),
    NK(GlobalHandle, 1), NK(GlobalFree, 1), NK(VirtualAlloc, 4), NK(VirtualFree, 3), NK(CreateFileA, 7), NK(_lopen, 2),
    NK(CloseHandle, 1), NK(_lclose, 1), NK(ReadFile, 5), NK(_hread, 3), NK(WriteFile, 5), NK(FlushFileBuffers, 1),
    NK(SetFilePointer, 4), NK(_llseek, 3), NK(GetFileSize, 2), NK(GetFileAttributesA, 1), NK(DeleteFileA, 1),
    NK(MoveFileA, 2), NK(SetCurrentDirectoryA, 1), NK(GetCurrentDirectoryA, 2), NK(CreateProcessA, 10),
    NK(GetPrivateProfileStringA, 6), NK(WritePrivateProfileStringA, 4), NK(LoadLibraryA, 1), NK(FreeLibrary, 1),
    NK(GetProcAddress, 2),
    NU(RegisterClassA, 1), NU(CreateWindowExA, 12), NU(ShowWindow, 2), NU(UpdateWindow, 1), NU(InvalidateRect, 3),
    NU(DestroyWindow, 1), NU(MoveWindow, 6), NU(GetWindowRect, 2), NU(GetClientRect, 2), NU(GetWindowPlacement, 2),
    NU(SetWindowPlacement, 2), NU(GetWindowTextA, 3), NU(SetWindowTextA, 2), NU(GetWindowLongA, 2), NU(GetClassLongA, 2),
    NU(DefDlgProcA, 4), NU(GetParent, 1), NU(IsChild, 2), NU(IsIconic, 1), NU(GetWindow, 2), NU(GetTopWindow, 1), NU(GetActiveWindow, 0),
    NU(GetLastActivePopup, 1), NU(GetFocus, 0), NU(SetFocus, 1), NU(SetForegroundWindow, 1), NU(BringWindowToTop, 1),
    NU(EnableWindow, 2), NU(FindWindowA, 2), NU(GetDlgCtrlID, 1), NU(ChildWindowFromPoint, 3), NU(GetDialogBaseUnits, 0),
    NU(GetSystemMetrics, 1), NU(EnumThreadWindows, 3), NU(DefWindowProcA, 4), NU(PostMessageA, 4), NU(SendMessageA, 4),
    NU(PostQuitMessage, 1), NU(PeekMessageA, 5), NU(GetMessageA, 4), NU(TranslateMessage, 1), NU(DispatchMessageA, 1),
    NU(GetMessageTime, 0), NU(TranslateAcceleratorA, 3), NU(SetTimer, 4), NU(KillTimer, 2), NU(BeginPaint, 2),
    NU(EndPaint, 2), NU(MessageBoxA, 4), NU(DialogBoxParamA, 5), NU(DialogBoxIndirectParamA, 5), NU(EndDialog, 2),
    NU(LoadMenuA, 2), NU(GetMenu, 1), NU(DestroyMenu, 1), NU(EnableMenuItem, 3), NU(CheckMenuItem, 3), NU(LoadIconA, 2),
    NU(LoadCursorA, 2), NU(LoadAcceleratorsA, 2), NU(SetCursor, 1), NU(GetScrollRange, 4), NU(SetScrollRange, 5),
    NU(SetScrollPos, 4), NU(GetAsyncKeyState, 1), NU(GetKeyState, 1), NU(GetCursorPos, 1), NU(SetCursorPos, 2),
    NU(ClientToScreen, 2), NU(ScreenToClient, 2), NU(SetCapture, 1), NU(ReleaseCapture, 0), NU(GetCapture, 0),
    NU(GetDC, 1), NU(ReleaseDC, 2),
    { "user32", "wsprintfA", u_wsprintfA, 0, 1 },
    NG(GetStockObject, 1), NG(CreateSolidBrush, 1), NG(SetTextColor, 2), NG(SetBkColor, 2), NG(CreateCompatibleDC, 1),
    NG(DeleteDC, 1), NG(DeleteObject, 1), NG(CreateDIBSection, 6), NG(SelectObject, 2), NG(CreatePalette, 1),
    NG(SelectPalette, 3), NG(RealizePalette, 1), NG(GetSystemPaletteEntries, 4), NG(GetSystemPaletteUse, 1),
    NG(SetDIBColorTable, 4), NG(GetDIBColorTable, 4), NG(BitBlt, 9), NG(StretchBlt, 11),
    { "winmm", "timeGetTime", w_timeGetTime, 0, 0 },
    { "winmm", "mciSendStringA", w_mciSendStringA, 4, 0 },
    { NULL, NULL, NULL, 0, 0 }
};

void rt_win32_init(void)
{
    rt_register_natives(natives);
    rt_register_natives(rt_sos_natives);
}
