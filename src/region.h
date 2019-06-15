#ifndef PLUGIN_REGION_H
#define PLUGIN_REGION_H

#include <CoreGraphics/CGGeometry.h>

#include "controller.h"

struct region
{
    float X, Y;
    float Width, Height;
};

struct region_offset
{
    float Top, Bottom;
    float Left, Right;
    float Gap;
};

// struct node;
struct macos_space;
struct virtual_space;

bool ResultIsInsideRegion(const region New, const region Bounds);

region CGRectToRegion(CGRect Rect);
region RegionFromPointAndSize(CGPoint Position, CGSize Size);
void ConstrainResultToRegion(region *Result, const region Region, window_cmd Cmd);
// void ConstrainRegion(CFStringRef DisplayRef, region *Region);
// region FullscreenRegion(CFStringRef DisplayRef, virtual_space *VirtualSpace);
 
 #endif
