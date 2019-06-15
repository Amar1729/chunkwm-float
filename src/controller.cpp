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

enum window_cmd
{
    Move = 0,
    Increment = 1,
    Decrement = 2,
};

bool ValidateCoordinates(region Bounds, region New)
{
    bool Success = (New.X > Bounds.X) && ((New.X+New.Width) < Bounds.Width) &&
                   (New.Y > Bounds.Y) && ((New.Y+New.Height) < Bounds.Height);
    return Success;
}

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

CGPoint _Move(macos_window *Window, char *Op, float Step)
{
    CGPoint Position = AXLibGetWindowPosition(Window->Ref);

    // original coords
    c_log(C_LOG_LEVEL_WARN, "window: %fx%f\n", Position.x, Position.y);

    if        (StringEquals(Op, "north")) {
        Position.y -= Step;
    } else if (StringEquals(Op, "south")) {
        Position.y += Step;
    } else if (StringEquals(Op, "west")) {
        Position.x -= Step;
    } else if (StringEquals(Op, "east")) {
        Position.x += Step;
    }

    return Position;
}

region _Dilate(macos_window *Window, char *Op, float Step)
{
    CGPoint Position = AXLibGetWindowPosition(Window->Ref);
    CGSize Size = AXLibGetWindowSize(Window->Ref);

    // original coords
    c_log(C_LOG_LEVEL_WARN, "window: %fx%f - %fx%f\n", Position.x, Position.y, Size.width, Size.height);

    // inc/dec comments :
    if        (StringEquals(Op, "north")) {
        // has precision bleed
        // has precision bleed
        Position.y -= Step;
        Size.height += Step;
        if (Step > 0) {
            Size.height += Step;
        }
    } else if (StringEquals(Op, "south")) {
        // NOTE - too small StepSize wont do anything here!
        // doesnt do anything
        // works fine
        if (Step > 0) { Size.height += 20; } // hardcoded until i fix
        else { Size.height -= 20; } // hardcoded until i fix
    } else if (StringEquals(Op, "west")) {
        // has precision bleed
        // has precision bleed
        Position.x -= Step;
        Size.width += Step;
        if (Step > 0) {
            Size.width += Step;
        }
    } else if (StringEquals(Op, "east")) {
        // works fine
        // works fine
        Size.width += Step;
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

    region Result;
    switch (Cmd) {
        case Move : {
            // get move cvar
            float Step = 10.0f;
            CGPoint Position = _Move(Window, Op, Step);
            CGSize Size = AXLibGetWindowSize(Window->Ref);
            Result = RegionFromPointAndSize(Position, Size);
        } break ;
        case Increment : {
            // get resize cvar
            float Step = 10.0f;
            Result = _Dilate(Window, Op, Step);
        } break ;
        case Decrement : {
            // get resize cvar
            //float Step = CVarIntegerValue(CVAR_FLOAT_STEPSIZE);
            float Step = 10.0f;
            Result = _Dilate(Window, Op, -Step);
        } break ;
    }

    c_log(C_LOG_LEVEL_WARN, "result: %fx%f - %fx%f\n", Result.X, Result.Y, Result.Width, Result.Height);

    if (ValidateCoordinates(Region, Result)) {
        ResizeWindow(Window, Result);
    }

    ReleaseVirtualSpace(VirtualSpace);
    AXLibDestroySpace(Space);
    CFRelease(DisplayRef);
}

void MoveWindow(char *Op)
{
    WindowHandler(Op, Move);
}

void IncWindow(char *Op)
{
    WindowHandler(Op, Increment);
}

void DecWindow(char *Op)
{
    WindowHandler(Op, Decrement);
}

void SetSize(char *Size)
{
    c_log(C_LOG_LEVEL_WARN, "setsize\n");
    uint32_t StepSize;
    sscanf(Size, "%d", &StepSize);
    UpdateCVar(CVAR_FLOAT_STEPSIZE, StepSize);
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
