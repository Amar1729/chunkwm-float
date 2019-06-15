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

internal region_offset
GetVirtualSpaceOffset()
{
    region_offset Offset = {
        CVarFloatingPointValue(CVAR_FLOAT_OFFSET_TOP),
        CVarFloatingPointValue(CVAR_FLOAT_OFFSET_BOTTOM),
        CVarFloatingPointValue(CVAR_FLOAT_OFFSET_LEFT),
        CVarFloatingPointValue(CVAR_FLOAT_OFFSET_RIGHT),
    };

    return Offset;
}

internal virtual_space *
CreateAndInitVirtualSpace()
{
    virtual_space *VirtualSpace = (virtual_space *) malloc(sizeof(virtual_space));

    // TODO(koekeishiya): How do we react if this call fails ??
    bool Mutex = pthread_mutex_init(&VirtualSpace->Lock, NULL) == 0;
    ASSERT(Mutex);

    region_offset Offset = GetVirtualSpaceOffset();
    VirtualSpace->_Offset = Offset;
    VirtualSpace->Offset = &VirtualSpace->_Offset;

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
        VirtualSpace = CreateAndInitVirtualSpace();
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
