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

/* ---- digital audio: first pass reports no device ---- */
#define ERR_NO_DRIVER 0x0e
STUB(sosDIGIInitSystem, 2, 0)
STUB(sosDIGIUnInitSystem, 0, 0)
STUB(sosDIGIInitDriver, 2, ERR_NO_DRIVER)
STUB(sosDIGIUnInitDriver, 1, 0)
STUB(sosDIGIDetectInit, 1, 0)
STUB(sosDIGIDetectUnInit, 0, 0)
STUB(sosDIGIDetectFindFirst, 1, ERR_NO_DRIVER)
STUB(sosDIGIDetectFindNext, 1, ERR_NO_DRIVER)
STUB(sosDIGIAcquireDriver, 1, 0)
STUB(sosDIGIReleaseDriver, 1, 0)
STUB(sosDIGIUserService, 1, 0)
STUB(sosDIGISetBufferSkip, 2, 0)
STUB(sosDIGIPauseOutput, 1, 0)
STUB(sosDIGIResumeOutput, 1, 0)
STUB(sosDIGISetActiveWindow, 2, 0)
STUB(sosGetErrorString, 1, 0)
STUB(sosDIGIStreamAcquire, 3, 1)
STUB(sosDIGIStreamRelease, 2, 0)
STUB(sosDIGIStreamPrepareBuffer, 3, 0)
STUB(sosDIGIStreamUnPrepareBuffer, 3, 0)
STUB(sosDIGIStreamAddBuffer, 3, 0)
STUB(sosDIGIStreamStart, 2, 0)
STUB(sosDIGIStreamStop, 2, 0)
STUB(sosDIGIStreamPause, 2, 0)
STUB(sosDIGIStreamResume, 2, 0)
STUB(sosDIGISetSampleVolume, 3, 0)
STUB(sosDIGISetPanLocation, 3, 0)
STUB(sosDIGISetSampleRate, 3, 0)
STUB(sosDIGISampleDone, 2, 1)
STUB(sosDIGIStartSample, 3, 0)
STUB(sosDIGIStopSample, 2, 0)
STUB(sosDIGIStopAllSamples, 1, 0)

/* ---- MIDI: first pass reports no device ---- */
STUB(sosMIDIInitSystem, 2, 0)
STUB(sosMIDIUnInitSystem, 0, 0)
STUB(sosMIDIInitDriver, 2, ERR_NO_DRIVER)
STUB(sosMIDIUnInitDriver, 1, 0)
STUB(sosMIDIResetDriver, 1, 0)
STUB(sosMIDISetActiveWindow, 2, 0)
STUB(sosMIDIDetectInit, 1, 0)
STUB(sosMIDIDetectUnInit, 0, 0)
STUB(sosMIDIDetectFindFirst, 1, ERR_NO_DRIVER)
STUB(sosMIDIDetectFindNext, 1, ERR_NO_DRIVER)
STUB(sosMIDIInitSong, 2, 1)
STUB(sosMIDIUnInitSong, 1, 0)
STUB(sosMIDIResetSong, 2, 0)
STUB(sosMIDIStartSong, 1, 0)
STUB(sosMIDIStopSong, 1, 0)
STUB(sosMIDISongDone, 1, 1)
STUB(sosMIDIStopAllSongs, 0, 0)
STUB(sosMIDIPauseSong, 2, 0)
STUB(sosMIDIResumeSong, 1, 0)
STUB(sosMIDIMuteSong, 1, 0)
STUB(sosMIDIUnMuteSong, 1, 0)
STUB(sosMIDISetSongVolume, 2, 0)
STUB(sosMIDIFadeSong, 6, 0)
STUB(sosMIDISetMasterVolume, 1, 0)
STUB(sosMIDISetDriverVolume, 2, 0)
STUB(sosMIDISongAlterTempo, 2, 0)
STUB(sosMIDISetInsData, 3, 0)
STUB(sosMIDIBranchToSongID, 2, 0)
STUB(sosMIDIBranchToTrackID, 3, 0)

void rt_sos_init(void) {}
void rt_sos_poll(void) {}

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
