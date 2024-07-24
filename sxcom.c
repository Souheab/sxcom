#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  Window id;
  XWindowAttributes attr;
  Damage damage;
  bool damaged;
} Win;

static Win *windows = NULL;
static int window_count = 0;
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

static Win *find_win(Window id) {
  for (int i = 0; i < window_count; i++) {
    if (windows[i].id == id) {
      return &windows[i];
    }
  }
  return NULL;
}

static void add_win(Window id) {
  if (find_win(id))
    return;

  windows = realloc(windows, (window_count + 1) * sizeof(Win));
  if (!windows) {
    log_fatalf("Failed to allocate memory for new window\n");
  }
  Win *new = &windows[window_count];
  new->id = id;
  if (!XGetWindowAttributes(dpy, id, &new->attr)) {
    log_fatalf("Failed to get window attributes\n");
  }
  new->damage = XDamageCreate(dpy, id, XDamageReportNonEmpty);

  XRenderPictureAttributes pa;
  pa.subwindow_mode = IncludeInferiors;
  XRenderPictFormat *format = XRenderFindVisualFormat(dpy, new->attr.visual);
  if (!format) {
    log_fatalf("Failed to find visual format for window 0x%lx\n", id);
  }
  new->damaged = true; // Consider new windows as damaged

  window_count++;
}

static void damage_win(XDamageNotifyEvent *de) {
  Win *win = find_win(de->drawable);
  if (!win) {
    log_fatalf("Received damage event for unknown window\n");
  }
  win->damaged = true;
  
}

static void remove_win(Window id) {
  for (int i = 0; i < window_count; i++) {
    if (windows[i].id == id) {
      if (windows[i].damaged) {
        XDamageDestroy(dpy, windows[i].damage);
      }

      memmove(&windows[i], &windows[i + 1],
              (window_count - i - 1) * sizeof(Win));
      window_count--;
      return;
    }
  }
}

static void composite_window(Win *win) {
  if (!win->damaged || win->attr.map_state != IsViewable) {
    return;
  }

  XRenderPictureAttributes pa;
  pa.subwindow_mode = IncludeInferiors;
  XRenderPictFormat *format = XRenderFindVisualFormat(dpy, win->attr.visual);

  Picture picture = XRenderCreatePicture(dpy, win->id, format, CPSubwindowMode, &pa);

  XRenderComposite(dpy, PictOpOver, picture, None, overlay_picture, 0, 0, 0,
                   0, win->attr.x, win->attr.y, win->attr.width,
                   win->attr.height);
  win->damaged = false;
}

static void composite_damaged_windows() {

  for (int i = 0; i < window_count; i++) {
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
  if (!XCompositeQueryExtension(dpy, &event_base, &error_base)) {
    log_fatalf("X Composite extension not available\n");
  }
  if (!XDamageQueryExtension(dpy, &damage_event, &damage_error)) {
    log_fatalf("X Damage extension not available\n");
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

  for (int i = 0; i < window_count; i++) {
    XDamageDestroy(dpy, windows[i].damage);
  }
  if (overlay_picture != None) {
    XRenderFreePicture(dpy, overlay_picture);
  }
  free(windows);
  XCloseDisplay(dpy);
  return 0;
}
