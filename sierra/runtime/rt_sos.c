/*
 * rt_sos.c - the Human Machine Interfaces "SOS" sound library (SOS9502.DLL / SOS32S02.DLL) that
 * 3-D Ultra Pinball loads at run time, implemented natively on SDL2 audio.
 *
 * All SOS entry points are stdcall; argument counts come from the DLL's own `ret n`.
 */
#include "rt_internal.h"
#include <stdlib.h>

/* ---- timer events: the game's callbacks run on the main thread, between recompiled blocks ---- */
typedef struct { int used, paused; uint32_t rate, proc; uint64_t next_us; } sos_timer;
static sos_timer TM[16];

void rt_poll_async(void)
{
    if (rt_async_depth) return;
    rt_async_depth++;
    uint64_t now = rt_ticks_us();
    for (int k = 0; k < 16; k++) {
        sos_timer *t = &TM[k];
        if (!t->used || t->paused || !t->rate) continue;
        uint64_t per = 1000000u / t->rate;
        int n = 0;
        while (now >= t->next_us && n < 4) {           /* catch up a little after a stall, not forever */
            t->next_us += per; n++;
            rt_call_guest(t->proc, 1, 0, NULL);
        }
        if (now >= t->next_us) t->next_us = now + per;
    }
    rt_sos_poll();
    rt_async_depth--;
}

#define S(name, n) static uint32_t s_##name(const uint32_t *a)
#define LOGARGS(name, n) rt_log("%s(%08x %08x %08x %08x %08x %08x)\n", name, n > 0 ? a[0] : 0, n > 1 ? a[1] : 0, \
                                n > 2 ? a[2] : 0, n > 3 ? a[3] : 0, n > 4 ? a[4] : 0, n > 5 ? a[5] : 0)
#define STUB(name, n, ret) S(name, n) { LOGARGS(#name, n); return ret; }

S(sosTIMERInitSystem, 1) { LOGARGS("sosTIMERInitSystem", 1); return 0; }
S(sosTIMERUnInitSystem, 0) { (void)a; memset(TM, 0, sizeof TM); return 0; }
S(sosTIMERRegisterEvent, 3)
{
    LOGARGS("sosTIMERRegisterEvent", 3);
    for (int k = 0; k < 16; k++) if (!TM[k].used) {
        TM[k].used = 1; TM[k].paused = 0; TM[k].rate = a[0] ? a[0] : 1; TM[k].proc = a[1];
        TM[k].next_us = rt_ticks_us() + 1000000u / TM[k].rate;
        if (a[2]) WR32(a[2], (uint32_t)k);
        return 0;
    }
    return 1;
}
S(sosTIMERAlterEventRate, 2) { if (a[0] < 16) { TM[a[0]].rate = a[1] ? a[1] : 1; } return 0; }
S(sosTIMERRemoveEvent, 1) { if (a[0] < 16) TM[a[0]].used = 0; return 0; }
S(sosTIMERGetEventRate, 2) { if (a[0] < 16 && a[1]) WR32(a[1], TM[a[0]].rate); return 0; }
S(sosTIMERPauseEvent, 1) { if (a[0] < 16) TM[a[0]].paused = 1; return 0; }
S(sosTIMERResumeEvent, 1) { if (a[0] < 16) TM[a[0]].paused = 0; return 0; }

static int trace_snd = -1;
static void dump(const char *what, uint32_t p, int n)
{
    if (trace_snd < 0) trace_snd = getenv("SIERRA_TRACE_SOUND") != NULL;
    if (!p || !trace_snd) return;
    rt_log("  %s @%08x:", what, p);
    for (int k = 0; k < n; k++) rt_log("%s%02x", k % 16 ? " " : "\n    ", RD8(p + k));
    rt_log("\n");
}

/* ---- device detection: one digital device and one General MIDI device ----
 * The game probes MIDI first and only then digital audio; both must be found. The capability structures are
 * written only where the game reads them: the name, the device id (+0x50 for MIDI, 0xffff ends the search) and
 * the accepted formats (+0x4c for digital). */
S(sosMIDIDetectFindFirst, 1)
{
    LOGARGS("sosMIDIDetectFindFirst", 1);
    rt_put_str(a[0], "General MIDI", 32);
    WR16(a[0] + 0x50, 0xffff);
    return 0;
}
S(sosDIGIDetectFindFirst, 1)
{
    LOGARGS("sosDIGIDetectFindFirst", 1);
    dump("caps before", a[0], 0x60);
    rt_put_str(a[0], "Digital audio", 32);
    WR16(a[0] + 0x4c, 0xffff);
    return 0;
}
S(sosDIGIInitDriver, 2)
{
    LOGARGS("sosDIGIInitDriver", 2);
    dump("driver", a[0], 0x80);
    if (a[1]) WR16(a[1], 0);
    return 0;
}
S(sosMIDIInitDriver, 2)
{
    LOGARGS("sosMIDIInitDriver", 2);
    dump("driver", a[0], 0x60);
    if (a[1]) WR16(a[1], 0);
    return 0;
}
static int nsongs;
static uint8_t song_playing[64];
S(sosMIDIInitSong, 2)
{
    int k = nsongs++ & 63;
    song_playing[k] = 0;
    if (a[1]) WR16(a[1], (uint16_t)k);
    return 0;
}

/* ---- digital audio: a software mixer on SDL2 audio ----
 * _SOS_START_SAMPLE as 3-D Ultra Pinball fills it (32-bit SOS for Windows 95):
 *   +0x00 sample data   +0x10 size in bytes   +0x30 volume (left word, right word, 0x7fff = full)
 *   +0x34 loop count (-1 = for ever)   +0x38 rate (Hz)   +0x3c bits (8 unsigned / 16 signed)   +0x40 channels
 * The handle is returned through the third argument (a WORD). */
#include <SDL.h>

#define VOICES 32
#define OUT_RATE 22050
typedef struct {
    int used;
    uint8_t *data; int live;              /* live: read from guest memory (looping buffers the game may refill) */
    uint32_t gaddr, len;                  /* bytes */
    int bits, chans, loops;
    uint64_t pos, step;                   /* source frames, 32.32 fixed point */
    int vl, vr;                           /* 0..0x7fff */
} voice;
static voice V[VOICES];
#define STREAMS 4
#define SQ 32
typedef struct {
    int used, playing, bits, chans, vl, vr;
    uint64_t pos, step;
    struct { uint32_t hdr; uint8_t *data; uint32_t len; } q[SQ];
    int qhead, qtail;
} sos_stream;
static sos_stream ST[STREAMS];
static uint32_t done_q[256]; static int done_head, done_tail;   /* finished buffers (headers), and samples (0x80000000 | handle) */
static uint32_t notify_hwnd;
static SDL_AudioDeviceID audio_dev;
static int audio_paused;

static void mix(void *ud, Uint8 *stream, int bytes)
{
    (void)ud;
    int16_t *out = (int16_t *)stream;
    int frames = bytes / 4;
    static int32_t acc[2 * 4096];
    if (frames > 4096) frames = 4096;
    memset(acc, 0, sizeof(int32_t) * 2 * frames);
    if (!audio_paused) for (int k = 0; k < VOICES; k++) {
        voice *v = &V[k];
        if (!v->used) continue;
        int fsz = (v->bits / 8) * v->chans;
        uint32_t nfr = fsz ? v->len / fsz : 0;
        if (!nfr) { v->used = 0; continue; }
        const uint8_t *d = v->live ? M + (v->gaddr & GUEST_MASK) : v->data;
        for (int i = 0; i < frames; i++) {
            uint32_t f = (uint32_t)(v->pos >> 32);
            if (f >= nfr) {
                if (v->loops == 0) {
                    v->used = 0;
                    int n = (done_tail + 1) % 256;
                    if (n != done_head) { done_q[done_tail] = 0x80000000u | (uint32_t)k; done_tail = n; }
                    break;
                }
                if (v->loops > 0) v->loops--;
                v->pos -= (uint64_t)nfr << 32; f = (uint32_t)(v->pos >> 32);
                if (f >= nfr) { v->pos = 0; f = 0; }
            }
            int l, r;
            const uint8_t *q = d + (size_t)f * fsz;
            if (v->bits == 8) { l = ((int)q[0] - 128) << 8; r = v->chans > 1 ? ((int)q[1] - 128) << 8 : l; }
            else { l = (int16_t)(q[0] | q[1] << 8); r = v->chans > 1 ? (int16_t)(q[2] | q[3] << 8) : l; }
            acc[2 * i] += l * v->vl >> 15;
            acc[2 * i + 1] += r * v->vr >> 15;
            v->pos += v->step;
        }
    }
    if (!audio_paused) for (int k = 0; k < STREAMS; k++) {
        sos_stream *t = &ST[k];
        if (!t->used || !t->playing) continue;
        int fsz = (t->bits / 8) * t->chans;
        for (int i = 0; i < frames && t->qhead != t->qtail; i++) {
            uint32_t nfr = t->q[t->qhead].len / fsz;
            uint32_t f = (uint32_t)(t->pos >> 32);
            if (f >= nfr) {                            /* buffer played: give it back */
                int n = (done_tail + 1) % 256;
                if (n != done_head) { done_q[done_tail] = t->q[t->qhead].hdr; done_tail = n; }
                free(t->q[t->qhead].data); t->q[t->qhead].data = NULL;
                t->qhead = (t->qhead + 1) % SQ;
                t->pos -= (uint64_t)nfr << 32;
                i--; continue;
            }
            const uint8_t *q = t->q[t->qhead].data + (size_t)f * fsz;
            int l, r;
            if (t->bits == 8) { l = ((int)q[0] - 128) << 8; r = t->chans > 1 ? ((int)q[1] - 128) << 8 : l; }
            else { l = (int16_t)(q[0] | q[1] << 8); r = t->chans > 1 ? (int16_t)(q[2] | q[3] << 8) : l; }
            acc[2 * i] += l * t->vl >> 15;
            acc[2 * i + 1] += r * t->vr >> 15;
            t->pos += t->step;
        }
    }
    for (int i = 0; i < 2 * frames; i++) { int32_t x = acc[i]; out[i] = (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x); }
    for (int i = 2 * frames; i < bytes / 2; i++) out[i] = 0;
}

static void audio_open(void)
{
    if (audio_dev || getenv("SIERRA_NO_AUDIO")) return;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { rt_log("audio: %s\n", SDL_GetError()); return; }
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof want);
    want.freq = OUT_RATE; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 512; want.callback = mix;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!audio_dev) { rt_log("audio: %s\n", SDL_GetError()); return; }
    rt_log("audio: %d Hz, %d samples\n", have.freq, have.samples);
    SDL_PauseAudioDevice(audio_dev, 0);
}
static void lock(void) { if (audio_dev) SDL_LockAudioDevice(audio_dev); }
static void unlock(void) { if (audio_dev) SDL_UnlockAudioDevice(audio_dev); }

static voice *voice_of(uint32_t h) { h &= 0xffff; return h < VOICES ? &V[h] : NULL; }

#define ERR_NO_DRIVER 0x0e
STUB(sosDIGIInitSystem, 2, 0)
S(sosDIGIUnInitSystem, 0) { (void)a; lock(); for (int k = 0; k < VOICES; k++) { V[k].used = 0; free(V[k].data); V[k].data = NULL; } unlock(); return 0; }
S(sosDIGIUnInitDriver, 1) { return s_sosDIGIUnInitSystem(a); }
STUB(sosDIGIDetectInit, 1, 0)
STUB(sosDIGIDetectUnInit, 0, 0)
STUB(sosDIGIDetectFindNext, 1, ERR_NO_DRIVER)
STUB(sosDIGIAcquireDriver, 1, 0)
STUB(sosDIGIReleaseDriver, 1, 0)
S(sosDIGIUserService, 1) { (void)a; return 0; }
STUB(sosDIGISetBufferSkip, 2, 0)
S(sosDIGIPauseOutput, 1) { (void)a; audio_paused = 1; return 0; }
S(sosDIGIResumeOutput, 1) { (void)a; audio_paused = 0; return 0; }
S(sosDIGISetActiveWindow, 2) { notify_hwnd = a[1]; return 0; }
STUB(sosGetErrorString, 1, 0)
/* ---- streams: the game mixes its effects itself and queues 1 KB buffers (8 at a time). Each finished buffer is
 * handed back with message 0xA000 (wParam 0xA000, lParam = the buffer header) to the window given to
 * sosDIGISetActiveWindow, and the game refills it.
 * stream info:   +0x32 volume  +0x3a rate  +0x3e bits  +0x42 channels  (the sample layout, 2 bytes further on)
 * buffer header: +0x00 flags  +0x02 data  +0x0e size  +0x14 stream id */

S(sosDIGIStreamAcquire, 3)
{
    audio_open();
    uint32_t p = a[1];
    int k; for (k = 0; k < STREAMS && ST[k].used; k++) ;
    if (k == STREAMS) return 1;
    lock();
    sos_stream *t = &ST[k]; memset(t, 0, sizeof *t);
    uint32_t vol = RD32(p + 0x32), rate = RD32(p + 0x3a);
    t->bits = (int)RD32(p + 0x3e); t->chans = (int)RD32(p + 0x42);
    if (t->bits != 8 && t->bits != 16) t->bits = 8;
    if (t->chans < 1 || t->chans > 2) t->chans = 1;
    if (!rate) rate = 22050;
    t->step = ((uint64_t)rate << 32) / OUT_RATE;
    t->vl = vol & 0x7fff; t->vr = (vol >> 16) & 0x7fff;
    if (!t->vl && !t->vr) t->vl = t->vr = 0x7fff;
    t->used = 1; t->playing = 1;
    unlock();
    if (a[2]) WR16(a[2], (uint16_t)k);
    if (trace_snd > 0) rt_log("stream %d: %u Hz %d bit %d ch\n", k, rate, t->bits, t->chans);
    return 0;
}
S(sosDIGIStreamRelease, 2) { if (a[1] < STREAMS) { lock(); sos_stream *t = &ST[a[1]]; for (int i = t->qhead; i != t->qtail; i = (i + 1) % SQ) free(t->q[i].data); memset(t, 0, sizeof *t); unlock(); } return 0; }
S(sosDIGIStreamPrepareBuffer, 3) { (void)a; return 0; }
S(sosDIGIStreamUnPrepareBuffer, 3) { (void)a; return 0; }
S(sosDIGIStreamAddBuffer, 3)
{
    if (a[1] >= STREAMS || !ST[a[1]].used) return 1;
    sos_stream *t = &ST[a[1]];
    uint32_t h = a[2], len = RD32(h + 0x0e), data = RD32(h + 0x02);
    if (len > 0x100000) len = 0x100000;
    uint8_t *copy = (uint8_t *)malloc(len ? len : 1);
    memcpy(copy, GPTR(data), len);
    lock();
    int n = (t->qtail + 1) % SQ;
    if (n == t->qhead) { unlock(); free(copy); return 1; }
    t->q[t->qtail].hdr = h; t->q[t->qtail].data = copy; t->q[t->qtail].len = len; t->qtail = n;
    unlock();
    return 0;
}
S(sosDIGIStreamStart, 2) { if (a[1] < STREAMS) ST[a[1]].playing = 1; return 0; }
S(sosDIGIStreamStop, 2) { if (a[1] < STREAMS) ST[a[1]].playing = 0; return 0; }
S(sosDIGIStreamPause, 2) { if (a[1] < STREAMS) ST[a[1]].playing = 0; return 0; }
S(sosDIGIStreamResume, 2) { if (a[1] < STREAMS) ST[a[1]].playing = 1; return 0; }

S(sosDIGIStartSample, 3)
{
    uint32_t p = a[1];
    if (trace_snd < 0) trace_snd = getenv("SIERRA_TRACE_SOUND") != NULL;
    audio_open();
    uint32_t data = RD32(p), len = RD32(p + 0x10), vol = RD32(p + 0x30), rate = RD32(p + 0x38);
    int bits = (int)RD32(p + 0x3c), ch = (int)RD32(p + 0x40), loops = (int32_t)RD32(p + 0x34);
    if (bits != 8 && bits != 16) bits = 8;
    if (ch < 1 || ch > 2) ch = 1;
    if (!rate) rate = 11025;
    int k; for (k = 0; k < VOICES && V[k].used; k++) ;
    if (k == VOICES) { if (a[2]) WR16(a[2], 0xffff); return 1; }
    lock();
    voice *v = &V[k];
    free(v->data); memset(v, 0, sizeof *v);
    v->live = loops < 0;                  /* a buffer looping for ever is the game's to refill */
    v->gaddr = data; v->len = len;
    if (!v->live) { v->data = (uint8_t *)malloc(len ? len : 1); memcpy(v->data, GPTR(data), len); }
    v->bits = bits; v->chans = ch; v->loops = loops;
    v->step = ((uint64_t)rate << 32) / OUT_RATE;
    v->vl = vol & 0x7fff; v->vr = (vol >> 16) & 0x7fff;
    v->used = 1;
    unlock();
    if (a[2]) WR16(a[2], (uint16_t)k);
    if (trace_snd) rt_log("sound %d: %u bytes %u Hz %d bit %d ch loops %d vol %08x\n", k, len, rate, bits, ch, loops, vol);
    return 0;
}
S(sosDIGISampleDone, 2) { voice *v = voice_of(a[1]); return !v || !v->used; }
S(sosDIGIStopSample, 2) { voice *v = voice_of(a[1]); if (v) { lock(); v->used = 0; unlock(); } return 0; }
S(sosDIGIStopAllSamples, 1) { (void)a; lock(); for (int k = 0; k < VOICES; k++) V[k].used = 0; unlock(); return 0; }
S(sosDIGISetSampleVolume, 3)
{
    voice *v = voice_of(a[1]);
    if (v) { lock(); v->vl = a[2] & 0x7fff; v->vr = a[2] > 0xffff ? (a[2] >> 16) & 0x7fff : v->vl; unlock(); }
    return 0;
}
S(sosDIGISetPanLocation, 3)
{
    /* 0 left .. 0x8000 centre .. 0xffff right; keeps the louder of the two channel volumes */
    voice *v = voice_of(a[1]);
    if (v) { lock(); int m = v->vl > v->vr ? v->vl : v->vr; uint32_t pan = a[2] & 0xffff;
        v->vl = pan <= 0x8000 ? m : (int)((uint64_t)m * (0xffff - pan) / 0x7fff);
        v->vr = pan >= 0x8000 ? m : (int)((uint64_t)m * pan / 0x8000); unlock(); }
    return 0;
}
S(sosDIGISetSampleRate, 3) { voice *v = voice_of(a[1]); if (v && a[2]) { lock(); v->step = ((uint64_t)a[2] << 32) / OUT_RATE; unlock(); } return 0; }

/* ---- MIDI: songs are accepted and "play" until stopped (no synthesizer yet: the music is silent) ---- */
STUB(sosMIDIInitSystem, 2, 0)
STUB(sosMIDIUnInitSystem, 0, 0)
STUB(sosMIDIUnInitDriver, 1, 0)
STUB(sosMIDIResetDriver, 1, 0)
STUB(sosMIDISetActiveWindow, 2, 0)
STUB(sosMIDIDetectInit, 1, 0)
STUB(sosMIDIDetectUnInit, 0, 0)
STUB(sosMIDIDetectFindNext, 1, ERR_NO_DRIVER)
S(sosMIDIUnInitSong, 1) { song_playing[a[0] & 63] = 0; return 0; }
STUB(sosMIDIResetSong, 2, 0)
S(sosMIDIStartSong, 1) { song_playing[a[0] & 63] = 1; return 0; }
S(sosMIDIStopSong, 1) { song_playing[a[0] & 63] = 0; return 0; }
S(sosMIDISongDone, 1) { return !song_playing[a[0] & 63]; }
S(sosMIDIStopAllSongs, 0) { (void)a; memset(song_playing, 0, sizeof song_playing); return 0; }
STUB(sosMIDIPauseSong, 2, 0)
STUB(sosMIDIResumeSong, 1, 0)
STUB(sosMIDIMuteSong, 1, 0)
STUB(sosMIDIUnMuteSong, 1, 0)
S(sosMIDISetSongVolume, 2) { (void)a; return 0; }
STUB(sosMIDIFadeSong, 6, 0)
STUB(sosMIDISetMasterVolume, 1, 0)
STUB(sosMIDISetDriverVolume, 2, 0)
STUB(sosMIDISongAlterTempo, 2, 0)
STUB(sosMIDISetInsData, 3, 0)
STUB(sosMIDIBranchToSongID, 2, 0)
STUB(sosMIDIBranchToTrackID, 3, 0)

void rt_sos_init(void) {}

/* main thread, between recompiled blocks: hand finished buffers and samples back to the game */
void rt_sos_poll(void)
{
    extern void rt_post_message(uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp);
    if (done_head == done_tail) return;
    lock();
    while (done_head != done_tail) {
        uint32_t d = done_q[done_head]; done_head = (done_head + 1) % 256;
        if (!notify_hwnd) continue;
        if (d & 0x80000000u) rt_post_message(notify_hwnd, 0xa000, 0xa002, d & 0xffff);   /* sample finished */
        else rt_post_message(notify_hwnd, 0xa000, 0xa000, d);                          /* stream buffer done */
    }
    unlock();
}

#define N(name, n) { "sos9502", #name, s_##name, n, 0 }
const rt_native rt_sos_natives[] = {
    N(sosTIMERInitSystem, 1), N(sosTIMERUnInitSystem, 0), N(sosTIMERRegisterEvent, 3), N(sosTIMERAlterEventRate, 2),
    N(sosTIMERRemoveEvent, 1), N(sosTIMERGetEventRate, 2), N(sosTIMERPauseEvent, 1), N(sosTIMERResumeEvent, 1),
    N(sosDIGIInitSystem, 2), N(sosDIGIUnInitSystem, 0), N(sosDIGIInitDriver, 2), N(sosDIGIUnInitDriver, 1),
    N(sosDIGIDetectInit, 1), N(sosDIGIDetectUnInit, 0), N(sosDIGIDetectFindFirst, 1), N(sosDIGIDetectFindNext, 1),
    N(sosDIGIAcquireDriver, 1), N(sosDIGIReleaseDriver, 1), N(sosDIGIUserService, 1), N(sosDIGISetBufferSkip, 2),
    N(sosDIGIPauseOutput, 1), N(sosDIGIResumeOutput, 1), N(sosDIGISetActiveWindow, 2), N(sosGetErrorString, 1),
    N(sosDIGIStreamAcquire, 3), N(sosDIGIStreamRelease, 2), N(sosDIGIStreamPrepareBuffer, 3),
    N(sosDIGIStreamUnPrepareBuffer, 3), N(sosDIGIStreamAddBuffer, 3), N(sosDIGIStreamStart, 2), N(sosDIGIStreamStop, 2),
    N(sosDIGIStreamPause, 2), N(sosDIGIStreamResume, 2), N(sosDIGISetSampleVolume, 3), N(sosDIGISetPanLocation, 3),
    N(sosDIGISetSampleRate, 3), N(sosDIGISampleDone, 2), N(sosDIGIStartSample, 3), N(sosDIGIStopSample, 2),
    N(sosDIGIStopAllSamples, 1),
    N(sosMIDIInitSystem, 2), N(sosMIDIUnInitSystem, 0), N(sosMIDIInitDriver, 2), N(sosMIDIUnInitDriver, 1),
    N(sosMIDIResetDriver, 1), N(sosMIDISetActiveWindow, 2), N(sosMIDIDetectInit, 1), N(sosMIDIDetectUnInit, 0),
    N(sosMIDIDetectFindFirst, 1), N(sosMIDIDetectFindNext, 1), N(sosMIDIInitSong, 2), N(sosMIDIUnInitSong, 1),
    N(sosMIDIResetSong, 2), N(sosMIDIStartSong, 1), N(sosMIDIStopSong, 1), N(sosMIDISongDone, 1),
    N(sosMIDIStopAllSongs, 0), N(sosMIDIPauseSong, 2), N(sosMIDIResumeSong, 1), N(sosMIDIMuteSong, 1),
    N(sosMIDIUnMuteSong, 1), N(sosMIDISetSongVolume, 2), N(sosMIDIFadeSong, 6), N(sosMIDISetMasterVolume, 1),
    N(sosMIDISetDriverVolume, 2), N(sosMIDISongAlterTempo, 2), N(sosMIDISetInsData, 3), N(sosMIDIBranchToSongID, 2),
    N(sosMIDIBranchToTrackID, 3),
    { NULL, NULL, NULL, 0, 0 }
};
