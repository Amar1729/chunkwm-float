#include "region.h"
//#include "node.h"
#include "vspace.h"
#include "constants.h"

#include "../chunkwm/src/common/misc/assert.h"
#include "../chunkwm/src/common/accessibility/display.h"
#include "../chunkwm/src/common/accessibility/window.h"

#define internal static

region CGRectToRegion(CGRect Rect)
{
    region Result = { (float) Rect.origin.x,   (float) Rect.origin.y,
                      (float) Rect.size.width, (float) Rect.size.height, };
    return Result;
}

region RegionFromPointAndSize(CGPoint Position, CGSize Size)
{
    region Result = { (float) Position.x, (float) Position.y,
                      (float) Size.width, (float) Size.height, };
    return Result;
}

// // #define OSX_MENU_BAR_HEIGHT 22.0f
// // void ConstrainRegion(CFStringRef DisplayRef, region *Region)
// // {
// //     // NOTE(koekeishiya): Automatically adjust padding to account for osx menubar status.
// //     if (!AXLibIsMenuBarAutoHideEnabled()) {
// //         Region->Y += OSX_MENU_BAR_HEIGHT;
// //         Region->Height -= OSX_MENU_BAR_HEIGHT;
// //     }
// // 
// //     if (CVarIntegerValue(CVAR_BAR_ENABLED)) {
// //         bool ShouldApplyOffset = true;
// //         if (!CVarIntegerValue(CVAR_BAR_ALL_MONITORS)) {
// //             CFStringRef MainDisplayRef = AXLibGetDisplayIdentifierForMainDisplay();
// //             ASSERT(MainDisplayRef);
// // 
// //             if (CFStringCompare(DisplayRef, MainDisplayRef, 0) != kCFCompareEqualTo) {
// //                 ShouldApplyOffset = false;
// //             }
// // 
// //             CFRelease(MainDisplayRef);
// //         }
// // 
// //         if (ShouldApplyOffset) {
// //             Region->X += CVarFloatingPointValue(CVAR_BAR_OFFSET_LEFT);
// //             Region->Width -= CVarFloatingPointValue(CVAR_BAR_OFFSET_LEFT);
// // 
// //             Region->Y += CVarFloatingPointValue(CVAR_BAR_OFFSET_TOP);
// //             Region->Height -= CVarFloatingPointValue(CVAR_BAR_OFFSET_TOP);
// // 
// //             Region->Width -= CVarFloatingPointValue(CVAR_BAR_OFFSET_RIGHT);
// //             Region->Height -= CVarFloatingPointValue(CVAR_BAR_OFFSET_BOTTOM);
// //         }
// //     }
// // 
// //     if (!AXLibIsDockAutoHideEnabled()) {
// //         macos_dock_orientation Orientation = AXLibGetDockOrientation();
// //         size_t TileSize = AXLibGetDockTileSize() + 16;
// // 
// //         switch (Orientation) {
// //         case Dock_Orientation_Left: {
// //             CFStringRef LeftMostDisplayRef = AXLibGetDisplayIdentifierForLeftMostDisplay();
// //             ASSERT(LeftMostDisplayRef);
// // 
// //             if (CFStringCompare(DisplayRef, LeftMostDisplayRef, 0) == kCFCompareEqualTo) {
// //                 Region->X += TileSize;
// //                 Region->Width -= TileSize;
// //             }
// // 
// //             CFRelease(LeftMostDisplayRef);
// //         } break;
// //         case Dock_Orientation_Right: {
// //             CFStringRef RightMostDisplayRef = AXLibGetDisplayIdentifierForRightMostDisplay();
// //             ASSERT(RightMostDisplayRef);
// // 
// //             if (CFStringCompare(DisplayRef, RightMostDisplayRef, 0) == kCFCompareEqualTo) {
// //                 Region->Width -= TileSize;
// //             }
// // 
// //             CFRelease(RightMostDisplayRef);
// //         } break;
// //         case Dock_Orientation_Bottom: {
// //             CFStringRef BottomMostDisplayRef = AXLibGetDisplayIdentifierForBottomMostDisplay();
// //             ASSERT(BottomMostDisplayRef);
// // 
// //             if (CFStringCompare(DisplayRef, BottomMostDisplayRef, 0) == kCFCompareEqualTo) {
// //                 Region->Height -= TileSize;
// //             }
// // 
// //             CFRelease(BottomMostDisplayRef);
// //         } break;
// //         case Dock_Orientation_Top: { /* NOTE(koekeishiya) compiler warning.. */ } break;
// //         }
// //     }
// // }
// 
// region FullscreenRegion(CFStringRef DisplayRef, virtual_space *VirtualSpace)
// {
//     region Result = CGRectToRegion(AXLibGetDisplayBounds(DisplayRef));
//     ConstrainRegion(DisplayRef, &Result);
// 
//     region_offset *Offset = VirtualSpace->Offset;
//     if (Offset) {
//         Result.X += Offset->Left;
//         Result.Y += Offset->Top;
//         Result.Width -= (Offset->Left + Offset->Right);
//         Result.Height -= (Offset->Top + Offset->Bottom);
//     }
// 
//     return Result;
// }
