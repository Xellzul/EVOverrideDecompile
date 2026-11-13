/* Buttons.c -- the picture buttons of every dialog: one Track/Draw pair per button row. Notes: Research/notes/Buttons.md */
#include <Dialogs.h>
#include <Events.h>

#include "evo.h"

#define kGridScrollStep		4
#define kGridPageCells		20

#define kShipyardBuyItem	1
#define kShipyardDoneItem	7
#define kShipyardInfoItem	10
#define kShipyardScrollUpItem	12
#define kShipyardScrollDownItem	13

#define kOutfitDoneItem		1
#define kOutfitSellItem		4
#define kOutfitBuyItem		7
#define kOutfitScrollUpItem	10
#define kOutfitScrollDownItem	11

#define kPortRefuelItem		4
#define kPortTradeItem		7
#define kPortOutfitItem		8
#define kPortShipyardItem	9
#define kPortMissionsItem	10
#define kPortBarItem		11
#define kPortLeaveItem		12
#define kPortMissionsNoBarItem	13

#define kTradeDoneItem		1
#define kTradeBuyItem		13
#define kTradeSellItem		14


// FUNCTION: EVO 0x1000BE2C
short TrackMapButtons(Point where)
{
	Rect	box[4];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	newButton, button;

	ForeColor(blackColor);
	GetDialogItem(mapDialog, kMapDoneItem, &itemType, &item, &box[0]);
	GetDialogItem(mapDialog, kMapZoomOutItem, &itemType, &item, &box[1]);
	GetDialogItem(mapDialog, kMapZoomInItem, &itemType, &item, &box[2]);
	GetDialogItem(mapDialog, kMapClearRouteItem, &itemType, &item, &box[3]);
	button = -1;
	if (PtInRect(where, &box[0]))
		button = 0;
	if (PtInRect(where, &box[1]) && canZoomOut)
		button = 1;
	if (PtInRect(where, &box[2]) && canZoomIn)
		button = 2;
	if (PtInRect(where, &box[3]) && showClearRouteButton)
		button = 3;
	if (button != -1) {
		DrawMapButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			if (PtInRect(mouse, &box[0]))
				newButton = 0;
			if (PtInRect(mouse, &box[1]) && canZoomOut)
				newButton = 1;
			if (PtInRect(mouse, &box[2]) && canZoomIn)
				newButton = 2;
			if (PtInRect(mouse, &box[3]) && showClearRouteButton)
				newButton = 3;
			if (newButton != button)
				DrawMapButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000C06C
void DrawMapButtons(short hilite)
{
	Rect	box[4];
	short	itemType;
	Handle	item;

	ForeColor(blackColor);
	GetDialogItem(mapDialog, kMapDoneItem, &itemType, &item, &box[0]);
	GetDialogItem(mapDialog, kMapZoomOutItem, &itemType, &item, &box[1]);
	GetDialogItem(mapDialog, kMapZoomInItem, &itemType, &item, &box[2]);
	GetDialogItem(mapDialog, kMapClearRouteItem, &itemType, &item, &box[3]);
	if (hilite == 0)
		DrawPicture(mapPics[1], &box[0]);
	else
		DrawPicture(mapPics[0], &box[0]);
	if (canZoomOut) {
		if (hilite == 1)
			DrawPicture(mapPics[3], &box[1]);
		else
			DrawPicture(mapPics[2], &box[1]);
	} else
		PaintRect(&box[1]);
	if (canZoomIn) {
		if (hilite == 2)
			DrawPicture(mapPics[5], &box[2]);
		else
			DrawPicture(mapPics[4], &box[2]);
	} else
		PaintRect(&box[2]);
	if (showClearRouteButton) {
		if (hilite == 3)
			DrawPicture(mapPics[7], &box[3]);
		else
			DrawPicture(mapPics[6], &box[3]);
	} else
		PaintRect(&box[3]);
}

// FUNCTION: EVO 0x1000C244
short TrackShipyardButtons(Point where)
{
	Rect	box[5];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	i, count, newButton, button;

	ForeColor(blackColor);
	GetDialogItem(shipyardDialog, kShipyardDoneItem, &itemType, &item, &box[0]);
	GetDialogItem(shipyardDialog, kShipyardBuyItem, &itemType, &item, &box[1]);
	GetDialogItem(shipyardDialog, kShipyardInfoItem, &itemType, &item, &box[2]);
	GetDialogItem(shipyardDialog, kShipyardScrollUpItem, &itemType, &item, &box[3]);
	GetDialogItem(shipyardDialog, kShipyardScrollDownItem, &itemType, &item, &box[4]);
	button = -1;
	if (PtInRect(where, &box[1]) && canBuy)
		button = 1;
	if (PtInRect(where, &box[0]))
		button = 0;
	if (PtInRect(where, &box[2]) && selShipyardItem != -1)
		button = 2;
	if (PtInRect(where, &box[3]) && shipyardScroll >= kGridScrollStep)
		button = 3;
	if (PtInRect(where, &box[4])) {
		count = 0;
		for (i = 0; i < MaxShipClasses; i++)
			if (dockedPortShipTypes[i] != -1)
				count++;
		if (shipyardScroll < count - kGridPageCells)
			button = 4;
	}
	if (button != -1) {
		DrawShipyardButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			if (PtInRect(mouse, &box[1]) && canBuy)
				newButton = 1;
			if (PtInRect(mouse, &box[0]))
				newButton = 0;
			if (PtInRect(mouse, &box[2]) && selShipyardItem != -1)
				newButton = 2;
			if (PtInRect(mouse, &box[3]) && shipyardScroll >= kGridScrollStep)
				newButton = 3;
			if (PtInRect(mouse, &box[4])) {
				count = 0;
				for (i = 0; i < MaxShipClasses; i++)
					if (dockedPortShipTypes[i] != -1)
						count++;
				if (shipyardScroll < count - kGridPageCells)
					newButton = 4;
			}
			if (newButton != button)
				DrawShipyardButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000C56C
void DrawShipyardButtons(short hilite)
{
	Rect	box[5];
	short	itemType;
	Handle	item;
	short	i, count;

	GetDialogItem(shipyardDialog, kShipyardDoneItem, &itemType, &item, &box[0]);
	GetDialogItem(shipyardDialog, kShipyardBuyItem, &itemType, &item, &box[1]);
	GetDialogItem(shipyardDialog, kShipyardInfoItem, &itemType, &item, &box[2]);
	GetDialogItem(shipyardDialog, kShipyardScrollUpItem, &itemType, &item, &box[3]);
	GetDialogItem(shipyardDialog, kShipyardScrollDownItem, &itemType, &item, &box[4]);
	if (hilite == 0)
		DrawPicture(shipyardPics[3], &box[0]);
	else
		DrawPicture(shipyardPics[2], &box[0]);
	if (canBuy) {
		if (hilite == 1)
			DrawPicture(shipyardPics[1], &box[1]);
		else
			DrawPicture(shipyardPics[0], &box[1]);
	} else
		PaintRect(&box[1]);
	if (hilite == 2)
		DrawPicture(shipyardPics[7], &box[2]);
	else
		DrawPicture(shipyardPics[6], &box[2]);
	if (shipyardScroll >= kGridScrollStep) {
		if (hilite == 3)
			DrawPicture(shipyardPics[9], &box[3]);
		else
			DrawPicture(shipyardPics[8], &box[3]);
	} else
		PaintRect(&box[3]);
	count = 0;
	for (i = 0; i < MaxShipClasses; i++)
		if (dockedPortShipTypes[i] != -1)
			count++;
	if (shipyardScroll < count - kGridPageCells) {
		if (hilite == 4)
			DrawPicture(shipyardPics[11], &box[4]);
		else
			DrawPicture(shipyardPics[10], &box[4]);
	} else
		PaintRect(&box[4]);
}

// FUNCTION: EVO 0x1000C7B4
short TrackOutfitButtons(Point where)
{
	Rect	box[5];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	i, count, newButton, button;

	ForeColor(blackColor);
	GetDialogItem(outfitDialog, kOutfitDoneItem, &itemType, &item, &box[0]);
	GetDialogItem(outfitDialog, kOutfitBuyItem, &itemType, &item, &box[1]);
	GetDialogItem(outfitDialog, kOutfitSellItem, &itemType, &item, &box[2]);
	GetDialogItem(outfitDialog, kOutfitScrollUpItem, &itemType, &item, &box[3]);
	GetDialogItem(outfitDialog, kOutfitScrollDownItem, &itemType, &item, &box[4]);
	button = -1;
	if (PtInRect(where, &box[0]))
		button = 0;
	if (PtInRect(where, &box[1]))
		button = 1;
	if (PtInRect(where, &box[2]))
		button = 2;
	if (PtInRect(where, &box[3]))
		button = 3;
	if (PtInRect(where, &box[4]))
		button = 4;
	if (button != -1) {
		DrawOutfitButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			if (PtInRect(mouse, &box[0]))
				newButton = 0;
			if (PtInRect(mouse, &box[1]) && canBuy)
				newButton = 1;
			if (PtInRect(mouse, &box[2]) && canSell)
				newButton = 2;
			if (PtInRect(mouse, &box[3]) && outfitScroll >= kGridScrollStep)
				newButton = 3;
			if (PtInRect(mouse, &box[4])) {
				count = 0;
				for (i = 0; i < MaxOutfitItemTypes; i++)
					if (dockedPortItems[i] != -1)
						count++;
				if (outfitScroll < count - kGridPageCells)
					newButton = 4;
			}
			if (newButton != button)
				DrawOutfitButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000CA70
void DrawOutfitButtons(short hilite)
{
	Rect	box[5];
	Boolean	enabled[5];
	short	itemType;
	Handle	item;
	short	i, count;

	ForeColor(blackColor);
	GetDialogItem(outfitDialog, kOutfitDoneItem, &itemType, &item, &box[0]);
	GetDialogItem(outfitDialog, kOutfitBuyItem, &itemType, &item, &box[1]);
	GetDialogItem(outfitDialog, kOutfitSellItem, &itemType, &item, &box[2]);
	GetDialogItem(outfitDialog, kOutfitScrollUpItem, &itemType, &item, &box[3]);
	GetDialogItem(outfitDialog, kOutfitScrollDownItem, &itemType, &item, &box[4]);
	for (i = 0; i <= 4; i++)
		enabled[i] = true;
	if (!canBuy)
		enabled[1] = false;
	if (!canSell)
		enabled[2] = false;
	if (outfitScroll < kGridScrollStep)
		enabled[3] = false;
	count = 0;
	for (i = 0; i < MaxOutfitItemTypes; i++)
		if (dockedPortItems[i] != -1)
			count++;
	if (outfitScroll >= count - kGridPageCells)
		enabled[4] = false;
	for (i = 0; i <= 4; i++) {
		if (enabled[i]) {
			if (hilite == i)
				DrawPicture(outfitPics[i * 2 + 1], &box[i]);
			else
				DrawPicture(outfitPics[i * 2], &box[i]);
		} else {
			ForeColor(blackColor);
			PaintRect(&box[i]);
		}
	}
}

// FUNCTION: EVO 0x1000CCB8
short TrackPlanetButtons(Point where)
{
	Rect	box[7];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	i, newButton, button;
	Boolean	uninhabited;

	ForeColor(blackColor);
	GetDialogItem(portDialog, kPortLeaveItem, &itemType, &item, &box[0]);
	GetDialogItem(portDialog, kPortRefuelItem, &itemType, &item, &box[1]);
	GetDialogItem(portDialog, kPortTradeItem, &itemType, &item, &box[2]);
	GetDialogItem(portDialog, kPortOutfitItem, &itemType, &item, &box[3]);
	GetDialogItem(portDialog, kPortShipyardItem, &itemType, &item, &box[4]);
	if (SpobHasBar(currentSpob))
		GetDialogItem(portDialog, kPortMissionsItem, &itemType, &item, &box[5]);
	else
		GetDialogItem(portDialog, kPortMissionsNoBarItem, &itemType, &item, &box[5]);
	GetDialogItem(portDialog, kPortBarItem, &itemType, &item, &box[6]);
	button = -1;
	for (i = 0; i <= 6; i++)
		if (PtInRect(where, &box[i]))
			button = i;
	uninhabited = SpobUninhabited(currentSpob);
	if (button != -1) {
		DrawPlanetButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			if (PtInRect(mouse, &box[0]))
				newButton = 0;
			if (PtInRect(mouse, &box[1]) && !uninhabited && shipData->credits > 0
					&& shipData->fuel < ShipFuelCapacity(shipData))
				newButton = 1;
			if (PtInRect(mouse, &box[2]) && SpobHasTrade(currentSpob))
				newButton = 2;
			if (PtInRect(mouse, &box[3]) && SpobHasOutfitter(currentSpob))
				newButton = 3;
			if (PtInRect(mouse, &box[4]) && SpobHasShipyard(currentSpob))
				newButton = 4;
			if (PtInRect(mouse, &box[5]) && spobInhabited)
				newButton = 5;
			if (PtInRect(mouse, &box[6]) && SpobHasBar(currentSpob))
				newButton = 6;
			if (newButton != button)
				DrawPlanetButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000D004
void DrawPlanetButtons(short hilite)
{
	Rect	box[7];
	Boolean	enabled[7];
	short	itemType;
	Handle	item;
	short	i;
	Boolean	uninhabited;

	ForeColor(blackColor);
	GetDialogItem(portDialog, kPortLeaveItem, &itemType, &item, &box[0]);
	GetDialogItem(portDialog, kPortRefuelItem, &itemType, &item, &box[1]);
	GetDialogItem(portDialog, kPortTradeItem, &itemType, &item, &box[2]);
	GetDialogItem(portDialog, kPortOutfitItem, &itemType, &item, &box[3]);
	GetDialogItem(portDialog, kPortShipyardItem, &itemType, &item, &box[4]);
	if (SpobHasBar(currentSpob))
		GetDialogItem(portDialog, kPortMissionsItem, &itemType, &item, &box[5]);
	else
		GetDialogItem(portDialog, kPortMissionsNoBarItem, &itemType, &item, &box[5]);
	GetDialogItem(portDialog, kPortBarItem, &itemType, &item, &box[6]);
	uninhabited = SpobUninhabited(currentSpob);
	enabled[0] = true;
	if (shipData->credits > 0 && !uninhabited && shipData->fuel < ShipFuelCapacity(shipData))
		enabled[1] = true;
	else
		enabled[1] = false;
	if (SpobHasTrade(currentSpob))
		enabled[2] = true;
	else
		enabled[2] = false;
	if (SpobHasOutfitter(currentSpob))
		enabled[3] = true;
	else
		enabled[3] = false;
	if (SpobHasShipyard(currentSpob))
		enabled[4] = true;
	else
		enabled[4] = false;
	enabled[5] = spobInhabited;
	if (SpobHasBar(currentSpob))
		enabled[6] = true;
	else
		enabled[6] = false;
	for (i = 0; i <= 6; i++) {
		if (enabled[i]) {
			if (i == hilite)
				DrawPicture(portPics[i * 2 + 1], &box[i]);
			else
				DrawPicture(portPics[i * 2], &box[i]);
		}
	}
}

// FUNCTION: EVO 0x1000D2E4
short TrackTradeButtons(Point where)
{
	Rect	box[3];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	newButton, button;

	ForeColor(blackColor);
	GetDialogItem(tradeDialog, kTradeDoneItem, &itemType, &item, &box[0]);
	GetDialogItem(tradeDialog, kTradeBuyItem, &itemType, &item, &box[1]);
	GetDialogItem(tradeDialog, kTradeSellItem, &itemType, &item, &box[2]);
	button = -1;
	if (PtInRect(where, &box[0]))
		button = 0;
	if (PtInRect(where, &box[1]))
		button = 1;
	if (PtInRect(where, &box[2]))
		button = 2;
	if (button != -1) {
		DrawTradeButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			if (PtInRect(mouse, &box[0]))
				newButton = 0;
			if (PtInRect(mouse, &box[1]) && CanBuyGoods(selTradeItem))
				newButton = 1;
			if (PtInRect(mouse, &box[2]) && CanSellGoods(selTradeItem))
				newButton = 2;
			if (newButton != button)
				DrawTradeButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000D4C0
void DrawTradeButtons(short hilite)
{
	Rect	box[3];
	short	itemType;
	short	i;
	Handle	item;

	ForeColor(blackColor);
	GetDialogItem(tradeDialog, kTradeDoneItem, &itemType, &item, &box[0]);
	GetDialogItem(tradeDialog, kTradeBuyItem, &itemType, &item, &box[1]);
	GetDialogItem(tradeDialog, kTradeSellItem, &itemType, &item, &box[2]);
	if (hilite == 0)
		DrawPicture(tradePics[1], &box[0]);
	else
		DrawPicture(tradePics[0], &box[0]);
	if (CanBuyGoods(selTradeItem)) {
		if (hilite == 1)
			DrawPicture(tradePics[3], &box[1]);
		else
			DrawPicture(tradePics[2], &box[1]);
	} else
		PaintRect(&box[1]);
	if (CanSellGoods(selTradeItem)) {
		if (hilite == 2)
			DrawPicture(tradePics[5], &box[2]);
		else
			DrawPicture(tradePics[4], &box[2]);
	} else
		PaintRect(&box[2]);
}

// FUNCTION: EVO 0x1000D63C
short TrackCapAssignButtons(Point where)
{
	Rect	box[2];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	i, newButton, button;

	for (i = 0; i <= 1; i++)
		GetDialogItem(alertDialog, i + 1, &itemType, &item, &box[i]);
	button = -1;
	for (i = 0; i <= 1; i++)
		if (PtInRect(where, &box[i])) {
			button = i;
			break;
		}
	if (button != -1) {
		DrawCapAssignButtons(button);
		while (StillDown()) {
			newButton = -1;
			GetMouse(&mouse);
			for (i = 0; i <= 1; i++)
				if (PtInRect(mouse, &box[i])) {
					newButton = i;
					break;
				}
			if (newButton != button)
				DrawCapAssignButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000D7AC
void DrawCapAssignButtons(short hilite)
{
	Rect	box;
	short	itemType;
	Handle	item;
	short	i;

	for (i = 0; i <= 1; i++) {
		GetDialogItem(alertDialog, i + 1, &itemType, &item, &box);
		if (hilite == i)
			DrawPicture(capturePics[i * 2 + 1], &box);
		else
			DrawPicture(capturePics[i * 2], &box);
	}
}

// FUNCTION: EVO 0x1000D874
short TrackCommDialogButtons(Point where)
{
	Rect	box[4];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	i, newButton, button;

	for (i = 0; i <= 3; i++)
		GetDialogItem(commDialog, i + 1, &itemType, &item, &box[i]);
	button = -1;
	for (i = 0; i <= 3; i++)
		if (PtInRect(where, &box[i])) {
			button = i;
			break;
		}
	if (button != -1) {
		DrawCommDialogButtons(button);
		while (StillDown()) {
			newButton = -1;
			GetMouse(&mouse);
			for (i = 0; i <= 3; i++)
				if (PtInRect(mouse, &box[i])) {
					newButton = i;
					break;
				}
			if (newButton != button)
				DrawCommDialogButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000D9E4
void DrawCommDialogButtons(short hilite)
{
	Rect	box[4];
	short	itemType;
	Handle	item;
	short	i;

	for (i = 0; i <= 3; i++)
		GetDialogItem(commDialog, i + 1, &itemType, &item, &box[i]);
	if (hilite == 0)
		DrawPicture(commPics[9], &box[0]);
	else
		DrawPicture(commPics[8], &box[0]);
	if (ExtendedIsThreatToPlayer(dialogShip)) {
		if (hilite == 1)
			DrawPicture(commPics[3], &box[1]);
		else
			DrawPicture(commPics[2], &box[1]);
	} else {
		if (hilite == 1)
			DrawPicture(commPics[5], &box[1]);
		else
			DrawPicture(commPics[4], &box[1]);
	}
	if (hilite == 2)
		DrawPicture(commPics[1], &box[2]);
	else
		DrawPicture(commPics[0], &box[2]);
}

// FUNCTION: EVO 0x1000DB44
short TrackPlanetCommDialogButtons(Point where)
{
	Rect	box[4];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	i, newButton, button;

	for (i = 0; i <= 2; i++)
		GetDialogItem(commDialog, i + 1, &itemType, &item, &box[i]);
	button = -1;
	for (i = 0; i <= 2; i++)
		if (PtInRect(where, &box[i])) {
			button = i;
			break;
		}
	if (button != -1) {
		DrawPlanetCommDialogButtons(button);
		while (StillDown()) {
			newButton = -1;
			GetMouse(&mouse);
			for (i = 0; i <= 2; i++)
				if (PtInRect(mouse, &box[i])) {
					newButton = i;
					break;
				}
			if (newButton != button)
				DrawPlanetCommDialogButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000DCB4
void DrawPlanetCommDialogButtons(short hilite)
{
	Rect	box;
	short	itemType;
	short	i;
	Handle	item;

	ForeColor(blackColor);
	GetDialogItem(commDialog, 1, &itemType, &item, &box);
	if (hilite == 0)
		DrawPicture(commPics[1], &box);
	else
		DrawPicture(commPics[0], &box);
	GetDialogItem(commDialog, 3, &itemType, &item, &box);
	if (stellarData[shipData->targetStellar].dominated) {
		if (hilite == 2)
			DrawPicture(commPics[9], &box);
		else
			DrawPicture(commPics[8], &box);
	} else {
		if (hilite == 2)
			DrawPicture(commPics[7], &box);
		else
			DrawPicture(commPics[6], &box);
	}
	GetDialogItem(commDialog, 2, &itemType, &item, &box);
	if (hatesPlayer) {
		if (hilite == 1)
			DrawPicture(commPics[5], &box);
		else
			DrawPicture(commPics[4], &box);
	} else {
		if (hilite == 1)
			DrawPicture(commPics[3], &box);
		else
			DrawPicture(commPics[2], &box);
	}
	ForeColor(blackColor);
}

// FUNCTION: EVO 0x1000DE8C
short TrackMissionButtons(Point where)
{
	Rect	box[2];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	newButton, button;

	ForeColor(blackColor);
	GetDialogItem(missionDialog, 1, &itemType, &item, &box[0]);
	GetDialogItem(missionDialog, 7, &itemType, &item, &box[1]);
	button = -1;
	if (PtInRect(where, &box[0]))
		button = 0;
	if (PtInRect(where, &box[1]))
		button = 1;
	if (button != -1) {
		DrawMissionButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			if (PtInRect(mouse, &box[0]))
				newButton = 0;
			if (PtInRect(mouse, &box[1]))
				newButton = 1;
			if (newButton != button)
				DrawMissionButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000DFDC
void DrawMissionButtons(short hilite)
{
	Rect	box[2];
	short	itemType;
	Handle	item;
	short	i;

	ForeColor(blackColor);
	GetDialogItem(missionDialog, 1, &itemType, &item, &box[0]);
	GetDialogItem(missionDialog, 7, &itemType, &item, &box[1]);
	for (i = 0; i <= 1; i++) {
		if (hilite == i)
			DrawPicture(missionPics[i * 2 + 1], &box[i]);
		else
			DrawPicture(missionPics[i * 2], &box[i]);
	}
}

// FUNCTION: EVO 0x1000E0E0
short TrackMissionInfoButtons(Point where)
{
	Rect	box[2];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	newButton, button;

	ForeColor(blackColor);
	GetDialogItem(missionInfoDialog, 1, &itemType, &item, &box[0]);
	GetDialogItem(missionInfoDialog, 5, &itemType, &item, &box[1]);
	button = -1;
	if (PtInRect(where, &box[0]))
		button = 0;
	if (PtInRect(where, &box[1]) && selMissionListItem != -1)
		button = 1;
	if (button != -1) {
		DrawMissionInfoButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			if (PtInRect(mouse, &box[0]))
				newButton = 0;
			if (PtInRect(mouse, &box[1]) && selMissionListItem != -1)
				newButton = 1;
			if (newButton != button)
				DrawMissionInfoButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000E254
void DrawMissionInfoButtons(short hilite)
{
	Rect	box[2];
	short	itemType;
	short	i;
	Handle	item;

	ForeColor(blackColor);
	GetDialogItem(missionInfoDialog, 1, &itemType, &item, &box[0]);
	GetDialogItem(missionInfoDialog, 5, &itemType, &item, &box[1]);
	if (hilite == 0)
		DrawPicture(missionInfoPics[1], &box[0]);
	else
		DrawPicture(missionInfoPics[0], &box[0]);
	if (selMissionListItem == -1)
		PaintRect(&box[1]);
	else if (hilite == 1)
		DrawPicture(missionInfoPics[3], &box[1]);
	else
		DrawPicture(missionInfoPics[2], &box[1]);
}

// FUNCTION: EVO 0x1000E350
short TrackSingleMissionButtons(Point where)
{
	Rect	box[2];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	i, newButton, button;

	ForeColor(blackColor);
	if (canRefuseMission) {
		for (i = 0; i <= 1; i++)
			GetDialogItem(missionDialog, i + 1, &itemType, &item, &box[i]);
	} else {
		for (i = 0; i <= 1; i++)
			GetDialogItem(missionDialog, 6, &itemType, &item, &box[i]);
	}
	button = -1;
	for (i = 0; i <= 1; i++)
		if (PtInRect(where, &box[i]))
			button = i;
	if (button != -1) {
		DrawSingleMissionButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			for (i = 0; i <= 1; i++)
				if (PtInRect(mouse, &box[i]))
					newButton = i;
			if (newButton != button)
				DrawSingleMissionButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000E518
void DrawSingleMissionButtons(short hilite)
{
	Rect	box[2];
	short	itemType;
	Handle	item;
	short	i;

	ForeColor(blackColor);
	if (canRefuseMission) {
		for (i = 0; i <= 1; i++)
			GetDialogItem(missionDialog, i + 1, &itemType, &item, &box[i]);
		for (i = 0; i <= 1; i++) {
			if (hilite == i)
				DrawPicture(missionPics[i * 2 + 1], &box[i]);
			else
				DrawPicture(missionPics[i * 2], &box[i]);
		}
	} else {
		GetDialogItem(missionDialog, 6, &itemType, &item, &box[0]);
		if (hilite != -1)
			DrawPicture(missionPics[1], &box[0]);
		else
			DrawPicture(missionPics[0], &box[0]);
	}
}

// FUNCTION: EVO 0x1000E688
short TrackPlayerInfoButtons(short page, Point where)
{
	Rect	box[5];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	i, newButton, button;

	ForeColor(blackColor);
	for (i = 0; i <= 3; i++)
		GetDialogItem(infoDialog, i + 1, &itemType, &item, &box[i]);
	button = -1;
	for (i = 0; i <= 3; i++)
		if (PtInRect(where, &box[i])) {
			button = i;
			break;
		}
	if (button != -1) {
		DrawPlayerInfoButtons(page, button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			for (i = 0; i <= 3; i++)
				if (PtInRect(mouse, &box[i])) {
					newButton = i;
					break;
				}
			if (newButton != button)
				DrawPlayerInfoButtons(page, newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000E7F8
void DrawPlayerInfoButtons(short page, short hilite)
{
	Rect	box[5];
	short	itemType;
	Handle	item;
	short	i;

	ForeColor(blackColor);
	for (i = 0; i <= 3; i++) {
		GetDialogItem(infoDialog, i + 1, &itemType, &item, &box[i]);
		if (hilite == i || page == i)
			DrawPicture(infoPics[i * 2 + 1], &box[i]);
		else
			DrawPicture(infoPics[i * 2], &box[i]);
	}
}

// FUNCTION: EVO 0x1000E8EC
short TrackHaggleButtons(Point where)
{
	Rect	box[2];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	newButton, button;

	ForeColor(blackColor);
	GetDialogItem(haggleDialog, 1, &itemType, &item, &box[0]);
	GetDialogItem(haggleDialog, 2, &itemType, &item, &box[1]);
	button = -1;
	if (PtInRect(where, &box[0]))
		button = 0;
	if (PtInRect(where, &box[1]))
		button = 1;
	if (button != -1) {
		DrawHaggleButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			if (PtInRect(mouse, &box[0]))
				newButton = 0;
			if (PtInRect(mouse, &box[1]))
				newButton = 1;
			if (newButton != button)
				DrawHaggleButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000EA3C
void DrawHaggleButtons(short hilite)
{
	Rect	box[2];
	short	itemType;
	Handle	item;
	short	i;

	ForeColor(blackColor);
	GetDialogItem(haggleDialog, 1, &itemType, &item, &box[0]);
	GetDialogItem(haggleDialog, 2, &itemType, &item, &box[1]);
	for (i = 0; i <= 1; i++) {
		if (hilite == i)
			DrawPicture(hagglePics[i * 2 + 1], &box[i]);
		else
			DrawPicture(hagglePics[i * 2], &box[i]);
	}
}

// FUNCTION: EVO 0x1000EB40
short TrackPlunderButtons(Point where)
{
	Rect	box[6];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	i, newButton, button;

	ForeColor(blackColor);
	for (i = 0; i <= 3; i++)
		GetDialogItem(plunderDialog, i + 1, &itemType, &item, &box[i]);
	for (i = 4; i <= 5; i++)
		GetDialogItem(plunderDialog, i + 2, &itemType, &item, &box[i]);
	button = -1;
	for (i = 0; i <= 5; i++)
		if (PtInRect(where, &box[i]))
			button = i;
	if (button != -1) {
		DrawPlunderButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			for (i = 0; i <= 5; i++)
				if (PtInRect(mouse, &box[i]))
					newButton = i;
			if (newButton != button)
				DrawPlunderButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000ECF4
void DrawPlunderButtons(short hilite)
{
	Rect	box[6];
	short	itemType;
	Handle	item;
	short	i;

	for (i = 0; i <= 3; i++)
		GetDialogItem(plunderDialog, i + 1, &itemType, &item, &box[i]);
	for (i = 4; i <= 5; i++)
		GetDialogItem(plunderDialog, i + 2, &itemType, &item, &box[i]);
	ForeColor(blackColor);
	for (i = 0; i <= 5; i++) {
		if (hilite == i)
			DrawPicture(plunderPics[i * 2 + 1], &box[i]);
		else
			DrawPicture(plunderPics[i * 2], &box[i]);
	}
}

// FUNCTION: EVO 0x1000EE44
short TrackBarButtons(Point where)
{
	Rect	box[6];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	i, newButton, button;

	ForeColor(blackColor);
	for (i = 0; i <= 5; i++)
		GetDialogItem(barDialog, i + 1, &itemType, &item, &box[i]);
	button = -1;
	for (i = 0; i <= 5; i++)
		if (PtInRect(where, &box[i]))
			button = i;
	if (button != -1) {
		DrawBarButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			for (i = 0; i <= 5; i++)
				if (PtInRect(mouse, &box[i]))
					newButton = i;
			if (newButton != button)
				DrawBarButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000EFB8
void DrawBarButtons(short hilite)
{
	Rect	box[6];
	short	itemType;
	Handle	item;
	short	i;
	Boolean	enabled;

	for (i = 0; i <= 5; i++)
		GetDialogItem(barDialog, i + 1, &itemType, &item, &box[i]);
	ForeColor(blackColor);
	for (i = 0; i <= 5; i++) {
		enabled = true;
		if (i == 4)
			enabled = CanHireEscorts();
		if (i == 3)
			enabled = false;
		if (enabled) {
			if (hilite == i)
				DrawPicture(barPics[i * 2 + 1], &box[i]);
			else
				DrawPicture(barPics[i * 2], &box[i]);
		} else
			PaintRect(&box[i]);
	}
}

// FUNCTION: EVO 0x1000F100
short TrackSlotButtons(Point where)
{
	Rect	box[2];
	short	itemType;
	Handle	item;
	Point	mouse;
	short	i, newButton, button;

	ForeColor(blackColor);
	for (i = 0; i <= 1; i++)
		GetDialogItem(slotDialog, i + 4, &itemType, &item, &box[i]);
	button = -1;
	for (i = 0; i <= 1; i++)
		if (PtInRect(where, &box[i]))
			button = i;
	if (button != -1) {
		DrawSlotButtons(button);
		while (StillDown()) {
			GetMouse(&mouse);
			newButton = -1;
			if (PtInRect(mouse, &box[0]) && shipData->credits >= kSmallBet)
				newButton = 0;
			if (PtInRect(mouse, &box[1]) && shipData->credits >= kBigBet)
				newButton = 1;
			if (newButton != button)
				DrawSlotButtons(newButton);
			button = newButton;
		}
	}
	return button;
}

// FUNCTION: EVO 0x1000F278
void DrawSlotButtons(short hilite)
{
	Rect	box[2];
	short	itemType;
	Handle	item;
	short	i;

	for (i = 0; i <= 1; i++)
		GetDialogItem(slotDialog, i + 4, &itemType, &item, &box[i]);
	ForeColor(blackColor);
	if (shipData->credits >= kSmallBet) {
		if (hilite == 0)
			DrawPicture(slotPics[1], &box[0]);
		else
			DrawPicture(slotPics[0], &box[0]);
	} else
		PaintRect(&box[0]);
	if (shipData->credits >= kBigBet) {
		if (hilite == 1)
			DrawPicture(slotPics[3], &box[1]);
		else
			DrawPicture(slotPics[2], &box[1]);
	} else
		PaintRect(&box[1]);
}

// FUNCTION: EVO 0x1000F3B0
short TrackOkButton(Rect box)
{
	Point	mouse;
	Boolean	newHilite, hilite;

	GetMouse(&mouse);
	if (PtInRect(mouse, &box))
		hilite = true;
	else
		hilite = false;
	DrawOkButton(box, hilite);
	while (StillDown()) {
		GetMouse(&mouse);
		if (PtInRect(mouse, &box))
			newHilite = true;
		else
			newHilite = false;
		if (newHilite != hilite)
			DrawOkButton(box, newHilite);
		hilite = newHilite;
	}
	return hilite;
}

// FUNCTION: EVO 0x1000F498
void DrawOkButton(Rect box, Boolean hilite)
{
	if (hilite)
		DrawPicture(textDialogPics[1], &box);
	else
		DrawPicture(textDialogPics[0], &box);
}
