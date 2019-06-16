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

void ResizeWindow(macos_window *Window, region Region)
{
    AXLibSetWindowPosition(Window->Ref, Region.X, Region.Y);
    AXLibSetWindowSize(Window->Ref, Region.Width, Region.Height);
}

void _CenterWindow(macos_window *Window, region Screen)
{
    //CGPoint Position = AXLibGetWindowPosition(Window->Ref);
    CGSize Size = AXLibGetWindowSize(Window->Ref);

    float newX = (Screen.Width / 2) - (Size.width / 2);
    float newY = (Screen.Height / 2) - (Size.height / 2);

    CGPoint Position;
    Position.x = newX;
    Position.y = newY;

    region Result = RegionFromPointAndSize(Position, Size);
    ResizeWindow(Window, Result);
}

void CenterWindow(char *Unused)
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
    region Screen = GetScreenDimensions(DisplayRef, VirtualSpace);

    _CenterWindow(Window, Screen);

    ReleaseVirtualSpace(VirtualSpace);
    AXLibDestroySpace(Space);
    CFRelease(DisplayRef);
}

region _WindowHandler(macos_window *Window, char *Op, region Screen) {
    // set the absolute size of a floating window
    // input format: fxf:fxf (fractions of screen)
    float X, Y, W, H;
    sscanf(Op, "%fx%f:%fx%f", &X, &Y, &W, &H);

    CGPoint Position = AXLibGetWindowPosition(Window->Ref);
    CGSize Size = AXLibGetWindowSize(Window->Ref);

    region Result = RegionFromPointAndSize(Position, Size);

    // validate
    // double check float-> int conversion
    if (((int)X < 0) || ((int)Y < 0) || ((int)W < 0) || ((int)H < 0)) { goto err; }
    if (((int)X > 1) || ((int)Y > 1) || ((int)W > 1) || ((int)H > 1)) { goto err; }

    region_offset Offset;
    Offset.Left =   Screen.X + (X * Screen.Width);
    Offset.Right =  Screen.X + (W * Screen.Width);
    Offset.Top =    Screen.Y + (Y * Screen.Height);
    Offset.Bottom = Screen.Y + (H * Screen.Height);

    Result.X = Offset.Left;
    Result.Y = Offset.Top;
    Result.Width = (Offset.Right - Offset.Left);
    Result.Height = (Offset.Bottom - Offset.Top);

    // need to add other checks for validation (result isn't weirdly sized, w !< x)

    /*
    Result.X = Offset.Left > Screen.X ? Offset.Left : Screen.X;
    Result.Y = Offset.Top > Screen.Y ? Offset.Top : Screen.Y;
    //Result.Width = 
    */

err:
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

    region Result;
    switch (Cmd) {
        case WindowMove :      { Step *=  CVarFloatingPointValue(CVAR_FLOAT_MOVE);   } break ;
        case WindowIncrement : { Step *=  CVarFloatingPointValue(CVAR_FLOAT_RESIZE); } break ;
        case WindowDecrement : { Step *= -CVarFloatingPointValue(CVAR_FLOAT_RESIZE); } break ;
        case WindowSetSize :   { Result = _WindowHandler(Window, Op, Region);        } break ; // bit gross, have to overload _WindowHandler
    }

    if (Cmd != WindowSetSize) {
        Result = _WindowHandler(Window, Op, Step, Cmd);
    }

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

void AbsoluteSize(char *Op)
{
    c_log(C_LOG_LEVEL_WARN, "Setting size: %s\n", Op);
    WindowHandler(Op, WindowSetSize);
}

void TemporaryStep(char *Size)
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
