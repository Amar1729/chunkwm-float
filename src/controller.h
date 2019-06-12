#ifndef PLUGIN_CONTROLLER_H
#define PLUGIN_CONTROLLER_H

#include <stdint.h>

struct macos_window;
struct macos_space;
struct virtual_space;

void IncWindow(char *Op);
void DecWindow(char *Op);
void SetSize(char *Size);
void QueryWindowCoord(char *Op, int SockFD);

#endif
