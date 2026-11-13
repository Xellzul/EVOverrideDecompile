/* bar.h -- spaceport bar. Notes: Research/notes/headers.md */
#ifndef BAR_H
#define BAR_H

#include "evglobals.h"

extern PicHandle barPics[12];
extern PicHandle slotPics[4];
extern DialogPtr barDialog;
extern DialogPtr slotDialog;

void DoBarDialog(void);
pascal Boolean BarFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void BarDialogUpdate(void);
void DoNewsDialog(void);
void NewsDialogUpdate(void);
void RandomizeNewsDialog(void);
void RunSlotMachine(void);
void DrawScrollbarPict(void);
void DrawSlotMachineReels(void);
void RunBarPersonEncounter(void);

#endif
