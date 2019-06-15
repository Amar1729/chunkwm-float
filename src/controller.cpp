#include "controller.h"

#include "../chunkwm/src/api/plugin_api.h"

#include "../chunkwm/src/common/accessibility/window.h"
#include "../chunkwm/src/common/accessibility/element.h"
#include "../chunkwm/src/common/accessibility/display.h"
#include "../chunkwm/src/common/config/cvar.h"
#include "../chunkwm/src/common/ipc/daemon.h"
#include "../chunkwm/src/common/misc/assert.h"

#include "vspace.h"
#include "constants.h"
#include "region.h"
#include "misc.h"

#define internal static

extern macos_window *GetFocusedWindow();

extern chunkwm_log *c_log;

region GetScreenDimensions(CFStringRef DisplayRef, virtual_space *VirtualSpace)
{
    region Result = CGRectToRegion(AXLibGetDisplayBounds(DisplayRef));
    // just don't constrain the region for now ?
    // yikes probably want this!
    //ConstrainRegion(DisplayRef, &Result);

    // note - should i just allow floating windows constrained to the entire desktop?
    // or will this approach conflict with space gaps set by tiling?
    region_offset *Offset = VirtualSpace->Offset;
    if (Offset) {
        Result.X += Offset->Left;
        Result.Y += Offset->Top;
        Result.Width -= (Offset->Left + Offset->Right);
        Result.Height -= (Offset->Top + Offset->Bottom);
    }

    c_log(C_LOG_LEVEL_WARN, "Region: %fx%f - %fx%f\n", Result.X, Result.Y, Result.Width, Result.Height);

    return Result;
}

region _WindowHandler(macos_window *Window, char *Op, float Step, window_cmd Cmd)
{
    CGPoint Position = AXLibGetWindowPosition(Window->Ref);
    CGSize Size = AXLibGetWindowSize(Window->Ref);

    // original coords
    c_log(C_LOG_LEVEL_WARN, "window: %fx%f\n", Position.x, Position.y);

    if        (StringEquals(Op, "north")) {
        Position.y -= Step;
        if (Cmd != WindowMove) { Size.height += Step; }
    } else if (StringEquals(Op, "south")) {
        if (Cmd != WindowMove) { Size.height += Step; }
        else { Position.y += Step; }
    } else if (StringEquals(Op, "west")) {
        Position.x -= Step;
        if (Cmd != WindowMove) { Size.width += Step; }
    } else if (StringEquals(Op, "east")) {
        if (Cmd != WindowMove) { Size.width += Step; }
        else { Position.x += Step; }
    }

    return RegionFromPointAndSize(Position, Size);
}

void ResizeWindow(macos_window *Window, region Region)
{
    AXLibSetWindowPosition(Window->Ref, Region.X, Region.Y);
    AXLibSetWindowSize(Window->Ref, Region.Width, Region.Height);
}

void WindowHandler(char *Op, window_cmd Cmd)
{
    macos_window *Window = GetFocusedWindow();
    if (!Window) { return; }

    // make sure window is floating
    if (!AXLibHasFlags(Window, Window_Float)) {
        c_log(C_LOG_LEVEL_DEBUG, "chunkwm-float: window %d not floating\n", Window->Id);
        return;
    }

    CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size);
    ASSERT(DisplayRef);

    macos_space *Space = AXLibActiveSpace(DisplayRef);
    ASSERT(Space);

    virtual_space *VirtualSpace = AcquireVirtualSpace(Space);
    region Region = GetScreenDimensions(DisplayRef, VirtualSpace);

    float Step;
    if (StringEquals(Op, "north") || StringEquals(Op, "south")) {
        Step = Region.Height;
    } else {
        Step = Region.Width;
    }

    switch (Cmd) {
        case WindowMove :      { Step *=  CVarFloatingPointValue(CVAR_FLOAT_MOVE);   } break ;
        case WindowIncrement : { Step *=  CVarFloatingPointValue(CVAR_FLOAT_RESIZE); } break ;
        case WindowDecrement : { Step *= -CVarFloatingPointValue(CVAR_FLOAT_RESIZE); } break ;
    }

    region Result = _WindowHandler(Window, Op, Step, Cmd);

    c_log(C_LOG_LEVEL_WARN, "result: %fx%f - %fx%f\n", Result.X, Result.Y, Result.Width, Result.Height);

    if (!ResultIsInsideRegion(Result, Region)) {
        ConstrainResultToRegion(&Result, Region, Cmd);
    }
    ResizeWindow(Window, Result);

    ReleaseVirtualSpace(VirtualSpace);
    AXLibDestroySpace(Space);
    CFRelease(DisplayRef);
}

void MoveWindow(char *Op)
{
    WindowHandler(Op, WindowMove);
}

void IncWindow(char *Op)
{
    WindowHandler(Op, WindowIncrement);
}

void DecWindow(char *Op)
{
    WindowHandler(Op, WindowDecrement);
}

void SetSize(char *Size)
{
    float StepSize;
    sscanf(Size, "%f", &StepSize);
    if (((int)StepSize < 0) || (int(StepSize) > 1)) { return; }
    c_log(C_LOG_LEVEL_WARN, "setsize: %f\n", StepSize);
    // update both temporarily
    UpdateCVar(CVAR_FLOAT_MOVE, StepSize);
    UpdateCVar(CVAR_FLOAT_RESIZE, StepSize);
}

void QueryWindowCoord(char *Op, int SockFD)
{
    char Message[512];

    macos_window *Window;
    float Pos;

    Window = GetFocusedWindow();
    if (!Window) { return; }

    switch (Op[0]) {
        case 'x' : { Pos = AXLibGetWindowPosition(Window->Ref).x;  } break ;
        case 'y' : { Pos = AXLibGetWindowPosition(Window->Ref).y;  } break ;
        case 'w' : { Pos = AXLibGetWindowSize(Window->Ref).width;  } break ;
        case 'h' : { Pos = AXLibGetWindowSize(Window->Ref).height; } break ;
        case '?' : { Pos = 0.0; } break ;
    }

    snprintf(Message, sizeof(Message), "%d", (int) Pos);
    WriteToSocket(Message, SockFD);
}
