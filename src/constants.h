#ifndef PLUGIN_CONSTANTS_H
#define PLUGIN_CONSTANTS_H

/* ---- used in vspace to determine the screen region ---- */
// from tiling: try to remove this
#define BUFFER_SIZE                 256

#define _CVAR_SPACE_OFFSET_TOP      "desktop_offset_top"
#define _CVAR_SPACE_OFFSET_BOTTOM   "desktop_offset_bottom"
#define _CVAR_SPACE_OFFSET_LEFT     "desktop_offset_left"
#define _CVAR_SPACE_OFFSET_RIGHT    "desktop_offset_right"
#define _CVAR_SPACE_OFFSET_GAP      "desktop_offset_gap"

#define CVAR_SPACE_OFFSET_TOP       "global_" _CVAR_SPACE_OFFSET_TOP
#define CVAR_SPACE_OFFSET_BOTTOM    "global_" _CVAR_SPACE_OFFSET_BOTTOM
#define CVAR_SPACE_OFFSET_LEFT      "global_" _CVAR_SPACE_OFFSET_LEFT
#define CVAR_SPACE_OFFSET_RIGHT     "global_" _CVAR_SPACE_OFFSET_RIGHT
#define CVAR_SPACE_OFFSET_GAP       "global_" _CVAR_SPACE_OFFSET_GAP
/* ---- used in vspace to determine the screen region ---- */

#define CVAR_FLOAT_STEPSIZE     "float_step_size"

#endif
