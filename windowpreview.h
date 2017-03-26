#pragma once

#include <X11/Xlib.h>
#include <cstdint>

class WindowPreview
{
public:
    WindowPreview();
    virtual ~WindowPreview();

    void Show();
    void Close();
    void ShowFrame(const uint8_t* data, size_t length);

private:
    Display* m_display=nullptr;
    Window m_window;
};

