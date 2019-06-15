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

//#include "util.cpp"
#include "controller.h"

#include "controller.cpp"

#define internal static

bool DoFloatSpace(CGSSpaceID SpaceId);
int NumberOfWindowsOnSpace(CGSSpaceID SpaceId);

typedef std::map<uint32_t, macos_window *> macos_window_map;
typedef macos_window_map::iterator macos_window_map_it;

internal const char *PluginName = "float";
internal const char *PluginVersion = "0.1.0";
internal chunkwm_api API;

internal macos_window_map Windows;
internal pthread_mutex_t WindowsLock;

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

internal void
CommandHandler(void *Data)
{
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

/*
 * NOTE(koekeishiya):
 * parameter: const char *Node
 * parameter: void *Data
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if (StringsAreEqual(Node, "chunkwm_daemon_command")) {
        CommandHandler(Data);
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
        bool Floating = NumberOfWindows == 1 && DoFloatSpace(Space->Id);

        return Floating;
    }

    return false;
}

/*
 * NOTE(koekeishiya):
 * parameter: chunkwm_api ChunkwmAPI
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    API = ChunkwmAPI;
    // in the future, can possibly have a FloatAllSpaces for startup
    return true;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
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

    chunkwm_export_display_changed,
    chunkwm_export_space_changed,
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
