/* Bar.c -- the spaceport bar: bar dialog, news terminal, slot machine, bar encounters. Notes: Research/notes/Bar.md */
#define OLDROUTINENAMES 1
#include <Dialogs.h>
#include <Fonts.h>
#include <Resources.h>
#include <TextUtils.h>
#include <string.h>

#define missionAvailData globalsMissionAvailData	/* globals.h has the current record */
#include "evo.h"
#undef missionAvailData

/* BUG: the EV 1.0 record, one short shorter than MissionAvailRec; the bar walks
   missionAvailData with this stride and tests the wrong field for the priority bit */
typedef struct OldMissionAvailRec {
	short	availStel;
	short	availBitSet;
	short	availBitClr;
	short	availLoc;
	short	availRecord;
	short	availRating;
	short	availRandom;
	short	cargoQty;
	short	flags;
} OldMissionAvailRec;

extern OldMissionAvailRec missionAvailData[];


#define kMissionAvailBarPriority	0x1000

#define kSponsorStrList		8100
#define kNewsStrList		8101
#define kDisasterNewsStrList	8102
#define kDisasterNewsSTR	6000

#define kBarDlogID			1013
#define kNewsDlogID			1014
#define kSlotDlogID			1015

#define kSlotStrList		8152

#define kBarLeaveItem		1
#define kBarSlotsItem		2
#define kBarNewsItem		3
#define kBarShipsItem		5
#define kBarPersonItem		6
#define kBarDescItem		7

#define kSlotBetItemSmall		4
#define kSlotBetItemRight		5

#define kBarNewsPictID		9000
#define kSlotReelStripPictID	9001
#define kSlotBlipSndID		460
#define kSlotLockSndID		461
#define kMaxBarQueue		512
#define kReelHeight			64

extern ShipObj		*shipData;
extern OopsRecord	*disasterData;
extern SpobRecord	*stellarData;

PicHandle	barPics[12];
PicHandle	slotPics[4];
PicHandle	newsPic;
DialogPtr	barDialog;
DialogPtr	newsDialog;
DialogPtr	slotDialog;
char		barDescText[1024];
char		newsString[1024];
char		sponsorString[256];
Rect		slotOffRect;
short		reelPos[3];
short		reelSpin[3];
short		barMashCounter;
short		barQueue[kMaxBarQueue];
short		barPersonIndex;
Ptr			slotBlipSnd;
Ptr			slotLockSnd;


// FUNCTION: EVO 0x10009eac
void DoBarDialog(void)
{
	Boolean done;
	ModalFilterUPP filterUPP;
	short i;
	short count;
	Handle itemHandle;
	short priorityCount;
	short itemType;
	Rect box;
	short itemHit;
	Str255 str1;
	Str255 str2;
	Rect r2;
	Point pt;
	short k;

	done = false;
	filterUPP = NewModalFilterProc(BarFilter);
	RandomizeNewsDialog();
	inBar = 1;
	drawGate = 0;
	if (lastMissionStellar != shipData->targetStellar || lastMissionStellar < 0) {
		for (i = 0; i < kMaxBarQueue; i++)
			randomOdds[i] = Rand(100) + 1;
		RandomizeMissionData();
	}
	lastMissionStellar = shipData->targetStellar;
	for (i = 0; i < kMaxBarQueue; i++)
		barQueue[i] = -1;
	count = 0;
	for (i = 0; i < kMaxBarQueue; i++) {
		if (missionGrid[1][i] != -1 && (missionAvailData[missionGrid[1][i]].flags & kMissionAvailBarPriority)) {
			barQueue[count] = missionGrid[1][i];
			count++;
		}
	}
	priorityCount = count;
	for (i = 0; i < kMaxBarQueue; i++) {
		if (missionGrid[1][i] != -1 && !(missionAvailData[missionGrid[1][i]].flags & kMissionAvailBarPriority)) {
			barQueue[count] = missionGrid[1][i];
			count++;
		}
	}
	if (count > 0)
		barPersonIndex = 0;
	else
		barPersonIndex = -1;
	barMashCounter = Rand(100) + 80;
	strncpy(barDescText, "", 3);
	LoadDescription(barDescText, shipData->targetStellar + kBarDescBase);
	barPics[0] = GetPicture(kLeave121ButtonPict);
	barPics[1] = GetPicture(kLeave121ButtonPict + 1);
	for (i = 2; i <= 11; i++)
		barPics[i] = GetPicture(i + (kGambleButtonPict - 2));
	barDialog = NULL;
	barDialog = GetNewDialog(kBarDlogID, NULL, (WindowPtr)-1);
	if (barDialog != NULL) {
		NewDialogHook(barDialog, 0);
		CenterWindow(barDialog);
		ShowWindow(barDialog);
		SelectWindow(barDialog);
		SetPort(barDialog);
		FlushEvents(everyEvent - diskMask, 0);
		BarDialogUpdate();
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kBarLeaveItem)
				done = true;
			if (itemHit == kBarSlotsItem) {
				DrawBarButtons(-1);
				if (shipData->credits >= kSmallBet)
					RunSlotMachine();
				else
					PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
				PortDialogUpdate();
				SetPort(barDialog);
				BarDialogUpdate();
			}
			if (itemHit == kBarNewsItem) {
				DrawBarButtons(-1);
				DoNewsDialog();
				PortDialogUpdate();
				SetPort(barDialog);
				BarDialogUpdate();
			}
			if (itemHit == kBarShipsItem && CanHireEscorts()) {
				DrawBarButtons(-1);
				shipBuyMode = 1;
				DoShipyardDialog();
				ShipStatsToSystemInfo();
				DrawStatusCargo();
				PortDialogUpdate();
				SetPort(barDialog);
				BarDialogUpdate();
			}
			if (itemHit == kBarPersonItem)
				RunBarPersonEncounter();
		} while (!done);
		for (i = 0; i <= 11; i++) {
			if (barPics[i] != NULL) {
				HPurge((Handle)barPics[i]);
				ReleaseResource((Handle)barPics[i]);
			}
		}
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(barDialog);
		RefreshSAT();
	}
}

// FUNCTION: EVO 0x1000a3ac
pascal Boolean BarFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	char key;
	short button;
	Point where;
	Rect box;
	Handle itemHandle;
	short itemType;

	if (ev->what == keyDown || ev->what == autoKey) {
		barMashCounter = 0;
		key = ev->message & charCodeMask;
		if (key == kReturnCharCode || key == kEnterCharCode) {
			*itemHit = kBarLeaveItem;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		barMashCounter = 0;
		where = ev->where;
		GlobalToLocal(&where);
		button = TrackBarButtons(where);
		if (button == 0) {
			*itemHit = kBarLeaveItem;
			return true;
		} else if (button == 1) {
			*itemHit = kBarSlotsItem;
			return true;
		} else if (button == 2) {
			*itemHit = kBarNewsItem;
			return true;
		} else if (button == 4) {
			*itemHit = kBarShipsItem;
			return true;
		} else {
			*itemHit = -1;
			return true;
		}
	} else {
		barMashCounter++;
		if (Rand(150) < barMashCounter) {
			*itemHit = kBarPersonItem;
			barMashCounter = 0;
			return true;
		} else if (ev->what == updateEvt) {
			BeginUpdate(barDialog);
			BarDialogUpdate();
			EndUpdate(barDialog);
			return false;
		}
		return false;
	}
}

// FUNCTION: EVO 0x1000a560
void BarDialogUpdate(void)
{
	Rect box;
	Point pt;
	short itemType;
	Handle itemHandle;
	Str255 str;

	SATSetPort(&loadPort);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	ForeColor(blackColor);
	PaintRect(&barDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&barDialog->portRect);
	ForeColor(blackColor);
	GetDialogItem(barDialog, kBarDescItem, &itemType, &itemHandle, &box);
	ForeColor(blackColor);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	TETextBox(barDescText, strlen(barDescText), &box, teFlushDefault);
	InvertRect(&box);
	ForeColor(blackColor);
	DrawBarButtons(-1);
	SATSetPortScreen();
	SetPort(barDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &barDialog->portBits, &barDialog->portRect, &barDialog->portRect, srcCopy, barDialog->visRgn);
}

// FUNCTION: EVO 0x1000a6d0
void DoNewsDialog(void)
{
	Str255 str1;
	Str255 str2;
	Rect box;
	Rect box2;
	Rect box3;
	Point pt;

	PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
	newsPic = GetPicture(kBarNewsPictID);
	newsDialog = NULL;
	newsDialog = GetNewDialog(kNewsDlogID, NULL, (WindowPtr)-1);
	if (newsDialog == NULL)
		return;
	NewDialogHook(newsDialog, 0);
	CenterWindow(newsDialog);
	ShowWindow(newsDialog);
	SelectWindow(newsDialog);
	SetPort(newsDialog);
	FlushEvents(everyEvent - diskMask, 0);
	NewsDialogUpdate();
	while (StillDown())
		;
	while (!Button() && !KeyCheck(kKeyReturn) && !KeyCheck(kKeySpace))
		;
	HPurge((Handle)newsPic);
	ReleaseResource((Handle)newsPic);
	DisposeDialog(newsDialog);
	/* BUG: event kinds passed as masks; only the low event bits are flushed */
	FlushEvents(mouseDown, 0);
	FlushEvents(mouseUp, 0);
	FlushEvents(keyDown, 0);
	FlushEvents(keyUp, 0);
	FlushEvents(autoKey, 0);
	PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
	RefreshSAT();
}

// FUNCTION: EVO 0x1000a898
void NewsDialogUpdate(void)
{
	Rect box;
	Str255 str;

	SATSetPort(&loadPort);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	ForeColor(blackColor);
	PaintRect(&newsDialog->portRect);
	DrawPicture(newsPic, &newsDialog->portRect);
	SetRect(&box, newsDialog->portRect.left + 15, newsDialog->portRect.top + 140,
			newsDialog->portRect.right - 15, newsDialog->portRect.top + kHalfCircle);
	TETextBox(sponsorString, strlen(sponsorString), &box, teFlushDefault);
	InvertRect(&box);
	SetRect(&box, newsDialog->portRect.left + 15, newsDialog->portRect.top + 170,
			newsDialog->portRect.right - 15, newsDialog->portRect.bottom - 18);
	TETextBox(newsString, strlen(newsString), &box, teFlushDefault);
	InvertRect(&box);
	SATSetPortScreen();
	SetPort(newsDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &newsDialog->portBits, &newsDialog->portRect, &newsDialog->portRect, srcCopy, NULL);
}

// FUNCTION: EVO 0x1000aa44
void RandomizeNewsDialog(void)
{
	Str255 line;
	Str255 tmp;
	short count;
	Handle h;
	short i;
	short pick;
	short matching;
	short eligible;
	Boolean ok;

	count = 0;
	h = GetResource('STR#', kSponsorStrList);
	if (h != NULL) {
		HLock(h);
		BlockMove(*h, &count, 2);
		HUnlock(h);
		HPurge(h);
		ReleaseResource(h);
	}
	if (count > 0) {
		GetIndString(line, kSponsorStrList, Rand(count) + 1);
		strncpy(sponsorString, (char *)line, 255);
		p2cstr((StringPtr)sponsorString);
	} else
		strncpy(sponsorString, "", 3);
	h = GetResource('STR#', kNewsStrList);
	if (h != NULL) {
		HLock(h);
		BlockMove(*h, &count, 2);
		HUnlock(h);
		HPurge(h);
		ReleaseResource(h);
	}
	eligible = matching = 0;
	pick = -1;
	for (i = 0; i < MaxDisasters; i++) {
		if (disasterData[i].countdown > 1) {
			eligible++;
			if (shipData->targetStellar == disasterData[i].stellarID || disasterData[i].stellar == -2)
				matching++;
		}
	}
	if (eligible > 0) {
		if (matching > 0) {
			do {
				ok = false;
				pick = Rand(MaxDisasters);
				if (shipData->targetStellar == disasterData[pick].stellarID)
					ok = true;
				if (disasterData[pick].stellar == -2)
					ok = true;
				if (disasterData[pick].countdown <= 1)
					ok = false;
			} while (!ok);
		} else if (Rand(2) == 0) {
			do {
				pick = Rand(MaxDisasters);
			} while (disasterData[pick].countdown <= 1);
		} else
			pick = -1;
	}
	if (pick != -1) {
		strncpy((char *)line, (char *)"\p", 3);
		if (!LoadPluginString(pick + kDisasterNewsSTR, line))
			GetIndString(line, kDisasterNewsStrList, pick + 1);
		if (line[0] > 1) {
			p2cstr(line);
			strncpy(newsString, (char *)line, 255);
		} else if (disasterData[pick].stellarID != -1) {
			strcat((char *)line, "TodayÕs Top News: ");
			strncpy((char *)tmp, (char *)disasterData[pick].name, 63);
			p2cstr(tmp);
			strcat((char *)line, (char *)tmp);
			strcat((char *)line, " has ");
			if (disasterData[pick].amount > 0)
				strcat((char *)line, "raised");
			else
				strcat((char *)line, "lowered");
			strcat((char *)line, " the price of ");
			strncpy((char *)tmp, lcCargoName[disasterData[pick].commodity], 63);
			p2cstr(tmp);
			strcat((char *)line, (char *)tmp);
			strcat((char *)line, " on ");
			strncpy((char *)tmp, (char *)stellarData[disasterData[pick].stellarID].name, 31);
			p2cstr(tmp);
			strncat((char *)line, (char *)tmp, 63);
			strcat((char *)line, ".");
			strncpy(newsString, (char *)line, 255);
		}
	} else if (count > 0) {
		GetIndString(line, kNewsStrList, Rand(count) + 1);
		p2cstr(line);
		strncpy(newsString, (char *)line, 255);
	} else
		strncpy(newsString, "No news to report", 24);
}

// FUNCTION: EVO 0x1000af9c
void RunSlotMachine(void)
{
	Rect r3;
	short counts[6];
	Rect box;
	Rect r2;
	short unused;
	short itemType;
	Handle itemHandle;
	Point where;
	Str255 msg;
	Str255 tmp;
	char text[255];
	Boolean quit;
	short bet;
	Boolean done;
	short i;
	short n;

	quit = false;
	for (i = 0; i <= 3; i++)
		slotPics[i] = GetPicture(i + kBet1000ButtonPict);
	slotBlipSnd = LoadSndResource(kSlotBlipSndID);
	slotLockSnd = LoadSndResource(kSlotLockSndID);
	slotDialog = NULL;
	slotDialog = GetNewDialog(kSlotDlogID, NULL, (WindowPtr)-1);
	if (slotDialog != NULL) {
		NewDialogHook(slotDialog, 0);
		CenterWindow(slotDialog);
		ShowWindow(slotDialog);
		SelectWindow(slotDialog);
		SetPort(slotDialog);
		FlushEvents(everyEvent - diskMask, 0);
		for (i = 0; i <= 2; i++) {
			reelPos[i] = Rand(6) * kReelHeight;
			reelSpin[i] = Rand(200) + 200;
		}
		ForeColor(blackColor);
		PaintRect(&slotDialog->portRect);
		RGBForeColor(&rgbMedGrey);
		FrameRect(&slotDialog->portRect);
		ForeColor(blackColor);
		DrawScrollbarPict();
		DrawSlotMachineReels();
		DrawSlotButtons(-1);
		for (;;) {
			while (!Button())
				;
			GetMouse(&where);
			n = TrackSlotButtons(where);
			if (n == 0) {
				if (shipData->credits >= kSmallBet) {
					bet = kSmallBet;
					break;
				}
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
			} else if (n == 1) {
				if (shipData->credits >= kBigBet) {
					bet = kBigBet;
					break;
				}
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
			} else {
				quit = true;
				break;
			}
		}
		if (!quit) {
			shipData->credits -= bet;
			hudRedraw.credits = true;
			DrawStatus();
			SetPort(slotDialog);
			done = false;
			while (!done) {
				done = true;
				for (i = 0; i <= 2; i++) {
					if (reelSpin[i] > 0 || reelPos[i] % kReelHeight > 1) {
						if (reelPos[i] % kReelHeight <= 1 && CountMatchingSoundVoices((long)slotBlipSnd) == 0)
							PlaySound(slotBlipSnd, 1, kMidVolume, kMidVolume);
						if (reelSpin[i] >= 100)
							reelPos[i] -= 8;
						else if (reelSpin[i] > 1)
							reelPos[i] -= 8.0 * (reelSpin[i] / 100.0);
						else
							reelPos[i]--;
						if (reelPos[i] < 0)
							reelPos[i] += 6 * kReelHeight;
						reelSpin[i]--;
						done = false;
						if (reelSpin[i] <= 0 && reelPos[i] % kReelHeight <= 1)
							PlaySound(slotLockSnd, 9, kMidVolume, kMidVolume);
					}
				}
				DrawSlotMachineReels();
			}
			for (i = 0; i <= 5; i++)
				counts[i] = 0;
			for (i = 0; i <= 2; i++) {
				n = reelPos[i] / kReelHeight;
				if (n < 0)
					n = 0;
				if (n > 5)
					n = 5;
				counts[n]++;
			}
			SetPort(slotDialog);
			GetIndString(msg, kSlotStrList, Rand(3) + 1);
			for (i = 0; i <= 5; i++) {
				if (counts[i] == 2) {
					bet *= 1.3;
					PlaySound(explodeSnd[0], 10, kMidVolume, kMidVolume);
					shipData->credits += bet;
					strcpy((char *)msg, "Congratulations - you win ");
					NumToString(bet / 1000.0, tmp);
					p2cstr(tmp);
					strcat((char *)msg, (char *)tmp);
					strcat((char *)msg, ",");
					n = bet % 1000;
					if (n < 100)
						strcat((char *)msg, "0");
					if (n < 10)
						strcat((char *)msg, "0");
					NumToString(n, tmp);
					p2cstr(tmp);
					strcat((char *)msg, (char *)tmp);
					strcat((char *)msg, " credits!");
					c2pstr((char *)msg);
					break;
				}
				if (counts[i] == 3) {
					bet *= 3.0;
					PlaySound(explodeSnd[3], 10, kMidVolume, kMidVolume);
					shipData->credits += bet;
					strcpy((char *)msg, "Congratulations - you win ");
					NumToString(bet / 1000.0, tmp);
					p2cstr(tmp);
					strcat((char *)msg, (char *)tmp);
					strcat((char *)msg, ",");
					n = bet % 1000;
					if (n < 100)
						strcat((char *)msg, "0");
					if (n < 10)
						strcat((char *)msg, "0");
					NumToString(n, tmp);
					p2cstr(tmp);
					strcat((char *)msg, (char *)tmp);
					strcat((char *)msg, " credits!");
					c2pstr((char *)msg);
					break;
				}
			}
			hudRedraw.credits = true;
			DrawStatus();
			SetPort(slotDialog);
			ForeColor(blackColor);
			GetDialogItem(slotDialog, kSlotBetItemSmall, &itemType, &itemHandle, &box);
			GetDialogItem(slotDialog, kSlotBetItemRight, &itemType, &itemHandle, &r2);
			box.right = r2.right;
			PaintRect(&box);
			strncpy(text, (char *)msg, 255);
			p2cstr((StringPtr)text);
			SATSetPort(&loadPort);
			ForeColor(blackColor);
			TextFont(geneva);
			TextSize(kDialogTextSize);
			TETextBox(text, strlen(text), &box, teFlushDefault);
			InvertRect(&box);
			SATSetPortScreen();
			SetPort(slotDialog);
			ForeColor(blackColor);
			CopyBits(&loadPort.port->portBits, &slotDialog->portBits, &box, &box, srcCopy, NULL);
			while (!Button())
				;
		}
		for (i = 0; i <= 3; i++) {
			if (slotPics[i] != NULL) {
				HPurge((Handle)slotPics[i]);
				ReleaseResource((Handle)slotPics[i]);
			}
		}
		if (slotBlipSnd != NULL) {
			FlushMixQueueEntries((long)slotBlipSnd);
			DisposePtr(slotBlipSnd);
		}
		if (slotLockSnd != NULL) {
			FlushMixQueueEntries((long)slotLockSnd);
			DisposePtr(slotLockSnd);
		}
		DisposeDialog(slotDialog);
		FlushEvents(everyEvent - diskMask, 0);
	}
}

// FUNCTION: EVO 0x1000ba68
void DrawScrollbarPict(void)
{
	PicHandle pic;
	Rect r;

	pic = GetPicture(kSlotReelStripPictID);
	SATSetPort(&loadPort);
	SetRect(&slotOffRect, 0, 0, kReelHeight, 6 * kReelHeight + 1);
	DrawPicture(pic, &slotOffRect);
	r = slotOffRect;
	OffsetRect(&r, 0, 6 * kReelHeight + 1);
	DrawPicture(pic, &r);
	HPurge((Handle)pic);
	ReleaseResource((Handle)pic);
	SATSetPortScreen();
	SetPort(slotDialog);
}

// FUNCTION: EVO 0x1000bb44
void DrawSlotMachineReels(void)
{
	short itemType;
	Rect box;
	Rect src;
	Rect dst;
	Handle itemHandle;
	short i;

	for (i = 0; i <= 2; i++) {
		GetDialogItem(slotDialog, i + 1, &itemType, &itemHandle, &box);
		SetRect(&src, 0, reelPos[i], kReelHeight, reelPos[i] + kReelHeight);
		dst = src;
		SATSetPort(&loadPort);
		OffsetRect(&dst, 2 * kReelHeight, 0);
		CopyBits(&loadPort.port->portBits, &loadPort.port->portBits, &src, &dst, srcCopy, NULL);
		ForeColor(whiteColor);
		FrameRect(&dst);
		ForeColor(blackColor);
		SATSetPortScreen();
		SetPort(slotDialog);
		CopyBits(&loadPort.port->portBits, &slotDialog->portBits, &dst, &box, srcCopy, NULL);
	}
}

// FUNCTION: EVO 0x1000bca8
void RunBarPersonEncounter(void)
{
	Str255 str;
	short newQueue[kMaxBarQueue];
	short i;
	short count;
	short pers;

	if (barPersonIndex > -1 && barPersonIndex < kMaxBarQueue && barQueue[barPersonIndex] > -1) {
		RunSingleMissionDialog(barQueue[barPersonIndex]);
		pers = barQueue[barPersonIndex];
		for (i = 0; i < kMaxBarQueue; i++)
			newQueue[i] = -1;
		count = 0;
		for (i = 0; i < kMaxBarQueue; i++) {
			if (pers != barQueue[i] && barQueue[i] != -1) {
				newQueue[count] = barQueue[i];
				count++;
			}
		}
		for (i = 0; i < kMaxBarQueue; i++)
			barQueue[i] = newQueue[i];
		SetPort(portDialog);
		PortDialogUpdate();
		SetPort(barDialog);
		BarDialogUpdate();
		barPersonIndex++;
		barMashCounter = 0;
	}
}
