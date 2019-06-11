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

// should accept window ID?
void ResizeWindow(char *Op)
{
    macos_window *Window = GetFocusedWindow();
    if (!Window) { return; }

    // make sure window is floating
    if (!AXLibHasFlags(Window, Window_Float)) {
        c_log(C_LOG_LEVEL_DEBUG, "chunkwm-float: window %d not floating\n", Window->Id);
        return;
    }

    /*
    bool WindowMoved  = AXLibSetWindowPosition(Window->Ref, Node->Region.X, Node->Region.Y);
    bool WindowResized = AXLibSetWindowSize(Window->Ref, Node->Region.Width, Node->Region.Height);

    if (Center) {
        if (WindowMoved || WindowResized) {
            CenterWindowInRegion(Window, Node->Region);
        }
    }
    */
    // WindowMoved or WindowResized flags? necessary?

    // for now assume we're dilating a window south
    CGPoint Position = AXLibGetWindowPosition(Window->Ref);
    CGSize Size = AXLibGetWindowSize(Window->Ref);

    /*

    // does the type of this region matter? not sure
    // feels hacky
    region Region = {
        .X = (float) AXLibGetWindowPosition(Window->Ref).x,
        .Y = (float) AXLibGetWindowPosition(Window->Ref).y,
        .Width = (float) AXLibGetWindowSize(Window->Ref).width,
        .Height = (float) AXLibGetWindowSize(Window->Ref).height,
        .Type = Region_Full
    };

    //float DiffX = (Region.X + Region.Width) - (Position.x + Size.width);
    //float DiffX = (AXLibGetWindowPosition(Window->Ref).x + AXLibGetWindowSize(Window->Ref).width) - (Position.x + Size.width);
    //float DiffY = (AXLibGetWindowPosition(Window->Ref).y + AXLibGetWindowSize(Window->Ref).height) - (Position.y + Size.height);
    float DiffX = (Region.X + Region.Width) - (Position.x + Size.width);
    float DiffY = (Region.Y + Region.Height) - (Position.y + Size.height);

    if ((DiffX > 0.0f) || (DiffY > 0.0f)) {
        float OffsetX = DiffX / 2.0f;
        Region.X += OffsetX;
        Region.Width -= OffsetX;

        float OffsetY = DiffY / 2.0f;
        Region.Y += OffsetY;
        Region.Height -= OffsetY;

        AXLibSetWindowPosition(Window->Ref, Region.X, Region.Y);
        AXLibSetWindowSize(Window->Ref, Region.Width, Region.Height);
    }
    */

    // hardcode the move for now
    //AXLibSetWindowPosition(Window->Ref, AXLibGetWindowPosition(Window->Ref).x + 100, AXLibGetWindowPosition(Window->Ref).y);
    AXLibSetWindowPosition(Window->Ref, Position.x + 100, Position.y);
    AXLibSetWindowSize(Window->Ref, AXLibGetWindowSize(Window->Ref).width, AXLibGetWindowSize(Window->Ref).height);
}

void QueryWindowCoord(char *Op, int SockFD)
{
    char Message[512];

    macos_window *Window;
    float Pos;

    Window = GetFocusedWindow();
    if (!Window) { return; }

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
