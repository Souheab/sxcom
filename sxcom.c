#include "./draw.c"
#include "./log.h"
#include "./signals.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>

int main(int argc, char **argv) {
  setlocale(LC_ALL, "");
  bool quit = false;
  handle_signals(&quit);
  Display *dpy = XOpenDisplay(NULL);

  if (!dpy) {
    log_fatal("Failed to open X11 display");
  } else {
    log_normalf("Opened X11 display: %s", DisplayString(dpy));
  }

  int scr = DefaultScreen(dpy);
  Window root = RootWindow(dpy, scr);
  Window overlay = XCompositeGetOverlayWindow(dpy, root);

  if (!overlay) {
    log_fatal("Failed to get overlay window");
  } else {
    log_normalf("Got overlay window: %lu", overlay);
  }

  DrawContext ctx;
  init_draw_context(&ctx, dpy, overlay);

  log_normal("Entering main compositing loop");

  XEvent ev;
  XSelectInput(dpy, root, SubstructureNotifyMask);
  while (!quit) {
    while (XPending(dpy)) {
      printf("Processing xevent\n");
      XNextEvent(dpy, &ev);
      switch (ev.type) {
      case Expose:
        printf("Expose\n");
        draw_text(&ctx, root, (FcChar8 *)"Hello", 5);
        break;
      }
    }
  }
}
