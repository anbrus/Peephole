#include "windowpreview.h"

#include <X11/Xutil.h>

#include <stdlib.h>
#include <memory.h>
#include <iostream>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

WindowPreview::WindowPreview()
{
}

WindowPreview::~WindowPreview() {
}

void WindowPreview::Show() {
    m_display = XOpenDisplay(nullptr);
    if(!m_display) {
        std::cerr<<"No X11 display found"<<std::endl;
        return;
    }

    m_window = XCreateSimpleWindow(m_display, RootWindow(m_display, 0), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, BlackPixel (m_display, 0), BlackPixel(m_display, 0));
    XMapWindow(m_display, m_window);
    static char data[1]={ 0 };
    XColor noColor;
    Pixmap blank=XCreateBitmapFromData(m_display, m_window, data, 1, 1);
    Cursor cursor=XCreatePixmapCursor(m_display, blank, blank, &noColor, &noColor, 0, 0);
    XFreePixmap(m_display, blank);
    XDefineCursor(m_display, m_window, cursor);
    XFlush(m_display);
}

void WindowPreview::Close() {
    if(!m_display) return;

    XDestroyWindow(m_display, m_window);
    XCloseDisplay(m_display);
    m_display=nullptr;
}

void WindowPreview::ShowFrame(const uint8_t* data, size_t length) {
    if(!m_display) return;

    /*GC gc;
    XGCValues values;
    unsigned long valuemask;
    values.foreground = BlackPixel(m_display, 0);
    values.background = WhitePixel(m_display, 0);
    gc = XCreateGC(m_display, m_window, (GCForeground | GCBackground), &values);
    XDrawLine(m_display, m_window, gc, 0, 0, 100, 100);
    XFlushGC(m_display, gc);
    XFreeGC(m_display, gc);
    return;*/

    uint8_t* buf=reinterpret_cast<uint8_t*>(malloc(SCREEN_WIDTH*SCREEN_HEIGHT*2));
    memcpy(buf, data, length);
    Visual *visual=DefaultVisual(m_display, 0);
    XImage* image=XCreateImage(m_display, visual, 16, ZPixmap, 0, reinterpret_cast<char*>(buf), SCREEN_WIDTH, SCREEN_HEIGHT, 16, SCREEN_WIDTH*2);
    XPutImage(m_display, m_window, DefaultGC(m_display, 0), image, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    XFlush(m_display);
    XSync(m_display, 1);
    XDestroyImage(image);
}
