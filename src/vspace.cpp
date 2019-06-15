/*
 * NOTE
 *
 * Copy from tiling plugin
 *
 * In its current state, this file is a pared-down version of vspace.cpp from the
 * chunkwm tiling plugin, reworked for a subset of its original functionality
 */
#include "vspace.h"
#include "constants.h"

#include "../chunkwm/src/common/accessibility/element.h"
#include "../chunkwm/src/common/accessibility/display.h"
#include "../chunkwm/src/common/misc/assert.h"
#include "../chunkwm/src/common/config/cvar.h"

#include <stdlib.h>
#include <pthread.h>

#define internal static
#define local_persist static

internal virtual_space_map VirtualSpaces;
internal pthread_mutex_t VirtualSpacesLock;

internal virtual_space *
CreateAndInitVirtualSpace(macos_space *Space)
{
    virtual_space *VirtualSpace = (virtual_space *) malloc(sizeof(virtual_space));
    //VirtualSpace->Tree = NULL;
    //VirtualSpace->Preselect = NULL;

    // TODO(koekeishiya): How do we react if this call fails ??
    bool Mutex = pthread_mutex_init(&VirtualSpace->Lock, NULL) == 0;
    ASSERT(Mutex);

    // NOTE(koekeishiya): The monitor arrangement is not necessary here.
    // We are able to address spaces using mission-control indexing.
    unsigned DesktopId = 1;
    bool Success = AXLibCGSSpaceIDToDesktopID(Space->Id, NULL, &DesktopId);
    ASSERT(Success);

    //virtual_space_config Config = GetVirtualSpaceConfig(DesktopId);
    //VirtualSpace->Mode = Config.Mode;
    //VirtualSpace->TreeLayout = Config.TreeLayout;
    //VirtualSpace->_Offset = Config.Offset;
    //VirtualSpace->Offset = &VirtualSpace->_Offset;

    return VirtualSpace;
}

// NOTE(koekeishiya): If the requested space does not exist, we create it.
virtual_space *AcquireVirtualSpace(macos_space *Space)
{
    virtual_space *VirtualSpace;

    char *SpaceCRef = CopyCFStringToC(Space->Ref);
    ASSERT(SpaceCRef);

    pthread_mutex_lock(&VirtualSpacesLock);
    virtual_space_map_it It = VirtualSpaces.find(SpaceCRef);
    if (It != VirtualSpaces.end()) {
        VirtualSpace = It->second;
        free(SpaceCRef);
    } else {
        VirtualSpace = CreateAndInitVirtualSpace(Space);
        VirtualSpaces[SpaceCRef] = VirtualSpace;
    }
    pthread_mutex_unlock(&VirtualSpacesLock);

    pthread_mutex_lock(&VirtualSpace->Lock);
    return VirtualSpace;
}

void ReleaseVirtualSpace(virtual_space *VirtualSpace)
{
    pthread_mutex_unlock(&VirtualSpace->Lock);
}
