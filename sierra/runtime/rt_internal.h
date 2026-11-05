/* rt_internal.h - declarations shared by the Sierra runtime's modules (not used by generated code). */
#ifndef SIERRA_RT_INTERNAL_H
#define SIERRA_RT_INTERNAL_H

#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE                  /* strcasecmp, strdup, mmap flags with -std=c99 */
#endif
#include "sierra_rt.h"
#include <string.h>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strdup _strdup
#else
#include <strings.h>
#endif
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* guest memory layout */
#define LOW_BASE     0x00010000u     /* runtime-owned guest structures (TIB, strings, MSG scratch) */
#define LOW_END      0x00100000u
#define STACK_LO     0x00100000u
#define STACK_TOP    0x003ff000u
#define HEAP_BASE    0x01000000u
#define HEAP_END     0x1fe00000u

/* imported functions: implemented natively; a = the guest's arguments (copied), returns EAX */
typedef uint32_t (*rt_native_fn)(const uint32_t *a);
typedef struct { const char *dll, *name; rt_native_fn fn; int nargs; int cdecl_; } rt_native;
void rt_register_natives(const rt_native *list);
uint32_t rt_thunk_for(const char *dll, const char *name);   /* 0 if not implemented */
void rt_call_thunk(uint32_t slot);
uint32_t rt_load_image(const char *exe_path, const rt_program *p);  /* returns entry point */

/* heap in guest memory */
uint32_t rt_alloc(uint32_t size, uint32_t align);
void rt_free(uint32_t addr);
uint32_t rt_alloc_size(uint32_t addr);
uint32_t rt_realloc(uint32_t addr, uint32_t size);
uint32_t rt_low_alloc(uint32_t size);                  /* permanent runtime-owned guest memory */
uint32_t rt_guest_strdup(const char *s);

/* guest strings */
static inline const char *GSTR(uint32_t a) { return a ? (const char *)GPTR(a) : ""; }
void rt_put_str(uint32_t dst, const char *s, uint32_t max);

/* files */
extern char rt_game_dir[1024];
extern char rt_save_dir[1024];
int rt_resolve_path(const char *guest, char *host, size_t n, int for_write);
FILE *rt_open_save(const char *name, const char *mode);

/* misc */
void rt_log(const char *fmt, ...);
void rt_fatal(const char *fmt, ...);
uint32_t rt_ticks_ms(void);
uint64_t rt_ticks_us(void);
void rt_set_program(const rt_program *p);
void rt_poll_async(void);                 /* timers and sound callbacks; runs guest code */
extern int rt_async_depth;

/* window / video / input (rt_win32.c, rt_main.c) */
void rt_video_present(const uint8_t *pix8, int w, int h, int pitch, const uint32_t *pal, int dx, int dy, int dw, int dh);
void rt_video_init(int w, int h);
void rt_pump_events(void);
void rt_post_input(uint32_t msg, uint32_t wparam, uint32_t lparam);
int rt_key_down(int vk);
extern int rt_quit_requested;

/* sound (rt_sos.c) */
void rt_sos_init(void);
void rt_sos_poll(void);
extern const rt_native rt_sos_natives[];

#ifdef __cplusplus
}
#endif
#endif
