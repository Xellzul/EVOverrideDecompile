/* communications.h -- comm dialogs. Notes: Research/notes/headers.md */
#ifndef COMMUNICATIONS_H
#define COMMUNICATIONS_H

#include "evglobals.h"

extern DialogPtr commDialog;
extern DialogPtr haggleDialog;
extern DialogPtr plunderDialog;
extern short adviceRandom;
extern short bribeRandom;
extern Boolean hatesPlayer;
extern PicHandle commPics[16];
extern PicHandle hagglePics[4];
extern PicHandle plunderPics[12];

void DoCommDialog(short shipIndex);
short DoShipCommPayment(void);
pascal Boolean CommFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void CommDialogUpdate(void);
void DoPlanetCommDialog(short stellar);
pascal Boolean PlanetCommFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void PlanetCommDialogUpdate(void);
void LoadAdvice(void);
short DoHaggleDialog(Boolean raise, short chance);
pascal Boolean HaggleFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void HaggleDialogUpdate(void);
void LoadResponse(short which);
void LoadStellarResponse(short which);
void DoPlunderDialog(void);
void SetPlunderValues(void);
pascal Boolean PlunderFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void PlunderDialogUpdate(void);

#endif
