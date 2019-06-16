#ifndef PLUGIN_CONTROLLER_H
#define PLUGIN_CONTROLLER_H

#include <stdint.h>

enum window_cmd
{
    WindowMove = 0,
    WindowIncrement = 1,
    WindowDecrement = 2,
    WindowSetSize = 3,
};

struct macos_window;
struct macos_space;
struct virtual_space;

void IncWindow(char *Op);
void DecWindow(char *Op);
void MoveWindow(char *Op);
void AbsoluteSize(char *Op);
void CenterWindow(char *Unused);
void TemporaryStep(char *Size);

void QueryWindowCoord(char *Op, int SockFD);

#endif
