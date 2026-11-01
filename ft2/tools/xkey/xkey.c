// xkey KEYSYM press|release|tap [holdms] ... (multiple triples allowed, "sleep MS" too)
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
extern int XTestFakeKeyEvent(Display*, unsigned int, Bool, unsigned long);
int main(int argc, char** argv) {
    Display* d = XOpenDisplay(NULL);
    if (!d) { fprintf(stderr, "no display\n"); return 1; }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "warp")) { XWarpPointer(d, None, DefaultRootWindow(d), 0,0,0,0, atoi(argv[i+1]), atoi(argv[i+2])); XFlush(d); i += 2; continue; }
        if (!strcmp(argv[i], "sleep")) { usleep(atoi(argv[++i]) * 1000); continue; }
        KeySym ks = XStringToKeysym(argv[i]);
        KeyCode kc = XKeysymToKeycode(d, ks);
        const char* act = argv[++i];
        if (!strcmp(act, "press") || !strcmp(act, "tap")) XTestFakeKeyEvent(d, kc, True, 0);
        XFlush(d);
        if (!strcmp(act, "tap")) usleep(atoi(argv[++i]) * 1000);
        if (!strcmp(act, "release") || !strcmp(act, "tap")) XTestFakeKeyEvent(d, kc, False, 0);
        XFlush(d);
    }
    XCloseDisplay(d);
    return 0;
}
