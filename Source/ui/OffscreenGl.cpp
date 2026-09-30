#include "OffscreenGl.h"

// Здесь только системный API: заголовки Windows не смешиваются с JUCE

#if defined (_WIN32)

#ifndef NOMINMAX
 #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace sonder::ui
{

OffscreenGl::OffscreenGl()
{
    // Окно нужно только как носитель контекста: оно никогда не показывается
    const HWND hwnd = CreateWindowExW (0, L"STATIC", L"", WS_POPUP, 0, 0, 16, 16, nullptr, nullptr,
                                       GetModuleHandleW (nullptr), nullptr);
    if (hwnd == nullptr)
        return;

    const HDC dc = GetDC (hwnd);

    PIXELFORMATDESCRIPTOR format {};
    format.nSize = sizeof (format);
    format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    format.iPixelType = PFD_TYPE_RGBA;
    format.cColorBits = 32;
    format.iLayerType = PFD_MAIN_PLANE;

    const int index = dc != nullptr ? ChoosePixelFormat (dc, &format) : 0;
    const HGLRC rc = index != 0 && SetPixelFormat (dc, index, &format) ? wglCreateContext (dc) : nullptr;

    if (rc == nullptr)
    {
        if (dc != nullptr)
            ReleaseDC (hwnd, dc);

        DestroyWindow (hwnd);
        return;
    }

    window = hwnd;
    deviceContext = dc;
    renderContext = rc;
}

OffscreenGl::~OffscreenGl()
{
    if (renderContext != nullptr)
    {
        if (wglGetCurrentContext() == (HGLRC) renderContext)
            wglMakeCurrent (nullptr, nullptr);

        wglDeleteContext ((HGLRC) renderContext);
    }

    if (window != nullptr)
    {
        if (deviceContext != nullptr)
            ReleaseDC ((HWND) window, (HDC) deviceContext);

        DestroyWindow ((HWND) window);
    }
}

OffscreenGl::Scope::Scope (OffscreenGl& context)
{
    previousDevice = wglGetCurrentDC();
    previousContext = wglGetCurrentContext();
    wglMakeCurrent ((HDC) context.deviceContext, (HGLRC) context.renderContext);
}

OffscreenGl::Scope::~Scope()
{
    wglMakeCurrent ((HDC) previousDevice, (HGLRC) previousContext);
}

} // namespace sonder::ui

#else

namespace sonder::ui
{

// На других системах шейдерных эффектов пока нет: экраны рисуются программно
OffscreenGl::OffscreenGl() = default;
OffscreenGl::~OffscreenGl() = default;
OffscreenGl::Scope::Scope (OffscreenGl&) {}
OffscreenGl::Scope::~Scope() = default;

} // namespace sonder::ui

#endif
