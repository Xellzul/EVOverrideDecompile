/* Interface.c -- the spaceport dialogs: trade, outfit, shipyard, ship specs, player info; the alerts and text dialogs. Notes: Research/notes/Interface.md */
#define OLDROUTINENAMES 1
#include <Dialogs.h>
#include <Fonts.h>
#include <Resources.h>
#include <TextUtils.h>
#include <TextEdit.h>
#include <ToolUtils.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "evo.h"

extern char			gSpobDesc[4096];
extern short		tradeState[6];

#define kWaitDlogID			130
#define kNoteAlertID		3000
#define kTextInputDlogID	3001
#define kYesNoDlogID		3002
#define kTextDlogID			3003
#define kHectorDlogID		4006
#define kPortDlogID			1000
#define kTradeDlogID		1001
#define kOutfitDlogID		1002
#define kShipyardDlogID		1004
#define kSpecsDlogID		1005
#define kInfoDlogID			1017
#define kCaptureDlogID		1018
#define kHectorPictID		150

#define kOptionLot			5
#define kCommandLot			10
#define kTradeLot			10
#define kBigTradeLot		100
#define kMinCommodityPrice	5
#define kShopColumns		4
#define kShopCells			20
#define kOutfitDescBase		3000
#define kOutfitPictBase		6000
#define kOutfitIconBase		6100
#define kShipDescBase		2000
#define kShipAltDescBase	2100
#define kShipPictBase		5000
#define kShipIconBase		5100
#define kNoShipDescID		2900
#define kNoShipPictID		5200
#define kSpobPictBase		10000

/* dialog items: the DITL numbers */
#define kAlertTextItem			3
#define kAlertEditItem			5
#define kAlertNoItem			5
#define kAlertCancelItem		6
#define kTextMapItem			4
#define kCaptureMyShipItem		1
#define kCaptureEscortItem		2
#define kInfoPageItem			6
#define kHectorTextItem			2
#define kHectorPictItem			3

#define kTradeMapItem			2
#define kTradeHeaderItem		3
#define kTradeFirstRowItem		4
#define kTradeLastRowItem		11
#define kTradeOtherItem			12
#define kTradeBuyItem			13
#define kTradeSellItem			14
#define kTradeNewsItem			15
#define kTradePilotItem			16
#define kTradeMissionsItem		17

#define kPortPilotItem			2
#define kPortMapItem			3
#define kPortRefuelItem			4
#define kPortPictItem			5
#define kPortDescItem			6
#define kPortTradeItem			7
#define kPortOutfitItem			8
#define kPortShipyardItem		9
#define kPortMissionsItem		10
#define kPortBarItem			11
#define kPortLeaveItem			12
#define kPortMissionLogItem		14

#define kOutfitMapItem			2
#define kOutfitQtyItem			3
#define kOutfitSellItem			4
#define kOutfitGridItem			5
#define kOutfitDescItem			6
#define kOutfitBuyItem			7
#define kOutfitPictItem			8
#define kOutfitPriceItem		9
#define kOutfitScrollUpItem		10
#define kOutfitScrollDownItem	11
#define kOutfitMissionLogItem	13

#define kShipyardBuyItem		1
#define kShipyardMapItem		3
#define kShipyardPilotItem		4
#define kShipyardGridItem		5
#define kShipyardDescItem		6
#define kShipyardLeaveItem		7
#define kShipyardPictItem		8
#define kShipyardPriceItem		9
#define kShipyardSpecsItem		10
#define kShipyardMissionLogItem	11
#define kShipyardScrollUpItem	12
#define kShipyardScrollDownItem	13

#define kSpecsPilotItem			2
#define kSpecsHeaderItem		3
#define kSpecsMapItem			4
#define kSpecsTextItem			5
#define kSpecsMissionLogItem	6

DialogPtr alertDialog;
DialogPtr waitDialog;
DialogPtr tradeDialog;
DialogPtr portDialog;
DialogPtr outfitDialog;
DialogPtr shipyardDialog;
DialogPtr specsDialog;
DialogPtr infoDialog;
char gOutfitDesc[1024];
short portPicID;
short selOutfitItem;
short selShipyardItem;
short outfitHover;
short infoPage;
short shipyardHover;
short outfitLastCell;
short outfitScroll;
short shipyardScroll;
short portSoundTimer;
short shipBuyMode;
short junkSlot[2];
long outfitClickWhen;
float priceMult[2];
PicHandle outfitPic;
PicHandle portPic;
PicHandle textDialogPics[2];
PicHandle infoPics[10];
PicHandle capturePics[4];
PicHandle outfitPics[10];
PicHandle tradePics[6];
PicHandle shipyardPics[12];
PicHandle portPics[14];
PicHandle hectorPic;
Ptr portSound;
Boolean canBuy;
Boolean canSell;
Boolean spobInhabited;
Boolean gCheatSoundPlayed;
Boolean daysPassed1;
Boolean daysPassed4;
Boolean modKeyHeld[2];
Boolean hasMapOutfit;
Boolean hasCleanRecordOutfit;
Str255 gTextInputResult;
Rect shopIconRect[MaxOutfitItemTypes];
Rect shopCellRect[kShopCells];
Rect shopCellIcon[kShopCells];
Boolean onPlanet;

// FUNCTION: EVO 0x10034C20
void DoTradeDialog(void)
{
	short			i;
	short			space;
	short			held;
	short			unit;
	long			qty;
	Rect			box;
	Rect			pad1;
	Rect			pad2;
	short			itemHit;
	short			itemType;
	short			pad5;
	short			savedSystem;
	long			maxBuy;
	long			pad3;
	Handle			item;
	long			pad4;
	Str255			buf;
	Boolean			done;
	ModalFilterUPP	filterUPP;
	double			mult;

	done = false;
	filterUPP = NewModalFilterProc(TradeFilter);
	for (i = 0; i <= 5; i++)
		tradePics[i] = GetPicture(kLeave78ButtonPict + i);
	savedSystem = shipData->targetStellar;
	mult = 1.25;
	if (currentSpob->govt != -1 && legalStatus[currentSpob->system] < 0)
		mult = 1.1;
	if (currentSpob->dominated)
		mult = 1.5;
	for (i = 0; i <= 5; i++) {
		tradeState[i] = CalcPortDemand(i, currentSpob->flags);
		if (tradeState[i] == 0) {
			commodityPrice[i] = 0;
		} else {
			if (tradeState[i] == 1)
				commodityPrice[i] = basePrice[i] / mult;
			if (tradeState[i] == 2)
				commodityPrice[i] = basePrice[i];
			if (tradeState[i] == 4)
				commodityPrice[i] = mult * basePrice[i];
			if (commodityPrice[i] < kMinCommodityPrice)
				commodityPrice[i] = kMinCommodityPrice;
		}
	}
	for (i = 0; i < MaxDisasters; i++) {
		if (disasterData[i].countdown > 0 && shipData->targetStellar == disasterData[i].stellarID) {
			commodityPrice[disasterData[i].commodity] = basePrice[disasterData[i].commodity];
			commodityPrice[disasterData[i].commodity] += disasterData[i].amount;
			if (commodityPrice[disasterData[i].commodity] < kMinCommodityPrice)
				commodityPrice[disasterData[i].commodity] = kMinCommodityPrice;
		}
	}
	junkSlot[0] = -1;
	junkSlot[1] = -1;
	commodityPrice[6] = commodityPrice[7] = 0;
	for (i = 0; i < kMaxJunkTypes; i++) {
		if (shipData->targetStellar == junkData[i].boughtAt) {
			junkSlot[0] = i;
			commodityPrice[6] = mult * junkData[i].basePrice;
		}
		if (shipData->targetStellar == junkData[i].soldAt) {
			junkSlot[1] = i;
			commodityPrice[7] = junkData[i].basePrice / mult;
		}
	}
	tradeDialog = nil;
	tradeDialog = GetNewDialog(kTradeDlogID, nil, (WindowPtr)-1);
	if (tradeDialog != nil) {
		NewDialogHook(tradeDialog, 0);
		CenterWindow(tradeDialog);
		selTradeItem = 7;
		TradeDialogTabKey(false);
		ShowWindow(tradeDialog);
		SelectWindow(tradeDialog);
		SetPort(tradeDialog);
		InvalRect(&tradeDialog->portRect);
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kOkItem)
				done = true;
			if (itemHit == kTradeMapItem) {
				DoSystemMap();
				shipData->targetStellar = savedSystem;
				PortDialogUpdate();
				SetPort(tradeDialog);
				InvalRect(&tradeDialog->portRect);
			}
			if (itemHit == kTradePilotItem) {
				DoPlayerInfoDialog();
				PortDialogUpdate();
				SetPort(tradeDialog);
				InvalRect(&tradeDialog->portRect);
			}
			if (itemHit == kTradeMissionsItem) {
				qty = 0;
				for (i = 0; i < MaxSimultaneousMissions; i++) {
					if (currentMissionObjectives[i].active)
						qty++;
				}
				if (qty > 0) {
					DoMissionInfoDialog();
					SetPort(portDialog);
					InvalRect(&portDialog->portRect);
					PortDialogUpdate();
					SetPort(tradeDialog);
					InvalRect(&tradeDialog->portRect);
				}
			}
			if (itemHit == kTradeBuyItem) {
				GetDialogItem(tradeDialog, kTradeBuyItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(tradeDialog, kTradeSellItem, &itemType, &item, &box);
				InvalRect(&box);
				space = TotalFleetHolds() - TotalCargo(shipData);
				if (space > 0) {
					maxBuy = shipData->credits / (float)commodityPrice[selTradeItem];
					if (KeyCheck(kKeyOption))
						qty = kBigTradeLot;
					else
						qty = kTradeLot;
					if (qty > space)
						qty = space;
					if (qty > maxBuy)
						qty = maxBuy;
					if (qty < 0)
						qty = 0;
					if (selTradeItem <= 5) {
						shipData->cargo[selTradeItem] += qty;
					} else if (junkSlot[selTradeItem - 6] != -1) {
						junkData[junkSlot[selTradeItem - 6]].held += qty;
					}
					shipData->credits -= qty * commodityPrice[selTradeItem];
					GetDialogItem(tradeDialog, selTradeItem + kTradeFirstRowItem, &itemType, &item, &box);
					InvalRect(&box);
					hudRedraw.credits = true;
					DrawStatus();
					SetPort(tradeDialog);
				}
			}
			if (itemHit == kTradeSellItem) {
				GetDialogItem(tradeDialog, kTradeBuyItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(tradeDialog, kTradeSellItem, &itemType, &item, &box);
				InvalRect(&box);
				if (selTradeItem <= 5) {
					held = shipData->cargo[selTradeItem];
				} else if (junkSlot[selTradeItem - 6] != -1) {
					held = junkData[junkSlot[selTradeItem - 6]].held;
				}
				if (held > 0) {
					if (KeyCheck(kKeyOption))
						unit = kBigTradeLot;
					else
						unit = kTradeLot;
					if (selTradeItem <= 5) {
						if (shipData->cargo[selTradeItem] % unit == 0)
							qty = unit;
						else
							qty = shipData->cargo[selTradeItem] % unit;
					} else if (junkSlot[selTradeItem - 6] != -1) {
						if (junkData[junkSlot[selTradeItem - 6]].held % unit == 0)
							qty = unit;
						else
							qty = junkData[junkSlot[selTradeItem - 6]].held % unit;
					}
					if (selTradeItem <= 5) {
						shipData->cargo[selTradeItem] -= qty;
					} else if (junkSlot[selTradeItem - 6] != -1) {
						junkData[junkSlot[selTradeItem - 6]].held -= qty;
					}
					shipData->credits += qty * commodityPrice[selTradeItem];
					GetDialogItem(tradeDialog, selTradeItem + kTradeFirstRowItem, &itemType, &item, &box);
					InvalRect(&box);
					hudRedraw.credits = true;
					DrawStatus();
					SetPort(tradeDialog);
				}
			}
		} while (!done);
		for (i = 0; i <= 5; i++) {
			HPurge((Handle)tradePics[i]);
			ReleaseResource((Handle)tradePics[i]);
		}
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(tradeDialog);
		RefreshSAT();
		shipData->targetStellar = savedSystem;
		hudRedraw.nav = true;
	}
}

// FUNCTION: EVO 0x1003579C
pascal Boolean TradeFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	Rect	box;
	Rect	r;
	short	itemType;
	Point	where;
	Point	mouse;
	Handle	item;
	short	i;
	short	j;
	short	old;
	short	hit;
	char	key;

	QuickKeyCheckHandler();
	if (QuickKeyCheck(keyMap[kCtlMap])) {
		*itemHit = kTradeMapItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlPlayerInfo])) {
		*itemHit = kTradePilotItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlMissions])) {
		*itemHit = kTradeMissionsItem;
		return true;
	}
	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		if (key == kReturnCharCode || key == kEnterCharCode) {
			ev->what = mouseDown;
			*itemHit = kOkItem;
			return true;
		}
		if (key == '	') {
			if (ev->modifiers & shiftKey)
				TradeDialogTabKey(true);
			else
				TradeDialogTabKey(false);
		}
		if (key == kDownArrowCharCode)
			TradeDialogTabKey(false);
		if (key == kUpArrowCharCode)
			TradeDialogTabKey(true);
		key = tolower(key);
		if (key == 'b') {
			*itemHit = kTradeBuyItem;
			return true;
		}
		if (key == 's') {
			*itemHit = kTradeSellItem;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		for (i = kTradeFirstRowItem; i <= kTradeLastRowItem; i++) {
			GetDialogItem(tradeDialog, i, &itemType, &item, &box);
			if (PtInRect(where, &box) && commodityPrice[i - kTradeFirstRowItem] != 0) {
				GetDialogItem(tradeDialog, selTradeItem + kTradeFirstRowItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(tradeDialog, kTradeBuyItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(tradeDialog, kTradeSellItem, &itemType, &item, &box);
				InvalRect(&box);
				old = selTradeItem;
				selTradeItem = i - kTradeFirstRowItem;
				while (StillDown()) {
					if (selTradeItem != old) {
						GetDialogItem(tradeDialog, selTradeItem + kTradeFirstRowItem, &itemType, &item, &box);
						InvalRect(&box);
						GetDialogItem(tradeDialog, old + kTradeFirstRowItem, &itemType, &item, &box);
						InvalRect(&box);
						TradeDialogUpdate();
					}
					GetMouse(&mouse);
					for (j = 0; j <= 7; j++) {
						GetDialogItem(tradeDialog, j + kTradeFirstRowItem, &itemType, &item, &box);
						if (PtInRect(mouse, &box) && commodityPrice[j] != 0) {
							old = selTradeItem;
							selTradeItem = j;
							break;
						}
					}
				}
				SetPort(tradeDialog);
				GetDialogItem(tradeDialog, selTradeItem + kTradeFirstRowItem, &itemType, &item, &box);
				InvalRect(&box);
				return false;
			}
		}
		hit = TrackTradeButtons(where);
		if (hit == 0) {
			*itemHit = kOkItem;
			return true;
		}
		if (hit == 1) {
			*itemHit = kTradeBuyItem;
			return true;
		}
		if (hit == 2) {
			*itemHit = kTradeSellItem;
			return true;
		}
		*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(tradeDialog);
		TradeDialogUpdate();
		EndUpdate(tradeDialog);
		return false;
	}
	return false;
}

// FUNCTION: EVO 0x10035C34
void TradeDialogTabKey(Boolean backward)
{
	Rect	box;
	Rect	r2;
	short	itemType;
	Handle	item;

	if (backward)
		selTradeItem--;
	else
		selTradeItem++;
	if (selTradeItem > 7)
		selTradeItem = 0;
	if (selTradeItem < 0)
		selTradeItem = 7;
	while (commodityPrice[selTradeItem] == 0) {
		if (backward)
			selTradeItem--;
		else
			selTradeItem++;
		if (selTradeItem > 7)
			selTradeItem = 0;
		if (selTradeItem < 0)
			selTradeItem = 7;
	}
	GetDialogItem(tradeDialog, kTradeBuyItem, &itemType, &item, &box);
	InvalRect(&box);
	GetDialogItem(tradeDialog, kTradeSellItem, &itemType, &item, &box);
	InvalRect(&box);
	GetDialogItem(tradeDialog, kTradeHeaderItem, &itemType, &item, &box);
	r2 = box;
	GetDialogItem(tradeDialog, kTradeOtherItem, &itemType, &item, &box);
	r2.bottom = box.bottom;
	InvalRect(&r2);
}

// FUNCTION: EVO 0x10035DDC
void TradeDialogUpdate(void)
{
	Rect		box;
	Rect		r;
	short		itemType;
	short		missionTons;
	short		junkHeld;
	short		missionKinds;
	short		junkKinds;
	Boolean		spare;
	Boolean		cargoSeen[MaxCargoTypes];
	Boolean		junkSeen[kMaxJunkTypes];
	Str255		name;
	char		text[1024];
	char		copy[255];
	Handle		item;
	RGBColor	green;
	short		i;
	short		m;
	short		n;
	short		p;
	short		kinds;
	Boolean		found;

	SATSetPort(&loadPort);
	ForeColor(blackColor);
	PaintRect(&tradeDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&tradeDialog->portRect);
	ForeColor(blackColor);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	GetDialogItem(tradeDialog, kTradeHeaderItem, &itemType, &item, &box);
	if (RectInRgn(&box, tradeDialog->visRgn)) {
		RGBForeColor(&rgbDarkGrey);
		FrameRect(&box);
		RGBForeColor(&rgbLightGrey);
		MoveTo(box.left + 6, box.top + 12);
		DrawString("\pCommodity:");
		MoveTo(box.left + 77, box.top + 12);
		if (TotalFleetHolds() > ShipTotalHolds())
			DrawString("\pIn Fleet:");
		else
			DrawString("\pIn Hold:");
		MoveTo(box.left + 127, box.top + 12);
		DrawString("\pPrice:");
		ForeColor(blackColor);
	}
	for (i = 0; i <= 7; i++) {
		GetDialogItem(tradeDialog, i + kTradeFirstRowItem, &itemType, &item, &box);
		if (RectInRgn(&box, tradeDialog->visRgn)) {
			if (i == selTradeItem) {
				r = box;
				InsetRect(&r, 1, 1);
				green.red = green.blue = 0;
				green.green = 0x6666;
				RGBForeColor(&green);
				PaintRect(&r);
				ForeColor(blackColor);
			}
			RGBForeColor(&rgbDarkGrey);
			MoveTo(box.left, box.top);
			LineTo(box.left, box.bottom - 1);
			MoveTo(box.right - 1, box.top);
			LineTo(box.right - 1, box.bottom - 1);
			if (i == 0) {
				MoveTo(box.left, box.top);
				LineTo(box.right - 1, box.top);
			}
			if (i == 7) {
				MoveTo(box.left, box.bottom - 1);
				LineTo(box.right - 1, box.bottom - 1);
			}
			if (commodityPrice[i] != 0) {
				ForeColor(whiteColor);
				if (i <= 5) {
					MoveTo(box.left + 6, box.top + 10);
					DrawString(cargoNames[i]);
					if (shipData->cargo[i] > 0) {
						NumToString(shipData->cargo[i], (StringPtr)text);
						MoveTo(box.left + 100 - StringWidth((StringPtr)text), box.top + 10);
						DrawString((StringPtr)text);
					}
				} else if (junkSlot[i - 6] != -1) {
					MoveTo(box.left + 6, box.top + 10);
					DrawString(junkData[junkSlot[i - 6]].name);
					if (junkData[junkSlot[i - 6]].held > 0) {
						NumToString(junkData[junkSlot[i - 6]].held, (StringPtr)text);
						MoveTo(box.left + 100 - StringWidth((StringPtr)text), box.top + 10);
						DrawString((StringPtr)text);
					}
				}
				MoveTo(box.left + 127, box.top + 10);
				found = false;
				for (kinds = 0; kinds < MaxDisasters; kinds++) {
					if (disasterData[kinds].countdown > 0 && shipData->targetStellar == disasterData[kinds].stellarID &&
						i == disasterData[kinds].commodity) {
						found = true;
						if (disasterData[kinds].amount > 0)
							DrawString("\pHigher");
						if (disasterData[kinds].amount < 0)
							DrawString("\pLower");
						break;
					}
				}
				if (!found) {
					if (i <= 5) {
						if (tradeState[i] == 1)
							DrawString("\pLow");
						if (tradeState[i] == 2)
							DrawString("\pMed");
						if (tradeState[i] == 4)
							DrawString("\pHigh");
					} else if (junkSlot[i - 6] != -1) {
						if (i == 6)
							DrawString("\pHigh");
						if (i == 7)
							DrawString("\pLow");
					}
				}
				NumToString(commodityPrice[i], (StringPtr)text);
				MoveTo(box.left + 190 - StringWidth((StringPtr)text), box.top + 10);
				DrawString((StringPtr)text);
			}
			ForeColor(blackColor);
		}
	}
	GetDialogItem(tradeDialog, kTradeOtherItem, &itemType, &item, &box);
	if (RectInRgn(&box, tradeDialog->visRgn)) {
		n = 0;
		kinds = 0;
		missionTons = 0;
		junkHeld = 0;
		missionKinds = 0;
		junkKinds = 0;
		for (i = 0; i < MaxCargoTypes; i++)
			cargoSeen[i] = 0;
		for (i = 0; i < kMaxJunkTypes; i++)
			junkSeen[i] = 0;
		for (i = 0; i <= 1; i++) {
			if (junkSlot[i] != -1)
				junkSeen[junkSlot[i]] = 1;
		}
		for (m = 0; m < MaxSimultaneousMissions; m++) {
			if (currentMissionObjectives[m].active && currentMissions[m].cargoPickedUp &&
				currentMissions[m].cargoType != -1) {
				n++;
				missionKinds++;
				missionTons += currentMissions[m].cargoQty;
				if (cargoSeen[currentMissions[m].cargoType] == 0) {
					cargoSeen[currentMissions[m].cargoType] = 1;
					kinds++;
				}
			}
		}
		i = 0;
		while (i < kMaxJunkTypes) {
			if (junkData[i].held > 0 && i != junkSlot[0] && i != junkSlot[1]) {
				n++;
				junkHeld += junkData[i].held;
				junkKinds++;
				kinds++;
			}
			i++;
		}
		if (n > 0) {
			ForeColor(blackColor);
			r = box;
			InsetRect(&r, 5, 3);
			p = 0;
			strncpy(text, "Other cargo: ", 20);
			if (missionKinds > 0) {
				if (missionTons > 0) {
					NumToString(missionTons, name);
					p2cstr(name);
					strcat(text, (char *)name);
					if (missionTons == 1)
						strcat(text, " ton");
					else
						strcat(text, " tons");
					strcat(text, " of ");
				}
				for (i = 0; i < MaxCargoTypes; i++)
					cargoSeen[i] = 0;
				for (m = 0; m < MaxSimultaneousMissions; m++) {
					if (currentMissionObjectives[m].active && currentMissions[m].cargoPickedUp &&
						currentMissions[m].cargoType != -1 && cargoSeen[currentMissions[m].cargoType] == 0) {
						cargoSeen[currentMissions[m].cargoType] = 1;
						strncpy((char *)name, lcCargoName[currentMissions[m].cargoType], 19);
						p2cstr(name);
						strcat(text, (char *)name);
						if (p < kinds - 2)
							strcat(text, ", ");
						if (p == kinds - 2) {
							if (kinds >= 3)
								strcat(text, ",");
							strcat(text, " and ");
						}
						p++;
					}
				}
			}
			if (junkKinds > 0) {
				for (i = 0; i < kMaxJunkTypes; i++) {
					if (junkData[i].held > 0 && junkSeen[i] == 0 && i != junkSlot[0] && i != junkSlot[1]) {
						strncpy((char *)name, lcJunkName[i], 19);
						p2cstr(name);
						strcat(text, (char *)name);
						if (p < kinds - 2)
							strcat(text, ", ");
						if (p == kinds - 2) {
							if (kinds >= 3)
								strcat(text, ",");
							strcat(text, " and ");
						}
						p++;
					}
				}
			}
			TETextBox(text, strlen(text), &r, teFlushDefault);
			InvertRect(&r);
			RGBForeColor(&rgbDarkGrey);
			FrameRect(&box);
			ForeColor(blackColor);
		}
	}
	GetDialogItem(tradeDialog, kOkItem, &itemType, &item, &box);
	if (RectInRgn(&box, tradeDialog->visRgn))
		DrawPicture(tradePics[0], &box);
	GetDialogItem(tradeDialog, kTradeBuyItem, &itemType, &item, &box);
	if (RectInRgn(&box, tradeDialog->visRgn)) {
		if (CanBuyGoods(selTradeItem))
			DrawPicture(tradePics[2], &box);
		else
			PaintRect(&box);
	}
	GetDialogItem(tradeDialog, kTradeSellItem, &itemType, &item, &box);
	if (RectInRgn(&box, tradeDialog->visRgn)) {
		if (CanSellGoods(selTradeItem))
			DrawPicture(tradePics[4], &box);
		else
			PaintRect(&box);
	}
	GetDialogItem(tradeDialog, kTradeNewsItem, &itemType, &item, &box);
	if (RectInRgn(&box, tradeDialog->visRgn)) {
		for (i = 0; i < MaxDisasters; i++) {
			if (disasterData[i].countdown > 0 && shipData->targetStellar == disasterData[i].stellarID) {
				strncpy(text, (char *)disasterData[i].name, 63);
				p2cstr((StringPtr)text);
				strcat(text, " has ");
				if (disasterData[i].amount > 0)
					strcat(text, "raised");
				else
					strcat(text, "lowered");
				strcat(text, " the price of ");
				strncpy((char *)name, lcCargoName[disasterData[i].commodity], 19);
				p2cstr(name);
				strcat(text, (char *)name);
				strcat(text, ".");
				strncpy(copy, text, 250);
				ForeColor(blackColor);
				TextFont(geneva);
				TextSize(kDialogTextSize);
				TETextBox(copy, strlen(copy), &box, teFlushDefault);
				InvertRect(&box);
				break;
			}
		}
	}
	SATSetPortScreen();
	SetPort(tradeDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &tradeDialog->portBits, &tradeDialog->portRect, &tradeDialog->portRect, srcCopy, tradeDialog->visRgn);
}

// FUNCTION: EVO 0x10036E74
void DoPortDialog(SpobRecord *spob, short spobIndex)
{
	Rect			pad0;
	Rect			box;
	short			itemHit;
	short			itemType;
	short			pad4;
	Handle			item;
	long			pad1;
	long			pad2;
	long			pad3;
	Boolean			done;
	ModalFilterUPP	filterUPP;
	short			i;
	short			n;
	short			savedTarget;
	Boolean			uninhabited;

	done = false;
	filterUPP = NewModalFilterProc(PortFilter);
	onPlanet = true;
	currentSpob = spob;
	daysPassed1 = daysPassed4 = false;
	strncpy(gSpobDesc, "", 5);
	LoadDescription(gSpobDesc, spobIndex + FirstResourceID);
	for (i = 0; i <= 1; i++)
		priceMult[i] = 1.1235;
	if (spob->customPict >= FirstResourceID)
		portPicID = spob->customPict;
	else
		portPicID = spob->type + kSpobPictBase;
	if (spob->customSound >= FirstResourceID) {
		portSound = LoadSndResource(spob->customSound);
		portSoundTimer = 0;
	} else {
		portSound = nil;
	}
	portPic = nil;
	portPic = GetPicture(portPicID);
	for (i = 0; i <= 9; i++)
		portPics[i] = GetPicture(i + kLeave200ButtonPict);
	portPics[10] = GetPicture(kMissionComputerButtonPict);
	portPics[11] = GetPicture(kMissionComputerButtonPict + 1);
	portPics[12] = GetPicture(kSpaceportBarButtonPict);
	portPics[13] = GetPicture(kSpaceportBarButtonPict + 1);
	portDialog = nil;
	portDialog = GetNewDialog(kPortDlogID, nil, (WindowPtr)-1);
	if (portDialog != nil) {
		NewDialogHook(portDialog, 0);
		CenterWindow(portDialog);
		ShowWindow(portDialog);
		SelectWindow(portDialog);
		SetPort(portDialog);
		FlushEvents(everyEvent - diskMask, 0);
		ForeColor(blackColor);
		InvalRect(&portDialog->portRect);
		if (SpobUninhabited(currentSpob))
			spobInhabited = false;
		else
			spobInhabited = true;
		SATSetPortOffScreen();
		ForeColor(blackColor);
		PaintRect(&gSAT.offScreen.bounds);
		SATSetPortScreen();
		RefreshSAT();
		DrawStatusRadar(1);
		DrawStatusCargo();
		SetPort(portDialog);
		PortDialogUpdate();
		MissionLandCheck();
		HandleStellarSystemVisibility();
		DrawStatusCargo();
		PortDialogUpdate();
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kPortLeaveItem)
				done = true;
			if (itemHit == kPortPilotItem) {
				DoPlayerInfoDialog();
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
			}
			if (itemHit == kPortMapItem) {
				savedTarget = shipData->targetStellar;
				DoSystemMap();
				shipData->targetStellar = savedTarget;
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
			}
			if (itemHit == kPortMissionLogItem) {
				n = 0;
				for (i = 0; i < MaxSimultaneousMissions; i++) {
					if (currentMissionObjectives[i].active)
						n++;
				}
				if (n > 0) {
					DoMissionInfoDialog();
					SetPort(portDialog);
					InvalRect(&portDialog->portRect);
				}
			}
			if (itemHit == kPortRefuelItem) {
				uninhabited = SpobUninhabited(currentSpob);
				if (!uninhabited) {
					n = ShipFuelCapacity(shipData) - shipData->fuel;
					if (!currentSpob->dominated) {
						if (n > shipData->credits)
							n = shipData->credits;
						shipData->credits -= n;
					}
					shipData->fuel += n;
					hudRedraw.fuel = true;
					hudRedraw.credits = true;
					DrawStatus();
					SetPort(portDialog);
				}
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
			}
			if (itemHit == kPortPictItem && portSound != nil)
				PlaySound(portSound, 10, kMidVolume, kMidVolume);
			if (itemHit == kPortTradeItem && SpobHasTrade(currentSpob)) {
				SetPort(portDialog);
				DrawPlanetButtons(-8);
				DoTradeDialog();
				hudRedraw.credits = true;
				DrawStatus();
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
			}
			if (itemHit == kPortOutfitItem && SpobHasOutfitter(currentSpob)) {
				GetDialogItem(portDialog, kPortOutfitItem, &itemType, &item, &box);
				InvalRect(&box);
				PortDialogUpdate();
				DoOutfitDialog();
				hudRedraw.credits = true;
				DrawStatus();
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
			}
			if (itemHit == kPortShipyardItem && SpobHasShipyard(currentSpob)) {
				GetDialogItem(portDialog, kPortShipyardItem, &itemType, &item, &box);
				InvalRect(&box);
				PortDialogUpdate();
				shipBuyMode = 0;
				DoShipyardDialog();
				hudRedraw.credits = true;
				DrawStatus();
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
			}
			if (itemHit == kPortMissionsItem && spobInhabited) {
				savedTarget = shipData->targetStellar;
				SetPort(portDialog);
				GetDialogItem(portDialog, kPortMissionsItem, &itemType, &item, &box);
				InvalRect(&box);
				PortDialogUpdate();
				DoMissionDialog(false);
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
				shipData->targetStellar = savedTarget;
				hudRedraw.credits = true;
				hudRedraw.nav = true;
				DrawStatus();
			}
			if (itemHit == kPortBarItem) {
				SetPort(portDialog);
				GetDialogItem(portDialog, kPortBarItem, &itemType, &item, &box);
				InvalRect(&box);
				PortDialogUpdate();
				if (SpobHasBar(currentSpob)) {
					DoBarDialog();
					DrawStatusCargo();
					SetPort(portDialog);
					InvalRect(&portDialog->portRect);
				}
			}
		} while (!done);
		for (i = 0; i <= 13; i++) {
			if (portPics[i] != nil) {
				HPurge((Handle)portPics[i]);
				ReleaseResource((Handle)portPics[i]);
			}
		}
		if (portPic != nil) {
			HPurge((Handle)portPic);
			ReleaseResource((Handle)portPic);
		}
		if (portSound != nil) {
			FlushMixQueueEntries((long)portSound);
			DisposePtr(portSound);
		}
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(portDialog);
		portDialog = nil;
		if (daysPassed1)
			IncrementGameTime();
		if (daysPassed4) {
			for (i = 0; i <= 3; i++)
				IncrementGameTime();
		}
		if (daysPassed1 || daysPassed4) {
			shipData->targetShip = -1;
			hudRedraw.ammo = true;
		}
		onPlanet = false;
		RefreshSAT();
	}
}

// FUNCTION: EVO 0x100377D8
pascal Boolean PortFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	short	itemType;
	Handle	item;
	Rect	box;
	Point	where;
	char	key;
	short	hit;

	if (SpobUninhabited(currentSpob))
		spobInhabited = false;
	else
		spobInhabited = true;
	if (portSound != nil) {
		portSoundTimer--;
		if (portSoundTimer <= 0) {
			PlaySound(portSound, 10, kMidVolume, kMidVolume);
			portSoundTimer = Rand(512) + 512;
		}
	}
	QuickKeyCheckHandler();
	if (QuickKeyCheck(keyMap[kCtlMap])) {
		*itemHit = kPortMapItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlPlayerInfo])) {
		*itemHit = kPortPilotItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlMissions])) {
		*itemHit = kPortMissionLogItem;
		return true;
	}
	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		key = tolower(key);
		if (key == kReturnCharCode || key == kEnterCharCode) {
			ev->what = mouseDown;
			*itemHit = kPortLeaveItem;
			return true;
		}
		if (key == 'r' || key == 'f') {
			*itemHit = kPortRefuelItem;
			return true;
		}
		if (key == 'c' || key == 'e' || key == 't') {
			*itemHit = kPortTradeItem;
			return true;
		}
		if (key == 'o') {
			*itemHit = kPortOutfitItem;
			return true;
		}
		if (key == 's') {
			*itemHit = kPortShipyardItem;
			return true;
		}
		if (key == 'n') {
			*itemHit = kPortMissionsItem;
			return true;
		}
		if (key == 'b') {
			*itemHit = kPortBarItem;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		hit = TrackPlanetButtons(where);
		GetDialogItem(portDialog, kPortPictItem, &itemType, &item, &box);
		if (PtInRect(where, &box)) {
			*itemHit = kPortPictItem;
			return true;
		}
		if (hit == 0) {
			*itemHit = kPortLeaveItem;
			return true;
		}
		if (hit == 1) {
			*itemHit = kPortRefuelItem;
			return true;
		}
		if (hit == 2) {
			*itemHit = kPortTradeItem;
			return true;
		}
		if (hit == 3) {
			*itemHit = kPortOutfitItem;
			return true;
		}
		if (hit == 4) {
			*itemHit = kPortShipyardItem;
			return true;
		}
		if (hit == 5) {
			*itemHit = kPortMissionsItem;
			return true;
		}
		if (hit == 6) {
			*itemHit = kPortBarItem;
			return true;
		}
		*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(portDialog);
		PortDialogUpdate();
		EndUpdate(portDialog);
		return false;
	}
	return false;
}

// FUNCTION: EVO 0x10037BB4
void PortDialogUpdate(void)
{
	Rect	r;
	Rect	box;
	Rect	pad;
	short	itemType;
	short	pad2;
	Str255	buf;
	Handle	item;
	long	pad3;

	SATSetPort(&loadPort);
	ForeColor(blackColor);
	PaintRect(&portDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&portDialog->portRect);
	ForeColor(blackColor);
	GetDialogItem(portDialog, kPortMapItem, &itemType, &item, &box);
	if (RectInRgn(&box, portDialog->visRgn)) {
		ForeColor(blackColor);
		PaintRect(&box);
		ForeColor(whiteColor);
		MoveTo(box.left, box.top + 18);
		TextFont(times);
		TextSize(18);
		DrawString(currentSpob->name);
		ForeColor(blackColor);
	}
	GetDialogItem(portDialog, kPortPictItem, &itemType, &item, &box);
	if (RectInRgn(&box, portDialog->visRgn)) {
		DrawPicture(portPic, &box);
		if (showAll) {
			r.top = box.top + 3;
			r.left = box.left + 3;
			r.right = r.left + 64;
			r.bottom = r.top + 11;
			ForeColor(blackColor);
			PaintRect(&r);
			MoveTo(r.left + 3, r.top + 9);
			TextFont(geneva);
			TextSize(kDialogTextSize);
			ForeColor(whiteColor);
			NumToString(FreeMem() / 1000, buf);
			DrawString(buf);
			DrawString("\pK free");
			ForeColor(blackColor);
		}
	}
	GetDialogItem(portDialog, kPortDescItem, &itemType, &item, &box);
	if (RectInRgn(&box, portDialog->visRgn)) {
		ForeColor(blackColor);
		TextFont(geneva);
		TextSize(kDialogTextSize);
		TETextBox(gSpobDesc, strlen(gSpobDesc), &box, teFlushDefault);
		InvertRect(&box);
	}
	DrawPlanetButtons(-8);
	SATSetPortScreen();
	SetPort(portDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &portDialog->portBits, &portDialog->portRect, &portDialog->portRect, srcCopy, portDialog->visRgn);
}

// FUNCTION: EVO 0x10037EE4
void DoOutfitDialog(void)
{
	Rect			pad1;
	short			pad2;
	Rect			box;
	Rect			pad0;
	short			itemHit;
	short			itemType;
	short			savedTarget;
	short			before;
	short			pad7;
	short			origCount[MaxOutfitItemTypes];
	long			pad8;
	Handle			item;
	Str255			pad3;
	long			pad4;
	long			pad5;
	long			pad6;
	Boolean			done;
	ModalFilterUPP	filterUPP;
	short			i;
	short			j;
	long			price;
	short			n;

	done = false;
	filterUPP = NewModalFilterProc(OutfitFilter);
	for (i = 0; i < MaxOutfitItemTypes; i++)
		origCount[i] = outfitCount[i];
	savedTarget = shipData->targetStellar;
	selOutfitItem = -1;
	outfitHover = -1;
	outfitScroll = 0;
	hasMapOutfit = hasCleanRecordOutfit = false;
	ShipStatsToSystemInfo();
	SetupPortAvailableItems(currentSpob);
	outfitDialog = nil;
	outfitDialog = GetNewDialog(kOutfitDlogID, nil, (WindowPtr)-1);
	if (outfitDialog != nil) {
		NewDialogHook(outfitDialog, 0);
		CenterWindow(outfitDialog);
		GetDialogItem(outfitDialog, kOutfitGridItem, &itemType, &item, &box);
		SetupListRects(&box);
		PreloadOutfitIconStrip();
		for (i = 0; i <= 5; i++)
			outfitPics[i] = GetPicture(i + kLeave78ButtonPict);
		for (i = 6; i <= 9; i++)
			outfitPics[i] = GetPicture(i + (kUpArrowButtonPict - 6));
		strncpy(gOutfitDesc, "", 3);
		ShowWindow(outfitDialog);
		SelectWindow(outfitDialog);
		SetPort(outfitDialog);
		FlushEvents(everyEvent - diskMask, 0);
		ForeColor(blackColor);
		InvalRect(&outfitDialog->portRect);
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kOkItem)
				done = true;
			if (itemHit == kOutfitMapItem) {
				DoSystemMap();
				shipData->targetStellar = savedTarget;
				PreloadOutfitIconStrip();
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
				PortDialogUpdate();
				SetPort(outfitDialog);
				InvalRect(&outfitDialog->portRect);
			}
			if (itemHit == kOutfitMissionLogItem) {
				n = 0;
				for (i = 0; i < MaxSimultaneousMissions; i++) {
					if (currentMissionObjectives[i].active)
						n++;
				}
				if (n > 0) {
					DoMissionInfoDialog();
					SetPort(portDialog);
					InvalRect(&portDialog->portRect);
					PortDialogUpdate();
					SetPort(outfitDialog);
					InvalRect(&outfitDialog->portRect);
				}
			}
			if (itemHit == kOutfitBuyItem && canBuy) {
				if (selOutfitItem != -1) {
					n = 1;
					if (KeyCheck(kKeyOption))
						n *= kOptionLot;
					if (KeyCheck(kKeyCommand))
						n *= kCommandLot;
					for (i = 0; i < n; i++) {
						if (canBuy) {
							price = ApplyPriceAndTechnologyFlux(gOutfits[selOutfitItem].cost, priceMult[0],
								gOutfits[selOutfitItem].techLevel, stellarData[shipData->targetStellar].techLevel);
							if (gOutfits[selOutfitItem].modType[0] == kOutfModMap) {
								AutoSetExploration(shipData->curSystem, gOutfits[selOutfitItem].modVal[0]);
								hasMapOutfit = true;
							} else if (gOutfits[selOutfitItem].modType[1] == kOutfModMap) {
								AutoSetExploration(shipData->curSystem, gOutfits[selOutfitItem].modVal[1]);
								hasMapOutfit = true;
							} else if (gOutfits[selOutfitItem].modType[0] == kOutfModCleanRecord ||
									gOutfits[selOutfitItem].modType[1] == kOutfModCleanRecord) {
								for (j = 0; j < MaxOutfitMods; j++) {
									if (gOutfits[selOutfitItem].modType[j] == kOutfModCleanRecord) {
										for (i = 0; i < MaxSystems; i++) {
											if (gSystems[i].loaded) {
												if (gOutfits[selOutfitItem].modVal[j] == -1) {
													if (legalStatus[i] < 0)
														legalStatus[i] = 0;
												} else if (gSystems[i].govt == gOutfits[selOutfitItem].modVal[j] &&
														legalStatus[i] < 0) {
													legalStatus[i] = 0;
												}
											}
										}
									}
								}
								hasCleanRecordOutfit = true;
							} else if (gOutfits[selOutfitItem].modType[0] == kOutfModMissionBit ||
									gOutfits[selOutfitItem].modType[1] == kOutfModMissionBit) {
								for (j = 0; j < MaxOutfitMods; j++) {
									if (gOutfits[selOutfitItem].modType[j] == kOutfModMissionBit) {
										if (gOutfits[selOutfitItem].modVal[j] < kNotBitBase)
											missionBit[gOutfits[selOutfitItem].modVal[j]] = true;
										else
											missionBit[gOutfits[selOutfitItem].modVal[j] - kNotBitBase] = false;
									}
								}
								outfitCount[selOutfitItem]++;
							} else {
								outfitCount[selOutfitItem]++;
							}
							shipData->credits -= price;
						}
						canBuy = CanBuyOutfitItem();
					}
					GetDialogItem(outfitDialog, kOutfitSellItem, &itemType, &item, &box);
					InvalRect(&box);
					GetDialogItem(outfitDialog, kOutfitGridItem, &itemType, &item, &box);
					InvalRect(&box);
					GetDialogItem(outfitDialog, kOutfitBuyItem, &itemType, &item, &box);
					InvalRect(&box);
					GetDialogItem(outfitDialog, kOutfitPriceItem, &itemType, &item, &box);
					InvalRect(&box);
					daysPassed1 = true;
					hudRedraw.credits = true;
					DrawStatus();
					SetPort(outfitDialog);
				}
			}
			if (itemHit == kOutfitSellItem && canSell) {
				n = 1;
				if (KeyCheck(kKeyOption))
					n *= kOptionLot;
				if (KeyCheck(kKeyCommand))
					n *= kCommandLot;
				for (i = 0; i < n; i++) {
					if (canSell) {
						price = ApplyPriceAndTechnologyFlux(gOutfits[selOutfitItem].cost, priceMult[0],
							gOutfits[selOutfitItem].techLevel, stellarData[shipData->targetStellar].techLevel);
						if (outfitCount[selOutfitItem] <= origCount[selOutfitItem])
							price *= 0.8;
						before = ShipFreeMass();
						outfitCount[selOutfitItem]--;
						if (ShipFreeMass() < 0 && before >= 0) {
							outfitCount[selOutfitItem]++;
							PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
							break;
						}
						shipData->credits += price;
						if (outfitCount[selOutfitItem] <= 0 && gOutfits[selOutfitItem].modType[0] == kOutfModWeapon &&
								shipData->selectedWeapon == gOutfits[selOutfitItem].modVal[0]) {
							shipData->selectedWeapon = -1;
							hudRedraw.ammo = true;
						}
						if (outfitCount[selOutfitItem] <= 0 && gOutfits[selOutfitItem].modType[1] == kOutfModWeapon &&
								shipData->selectedWeapon == gOutfits[selOutfitItem].modVal[1]) {
							shipData->selectedWeapon = -1;
							hudRedraw.ammo = true;
						}
					}
					if (selOutfitItem != -1 && outfitCount[selOutfitItem] > 0) {
						if (OutfCantSell(&gOutfits[selOutfitItem]))
							canSell = false;
						else
							canSell = true;
					} else {
						canSell = false;
					}
				}
				GetDialogItem(outfitDialog, kOutfitSellItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(outfitDialog, kOutfitGridItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(outfitDialog, kOutfitBuyItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(outfitDialog, kOutfitPriceItem, &itemType, &item, &box);
				InvalRect(&box);
				daysPassed1 = true;
				hudRedraw.credits = true;
				DrawStatus();
				SetPort(outfitDialog);
			}
			if (itemHit == kOutfitScrollUpItem && outfitScroll >= kShopColumns) {
				outfitScroll -= kShopColumns;
				selOutfitItem = outfitHover = -1;
				strncpy(gOutfitDesc, "", 3);
				GetDialogItem(outfitDialog, kOutfitSellItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(outfitDialog, kOutfitBuyItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(outfitDialog, kOutfitGridItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(outfitDialog, kOutfitDescItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(outfitDialog, kOutfitPictItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(outfitDialog, kOutfitPriceItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(outfitDialog, kOutfitScrollUpItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(outfitDialog, kOutfitScrollDownItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(outfitDialog, kOutfitMissionLogItem, &itemType, &item, &box);
				InvalRect(&box);
			}
			if (itemHit == kOutfitScrollDownItem) {
				n = 0;
				for (i = 0; i < MaxOutfitItemTypes; i++) {
					if (dockedPortItems[i] != -1)
						n++;
				}
				if (outfitScroll < n - kShopCells) {
					outfitScroll += kShopColumns;
					selOutfitItem = outfitHover = -1;
					strncpy(gOutfitDesc, "", 3);
					GetDialogItem(outfitDialog, kOutfitSellItem, &itemType, &item, &box);
					InvalRect(&box);
					GetDialogItem(outfitDialog, kOutfitBuyItem, &itemType, &item, &box);
					InvalRect(&box);
					GetDialogItem(outfitDialog, kOutfitGridItem, &itemType, &item, &box);
					InvalRect(&box);
					GetDialogItem(outfitDialog, kOutfitDescItem, &itemType, &item, &box);
					InvalRect(&box);
					GetDialogItem(outfitDialog, kOutfitPictItem, &itemType, &item, &box);
					InvalRect(&box);
					GetDialogItem(outfitDialog, kOutfitPriceItem, &itemType, &item, &box);
					InvalRect(&box);
					GetDialogItem(outfitDialog, kOutfitScrollUpItem, &itemType, &item, &box);
					InvalRect(&box);
					GetDialogItem(outfitDialog, kOutfitScrollDownItem, &itemType, &item, &box);
					InvalRect(&box);
					GetDialogItem(outfitDialog, kOutfitMissionLogItem, &itemType, &item, &box);
					InvalRect(&box);
				}
			}
		} while (!done);
		n = ShipFuelCapacity(shipData);
		if (shipData->fuel > n) {
			shipData->fuel = n;
			hudRedraw.fuel = true;
		}
		n = ShipShieldCapacity(shipData);
		if (shipData->shield > n) {
			shipData->shield = n;
			hudRedraw.shield = true;
		}
		for (i = 0; i < MaxOutfitItemTypes; i++) {
			if (outfitCount[i] > 0 && (gOutfits[i].flags & kOutfRemoveAfterBuy))
				outfitCount[i] = 0;
		}
		SystemInfoToShipStats();
		for (i = 0; i <= 9; i++) {
			HPurge((Handle)outfitPics[i]);
			ReleaseResource((Handle)outfitPics[i]);
		}
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(outfitDialog);
		SATSetPortBackScreen();
		ForeColor(blackColor);
		PaintRect(&gSAT.backScreen.bounds);
		RedrawStatus();
		SATSetPortScreen();
		RefreshSAT();
		shipData->targetStellar = savedTarget;
		hudRedraw.nav = true;
	}
}

// FUNCTION: EVO 0x1003904C
pascal Boolean OutfitFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	Rect	box;
	Rect	box2;
	short	itemType;
	Handle	item;
	Point	where;
	Boolean	spare[10];
	Boolean	prevMod[2];
	char	key;
	short	i;
	short	row;
	short	col;
	short	prevCell;

	for (i = 0; i <= 1; i++) {
		prevMod[i] = modKeyHeld[i];
		modKeyHeld[i] = 0;
	}
	if (KeyCheck(kKeyOption))
		modKeyHeld[0] = 1;
	if (KeyCheck(kKeyCommand))
		modKeyHeld[1] = 1;
	for (i = 0; i <= 1; i++) {
		if (prevMod[i] != modKeyHeld[i]) {
			SetPort(outfitDialog);
			GetDialogItem(outfitDialog, kOutfitQtyItem, &itemType, &item, &box);
			InvalRect(&box);
			break;
		}
	}
	if (selOutfitItem != -1) {
		canBuy = CanBuyOutfitItem();
		if (outfitCount[selOutfitItem] > 0) {
			if (OutfCantSell(&gOutfits[selOutfitItem]))
				canSell = false;
			else
				canSell = true;
		} else {
			canSell = false;
		}
	} else {
		canSell = false;
		canBuy = false;
	}
	QuickKeyCheckHandler();
	if (QuickKeyCheck(keyMap[kCtlMap])) {
		*itemHit = kOutfitMapItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlMissions])) {
		*itemHit = kOutfitMissionLogItem;
		return true;
	}
	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		if (key == kReturnCharCode || key == kEnterCharCode) {
			ev->what = mouseDown;
			*itemHit = kOkItem;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		GetDialogItem(outfitDialog, kOutfitGridItem, &itemType, &item, &box);
		if (PtInRect(where, &box)) {
			col = (where.h - box.left) / 84;
			row = (where.v - box.top) / 55;
			if (col >= 0 && col <= 3 && row >= 0 && row <= 4) {
				prevCell = outfitHover;
				outfitHover = col + row * kShopColumns;
				if (dockedPortItems[outfitHover + outfitScroll] == -1) {
					outfitHover = selOutfitItem = -1;
				} else {
					selOutfitItem = dockedPortItems[outfitHover + outfitScroll];
				}
				if (outfitHover != prevCell) {
					GetDialogItem(outfitDialog, kOkItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(outfitDialog, kOutfitSellItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(outfitDialog, kOutfitBuyItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(outfitDialog, kOutfitGridItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(outfitDialog, kOutfitDescItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(outfitDialog, kOutfitPictItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(outfitDialog, kOutfitPriceItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(outfitDialog, kOutfitScrollUpItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(outfitDialog, kOutfitScrollDownItem, &itemType, &item, &box2);
					InvalRect(&box2);
					if (selOutfitItem != -1) {
						if (outfitPic != nil) {
							HPurge((Handle)outfitPic);
							ReleaseResource((Handle)outfitPic);
						}
						LoadDescription(gOutfitDesc, selOutfitItem + kOutfitDescBase);
						if (selOutfitItem < 100)
							outfitPic = GetPicture(selOutfitItem + kOutfitPictBase);
						else
							outfitPic = GetPicture(selOutfitItem + 6200);
					} else {
						strncpy(gOutfitDesc, "", 3);
					}
				}
				outfitLastCell = outfitHover;
				outfitClickWhen = ev->when;
			}
		}
		col = TrackOutfitButtons(where);
		if (col == 0) {
			*itemHit = kOkItem;
			return true;
		}
		if (col == 1) {
			*itemHit = kOutfitBuyItem;
			return true;
		}
		if (col == 2) {
			*itemHit = kOutfitSellItem;
			return true;
		}
		if (col == 3) {
			*itemHit = kOutfitScrollUpItem;
			return true;
		}
		if (col == 4) {
			*itemHit = kOutfitScrollDownItem;
			return true;
		}
		*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(outfitDialog);
		OutfitDialogUpdate();
		DrawDialog(outfitDialog);
		EndUpdate(outfitDialog);
		return false;
	}
	return false;
}

// FUNCTION: EVO 0x100396BC
void OutfitDialogUpdate(void)
{
	Rect	pad0;
	long	pad1;
	Rect	box;
	Rect	pad2;
	Rect	pad3;
	long	price;
	short	itemType;
	short	count;
	long	pad5;
	Handle	item;
	long	pad4;
	Str255	buf;
	short	i;
	short	n;
	short	mass;

	SATSetPort(&loadPort);
	ForeColor(blackColor);
	PaintRect(&outfitDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&outfitDialog->portRect);
	ForeColor(blackColor);
	GetDialogItem(outfitDialog, kOutfitQtyItem, &itemType, &item, &box);
	if (RectInRgn(&box, outfitDialog->visRgn)) {
		if (selOutfitItem != -1) {
			count = 1;
			if (KeyCheck(kKeyOption))
				count *= kOptionLot;
			if (KeyCheck(kKeyCommand))
				count *= kCommandLot;
			if (count > 1 && (canBuy || canSell)) {
				TextFont(geneva);
				TextSize(kDialogTextSize);
				ForeColor(whiteColor);
				MoveTo(box.left, box.bottom);
				DrawString("\px");
				NumToString(count, buf);
				DrawString(buf);
				ForeColor(blackColor);
			}
		}
	}
	GetDialogItem(outfitDialog, kOutfitGridItem, &itemType, &item, &box);
	if (RectInRgn(&box, outfitDialog->visRgn)) {
		TextFont(geneva);
		TextSize(kDialogTextSize);
		ForeColor(blackColor);
		n = 0;
		for (i = outfitScroll; i < MaxOutfitItemTypes; i++) {
			if (RectInRgn(&shopCellRect[i - outfitScroll], outfitDialog->visRgn) && n < kShopCells &&
					dockedPortItems[i] != -1) {
				CopyBits(&gSAT.backScreen.port->portBits, &loadPort.port->portBits,
					&shopIconRect[dockedPortItems[i]], &shopCellIcon[i - outfitScroll], srcCopy, nil);
				if (outfitCount[dockedPortItems[i]] > 0) {
					NumToString(outfitCount[dockedPortItems[i]], buf);
					ForeColor(whiteColor);
					MoveTo(shopCellRect[i - outfitScroll].right - (StringWidth(buf) + 3), shopCellRect[i - outfitScroll].top + 12);
					DrawString(buf);
					ForeColor(blackColor);
				}
				strncpy((char *)buf, outfitShortName[dockedPortItems[i]], 19);
				MoveTo((shopCellRect[i - outfitScroll].left + shopCellRect[i - outfitScroll].right) / 2 - StringWidth(buf) / 2,
					shopCellRect[i - outfitScroll].bottom - 6);
				ForeColor(whiteColor);
				DrawString(buf);
				ForeColor(blackColor);
				RGBForeColor(&rgbRealDarkBlue);
				FrameRect(&shopCellRect[i - outfitScroll]);
				ForeColor(blackColor);
				n++;
			}
		}
		if (outfitHover != -1) {
			RGBForeColor(&brightGreen);
			FrameRect(&shopCellRect[outfitHover]);
			ForeColor(blackColor);
		}
	}
	if (outfitHover != -1) {
		GetDialogItem(outfitDialog, kOutfitPictItem, &itemType, &item, &box);
		if (RectInRgn(&box, outfitDialog->visRgn)) {
			if (outfitPic != nil) {
				DrawPicture(outfitPic, &box);
			} else {
				ForeColor(blackColor);
				PaintRect(&box);
				ForeColor(whiteColor);
				FrameRect(&box);
				TextFont(geneva);
				TextSize(kDialogTextSize);
				DrawCenteredText("\pNo Picture", box.left, box.right, box.top + 44);
				DrawCenteredText("\pAvailable", box.left, box.right, box.top + 56);
				ForeColor(blackColor);
			}
		}
		GetDialogItem(outfitDialog, kOutfitPriceItem, &itemType, &item, &box);
		if (RectInRgn(&box, outfitDialog->visRgn)) {
			TextFont(geneva);
			TextSize(kDialogTextSize);
			ForeColor(whiteColor);
			MoveTo(box.left, box.top + 12);
			DrawString("\pItem Price:");
			MoveTo(box.left + 70, box.top + 12);
			price = ApplyPriceAndTechnologyFlux(gOutfits[selOutfitItem].cost, priceMult[0],
				gOutfits[selOutfitItem].techLevel, stellarData[shipData->targetStellar].techLevel);
			DrawCommaNumber(price);
			DrawString("\p cr");
			MoveTo(box.left, box.top + 24);
			DrawString("\pYou Have:");
			MoveTo(box.left + 70, box.top + 24);
			DrawCommaNumber(shipData->credits);
			DrawString("\p cr");
			if (gOutfits[selOutfitItem].mass > 0) {
				MoveTo(box.left, box.top + 48);
				DrawString("\pItem Mass:");
				MoveTo(box.left + 70, box.top + 48);
				mass = gOutfits[selOutfitItem].mass;
				DrawCommaNumber(mass);
				DrawString("\p ton");
				if (mass != 1)
					DrawString("\ps");
				MoveTo(box.left, box.top + 60);
				DrawString("\pAvailable:");
				MoveTo(box.left + 70, box.top + 60);
				mass = ShipFreeMass();
				if (mass < 0)
					mass = 0;
				DrawCommaNumber(mass);
				DrawString("\p ton");
				if (mass != 1)
					DrawString("\ps");
			}
			if (HasMaxOfItem(selOutfitItem)) {
				MoveTo(box.left, box.top + 96);
				if (outfitCount[selOutfitItem] > 0)
					DrawString("\pCan\xd5t have any more!");
				else
					DrawString("\pCan\xd5t have any of this item!");
			} else if (gOutfits[selOutfitItem].mass > 0) {
				if (gOutfits[selOutfitItem].mass > ShipFreeMass()) {
					MoveTo(box.left, box.top + 96);
					if (outfitCount[selOutfitItem] > 0)
						DrawString("\pCan\xd5t hold any more!");
					else
						DrawString("\pCan\xd5t hold any of this item!");
				}
			}
			ForeColor(blackColor);
		}
	}
	GetDialogItem(outfitDialog, kOutfitDescItem, &itemType, &item, &box);
	if (RectInRgn(&box, outfitDialog->visRgn)) {
		TextFont(geneva);
		TextSize(kDialogTextSize);
		TETextBox(gOutfitDesc, strlen(gOutfitDesc), &box, teFlushDefault);
		InvertRect(&box);
	}
	DrawOutfitButtons(-1);
	SATSetPortScreen();
	SetPort(outfitDialog);
	CopyBits(&loadPort.port->portBits, &outfitDialog->portBits, &outfitDialog->portRect, &outfitDialog->portRect, srcCopy, outfitDialog->visRgn);
}

// FUNCTION: EVO 0x1003A0AC
Boolean CanBuyOutfitItem(void)
{
	Boolean	ok;
	short	freeMass;
	long	price;

	freeMass = ShipFreeMass();
	if (selOutfitItem != -1) {
		if (!HasMaxOfItem(selOutfitItem) &&
			(freeMass >= gOutfits[selOutfitItem].mass || gOutfits[selOutfitItem].mass <= 0)) {
			if (gOutfits[selOutfitItem].modType[0] == kOutfModAmmo) {
				if (gOutfits[selOutfitItem].modVal[0] >= 0) {
					if (gWeapons[gOutfits[selOutfitItem].modVal[0]].guidance == kGuidanceBay) {
						if (CanBuyFighter(gWeapons[gOutfits[selOutfitItem].modVal[0]].ammoType - kAmmoShipBase,
								selOutfitItem, gOutfits[selOutfitItem].modVal[0]))
							ok = true;
						else
							ok = false;
					} else {
						ok = true;
					}
				} else {
					ok = true;
				}
			} else if (gOutfits[selOutfitItem].modType[0] == kOutfModCargo) {
				if (gOutfits[selOutfitItem].modVal[0] < 0) {
					if (!gShipTypes[shipData->type].negativeHoldsFlag) {
						ok = false;
					} else if (ShipTotalHolds() - TotalCargo(shipData) > abs(gOutfits[selOutfitItem].modVal[0])) {
						ok = true;
					} else {
						ok = false;
					}
				} else {
					ok = true;
				}
			} else if (gOutfits[selOutfitItem].modType[0] == kOutfModMap) {
				if (!hasMapOutfit)
					ok = true;
				else
					ok = false;
			} else if (gOutfits[selOutfitItem].modType[0] == kOutfModCleanRecord) {
				if (!hasCleanRecordOutfit)
					ok = true;
				else
					ok = false;
			} else {
				ok = true;
			}
		} else {
			ok = false;
		}
		if (ok) {
			price = ApplyPriceAndTechnologyFlux(gOutfits[selOutfitItem].cost, priceMult[0],
				gOutfits[selOutfitItem].techLevel, stellarData[shipData->targetStellar].techLevel);
			if (shipData->credits < price)
				ok = false;
		}
	} else {
		ok = false;
	}
	return ok;
}

// FUNCTION: EVO 0x1003A3E4
void PreloadOutfitIconStrip(void)
{
	PicHandle	pic;
	Rect		r;
	short		i;
	short		w;
	short		h;

	SATSetPortBackScreen();
	pic = GetPicture(kOutfitIconBase);
	if (pic != NULL) {
		w = (*pic)->picFrame.right - (*pic)->picFrame.left;
		h = (*pic)->picFrame.bottom - (*pic)->picFrame.top;
		SetRect(&r, 0, 0, w, h);
		DrawPicture(pic, &r);
		HPurge((Handle)pic);
		ReleaseResource((Handle)pic);
	}
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		pic = GetPicture(i + (kOutfitIconBase + 1));
		if (pic != NULL) {
			DrawPicture(pic, &shopIconRect[i]);
			HPurge((Handle)pic);
			ReleaseResource((Handle)pic);
		}
	}
	SATSetPortScreen();
}

// FUNCTION: EVO 0x1003A500
void DoShipyardDialog(void)
{
	long			pad1;
	Rect			titleBox;
	Rect			box;
	Rect			pad4;
	short			itemHit;
	short			itemType;
	short			inset;
	short			savedTarget;
	Handle			item;
	long			pad8;
	char			prompt[256];
	Str255			tmp;
	Str255			num;
	long			pad5;
	long			pad6;
	long			pad7;
	Boolean			ok;
	Boolean			done;
	ModalFilterUPP	filterUPP;
	long			price;
	Boolean			found;
	short			i;
	short			j;
	short			n;

	done = false;
	filterUPP = NewModalFilterProc(ShipyardFilter);
	SATSetPortScreen();
	selShipyardItem = -1;
	shipyardHover = -1;
	outfitLastCell = -1;
	shipyardScroll = 0;
	savedTarget = shipData->targetStellar;
	shipyardDialog = nil;
	shipyardDialog = GetNewDialog(kShipyardDlogID, nil, (WindowPtr)-1);
	if (shipyardDialog != nil) {
		NewDialogHook(shipyardDialog, 0);
		CenterWindow(shipyardDialog);
		GetDialogItem(shipyardDialog, kShipyardGridItem, &itemType, &item, &box);
		SetupListRects(&box);
		SetupPortAvailableShipTypes(currentSpob);
		PreloadShipyardIconStrip();
		for (i = 0; i <= 3; i++) {
			if (shipBuyMode == 0)
				shipyardPics[i] = GetPicture(i + kBuyShipButtonPict);
			else
				shipyardPics[i] = GetPicture(i + kSelectButtonPict);
		}
		shipyardPics[4] = GetPicture(kDone121ButtonPict);
		shipyardPics[5] = GetPicture(kDone121ButtonPict + 1);
		shipyardPics[6] = GetPicture(kInfoButtonPict);
		shipyardPics[7] = GetPicture(kInfoButtonPict + 1);
		for (i = 0; i <= 3; i++)
			shipyardPics[i + 8] = GetPicture(i + kUpArrowButtonPict);
		strncpy(gOutfitDesc, "", 3);
		ShowWindow(shipyardDialog);
		SelectWindow(shipyardDialog);
		SetPort(shipyardDialog);
		FlushEvents(everyEvent - diskMask, 0);
		ForeColor(blackColor);
		InvalRect(&shipyardDialog->portRect);
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kShipyardBuyItem) {
				if (selShipyardItem != -1) {
					n = selShipyardItem;
					if (canBuy) {
						if (shipBuyMode == 0) {
							InvalRect(&shipyardDialog->portRect);
							strncpy(prompt, "Please name your new ", 25);
							strncpy((char *)tmp, (char *)longShipName[n], 200);
							p2cstr(tmp);
							strcat(prompt, (char *)tmp);
							strcat(prompt, ": ");
							c2pstr(prompt);
							strncpy((char *)tmp, (char *)shipyardName[n], 19);
							p2cstr(tmp);
							strcat((char *)tmp, " ");
							for (i = 0; i <= 2; i++) {
								NumToString(Rand(9) + 1, num);
								p2cstr(num);
								strcat((char *)tmp, (char *)num);
							}
							c2pstr((char *)tmp);
							ok = EVTextInputDialog((StringPtr)prompt, tmp, 20);
							RefreshSAT();
							SetPort(shipyardDialog);
							InvalRect(&shipyardDialog->portRect);
							if (ok) {
								daysPassed4 = true;
								price = ApplyPriceAndTechnologyFlux(PlayerShipTradeInPrice(), priceMult[0],
									gShipTypes[shipData->type].techLevel,
									stellarData[shipData->targetStellar].techLevel);
								price = ApplyPriceAndTechnologyFlux(price, priceMult[1],
									gShipTypes[shipData->type].techLevel,
									stellarData[shipData->targetStellar].techLevel);
								shipData->credits += price;
								shipData->type = n;
								price = ApplyPriceAndTechnologyFlux(gShipTypes[n].cost, priceMult[1],
									gShipTypes[n].techLevel,
									stellarData[shipData->targetStellar].techLevel);
								shipData->credits -= price;
								strncpy((char *)shipName, (char *)gTextInputResult, 32);
								CullNameString(shipName);
								for (i = 1; i < MaxShipsInSystem; i++) {
									if (shipData[i].active) {
										if (shipData[i].parentShip == 0) {
											if (shipData[i].aiType == kAIFighter) {
												shipData[i].aiType = kAIEscort;
												shipData[i].hired = true;
											}
										}
									}
								}
								for (i = 0; i < MaxWeaponTypes; i++) {
									found = false;
									for (j = 0; j < MaxOutfitItemTypes; j++) {
										if (gOutfits[j].modType[0] == kOutfModWeapon && i == gOutfits[j].modVal[0]) {
											if (gOutfits[j].persistent) {
												found = true;
												break;
											}
										}
									}
									if (!found) {
										shipData->weap[i].count = 0;
										shipData->weap[i].ammo = 0;
									}
								}
								for (i = 0; i < MaxOutfitItemTypes; i++) {
									if (!gOutfits[i].persistent)
										outfitCount[i] = 0;
								}
								for (j = 0; j < 4; j++) {
									if (gShipTypes[shipData->type].defOutfCount[j] > 0)
										outfitCount[gShipTypes[shipData->type].defOutfID[j]] += gShipTypes[shipData->type].defOutfCount[j];
								}
								SystemInfoToShipStats();
								shipData->shield = gShipTypes[shipData->type].shield;
								shipData->fuel = gShipTypes[shipData->type].fuel;
								for (i = 0; i < MaxWeaponTypes; i++) {
									shipData->weap[i].count += gShipTypes[shipData->type].weapCount[i];
									shipData->weap[i].ammo += gShipTypes[shipData->type].weapAmmo[i];
								}
								DestroyPartialFleetCargo(0);
								shipData->targetShip = -1;
								shipData->selectedWeapon = -1;
								hudRedraw.ammo = true;
								hudRedraw.nav = true;
								hudRedraw.target = true;
								hudRedraw.credits = true;
								shipData->pos.x = currentSpob->pos.h;
								shipData->pos.y = currentSpob->pos.v;
								hudRedraw.credits = true;
								DrawStatus();
								SetPort(shipyardDialog);
								done = true;
							}
							SetPort(shipyardDialog);
							InvalRect(&shipyardDialog->portRect);
						} else if (shipBuyMode != 0) {
							price = ApplyPriceAndTechnologyFlux(gShipTypes[selShipyardItem].cost, priceMult[1],
								gShipTypes[selShipyardItem].techLevel,
								stellarData[shipData->targetStellar].techLevel);
							shipData->credits -= price * (0.1);
							hudRedraw.credits = true;
							SpawnEscort(selShipyardItem, shipData->targetStellar);
							done = true;
						}
					} else {
						PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
					}
				} else {
					PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
				}
			}
			if (itemHit == kShipyardMapItem) {
				savedTarget = shipData->targetStellar;
				DoSystemMap();
				shipData->targetStellar = savedTarget;
				PreloadShipyardIconStrip();
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
				PortDialogUpdate();
				SetPort(shipyardDialog);
				InvalRect(&shipyardDialog->portRect);
			}
			if (itemHit == kShipyardPilotItem) {
				DoPlayerInfoDialog();
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
				PortDialogUpdate();
				SetPort(shipyardDialog);
				InvalRect(&shipyardDialog->portRect);
			}
			if (itemHit == kShipyardMissionLogItem) {
				n = 0;
				for (i = 0; i < MaxSimultaneousMissions; i++) {
					if (currentMissionObjectives[i].active)
						n++;
				}
				if (n > 0) {
					DoMissionInfoDialog();
					SetPort(portDialog);
					InvalRect(&portDialog->portRect);
					PortDialogUpdate();
					SetPort(shipyardDialog);
					InvalRect(&shipyardDialog->portRect);
				}
			}
			if (itemHit == kShipyardSpecsItem && selShipyardItem != -1) {
				ShipyardDialogUpdate();
				DoShipyardDescription(selShipyardItem);
				SetPort(shipyardDialog);
				InvalRect(&shipyardDialog->portRect);
			}
			if (itemHit == kShipyardScrollUpItem && shipyardScroll >= kShopColumns) {
				shipyardScroll -= kShopColumns;
				selShipyardItem = shipyardHover = -1;
				strncpy(gOutfitDesc, "", 3);
				InvalRect(&shipyardDialog->portRect);
			}
			if (itemHit == kShipyardScrollDownItem) {
				n = 0;
				for (i = 0; i < MaxShipClasses; i++) {
					if (dockedPortShipTypes[i] != -1)
						n++;
				}
				if (shipyardScroll < n - kShopCells) {
					shipyardScroll += kShopColumns;
					selShipyardItem = shipyardHover = -1;
					strncpy(gOutfitDesc, "", 3);
					InvalRect(&shipyardDialog->portRect);
				}
			}
			if (itemHit == kShipyardLeaveItem)
				done = true;
		} while (!done);
		for (i = 0; i <= 11; i++) {
			if (shipyardPics[i] != nil) {
				HPurge((Handle)shipyardPics[i]);
				ReleaseResource((Handle)shipyardPics[i]);
			}
		}
		if (shipBuyMode != 0) {
			n = (shipyardDialog->portRect.right - shipyardDialog->portRect.left) / -2;
			inset = (shipyardDialog->portRect.bottom - shipyardDialog->portRect.top) / -2;
		}
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(shipyardDialog);
		gCheatSoundPlayed = false;
		if (shipBuyMode != 0) {
			box.left = box.right = (gSAT.wind.bounds.left + gSAT.wind.bounds.right) / 2;
			box.top = box.bottom = (gSAT.wind.bounds.top + gSAT.wind.bounds.bottom) / 2;
			InsetRect(&box, n - 1, inset - 1);
			SATSetPortScreen();
			ForeColor(blackColor);
			PaintRect(&box);
			RedrawStatus();
			RedrawComm(0);
		}
		SATSetPortBackScreen();
		ForeColor(blackColor);
		PaintRect(&gSAT.backScreen.bounds);
		RedrawStatus();
		SATSetPortScreen();
		RefreshSAT();
		shipData->targetStellar = savedTarget;
		hudRedraw.nav = true;
	}
}

// FUNCTION: EVO 0x1003B444
pascal Boolean ShipyardFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	Rect	box;
	Rect	box2;
	char	key;
	long	diff;
	short	row;
	short	col;
	short	prevCell;
	short	itemType;
	long	price;
	long	trade;
	Handle	item;
	Point	where;
	short	pad0;
	short	pad1;
	short	pad2;

	if (shipBuyMode == 0) {
		if (selShipyardItem != -1) {
			price = ApplyPriceAndTechnologyFlux(gShipTypes[selShipyardItem].cost, priceMult[1],
				gShipTypes[selShipyardItem].techLevel, stellarData[shipData->targetStellar].techLevel);
			trade = ApplyPriceAndTechnologyFlux(PlayerShipTradeInPrice(), priceMult[0],
				gShipTypes[shipData->type].techLevel, stellarData[shipData->targetStellar].techLevel);
			trade = ApplyPriceAndTechnologyFlux(trade, priceMult[1],
				gShipTypes[shipData->type].techLevel, stellarData[shipData->targetStellar].techLevel);
			diff = price - trade;
			if (diff < 0)
				diff = 0;
			if (shipData->credits >= diff)
				canBuy = true;
			else
				canBuy = false;
		} else {
			canBuy = false;
		}
	} else if (selShipyardItem != -1) {
		price = ApplyPriceAndTechnologyFlux(gShipTypes[selShipyardItem].cost, priceMult[1],
			gShipTypes[selShipyardItem].techLevel, stellarData[shipData->targetStellar].techLevel);
		diff = price * (0.1);
		if (shipData->credits >= diff)
			canBuy = true;
		else
			canBuy = false;
	} else {
		canBuy = false;
	}
	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		if (key == kReturnCharCode || key == kEnterCharCode) {
			ev->what = mouseDown;
			*itemHit = kShipyardLeaveItem;
			return true;
		}
	}
	QuickKeyCheckHandler();
	if (QuickKeyCheck(keyMap[kCtlMap])) {
		*itemHit = kShipyardMapItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlPlayerInfo])) {
		*itemHit = kShipyardPilotItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlMissions])) {
		*itemHit = kShipyardMissionLogItem;
		return true;
	}
	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		key = tolower(key);
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		GetDialogItem(shipyardDialog, kShipyardGridItem, &itemType, &item, &box);
		if (PtInRect(where, &box)) {
			col = (where.h - box.left) / 84;
			row = (where.v - box.top) / 55;
			if (col >= 0 && col <= 3 && row >= 0 && row <= 4) {
				prevCell = shipyardHover;
				shipyardHover = col + row * kShopColumns;
				if (dockedPortShipTypes[shipyardHover + shipyardScroll] == -1) {
					selShipyardItem = shipyardHover = -1;
				} else {
					selShipyardItem = dockedPortShipTypes[shipyardHover + shipyardScroll];
				}
				if (shipyardHover != prevCell) {
					GetDialogItem(shipyardDialog, kShipyardBuyItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(shipyardDialog, kShipyardGridItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(shipyardDialog, kShipyardDescItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(shipyardDialog, kShipyardPictItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(shipyardDialog, kShipyardPriceItem, &itemType, &item, &box2);
					InvalRect(&box2);
					GetDialogItem(shipyardDialog, kShipyardSpecsItem, &itemType, &item, &box2);
					InvalRect(&box2);
					if (shipyardHover != -1) {
						if (outfitPic != nil) {
							HPurge((Handle)outfitPic);
							ReleaseResource((Handle)outfitPic);
						}
						if (gCheatSoundPlayed) {
							LoadDescription(gOutfitDesc, kNoShipDescID);
							outfitPic = GetPicture(kNoShipPictID);
						} else {
							if (shipBuyMode != 0)
								LoadDescription(gOutfitDesc, selShipyardItem + kShipAltDescBase);
							else
								LoadDescription(gOutfitDesc, selShipyardItem + kShipDescBase);
							outfitPic = GetPicture(selShipyardItem + kShipPictBase);
						}
					} else {
						strncpy(gOutfitDesc, "", 3);
					}
				}
				if (shipyardHover == outfitLastCell && shipyardHover != -1 &&
						ev->when - outfitClickWhen <= 15) {
					ShipyardDialogUpdate();
					DoShipyardDescription(selShipyardItem);
					SetPort(shipyardDialog);
					InvalRect(&shipyardDialog->portRect);
				}
				outfitLastCell = shipyardHover;
				outfitClickWhen = ev->when;
			}
		}
		col = TrackShipyardButtons(where);
		if (col == 0) {
			*itemHit = kShipyardLeaveItem;
			return true;
		}
		if (col == 1) {
			*itemHit = kShipyardBuyItem;
			return true;
		}
		if (col == 2) {
			*itemHit = kShipyardSpecsItem;
			return true;
		}
		if (col == 3) {
			*itemHit = kShipyardScrollUpItem;
			return true;
		}
		if (col == 4) {
			*itemHit = kShipyardScrollDownItem;
			return true;
		}
		*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(shipyardDialog);
		ShipyardDialogUpdate();
		EndUpdate(shipyardDialog);
		return false;
	}
	return false;
}

// FUNCTION: EVO 0x1003BBD0
void ShipyardDialogUpdate(void)
{
	Rect	box;
	Rect	pad0;
	Rect	pad1;
	short	itemType;
	Rect	pad2;
	long	diff;
	long	price;
	long	trade;
	Handle	item;
	long	pad3;
	Str255	buf;
	short	i;
	short	n;

	SATSetPort(&loadPort);
	ForeColor(blackColor);
	PaintRect(&shipyardDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&shipyardDialog->portRect);
	ForeColor(blackColor);
	GetDialogItem(shipyardDialog, kShipyardGridItem, &itemType, &item, &box);
	box.right++;
	box.bottom++;
	if (RectInRgn(&box, shipyardDialog->visRgn)) {
		TextFont(geneva);
		TextSize(kDialogTextSize);
		ForeColor(blackColor);
		n = 0;
		for (i = shipyardScroll; i < MaxShipClasses; i++) {
			if (RectInRgn(&shopCellRect[i - shipyardScroll], shipyardDialog->visRgn) && n < kShopCells &&
					dockedPortShipTypes[i] != -1) {
				CopyBits(&gSAT.backScreen.port->portBits, &loadPort.port->portBits,
					&shopIconRect[dockedPortShipTypes[i]], &shopCellIcon[i - shipyardScroll], srcCopy, nil);
				MoveTo((shopCellRect[i - shipyardScroll].left + shopCellRect[i - shipyardScroll].right) / 2 -
					StringWidth(shipyardName[dockedPortShipTypes[i]]) / 2,
					shopCellRect[i - shipyardScroll].bottom - 6);
				ForeColor(whiteColor);
				DrawString(shipyardName[dockedPortShipTypes[i]]);
				ForeColor(blackColor);
				RGBForeColor(&rgbRealDarkBlue);
				FrameRect(&shopCellRect[i - shipyardScroll]);
				ForeColor(blackColor);
				n++;
			}
		}
		if (shipyardHover != -1) {
			RGBForeColor(&brightGreen);
			FrameRect(&shopCellRect[shipyardHover]);
			ForeColor(blackColor);
		}
	}
	if (selShipyardItem != -1) {
		GetDialogItem(shipyardDialog, kShipyardPictItem, &itemType, &item, &box);
		if (RectInRgn(&box, shipyardDialog->visRgn)) {
			if (outfitPic != nil) {
				DrawPicture(outfitPic, &box);
			} else {
				ForeColor(blackColor);
				PaintRect(&box);
				ForeColor(whiteColor);
				FrameRect(&box);
				TextFont(geneva);
				TextSize(kDialogTextSize);
				DrawCenteredText("\pNo Picture", box.left, box.right, box.top + 44);
				DrawCenteredText("\pAvailable", box.left, box.right, box.top + 56);
				ForeColor(blackColor);
			}
		}
		GetDialogItem(shipyardDialog, kShipyardPriceItem, &itemType, &item, &box);
		if (RectInRgn(&box, shipyardDialog->visRgn)) {
			TextFont(geneva);
			TextSize(kDialogTextSize);
			if (shipBuyMode == 0 && selShipyardItem != shipData->type) {
				ForeColor(whiteColor);
				MoveTo(box.left, box.top + 12);
				DrawString("\pShip Price:");
				MoveTo(box.left + 70, box.top + 12);
				price = ApplyPriceAndTechnologyFlux(gShipTypes[selShipyardItem].cost, priceMult[1],
					gShipTypes[selShipyardItem].techLevel, stellarData[shipData->targetStellar].techLevel);
				DrawCommaNumber(price);
				DrawString("\p cr");
				MoveTo(box.left, box.top + 24);
				DrawString("\pTrade-In:");
				MoveTo(box.left + 70, box.top + 24);
				trade = ApplyPriceAndTechnologyFlux(PlayerShipTradeInPrice(), priceMult[0],
					gShipTypes[shipData->type].techLevel, stellarData[shipData->targetStellar].techLevel);
				trade = ApplyPriceAndTechnologyFlux(trade, priceMult[1],
					gShipTypes[shipData->type].techLevel, stellarData[shipData->targetStellar].techLevel);
				DrawCommaNumber(trade);
				DrawString("\p cr");
				diff = price - trade;
				if (diff < 0)
					diff = 0;
				MoveTo(box.left, box.top + 48);
				DrawString("\pFinal Price:");
				MoveTo(box.left + 70, box.top + 48);
				DrawCommaNumber(diff);
				DrawString("\p cr");
				MoveTo(box.left, box.top + 72);
				DrawString("\pYou Have:");
				MoveTo(box.left + 70, box.top + 72);
				DrawCommaNumber(shipData->credits);
				DrawString("\p cr");
				ForeColor(blackColor);
			} else if (shipBuyMode != 0) {
				ForeColor(whiteColor);
				MoveTo(box.left, box.top + 12);
				DrawString("\pHiring Price:");
				MoveTo(box.left + 70, box.top + 12);
				price = ApplyPriceAndTechnologyFlux(gShipTypes[selShipyardItem].cost, priceMult[1],
					gShipTypes[selShipyardItem].techLevel, stellarData[shipData->targetStellar].techLevel);
				DrawCommaNumber(price * (0.1));
				DrawString("\p cr");
				MoveTo(box.left, box.top + 36);
				DrawString("\pYou Have:");
				MoveTo(box.left + 70, box.top + 36);
				DrawCommaNumber(shipData->credits);
				DrawString("\p cr");
				ForeColor(blackColor);
			}
		}
	}
	GetDialogItem(shipyardDialog, kShipyardDescItem, &itemType, &item, &box);
	if (RectInRgn(&box, shipyardDialog->visRgn)) {
		TextFont(geneva);
		TextSize(kDialogTextSize);
		TETextBox(gOutfitDesc, strlen(gOutfitDesc), &box, teFlushDefault);
		InvertRect(&box);
	}
	DrawShipyardButtons(-1);
	SATSetPortScreen();
	SetPort(shipyardDialog);
	CopyBits(&loadPort.port->portBits, &shipyardDialog->portBits, &shipyardDialog->portRect, &shipyardDialog->portRect, srcCopy, nil);
}

// FUNCTION: EVO 0x1003C4D4
void PreloadShipyardIconStrip(void)
{
	PicHandle	pic;
	Rect		r;
	short		i;
	short		w;
	short		h;

	SATSetPortBackScreen();
	pic = GetPicture(kShipIconBase);
	if (pic != NULL) {
		w = (*pic)->picFrame.right - (*pic)->picFrame.left;
		h = (*pic)->picFrame.bottom - (*pic)->picFrame.top;
		SetRect(&r, 0, 0, w, h);
		DrawPicture(pic, &r);
		HPurge((Handle)pic);
		ReleaseResource((Handle)pic);
	}
	for (i = 0; i < MaxShipClasses; i++) {
		pic = NULL;
		pic = GetPicture(i + (kShipIconBase + 1));
		if (pic != NULL) {
			DrawPicture(pic, &shopIconRect[i]);
			HPurge((Handle)pic);
			ReleaseResource((Handle)pic);
		}
	}
	SATSetPortScreen();
}

// FUNCTION: EVO 0x1003C5F0
void DoShipyardDescription(void)
{
	Rect			box;
	short			itemType;
	Handle			item;
	short			itemHit;
	short			pa;
	short			pb;
	short			pc;
	Str255			buf;
	Boolean			done;
	ModalFilterUPP	filterUPP;
	short			savedTarget;
	short			count;
	short			i;

	done = false;
	filterUPP = NewModalFilterProc(ShipDescFilter);
	specsDialog = nil;
	specsDialog = GetNewDialog(kSpecsDlogID, nil, (WindowPtr)-1);
	if (specsDialog != nil) {
		NewDialogHook(specsDialog, 0);
		CenterWindow(specsDialog);
		ShowWindow(specsDialog);
		SelectWindow(specsDialog);
		SetPort(specsDialog);
		FlushEvents(everyEvent - diskMask, 0);
		ForeColor(blackColor);
		InvalRect(&specsDialog->portRect);
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kOkItem)
				done = true;
			if (itemHit == kSpecsMapItem) {
				savedTarget = shipData->targetStellar;
				DoSystemMap();
				shipData->targetStellar = savedTarget;
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
				PortDialogUpdate();
				SetPort(specsDialog);
				InvalRect(&specsDialog->portRect);
			}
			if (itemHit == kSpecsPilotItem) {
				DoPlayerInfoDialog();
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
				PortDialogUpdate();
				SetPort(specsDialog);
				InvalRect(&specsDialog->portRect);
			}
			if (itemHit == kSpecsMissionLogItem) {
				count = 0;
				for (i = 0; i < MaxSimultaneousMissions; i++) {
					if (currentMissionObjectives[i].active)
						count++;
				}
				if (count > 0) {
					DoMissionInfoDialog();
					SetPort(portDialog);
					InvalRect(&portDialog->portRect);
					PortDialogUpdate();
					SetPort(specsDialog);
					InvalRect(&specsDialog->portRect);
				}
			}
		} while (!done);
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(specsDialog);
		RefreshSAT();
	}
}

// FUNCTION: EVO 0x1003C864
pascal Boolean ShipDescFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	Rect	box;
	short	itemType;
	Handle	item;
	Point	where;
	char	key;
	short	state;
	short	cur;

	QuickKeyCheckHandler();
	if (QuickKeyCheck(keyMap[kCtlMap])) {
		*itemHit = kSpecsMapItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlPlayerInfo])) {
		*itemHit = kSpecsPilotItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlMissions])) {
		*itemHit = kSpecsMissionLogItem;
		return true;
	}
	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		key = tolower(key);
		if (key == kReturnCharCode || key == kEnterCharCode) {
			ev->what = mouseDown;
			*itemHit = kOkItem;
			return true;
		}
	}
	if (ev->what == updateEvt) {
		BeginUpdate(specsDialog);
		DrawDialog(specsDialog);
		ShipDescDialogUpdate();
		EndUpdate(specsDialog);
		return false;
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		GetDialogItem(specsDialog, kOkItem, &itemType, &item, &box);
		if (PtInRect(where, &box)) {
			DrawPicture(shipyardPics[5], &box);
			state = 1;
			while (StillDown()) {
				GetMouse(&where);
				if (PtInRect(where, &box))
					cur = 1;
				else
					cur = -1;
				if (cur != state) {
					if (cur == 1)
						DrawPicture(shipyardPics[5], &box);
					else
						DrawPicture(shipyardPics[4], &box);
				}
				state = cur;
			}
			if (state == 1) {
				*itemHit = kOkItem;
				return true;
			}
			*itemHit = -1;
			return true;
		}
	}
	return false;
}

// FUNCTION: EVO 0x1003CAD0
void ShipDescDialogUpdate(void)
{
	long	pad;
	Rect	box;
	Rect	pad0;
	short	itemType;
	Str255	buf;
	Handle	item;
	short	type;
	short	i;
	short	j;
	short	freeMass;
	short	count;
	int	y;

	SATSetPort(&loadPort);
	ForeColor(blackColor);
	PaintRect(&specsDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&specsDialog->portRect);
	ForeColor(blackColor);
	GetDialogItem(specsDialog, kSpecsHeaderItem, &itemType, &item, &box);
	if (RectInRgn(&box, specsDialog->visRgn)) {
		TextFont(times);
		TextSize(18);
		MoveTo(box.left, box.top + 18);
		ForeColor(whiteColor);
		DrawCenteredText(gShipTypes[selShipyardItem].name, box.left, box.right, box.top + 18);
		ForeColor(blackColor);
	}
	GetDialogItem(specsDialog, kSpecsTextItem, &itemType, &item, &box);
	if (RectInRgn(&box, specsDialog->visRgn)) {
		type = selShipyardItem;
		ForeColor(blackColor);
		PaintRect(&box);
		ForeColor(whiteColor);
		TextFont(geneva);
		TextSize(kDialogTextSize);
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left, box.top + 12);
		DrawString("\pSpeed:");
		ForeColor(whiteColor);
		MoveTo(box.left + 45, box.top + 12);
		NumToString(100.0 * gShipTypes[type].maxSpeed, buf);
		DrawString(buf);
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left, box.top + 24);
		DrawString("\pAccel:");
		ForeColor(whiteColor);
		MoveTo(box.left + 45, box.top + 24);
		if (gShipTypes[type].accel > 0.073)
			DrawString("\pExcellent");
		else if (gShipTypes[type].accel > 0.055)
			DrawString("\pVery Good");
		else if (gShipTypes[type].accel > 0.038)
			DrawString("\pGood");
		else if (gShipTypes[type].accel > 0.025)
			DrawString("\pAverage");
		else if (gShipTypes[type].accel > 0.013)
			DrawString("\pPoor");
		else if (gShipTypes[type].accel > 0.0)
			DrawString("\pTerrible");
		else
			DrawString("\pN/A");
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left, box.top + 36);
		DrawString("\pTurn:");
		ForeColor(whiteColor);
		MoveTo(box.left + 45, box.top + 36);
		if (gShipTypes[type].turnRate <= 0)
			DrawString("\pN/A");
		else if (gShipTypes[type].turnRate == 1)
			DrawString("\pTerrible");
		else if (gShipTypes[type].turnRate == 2)
			DrawString("\pPoor");
		else if (gShipTypes[type].turnRate == 3)
			DrawString("\pAverage");
		else if (gShipTypes[type].turnRate == 4)
			DrawString("\pGood");
		else if (gShipTypes[type].turnRate == 5)
			DrawString("\pVery Good");
		else
			DrawString("\pExcellent");
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left, box.top + 48);
		DrawString("\pShields:");
		ForeColor(whiteColor);
		MoveTo(box.left + 45, box.top + 48);
		if (gShipTypes[type].shield > 0)
			DrawCommaNumber(0.1 * gShipTypes[type].shield);
		else
			DrawString("\pNone");
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left, box.top + 60);
		DrawString("\pArmor:");
		ForeColor(whiteColor);
		MoveTo(box.left + 45, box.top + 60);
		if (gShipTypes[type].armor > 0)
			DrawCommaNumber(0.1 * gShipTypes[type].armor);
		else
			DrawString("\pNone");
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left, box.top + 72);
		DrawString("\pGuns:");
		ForeColor(whiteColor);
		MoveTo(box.left + 45, box.top + 72);
		if (gShipTypes[type].maxGuns > 0) {
			NumToString(gShipTypes[type].maxGuns, buf);
			DrawString("\pMaximum of ");
			DrawString(buf);
		} else {
			DrawString("\pNone");
		}
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left, box.top + 84);
		DrawString("\pTurrets:");
		ForeColor(whiteColor);
		MoveTo(box.left + 45, box.top + 84);
		if (gShipTypes[type].maxTurrets > 0) {
			NumToString(gShipTypes[type].maxTurrets, buf);
			DrawString("\pMaximum of ");
			DrawString(buf);
		} else {
			DrawString("\pNone");
		}
		freeMass = gShipTypes[type].freeMass;
		for (i = 0; i < MaxWeaponTypes; i++) {
			if (gShipTypes[type].weapCount[i] > 0) {
				for (j = 0; j < MaxOutfitItemTypes; j++) {
					if (gOutfits[j].modType[0] == kOutfModWeapon && i == gOutfits[j].modVal[0]) {
						freeMass -= gOutfits[j].mass * gShipTypes[type].weapCount[i];
						break;
					}
				}
			}
			if (gShipTypes[type].weapAmmo[i] > 0) {
				for (j = 0; j < MaxOutfitItemTypes; j++) {
					if (gOutfits[j].modType[0] == kOutfModAmmo && i == gOutfits[j].modVal[0]) {
						freeMass -= gOutfits[j].mass * gShipTypes[type].weapAmmo[i];
						break;
					}
				}
			}
		}
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left, box.top + 96);
		DrawString("\pSpace:");
		ForeColor(whiteColor);
		MoveTo(box.left + 45, box.top + 96);
		NumToString(freeMass, buf);
		DrawString(buf);
		DrawString("\p ton");
		if (freeMass != 1)
			DrawString("\ps");
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left, box.top + 108);
		DrawString("\pCargo:");
		ForeColor(whiteColor);
		MoveTo(box.left + 45, box.top + 108);
		NumToString(gShipTypes[type].cargo, buf);
		DrawString(buf);
		DrawString("\p ton");
		if (gShipTypes[type].cargo != 1)
			DrawString("\ps");
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left, box.top + 120);
		DrawString("\pFuel:");
		ForeColor(whiteColor);
		MoveTo(box.left + 45, box.top + 120);
		NumToString(gShipTypes[type].fuel / 100, buf);
		DrawString(buf);
		DrawString("\p jump");
		if (gShipTypes[type].fuel > 100)
			DrawString("\ps");
		count = 0;
		for (i = 0; i < MaxWeaponTypes; i++) {
			if (gShipTypes[type].weapCount[i] > 0)
				count++;
		}
		if (count == 0) {
			ForeColor(whiteColor);
			MoveTo(box.left, box.top + 144);
			DrawString("\pNo standard weapons");
		} else {
			ForeColor(whiteColor);
			MoveTo(box.left, box.top + 144);
			DrawString("\pStandard weapons:");
			y = box.top + 156;
			for (i = 0; i < MaxWeaponTypes; i++) {
				if (gShipTypes[type].weapCount[i] > 0) {
					MoveTo(box.left + 15, y);
					if (gWeapons[i].guidance != kGuidanceBay) {
						NumToString(gShipTypes[type].weapCount[i], buf);
						DrawString(buf);
						DrawString("\p ");
						for (j = 0; j < MaxOutfitItemTypes; j++) {
							if (gOutfits[j].modType[0] == kOutfModWeapon) {
								if (i == gOutfits[j].modVal[0]) {
									DrawString(gOutfits[j].name);
									break;
								}
							}
						}
						if (gShipTypes[type].weapCount[i] > 1)
							DrawString("\ps");
						if (gShipTypes[type].weapAmmo[i] > 0) {
							DrawString("\p + ");
							NumToString(gShipTypes[type].weapAmmo[i], buf);
							DrawString(buf);
							DrawString("\p ");
							for (j = 0; j < MaxOutfitItemTypes; j++) {
								if (gOutfits[j].modType[0] == kOutfModAmmo && i == gOutfits[j].modVal[0]) {
									DrawString(gOutfits[j].name);
									break;
								}
							}
							if (gShipTypes[type].weapAmmo[i] > 1)
								DrawString("\ps");
						}
					} else {
						NumToString(gShipTypes[type].weapAmmo[i], buf);
						DrawString(buf);
						DrawString("\p ");
						for (j = 0; j < MaxOutfitItemTypes; j++) {
							if (gOutfits[j].modType[0] == kOutfModAmmo && i == gOutfits[j].modVal[0]) {
								DrawString(gOutfits[j].name);
								break;
							}
						}
						if (gShipTypes[type].weapAmmo[i] > 1)
							DrawString("\ps");
					}
					y = y + 12;
				}
			}
		}
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left + 100, box.top + 96);
		DrawString("\pLength:");
		ForeColor(whiteColor);
		MoveTo(box.left + 138, box.top + 96);
		NumToString(gShipTypes[type].shipLength, buf);
		DrawString(buf);
		DrawString("\p m");
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left + 100, box.top + 108);
		DrawString("\pMass:");
		ForeColor(whiteColor);
		MoveTo(box.left + 138, box.top + 108);
		NumToString(gShipTypes[type].mass, buf);
		DrawString(buf);
		DrawString("\p ton");
		if (gShipTypes[type].mass != 1)
			DrawString("\ps");
		RGBForeColor(&rgbMedGrey);
		MoveTo(box.left + 100, box.top + 120);
		DrawString("\pCrew:");
		ForeColor(whiteColor);
		MoveTo(box.left + 138, box.top + 120);
		NumToString(gShipTypes[type].crew, buf);
		DrawString(buf);
	}
	GetDialogItem(specsDialog, kOkItem, &itemType, &item, &box);
	if (RectInRgn(&box, specsDialog->visRgn))
		DrawPicture(shipyardPics[4], &box);
	SATSetPortScreen();
	SetPort(specsDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &specsDialog->portBits, &specsDialog->portRect, &specsDialog->portRect, srcCopy, nil);
}

// FUNCTION: EVO 0x1003DD08
void ShowWaitDialog(void)
{
	waitDialog = GetNewDialog(kWaitDlogID, nil, (WindowPtr)-1);
	NewDialogHook(waitDialog, 0);
	ShowWindow(waitDialog);
	SelectWindow(waitDialog);
	SetPort(waitDialog);
	DrawDialog(waitDialog);
}

// FUNCTION: EVO 0x1003DD88
void CloseWaitDialog(void)
{
	DisposeDialog(waitDialog);
}

// FUNCTION: EVO 0x1003DDB4
void SetupListRects(Rect *box)
{
	short	i;
	short	x;
	short	y;

	for (i = 0; i < kShopCells; i++) {
		y = i / kShopColumns;
		x = i % kShopColumns;
		SetRect(&shopCellRect[i], box->left + x * 83, box->top + y * 54,
			box->left + (x + 1) * 83 + 1, box->top + (y + 1) * 54 + 1);
		x = (shopCellRect[i].right + shopCellRect[i].left) / 2;
		y = (shopCellRect[i].top + shopCellRect[i].bottom) / 2;
		SetRect(&shopCellIcon[i], x - 16, y - 24, x + 16, y + 8);
	}
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		y = i / 8;
		x = i % 8;
		SetRect(&shopIconRect[i], x * 32, y * 32, (x + 1) * 32, (y + 1) * 32);
	}
}

// FUNCTION: EVO 0x1003DF50
void EVNoteAlert(StringPtr text)
{
	Rect	box;
	short	itemHit;
	short	itemType;
	short	i;
	Handle	item;
	DialogPtr	dlg;
	Str255	buf;
	Boolean	done;

	done = false;
	SATSetPortScreen();
	alertDialog = nil;
	alertDialog = GetNewDialog(kNoteAlertID, nil, (WindowPtr)-1);
	if (alertDialog != nil) {
		SetPort(alertDialog);
		SetDialogDefaultItem(alertDialog, kOkItem);
		ShowWindow(alertDialog);
		SetPort(alertDialog);
		SelectWindow(alertDialog);
		SetPort(alertDialog);
		DrawDialog(alertDialog);
		FrameDefaultButton(alertDialog, kOkItem);
		strncpy((char *)buf, (char *)text, 254);
		GetDialogItem(alertDialog, kAlertTextItem, &itemType, &item, &box);
		SetDialogItemText(item, buf);
		FlushEvents(everyEvent - diskMask, 0);
		do {
			ModalDialog(nil, &itemHit);
			if (itemHit == kOkItem)
				done = true;
		} while (!done);
		HideWindow(alertDialog);
		SATSetPortScreen();
		DisposeDialog(alertDialog);
		SATSetPortScreen();
	}
}

// FUNCTION: EVO 0x1003E0C8
Boolean EVYesNoDialog(StringPtr text)
{
	Rect	box;
	short	itemHit;
	short	itemType;
	short	i;
	Handle	item;
	DialogPtr	dlg;
	Str255	buf;
	Boolean	done;
	Boolean	result;

	done = false;
	alertDialog = nil;
	alertDialog = GetNewDialog(kYesNoDlogID, nil, (WindowPtr)-1);
	if (alertDialog == nil)
		return;
	SetDialogDefaultItem(alertDialog, kOkItem);
	ShowWindow(alertDialog);
	SelectWindow(alertDialog);
	SetPort(alertDialog);
	FrameDefaultButton(alertDialog, kOkItem);
	strncpy((char *)buf, (char *)text, 254);
	GetDialogItem(alertDialog, kAlertTextItem, &itemType, &item, &box);
	SetDialogItemText(item, buf);
	FlushEvents(everyEvent - diskMask, 0);
	do {
		ModalDialog(nil, &itemHit);
		if (itemHit == kOkItem) {
			result = true;
			done = true;
		}
		if (itemHit == kAlertNoItem) {
			result = false;
			done = true;
		}
	} while (!done);
	SATSetPortScreen();
	HideWindow(alertDialog);
	DisposeDialog(alertDialog);
	SATSetPortScreen();
	return result;
}

// FUNCTION: EVO 0x1003E23C
Boolean EVTextInputDialog(StringPtr prompt, StringPtr text, short maxLen)
{
	Rect	box;
	short	itemHit;
	short	itemType;
	short	i;
	Handle	item;
	DialogPtr	dlg;
	Str255	buf;
	Boolean	done;
	Boolean	result;
	short	len;

	done = false;
	alertDialog = nil;
	alertDialog = GetNewDialog(kTextInputDlogID, nil, (WindowPtr)-1);
	if (alertDialog == nil)
		return;
	SetDialogDefaultItem(alertDialog, kOkItem);
	NewDialogHook(alertDialog, 0);
	ShowWindow(alertDialog);
	SelectWindow(alertDialog);
	SetPort(alertDialog);
	FrameDefaultButton(alertDialog, kOkItem);
	GetDialogItem(alertDialog, kAlertTextItem, &itemType, &item, &box);
	SetDialogItemText(item, prompt);
	strncpy((char *)buf, (char *)text, 254);
	GetDialogItem(alertDialog, kAlertEditItem, &itemType, &item, &box);
	SetDialogItemText(item, buf);
	SelectDialogItemText(alertDialog, kAlertEditItem, 0, 254);
	FlushEvents(everyEvent - diskMask, 0);
	do {
		ModalDialog(nil, &itemHit);
		if (itemHit == kOkItem) {
			GetDialogItem(alertDialog, kAlertEditItem, &itemType, &item, &box);
			GetDialogItemText(item, buf);
			p2cstr(buf);
			len = strlen((char *)buf);
			if (len <= maxLen) {
				result = true;
				done = true;
			} else {
				SysBeep(0);
				SelectDialogItemText(alertDialog, kAlertEditItem, 0, maxLen - 1);
			}
		}
		if (itemHit == kAlertCancelItem) {
			result = false;
			done = true;
		}
	} while (!done);
	GetDialogItem(alertDialog, kAlertEditItem, &itemType, &item, &box);
	GetDialogItemText(item, gTextInputResult);
	SATSetPortScreen();
	DisposeDialog(alertDialog);
	SATSetPortScreen();
	return result;
}

// FUNCTION: EVO 0x1003E49C
void ShowTextDialog(Boolean allowMap, Boolean redrawMission)
{
	Rect			box;
	short			itemHit;
	short			itemType;
	Handle			item;
	ModalFilterUPP	filterUPP;
	Boolean			done;
	short			target;
	short			i;

	done = false;
	filterUPP = NewModalFilterProc(TextDialogFilter);
	for (i = 0; i <= 1; i++)
		textDialogPics[i] = GetPicture(kOkayButtonPict + i);
	alertDialog = nil;
	alertDialog = GetNewDialog(kTextDlogID, nil, (WindowPtr)-1);
	if (alertDialog != nil) {
		NewDialogHook(alertDialog, 0);
		CenterWindow(alertDialog);
		ShowWindow(alertDialog);
		SelectWindow(alertDialog);
		SetPort(alertDialog);
		FlushEvents(everyEvent - diskMask, 0);
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kOkItem)
				done = true;
			if (itemHit == kTextMapItem && allowMap) {
				target = shipData->targetStellar;
				DoSystemMap();
				shipData->targetStellar = target;
				if (portDialog != nil) {
					SetPort(portDialog);
					InvalRect(&portDialog->portRect);
					PortDialogUpdate();
				}
				if (redrawMission) {
					SetPort(missionDialog);
					InvalRect(&missionDialog->portRect);
				}
				SetPort(alertDialog);
				InvalRect(&alertDialog->portRect);
				TextDialogUpdate();
			}
		} while (!done);
		for (i = 0; i <= 1; i++) {
			if (textDialogPics[i] != nil) {
				HPurge((Handle)textDialogPics[i]);
				ReleaseResource((Handle)textDialogPics[i]);
			}
		}
		SATSetPortScreen();
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(alertDialog);
	}
}

// FUNCTION: EVO 0x1003E6E4
pascal Boolean TextDialogFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	Rect	r;
	Rect	box;
	short	itemType;
	Handle	item;
	char	key;
	Boolean	hit;

	QuickKeyCheckHandler();
	if (QuickKeyCheck(keyMap[kCtlMap])) {
		*itemHit = kTextMapItem;
		return true;
	}
	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		if (key == kReturnCharCode || key == kEnterCharCode) {
			*itemHit = kOkItem;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		SetPort(alertDialog);
		GetDialogItem(alertDialog, kOkItem, &itemType, &item, &box);
		hit = TrackOkButton(box);
		DrawOkButton(box, false);
		if (hit)
			*itemHit = kOkItem;
		else
			*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(alertDialog);
		TextDialogUpdate();
		EndUpdate(alertDialog);
	}
	return false;
}

// FUNCTION: EVO 0x1003E83C
void TextDialogUpdate(void)
{
	Rect	box;
	short	itemType;
	Handle	item;

	TextFont(geneva);
	TextSize(kDialogTextSize);
	ForeColor(blackColor);
	PaintRect(&alertDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&alertDialog->portRect);
	ForeColor(blackColor);
	ForeColor(whiteColor);
	BackColor(blackColor);
	GetDialogItem(alertDialog, kAlertTextItem, &itemType, &item, &box);
	TETextBox(gDialogText, strlen(gDialogText), &box, teFlushDefault);
	ForeColor(blackColor);
	BackColor(whiteColor);
	GetDialogItem(alertDialog, kOkItem, &itemType, &item, &box);
	DrawOkButton(box, false);
}

// FUNCTION: EVO 0x1003E960
void ShowHectorDialog(void)
{
	Rect			box;
	short			itemHit;
	short			itemType;
	Handle			item;
	ModalFilterUPP	filterUPP;
	Boolean			done;
	short			i;

	done = false;
	filterUPP = NewModalFilterProc(HectorDialogFilter);
	for (i = 0; i <= 1; i++)
		textDialogPics[i] = GetPicture(kOkayButtonPict + i);
	hectorPic = GetPicture(kHectorPictID);
	alertDialog = nil;
	alertDialog = GetNewDialog(kHectorDlogID, nil, (WindowPtr)-1);
	if (alertDialog != nil) {
		NewDialogHook(alertDialog, 0);
		CenterWindow(alertDialog);
		ShowWindow(alertDialog);
		SelectWindow(alertDialog);
		SetPort(alertDialog);
		FlushEvents(everyEvent - diskMask, 0);
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kOkItem)
				done = true;
		} while (!done);
		for (i = 0; i <= 1; i++) {
			if (textDialogPics[i] != nil) {
				HPurge((Handle)textDialogPics[i]);
				ReleaseResource((Handle)textDialogPics[i]);
			}
		}
		if (hectorPic != nil) {
			HPurge((Handle)hectorPic);
			ReleaseResource((Handle)hectorPic);
		}
		SATSetPortScreen();
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(alertDialog);
	}
}

// FUNCTION: EVO 0x1003EB2C
pascal Boolean HectorDialogFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	Rect	r;
	Rect	box;
	short	itemType;
	Handle	item;
	char	key;
	Boolean	hit;

	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		if (key == kReturnCharCode || key == kEnterCharCode) {
			*itemHit = kOkItem;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		SetPort(alertDialog);
		GetDialogItem(alertDialog, kOkItem, &itemType, &item, &box);
		hit = TrackOkButton(box);
		DrawOkButton(box, false);
		if (hit)
			*itemHit = kOkItem;
		else
			*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(alertDialog);
		HectorDialogUpdate();
		EndUpdate(alertDialog);
	}
	return false;
}

// FUNCTION: EVO 0x1003EC54
void HectorDialogUpdate(void)
{
	Rect	box;
	short	itemType;
	Handle	item;

	TextFont(geneva);
	TextSize(kDialogTextSize);
	ForeColor(blackColor);
	PaintRect(&alertDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&alertDialog->portRect);
	ForeColor(blackColor);
	ForeColor(whiteColor);
	BackColor(blackColor);
	GetDialogItem(alertDialog, kHectorTextItem, &itemType, &item, &box);
	TETextBox(gDialogText, strlen(gDialogText), &box, teFlushDefault);
	ForeColor(blackColor);
	BackColor(whiteColor);
	GetDialogItem(alertDialog, kHectorPictItem, &itemType, &item, &box);
	DrawPicture(hectorPic, &box);
	GetDialogItem(alertDialog, kOkItem, &itemType, &item, &box);
	DrawOkButton(box, false);
}

// FUNCTION: EVO 0x1003EDA8
void DoPlayerInfoDialog(void)
{
	Rect			box;
	short			itemType;
	Handle			item;
	short			itemHit;
	char			pad[28];
	Str255			pa;
	Str255			pb;
	Boolean			done;
	ModalFilterUPP	filterUPP;
	short			i;

	done = false;
	filterUPP = NewModalFilterProc(PlayerInfoFilter); /* BUG: the routine descriptor leaks when the dialog is refused */
	if (shipData->jumpTimer != 0)
		return;
	if (shipData->deathTimer > 0)
		return;
	if (fadePending[0] || fadePending[1])
		return;
	ShipStatsToSystemInfo();
	infoPage = 1;
	infoPics[0] = GetPicture(kDone78ButtonPict);
	infoPics[1] = GetPicture(kDone78ButtonPict + 1);
	infoPics[2] = GetPicture(kGeneralButtonPict);
	infoPics[3] = GetPicture(kGeneralButtonPict + 1);
	infoPics[4] = GetPicture(kCargo121ButtonPict);
	infoPics[5] = GetPicture(kCargo121ButtonPict + 1);
	infoPics[6] = GetPicture(kExtrasButtonPict);
	infoPics[7] = GetPicture(kExtrasButtonPict + 1);
	infoDialog = nil;
	infoDialog = GetNewDialog(kInfoDlogID, nil, (WindowPtr)-1);
	if (infoDialog != nil) {
		NewDialogHook(infoDialog, 0);
		CenterWindow(infoDialog);
		ShowWindow(infoDialog);
		SelectWindow(infoDialog);
		SetPort(infoDialog);
		FlushEvents(everyEvent - diskMask, 0);
		InvalRect(&infoDialog->portRect);
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kOkItem)
				done = true;
			if (itemHit >= 2 && itemHit <= 5) {
				infoPage = itemHit - 1;
				InvalRect(&infoDialog->portRect);
			}
		} while (!done);
		for (i = 0; i <= 9; i++) {
			if (infoPics[i] != nil) {
				HPurge((Handle)infoPics[i]);
				ReleaseResource((Handle)infoPics[i]);
			}
		}
		SATSetPortScreen();
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(infoDialog);
		SATSetPortScreen();
		RefreshSAT();
	}
}

// FUNCTION: EVO 0x1003F044
pascal Boolean PlayerInfoFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	Rect	r;
	Rect	box;
	Point	where;
	char	key;
	short	hit;

	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		if (key == '	') {
			if (ev->modifiers & shiftKey)
				infoPage--;
			else
				infoPage++;
			if (infoPage > 3)
				infoPage = 1;
			if (infoPage < 1)
				infoPage = 3;
			InvalRect(&infoDialog->portRect);
			return true;
		}
		if (key == kEnterCharCode) {
			*itemHit = kOkItem;
			return true;
		}
		if (infoPage != 4 && key == kReturnCharCode) {
			*itemHit = kOkItem;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		hit = TrackPlayerInfoButtons(infoPage, where);
		if (hit == 0) {
			*itemHit = kOkItem;
			return true;
		}
		if (hit == 1) {
			*itemHit = 2;
			return true;
		}
		if (hit == 2) {
			*itemHit = 3;
			return true;
		}
		if (hit == 3) {
			*itemHit = 4;
			return true;
		}
		if (hit == 4) {
			*itemHit = 5;
			return true;
		}
		*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(infoDialog);
		PlayerInfoDialogUpdate();
		EndUpdate(infoDialog);
		return false;
	}
	return false;
}

// FUNCTION: EVO 0x1003F254
void PlayerInfoDialogUpdate(void)
{
	short		i;
	short		j;
	short		n;
	short		m;
	short		p;
	short		q;
	char		c;
	long		total;
	Boolean		isEscortMission;
	short		itemType;
	Handle		item;
	Rect		box;
	Rect		textBox;
	char		text[1024];
	Str255		num;
	short		freeShip;
	short		missionCount;
	short		junkTypes;
	short		missionTons;
	short		junkTons;
	short		tons[MaxCargoTypes];
	short		dead;
	Boolean		unused[MaxCargoTypes];

	SATSetPort(&loadPort);
	ForeColor(blackColor);
	PaintRect(&infoDialog->portRect);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	GetDialogItem(infoDialog, kInfoPageItem, &itemType, &item, &box);
	if (RectInRgn(&box, infoDialog->visRgn)) {
		if (infoPage == 1) {
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 5, box.top + 12);
			DrawString("\pPilot Name:");
			ForeColor(whiteColor);
			MoveTo(box.left + 80, box.top + 12);
			DrawString(playerName);
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 5, box.top + 28);
			DrawString("\pCurrent Date:");
			ForeColor(whiteColor);
			MoveTo(box.left + 80, box.top + 28);
			DateToLongString(gameTime, text);
			c2pstr(text);
			DrawString((StringPtr)text);
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 5, box.top + 44);
			DrawString("\pSystem:");
			ForeColor(whiteColor);
			MoveTo(box.left + 80, box.top + 44);
			DrawString(gSystems[shipData->curSystem].name);
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 5, box.top + 60);
			DrawString("\pLegal Status:");
			ForeColor(whiteColor);
			MoveTo(box.left + 80, box.top + 60);
			if (SystemIsInhabited(shipData->curSystem))
				DrawLegalStatusString(shipData->curSystem);
			else
				DrawString("\pN/A");
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 5, box.top + 76);
			DrawString("\pCombat Rating:");
			ForeColor(whiteColor);
			MoveTo(box.left + 80, box.top + 76);
			DrawCombatRatingString();
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 205, box.top + 12);
			DrawString("\pShip Name:");
			ForeColor(whiteColor);
			MoveTo(box.left + 280, box.top + 12);
			DrawString(shipName);
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 205, box.top + 28);
			DrawString("\pShip Class:");
			ForeColor(whiteColor);
			MoveTo(box.left + 280, box.top + 28);
			DrawString(gShipTypes[shipData->type].name);
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 205, box.top + 44);
			DrawString("\pTurn Rate:");
			ForeColor(whiteColor);
			MoveTo(box.left + 280, box.top + 44);
			NumToString(ShipTurnRate(shipData) * 30, (StringPtr)text);
			DrawString((StringPtr)text);
			DrawString("\p\xA1/sec");
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 205, box.top + 60);
			DrawString("\pAccel Rate:");
			ForeColor(whiteColor);
			MoveTo(box.left + 280, box.top + 60);
			NumToString(2500.0 * ShipAccelRate(shipData), (StringPtr)text);
			DrawString((StringPtr)text);
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 205, box.top + 76);
			DrawString("\pMax Speed:");
			ForeColor(whiteColor);
			MoveTo(box.left + 280, box.top + 76);
			if (strictPlay)
				NumToString(100.0f * ShipMaxSpeed(shipData), (StringPtr)text);
			else
				NumToString(2.0 / 3.0 * (100.0f * ShipMaxSpeed(shipData)), (StringPtr)text);
			DrawString((StringPtr)text);
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 205, box.top + 92);
			DrawString("\pCredits:");
			ForeColor(whiteColor);
			MoveTo(box.left + 280, box.top + 92);
			DrawCommaNumber(shipData->credits);
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 5, box.top + 92);
			if (shipData->shield >= 0)
				DrawString("\pShield Status:");
			else
				DrawString("\pArmor Status:");
			ForeColor(whiteColor);
			MoveTo(box.left + 80, box.top + 92);
			if (shipData->shield >= 0) {
				NumToString(100.0 * (shipData->shield / (float)ShipShieldCapacity(shipData)), (StringPtr)text);
				DrawString((StringPtr)text);
				DrawString("\p%");
			} else {
				if (IsDying(shipData))
					DrawString("\pFailed");
				else {
					NumToString(100.0 * ((shipData->shield + ShipArmorCapacity(shipData)) / (float)ShipArmorCapacity(shipData)), (StringPtr)text);
					DrawString((StringPtr)text);
					DrawString("\p%");
				}
			}
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 5, box.top + 108);
			DrawString("\pFuel Status:");
			ForeColor(whiteColor);
			MoveTo(box.left + 80, box.top + 108);
			NumToString(100.0 * (shipData->fuel / (float)ShipFuelCapacity(shipData)), (StringPtr)text);
			DrawString((StringPtr)text);
			DrawString("\p%");
			n = shipData->fuel / 100.0;
			if (n > 0) {
				DrawString("\p  (");
				NumToString(n, (StringPtr)text);
				DrawString((StringPtr)text);
				DrawString("\p jump");
				if (n > 1)
					DrawString("\ps");
				if ((short)shipData->fuel % 100 > 0)
					DrawString("\p plus maneuvering fuel");
				DrawString("\p)");
			} else if (shipData->fuel > 0)
				DrawString("\p  (maneuvering fuel only)");
			total = 0;
			n = 0;
			for (i = 1; i < MaxShipsInSystem; i++) {
				if (shipData[i].active && shipData[i].parentShip == 0) {
					isEscortMission = false;
					if (shipData[i].specialMission != -1 && currentMissionObjectives[shipData[i].specialMission].active) {
						for (j = currentMissions[shipData[i].specialMission].shipBehav; j >= 9; j -= 10)
							;
						if (j == 1)
							isEscortMission = true;
					}
					if (!isEscortMission && shipData[i].hired)
						total += 0.01 * gShipTypes[shipData[i].type].cost;
				}
			}
			if (total > 0) {
				n += 16;
				RGBForeColor(&rgbLightGrey);
				MoveTo(box.left + 5, box.top + 124);
				DrawString("\pEscort Payments:  ");
				ForeColor(whiteColor);
				DrawCommaNumber(total);
				DrawString("\p credits");
				RGBForeColor(&rgbLightGrey);
				DrawString("\p per day");
			}
			total = 0;
			for (i = 0; i < MaxStellarObjects; i++) {
				if (stellarData[i].loaded && stellarData[i].dominated)
					total += stellarData[i].techLevel * 1000;
			}
			if (total > 0) {
				RGBForeColor(&rgbLightGrey);
				MoveTo(box.left + 5, box.top + 124 + n);
				DrawString("\pTribute Received:  ");
				ForeColor(whiteColor);
				DrawCommaNumber(total);
				DrawString("\p credits");
				RGBForeColor(&rgbLightGrey);
				DrawString("\p per day");
			}
		}
		if (infoPage == 2) {
			n = 0;
			missionTons = 0;
			junkTons = 0;
			missionCount = 0;
			junkTypes = 0;
			for (i = 0; i < MaxCargoTypes; i++) {
				unused[i] = false;
				tons[i] = -1;
			}
			for (i = 0; i <= 5; i++) {
				if (shipData->cargo[i] > 0) {
					if (tons[i] < 0)
						tons[i] = 0;
					tons[i] += shipData->cargo[i];
					n++;
				}
			}
			for (p = 0; p < MaxSimultaneousMissions; p++) {
				if (currentMissionObjectives[p].active && currentMissions[p].cargoPickedUp && currentMissions[p].cargoType != -1) {
					n++;
					missionCount++;
					missionTons += currentMissions[p].cargoQty;
					if (tons[currentMissions[p].cargoType] < 0)
						tons[currentMissions[p].cargoType] = 0;
					tons[currentMissions[p].cargoType] += currentMissions[p].cargoQty;
				}
			}
			m = 0;
			for (i = 0; i < MaxCargoTypes; i++) {
				if (tons[i] >= 0)
					m++;
			}
			for (i = 0; i < kMaxJunkTypes; i++) {
				if (junkData[i].held > 0) {
					n++;
					junkTons += junkData[i].held;
					junkTypes++;
					m++;
				}
			}
			if (m > 0) {
				RGBForeColor(&rgbLightGrey);
				MoveTo(box.left + 5, box.top + 12);
				if (TotalFleetHolds() > ShipTotalHolds())
					DrawString("\pCurrent cargo aboard your fleet\xD5s ships:");
				else
					DrawString("\pCurrent cargo aboard your ship:");
				strncpy(text, "", 3);
				j = 0;
				for (i = 0; i < MaxCargoTypes; i++) {
					if (tons[i] != -1) {
						if (tons[i] > 0) {
							if (tons[i] > 5) {
								NumToString(tons[i], num);
								p2cstr(num);
								strcat(text, (char *)num);
							} else {
								if (tons[i] == 1)
									strcat(text, "one");
								else if (tons[i] == 2)
									strcat(text, "two");
								else if (tons[i] == 3)
									strcat(text, "three");
								else if (tons[i] == 4)
									strcat(text, "four");
								else if (tons[i] == 5)
									strcat(text, "five");
							}
							if (tons[i] == 1)
								strcat(text, " ton");
							else
								strcat(text, " tons");
							strcat(text, " of ");
						}
						strncpy((char *)num, lcCargoName[i], 19);
						p2cstr(num);
						strcat(text, (char *)num);
						if (j < m - 2)
							strcat(text, ", ");
						if (j == m - 2) {
							if (m >= 3)
								strcat(text, ",");
							strcat(text, " and ");
						}
						j++;
					}
				}
				if (junkTypes > 0) {
					for (i = 0; i < kMaxJunkTypes; i++) {
						if (junkData[i].held > 0) {
							if (junkData[i].held > 5) {
								NumToString(junkData[i].held, num);
								p2cstr(num);
								strcat(text, (char *)num);
							} else {
								if (junkData[i].held == 1)
									strcat(text, "one");
								else if (junkData[i].held == 2)
									strcat(text, "two");
								else if (junkData[i].held == 3)
									strcat(text, "three");
								else if (junkData[i].held == 4)
									strcat(text, "four");
								else if (junkData[i].held == 5)
									strcat(text, "five");
							}
							if (junkData[i].held == 1)
								strcat(text, " ton");
							else
								strcat(text, " tons");
							strcat(text, " of ");
							strncpy((char *)num, lcJunkName[i], 19);
							p2cstr(num);
							strcat(text, (char *)num);
							if (j < m - 2)
								strcat(text, ", ");
							if (j == m - 2) {
								if (m >= 3)
									strcat(text, ",");
								strcat(text, " and ");
							}
							j++;
						}
					}
				}
				ForeColor(blackColor);
				textBox = box;
				InsetRect(&textBox, 10, 10);
				textBox.top += 10;
				textBox.bottom -= 10;
				textBox.right -= 5;
				text[0] = toupper(text[0]);
				strcat(text, ".");
				TETextBox(text, strlen(text), &textBox, teJustLeft);
				InvertRect(&textBox);
				n = TotalFleetHolds() - TotalCargo(shipData);
				if (n > 0) {
					if (n < 0)
						n = 0;
					MoveTo(box.left + 5, box.bottom - 14);
					RGBForeColor(&rgbLightGrey);
					NumToString(n, num);
					if (TotalFleetHolds() > ShipTotalHolds()) {
						DrawString("\pFree cargo space in fleet: ");
						ForeColor(whiteColor);
						DrawString(num);
						DrawString("\p ton");
						if (n > 1)
							DrawString("\ps");
						freeShip = TotalMissionCargoSpace();
						if (freeShip < 0)
							freeShip = 0;
						if (freeShip != n && freeShip > 0) {
							DrawString("\p (");
							NumToString(freeShip, num);
							DrawString(num);
							DrawString("\p ton");
							if (freeShip > 1)
								DrawString("\ps");
							DrawString("\p free in your ship)");
						} else if (freeShip == 0 && n > 0)
							DrawString("\p (none free in your ship)");
					} else {
						DrawString("\pFree cargo space: ");
						ForeColor(whiteColor);
						DrawString(num);
						DrawString("\p ton");
						if (n > 1)
							DrawString("\ps");
					}
				}
			} else {
				RGBForeColor(&rgbLightGrey);
				if (TotalFleetHolds() > ShipTotalHolds())
					DrawCenteredText("\pYou don’t have any cargo aboard your fleet’s ships", box.left, box.right, box.top + 55);
				else
					DrawCenteredText("\pYou don’t have any cargo aboard your ship", box.left, box.right, box.top + 55);
			}
		}
		if (infoPage == 3) {
			q = 0;
			for (i = 0; i < MaxOutfitItemTypes; i++) {
				if (outfitCount[i] > 0)
					q++;
			}
			if (q > 0) {
				RGBForeColor(&rgbLightGrey);
				MoveTo(box.left + 5, box.top + 12);
				DrawString("\pCurrent extras for your ship:");
				strncpy(text, "", 3);
				j = 0;
				for (i = 0; i < MaxOutfitItemTypes; i++) {
					if (outfitCount[i] > 0) {
						if (outfitCount[i] == 1) {
							c = tolower(lcOutfitName[i][1]);
							if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
								strcat(text, "an ");
							else
								strcat(text, "a ");
						} else if (outfitCount[i] == 2)
							strcat(text, "two ");
						else if (outfitCount[i] == 3)
							strcat(text, "three ");
						else if (outfitCount[i] > 3) {
							NumToString(outfitCount[i], num);
							p2cstr(num);
							strcat(text, (char *)num);
							strcat(text, " ");
						}
						if (outfitCount[i] > 1)
							strncpy((char *)num, (char *)lcOutfitPluralName[i], 29);
						else
							strncpy((char *)num, (char *)lcOutfitName[i], 29);
						p2cstr(num);
						strcat(text, (char *)num);
						j++;
						if (j < q - 1)
							strcat(text, ", ");
						if (j == q - 1)
							strcat(text, ", and ");
					}
				}
				ForeColor(blackColor);
				textBox = box;
				InsetRect(&textBox, 10, 10);
				textBox.top += 10;
				textBox.bottom -= 10;
				textBox.right -= 5;
				text[0] = toupper(text[0]);
				strcat(text, ".");
				TETextBox(text, strlen(text), &textBox, teJustLeft);
				InvertRect(&textBox);
				MoveTo(box.left + 5, box.bottom - 14);
				RGBForeColor(&rgbLightGrey);
				DrawString("\pShip trade-in value: ");
				ForeColor(whiteColor);
				DrawCommaNumber(PlayerShipTradeInPrice());
				DrawString("\p credits");
			} else {
				RGBForeColor(&rgbLightGrey);
				DrawCenteredText("\pYou don\xD5t have any extras on your ship", box.left, box.right, box.top + 55);
			}
		}
	}
	DrawPlayerInfoButtons(infoPage, -1);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&infoDialog->portRect);
	ForeColor(blackColor);
	SATSetPortScreen();
	SetPort(infoDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &infoDialog->portBits, &infoDialog->portRect, &infoDialog->portRect, srcCopy, infoDialog->visRgn);
}

// FUNCTION: EVO 0x10040EB8
Boolean DoCapturedShipAssignmentDialog(void)
{
	Rect			box;
	short			itemHit;
	short			itemType;
	Handle			item;
	ModalFilterUPP	filterUPP;
	Boolean			done;
	Boolean			result;
	short			i;

	done = false;
	filterUPP = NewModalFilterProc(CapturedAssignmentFilter);
	capturePics[0] = GetPicture(kUseAsMyShipButtonPict);
	capturePics[1] = GetPicture(kUseAsMyShipButtonPict + 1);
	capturePics[2] = GetPicture(kUseAsEscortButtonPict);
	capturePics[3] = GetPicture(kUseAsEscortButtonPict + 1);
	alertDialog = nil;
	alertDialog = GetNewDialog(kCaptureDlogID, nil, (WindowPtr)-1);
	if (alertDialog == nil)
		return false;
	NewDialogHook(alertDialog, 0);
	CenterWindow(alertDialog);
	ShowWindow(alertDialog);
	SelectWindow(alertDialog);
	SetPort(alertDialog);
	FlushEvents(everyEvent - diskMask, 0);
	do {
		ModalDialog(filterUPP, &itemHit);
		if (itemHit == kCaptureMyShipItem) {
			PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
			result = true;
			done = true;
		}
		if (itemHit == kCaptureEscortItem) {
			PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
			result = false;
			done = true;
		}
	} while (!done);
	for (i = 0; i <= 3; i++) {
		if (capturePics[i] != nil) {
			HPurge((Handle)capturePics[i]);
			ReleaseResource((Handle)capturePics[i]);
		}
	}
	SATSetPortScreen();
	DisposeRoutineDescriptor(filterUPP);
	DisposeDialog(alertDialog);
	return result;
}

// FUNCTION: EVO 0x100410B4
pascal Boolean CapturedAssignmentFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	Rect	r;
	Rect	box;
	short	itemType;
	Point	where;
	Handle	item;
	char	key;
	short	button;

	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		key = tolower(key);
		if (key == 'm') {
			*itemHit = kCaptureMyShipItem;
			return true;
		}
		if (key == 'e') {
			*itemHit = kCaptureEscortItem;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		button = TrackCapAssignButtons(where);
		if (button == 0) {
			*itemHit = kCaptureMyShipItem;
			return true;
		}
		if (button == 1) {
			*itemHit = kCaptureEscortItem;
			return true;
		}
		*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(alertDialog);
		CapturedAssignmentDialogUpdate();
		EndUpdate(alertDialog);
	}
	return false;
}

// FUNCTION: EVO 0x1004120C
void CapturedAssignmentDialogUpdate(void)
{
	Rect	box;
	short	itemType;
	Handle	item;
	Str255	text;

	TextFont(geneva);
	TextSize(kDialogTextSize);
	ForeColor(blackColor);
	PaintRect(&alertDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&alertDialog->portRect);
	ForeColor(blackColor);
	ForeColor(whiteColor);
	BackColor(blackColor);
	GetDialogItem(alertDialog, kAlertTextItem, &itemType, &item, &box);
	strncpy((char *)text, "Do you want to use this ship as an escort to yours, or would you rather trade places with its captain and use it as your own?", 250);
	TETextBox(text, strlen((char *)text), &box, teFlushDefault);
	ForeColor(blackColor);
	BackColor(whiteColor);
	DrawCapAssignButtons(-1);
}

// FUNCTION: EVO 0x10041320
void FrameDefaultButton(DialogPtr dlg, short item)
{
	short	itemType;
	Handle	itemHandle;
	Rect	box;

	SetPort(dlg);
	GetDialogItem(dlg, item, &itemType, &itemHandle, &box);
	PenSize(3, 3);
	InsetRect(&box, -4, -4);
	FrameRoundRect(&box, 16, 16);
}

// FUNCTION: EVO 0x100413AC
long ApplyPriceAndTechnologyFlux(long cost, float multiplier, short itemTech, short portTech)
{
	long	price;

	price = cost;
	if (itemTech <= 5 && portTech <= 5 && itemTech < portTech && price >= 100)
		price *= 0.01 * (100 - (portTech - itemTech) * 3);
	if (price > 100000) {
		price /= 1000;
		price *= 1000;
	} else if (price > 10000) {
		price /= 100;
		price *= 100;
	} else if (price > 100) {
		price /= 10;
		price *= 10;
	}
	return price;
}
