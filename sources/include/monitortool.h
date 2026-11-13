/* monitortool.h -- Monitor Tool. Notes: Research/notes/headers.md */
#ifndef MONITORTOOL_H
#define MONITORTOOL_H

#include "evglobals.h"

#define kMaxClutEntries	256

#pragma options align=mac68k

typedef struct FractColor {
	long	red;
	long	green;
	long	blue;
} FractColor;

#pragma options align=reset

OSErr OpenMonitorTool(short depth, short width, short height, Boolean tryMain, GDHandle *dev);
void CloseMonitorTool(void);
void SwitchedOut(void);
Boolean SwitchedIn(void);
Boolean ColorCheck(void);
void SetMonitorClut(CTabHandle ctab, Boolean setEntries);
void HueScale(short hue, Boolean mode);
void SetClutToColor(RGBColor *c, Boolean setEntries);
void MapToHue(RGBColor *c, Boolean setEntries);
GDHandle FindMonitor(short depth, short width, short height);
CTabHandle GetCurrentClut(void);
void SaveCurrentClut(CTabHandle dest, long *seed);
void ForceScreenUpdate(void);
void ForceMonitorUpdate(void);
void UnprotectMonitor(GDHandle dev);
Boolean DepthAvail(void);
void SubtractFractCluts(FractColor *a, FractColor *b, short last);
void FractClutToCTab(FractColor *f, CTabHandle ctab);
FractColor *CTabToFractClut(CTabHandle ctab);

#endif
