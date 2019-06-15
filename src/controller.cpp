#include "controller.h"

#include "../chunkwm/src/common/accessibility/window.h"

#define internal static

extern macos_window *GetFocusedWindow();
extern uint32_t GetFocusedWindowId();
extern void BroadcastFocusedWindowFloating(macos_window *Window);
extern void BroadcastFocusedDesktopMode(virtual_space *VirtualSpace);

void FloatWindow(macos_window *Window)
{
    AXLibAddFlags(Window, Window_Float);
    BroadcastFocusedWindowFloating(Window);

    //if (CVarIntegerValue(CVAR_WINDOW_FLOAT_TOPMOST)) {
    //    ExtendedDockSetWindowLevel(Window, kCGModalPanelWindowLevelKey);
    //}
}

internal void
UnfloatWindow(macos_window *Window)
{
    AXLibClearFlags(Window, Window_Float);
    BroadcastFocusedWindowFloating(Window);

    //if (CVarIntegerValue(CVAR_WINDOW_FLOAT_TOPMOST)) {
    //    ExtendedDockSetWindowLevel(Window, kCGNormalWindowLevelKey);
    //}
}

internal void
ToggleWindowFloat()
{
    macos_window *Window = GetFocusedWindow();
    if (!Window) {
        return;
    }

    if (AXLibHasFlags(Window, Window_Float)) {
        UnfloatWindow(Window);
        //TileWindow(Window);
    } else {
        //UntileWindow(Window);
        FloatWindow(Window);
    }
}

