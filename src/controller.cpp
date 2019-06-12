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
#include "misc.h"

#define internal static

extern macos_window *GetWindowById(uint32_t Id);
extern macos_window *GetFocusedWindow();
extern uint32_t GetFocusedWindowId();
extern void BroadcastFocusedWindowFloating(macos_window *Window);
extern void BroadcastFocusedDesktopMode(virtual_space *VirtualSpace);

extern chunkwm_log *c_log;

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

    CGPoint Position = AXLibGetWindowPosition(Window->Ref);
    CGSize Size = AXLibGetWindowSize(Window->Ref);

    uint32_t X = Position.x;
    uint32_t Y = Position.y;
    uint32_t W = Size.width;
    uint32_t H = Size.height;

    c_log(C_LOG_LEVEL_WARN, "%dx%d - %dx%d\n", X, Y, W, H);

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

    c_log(C_LOG_LEVEL_WARN, "%dx%d - %dx%d\n", X, Y, W, H);
    c_log(C_LOG_LEVEL_WARN, "\n");

    AXLibSetWindowPosition(Window->Ref, X, Y);
    // what in the hek :
    AXLibSetWindowSize(Window->Ref, W, H);
    // incorrect coords are being read for (?) size??? they're off by about 3

    ReleaseVirtualSpace(VirtualSpace);
    AXLibDestroySpace(Space);
    CFRelease(DisplayRef);
}

void IncWindow(char *Op)
{
    macos_window *Window = GetFocusedWindow();
    if (Window) { ResizeWindow(Window, Op, true); }
}

void DecWindow(char *Op)
{
    macos_window *Window = GetFocusedWindow();
    if (Window) { ResizeWindow(Window, Op, false); }
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
