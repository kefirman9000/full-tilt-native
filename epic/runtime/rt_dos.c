/* rt_dos.c - the DOS and BIOS services Epic Pinball uses, implemented natively.
 *
 * Files are looked up case-insensitively in the user's game folder. Anything the game writes (settings, high
 * scores) goes to a separate save folder, which is also searched first when reading, so the user's original game
 * files are never modified.
 */
#include "rt_internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <dirent.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static char game_dir[1024], save_dir[1024];

/* ------------------------------------------------------------------------ */
/* helpers                                                                   */
/* ------------------------------------------------------------------------ */
static void read_asciiz(uint16_t seg, uint16_t off, char *out, int max)
{
    int i;
    for (i = 0; i < max - 1; i++)
    {
        char c = (char)RD8(seg, (uint16_t)(off + i));
        if (!c) break;
        out[i] = c;
    }
    out[i] = 0;
}

static void ret_ok(void) { CF = 0; }
static void ret_err(uint16_t code) { CF = 1; AX = code; }

/* Strip drive and directories the game might prepend ("C:\EPIC\EP1.DAT" -> "EP1.DAT"). Epic Pinball keeps
   everything in one folder. */
static const char *base_name(const char *dos)
{
    const char *b = dos, *p;
    for (p = dos; *p; p++)
        if (*p == '\\' || *p == '/' || *p == ':')
            b = p + 1;
    return b;
}

/* find `name` in `dir` ignoring case; writes the real path to out */
static int name_eq_ci(const char *a, const char *b)
{
    while (*a && *b && toupper((unsigned char)*a) == toupper((unsigned char)*b)) { a++; b++; }
    return !*a && !*b;
}

#ifdef _WIN32
/* Windows file names are case-insensitive already; still list the folder so the result has the real spelling */
static int find_ci(const char *dir, const char *name, char *out, size_t outsz)
{
    char pattern[1200];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(pattern, sizeof pattern, "%s\\*", dir);
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do
    {
        if (name_eq_ci(fd.cFileName, name))
        {
            snprintf(out, outsz, "%s\\%s", dir, fd.cFileName);
            FindClose(h);
            return 1;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return 0;
}
#else
static int find_ci(const char *dir, const char *name, char *out, size_t outsz)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    if (!d) return 0;
    while ((e = readdir(d)) != NULL)
    {
        if (name_eq_ci(e->d_name, name))
        {
            snprintf(out, outsz, "%s/%s", dir, e->d_name);
            closedir(d);
            return 1;
        }
    }
    closedir(d);
    return 0;
}
#endif

int rt_find_game_file(const char *dosname, char *out, size_t outsz)
{
    const char *b = base_name(dosname);
    if (find_ci(save_dir, b, out, outsz)) return 1;
    return find_ci(game_dir, b, out, outsz);
}

/* ------------------------------------------------------------------------ */
/* file handles                                                              */
/* ------------------------------------------------------------------------ */
#define MAXH 20
static FILE *handles[MAXH];
static char handle_names[MAXH][128];

static int new_handle(FILE *f)
{
    int h;
    for (h = 5; h < MAXH; h++)
        if (!handles[h]) { handles[h] = f; return h; }
    fclose(f);
    return -1;
}

static void dos_open(int create)
{
    char dos[128], path[1200];
    FILE *f = NULL;
    int h, mode = AL & 3;
    read_asciiz(DS, DX, dos, sizeof dos);
    if (create || mode != 0)
    {
        /* writing: always in the save folder. Opening an existing file read/write copies it there first. */
        char src[1200];
        snprintf(path, sizeof path, "%s/%s", save_dir, base_name(dos));
        {
            /* use the existing spelling in the save folder if present */
            char real[1200];
            if (find_ci(save_dir, base_name(dos), real, sizeof real)) snprintf(path, sizeof path, "%s", real);
        }
        if (create)
            f = fopen(path, "w+b");
        else
        {
            f = fopen(path, "r+b");
            if (!f && find_ci(game_dir, base_name(dos), src, sizeof src))
            {
                FILE *in = fopen(src, "rb");
                if (in)
                {
                    char buf[4096];
                    size_t n;
                    f = fopen(path, "w+b");
                    while (f && (n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, f);
                    fclose(in);
                    if (f) fseek(f, 0, SEEK_SET);
                }
            }
        }
    }
    else if (rt_find_game_file(dos, path, sizeof path))
        f = fopen(path, "rb");
    if (getenv("EPIC_TRACE_FILES"))
        rt_log("open%s \"%s\" mode %d -> %s", create ? "(create)" : "", dos, mode, f ? path : "NOT FOUND");
    if (!f)
    {
        ret_err(2);
        return;
    }
    h = new_handle(f);
    if (h < 0) { ret_err(4); return; }
    snprintf(handle_names[h], sizeof handle_names[h], "%s", base_name(dos));
    AX = (uint16_t)h;
    ret_ok();
}

static FILE *handle(uint16_t h)
{
    return (h < MAXH) ? handles[h] : NULL;
}

/* ------------------------------------------------------------------------ */
/* memory blocks (a small MCB-less allocator over conventional memory)       */
/* ------------------------------------------------------------------------ */
#define ARENA_LO 0x0700u
#define ARENA_HI 0xA000u          /* first segment past conventional memory */
#define MAXBLOCKS 8192
/* Like DOS, every block is preceded by a one-paragraph header (MCB), so blocks occupy [seg-1, seg+paras). The
   list is kept sorted by segment. */
typedef struct { uint16_t seg, paras, owner; } mblock;
static mblock blocks[MAXBLOCKS];
static int nblocks;
static uint16_t cur_psp;

/* largest gap usable for a block (MCB excluded); *at receives the block segment */
static uint16_t largest_free(uint16_t *at)
{
    uint32_t prev_end = ARENA_LO, best = 0, bestseg = 0;
    int i;
    for (i = 0; i <= nblocks; i++)
    {
        uint32_t next_mcb = i < nblocks ? (uint32_t)blocks[i].seg - 1 : ARENA_HI;
        if (next_mcb > prev_end && next_mcb - prev_end - 1 > best)
        {
            best = next_mcb - prev_end - 1;
            bestseg = prev_end + 1;
        }
        if (i < nblocks) prev_end = (uint32_t)blocks[i].seg + blocks[i].paras;
    }
    if (at) *at = (uint16_t)bestseg;
    return (uint16_t)(best > 0xFFFF ? 0xFFFF : best);
}

/* block i may grow to here (exclusive) */
static uint32_t block_limit(int i)
{
    return i + 1 < nblocks ? (uint32_t)blocks[i + 1].seg - 1 : ARENA_HI;
}

/* first fit, like DOS's default strategy */
static int alloc_block(uint16_t paras, uint16_t owner, uint16_t *seg)
{
    uint32_t prev_end = ARENA_LO;
    int i;
    if (nblocks >= MAXBLOCKS) return 0;
    for (i = 0; i <= nblocks; i++)
    {
        uint32_t next_mcb = i < nblocks ? (uint32_t)blocks[i].seg - 1 : ARENA_HI;
        if (next_mcb >= prev_end + 1 + paras)
        {
            memmove(&blocks[i + 1], &blocks[i], (size_t)(nblocks - i) * sizeof blocks[0]);
            blocks[i].seg = (uint16_t)(prev_end + 1);
            blocks[i].paras = paras;
            blocks[i].owner = owner;
            nblocks++;
            *seg = blocks[i].seg;
            return 1;
        }
        if (i < nblocks) prev_end = (uint32_t)blocks[i].seg + blocks[i].paras;
    }
    return 0;
}

/* allocate exactly at the largest gap (program loading takes the biggest block, like DOS) */
static int alloc_largest(uint16_t owner, uint16_t *seg, uint16_t *paras)
{
    uint16_t at, max = largest_free(&at);
    int i;
    if (!max || nblocks >= MAXBLOCKS) return 0;
    for (i = 0; i < nblocks && blocks[i].seg < at; i++) {}
    memmove(&blocks[i + 1], &blocks[i], (size_t)(nblocks - i) * sizeof blocks[0]);
    blocks[i].seg = at; blocks[i].paras = max; blocks[i].owner = owner;
    nblocks++;
    *seg = at; *paras = max;
    return 1;
}

static int find_block(uint16_t seg)
{
    int lo = 0, hi = nblocks - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if (blocks[mid].seg == seg) return mid;
        if (blocks[mid].seg < seg) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

static void remove_block(int i)
{
    memmove(&blocks[i], &blocks[i + 1], (size_t)(nblocks - i - 1) * sizeof blocks[0]);
    nblocks--;
}

static void free_owned(uint16_t owner)
{
    int i;
    for (i = 0; i < nblocks;)
        if (blocks[i].owner == owner) remove_block(i);
        else i++;
}

/* ------------------------------------------------------------------------ */
/* program loading                                                           */
/* ------------------------------------------------------------------------ */
extern const rt_program *const rt_programs[];

const rt_program *rt_find_program(const char *exe_name)
{
    const char *b = base_name(exe_name);
    int i;
    for (i = 0; rt_programs[i]; i++)
    {
        const char *a = rt_programs[i]->name, *c = b;
        /* program names are like "ep1" / "pinball"; match the file's stem */
        while (*a && *c && tolower((unsigned char)*a) == tolower((unsigned char)*c)) { a++; c++; }
        if (!*a && (*c == '.' || !*c))
            return rt_programs[i];
    }
    return NULL;
}

static uint32_t crc32_buf(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    size_t i;
    int k;
    for (i = 0; i < n; i++)
    {
        c ^= p[i];
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

static uint16_t rd16le(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* Load EXE `exe_name` from the game folder and start it. is_child: called from INT 21h/4B (parent keeps running
   afterwards). Returns 0 on success, DOS error code otherwise. */
int rt_exec(const char *exe_name, const char *tail, int is_child)
{
    char path[1200];
    const rt_program *prog = rt_find_program(exe_name);
    FILE *f;
    uint8_t *file;
    long fsz;
    uint16_t hdr, nrel, relo, psp, env, load, paras_needed, maxfree, envsz;
    uint32_t img_size;
    rt_task *t;
    int i, tl;

    if (!prog) { rt_log("exec: %s is not a recompiled program", exe_name); return 2; }
    if (!rt_find_game_file(exe_name, path, sizeof path)) { rt_log("exec: %s not found in the game folder", exe_name); return 2; }
    f = fopen(path, "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END); fsz = ftell(f); fseek(f, 0, SEEK_SET);
    file = (uint8_t *)malloc((size_t)fsz);
    if (!file || fread(file, 1, (size_t)fsz, f) != (size_t)fsz) { fclose(f); free(file); return 5; }
    fclose(f);
    if (fsz < 28 || file[0] != 'M' || file[1] != 'Z') { free(file); return 11; }
    hdr = (uint16_t)(rd16le(file + 8) * 16);
    img_size = (uint32_t)rd16le(file + 4) * 512u - (rd16le(file + 2) ? 512u - rd16le(file + 2) : 0) - hdr;
    nrel = rd16le(file + 6);
    relo = rd16le(file + 0x18);
    if (img_size != prog->image_size || crc32_buf(file + hdr, img_size) != prog->image_crc)
    {
        free(file);
        rt_log("exec: %s does not match the version this build was generated from (re-run the build with your game "
               "folder)", exe_name);
        return 11;
    }

    /* environment block: no variables, then the program path (some programs check their own name) */
    {
        char prog_path[160];
        snprintf(prog_path, sizeof prog_path, "C:\\%s", base_name(exe_name));
        for (i = 0; prog_path[i]; i++) prog_path[i] = (char)toupper((unsigned char)prog_path[i]);
        envsz = (uint16_t)((4 + strlen(prog_path) + 1 + 15) / 16);
        if (!alloc_block(envsz, 0, &env)) { free(file); return 8; }
        memset(&M[env * 16], 0, (size_t)envsz * 16);
        M[env * 16 + 2] = 1;
        memcpy(&M[env * 16 + 4], prog_path, strlen(prog_path) + 1);
    }

    /* the program gets the largest free block, like DOS does for maxalloc=FFFF */
    paras_needed = (uint16_t)(0x10 + (img_size + 15) / 16 + prog->minalloc);
    maxfree = largest_free(NULL);
    if (maxfree < paras_needed) { free(file); return 8; }
    {
        uint16_t want;
        uint32_t wanted = 0x10u + (img_size + 15) / 16 + prog->maxalloc;
        psp = 0; want = 0;
        if (prog->maxalloc == 0xFFFF || wanted >= maxfree)
        {
            if (!alloc_largest(0, &psp, &want)) { free(file); return 8; }
        }
        else
        {
            want = (uint16_t)wanted;
            if (!alloc_block(want, 0, &psp)) { free(file); return 8; }
        }
        blocks[find_block(psp)].owner = psp;
        blocks[find_block(env)].owner = psp;
        load = (uint16_t)(psp + 0x10);

        /* PSP */
        memset(&M[psp * 16], 0, 256);
        M[psp * 16 + 0] = 0xCD; M[psp * 16 + 1] = 0x20;
        {
            uint16_t top = (uint16_t)(psp + want);
            M[psp * 16 + 2] = (uint8_t)top; M[psp * 16 + 3] = (uint8_t)(top >> 8);
        }
        M[psp * 16 + 0x2C] = (uint8_t)env; M[psp * 16 + 0x2D] = (uint8_t)(env >> 8);
        tl = (int)strlen(tail);
        if (tl > 126) tl = 126;
        M[psp * 16 + 0x80] = (uint8_t)tl;
        memcpy(&M[psp * 16 + 0x81], tail, (size_t)tl);
        M[psp * 16 + 0x81 + tl] = 0x0D;

        /* image + relocations */
        memcpy(&M[load * 16], file + hdr, img_size);
        for (i = 0; i < nrel; i++)
        {
            uint16_t off = rd16le(file + relo + i * 4), seg = rd16le(file + relo + i * 4 + 2);
            uint32_t a = (uint32_t)(load + seg) * 16 + off;
            uint16_t v = (uint16_t)(rd16le(&M[a]) + load);
            M[a] = (uint8_t)v; M[a + 1] = (uint8_t)(v >> 8);
        }
        free(file);

        if (rt_ntasks >= 4) return 8;
        if (is_child && rt_ntasks > 0)
            rt_tasks[rt_ntasks - 1].saved = R;    /* parent state, inside its INT 21h frame */
        t = &rt_tasks[rt_ntasks++];
        memset(t, 0, sizeof *t);
        t->prog = prog;
        t->psp = psp;
        t->load = load;
        t->paras = want;
        t->active = 1;
        *prog->load_seg = load;
        cur_psp = psp;

        memset(&R, 0, sizeof R);
        CS = (uint16_t)(load + prog->cs);
        IP = prog->ip;
        SS = (uint16_t)(load + prog->ss);
        SP = prog->sp;
        DS = ES = psp;
        BX = (uint16_t)(img_size >> 16);
        CX = (uint16_t)img_size;
        IFL = 1;
        rt_log("exec: %s loaded at %04X (PSP %04X), tail \"%s\"", prog->name, load, psp, tail);
    }
    return 0;
}

static void hle_return(void);

static void dos_exit(uint8_t code)
{
    rt_task *t = &rt_tasks[rt_ntasks - 1];
    rt_exit_code = code;
    rt_log("exit: %s returned %d", t->prog->name, code);
    free_owned(t->psp);
    rt_ntasks--;
    if (rt_ntasks == 0)
    {
        rt_quit = 1;
        return;
    }
    t = &rt_tasks[rt_ntasks - 1];
    R = t->saved;
    cur_psp = t->psp;
    rt_vga_set_mode(-1);  /* a child may leave the VGA in any state; nothing to do for the parent */
    AX = 0;
    CF = 0;
    hle_return();
}

/* ------------------------------------------------------------------------ */
/* return from a built-in service: like IRET, but keep the status flags the service set */
/* ------------------------------------------------------------------------ */
static void hle_return(void)
{
    uint16_t f;
    IP = POP16();
    CS = POP16();
    f = POP16();
    SETFLAGS((uint16_t)((f & ~0x08D5u) | (GETFLAGS() & 0x08D5u)));
}

/* ------------------------------------------------------------------------ */
/* INT 21h                                                                   */
/* ------------------------------------------------------------------------ */
static int int21(void);
static int int21(void)
{
    if (getenv("EPIC_TRACE_DOS"))
        rt_log("int21 AX=%04X BX=%04X CX=%04X DX=%04X DS=%04X ES=%04X from %04X:%04X", AX, BX, CX, DX, DS, ES,
               RD16(SS, (uint16_t)(SP + 2)), RD16(SS, SP));
    switch (AH)
    {
    case 0x01: case 0x07: case 0x08:                   /* read a key (waits) */
    {
        static int pending_scan = -1;
        uint16_t k;
        if (pending_scan >= 0) { AL = (uint8_t)pending_scan; pending_scan = -1; break; }
        if (!rt_kbd_bios_get(&k, 1))
        {
            /* wait like DOS does: interrupts on, come back here once a key arrives */
            IFL = 1;
            rt_timer_poll();
            rt_idle();
            return 0;
        }
        AL = (uint8_t)k;
        if (AL == 0) pending_scan = k >> 8;           /* extended key: second call returns the scancode */
        if (AH == 0x01 && AL) fputc(AL, stdout);
        break;
    }
    case 0x06:                                         /* direct console I/O */
        if (DL == 0xFF)
        {
            uint16_t k;
            if (rt_kbd_bios_get(&k, 1)) { AL = (uint8_t)k; ZF = 0; }
            else { AL = 0; ZF = 1; }
        }
        else fputc(DL, stdout);
        break;
    case 0x0B: { uint16_t k; AL = rt_kbd_bios_get(&k, 0) ? 0xFF : 0x00; break; }
    case 0x0C:                                         /* flush buffer, then run function AL */
    {
        uint16_t k;
        while (rt_kbd_bios_get(&k, 1)) {}
        if (AL == 0x01 || AL == 0x06 || AL == 0x07 || AL == 0x08 || AL == 0x0A) { AH = AL; return int21(); }
        break;
    }
    case 0x02: fputc(DL, stdout); break;
    case 0x09:
    {
        uint16_t o = DX;
        char c;
        while ((c = (char)RD8(DS, o++)) != '$') fputc(c, stdout);
        fflush(stdout);
        break;
    }
    case 0x19: AL = 2; break;                          /* current drive C: */
    case 0x0E: AL = 26; break;
    case 0x1A: break;                                  /* set DTA */
    case 0x25:                                         /* set vector */
        WR16(0, (uint16_t)(AL * 4), DX);
        WR16(0, (uint16_t)(AL * 4 + 2), DS);
        break;
    case 0x2A: CX = 1994; DH = 6; DL = 1; AL = 3; break; /* date */
    case 0x2C:                                         /* time */
    {
        uint64_t us = rt_now_us();
        CH = (uint8_t)((us / 3600000000ull) % 24); CL = (uint8_t)((us / 60000000ull) % 60);
        DH = (uint8_t)((us / 1000000ull) % 60); DL = (uint8_t)((us / 10000ull) % 100);
        break;
    }
    case 0x30: AX = 0x0005; BX = 0; CX = 0; break;     /* DOS 5.0 */
    case 0x33: DL = 0; break;
    case 0x35:                                         /* get vector */
        BX = RD16(0, (uint16_t)(AL * 4));
        ES = RD16(0, (uint16_t)(AL * 4 + 2));
        break;
    case 0x3B: ret_ok(); break;                        /* chdir */
    case 0x3C: dos_open(1); return 1;
    case 0x3D: dos_open(0); return 1;
    case 0x3E:
    {
        FILE *f = handle(BX);
        if (f) { fclose(f); handles[BX] = NULL; ret_ok(); }
        else if (BX < 5) ret_ok();
        else ret_err(6);
        return 1;
    }
    case 0x3F:
    {
        FILE *f = handle(BX);
        uint16_t i, n = 0;
        long pos0;
        if (!f) { ret_err(6); return 1; }
        pos0 = ftell(f);
        {
            /* a MASI driver module read into memory: remember where, so calls into it reach its recompiled code */
            const rt_program *mp = rt_find_program(handle_names[BX]);
            if (mp && mp->raw && pos0 >= 0 && pos0 < 0x40)
                rt_module_loaded(mp, LIN(DS, DX) - (uint32_t)pos0);
        }
        for (i = 0; i < CX; i++)
        {
            int c = fgetc(f);
            if (c == EOF) break;
            WR8(DS, (uint16_t)(DX + i), (uint8_t)c);
            n++;
        }
        AX = n;
        ret_ok();
        return 1;
    }
    case 0x40:
    {
        uint16_t i;
        if (BX == 1 || BX == 2)
        {
            for (i = 0; i < CX; i++) fputc(RD8(DS, (uint16_t)(DX + i)), stdout);
            AX = CX; ret_ok(); return 1;
        }
        {
            FILE *f = handle(BX);
            if (!f) { ret_err(6); return 1; }
            for (i = 0; i < CX; i++) fputc(RD8(DS, (uint16_t)(DX + i)), f);
            fflush(f);
            AX = CX;
            ret_ok();
        }
        return 1;
    }
    case 0x41:                                         /* delete: only in the save folder */
    {
        char dos[128], path[1200];
        read_asciiz(DS, DX, dos, sizeof dos);
        if (find_ci(save_dir, base_name(dos), path, sizeof path)) remove(path);
        ret_ok();
        return 1;
    }
    case 0x42:
    {
        FILE *f = handle(BX);
        long pos = (long)(int32_t)(((uint32_t)CX << 16) | DX);   /* CX:DX is a signed 32-bit offset */
        if (!f) { ret_err(6); return 1; }
        if (fseek(f, pos, AL == 0 ? SEEK_SET : AL == 1 ? SEEK_CUR : SEEK_END)) { ret_err(25); return 1; }
        pos = ftell(f);
        AX = (uint16_t)pos;
        DX = (uint16_t)(pos >> 16);
        ret_ok();
        return 1;
    }
    case 0x43:                                         /* get/set file attributes: only for existing files */
    {
        char dos[128], path[1200];
        read_asciiz(DS, DX, dos, sizeof dos);
        if (getenv("EPIC_TRACE_FILES")) rt_log("attrib \"%s\"", dos);
        if (!rt_find_game_file(dos, path, sizeof path)) { ret_err(2); return 1; }
        if (AL == 0) CX = 0x20;
        ret_ok();
        return 1;
    }
    case 0x44: DX = (BX < 5) ? 0x80D3 : 0x0002; ret_ok(); return 1; /* ioctl: device info */
    case 0x47: WR8(DS, SI, 0); ret_ok(); return 1;     /* cwd = root */
    case 0x48:
    {
        uint16_t seg;
        if (alloc_block(BX, cur_psp, &seg)) { AX = seg; ret_ok(); }
        else { BX = largest_free(NULL); ret_err(8); }
        return 1;
    }
    case 0x49:
    {
        int i = find_block(ES);
        if (i < 0) { ret_err(9); return 1; }
        remove_block(i);
        ret_ok();
        return 1;
    }
    case 0x4A:
    {
        int i = find_block(ES);
        uint32_t lim;
        if (i < 0) { ret_err(9); return 1; }
        lim = block_limit(i);
        if ((uint32_t)ES + BX > lim) { BX = (uint16_t)(lim - ES); ret_err(8); return 1; }
        blocks[i].paras = BX;
        {
            int k;
            for (k = 0; k < rt_ntasks; k++)
                if (rt_tasks[k].psp == ES) rt_tasks[k].paras = BX;
        }
        ret_ok();
        return 1;
    }
    case 0x4B:
    {
        char name[128], tail[130];
        uint16_t toff, tseg;
        int len, err;
        if (AL != 0) { ret_err(1); return 1; }
        read_asciiz(DS, DX, name, sizeof name);
        toff = RD16(ES, (uint16_t)(BX + 2));
        tseg = RD16(ES, (uint16_t)(BX + 4));
        len = RD8(tseg, toff);
        if (len > 127) len = 127;
        {
            int i;
            for (i = 0; i < len; i++) tail[i] = (char)RD8(tseg, (uint16_t)(toff + 1 + i));
            tail[len] = 0;
        }
        ret_ok();                     /* parent sees success when the child returns */
        if (getenv("EPIC_FORCE_TABLE") && toupper((unsigned char)base_name(name)[0]) == 'E' &&
            toupper((unsigned char)base_name(name)[1]) == 'P' && isdigit((unsigned char)base_name(name)[2]))
            snprintf(name, sizeof name, "EP%s.EXE", getenv("EPIC_FORCE_TABLE"));   /* test aid only */
        err = rt_exec(name, tail, 1);
        if (err) { ret_err((uint16_t)err); return 1; }
        return 0;                     /* now running the child: do not return to the parent */
    }
    case 0x4C: dos_exit(AL); return 0;
    case 0x4D: AX = (uint16_t)rt_exit_code; ret_ok(); return 1;
    case 0x4E: ret_err(18); return 1; /* find first: nothing */
    case 0x4F: ret_err(18); return 1;
    case 0x62: BX = cur_psp; break;
    default:
        rt_log("INT 21h AH=%02X not implemented (AX=%04X)", AH, AX);
        ret_err(1);
        return 1;
    }
    return 1;
}

/* ------------------------------------------------------------------------ */
/* INT 10h / 16h / 1Ah                                                       */
/* ------------------------------------------------------------------------ */
static void int10(void)
{
    if (getenv("EPIC_TRACE_BIOS"))
        rt_log("int10 AX=%04X BX=%04X CX=%04X DX=%04X", AX, BX, CX, DX);
    switch (AH)
    {
    case 0x00: rt_vga_set_mode(AL & 0x7F); break;
    case 0x01: break;                                   /* cursor shape */
    case 0x02: M[0x450] = DL; M[0x451] = DH; break;     /* cursor position */
    case 0x03: DL = M[0x450]; DH = M[0x451]; CX = 0x0607; break;
    case 0x06: case 0x07:                               /* scroll window up / down */
    {
        int top = CH, left = CL, bot = DH, right = DL, n = AL, r, c;
        if (bot > 24) bot = 24;
        if (right > 79) right = 79;
        if (n == 0 || n > bot - top) n = bot - top + 1;
        for (r = (AH == 6 ? top : bot); AH == 6 ? r <= bot : r >= top; r += (AH == 6 ? 1 : -1))
            for (c = left; c <= right; c++)
            {
                int src = AH == 6 ? r + n : r - n;
                uint32_t d = 0xB8000u + (r * 80u + c) * 2u;
                if ((AH == 6 && src <= bot) || (AH == 7 && src >= top))
                {
                    uint32_t sa = 0xB8000u + (src * 80u + c) * 2u;
                    M[d] = M[sa]; M[d + 1] = M[sa + 1];
                }
                else { M[d] = ' '; M[d + 1] = BH; }
            }
        break;
    }
    case 0x08:                                          /* read character and attribute at cursor */
    {
        uint32_t a = 0xB8000u + (M[0x451] * 80u + M[0x450]) * 2u;
        AL = M[a]; AH = M[a + 1];
        break;
    }
    case 0x09: case 0x0A:                               /* write character (and attribute) at cursor */
    {
        uint32_t a = 0xB8000u + (M[0x451] * 80u + M[0x450]) * 2u;
        uint16_t k;
        for (k = 0; k < CX && a < 0xB8000u + 4000u; k++, a += 2)
        {
            M[a] = AL;
            if (AH == 0x09) M[a + 1] = BL;
        }
        break;
    }
    case 0x0F: AL = (uint8_t)rt_vga_mode(); AH = 80; BH = 0; break;
    case 0x10:
        if (AL == 0x10) { rt_vga_out(0x3C8, BL); rt_vga_out(0x3C9, DH); rt_vga_out(0x3C9, CH); rt_vga_out(0x3C9, CL); }
        break;
    case 0x11: case 0x12: case 0x1A:
        if (AH == 0x1A) { AL = 0x1A; BX = 0x0008; }     /* VGA present */
        if (AH == 0x12 && BL == 0x10) { BH = 0; BL = 3; }
        break;
    default:
        rt_log("INT 10h AH=%02X not implemented", AH);
        break;
    }
}

/* returns 0 if the service must be retried (blocking read with an empty buffer) */
static int int16(void)
{
    uint16_t k;
    if (getenv("EPIC_TRACE_BIOS"))
        rt_log("int16 AX=%04X", AX);
    switch (AH)
    {
    case 0x00: case 0x10:
        if (!rt_kbd_bios_get(&k, 1))
        {
            IFL = 1;          /* the BIOS waits with interrupts on */
            rt_timer_poll();
            rt_idle();
            return 0;
        }
        AX = k;
        return 1;
    case 0x01: case 0x11:
        if (rt_kbd_bios_get(&k, 0)) { AX = k; ZF = 0; }
        else ZF = 1;
        return 1;
    case 0x02: AL = 0; return 1;
    default: return 1;
    }
}

void rt_bios_int9(void);

void rt_hle(uint16_t n)
{
    switch (n)
    {
    case 0x08:
    {
        uint32_t ticks = (uint32_t)RD16(0, 0x46C) | ((uint32_t)RD16(0, 0x46E) << 16);
        ticks++;
        WR16(0, 0x46C, (uint16_t)ticks);
        WR16(0, 0x46E, (uint16_t)(ticks >> 16));
        rt_out8(0x20, 0x20);
        if (RD16(0, 0x1C * 4 + 2) != HLE_SEG)
        {
            /* chain to a user INT 1Ch handler; it returns to F000:01FF, which finishes this interrupt */
            rt_int(0x1C, HLE_BASE + 0xFF);
            return;
        }
        break;
    }
    case 0x09: rt_bios_int9(); break;
    case 0x10: int10(); break;
    case 0x11: AX = RD16(0, 0x410); break;
    case 0x12: AX = 640; break;
    case 0x16:
        if (!int16()) return;           /* stay here and try again after interrupts */
        break;
    case 0x1A:
        if (AH == 0) { CX = RD16(0, 0x46E); DX = RD16(0, 0x46C); AL = 0; }
        break;
    case 0x1C: break;
    case 0x20: dos_exit(0); return;
    case 0x21:
        if (!int21()) return;
        break;
    case 0x33: AX = 0; break;           /* no mouse */
    case 0x2F: break;
    case 0xFF: break;                   /* end of a chained INT 1Ch */
    default:
        if (n >= 0x70 && n < 0x78) rt_out8(0x20, 0x20);
        break;
    }
    hle_return();
}

void rt_dos_init(const char *gdir, const char *sdir)
{
    snprintf(game_dir, sizeof game_dir, "%s", gdir);
    snprintf(save_dir, sizeof save_dir, "%s", sdir);
    MKDIR(save_dir);
    nblocks = 0;
}
