#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <map>
#include <vector>

#include "../chunkwm/src/api/plugin_api.h"

#include "../chunkwm/src/common/accessibility/display.h"
#include "../chunkwm/src/common/accessibility/application.h"
#include "../chunkwm/src/common/accessibility/window.h"
#include "../chunkwm/src/common/accessibility/element.h"
#include "../chunkwm/src/common/accessibility/observer.h"
#include "../chunkwm/src/common/dispatch/cgeventtap.h"
#include "../chunkwm/src/common/config/cvar.h"
#include "../chunkwm/src/common/config/tokenize.h"
#include "../chunkwm/src/common/ipc/daemon.h"
#include "../chunkwm/src/common/misc/carbon.h"
#include "../chunkwm/src/common/misc/workspace.h"
#include "../chunkwm/src/common/misc/assert.h"
#include "../chunkwm/src/common/misc/profile.h"
#include "../chunkwm/src/common/border/border.h"

#include "../chunkwm/src/common/accessibility/display.mm"
#include "../chunkwm/src/common/accessibility/application.cpp"
#include "../chunkwm/src/common/accessibility/window.cpp"
#include "../chunkwm/src/common/accessibility/element.cpp"
#include "../chunkwm/src/common/accessibility/observer.cpp"
#include "../chunkwm/src/common/dispatch/cgeventtap.cpp"
#include "../chunkwm/src/common/config/cvar.cpp"
#include "../chunkwm/src/common/config/tokenize.cpp"
#include "../chunkwm/src/common/ipc/daemon.cpp"
#include "../chunkwm/src/common/misc/carbon.cpp"
#include "../chunkwm/src/common/misc/workspace.mm"
#include "../chunkwm/src/common/border/border.mm"

#include "config.h"
#include "controller.h"
#include "vspace.h"
#include "region.h"
#include "misc.h"
#include "constants.h"

extern chunkwm_log *c_log;

#include "config.cpp"
#include "controller.cpp"
#include "vspace.cpp"
#include "region.cpp"

#define internal static

bool DoFloatSpace(CGSSpaceID SpaceId);
int NumberOfWindowsOnSpace(CGSSpaceID SpaceId);

typedef std::map<pid_t, macos_application *> macos_application_map;
typedef macos_application_map::iterator macos_application_map_it;

typedef std::map<uint32_t, macos_window *> macos_window_map;
typedef macos_window_map::iterator macos_window_map_it;

internal const char *PluginName = "Floating";
internal const char *PluginVersion = "0.1.0";

internal macos_application_map Applications;
internal macos_window_map Windows;
internal pthread_mutex_t WindowsLock;
internal event_tap EventTap;
internal chunkwm_api API;
chunkwm_log *c_log;

internal const char *HelpMessage
    = "Floating window handler\n";

/*
 * Floats focused window.
 */
internal void
ForceFloatWin(void)
{
    API.Log(C_LOG_LEVEL_DEBUG, "chunkwm-float: floating focused window.\n");

    char *FloatCommand = (char *) malloc(
        sizeof(char) * (strlen("chunkc tiling::window --toggle float")));

    sprintf(FloatCommand, "chunkc tiling::window --toggle float");

    system(FloatCommand);
}

/*
 * NOTE(koekeishiya): We need a way to retrieve AXUIElementRef from a CGWindowID.
 * There is no way to do this, without caching AXUIElementRef references.
 * Here we perform a lookup of macos_window structs.
 */
internal inline macos_window *
_GetWindowByID(uint32_t Id)
{
    macos_window_map_it It = Windows.find(Id);
    return It != Windows.end() ? It->second : NULL;
}

macos_window *GetWindowByID(uint32_t Id)
{
    pthread_mutex_lock(&WindowsLock);
    macos_window *Result = _GetWindowByID(Id);
    pthread_mutex_unlock(&WindowsLock);
    return Result;
}

uint32_t GetFocusedWindowId()
{
    AXUIElementRef ApplicationRef, WindowRef;
    uint32_t WindowId = 0;

    ApplicationRef = AXLibGetFocusedApplication();
    if (!ApplicationRef) goto out;

    WindowRef = AXLibGetFocusedWindow(ApplicationRef);
    if (!WindowRef) goto err;

    WindowId = AXLibGetWindowID(WindowRef);
    CFRelease(WindowRef);

err:
    CFRelease(ApplicationRef);

out:
    return WindowId;
}

macos_window *GetFocusedWindow()
{
    uint32_t WindowId = GetFocusedWindowId();
    return WindowId ? GetWindowByID(WindowId) : NULL;
}

// put for now but want this in utils?
bool IsWindowValid(macos_window *Window)
{
    bool Result;
    if (AXLibHasFlags(Window, Window_Invalid)) {
        Result = false;
    } else if (AXLibHasFlags(Window, Window_ForceTile)) {
        Result = true;
    } else {
        Result = ((AXLibIsWindowStandard(Window)) &&
                  (AXLibHasFlags(Window, Window_Movable)) &&
                  (AXLibHasFlags(Window, Window_Resizable)));
    }
    return Result;
}

// NOTE(koekeishiya): Caller is responsible for making sure that the window is not a dupe.
internal void
AddWindowToCollection(macos_window *Window)
{
    if (!Window->Id) return;
    pthread_mutex_lock(&WindowsLock);
    Windows[Window->Id] = Window;
    pthread_mutex_unlock(&WindowsLock);
    //ApplyRulesForWindow(Window);
}

internal macos_window *
RemoveWindowFromCollection(macos_window *Window)
{
    pthread_mutex_lock(&WindowsLock);
    macos_window *Result = _GetWindowByID(Window->Id);
    if (Result) {
        Windows.erase(Window->Id);
    }
    pthread_mutex_unlock(&WindowsLock);
    return Result;
}

internal void
ClearWindowCache()
{
    pthread_mutex_lock(&WindowsLock);
    for (macos_window_map_it It = Windows.begin(); It != Windows.end(); ++It) {
        macos_window *Window = It->second;
        AXLibDestroyWindow(Window);
    }
    Windows.clear();
    pthread_mutex_unlock(&WindowsLock);
}

internal void
AddApplicationWindowList(macos_application *Application)
{
    macos_window **WindowList = AXLibWindowListForApplication(Application);
    if (!WindowList) {
        return;
    }

    macos_window **List = WindowList;
    macos_window *Window;
    while ((Window = *List++)) {
        if (GetWindowByID(Window->Id)) {
            AXLibDestroyWindow(Window);
        } else {
            AddWindowToCollection(Window);
        }
    }

    free(WindowList);
}

internal void
UpdateWindowCollection()
{
    for (macos_application_map_it It = Applications.begin(); It != Applications.end(); ++It) {
        macos_application *Application = It->second;
        AddApplicationWindowList(Application);
    }
}

internal void
AddApplication(macos_application *Application)
{
    Applications[Application->PID] = Application;
}

internal void
RemoveApplication(macos_application *Application)
{
    macos_application_map_it It = Applications.find(Application->PID);
    if (It != Applications.end()) {
        macos_application *Copy = It->second;
        AXLibDestroyApplication(Copy);
        Applications.erase(Application->PID);
    }
}

internal void
ClearApplicationCache()
{
    for (macos_application_map_it It = Applications.begin(); It != Applications.end(); ++It) {
        macos_application *Application = It->second;
        AXLibDestroyApplication(Application);
    }
    Applications.clear();
}

internal bool
GetSpaceAndDesktopId(macos_space **SpaceDest, unsigned *IdDest)
{
    macos_space *ActiveSpace;
    bool Result = AXLibActiveSpace(&ActiveSpace);

    if (!Result) {
        return false;
    }

    unsigned DesktopId = 1;
    Result = AXLibCGSSpaceIDToDesktopID(ActiveSpace->Id, NULL, &DesktopId);

    if (!Result) {
        return false;
    }

    *SpaceDest = ActiveSpace;
    *IdDest = DesktopId;

    return true;
}

internal void
Handler_Daemon(void *Data)
{
    chunkwm_payload *Payload = (chunkwm_payload *) Data;
    CommandCallback(Payload->SockFD, Payload->Command, Payload->Message);
}

internal void
Handler_WindowFloated(void *Data)
{
    uint32_t *Buf = (uint32_t *) Data;
    uint32_t WindowId = Buf[0];
    uint32_t Status = Buf[1];

    c_log(C_LOG_LEVEL_DEBUG, "chunkwm-float: received window %d float: %d\n", WindowId, Status);

    // validate
    macos_window *Window = GetWindowByID(WindowId);
    if (!Window || Status < 0 || Status > 1) { return; }

    // make sure we now it's floating
    if (Status) {
        AXLibAddFlags(Window, Window_Float);

        // TODO
        //WindowFloating_Resize();
    } else {
        AXLibClearFlags(Window, Window_Float);
    }
}

internal void
Handler_WindowCreated(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    macos_window *Copy = AXLibCopyWindow(Window);
    AddWindowToCollection(Copy);
    // TODO - float window here if need be
}

internal void
Handler_WindowDestroyed(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    macos_window *Copy = AXLibCopyWindow(Window);
    RemoveWindowFromCollection(Copy);
}

internal void
Handler_ApplicationActivated(void *Data)
{
    macos_application *Application = (macos_application *) Data;
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
    if (WindowRef) {
        uint32_t WindowId = AXLibGetWindowID(WindowRef);
        CFRelease(WindowRef);
        // TODO
        //WindowFocusedHandler(WindowId);
    }
}

internal void
Handler_ApplicationLaunched(void *Data)
{
    macos_application *Application = (macos_application *) Data;

    macos_window **WindowList = AXLibWindowListForApplication(Application);
    if (WindowList) {
        macos_window **List = WindowList;
        macos_window *Window;
        while ((Window = *List++)) {
            if (GetWindowByID(Window->Id)) {
                AXLibDestroyWindow(Window);
            } else {
                AddWindowToCollection(Window);
                //if (RuleChangedDesktop(Window->Flags)) continue;
                //if (RuleTiledWindow(Window->Flags))    continue;
                //TileWindow(Window);
            }
        }

        free(WindowList);
    }
}

internal void
Handler_ApplicationTerminated(void *Data)
{
    macos_application *Application = (macos_application *) Data;
    RemoveApplication(Application);
    //RebalanceWindowTree();
}

//internal void
//Handler_ApplicationHidden(void *Data)
//{
//    //RebalanceWindowTree();
//}

internal void
Handler_ApplicationUnhidden(void *Data)
{
    macos_application *Application = (macos_application *) Data;

    macos_space *Space;
    macos_window *Window, **List, **WindowList;

    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if (Space->Type != kCGSSpaceUser) {
        goto space_free;
    }

    List = WindowList = AXLibWindowListForApplication(Application);
    if (!WindowList) {
        goto space_free;
    }

    while ((Window = *List++)) {
        macos_window *Copy = RemoveWindowFromCollection(Window);
        if (Copy) {
            AXLibDestroyWindow(Copy);
        }

        AddWindowToCollection(Window);

        /*
        if (Window && AXLibSpaceHasWindow(Space->Id, Window->Id)) {
            uint32_t LastFocusedWindowId = CVarUnsignedValue(CVAR_LAST_FOCUSED_WINDOW);
            if (LastFocusedWindowId) {
                UpdateCVar(CVAR_BSP_INSERTION_POINT, LastFocusedWindowId);
            }

            TileWindow(Window);

            if (LastFocusedWindowId) {
                UpdateCVar(CVAR_BSP_INSERTION_POINT, 0);
            }
        }
        */
    }

    free(WindowList);

space_free:
    AXLibDestroySpace(Space);
}


/*
 * NOTE(koekeishiya):
 * parameter: const char *Node
 * parameter: void *Data
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if (StringEquals(Node, "chunkwm_export_window_created")) {
        Handler_WindowCreated(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_window_destroyed")) {
        Handler_WindowDestroyed(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_application_activated")) {
        Handler_ApplicationActivated(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_application_launched")) {
        Handler_ApplicationLaunched(Data);
        return true;
    //} else if (StringEquals(Node, "chunkwm_export_application_hidden")) {
    //    Handler_ApplicationHidden(Data);
    //    return true;
    } else if (StringEquals(Node, "chunkwm_export_application_unhidden")) {
        Handler_ApplicationUnhidden(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_export_application_terminated")) {
        Handler_ApplicationTerminated(Data);
        return true;
    } else if (StringEquals(Node, "chunkwm_daemon_command")) {
        Handler_Daemon(Data);
        return true;
    } else if (StringEquals(Node, "Tiling_focused_window_float")) {
        Handler_WindowFloated(Data);
        return true;
    } else {
    // TODO - want a cvar setting to auto-float the first window on a space
    // note - no clue how to do this since i'll need to call float from the tiling plugin
    }

    return false;
}

internal bool
Init(chunkwm_api ChunkwmAPI)
{
    bool Success;
    std::vector<macos_application *> Applications;
    uint32_t ProcessPolicy;

    macos_space *Space;
    unsigned DesktopId;

    API = ChunkwmAPI;
    c_log = API.Log;
    BeginCVars(&API);

    Success = (pthread_mutex_init(&WindowsLock, NULL) == 0);
    if (!Success) goto out;

    CreateCVar(CVAR_FLOAT_MOVE, 0.05f);
    CreateCVar(CVAR_FLOAT_RESIZE, 0.025f);

    CreateCVar(CVAR_FLOAT_OFFSET_TOP, 5.0f);
    CreateCVar(CVAR_FLOAT_OFFSET_BOTTOM, 5.0f);
    CreateCVar(CVAR_FLOAT_OFFSET_LEFT, 5.0f);
    CreateCVar(CVAR_FLOAT_OFFSET_RIGHT, 5.0f);

    ProcessPolicy = Process_Policy_Regular | Process_Policy_LSUIElement;
    Applications = AXLibRunningProcesses(ProcessPolicy);
    for (size_t Index = 0; Index < Applications.size(); ++Index) {
        macos_application *Application = Applications[Index];
        AddApplication(Application);
        AddApplicationWindowList(Application);
    }

    //Success = AXLibActiveSpace(&Space);
    //ASSERT(Success);

    goto out;

    // TODO - only clear these if init is unsuccessful
    ClearApplicationCache();
    ClearWindowCache();

out:
    c_log(C_LOG_LEVEL_WARN, "Initialized float plugin.\n");
    return Success;
}

internal void
Deinit()
{
    //EndEventTap
    ClearApplicationCache();
    ClearWindowCache();
}

/*
 * NOTE(koekeishiya):
 * parameter: chunkwm_api ChunkwmAPI
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    // TODO - in the future, can possibly have a FloatAllSpaces for startup
    return Init(ChunkwmAPI);
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    Deinit();
}

// NOTE(koekeishiya): Initialize plugin function pointers.
CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)

// NOTE(koekeishiya): Subscribe to ChunkWM events!
// clang-format off
chunkwm_plugin_export Subscriptions[] = {
    chunkwm_export_application_terminated,
    chunkwm_export_application_launched,
    chunkwm_export_application_activated,
    chunkwm_export_application_deactivated,
    chunkwm_export_application_hidden,
    chunkwm_export_application_unhidden,

    chunkwm_export_window_created,
    chunkwm_export_window_destroyed,
    chunkwm_export_window_minimized,
    chunkwm_export_window_deminimized,
    chunkwm_export_window_focused,

    chunkwm_export_space_changed,
    chunkwm_export_display_changed,
    chunkwm_export_display_resized,
};
// clang-format on
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN(PluginName, PluginVersion);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

bool
DoFloatSpace(CGSSpaceID SpaceId)
{
    API.Log(C_LOG_LEVEL_DEBUG, "chunkwm-float: DoFloatSpace called.\n");

    std::vector<macos_application *> ApplicationList
        = AXLibRunningProcesses(Process_Policy_Regular);

    for (int i = 0; i < ApplicationList.size(); ++i) {
        macos_window **WindowList = AXLibWindowListForApplication(ApplicationList[i]);
        if (!WindowList) {
            continue;
        }

        macos_window *Window;
        while ((Window = *WindowList++)) {
            if (AXLibSpaceHasWindow(SpaceId, Window->Id)
                && !AXLibHasFlags(Window, Window_Minimized)) {
                ForceFloatWin();
            }
        }
    }

    return true;
}

int
NumberOfWindowsOnSpace(CGSSpaceID SpaceId)
{
    int NumberOfWindows = 0;

    std::vector<macos_application *> ApplicationList
        = AXLibRunningProcesses(Process_Policy_Regular);

    for (int i = 0; i < ApplicationList.size(); ++i) {
        macos_window **WindowList = AXLibWindowListForApplication(ApplicationList[i]);
        if (!WindowList) {
            continue;
        }

        macos_window *Window;
        while ((Window = *WindowList++)) {
            if (AXLibSpaceHasWindow(SpaceId, Window->Id)
                && !AXLibHasFlags(Window, Window_Minimized)) {
                NumberOfWindows++;
            }
        }
    }

    return NumberOfWindows;
}
