#include "controller.h"

#include "../chunkwm/src/api/plugin_api.h"

#include "../chunkwm/src/common/accessibility/window.h"
#include "../chunkwm/src/common/accessibility/element.h"
#include "../chunkwm/src/common/ipc/daemon.h"

#include "misc.h"

#define internal static

extern macos_window *GetWindowById(uint32_t Id);
extern macos_window *GetFocusedWindow();
extern uint32_t GetFocusedWindowId();
extern void BroadcastFocusedWindowFloating(macos_window *Window);
extern void BroadcastFocusedDesktopMode(virtual_space *VirtualSpace);

extern chunkwm_log *c_log;

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

void QueryWindowCoord(char *Op, int SockFD)
{
    char Message[512];

    macos_window *Window;
    float Pos;

    Window = GetFocusedWindow();

    if (StringEquals(Op, "x")) {
        Pos = AXLibGetWindowPosition(Window->Ref).x;
    } else if (StringEquals(Op, "y")) {
        Pos = AXLibGetWindowPosition(Window->Ref).y;
    } else if (StringEquals(Op, "w")) {
        Pos = AXLibGetWindowSize(Window->Ref).width;
    } else if (StringEquals(Op, "h")) {
        Pos = AXLibGetWindowSize(Window->Ref).height;
    } else {
        Pos = 0.0;
    }

    snprintf(Message, sizeof(Message), "%d", (int) Pos);
    WriteToSocket(Message, SockFD);
}

// for reference: gridlayout
/*
void GridLayout(macos_window *Window, char *Op)
{
    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size);
    ASSERT(DisplayRef);

    macos_space *Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
    if ((AXLibHasFlags(Window, Window_Float)) || (VirtualSpace->Mode == Virtual_Space_Float)) {
        unsigned GridRows, GridCols, WinX, WinY, WinWidth, WinHeight;
        if (sscanf(Op, "%d:%d:%d:%d:%d:%d", &GridRows, &GridCols, &WinX, &WinY, &WinWidth, &WinHeight) == 6) {
            WinX = WinX >= GridCols ? GridCols - 1 : WinX;
            WinY = WinY >= GridRows ? GridRows - 1 : WinY;
            WinWidth = WinWidth <= 0 ? 1 : WinWidth;
            WinHeight = WinHeight <= 0 ? 1 : WinHeight;
            WinWidth = WinWidth > GridCols - WinX ? GridCols - WinX : WinWidth;
            WinHeight = WinHeight > GridRows - WinY ? GridRows - WinY : WinHeight;

            region Region = FullscreenRegion(DisplayRef, VirtualSpace);
            float CellWidth = Region.Width / GridCols;
            float CellHeight = Region.Height / GridRows;
            AXLibSetWindowPosition(Window->Ref,
                                   Region.X + Region.Width - CellWidth * (GridCols - WinX),
                                   Region.Y + Region.Height - CellHeight * (GridRows - WinY));
            AXLibSetWindowSize(Window->Ref, CellWidth * WinWidth, CellHeight * WinHeight);
        }
    }

    ReleaseVirtualSpace(VirtualSpace);
    AXLibDestroySpace(Space);
    CFRelease(DisplayRef);
}

void GridLayout(char *Op)
{
    macos_window *Window = GetFocusedWindow();
    if (Window) GridLayout(Window, Op);
}
*/
