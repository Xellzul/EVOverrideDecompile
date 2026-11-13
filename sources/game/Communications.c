/* Communications.c -- hailing: the ship and planet comm dialogs, bribes and haggling, boarding and plunder. Notes: Research/notes/Communications.md */
#define OLDROUTINENAMES 1
#include <Dialogs.h>
#include <Fonts.h>
#include <Resources.h>
#include <TextUtils.h>
#include <TextEdit.h>
#include <ToolUtils.h>
#include <string.h>
#include <ctype.h>

#include "evo.h"


#define kCommDlogID			1007
#define kHaggleDlogID		1008
#define kPlanetCommDlogID	1009
#define kPlunderDlogID		1011
#define kPlanetCommPictID	5500

#define kGovtNameStrList	1100
#define kResponseStrList	3000
#define kPlanetResponseStrList	3002
#define kGovtAdviceStrList	7000
#define kPersQuoteStrList	7100
#define kDudeAdviceStrList	7500

#define kPlunderLeaveItem	1
#define kPlunderCargoItem	2
#define kPlunderCreditsItem	3
#define kPlunderAmmoItem	4
#define kPlunderFuelItem	6
#define kPlunderCaptureItem	7

#define kShipClassStrList	6002
#define kShipGovtStrList	6001
#define kOwnShipClassStrList	5006
#define kCommShipPictBase	5300

#define kPlugShipClassName	3800
#define kPlugOwnShipClassName	3900
#define kPlugShipGovtName	4200
#define kPlugGovtName		7000
#define kPlugTraderAdvice	10010
#define kPlugWarshipAdvice	10015
#define kPlugPersQuote		12000

#define kBribeStep		1000
#define kMaxBribe		20000
#define kMaxTribute		900000
#define kAdviceBand		1000

#define kHaggleMission		0
#define kHaggleRaise		1
#define kHaggleLower		2

#define kBribeLandPermission	751


DialogPtr	commDialog;
DialogPtr	haggleDialog;
DialogPtr	plunderDialog;
short		haggleMode;
short		responseRandom;
short		adviceRandom;
short		adviceDisasterRandom;
short		salvageType;
short		salvageQty;
short		salvageAmmoType;
short		salvageAmmoQty;
short		salvageFuelQty;
short		captureOdds;
short		bribeRandom;
long		salvageCredits;
long		gBribeAsked;
long		haggleVal;
float		gCommMood;
Boolean		hatesPlayer;
Boolean		willTakeBribe;
Boolean		hasDemandedTribute;
Boolean		commShipXeno;
Boolean		releaseCommEscort;
PicHandle	commPics[16];
PicHandle	hagglePics[4];
PicHandle	plunderPics[12];
PicHandle	commShipPic;
ShipObj		*dialogShip;
Str255		commResponseString;
Str255		adviceString;
Str255		gCommGovtName;
Str255		shipClassStr;
Str255		shipGovtStr;

// FUNCTION: EVO 0x1000F4F8
void DoCommDialog(short shipIndex)
{
	short			y;
	short			itemHit;
	short			itemType;
	Handle			item;
	Str255			buf;
	Rect			box;
	ModalFilterUPP	filterUPP;
	Boolean			done;
	Boolean			hasMission;
	Boolean			missionPending;
	short			i;
	short			n;
	short			behav;

	done = false;
	filterUPP = NewModalFilterProc(CommFilter);
	if (shipIndex == -1)
		return;
	if (!shipData[shipIndex].active)
		return;
	dialogShip = &shipData[shipIndex];
	willTakeBribe = false;
	hasDemandedTribute = false;
	releaseCommEscort = false;
	if (dialogShip->govt == -1)
		willTakeBribe = true;
	else {
		if (dialogShip->aiType <= kAIBraveTrader && (govtData[dialogShip->govt].flags & kGovtFreighterBribes))
			willTakeBribe = true;
		if (dialogShip->aiType >= kAIWarship && (govtData[dialogShip->govt].flags & kGovtWarshipBribes))
			willTakeBribe = true;
	}
	responseRandom = Rand(5);
	gCommMood = (Rand(41) + 80) * (0.01);
	if (Rand(5) == 0)
		gCommMood -= 0.5f;
	else if (Rand(5) == 0)
		gCommMood += 0.5f;
	n = shipData->credits * (0.0000005);
	if (n <= 0)
		n = 1;
	gBribeAsked = (Rand(n) * kBribeStep + 3000) * gCommMood;
	if (dialogShip->govt != -1 && (govtData[dialogShip->govt].flags & kGovtGreedy)) {
		n = shipData->credits * (0.0001);
		if (n <= 0)
			n = 1;
		gBribeAsked = (Rand(n) * kBribeStep + 10000) * gCommMood;
	}
	if (gBribeAsked > shipData->credits * (0.333))
		gBribeAsked = shipData->credits * (0.333);
	gBribeAsked /= kBribeStep;
	gBribeAsked *= kBribeStep;
	if (gBribeAsked > kMaxBribe)
		gBribeAsked = kMaxBribe;
	if (gBribeAsked < kBribeStep)
		gBribeAsked = kBribeStep;
	commShipXeno = false;
	if (dialogShip->govt != -1 && (govtData[dialogShip->govt].flags & kGovtXenophobic))
		commShipXeno = true;
	commShipPic = GetPicture(dialogShip->type + kCommShipPictBase);
	if (dialogShip->govt != -1) {
		if (!LoadPluginString(dialogShip->type + kPlugShipClassName, shipClassStr))
			GetIndString(shipClassStr, kShipClassStrList, dialogShip->type + 1);
		if (!LoadPluginString(dialogShip->govt + kPlugShipGovtName, shipGovtStr))
			GetIndString(shipGovtStr, kShipGovtStrList, dialogShip->govt + 1);
	} else {
		if (!LoadPluginString(dialogShip->type + kPlugOwnShipClassName, shipClassStr))
			GetIndString(shipClassStr, kOwnShipClassStrList, dialogShip->type + 1);
	}
	PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
	ShowCursor();
	commPics[0] = GetPicture(kGreetingsButtonPict);
	commPics[1] = GetPicture(kGreetingsButtonPict + 1);
	commPics[2] = GetPicture(kBegForMercyButtonPict);
	commPics[3] = GetPicture(kBegForMercyButtonPict + 1);
	commPics[8] = GetPicture(kEndCommunicationButtonPict);
	commPics[9] = GetPicture(kEndCommunicationButtonPict + 1);
	hasMission = false;
	if (dialogShip->specialMission != -1 && currentMissionObjectives[dialogShip->specialMission].active) {
		for (behav = currentMissions[dialogShip->specialMission].shipBehav; behav >= 9; behav -= 10)
			;
		if (behav == kMisnBehavProtect)
			hasMission = true;
	}
	if (dialogShip->parentShip == 0 && dialogShip->aiType == kAIEscort && !hasMission) {
		commPics[4] = GetPicture(kReleaseFromServitudeButtonPict);
		commPics[5] = GetPicture(kReleaseFromServitudeButtonPict + 1);
	} else {
		commPics[4] = GetPicture(kRequestAssistanceButtonPict);
		commPics[5] = GetPicture(kRequestAssistanceButtonPict + 1);
	}
	LoadAdvice();
	LoadResponse(0);
	if (dialogShip->specialMission != -1 && !IsDisabled(dialogShip)) {
		if (currentMissionObjectives[dialogShip->specialMission].active) {
			if (currentMissions[dialogShip->specialMission].shipGoal == kMisnGoalDestroy)
				LoadResponse(2);
			if (currentMissions[dialogShip->specialMission].shipGoal == kMisnGoalEscort) {
				LoadResponse(8);
				p2cstr(commResponseString);
				strcpy((char *)buf, (char *)shipName);
				p2cstr(buf);
				strcat((char *)commResponseString, (char *)buf);
				strcat((char *)commResponseString, ".");
				c2pstr((char *)commResponseString);
			}
			if (currentMissions[dialogShip->specialMission].shipBehav == kMisnBehavAttack)
				LoadResponse(2);
			if (currentMissions[dialogShip->specialMission].shipBehav == kMisnBehavProtect)
				LoadResponse(4);
		}
	} else if (!IsDisabled(dialogShip)) {
		if (ExtendedIsThreatToPlayer(dialogShip))
			LoadResponse(2);
		else if (dialogShip->parentShip == 0) {
			if (dialogShip->aiType == kAIFighter)
				LoadResponse(5);
			if (dialogShip->aiType == kAIEscort)
				LoadResponse(4);
		} else if (AIDoesShipLikePlayer(dialogShip))
			LoadResponse(0);
		else
			LoadResponse(2);
	}
	commDialog = nil;
	commDialog = GetNewDialog(kCommDlogID, nil, (WindowPtr)-1);
	if (commDialog != nil) {
		NewDialogHook(commDialog, 0);
		CenterWindow(commDialog);
		ShowWindow(commDialog);
		SelectWindow(commDialog);
		SetPort(commDialog);
		FlushEvents(everyEvent - diskMask, 0);
		InvalRect(&commDialog->portRect);
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kOkItem)
				done = true;
			if (itemHit == 2) {
				PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
				if (!IsDisabled(dialogShip)) {
					if (dialogShip->parentShip == 0 && dialogShip->aiType == kAIEscort && !hasMission) {
						releaseCommEscort = true;
						LoadResponse(38);
						GetDialogItem(commDialog, 12, &itemType, &item, &box);
						InvalRect(&box);
					} else if (ExtendedIsThreatToPlayer(dialogShip)) {
						missionPending = false;
						if (dialogShip->specialMission != -1
								&& currentMissionObjectives[dialogShip->specialMission].active
								&& currentMissions[dialogShip->specialMission].shipBehav % kMisnBehavVariant == 0)
							missionPending = true;
						if (!missionPending) {
							if (dialogShip->defenseStellar != -1)
								LoadResponse(19);
							else if (dialogShip->persType == kPersBountyHunter)
								LoadResponse(19);
							else if (willTakeBribe) {
								if (gCommMood < (0.8))
									LoadResponse(23);
								else if (gCommMood > (1.2))
									LoadResponse(24);
								else
									LoadResponse(18);
								CommDialogUpdate();
								n = DoShipCommPayment();
								SetPort(commDialog);
								if (n == 1) {
									LoadResponse(20);
									AIMakeShipLeave(dialogShip);
									dialogShip->aiType = kAIWimpyTrader;
								} else if (n == 0) {
									LoadResponse(30);
									AIMakeShipAttackPlayer(dialogShip);
								} else if (n == -1)
									LoadResponse(12);
							} else
								LoadResponse(19);
						} else
							LoadResponse(19);
					} else if (AIDoesShipLikePlayer(dialogShip) && !commShipXeno) {
						if (IsShipThreatened(dialogShip) || AIIsShipBusy(dialogShip)) {
							if (dialogShip->aiType <= kAIInterceptor)
								LoadResponse(16);
							else
								LoadResponse(22);
						} else if (IsPlayerThreatened()) {
							if (IsPlayerThreatenedByMyEnemy(dialogShip)) {
								if (commShipXeno)
									LoadResponse(19);
								else if (dialogShip->aiType == kAIWimpyTrader)
									LoadResponse(17);
								else if (dialogShip->aiType == kAIBraveTrader) {
									if (dialogShip->index % 3 == 0 && dialogShip->govt == -1) {
										if (gCommMood < (0.8))
											LoadResponse(23);
										else if (gCommMood > (1.2))
											LoadResponse(24);
										else
											LoadResponse(18);
										CommDialogUpdate();
										n = DoShipCommPayment();
										SetPort(commDialog);
										if (n == 1) {
											LoadResponse(29);
											AIFightEnemyThreatToPlayer(dialogShip);
										} else if (n == 0)
											LoadResponse(31);
										else if (n == -1)
											LoadResponse(12);
									} else
										LoadResponse(17);
								} else if (dialogShip->aiType == kAIWarship || dialogShip->aiType == kAIInterceptor) {
									if (gCommMood < (0.8))
										LoadResponse(23);
									else if (gCommMood > (1.2))
										LoadResponse(24);
									else
										LoadResponse(18);
									CommDialogUpdate();
									n = DoShipCommPayment();
									SetPort(commDialog);
									if (n == 1) {
										LoadResponse(29);
										AIFightEnemyThreatToPlayer(dialogShip);
									} else if (n == 0)
										LoadResponse(31);
									else if (n == -1)
										LoadResponse(12);
								} else {
									LoadResponse(29);
									AIFightThreatToPlayer(dialogShip);
								}
							} else
								LoadResponse(19);
						} else if (shipData->fuel < 100) {
							if (commShipXeno)
								LoadResponse(19);
							else if (dialogShip->aiType <= kAIInterceptor) {
								if (gCommMood < (0.8))
									LoadResponse(23);
								else if (gCommMood > (1.2))
									LoadResponse(24);
								else
									LoadResponse(28);
								CommDialogUpdate();
								n = DoShipCommPayment();
								SetPort(commDialog);
								if (n == 1) {
									LoadResponse(29);
									AIMakeShipRefuelPlayer(dialogShip);
								} else if (n == 0)
									LoadResponse(31);
								else if (n == -1)
									LoadResponse(12);
							} else
								LoadResponse(25);
						} else
							LoadResponse(14);
					} else
						LoadResponse(19);
				} else
					LoadResponse(1);
			}
			if (itemHit == 3) {
				PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
				if (IsDisabled(dialogShip))
					LoadResponse(1);
				else if (ExtendedIsThreatToPlayer(dialogShip))
					LoadResponse(13);
				else if (AIDoesShipLikePlayer(dialogShip))
					strncpy((char *)commResponseString, (char *)adviceString, 254);
				else
					LoadResponse(13);
			}
			SetPort(commDialog);
			GetDialogItem(commDialog, 10, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(commDialog, 12, &itemType, &item, &box);
			InvalRect(&box);
			DrawCommDialogButtons(-1);
		} while (!done);
		HideCursor();
		for (i = 0; i <= 9; i++) {
			if (commPics[i] != nil) {
				HPurge((Handle)commPics[i]);
				ReleaseResource((Handle)commPics[i]);
			}
		}
		if (commShipPic != nil) {
			HPurge((Handle)commShipPic);
			ReleaseResource((Handle)commShipPic);
		}
		PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
		if (releaseCommEscort) {
			DestroyPartialFleetCargo(dialogShip->index);
			dialogShip->parentShip = -1;
			dialogShip->aiType = gShipTypes[dialogShip->type].inherentAI;
			AIMakeShipLeave(dialogShip);
			hudRedraw.credits = true;
		}
		n = (commDialog->portRect.right - commDialog->portRect.left) / 2;
		y = (commDialog->portRect.bottom - commDialog->portRect.top) / 2;
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(commDialog);
		dialogShip = nil;
		SATSetPortScreen();
		RefreshSAT();
	}
}

// FUNCTION: EVO 0x100107D8
short DoShipCommPayment(void)
{
	short	answer;

	CommDialogUpdate();
	haggleVal = gBribeAsked;
	answer = DoHaggleDialog(false, 50);
	SetPort(commDialog);
	if (shipData->credits >= haggleVal) {
		if (answer == 1) {
			shipData->credits -= haggleVal;
			hudRedraw.credits = true;
			return 1;
		}
		gBribeAsked += kBribeStep;
		return 0;
	}
	return -1;
}

// FUNCTION: EVO 0x100108B0
pascal Boolean CommFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	char	key;
	short	button;
	Point	where;
	short	item;

	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		key = tolower(key);
		if (key == kReturnCharCode || key == kEnterCharCode || key == 'e') {
			*itemHit = 1;
			return true;
		}
		if (key == 'r') {
			*itemHit = 2;
			return true;
		}
		if (key == 'g') {
			*itemHit = 3;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		button = TrackCommDialogButtons(where);
		if (button >= 0 && button <= 3) {
			*itemHit = button + 1;
			return true;
		}
		*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(commDialog);
		CommDialogUpdate();
		EndUpdate(commDialog);
	}
	return false;
}

// FUNCTION: EVO 0x10010A2C
void CommDialogUpdate(void)
{
	Rect	box;
	Rect	textBox;
	short	itemType;
	Handle	item;
	Str255	text;

	SATSetPort(&loadPort);
	ForeColor(blackColor);
	box = commDialog->portRect;
	InsetRect(&box, 1, 1);
	PaintRect(&box);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&commDialog->portRect);
	ForeColor(blackColor);
	DrawCommDialogButtons(-1);
	GetDialogItem(commDialog, 11, &itemType, &item, &box);
	if (RectInRgn(&box, commDialog->visRgn)) {
		ForeColor(blackColor);
		DrawPicture(commShipPic, &box);
		InsetRect(&box, -5, -2);
		RGBForeColor(&rgbDarkGrey);
		FrameRect(&box);
		ForeColor(blackColor);
	}
	GetDialogItem(commDialog, 12, &itemType, &item, &box);
	if (RectInRgn(&box, commDialog->visRgn)) {
		ForeColor(blackColor);
		PaintRect(&box);
		RGBForeColor(&rgbMedGrey);
		TextFont(geneva);
		TextSize(kDialogTextSize);
		if (dialogShip->persType == kPersAmbrosiaMascot) {
			MoveTo(box.left, box.top + 12);
			DrawString("\pAlignment & Type:");
			ForeColor(whiteColor);
			MoveTo(box.left + 5, box.top + 25);
			DrawString("\pAmbrosia Mascot");
		} else if (dialogShip->govt != -1) {
			MoveTo(box.left, box.top + 12);
			DrawString("\pAlignment & Type:");
			ForeColor(whiteColor);
			MoveTo(box.left + 5, box.top + 25);
			DrawString(shipGovtStr);
			DrawString("\p ");
			DrawString(shipClassStr);
		} else {
			MoveTo(box.left, box.top + 12);
			DrawString("\pClass: ");
			ForeColor(whiteColor);
			DrawString(shipClassStr);
		}
		if (ExtendedIsThreatToPlayer(dialogShip)) {
			MoveTo(box.left, box.top + 37);
			RGBForeColor(&rgbMedGrey);
			DrawString("\pStatus: ");
			ForeColor(redColor);
			TextFace(bold);
			DrawString("\pHostile");
			TextFace(normal);
		} else if (dialogShip->parentShip == 0 && !releaseCommEscort) {
			MoveTo(box.left, box.top + 37);
			RGBForeColor(&rgbMedGrey);
			DrawString("\pStatus: ");
			RGBForeColor(&brightGreen);
			if (dialogShip->hired)
				DrawString("\pHired Escort");
			else
				DrawString("\pEscort");
		}
		ForeColor(blackColor);
	}
	GetDialogItem(commDialog, 10, &itemType, &item, &box);
	if (RectInRgn(&box, commDialog->visRgn)) {
		strncpy((char *)text, (char *)commResponseString, 254);
		p2cstr(text);
		TextFont(geneva);
		TextSize(kDialogTextSize);
		ForeColor(whiteColor);
		PaintRect(&box);
		textBox = box;
		InsetRect(&textBox, 4, 2);
		ForeColor(blackColor);
		TETextBox(text, strlen((char *)text), &textBox, teJustLeft);
		InvertRect(&box);
		RGBForeColor(&rgbDarkGrey);
		FrameRect(&box);
		ForeColor(blackColor);
	}
	SATSetPortScreen();
	SetPort(commDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &commDialog->portBits,
			&commDialog->portRect, &commDialog->portRect, srcCopy, commDialog->visRgn);
}

// FUNCTION: EVO 0x10010F70
void DoPlanetCommDialog(short stellar)
{
	Rect			box;
	short			itemHit;
	short			itemType;
	short			y;
	short			answer;
	Handle			item;
	long			dead;
	char			text[256];
	Str255			pname;
	short			k;
	PicHandle		pic;
	ModalFilterUPP	filterUPP;
	Boolean			done;
	short			n;
	short			i;

	done = false;
	filterUPP = NewModalFilterProc(PlanetCommFilter);
	if (shipData->targetStellar == -1)
		return;
	if (stellarData[shipData->targetStellar].flags & kSpobUninhabited) {
		PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
		return;
	}
	responseRandom = Rand(5);
	adviceRandom = adviceDisasterRandom = -1;
	dialogShip = nil;
	n = shipData->credits * (0.000001);
	if (n <= 0)
		n = 1;
	gBribeAsked = Rand(n) * kBribeStep + 3000;
	if (stellarData[shipData->targetStellar].govt != -1
			&& (govtData[stellarData[shipData->targetStellar].govt].flags & kGovtGreedy))
		gBribeAsked *= 1.5;
	if (gBribeAsked > shipData->credits * (0.333))
		gBribeAsked = shipData->credits * (0.333);
	gBribeAsked /= kBribeStep;
	gBribeAsked *= kBribeStep;
	if (gBribeAsked > kMaxTribute)
		gBribeAsked = kMaxTribute;
	if (gBribeAsked < kBribeStep)
		gBribeAsked = kBribeStep;
	hasDemandedTribute = false;
	commPics[0] = GetPicture(kEndCommunicationButtonPict);
	commPics[1] = GetPicture(kEndCommunicationButtonPict + 1);
	commPics[2] = GetPicture(kGreetingsButtonPict);
	commPics[3] = GetPicture(kGreetingsButtonPict + 1);
	commPics[4] = GetPicture(kOfferBribeButtonPict);
	commPics[5] = GetPicture(kOfferBribeButtonPict + 1);
	commPics[6] = GetPicture(kDemandTributeButtonPict);
	commPics[7] = GetPicture(kDemandTributeButtonPict + 1);
	commPics[8] = GetPicture(kReleaseFromServitudeButtonPict);
	commPics[9] = GetPicture(kReleaseFromServitudeButtonPict + 1);
	if (!LoadPluginString(stellarData[shipData->targetStellar].type + kPlugGovtName, gCommGovtName))
		GetIndString(gCommGovtName, kGovtNameStrList, stellarData[shipData->targetStellar].type + 1);
	PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
	ShowCursor();
	commDialog = nil;
	commDialog = GetNewDialog(kPlanetCommDlogID, nil, (WindowPtr)-1);
	if (commDialog == nil)
		return;
	if (legalStatus[shipData->curSystem] >= stellarData[shipData->targetStellar].minStatus)
		hatesPlayer = false;
	else
		hatesPlayer = true;
	willTakeBribe = false;
	if (bribeRandom < 0)
		bribeRandom = Rand(100);
	if (bribeRandom > 30) {
		if (stellarData[shipData->targetStellar].govt == -1)
			willTakeBribe = true;
		else if (govtData[stellarData[shipData->targetStellar].govt].flags & kGovtPlanetBribes)
			willTakeBribe = true;
	}
	if (stellarData[shipData->targetStellar].govt != -1
			&& (govtData[stellarData[shipData->targetStellar].govt].flags & kGovtGreedy))
		willTakeBribe = true;
	if (stellarData[shipData->targetStellar].flags & kSpobUninhabited)
		LoadResponse(1);
	else if (!hatesPlayer || stellarData[shipData->targetStellar].dominated) {
		LoadStellarResponse(0);
		strncpy(text, (char *)commResponseString, 254);
		p2cstr((StringPtr)text);
		strcpy((char *)pname, (char *)stellarData[shipData->targetStellar].name);
		p2cstr(pname);
		strcat(text, (char *)pname);
		strcat(text, ".");
		c2pstr(text);
		strncpy((char *)commResponseString, text, 254);
	} else
		LoadResponse(2);
	pic = GetPicture(kPlanetCommPictID);
	if (pic != nil) {
		box = (**pic).picFrame;
		OffsetRect(&box, -box.left, -box.top);
		SATSetPortBackScreen();
		ForeColor(blackColor);
		DrawPicture(pic, &box);
		SATSetPortScreen();
		ReleaseResource((Handle)pic);
	}
	NewDialogHook(commDialog, 0);
	CenterWindow(commDialog);
	ShowWindow(commDialog);
	SelectWindow(commDialog);
	SetPort(commDialog);
	FlushEvents(everyEvent - diskMask, 0);
	InvalRect(&commDialog->portRect);
	do {
		ModalDialog(filterUPP, &itemHit);
		if (itemHit == kOkItem)
			done = true;
		if (itemHit == 2 || itemHit == 3) {
			SetPort(commDialog);
			DrawPlanetCommDialogButtons(-1);
			LoadResponse(1);
			PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
			if (itemHit == 2) {
				if (hatesPlayer && !stellarData[shipData->targetStellar].dominated) {
					if (willTakeBribe) {
						LoadStellarResponse(8);
						PlanetCommDialogUpdate();
						haggleVal = gBribeAsked;
						answer = DoHaggleDialog(false, 50);
						PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
						if (shipData->credits >= haggleVal) {
							if (answer == 1) {
								dockPermission = kBribeLandPermission;
								dockTarget = shipData->targetStellar;
								shipData->aiTimer = 0;
								strcpy(text, (char *)shipName);
								p2cstr((StringPtr)text);
								strcat(text, ", you are ");
								if (stellarData[shipData->targetStellar].flags & kSpobStation)
									strcat(text, "cleared to dock. Commence final approach.");
								else
									strcat(text, "cleared to land. Commence final approach.");
								c2pstr(text);
								DisplayComm((StringPtr)text, 250, 0, kCommTextSize, &rgbWhite, false, false);
								shipData->credits -= haggleVal;
								hudRedraw.credits = true;
								willTakeBribe = false;
								done = true;
							} else {
								willTakeBribe = false;
								bribeRandom = 0;
								gBribeAsked += kBribeStep;
								LoadStellarResponse(6);
								PlanetCommDialogUpdate();
							}
						} else {
							LoadStellarResponse(4);
							PlanetCommDialogUpdate();
						}
					} else {
						LoadStellarResponse(9);
						PlanetCommDialogUpdate();
					}
				} else {
					if (!hatesPlayer)
						LoadResponse(9);
					else
						LoadStellarResponse(4);
					InvalRect(&commDialog->portRect);
				}
			}
			if (itemHit == 3) {
				if (stellarData[shipData->targetStellar].dominated) {
					if (stellarData[shipData->targetStellar].flags & kSpobStation)
						GetIndString(commResponseString, kPlanetResponseStrList, 37);
					else
						GetIndString(commResponseString, kPlanetResponseStrList, 36);
					dockPermission = 0;
					dockTarget = -1;
					stellarData[shipData->targetStellar].dominated = false;
					if (stellarData[shipData->targetStellar].defCount > 1000) {
						if (stellarData[shipData->targetStellar].defCount > 10000)
							stellarData[shipData->targetStellar].tribute = stellarData[shipData->targetStellar].defCount / 10 - 1000;
						else
							stellarData[shipData->targetStellar].tribute = stellarData[shipData->targetStellar].defCount / 10 - 100;
					} else
						stellarData[shipData->targetStellar].tribute = stellarData[shipData->targetStellar].defCount;
					if (legalStatus[shipData->curSystem] >= stellarData[shipData->targetStellar].minStatus)
						legalStatus[shipData->curSystem] = stellarData[shipData->targetStellar].minStatus - 1;
					InvalRect(&commDialog->portRect);
				} else {
					hatesPlayer = true;
					if (legalStatus[shipData->curSystem] >= stellarData[shipData->targetStellar].minStatus)
						legalStatus[shipData->curSystem] = stellarData[shipData->targetStellar].minStatus - 1;
					SlapWithPenalty(shipData->curSystem, stellarData[shipData->targetStellar].govt, 3, -1);
					if (playerKills < 12800) {
						SetPort(commDialog);
						LoadStellarResponse(1);
						InvalRect(&commDialog->portRect);
					} else {
						for (i = 0; i <= 4; i++)
							SlapWithPenalty(shipData->curSystem, stellarData[shipData->targetStellar].govt, 3, -1);
						if (hasDemandedTribute || stellarData[shipData->targetStellar].tribute > 0
								|| StellarHasDefenders(shipData->targetStellar)) {
							SetPort(commDialog);
							LoadStellarResponse(2);
							InvalRect(&commDialog->portRect);
							if (!StellarHasDefenders(shipData->targetStellar)) {
								if (stellarData[shipData->targetStellar].defCount <= 1000) {
									n = stellarData[shipData->targetStellar].tribute;
									stellarData[shipData->targetStellar].tribute = 0;
								} else {
									stellarData[shipData->targetStellar].tribute -= stellarData[shipData->targetStellar].defCount % 10;
									n = stellarData[shipData->targetStellar].defCount % 10;
									if (stellarData[shipData->targetStellar].tribute < 0) {
										n += stellarData[shipData->targetStellar].tribute;
										stellarData[shipData->targetStellar].tribute = 0;
									}
								}
								for (i = 0; i < n; i++)
									SpawnStellarDefenseShip(shipData->targetStellar);
							}
						} else {
							if (stellarData[shipData->targetStellar].flags & kSpobStation)
								GetIndString(commResponseString, kPlanetResponseStrList, 27);
							else
								GetIndString(commResponseString, kPlanetResponseStrList, 26);
							dockPermission = 0;
							dockTarget = -1;
							personData[kPersBountyHunter].loaded = true;
							stellarData[shipData->targetStellar].dominated = true;
							stellarData[shipData->targetStellar].daysDominated = 0;
							SetPort(commDialog);
							InvalRect(&commDialog->portRect);
						}
					}
					hasDemandedTribute = true;
				}
			}
			SetPort(commDialog);
			GetDialogItem(commDialog, 4, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(commDialog, 6, &itemType, &item, &box);
			InvalRect(&box);
		}
	} while (!done);
	HideCursor();
	for (i = 0; i <= 9; i++) {
		if (commPics[i] != nil) {
			HPurge((Handle)commPics[i]);
			ReleaseResource((Handle)commPics[i]);
		}
	}
	PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
	n = (commDialog->portRect.right - commDialog->portRect.left) / 2;
	y = (commDialog->portRect.bottom - commDialog->portRect.top) / 2;
	DisposeRoutineDescriptor(filterUPP);
	DisposeDialog(commDialog);
	SATSetPortBackScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.bounds);
	SATSetPortScreen();
	RefreshSAT();
	RedrawStatus();
	RedrawComm(false);
}

// FUNCTION: EVO 0x100120CC
pascal Boolean PlanetCommFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	char	key;
	short	button;
	Point	where;
	short	item;

	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		key = tolower(key);
		if (key == kReturnCharCode || key == kEnterCharCode || key == 'e') {
			*itemHit = 1;
			return true;
		}
		if ((key == 'o' || key == 'b') && hatesPlayer) {
			*itemHit = 2;
			return true;
		}
		if (key == 'g' && !hatesPlayer) {
			*itemHit = 2;
			return true;
		}
		if ((key == 'd' || key == 't') && !stellarData[shipData->targetStellar].dominated) {
			*itemHit = 3;
			return true;
		}
		if (key == 'r' && stellarData[shipData->targetStellar].dominated) {
			*itemHit = 3;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		button = TrackPlanetCommDialogButtons(where);
		if (button >= 0 && button <= 2) {
			*itemHit = button + 1;
			return true;
		}
		*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(commDialog);
		PlanetCommDialogUpdate();
		EndUpdate(commDialog);
	}
	return false;
}

// FUNCTION: EVO 0x10012308
void PlanetCommDialogUpdate(void)
{
	Rect		box;
	Rect		srcRect;
	Rect		textBox;
	short		itemType;
	Handle		item;
	Point		pt;
	Str255		text;
	RGBColor	gold;

	gold.red = 0xFFFF;
	gold.green = 0x6666;
	gold.blue = 0;
	SATSetPort(&loadPort);
	ForeColor(blackColor);
	box = commDialog->portRect;
	InsetRect(&box, 1, 1);
	PaintRect(&box);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&commDialog->portRect);
	ForeColor(blackColor);
	DrawPlanetCommDialogButtons(-1);
	GetDialogItem(commDialog, 4, &itemType, &item, &box);
	if (RectInRgn(&box, commDialog->visRgn)) {
		strncpy((char *)text, (char *)commResponseString, 254);
		p2cstr(text);
		TextFont(geneva);
		TextSize(kDialogTextSize);
		ForeColor(whiteColor);
		PaintRect(&box);
		textBox = box;
		InsetRect(&textBox, 4, 2);
		ForeColor(blackColor);
		TETextBox(text, strlen((char *)text), &textBox, teJustLeft);
		InvertRect(&box);
		RGBForeColor(&rgbDarkGrey);
		FrameRect(&box);
		ForeColor(blackColor);
	}
	GetDialogItem(commDialog, 5, &itemType, &item, &box);
	if (RectInRgn(&box, commDialog->visRgn)) {
		pt.h = (box.left + box.right) / 2 - GetSpriteXSize(spobFaces[stellarData[shipData->targetStellar].type]) / 2;
		pt.v = (box.top + box.bottom) / 2 - GetSpriteYSize(spobFaces[stellarData[shipData->targetStellar].type]) / 2;
		ForeColor(blackColor);
		srcRect = box;
		OffsetRect(&srcRect, -srcRect.left, -srcRect.top);
		CopyBits(&gSAT.backScreen.port->portBits, &loadPort.port->portBits, &srcRect, &box, srcCopy, nil);
		SATPlotFace(spobFaces[stellarData[shipData->targetStellar].type], &loadPort, pt, false);
		RGBForeColor(&rgbDarkGrey);
		InsetRect(&box, -5, -5);
		FrameRect(&box);
		ForeColor(blackColor);
	}
	GetDialogItem(commDialog, 6, &itemType, &item, &box);
	if (RectInRgn(&box, commDialog->visRgn)) {
		TextFont(geneva);
		TextSize(kDialogTextSize);
		MoveTo(box.left, box.top + 12);
		ForeColor(whiteColor);
		DrawString(stellarData[shipData->targetStellar].name);
		MoveTo(box.left, box.top + 26);
		RGBForeColor(&rgbMedGrey);
		DrawString(gCommGovtName);
		if (stellarData[shipData->targetStellar].flags & kSpobUninhabited) {
			MoveTo(box.left, box.top + 42);
			RGBForeColor(&rgbMedGrey);
			DrawString("\pStatus: ");
			RGBForeColor(&rgbLightGrey);
			DrawString("\pUninhabited");
		} else if (stellarData[shipData->targetStellar].dominated) {
			MoveTo(box.left, box.top + 42);
			RGBForeColor(&rgbMedGrey);
			DrawString("\pStatus: ");
			RGBForeColor(&brightGreen);
			DrawString("\pDominated");
		} else if (hatesPlayer) {
			MoveTo(box.left, box.top + 42);
			RGBForeColor(&rgbMedGrey);
			DrawString("\pStatus: ");
			TextFace(bold);
			if (legalStatus[shipData->curSystem] >= 0) {
				RGBForeColor(&gold);
				DrawString("\pForbidden");
			} else {
				ForeColor(redColor);
				DrawString("\pHostile");
			}
			TextFace(normal);
		}
	}
	SATSetPortScreen();
	SetPort(commDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &commDialog->portBits,
			&commDialog->portRect, &commDialog->portRect, srcCopy, commDialog->visRgn);
}

// FUNCTION: EVO 0x1001291C
void LoadAdvice(void)
{
	short	count;
	Str255	text;
	short	which;
	short	n;
	Boolean	ok;
	short	i;
	short	cargo;
	short	mode;
	Handle	h;

	if (dialogShip->aiType <= kAIBraveTrader) {
		if (!LoadPluginString(kPlugTraderAdvice + dialogShip->govt * 10 + responseRandom, adviceString))
			GetIndString(adviceString, dialogShip->govt + kGovtAdviceStrList, responseRandom + 1);
	} else {
		if (!LoadPluginString(kPlugWarshipAdvice + dialogShip->govt * 10 + responseRandom, adviceString))
			GetIndString(adviceString, dialogShip->govt + kGovtAdviceStrList, responseRandom + 6);
	}
	if (adviceString[0] == '*' || adviceString[1] == '*' || strlen((char *)adviceString) <= 2)
		GetIndString(adviceString, kResponseStrList, responseRandom + 46);
	which = -1;
	if (dialogShip->dude != -1
			&& dudeData[dialogShip->dude].adviceFlags >= kAdviceBand
			&& dudeData[dialogShip->dude].adviceFlags < 7128) {
		n = dudeData[dialogShip->dude].adviceFlags / kAdviceBand;
		do {
			ok = false;
			which = Rand(3);
			if (which == 0 && (n == 1 || n == 3 || n == 5 || n == 7))
				ok = true;
			if (which == 1 && (n == 2 || n == 3 || n == 6 || n == 7))
				ok = true;
			if (which == 2 && (n == 4 || n == 6 || n == 7))
				ok = true;
		} while (!ok);
	}
	if (dialogShip->persType != -1
			&& (personData[dialogShip->persType].flags & kPersDisasterInfo)
			&& personData[dialogShip->persType].commQuote == -1) {
		n = 2;
		which = 1;
	}
	if (which == 0) {
		n = 0;
		for (i = 0; i < MaxStellarObjects; i++)
			if (stellarData[i].loaded && (stellarData[i].flags & (kSpobCanLand | kSpobTradeCenter)))
				n++;
		i = -1;
		do {	/* BUG: n is never tested; with no landable stellar this never ends */
			cargo = Rand(NumCommodities);
			i = Rand(MaxStellarObjects);
			if (stellarData[i].loaded && (stellarData[i].flags & (kSpobCanLand | kSpobTradeCenter))) {
				mode = CalcPortDemand(cargo, stellarData[i].flags);
				if (mode != 1 && mode != 4)
					i = -1;
			} else
				i = -1;
		} while (i == -1);
		stellarData[i].loaded && (stellarData[i].flags & (kSpobCanLand | kSpobTradeCenter));
	}
	if (which == 1) {
		n = 0;
		for (i = 0; i < MaxDisasters; i++)
			if (disasterData[i].countdown > 1)
				n++;
		i = -1;
		if (n == 1) {
			for (i = 0; i < MaxDisasters; i++)
				if (disasterData[i].countdown > 1)
					break;
		} else if (n > 1) {
			do
				i = Rand(MaxDisasters);
			while (disasterData[i].countdown <= 1);
		}
		if (i != -1 && disasterData[i].stellarID >= 0 && disasterData[i].stellarID < MaxStellarObjects) {
			strcpy((char *)adviceString, "The last time I was on ");
			strncpy((char *)text, (char *)stellarData[disasterData[i].stellarID].name, 30);
			p2cstr(text);
			strcat((char *)adviceString, (char *)text);
			strcat((char *)adviceString, ", the price of ");
			strcpy((char *)text, lcCargoName[disasterData[i].commodity]);
			p2cstr(text);
			strcat((char *)adviceString, (char *)text);
			n = Rand(3);
			if (n == 0)
				strcat((char *)adviceString, " was really ");
			else if (n == 1)
				strcat((char *)adviceString, " was pretty ");
			else
				strcat((char *)adviceString, " was very ");
			if (disasterData[i].amount < 0)
				strcat((char *)adviceString, "low.");
			else
				strcat((char *)adviceString, "high.");
			c2pstr((char *)adviceString);
		}
	}
	if (which == 2) {
		h = GetResource('STR#', dudeData[dialogShip->dude].adviceFlags % kAdviceBand + kDudeAdviceStrList);
		if (h != nil) {
			HLock(h);
			BlockMove(*h, &count, 2);
			HUnlock(h);
			HPurge(h);
			ReleaseResource(h);
		}
		if (count > 0) {
			GetIndString(text, dudeData[dialogShip->dude].adviceFlags % kAdviceBand + kDudeAdviceStrList,
					Rand(adviceRandom % count + 1) + 1);
			strcpy((char *)adviceString, (char *)text);
		}
	}
	if (dialogShip->persType != -1 && personData[dialogShip->persType].commQuote >= 1
			&& !LoadPluginString(personData[dialogShip->persType].commQuote + kPlugPersQuote, adviceString))
		GetIndString(adviceString, kPersQuoteStrList, personData[dialogShip->persType].commQuote);
}

// FUNCTION: EVO 0x1001312C
short DoHaggleDialog(Boolean raise, short chance)
{
	short			x;
	short			y;
	Rect			r;
	short			itemHit;
	short			itemType;
	Rect			box;
	Boolean			done;
	ModalFilterUPP	filterUPP;
	short			i;
	short			result;
	Boolean			canHaggle;
	long			conv;
	Boolean			haggled;

	done = false;
	filterUPP = NewModalFilterProc(HaggleFilter);
	for (i = 0; i <= 3; i++) {
		if (raise)
			hagglePics[i] = GetPicture(i + kAcceptPaymentButtonPict);
		else
			hagglePics[i] = GetPicture(i + kAcceptPriceButtonPict);
	}
	if (raise)
		haggleMode = kHaggleRaise;
	else
		haggleMode = kHaggleLower;
	result = 0;
	if (Rand(100) <= chance)
		canHaggle = true;
	else
		canHaggle = false;
	haggleDialog = nil;
	haggleDialog = GetNewDialog(kHaggleDlogID, nil, (WindowPtr)-1);
	if (haggleDialog != nil) {
		NewDialogHook(haggleDialog, 0);
		CenterWindow(haggleDialog);
		ShowWindow(haggleDialog);
		SelectWindow(haggleDialog);
		SetPort(haggleDialog);
		FlushEvents(everyEvent - diskMask, 0);
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kOkItem) {
				DrawHaggleButtons(-1);
				if (result != 2)
					result = 1;
				done = true;
			}
			if (itemHit == 2) {
				if (canHaggle) {
					if (haggleMode == kHaggleRaise)
						haggleVal *= (1.33);
					if (haggleMode == kHaggleLower)
						haggleVal *= 0.75;
					conv = haggleVal;
					conv *= (0.01);
					conv *= 100;
					haggleVal = conv;
					HaggleDialogUpdate();
					canHaggle = false;
					haggled = true;
					result = 1;
				} else {
					result = 2;
					done = true;
				}
				DrawHaggleButtons(-1);
			}
		} while (!done);
		for (i = 0; i <= 3; i++) {
			if (hagglePics[i] != nil) {
				HPurge((Handle)hagglePics[i]);
				ReleaseResource((Handle)hagglePics[i]);
			}
		}
		DisposeRoutineDescriptor(filterUPP);
		x = (haggleDialog->portRect.right - haggleDialog->portRect.left) / 2;
		y = (haggleDialog->portRect.bottom - haggleDialog->portRect.top) / 2;
		r.left = r.right = (gSAT.wind.bounds.left + gSAT.wind.bounds.right) / 2;
		r.top = r.bottom = (gSAT.wind.bounds.top + gSAT.wind.bounds.bottom) / 2;
		DisposeDialog(haggleDialog);
		RefreshSAT();
		return result;
	}
}	/* BUG: no return value when GetNewDialog fails */

// FUNCTION: EVO 0x100134DC
pascal Boolean HaggleFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	char	key;
	short	button;
	Point	where;
	Rect	box;
	Handle	itemHandle;
	short	itemType;

	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		if (key == kReturnCharCode || key == kEnterCharCode) {
			*itemHit = 1;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		button = TrackHaggleButtons(where);
		if (button == 0) {
			*itemHit = 1;
			return true;
		}
		if (button == 1) {
			*itemHit = 2;
			return true;
		}
		*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(haggleDialog);
		HaggleDialogUpdate();
		EndUpdate(haggleDialog);
	}
	return false;
}

// FUNCTION: EVO 0x10013614
void HaggleDialogUpdate(void)
{
	Rect	box;
	short	itemType;
	Handle	item;
	Str255	text;

	SATSetPort(&loadPort);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	ForeColor(blackColor);
	PaintRect(&haggleDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&haggleDialog->portRect);
	ForeColor(blackColor);
	RGBForeColor(&rgbDarkGrey);
	GetDialogItem(haggleDialog, 3, &itemType, &item, &box);
	FrameRect(&box);
	ForeColor(whiteColor);
	MoveTo(box.left + 4, box.top + 12);
	if (haggleMode == kHaggleMission)
		DrawString("\pThe pay for having completed this mission is ");
	if (haggleMode == kHaggleRaise)
		DrawString("\pI\325ll pay you ");
	if (haggleMode == kHaggleLower) {
		if (shipData->targetShip != -1)
			DrawString("\pPay me ");
		else
			DrawString("\pPay us ");
	}
	DrawCommaNumber(haggleVal);
	DrawString("\p credits.");
	ForeColor(blackColor);
	DrawHaggleButtons(-1);
	SATSetPortScreen();
	SetPort(haggleDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &haggleDialog->portBits,
			&haggleDialog->portRect, &haggleDialog->portRect, srcCopy, haggleDialog->visRgn);
}

// FUNCTION: EVO 0x10013800
void LoadResponse(short which)
{
	if (which <= 37)
		GetIndString(commResponseString, kResponseStrList, responseRandom + which * 5 + 1);
	else
		GetIndString(commResponseString, kResponseStrList + 1, responseRandom + which * 5 - 189);
}

// FUNCTION: EVO 0x10013880
void LoadStellarResponse(short which)
{
	GetIndString(commResponseString, kPlanetResponseStrList, responseRandom + which * 5 + 1);
}

// FUNCTION: EVO 0x100138C8
void DoPlunderDialog(void)
{
	Rect			dlgRect;
	short			itemHit;
	short			itemType;
	Handle			item;
	Rect			box;
	Rect			r;
	char			text[256];
	Str255			tmp;
	Rect			frame;
	short			k;
	Boolean			keepShip;
	Boolean			named;
	ModalFilterUPP	filterUPP;
	Boolean			done;
	Boolean			risky;
	short			risk;
	short			i;
	short			j;
	short			found;
	short			qty;

	risky = false;
	done = false;
	filterUPP = NewModalFilterProc(PlunderFilter);
	risk = Rand(26) + 15;
	SetPlunderValues();
	ShowCursor();
	plunderDialog = nil;
	plunderDialog = GetNewDialog(kPlunderDlogID, nil, (WindowPtr)-1);
	if (plunderDialog == nil)
		return;
	for (i = 0; i <= 7; i++)
		plunderPics[i] = GetPicture(i + kAbortButtonPict);
	for (i = 8; i <= 11; i++)
		plunderPics[i] = GetPicture(i + (kFuelButtonPict - 8));
	CenterWindow(plunderDialog);
	ShowWindow(plunderDialog);
	SelectWindow(plunderDialog);
	SetPort(plunderDialog);
	FlushEvents(everyEvent - diskMask, 0);
	do {
		ModalDialog(filterUPP, &itemHit);
		if (itemHit == kPlunderLeaveItem) {
			risk = -1;
			risky = false;
			DrawPlunderButtons(-1);
			PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
			done = true;
		}
		if (risky && Rand(100) <= risk) {
			shipData[shipData->targetShip].shield = -(ShipArmorCapacity(&shipData[shipData->targetShip]) + 1);
			shipData[shipData->targetShip].deathTimer = 0;
			PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
			DisplayComm("\pOops! You tripped this ship\325s security self-destruct mechanism.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
			SATRun(false);
			SetPort(plunderDialog);
			done = true;
			break;
		}
		risky = false;
		if (itemHit == kPlunderCargoItem) {
			DrawPlunderButtons(-1);
			if (salvageType != -1) {
				if (salvageQty > TotalFleetHolds() - TotalCargo(shipData))
					salvageQty = TotalFleetHolds() - TotalCargo(shipData);
				if (salvageQty <= 0) {
					PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
					DisplayComm("\pYou couldn\325t store any of the cargo you salvaged from this ship.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
					RefreshSAT();
					TwoStepRepaintGameWindow();
				} else {
					strcpy(text, "You salvaged ");
					NumToString(salvageQty, tmp);
					p2cstr(tmp);
					strcat(text, (char *)tmp);
					strcat(text, " tons of ");
					strcpy((char *)tmp, lcCargoName[salvageType]);
					p2cstr(tmp);
					strcat(text, (char *)tmp);
					strcat(text, " from this ship.");
					c2pstr(text);
					PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
					DisplayComm((StringPtr)text, kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
					RefreshSAT();
					TwoStepRepaintGameWindow();
					shipData->cargo[salvageType] += salvageQty;
					hudRedraw.credits = true;
					salvageQty = 0;
					salvageType = -1;
					SetPort(plunderDialog);
					InvalRect(&plunderDialog->portRect);
				}
				salvageType = -1;
				risk *= 2.0;
				risky = true;
			} else
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
		}
		if (itemHit == kPlunderCreditsItem) {
			DrawPlunderButtons(-1);
			if (salvageCredits > 0) {
				PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
				DisplayComm("\pYou stole all the credits from this ship.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
				RefreshSAT();
				TwoStepRepaintGameWindow();
				shipData->credits += salvageCredits;
				risk *= 1.25;
				risky = true;
				hudRedraw.credits = true;
				DrawStatus();
				salvageCredits = 0;
				SetPort(plunderDialog);
				InvalRect(&plunderDialog->portRect);
			} else
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
		}
		if (itemHit == kPlunderAmmoItem) {
			DrawPlunderButtons(-1);
			ShipStatsToSystemInfo();
			found = -1;
			for (j = 0; j < MaxOutfitItemTypes; j++) {
				if (gOutfits[j].modType[0] == kOutfModAmmo && salvageAmmoType == gOutfits[j].modVal[0]) {
					found = j;
					break;
				}
			}
			if (found != -1 && salvageAmmoType != -1) {
				qty = 0;
				for (;;) {
					if (ShipFreeMass() < gOutfits[found].mass || qty >= salvageAmmoQty)
						break;
					shipData->weap[salvageAmmoType].ammo++;
					qty++;
					ShipStatsToSystemInfo();
				}
				if (qty > 0) {
					strcpy(text, "You salvaged ");
					if (qty == 1)
						strcpy((char *)tmp, "one");
					else if (qty == 2)
						strcpy((char *)tmp, "two");
					else if (qty == 3)
						strcpy((char *)tmp, "three");
					else if (qty == 4)
						strcpy((char *)tmp, "four");
					else if (qty == 5)
						strcpy((char *)tmp, "five");
					else if (qty == 6)
						strcpy((char *)tmp, "six");
					else if (qty == 7)
						strcpy((char *)tmp, "seven");
					else if (qty == 8)
						strcpy((char *)tmp, "eight");
					else if (qty == 9)
						strcpy((char *)tmp, "nine");
					else if (qty == 10)
						strcpy((char *)tmp, "ten");
					else {
						NumToString(qty, tmp);
						p2cstr(tmp);
					}
					strcat(text, (char *)tmp);
					strcat(text, " ");
					if (qty > 1)
						strcpy((char *)tmp, (char *)lcOutfitPluralName[found]);
					else
						strcpy((char *)tmp, (char *)lcOutfitName[found]);
					p2cstr(tmp);
					strcat(text, (char *)tmp);
					strcat(text, " from this ship.");
					c2pstr(text);
					PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
					DisplayComm((StringPtr)text, kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
					RefreshSAT();
					TwoStepRepaintGameWindow();
				} else {
					PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
					DisplayComm("\pYou couldn\325t store any of the ammo you salvaged from this ship.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
					RefreshSAT();
					TwoStepRepaintGameWindow();
				}
				hudRedraw.ammo = true;
				risk *= 2.0;
				risky = true;
				salvageAmmoQty = 0;
				salvageAmmoType = -1;
				SetPort(plunderDialog);
				InvalRect(&plunderDialog->portRect);
			} else
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
		}
		if (itemHit == kPlunderFuelItem) {
			DrawPlunderButtons(-1);
			if (salvageFuelQty > 0) {
				PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
				qty = ShipFuelCapacity(shipData) - shipData->fuel;
				if (qty > salvageFuelQty)
					qty = salvageFuelQty;
				if (qty > 0) {
					shipData->fuel += qty;
					if (shipData->fuel >= ShipFuelCapacity(shipData))
						DisplayComm("\pYou filled your tanks with fuel from this ship.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
					else
						DisplayComm("\pYou transferred all of this ship\325s fuel to your tanks.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
					RefreshSAT();
					TwoStepRepaintGameWindow();
				} else {
					DisplayComm("\pYou couldn\325t store any of the fuel you transferred from this ship.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
					RefreshSAT();
					TwoStepRepaintGameWindow();
				}
				hudRedraw.fuel = true;
				DrawStatus();
				risk *= 1.5;
				risky = true;
				salvageFuelQty = 0;
				SetPort(plunderDialog);
				InvalRect(&plunderDialog->portRect);
			} else
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
		}
		if (itemHit == kPlunderCaptureItem) {
			DrawPlunderButtons(-1);
			if (shipData[shipData->targetShip].govt != -1
					&& (govtData[shipData[shipData->targetShip].govt].flags & kGovtStartDisabled))
				captureOdds = -1;
			if (Rand(100) <= captureOdds && captureOdds > 0) {
				if (Rand(10) == 0) {
					done = true;
					shipData[shipData->targetShip].shield = kDeadMark;
					shipData[shipData->targetShip].deathTimer = 0;
					DisplayComm("\p Oops! You tripped this ship\325s security self-destruct mechanism.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
					RefreshSAT();
					TwoStepRepaintGameWindow();
					SetPort(plunderDialog);
				} else {
					PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
					if (CanHireEscorts()) {
						keepShip = DoCapturedShipAssignmentDialog();
						if (keepShip) {
							done = true;
							strcpy(text, (char *)gShipTypes[shipData[shipData->targetShip].type].name);
							named = EVTextInputDialog("\pNow rename this captured ship:", (StringPtr)text, 20);
							SetPort(plunderDialog);
							if (named) {
								strcpy((char *)shipName, (char *)gTextInputResult);
								SetPort(plunderDialog);
								DoShipCapture(&shipData[shipData->targetShip], false);
								SetPort(plunderDialog);
							} else {
								DisplayComm("\pYou decided not to capture this ship after all.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
								SetPort(plunderDialog);
							}
						} else {
							done = true;
							shipData[shipData->targetShip].aiType = kAIEscort;
							shipData[shipData->targetShip].parentShip = 0;
							shipData[shipData->targetShip].hired = false;
							shipData[shipData->targetShip].shield = 0;
							shipData[shipData->targetShip].govt = -1;
							shipData[shipData->targetShip].persType = -1;
							shipData[shipData->targetShip].targetShip = -1;
							shipData[shipData->targetShip].boarded = true;
							shipData[shipData->targetShip].targetShip = -1;
							Accel(Rand(kFullCircle), 75.0, &shipData[shipData->targetShip].pos);
							AIMakeEscortFlyInForm(&shipData[shipData->targetShip]);
							DisplayComm("\pYou assigned this ship to your fleet of escorts.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
							RefreshSAT();
							TwoStepRepaintGameWindow();
							SetPort(plunderDialog);
							hudRedraw.credits = true;
						}
					} else {
						PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
						DisplayComm("\pYou already have your maximum number of escorts.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
						RefreshSAT();
						TwoStepRepaintGameWindow();
						SetPort(plunderDialog);
					}
				}
			} else {
				done = true;
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
				DisplayComm("\pYour attempt to capture this ship was unsuccessful.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
				RefreshSAT();
				TwoStepRepaintGameWindow();
				SetPort(plunderDialog);
			}
		}
		if (!done) {
			PlunderDialogUpdate();
			HandleComm();
			SetPort(plunderDialog);
		}
	} while (!done);
	for (i = 0; i <= 11; i++) {
		if (plunderPics[i] != nil) {
			HUnlock((Handle)plunderPics[i]);
			HPurge((Handle)plunderPics[i]);
			ReleaseResource((Handle)plunderPics[i]);
		}
	}
	SATSetPortScreen();
	DisposeRoutineDescriptor(filterUPP);
	DisposeDialog(plunderDialog);
	SATSetPortScreen();
	HideCursor();
	RefreshSAT();
}

// FUNCTION: EVO 0x10014AE8
void SetPlunderValues(void)
{
	long	mass;
	short	flags;
	short	n;
	short	k;
	Boolean	ok;
	short	crew;
	short	i;
	short	j;

	if (shipData[shipData->targetShip].dude != -1)
		flags = dudeData[shipData[shipData->targetShip].dude].flags;
	else
		flags = 0;
	if (flags & kBootyMoney) {
		n = gShipTypes[shipData[shipData->targetShip].type].cost / 1000;
		if (n * (0.025) > 2.0)
			salvageCredits = (Rand(n * (0.025)) + n * (0.025)) * 1000.0;
		else
			salvageCredits = n * (0.025) * 1000.0;
		if (salvageCredits < 1000)
			salvageCredits = 1000;
	} else if (shipData[shipData->targetShip].persType != -1) {
		n = personData[shipData[shipData->targetShip].persType].credits / 1000;
		if (n * 0.5 > 2.0)
			salvageCredits = (Rand(n * 0.5) + n * 0.5) * 1000.0;
		else
			salvageCredits = n * 0.5 * 1000.0;
	} else
		salvageCredits = -1;
	if (salvageCredits <= 0)
		salvageCredits = -1;
	if (flags & (0xFFFF - kBootyMoney)) {
		ok = false;
		while (!ok) {
			salvageType = Rand(NumCommodities + 1);
			ok = false;
			if (salvageType == 0 && (flags & kBootyFood))
				ok = true;
			if (salvageType == 1 && (flags & kBootyIndustrial))
				ok = true;
			if (salvageType == 2 && (flags & kBootyMedical))
				ok = true;
			if (salvageType == 3 && (flags & kBootyLuxury))
				ok = true;
			if (salvageType == 4 && (flags & kBootyMetal))
				ok = true;
			if (salvageType == 5 && (flags & kBootyEquipment))
				ok = true;
		}
		salvageQty = Rand(gShipTypes[shipData[shipData->targetShip].type].cargo / 2)
				+ gShipTypes[shipData[shipData->targetShip].type].cargo / 2;
	} else
		salvageType = -1;
	n = 0;
	for (i = 0; i < MaxWeaponTypes; i++)
		if (shipData[shipData->targetShip].weap[i].ammo > 0 && PlayerCanUseAmmoType(i))
			n++;
	if (n > 0) {
		for (;;) {
			k = Rand(MaxWeaponTypes);
			if (shipData[shipData->targetShip].weap[k].ammo > 0 && PlayerCanUseAmmoType(k)) {
				salvageAmmoType = k;
				salvageAmmoQty = shipData[shipData->targetShip].weap[k].ammo;
				break;
			}
		}
	} else
		salvageAmmoType = -1;
	salvageFuelQty = Rand(gShipTypes[shipData[shipData->targetShip].type].fuel / 10) * 10;
	crew = gShipTypes[shipData->type].crew;
	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && shipData[i].parentShip == 0 && shipData[i].aiType == kAIEscort
				&& shipData[i].specialMission == -1
				&& gShipTypes[shipData[i].type].crew >= 10
				&& gShipTypes[shipData[i].type].inherentAI >= kAIWarship)
			crew = gShipTypes[shipData[i].type].crew * (0.1) + crew;
	}
	for (i = 0; i < MaxOutfitItemTypes; i++)
		for (j = 0; j < 2; j++)
			if (gOutfits[i].modType[j] == kOutfModMarines)
				crew += gOutfits[i].modVal[j] * outfitCount[i];
	captureOdds = crew / (gShipTypes[shipData[shipData->targetShip].type].crew * 15.0) * 100.0;
	captureOdds += 10 - Rand(21);
	if (captureOdds < 1)
		captureOdds = 1;
	if (captureOdds > 75)
		captureOdds = 75;
}

// FUNCTION: EVO 0x100153A0
pascal Boolean PlunderFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	char	key;
	short	button;
	Point	where;
	Rect	box;
	Handle	itemHandle;
	short	itemType;

	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		if (key == kReturnCharCode || key == kEnterCharCode) {
			*itemHit = kPlunderLeaveItem;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		button = TrackPlunderButtons(where);
		if (button == 0) {
			*itemHit = kPlunderLeaveItem;
			return true;
		}
		if (button == 1) {
			*itemHit = kPlunderCargoItem;
			return true;
		}
		if (button == 2) {
			*itemHit = kPlunderCreditsItem;
			return true;
		}
		if (button == 3) {
			*itemHit = kPlunderAmmoItem;
			return true;
		}
		if (button == 4) {
			*itemHit = kPlunderFuelItem;
			return true;
		}
		if (button == 5) {
			*itemHit = kPlunderCaptureItem;
			return true;
		}
		*itemHit = -1;
		return true;
	}
	if (ev->what == updateEvt) {
		BeginUpdate(plunderDialog);
		PlunderDialogUpdate();
		EndUpdate(plunderDialog);
	}
	return false;
}

// FUNCTION: EVO 0x10015548
void PlunderDialogUpdate(void)
{
	Rect	box;
	short	itemType;
	short	k;
	Handle	item;
	Str255	str;
	short	i;

	SATSetPort(&loadPort);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	ForeColor(blackColor);
	PaintRect(&plunderDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&plunderDialog->portRect);
	ForeColor(blackColor);
	GetDialogItem(plunderDialog, 5, &itemType, &item, &box);
	ForeColor(blackColor);
	PaintRect(&box);
	RGBForeColor(&rgbLightGrey);
	MoveTo(box.left, box.top + 12);
	DrawString("\pSelect what to plunder from this ship:");
	RGBForeColor(&rgbLightGrey);
	MoveTo(box.left, box.top + 28);
	DrawString("\pCargo:");
	MoveTo(box.left, box.top + 42);
	DrawString("\pCredits:");
	MoveTo(box.left, box.top + 56);
	DrawString("\pAmmo:");
	MoveTo(box.left + 50, box.top + 28);
	if (salvageType != -1) {
		ForeColor(whiteColor);
		NumToString(salvageQty, str);
		DrawString(str);
		DrawString("\p ton");
		if (salvageQty > 1)
			DrawString("\ps");
		DrawString("\p of ");
		DrawString((StringPtr)lcCargoName[salvageType]);
	} else {
		RGBForeColor(&rgbMedGrey);
		DrawString("\pNone");
	}
	MoveTo(box.left + 50, box.top + 42);
	if (salvageCredits > 0) {
		ForeColor(whiteColor);
		DrawCommaNumber(salvageCredits);
	} else {
		RGBForeColor(&rgbMedGrey);
		DrawString("\pNone");
	}
	MoveTo(box.left + 50, box.top + 56);
	if (salvageAmmoType != -1) {
		ForeColor(whiteColor);
		NumToString(salvageAmmoQty, str);
		DrawString(str);
		DrawString("\p ");
		for (i = 0; i < MaxOutfitItemTypes; i++) {
			if (gOutfits[i].modType[0] == kOutfModAmmo && salvageAmmoType == gOutfits[i].modVal[0]) {
				DrawString(gOutfits[i].name);
				break;
			}
			if (gOutfits[i].modType[1] == kOutfModAmmo && salvageAmmoType == gOutfits[i].modVal[1]) {
				DrawString(gOutfits[i].name);
				break;
			}
		}
		if (salvageAmmoQty > 1)
			DrawString("\ps");
	} else {
		RGBForeColor(&rgbMedGrey);
		DrawString("\pNone");
	}
	MoveTo(box.left + 1, box.top + 70);
	RGBForeColor(&rgbLightGrey);
	DrawString("\pFuel: ");
	MoveTo(box.left + 50, box.top + 70);
	if (salvageFuelQty > 0) {
		ForeColor(whiteColor);
		DrawCommaNumber(salvageFuelQty);
	} else {
		RGBForeColor(&rgbMedGrey);
		DrawString("\pNone");
	}
	MoveTo(box.left + 120, box.top + 70);
	RGBForeColor(&rgbLightGrey);
	DrawString("\pCapture Odds: ");
	MoveTo(box.left + 195, box.top + 70);
	ForeColor(whiteColor);
	NumToString(captureOdds, str);
	DrawString(str);
	DrawString("\p%");
	ForeColor(blackColor);
	DrawPlunderButtons(-1);
	SATSetPortScreen();
	SetPort(plunderDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &plunderDialog->portBits,
			&plunderDialog->portRect, &plunderDialog->portRect, srcCopy, plunderDialog->visRgn);
}
