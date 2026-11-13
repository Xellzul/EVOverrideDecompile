/* pending.h -- functions whose source file is not drafted yet. Notes: Research/notes/headers.md */
#ifndef PENDING_H
#define PENDING_H

#include "evglobals.h"

void HandlePlayer(ShipObj *player);
void FadeOut(long speed, RGBColor *color);
void FadeIn(long speed);
void FadeToColor(short speed, RGBColor *color);
void FadeToClut(short speed, CTabHandle clut);
GDHandle TryMainMonitor(short depth, short *origDepth, short width, short height);
void RestoreClut(Boolean setEntries);
void RestoreOriginalClut(Boolean setEntries);
void RemoveNotificationIfDone(void);

#endif
