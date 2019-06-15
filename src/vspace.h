#ifndef PLUGIN_VSPACE_H
#define PLUGIN_VSPACE_H

#include "region.h"

#include "../chunkwm/src/common/misc/string.h"
#include <stdint.h>
#include <pthread.h>
#include <map>

enum virtual_space_flags
{
    Virtual_Space_Require_Resize = 1 << 0,
    Virtual_Space_Require_Region_Update = 1 << 1,
};

struct virtual_space
{
    region_offset _Offset;

    region_offset *Offset;
    uint32_t Flags;

    pthread_mutex_t Lock;
};

typedef std::map<const char *, virtual_space *, string_comparator> virtual_space_map;
typedef virtual_space_map::iterator virtual_space_map_it;

struct macos_space;
virtual_space *AcquireVirtualSpace(macos_space *Space);
void ReleaseVirtualSpace(virtual_space *VirtualSpace);

#endif
