/* interface.h -- spaceport interface dialogs. Notes: Research/notes/headers.md */
#ifndef INTERFACE_H
#define INTERFACE_H

#include "evglobals.h"

extern DialogPtr alertDialog;
extern PicHandle textDialogPics[2];
extern PicHandle capturePics[4];

void DoTradeDialog(void);
pascal Boolean TradeFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void TradeDialogTabKey(Boolean backward);
void TradeDialogUpdate(void);
void DoPortDialog(SpobRecord *spob, short spobIndex);
pascal Boolean PortFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void PortDialogUpdate(void);
void DoOutfitDialog(void);
pascal Boolean OutfitFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void OutfitDialogUpdate(void);
Boolean CanBuyOutfitItem(void);
void PreloadOutfitIconStrip(void);
void DoShipyardDialog(void);
pascal Boolean ShipyardFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void ShipyardDialogUpdate(void);
void PreloadShipyardIconStrip(void);
void DoShipyardDescription();
pascal Boolean ShipDescFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void ShipDescDialogUpdate(void);
void ShowWaitDialog(void);
void CloseWaitDialog(void);
void SetupListRects(Rect *box);
void EVNoteAlert(StringPtr text);
Boolean EVYesNoDialog(StringPtr text);
Boolean EVTextInputDialog(StringPtr prompt, StringPtr text, short maxLen);
void ShowTextDialog(Boolean allowMap, Boolean redrawMission);
pascal Boolean TextDialogFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void TextDialogUpdate(void);
void ShowHectorDialog(void);
pascal Boolean HectorDialogFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void HectorDialogUpdate(void);
void DoPlayerInfoDialog(void);
pascal Boolean PlayerInfoFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void PlayerInfoDialogUpdate(void);
Boolean DoCapturedShipAssignmentDialog(void);
pascal Boolean CapturedAssignmentFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void CapturedAssignmentDialogUpdate(void);
void FrameDefaultButton(DialogPtr dlg, short item);
long ApplyPriceAndTechnologyFlux(long cost, float multiplier, short itemTech, short portTech);

#endif
