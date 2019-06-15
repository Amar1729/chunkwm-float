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

bool ValidateCoordinates(region Region, float X, float Y, float W, float H)
{
    bool Success = (X > Region.X) &&
                   (Y > Region.Y) &&
                   ((X+W) < Region.Width) &&
                   ((Y+H) < Region.Height);
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

void _Move(macos_window *Window, char *Op, int Step/*, region Region*/)
{
    CGPoint Position = AXLibGetWindowPosition(Window->Ref);

    uint32_t X = Position.x;
    uint32_t Y = Position.y;

    if        (StringEquals(Op, "north")) {
        Y -= Step;
    } else if (StringEquals(Op, "south")) {
        Y += Step;
    } else if (StringEquals(Op, "west")) {
        X -= Step;
    } else if (StringEquals(Op, "east")) {
        X += Step;
    }

    // todo - find a good way to pass region?
    //if (!ValidateCoordinates(Region, X, Y, AXLibGetWindowSize(Window->Ref).width, AXLibGetWindowSize(Window->Ref).height)) { return; }

    AXLibSetWindowPosition(Window->Ref, X, Y);
}

void _Dilate(macos_window *Window, char *Op, int Step, region Region)
{
    CGPoint Position = AXLibGetWindowPosition(Window->Ref);
    CGSize Size = AXLibGetWindowSize(Window->Ref);

    float X = (float) Position.x;
    float Y = (float) Position.y;
    float W = (float) Size.width;
    float H = (float) Size.height;

    c_log(C_LOG_LEVEL_WARN, "%fx%f - %fx%f\n", X, Y, W, H);
    c_log(C_LOG_LEVEL_WARN, "Region: %fx%f - %fx%f\n", Region.X, Region.Y, Region.Width, Region.Height);

    // inc/dec comments :
    if        (StringEquals(Op, "north")) {
        // has precision bleed
        // has precision bleed
        Y -= Step;
        H += Step;
        if (Step > 0) {
            H += Step;
        }
    } else if (StringEquals(Op, "south")) {
        // NOTE - too small StepSize wont do anything here!
        // doesnt do anything
        // works fine
        H += 20;
    } else if (StringEquals(Op, "west")) {
        // has precision bleed
        // has precision bleed
        X -= Step;
        W += Step;
        if (Step > 0) {
            W += Step;
        }
    } else if (StringEquals(Op, "east")) {
        // works fine
        // works fine
        W += Step;
    }

    c_log(C_LOG_LEVEL_WARN, "%fx%f - %fx%f\n\n", X, Y, W, H);

    if (!ValidateCoordinates(Region, X, Y, W, H)) { return; }

    AXLibSetWindowPosition(Window->Ref, X, Y);
    AXLibSetWindowSize(Window->Ref, W, H);
}

void ResizeWindow(macos_window *Window, char *Op, bool Increase)
{
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

    //int Step = CVarIntegerValue(CVAR_FLOAT_STEPSIZE);
    int Step = 10;
    if (!Increase) { Step *= -1; }

    region Region = GetScreenDimensions(DisplayRef, VirtualSpace);
    _Dilate(Window, Op, Step, Region);

    ReleaseVirtualSpace(VirtualSpace);
    AXLibDestroySpace(Space);
    CFRelease(DisplayRef);
}

void MoveWindow(char *Op)
{
    macos_window *Window = GetFocusedWindow();
    if (Window) {
        //int Step = CVarIntegerValue(CVAR_FLOAT_STEPSIZE);
        //ResizeWindow(Window, Op, Step);
        _Move(Window, Op, 10);
    }
}

void IncWindow(char *Op)
{
    macos_window *Window = GetFocusedWindow();
    if (Window) {
        //int Step = CVarIntegerValue(CVAR_FLOAT_STEPSIZE);
        //ResizeWindow(Window, Op, Step);
        ResizeWindow(Window, Op, true);
    }
}

void DecWindow(char *Op)
{
    macos_window *Window = GetFocusedWindow();
    if (Window) {
        //int Step = CVarIntegerValue(CVAR_FLOAT_STEPSIZE);
        //ResizeWindow(Window, Op, -Step);
        ResizeWindow(Window, Op, false);
    }
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
