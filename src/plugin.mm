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

extern chunkwm_log *c_log;

#include "config.cpp"
#include "controller.cpp"

#define internal static

bool DoFloatSpace(CGSSpaceID SpaceId);
int NumberOfWindowsOnSpace(CGSSpaceID SpaceId);

typedef std::map<pid_t, macos_application *> macos_application_map;
typedef macos_application_map::iterator macos_application_map_it;

typedef std::map<uint32_t, macos_window *> macos_window_map;
typedef macos_window_map::iterator macos_window_map_it;

internal const char *PluginName = "float";
internal const char *PluginVersion = "0.1.0";

internal macos_application_map Applications;
internal macos_window_map Windows;
internal pthread_mutex_t WindowsLock;
internal event_tap EventTap;
internal chunkwm_api API;
chunkwm_log *c_log;

internal const char *HelpMessage
    = "Floating window handler\n";

inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
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

internal void
BroadcastFocusedWindowFloating()
{
    uint32_t Data[2] = { 0, 0 };
    API.Broadcast(PluginName, "focused_window_float", (char *) Data, sizeof(Data));
}

void BroadcastFocusedWindowFloating(macos_window *Window)
{
    uint32_t Status = (uint32_t) AXLibHasFlags(Window, Window_Float);
    uint32_t Data[2] = { Window->Id, Status };
    API.Broadcast(PluginName, "focused_window_float", (char *) Data, sizeof(Data));
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
    } else if (StringsAreEqual(Node, "chunkwm_daemon_command")) {
        Handler_Daemon(Data);
        return true;
    } else {
        macos_space *Space;
        unsigned DesktopId = 1;
        bool Result = GetSpaceAndDesktopId(&Space, &DesktopId);
        if (!Result) {
            return false;
        }

        if (Space->Type != kCGSSpaceUser) {
            return true;
        }

        int NumberOfWindows = NumberOfWindowsOnSpace(Space->Id);
        bool Floating = NumberOfWindows <= 5; // && DoFloatSpace(Space->Id);
        //c_log(C_LOG_LEVEL_WARN, "number of windows: %d\n", NumberOfWindows);

        return Floating;
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
    //BeginCVars(&API);

    Success = (pthread_mutex_init(&WindowsLock, NULL) == 0);
    if (!Success) goto out;

    // create cvars

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
    API.Log(C_LOG_LEVEL_DEBUG, "float: DoFloatSpace called.\n");

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
                FloatWindow(Window);
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
