/* status.h -- HUD status panel. Notes: Research/notes/headers.md */
#ifndef STATUS_H
#define STATUS_H

#include "evglobals.h"

void RedrawStatus(void);
void DrawStatus(void);
void DrawStatusRadar(Boolean quiet);
void DrawStatusNav(void);
void DrawStatusShield(void);
void DrawStatusFuel(void);
void DrawStatusTarg(void);
void DrawStatusWeap(void);
void DrawStatusCargo(void);

#endif
