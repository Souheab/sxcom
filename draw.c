#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
typedef struct {
  Display *dpy;
  Window win;
  GC gc;
  XftFont *font;
  XftDraw *draw;
  XftColor color;
} DrawContext;

void draw_text(DrawContext *ctx, Window window, FcChar8 *string, int len) {
  XWindowAttributes attr;
  XGetWindowAttributes(ctx->dpy, window, &attr);

  XClearWindow(ctx->dpy, window);
  XftDrawStringUtf8(ctx->draw, &ctx->color, ctx->font, 10, 20, string, len);
}

void init_draw_context(DrawContext *ctx, Display *dpy, Window win) {
  ctx->dpy = dpy;
  ctx->win = win;
  ctx->gc = XCreateGC(dpy, win, 0, NULL);
  ctx->font = XftFontOpenName(dpy, DefaultScreen(dpy), "monospace");
  ctx->draw = XftDrawCreate(dpy, win, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultColormap(dpy, DefaultScreen(dpy)));
  XftColorAllocName(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultColormap(dpy, DefaultScreen(dpy)), "black", &ctx->color);
}
