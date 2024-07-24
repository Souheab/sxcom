#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xfixes.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  Damage damage;
  XserverRegion damaged_region;
  bool redraw_needed;
  XRenderPictFormat *format;
} MapWin;

typedef struct {
  Window window;
  XWindowAttributes attr;
  MapWin *mapwin;
} Win;

static Win *windows = NULL;
static int win_count = 0;
static Picture overlay_picture = None;
static int damage_event, damage_error;
static Display *dpy;
static Window root;
static Window overlay;

static void log_fatalf(const char *str, ...) {
  va_list args;
  va_start(args, str);
  printf("FATAL ERROR: ");
  vprintf(str, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

static void log_f(const char *str, ...) {
  va_list args;
  va_start(args, str);
  printf("LOG: ");
  vprintf(str, args);
  va_end(args);
}

static void log_warn_f(const char *str, ...) {
  va_list args;
  va_start(args, str);
  printf("WARNING: ");
  vprintf(str, args);
  va_end(args);
}

static void init_overlay() {
  XRenderPictureAttributes pa;
  pa.subwindow_mode = IncludeInferiors;
  XRenderPictFormat *format = XRenderFindVisualFormat(dpy, DefaultVisual(dpy, DefaultScreen(dpy)));
  if (!format) {
    log_fatalf("Failed to find visual format for root window\n");
  }
  overlay_picture = XRenderCreatePicture(dpy, overlay, format, CPSubwindowMode, &pa);
  if (overlay_picture == None) {
    log_fatalf("Failed to create picture for root window\n");
  }

  XRenderColor clear = {0};
  XRenderColor red = {0xffff, 0, 0, 0xffff};
  XRenderFillRectangle(dpy, PictOpSrc, overlay_picture, &red, 0, 0,
                       DisplayWidth(dpy, DefaultScreen(dpy)),
                       DisplayHeight(dpy, DefaultScreen(dpy)));
}

static Win *find_win(Window window) {
  for (int i = 0; i < win_count; i++) {
    if (windows[i].window == window) {
      return &windows[i];
    }
  }
  return NULL;
}

static MapWin *map_win(Window window, XWindowAttributes attr) {
  if (attr.width <= 0 || attr.height <= 0) {
    log_f("Window %lu has invalid size (%dx%d), skipping\n", window, attr.width,
          attr.height);
    return NULL;
  }

  XRenderPictFormat *format = XRenderFindVisualFormat(dpy, attr.visual);
  if (format == NULL) {
    log_warn_f("Failed to find visual format for window %lu, skipping\n", window);
    return NULL;
  }

  Damage damage = XDamageCreate(dpy, window, XDamageReportNonEmpty);
  if (damage == None) {
    log_warn_f("Failed to create damage object for window %lu, skipping\n", window);
    return NULL;
  }

  MapWin *new = calloc(1, sizeof(MapWin));

  new->damage = damage;
  new->damaged_region = XFixesCreateRegion(dpy, NULL, 0);
  new->format = format;
  new->redraw_needed = true;

  return new;
}

static void add_win(Window window) {

  log_f("Considering window %lu for addition\n", window);

  if (find_win(window) != NULL) {
    log_f("Window %lu already added, skipping\n", window);
    return;
  }

  XWindowAttributes attr;
  if (!XGetWindowAttributes(dpy, window, &attr)) {
    log_warn_f("Failed to get window attributes for window %lu, skipping\n",
               window);
    return;
  }


  MapWin *mapwin = NULL;
  if (attr.map_state == IsViewable) {
    mapwin = map_win(window, attr);
  }

  Win *new = realloc(windows, sizeof(Win) * (win_count + 1));
  if (new == NULL) {
    log_fatalf("Failed to allocate memory for new Win");
  }
  windows = new;

  windows[win_count].window = window;
  windows[win_count].attr = attr;
  windows[win_count].mapwin = mapwin;

  log_f("Added window %lu (x: %d, y: %d, width: %d, height: %d, depth: %d)\n",
        window, attr.x, attr.y, attr.width, attr.height, attr.depth);

  win_count++;
}

static void damage_win(XDamageNotifyEvent *de) {
  Win *win = find_win(de->drawable);
  if (!win) {
    log_fatalf("Received damage event for unknown window\n");
  }
  win->mapwin->redraw_needed = true;
  
}

static void remove_win(Window window) {
  for (int i = 0; i < win_count; i++) {
    if (windows[i].window == window) {
      if (windows[i].mapwin->redraw_needed) {
        XDamageDestroy(dpy, windows[i].mapwin->redraw_needed);
      }

      memmove(&windows[i], &windows[i + 1],
              (win_count - i - 1) * sizeof(Win));
      win_count--;
      return;
    }
  }
}

static void composite_window(Win *win) {
  if (win->mapwin == NULL || !win->mapwin->redraw_needed || win->attr.map_state != IsViewable) {
    return;
  }

  XRenderPictureAttributes pa;
  pa.subwindow_mode = IncludeInferiors;
  XRenderPictFormat *format = XRenderFindVisualFormat(dpy, win->attr.visual);

  Picture picture = XRenderCreatePicture(dpy, win->window, format, CPSubwindowMode, &pa);

  XRenderComposite(dpy, PictOpOver, picture, None, overlay_picture, 0, 0, 0,
                   0, win->attr.x, win->attr.y, win->attr.width,
                   win->attr.height);
  win->mapwin->redraw_needed = false;
}

static void composite_damaged_windows() {

  for (int i = 0; i < win_count; i++) {
    composite_window(&windows[i]);
  }
}

static int error_handler(Display *dpy, XErrorEvent *ev) {
  char error_text[256];
  XGetErrorText(dpy, ev->error_code, error_text, sizeof(error_text));
  log_warn_f(
      "X11 error: %s (request code: %d, error code: %d, resource id: %lu)\n",
      error_text, ev->request_code, ev->error_code, ev->resourceid);
  return 0;
}

void handle_damage(){}

int main(int argc, char **argv) {
  setlocale(LC_ALL, "");
  Window root_return, parent_return;
  Window *children;
  unsigned int nchildren;
  int scr;

  dpy = XOpenDisplay(NULL);

  if (!dpy) {
    log_fatalf("Failed to open X11 display\n");
  } else {
    log_f("Opened X11 display: %s\n", DisplayString(dpy));
  }

  scr = DefaultScreen(dpy);
  root = RootWindow(dpy, scr);
  XSetErrorHandler(error_handler);
  XSynchronize(dpy, True);

  int event_base, error_base;
  int damage_event, damage_error;
  int xfixes_event, xfixes_error;
  if (!XCompositeQueryExtension(dpy, &event_base, &error_base)) {
    log_fatalf("X Composite extension not available\n");
  }
  if (!XDamageQueryExtension(dpy, &damage_event, &damage_error)) {
    log_fatalf("X Damage extension not available\n");
  }
  if (!XFixesQueryExtension(dpy, &xfixes_event, &xfixes_error)) {
    log_fatalf("X Fixes extension not available\n");
  }

  XCompositeRedirectSubwindows(dpy, root, CompositeRedirectManual);
  printf("Redirected subwindows\n");

  XSelectInput(dpy, root,
               SubstructureNotifyMask |
               ExposureMask |
               StructureNotifyMask |
               PropertyChangeMask);

  XQueryTree(dpy, root, &root_return, &parent_return, &children, &nchildren);
  for (unsigned int i = 0; i < nchildren; i++) {
    add_win(children[i]);
  }
  XFree(children);

  overlay = XCompositeGetOverlayWindow(dpy, root);
  init_overlay();
  composite_damaged_windows();

  XEvent ev;
  while (1) {
    XNextEvent(dpy, &ev);
    switch (ev.type) {
    case CreateNotify:
      add_win(ev.xcreatewindow.window);
      break;
    case DestroyNotify:
      remove_win(ev.xdestroywindow.window);
      break;
    case ConfigureNotify: 
      break;
    case MapNotify:
      break;
    case UnmapNotify:
      break;
    case Expose:
    default:
      if (ev.type == damage_event + XDamageNotify) {
        printf("Damage event\n");
        XDamageNotifyEvent *dev = (XDamageNotifyEvent *)&ev;
      }
      break;
    }
    printf("comp\n");
    composite_damaged_windows();
  }

  for (int i = 0; i < win_count; i++) {
    XDamageDestroy(dpy, windows[i].mapwin->damage);
  }
  if (overlay_picture != None) {
    XRenderFreePicture(dpy, overlay_picture);
  }
  free(windows);
  XCloseDisplay(dpy);
  return 0;
}
