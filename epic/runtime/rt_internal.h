/* rt_internal.h - shared between the runtime's own modules (not used by generated code). */
#ifndef RT_INTERNAL_H
#define RT_INTERNAL_H

#include "epic_rt.h"
#include <stdio.h>

#define HLE_SEG 0xF000u          /* BIOS/DOS services live at F000:0100+n */
#define HLE_BASE 0x0100u

/* ---- loaded programs (PINBALL.EXE stays resident while a table runs) ---- */
typedef struct {
    const rt_program *prog;
    uint16_t psp;            /* PSP segment */
    uint16_t load;           /* image load segment */
    uint16_t paras;          /* size of the memory block owned (PSP + image + heap) */
    rt_cpu saved;            /* parent CPU state while a child runs */
    int active;
} rt_task;

extern rt_task rt_tasks[4];
extern int rt_ntasks;
extern int rt_exit_code;
extern int rt_quit;              /* set to leave the main loop */

/* ---- driver modules the game loads into memory itself (MASI MDRVnnnR.MUS files) ---- */
typedef struct {
    const rt_program *prog;
    uint32_t base;           /* linear address of file offset 0 */
} rt_module;
void rt_module_loaded(const rt_program *prog, uint32_t base);

/* ---- core ---- */
void rt_core_init(void);
void rt_run(void);               /* runs until the outermost program exits or rt_quit */
void rt_fatal(const char *fmt, ...);
void rt_log(const char *fmt, ...);
uint64_t rt_now_us(void);        /* host time in microseconds */

/* interrupts / PIC / PIT */
void rt_raise_irq(int irq);
void rt_timer_poll(void);        /* called often: fires IRQ0 and display retrace by host time */
double rt_pit_hz(void);

/* keyboard controller */
void rt_kbd_push(uint8_t scancode);  /* queue an XT set-1 scancode */
int  rt_kbd_bios_get(uint16_t *key, int remove);  /* BIOS keyboard buffer */

/* ---- DOS / BIOS (rt_dos.c) ---- */
void rt_dos_init(const char *game_dir, const char *save_dir);
void rt_hle(uint16_t n);         /* service interrupt n (CPU is inside the interrupt frame) */
int  rt_exec(const char *exe_name, const char *tail, int is_child);
const rt_program *rt_find_program(const char *exe_name);

/* ---- VGA (rt_vga.c) ---- */
void rt_vga_init(void);
uint8_t rt_vga_in(uint16_t port);
void rt_vga_out(uint16_t port, uint8_t v);
void rt_vga_set_mode(int mode);
int  rt_vga_mode(void);
int  rt_vga_retrace(void);       /* 1 while in vertical retrace (host-time based) */

enum { RT_VIEW_ORIGINAL = 0, RT_VIEW_FULL = 1 };
extern int rt_view;              /* user-selected view */
/* Render the current screen into rgba (w*h), returns 1 if a frame was produced. w and h receive the size,
   *aspect receives the intended display aspect (width / height). */
int rt_vga_render(uint32_t *rgba, int maxw, int maxh, int *w, int *h, double *aspect);

/* ---- front end (rt_main.c) ---- */
void rt_present(void);           /* called at each vertical retrace */
void rt_pump_events(void);
void rt_idle(void);              /* the game is busy-waiting: sleep a little */

#endif
