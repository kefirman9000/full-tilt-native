/*
 * rt_mem.c - guest address space, heap, loading the original executable image, import thunks,
 * file name mapping and logging.
 */
#include "rt_internal.h"
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
/* windows.h has its own GPTR (a GlobalAlloc flag): use the runtime's guest-pointer macro */
#undef GPTR
#define GPTR(a) ((void *)(M + ((uint32_t)(a) & GUEST_MASK)))
#define mkdir_(p) _mkdir(p)
#else
#include <dirent.h>
#include <sys/mman.h>
#define mkdir_(p) mkdir(p, 0755)
#endif

char rt_game_dir[1024];
char rt_save_dir[1024];

/* ------------------------------------------------------------------------ */
/* logging                                                                   */
/* ------------------------------------------------------------------------ */
static FILE *logf_;
void rt_log(const char *fmt, ...)
{
    va_list ap;
    if (!logf_) logf_ = rt_open_save("sierra.log", "w");
    va_start(ap, fmt);
    if (logf_) { vfprintf(logf_, fmt, ap); fflush(logf_); }
    va_end(ap);
    if (getenv("SIERRA_TRACE")) { va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }
}

/* ------------------------------------------------------------------------ */
/* guest memory                                                              */
/* ------------------------------------------------------------------------ */
typedef struct { uint32_t a, n; } range;
static range *freel; static int nfree, capfree;
static range *used; static int nused, capused;
static uint32_t low_next = LOW_BASE;

void rt_mem_init(void)
{
#ifdef _WIN32
    M = (uint8_t *)VirtualAlloc(NULL, GUEST_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void *p = mmap(NULL, GUEST_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    M = p == MAP_FAILED ? NULL : (uint8_t *)p;
#endif
    if (!M) rt_fatal("Could not reserve memory for the game (%u MB).", GUEST_SIZE >> 20);
    capfree = 64; freel = (range *)malloc(sizeof(range) * capfree);
    freel[0].a = HEAP_BASE; freel[0].n = HEAP_END - HEAP_BASE; nfree = 1;
}

uint32_t rt_low_alloc(uint32_t size)
{
    uint32_t a = (low_next + 15) & ~15u;
    if (a + size > LOW_END) rt_fatal("Runtime scratch memory exhausted.");
    low_next = a + size;
    memset(M + a, 0, size);
    return a;
}

uint32_t rt_guest_strdup(const char *s)
{
    uint32_t n = (uint32_t)strlen(s) + 1, a = rt_low_alloc(n);
    memcpy(M + a, s, n);
    return a;
}

void rt_put_str(uint32_t dst, const char *s, uint32_t max)
{
    if (!max) return;
    size_t n = strlen(s);
    if (n >= max) n = max - 1;
    memcpy(GPTR(dst), s, n); M[(dst + n) & GUEST_MASK] = 0;
}

static void used_add(uint32_t a, uint32_t n)
{
    if (nused == capused) { capused = capused ? capused * 2 : 256; used = (range *)realloc(used, sizeof(range) * capused); }
    used[nused].a = a; used[nused].n = n; nused++;
}

static int used_find(uint32_t a)
{
    for (int k = nused - 1; k >= 0; k--) if (used[k].a == a) return k;
    return -1;
}

uint32_t rt_alloc(uint32_t size, uint32_t align)
{
    if (!size) size = 1;
    size = (size + 15) & ~15u;
    if (align < 16) align = 16;
    for (int k = 0; k < nfree; k++) {
        uint32_t a = (freel[k].a + align - 1) & ~(align - 1);
        uint32_t pad = a - freel[k].a;
        if (freel[k].n < pad + size) continue;
        uint32_t end = freel[k].a + freel[k].n;
        if (pad) {                         /* keep the alignment gap as its own free range */
            freel[k].n = pad;
            if (end > a + size) {
                if (nfree == capfree) { capfree *= 2; freel = (range *)realloc(freel, sizeof(range) * capfree); }
                memmove(freel + k + 2, freel + k + 1, sizeof(range) * (nfree - k - 1));
                freel[k + 1].a = a + size; freel[k + 1].n = end - (a + size); nfree++;
            }
        } else {
            freel[k].a += size; freel[k].n -= size;
            if (!freel[k].n) { memmove(freel + k, freel + k + 1, sizeof(range) * (nfree - k - 1)); nfree--; }
        }
        memset(M + a, 0, size);
        used_add(a, size);
        return a;
    }
    rt_log("out of guest memory (%u bytes)\n", size);
    return 0;
}

uint32_t rt_alloc_size(uint32_t addr)
{
    int k = used_find(addr);
    return k < 0 ? 0 : used[k].n;
}

void rt_free(uint32_t addr)
{
    int k = used_find(addr);
    if (k < 0) return;
    uint32_t a = used[k].a, n = used[k].n;
    used[k] = used[--nused];
    int p = 0;
    while (p < nfree && freel[p].a < a) p++;
    if (nfree == capfree) { capfree *= 2; freel = (range *)realloc(freel, sizeof(range) * capfree); }
    memmove(freel + p + 1, freel + p, sizeof(range) * (nfree - p));
    freel[p].a = a; freel[p].n = n; nfree++;
    if (p + 1 < nfree && freel[p].a + freel[p].n == freel[p + 1].a) {
        freel[p].n += freel[p + 1].n; memmove(freel + p + 1, freel + p + 2, sizeof(range) * (nfree - p - 2)); nfree--;
    }
    if (p > 0 && freel[p - 1].a + freel[p - 1].n == freel[p].a) {
        freel[p - 1].n += freel[p].n; memmove(freel + p, freel + p + 1, sizeof(range) * (nfree - p - 1)); nfree--;
    }
}

uint32_t rt_realloc(uint32_t addr, uint32_t size)
{
    if (!addr) return rt_alloc(size, 16);
    uint32_t old = rt_alloc_size(addr);
    if (old >= size) return addr;
    uint32_t n = rt_alloc(size, 16);
    if (!n) return 0;
    memcpy(M + n, M + addr, old);
    rt_free(addr);
    return n;
}

/* ------------------------------------------------------------------------ */
/* import thunks                                                             */
/* ------------------------------------------------------------------------ */
#define MAXNAT 1024
static const rt_native *natives[MAXNAT]; static int nnat;
static const rt_native *slots[THUNK_SLOTS]; static char *slotname[THUNK_SLOTS]; static uint32_t nslots;

void rt_register_natives(const rt_native *list)
{
    for (; list->name && nnat < MAXNAT; list++) natives[nnat++] = list;
}

static int dll_eq(const char *a, const char *b)
{
    /* compare without case and without a .dll suffix */
    for (; *a && *b; a++, b++) {
        if (*a == '.' && *b == '.') return 1;
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    }
    return (!*a || *a == '.') && (!*b || *b == '.');
}

static uint32_t new_slot(const rt_native *nt, const char *dll, const char *name)
{
    for (uint32_t k = 0; k < nslots; k++)
        if (slots[k] == nt && (nt || !strcmp(slotname[k], name))) return THUNK_BASE + 4 * k;
    if (nslots >= THUNK_SLOTS) rt_fatal("Too many imported functions.");
    slots[nslots] = nt;
    size_t n = strlen(dll) + strlen(name) + 2;
    slotname[nslots] = (char *)malloc(n); snprintf(slotname[nslots], n, "%s!%s", dll, name);
    return THUNK_BASE + 4 * nslots++;
}

uint32_t rt_thunk_for(const char *dll, const char *name)
{
    for (int k = 0; k < nnat; k++)
        if (dll_eq(natives[k]->dll, dll) && !strcmp(natives[k]->name, name))
            return new_slot(natives[k], dll, name);
    return 0;
}

void rt_call_thunk(uint32_t slot)
{
    const rt_native *nt = slot < nslots ? slots[slot] : NULL;
    if (!nt) rt_fatal("The game called %s, which this runtime does not implement yet.",
                      slot < nslots ? slotname[slot] : "an unknown function");
    uint32_t ret = POP32();
    uint32_t a[16];
    for (int k = 0; k < 16; k++) a[k] = RD32(ESP + 4 * k);
    static int trace = -1;
    if (trace < 0) trace = getenv("SIERRA_TRACE_CALLS") != NULL;
    uint32_t r = nt->fn(a);
    if (trace && strcmp(nt->name, "PeekMessageA") && strcmp(nt->name, "timeGetTime") && strcmp(nt->name, "GetAsyncKeyState") && strcmp(nt->name, "GetKeyState"))
        rt_log("call %s(%x, %x, %x, %x) = %x from %08x\n", slotname[slot], a[0], a[1], a[2], a[3], r, ret);
    if (!nt->cdecl_) ESP += 4 * nt->nargs;
    EAX = r;
    EIP = ret;
}

/* ------------------------------------------------------------------------ */
/* loading the original executable                                           */
/* ------------------------------------------------------------------------ */
static uint32_t crc32_(uint32_t crc, const uint8_t *p, size_t n)
{
    static uint32_t t[256];
    if (!t[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1; t[i] = c; }
    crc = ~crc;
    while (n--) crc = t[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

static uint32_t rd32le(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16le(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

uint32_t rt_load_image(const char *path, const rt_program *p)
{
    FILE *f = fopen(path, "rb");
    if (!f) rt_fatal("Could not open %s.", path);
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = (uint8_t *)malloc(n);
    if (fread(d, 1, n, f) != (size_t)n) rt_fatal("Could not read %s.", path);
    fclose(f);
    uint32_t pe = rd32le(d + 0x3c), opt = pe + 24;
    uint16_t nsec = rd16le(d + pe + 6), optsz = rd16le(d + pe + 20);
    uint32_t entry = rd32le(d + opt + 16), base = rd32le(d + opt + 28), size = rd32le(d + opt + 56);
    if (base != p->image_base) rt_fatal("%s is not the expected program (image base %08x).", path, base);
    uint32_t crc = 0;
    for (int s = 0; s < nsec; s++) {
        const uint8_t *h = d + opt + optsz + 40 * s;
        uint32_t vs = rd32le(h + 8), va = rd32le(h + 12), rs = rd32le(h + 16), rp = rd32le(h + 20), ch = rd32le(h + 36);
        uint32_t cnt = rs < (vs ? vs : rs) ? rs : (vs ? vs : rs);
        memcpy(M + base + va, d + rp, cnt);
        if (ch & 0x20) {
            uint32_t sz = vs > rs ? vs : rs;
            crc = crc32_(crc, M + base + va, sz);
        }
    }
    if (crc != p->crc)
        rt_fatal("%s does not match the program this build was recompiled from.\n"
                 "Recompile the tables from your game folder (Compile button in the launcher).", path);
    (void)size;
    /* imports */
    uint32_t irva = rd32le(d + opt + 96 + 8);
    for (uint32_t o = base + irva; RD32(o + 12); o += 20) {
        const char *dll = GSTR(base + RD32(o + 12));
        uint32_t ilt = RD32(o) ? RD32(o) : RD32(o + 16), iat = RD32(o + 16);
        for (uint32_t k = 0;; k++) {
            uint32_t v = RD32(base + ilt + 4 * k);
            if (!v) break;
            char ord[16]; const char *name;
            if (v & 0x80000000u) { snprintf(ord, sizeof ord, "#%u", v & 0xffff); name = ord; }
            else name = GSTR(base + v + 2);
            uint32_t t = rt_thunk_for(dll, name);
            if (!t) { rt_log("import not implemented: %s!%s\n", dll, name); t = new_slot(NULL, dll, name); }
            WR32(base + iat + 4 * k, t);
        }
    }
    free(d);
    return base + entry;
}

/* ------------------------------------------------------------------------ */
/* file names: the game believes it is installed in C:\SIERRA\UPINBALL with the CD in D:.    */
/* Any path is mapped to the user's game folder by its trailing components; files the game   */
/* writes go to the save folder, which is also searched first when reading.                  */
/* ------------------------------------------------------------------------ */
static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }

/* find `name` in `dir` ignoring case; writes the real name into out */
static int find_ci(const char *dir, const char *name, char *out, size_t n)
{
    char p[2048];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    if (exists(p)) { snprintf(out, n, "%s", name); return 1; }
#ifdef _WIN32
    return 0;   /* the Windows file system is case-insensitive already */
#else
    DIR *dd = opendir(dir);
    if (!dd) return 0;
    struct dirent *e; int ok = 0;
    while ((e = readdir(dd))) if (!strcasecmp(e->d_name, name)) { snprintf(out, n, "%s", e->d_name); ok = 1; break; }
    closedir(dd);
    return ok;
#endif
}

static int map_under(const char *root, char **comp, int nc, char *host, size_t n)
{
    char cur[2048], real[512];
    snprintf(cur, sizeof cur, "%s", root);
    for (int k = 0; k < nc; k++) {
        if (!find_ci(cur, comp[k], real, sizeof real)) return 0;
        size_t l = strlen(cur);
        snprintf(cur + l, sizeof cur - l, "/%s", real);
    }
    snprintf(host, n, "%s", cur);
    return 1;
}

int rt_resolve_path(const char *guest, char *host, size_t n, int for_write)
{
    char buf[1024], *comp[64]; int nc = 0;
    snprintf(buf, sizeof buf, "%s", guest);
    char *s = buf;
    if (s[0] && s[1] == ':') s += 2;
    for (char *t = strtok(s, "\\/"); t && nc < 64; t = strtok(NULL, "\\/")) {
        if (!strcmp(t, ".")) continue;
        if (!strcmp(t, "..")) { if (nc) nc--; continue; }
        comp[nc++] = t;
    }
    if (!nc) return 0;
    /* drop leading install folders the original setup used */
    for (int start = 0; start < nc; start++) {
        if (map_under(rt_save_dir, comp + start, nc - start, host, n)) return 1;
        if (!for_write && map_under(rt_game_dir, comp + start, nc - start, host, n)) return 1;
    }
    if (for_write) {
        /* new file: keep only the file name, inside the save folder */
        snprintf(host, n, "%s/%s", rt_save_dir, comp[nc - 1]);
        return 1;
    }
    return 0;
}

FILE *rt_open_save(const char *name, const char *mode)
{
    char p[2048];
    if (!rt_save_dir[0]) return NULL;
    mkdir_(rt_save_dir);
    snprintf(p, sizeof p, "%s/%s", rt_save_dir, name);
    return fopen(p, mode);
}
