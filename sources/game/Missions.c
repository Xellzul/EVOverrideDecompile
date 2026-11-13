/* Missions.c -- mission tables, the mission BBS, mission accept/complete/fail and the mission dialogs. Notes: Research/notes/Missions.md */
#define OLDROUTINENAMES 1
#include <Quickdraw.h>
#include <Resources.h>
#include <Memory.h>
#include <TextUtils.h>
#include <Dialogs.h>
#include <Events.h>
#include <Lists.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "evo.h"

#define kMissionBbsDlogID	1006
#define kMissionInfoDlogID	1012
#define kMissionOfferDlogID	1016
#define kBbsLeaveItem		7

#define kBbsListItem		2
#define kBbsScrollItem		3
#define kBbsTextItem		4
#define kBbsTitleItem		5
#define kBbsMapItem			6
#define kBbsHeaderItem		8
#define kBbsPilotItem		9
#define kBbsMissionLogItem	10
#define kMinfoListItem		2
#define kMinfoHeaderItem	3
#define kMinfoTextItem		4
#define kMinfoAbortItem		5
#define kMinfoMapItem		6
#define kOfferRefuseItem	2
#define kOfferTextItem		3
#define kOfferMapItem		4
#define kOfferPilotItem		5
#define kOfferMissionLogItem	7
#define kLinkSystBase	5000
#define kListDefID			128


extern ShipObj			*shipData;
extern ShipRec			*gShipTypes;
extern GovtRecord		*govtData;
extern SpobRecord		*stellarData;
extern OopsRecord		*disasterData;
extern SystRecord		*gSystems;
extern MissionRecord	*currentMissions;


extern SpobRecord	*currentSpob;
extern PersRecord	*personData;


Str255 missionName[512];
Boolean gDebugA;
Boolean gDebugB;
Boolean canRefuseMission;
Boolean drawGate;
PicHandle missionPics[4];
PicHandle missionInfoPics[4];
short missionInfoMap[8];
short inBar;
short selMissionListItem;
short missionInfoSel;
short lastMissionStellar;
short speakerShip;
DialogPtr missionDialog;
DialogPtr missionInfoDialog;
ListHandle missionList;
ListHandle missionInfoList;
char missionDescText[1024];
Handle descTextHandle;
BriefMissionRec briefMissionInfo[512];
MisnResHandle misnHandle;
MissionAvailRec missionAvailData[512];

// FUNCTION: EVO 0x100473E8
void InitMissions(void)
{
	short	i;
	short	j;
	short	id;
	ResType	type;
	Str255	name;

	lastMissionStellar = -1;
	drawGate = false;
	speakerShip = -1;
	missionDialog = 0;
	greenMissionSyst = -1;
	gMissionFlag = true;
	gDebugA = EVBugResourceFieldTest(14);
	gDebugB = EVBugResourceFieldTest(15);
	if (!EVBugResourceFieldTest(1)) {
		if (!EVBugResourceFieldTest(9)) {
			for (j = 0; j < 8; j++) {
				currentMissionObjectives[j].active = false;
				currentMissionObjectives[j].failed = false;
			}
			for (i = 0; i < NumMissionBits; i++) {
				missionBit[i] = 0;
				missionAvailData[i].availStel = -kShortBound;
				missionAvailData[i].availRandom = 0;
				missionAvailData[i].availLoc = -1;
				missionAvailData[i].availShipType = -1;
			}
		}
		if (!EVBugResourceFieldTest(8)) {
			for (i = 0; i < NumMissionBits; i++) {
				misnHandle = NULL;
				misnHandle = (MisnResHandle)GetResource(kMisnType, i + FirstResourceID);
				if (misnHandle != NULL) {
					HLock((Handle)misnHandle);
					if (!EVBugResourceFieldTest(6)) {
						missionAvailData[i].availStel = (*misnHandle)->availStel;
						missionAvailData[i].availBitSet = (*misnHandle)->availBitSet;
						missionAvailData[i].availBitClr = (*misnHandle)->availBitClr;
						missionAvailData[i].availLoc = (*misnHandle)->availLoc;
						missionAvailData[i].availRecord = (*misnHandle)->availRecord;
						missionAvailData[i].availRating = (*misnHandle)->availRating;
						missionAvailData[i].availRandom = (*misnHandle)->availRandom;
						missionAvailData[i].cargoQty = (*misnHandle)->cargoQty;
						missionAvailData[i].flags = (*misnHandle)->flags;
						if (GetHandleSize((Handle)misnHandle) < sizeof(MisnResource))
							missionAvailData[i].availShipType = -1;
						else
							missionAvailData[i].availShipType = (*misnHandle)->availShipType;
						if (missionAvailData[i].availLoc < kAvailLocComputer)
							missionAvailData[i].availLoc = kAvailLocComputer;
						if (missionAvailData[i].availLoc > kAvailLocShip)
							missionAvailData[i].availLoc = kAvailLocShip;
					}
					if (!EVBugResourceFieldTest(5))
						SelectMissionCargoQty(&missionAvailData[i].cargoQty);
					if (!EVBugResourceFieldTest(7)) {
						GetResInfo((Handle)misnHandle, &id, &type, name);
						strncpy((char *)missionName[i], (char *)name, 250);
					}
					HUnlock((Handle)misnHandle);
					HPurge((Handle)misnHandle);
					ReleaseResource((Handle)misnHandle);
				}
			}
		}
	}
}

// FUNCTION: EVO 0x10047814
void DoMissionDialog(Boolean fromBar)
{
	short	j;
	Boolean	done;
	short	i;
	short	count;
	Point	pt;
	Rect	r2;
	Rect	box;
	short	itemHit;
	short	unused1;
	short	unused2;
	short	savedStellar;
	long	unused5;
	char	text[256];
	Str255	numStr;
	short	unused3;
	short	unused4;
	ModalFilterUPP	filterUPP;

	done = false;
	filterUPP = NewModalFilterProc(MissionFilter);
	if (fromBar)
		inBar = 1;
	else
		inBar = 0;
	count = 0;
	for (j = 0; j < MaxSimultaneousMissions; j++) {
		if (!currentMissionObjectives[j].active)
			count++;
	}
	if (count <= 0) {
		strncpy(text, "You\325re already on ", 30);
		NumToString(MaxSimultaneousMissions, numStr);
		p2cstr(numStr);
		strcat(text, (char *)numStr);
		strcat(text, " missions - you\325ll have to abort or finish one before you can accept another.");
		strncpy(gDialogText, text, 250);
		ShowTextDialog(false, false);
		RefreshSAT();
		SetPort(portDialog);
	} else {
		if (lastMissionStellar != shipData->targetStellar || lastMissionStellar < 0) {
			for (i = 0; i < NumMissionBits; i++)
				randomOdds[i] = Rand(100) + 1;
			RandomizeMissionData();
		}
		lastMissionStellar = shipData->targetStellar;
		greenMissionSyst = -1;
		gMissionFlag = true;
		count = 0;
		for (i = 0; i < NumMissionBits; i++) {
			if (missionGrid[inBar][i] != -1)
				count++;
		}
		if (count <= 0) {
			strncpy(gDialogText, "There are no missions available here.", 100);
			ShowTextDialog(false, false);
			RefreshSAT();
			SetPort(portDialog);
		} else {
			missionPics[0] = GetPicture(kAcceptMissionButtonPict);
			missionPics[1] = GetPicture(kAcceptMissionButtonPict + 1);
			missionPics[2] = GetPicture(kLeave121ButtonPict);
			missionPics[3] = GetPicture(kLeave121ButtonPict + 1);
			missionDialog = NULL;
			missionDialog = GetNewDialog(kMissionBbsDlogID, NULL, (WindowPtr)-1);
			if (missionDialog != NULL) {
				NewDialogHook(missionDialog, 0);
				CenterWindow(missionDialog);
				ShowWindow(missionDialog);
				SelectWindow(missionDialog);
				SetPort(missionDialog);
				FlushEvents(everyEvent - diskMask, 0);
				ForeColor(blackColor);
				InvalRect(&missionDialog->portRect);
				SetupMissionList();
				strncpy(missionDescText, "", 3);
				if (selMissionListItem != -1) {
					LoadDescription(missionDescText, missionGrid[inBar][selMissionListItem] + kMissionDescBase);
					MungeBriefing(1, missionGrid[inBar][selMissionListItem]);
				}
				do {
					ModalDialog(filterUPP, &itemHit);
					if (itemHit == kOkItem) {
						/* BUG: event kinds passed as masks; only the low event bits are flushed */
						FlushEvents(keyDown, 0);
						FlushEvents(autoKey, 0);
						FlushEvents(mouseDown, 0);
						FlushEvents(mouseUp, 0);
						if (selMissionListItem != -1)
							done = DoMissionAccept(missionGrid[inBar][selMissionListItem]);
						if (selMissionListItem != -1) {
							LoadDescription(missionDescText, missionGrid[inBar][selMissionListItem] + kMissionDescBase);
							MungeBriefing(1, missionGrid[inBar][selMissionListItem]);
						}
						MissionDialogUpdate();
					}
					if (itemHit == kBbsLeaveItem)
						done = true;
					if (itemHit == kBbsMapItem) {
						/* BUG: event kinds passed as masks; only the low event bits are flushed */
						FlushEvents(keyDown, 0);
						FlushEvents(autoKey, 0);
						FlushEvents(mouseDown, 0);
						FlushEvents(mouseUp, 0);
						greenMissionSyst = -1;
						gMissionFlag = true;
						if (selMissionListItem != -1 && (missionAvailData[missionGrid[inBar][selMissionListItem]].flags & kMisnGreenArrow)) {
							if (briefMissionInfo[missionGrid[inBar][selMissionListItem]].destStellar != -1)
								greenMissionSyst = stellarData[briefMissionInfo[missionGrid[inBar][selMissionListItem]].destStellar].system;
							else if (briefMissionInfo[missionGrid[inBar][selMissionListItem]].returnStellar != -1)
								greenMissionSyst = stellarData[briefMissionInfo[missionGrid[inBar][selMissionListItem]].returnStellar].system;
						}
						savedStellar = shipData->targetStellar;
						DoSystemMap();
						greenMissionSyst = -1;
						shipData->targetStellar = savedStellar;
						hudRedraw.nav = true;
						DrawStatus();
						SetPort(portDialog);
						InvalRect(&portDialog->portRect);
						PortDialogUpdate();
						SetPort(missionDialog);
						InvalRect(&missionDialog->portRect);
					}
					if (itemHit == kBbsMissionLogItem) {
						/* BUG: event kinds passed as masks; only the low event bits are flushed */
						FlushEvents(keyDown, 0);
						FlushEvents(autoKey, 0);
						FlushEvents(mouseDown, 0);
						FlushEvents(mouseUp, 0);
						count = 0;
						for (i = 0; i < MaxSimultaneousMissions; i++) {
							if (currentMissionObjectives[i].active)
								count++;
						}
						if (count > 0)
							DoMissionInfoDialog();
						if (selMissionListItem != -1) {
							LoadDescription(missionDescText, missionGrid[inBar][selMissionListItem] + kMissionDescBase);
							MungeBriefing(1, missionGrid[inBar][selMissionListItem]);
						}
						SetPort(portDialog);
						InvalRect(&portDialog->portRect);
						PortDialogUpdate();
						SetPort(missionDialog);
						InvalRect(&missionDialog->portRect);
					}
					if (itemHit == kBbsPilotItem) {
						/* BUG: event kinds passed as masks; only the low event bits are flushed */
						FlushEvents(keyDown, 0);
						FlushEvents(autoKey, 0);
						FlushEvents(mouseDown, 0);
						FlushEvents(mouseUp, 0);
						DoPlayerInfoDialog();
						SetPort(portDialog);
						InvalRect(&portDialog->portRect);
						PortDialogUpdate();
						SetPort(missionDialog);
						InvalRect(&missionDialog->portRect);
					}
				} while (!done);
				for (i = 0; i <= 3; i++) {
					if (missionPics[i] != NULL) {
						HPurge((Handle)missionPics[i]);
						ReleaseResource((Handle)missionPics[i]);
					}
				}
				if (missionList != NULL)
					LDispose(missionList);
				DisposeRoutineDescriptor(filterUPP);
				DisposeDialog(missionDialog);
				missionDialog = NULL;
				RefreshSAT();
			}
		}
	}
}

// FUNCTION: EVO 0x10048110
void SetupMissionList(void)
{
	short	i;
	short	count;
	long	len;
	Rect	box;
	Rect	dataBounds;
	short	itemType;
	long	unused;
	Handle	itemHandle;
	Point	cSize;
	Cell	cell;
	Str255	name;

	selMissionListItem = 0;
	count = 0;
	for (i = 0; i < NumMissionBits; i++) {
		if (missionGrid[inBar][i] != -1)
			count++;
	}
	SetPort(missionDialog);
	GetDialogItem(missionDialog, kBbsListItem, &itemType, &itemHandle, &box);
	SetRect(&dataBounds, 0, 0, 1, count);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	cSize.h = cSize.v = 0;
	missionList = LNew(&box, &dataBounds, cSize, kListDefID, missionDialog, false, false, false, true);
	(*missionList)->selFlags = lOnlyOne;
	LSetDrawingMode(false, missionList);
	cell.h = 0;
	cell.v = 0;
	while (cell.v < count) {
		if (missionGrid[inBar][cell.v] != -1) {
			strncpy((char *)name, (char *)missionName[missionGrid[inBar][cell.v]], 250);
			p2cstr(name);
			strncpy(missionDescText, (char *)name, 250);
			MungeBriefing(1, missionGrid[inBar][cell.v]);
			strncpy((char *)name, missionDescText, 250);
		} else
			strncpy((char *)name, "", 3);
		len = strlen((char *)name);
		LSetCell(name, len, cell, missionList);
		cell.v++;
	}
	cell.h = cell.v = 0;
	LSetSelect(true, cell, missionList);
	LSetDrawingMode(true, missionList);
}

// FUNCTION: EVO 0x1004839C
void SetupMissionInfoList(void)
{
	short	i;
	short	count;
	long	len;
	Rect	box;
	Rect	dataBounds;
	short	itemType;
	long	unused;
	Handle	itemHandle;
	Point	cSize;
	Cell	cell;
	char	line[256];
	char	name[255];

	for (i = 0; i < MaxSimultaneousMissions; i++)
		missionInfoMap[i] = -1;
	count = 0;
	for (i = 0; i < MaxSimultaneousMissions; i++) {
		if (currentMissionObjectives[i].active) {
			missionInfoMap[count] = i;
			count++;
		}
	}
	SetPort(missionInfoDialog);
	GetDialogItem(missionInfoDialog, kMinfoListItem, &itemType, &itemHandle, &box);
	box.right -= 15;
	SetRect(&dataBounds, 0, 0, 1, count);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	cSize.h = cSize.v = 0;
	missionInfoList = LNew(&box, &dataBounds, cSize, kListDefID, missionInfoDialog, false, false, false, true);
	(*missionInfoList)->selFlags = lOnlyOne;
	cell.h = cell.v = 0;
	for (i = 0; i < MaxSimultaneousMissions; i++) {
		if (currentMissionObjectives[i].active) {
			strncpy(line, "", 3);
			if (currentMissionObjectives[i].failed)
				strncpy(line, "\245 ", 4);
			strncpy(name, currentMissions[i].name, 250);
			p2cstr((StringPtr)name);
			strncpy(missionDescText, name, 250);
			MungeBriefing(0, i);
			strncpy(name, missionDescText, 250);
			strcat(line, name);
			len = strlen(line);
			LSetCell(line, len, cell, missionInfoList);
			cell.v++;
		}
	}
	cell.h = cell.v = 0;
	LSetSelect(true, cell, missionInfoList);
	LSetDrawingMode(true, missionInfoList);
}

// FUNCTION: EVO 0x10048658
void RandomizeMissionData(void)
{
	short	saved;
	short	page;
	short	i;
	short	j;
	short	n;
	Boolean	taken;

	saved = inBar;
	for (page = 0; page <= 1; page++) {
		inBar = page;
		for (i = 0; i < NumMissionBits; i++)
			missionGrid[page][i] = -1;
		n = 0;
		for (i = 0; i < NumMissionBits; i++) {
			taken = false;
			for (j = 0; j < MaxSimultaneousMissions; j++) {
				if (currentMissionObjectives[j].active && i == currentMissions[j].misnIndex)
					taken = true;
			}
			if (!taken && IsMissionAvailable(i)) {
				missionGrid[page][n] = i;
				n++;
			}
		}
		for (i = 0; i < NumMissionBits; i++) {
			if (missionGrid[page][i] != -1)
				RandomizeOneMission(missionGrid[page][i]);
		}
	}
	inBar = saved;
}

// FUNCTION: EVO 0x100487E8
void RandomizeOneMission(short mission)
{
	short	home;
	short	j;
	short	stel;

	if (drawGate) {
		home = 0;
		for (j = 0; j <= 3; j++) {
			if (gSystems[shipData->curSystem].navDefault[j] != -1) {
				home = gSystems[shipData->curSystem].navDefault[j];
				break;
			}
		}
	} else
		home = shipData->targetStellar;
	misnHandle = (MisnResHandle)GetResource(kMisnType, mission + FirstResourceID);
	if (misnHandle != NULL) {
		HLock((Handle)misnHandle);
		stel = (*misnHandle)->travelStel;
		if (stel == -1) {
			briefMissionInfo[mission].destStellar = -1;
			briefMissionInfo[mission].destSystem = -1;
		} else {
			briefMissionInfo[mission].destStellar = SelectMissionStellar(stel, home);
			briefMissionInfo[mission].destSystem = stellarData[briefMissionInfo[mission].destStellar].system;
		}
		stel = (*misnHandle)->returnStel;
		if (stel == -1)
			briefMissionInfo[mission].returnStellar = briefMissionInfo[mission].destStellar;
		else
			briefMissionInfo[mission].returnStellar = SelectMissionStellar(stel, home);
		briefMissionInfo[mission].returnSystem = stellarData[briefMissionInfo[mission].returnStellar].system;
		briefMissionInfo[mission].cargoType = (*misnHandle)->cargoType;
		briefMissionInfo[mission].cargoUnits = missionAvailData[mission].cargoQty;
		if (briefMissionInfo[mission].cargoType == kCargoTypeRandom)
			briefMissionInfo[mission].cargoType = Rand(NumCommodities);
		if (GetHandleSize((Handle)misnHandle) < sizeof(MisnResource))
			briefMissionInfo[mission].startBitSet = -1;
		else
			briefMissionInfo[mission].startBitSet = (*misnHandle)->startBitSet;
		SetMissionDeadline(&briefMissionInfo[mission].dueDate, (*misnHandle)->timeLimit);
		HUnlock((Handle)misnHandle);
		HPurge((Handle)misnHandle);
		ReleaseResource((Handle)misnHandle);
		misnHandle = NULL;
	}
}

// FUNCTION: EVO 0x10048B00
void SelectMissionCargoQty(short *value)
{
	short	v;
	short	r;
	short	total;

	v = *value;
	if (v >= 0)
		*value = v;
	if (v <= -2) {
		r = abs(v);
		total = r + Rand(r);
		*value = total;
	}
}

// FUNCTION: EVO 0x10048B80
short SelectMissionStellar(short type, short home)
{
	short	result;
	short	count;
	short	i;
	Boolean	ok;
	Boolean	showAll;
	Rect	deadRect;

	result = home;
	if (type == -1 || type == -4) {
		result = home;
	} else {
		if (type == -2) {
			count = 0;
			for (i = 0; i < MaxStellarObjects; i++) {
				if (i != home && stellarData[i].loaded
						&& (stellarData[i].flags & kSpobCanLand)
						&& !(stellarData[i].flags & kSpobUninhabited)
						&& StellarsAreDistant(&stellarData[i], &stellarData[home]))
					count++;
				if (count >= 1)
					break;
			}
			if (count <= 0)
				return home;
			do {
				result = Rand(MaxStellarObjects);
			} while (result == home
					|| !stellarData[result].loaded
					|| !SystemVisible(stellarData[result].system)
					|| !(stellarData[result].flags & kSpobCanLand)
					|| (stellarData[result].flags & kSpobUninhabited)
					|| (stellarData[result].flags & kSpobStation)
					|| !StellarsAreDistant(&stellarData[i], &stellarData[home]));
		}
		if (type == -3) {
			count = 0;
			for (i = 0; i < MaxStellarObjects; i++) {
				if (i != home && stellarData[i].loaded
						&& SystemVisible(stellarData[i].system)
						&& (stellarData[i].flags & kSpobUninhabited)
						&& (stellarData[i].flags & kSpobCanLand)
						&& !(stellarData[i].flags & kSpobStation))
					StellarsAreDistant(&stellarData[i], &stellarData[home]);
				count++;
				if (count >= 1)
					break;
			}
			if (count <= 0)
				return home;
			do {
				result = Rand(MaxStellarObjects);
			} while (result == home
					|| !stellarData[result].loaded
					|| !SystemVisible(stellarData[result].system)
					|| !(stellarData[result].flags & kSpobUninhabited)
					|| !(stellarData[result].flags & kSpobCanLand)
					|| (stellarData[result].flags & kSpobStation)
					|| !StellarsAreDistant(&stellarData[result], &stellarData[home]));
		}
		if (type >= FirstResourceID && type < FirstResourceID + MaxStellarObjects)
			result = type - FirstResourceID;
		if (type >= kLinkGovtBase - 1 && type < kLinkAllyBase) {
			showAll = false;
			if (type >= kLinkGovtBase && (govtData[type - kLinkGovtBase].flags & kGovtStartDisabled))
				showAll = true;
			count = 0;
			for (i = 0; i < MaxStellarObjects; i++) {
				if (i != home && stellarData[i].loaded
						&& SystemVisible(stellarData[i].system)
						&& type - kLinkGovtBase == stellarData[i].govt
						&& (!(stellarData[i].flags & kSpobUninhabited) || showAll)
						&& (stellarData[i].flags & kSpobCanLand)
						&& StellarsAreDistant(&stellarData[i], &stellarData[home]))
					count++;
				if (count >= 1)
					break;
			}
			if (count <= 0)
				return home;
			do {
				result = Rand(MaxStellarObjects);
			} while (result == home
					|| !stellarData[result].loaded
					|| !SystemVisible(stellarData[result].system)
					|| type - kLinkGovtBase != stellarData[result].govt
					|| ((stellarData[result].flags & kSpobUninhabited) && !showAll)
					|| !(stellarData[result].flags & kSpobCanLand)
					|| !StellarsAreDistant(&stellarData[result], &stellarData[home]));
		}
		if (type >= kLinkAllyBase && type < kLinkNotGovtBase) {
			count = 0;
			for (i = 0; i < MaxStellarObjects; i++) {
				ok = false;
				if (stellarData[i].govt != -1 && type - kLinkAllyBase != stellarData[i].govt) {
					if (type - kLinkAllyBase == gSystems[i].govt)
						ok = true;
					if (govtData[stellarData[i].govt].ally != -1 && govtData[type - kLinkAllyBase].ally != -1) {
						if (type - kLinkAllyBase == govtData[stellarData[i].govt].ally)
							ok = true;
						if (govtData[type - kLinkAllyBase].ally == stellarData[i].govt)
							ok = true;
					}
				}
				if (i != home && stellarData[i].loaded
						&& SystemVisible(stellarData[i].system)
						&& ok
						&& !(stellarData[i].flags & kSpobUninhabited)
						&& (stellarData[i].flags & kSpobCanLand)
						&& StellarsAreDistant(&stellarData[i], &stellarData[home]))
					count++;
				if (count >= 1)
					break;
			}
			if (count <= 0)
				return home;
			do {
				result = Rand(MaxStellarObjects);
				ok = false;
				if (stellarData[result].govt != -1 && type - kLinkAllyBase != stellarData[result].govt) {
					if (type - kLinkAllyBase == gSystems[result].govt)
						ok = true;
					if (govtData[stellarData[result].govt].ally != -1 && govtData[type - kLinkAllyBase].ally != -1) {
						if (type - kLinkAllyBase == govtData[stellarData[result].govt].ally)
							ok = true;
						if (govtData[type - kLinkAllyBase].ally == stellarData[result].govt)
							ok = true;
					}
				}
			} while (result == home || !stellarData[result].loaded
					|| !SystemVisible(stellarData[result].system)
					|| !ok
					|| (stellarData[result].flags & kSpobUninhabited)
					|| !(stellarData[result].flags & kSpobCanLand)
					|| !StellarsAreDistant(&stellarData[result], &stellarData[home]));
		}
		if (type >= kLinkNotGovtBase && type < kLinkEnemyBase) {
			count = 0;
			for (i = 0; i < MaxStellarObjects; i++) {
				if (i != home && stellarData[i].loaded
						&& SystemVisible(stellarData[i].system)
						&& type - kLinkNotGovtBase != stellarData[i].govt
						&& !(stellarData[i].flags & kSpobUninhabited)
						&& (stellarData[i].flags & kSpobCanLand)
						&& StellarsAreDistant(&stellarData[i], &stellarData[home]))
					count++;
				if (count >= 1)
					break;
			}
			if (count <= 0)
				return home;
			do {
				result = Rand(MaxStellarObjects);
			} while (!stellarData[result].loaded
					|| !SystemVisible(stellarData[result].system)
					|| type - kLinkNotGovtBase == stellarData[result].govt
					|| (stellarData[result].flags & kSpobUninhabited)
					|| !(stellarData[result].flags & kSpobCanLand)
					|| !StellarsAreDistant(&stellarData[result], &stellarData[home]));
		}
		if (type >= kLinkEnemyBase && type < kLinkLimit) {
			count = 0;
			for (i = 0; i < MaxStellarObjects; i++) {
				ok = false;
				if (stellarData[i].govt != -1 && type - kLinkEnemyBase != stellarData[i].govt
						&& govtData[stellarData[i].govt].enemy != -1
						&& govtData[type - kLinkEnemyBase].enemy != -1) {
					if (type - kLinkAllyBase == govtData[stellarData[i].govt].enemy)
						ok = true;
					if (govtData[type - kLinkAllyBase].enemy == stellarData[i].govt)
						ok = true;
				}
				if (i != home && stellarData[i].loaded
						&& SystemVisible(stellarData[i].system)
						&& ok
						&& !(stellarData[i].flags & kSpobUninhabited)
						&& (stellarData[i].flags & kSpobCanLand)
						&& StellarsAreDistant(&stellarData[i], &stellarData[home]))
					count++;
				if (count >= 1)
					break;
			}
			if (count <= 0)
				return home;
			do {
				result = Rand(MaxStellarObjects);
				ok = false;
				if (stellarData[result].govt != -1 && type - kLinkEnemyBase != stellarData[result].govt
						&& govtData[stellarData[result].govt].enemy != -1
						&& govtData[type - kLinkEnemyBase].enemy != -1) {
					if (type - kLinkAllyBase == govtData[stellarData[result].govt].enemy)
						ok = true;
					if (govtData[type - kLinkAllyBase].enemy == stellarData[result].govt)
						ok = true;
				}
			} while (result == home || !stellarData[result].loaded
					|| !SystemVisible(stellarData[result].system)
					|| !ok
					|| (stellarData[result].flags & kSpobUninhabited)
					|| !(stellarData[result].flags & kSpobCanLand)
					|| !StellarsAreDistant(&stellarData[result], &stellarData[home]));
		}
	}
	return result;
}

// FUNCTION: EVO 0x10049C5C
short SelectMissionSystem(short type, short home)
{
	short	result;
	short	count;
	short	i;
	short	j;
	Boolean	ok;

	result = home;
	if (type == kShipSystRandom) {
		count = 0;
		for (i = 0; i < MaxSystems; i++) {
			if (gSystems[i].loaded)
				count++;
			if (count >= 1)
				break;
		}
		if (count <= 0)
			return;	/* BUG: no value returned */
		do {
			result = Rand(MaxSystems);
		} while (!gSystems[result].loaded || !SystemVisible(result));
	}
	if (type == kShipSystAdjacent) {
		do {
			j = Rand(kMaxSystemLinks);
		} while (gSystems[home].links[j] == -1
				|| !gSystems[gSystems[home].links[j]].loaded
				|| !SystemVisible(gSystems[home].links[j]));
		result = gSystems[home].links[j];
	}
	if (type >= FirstResourceID && type < 1128)
		result = type - FirstResourceID;
	if (type >= kLinkGovtBase - 1 && type < kLinkAllyBase) {
		count = 0;
		for (i = 0; i < MaxSystems; i++) {
			if (gSystems[i].loaded && SystemVisible(i) && type - kLinkGovtBase == gSystems[i].govt)
				count++;
			if (count >= 1)
				break;
		}
		if (count <= 0)
			return;
		do {
			result = Rand(MaxSystems);
		} while (!gSystems[result].loaded || !SystemVisible(result) || type - kLinkGovtBase != gSystems[result].govt);
	}
	if (type >= kLinkAllyBase && type < kLinkNotGovtBase) {
		count = 0;
		for (i = 0; i < MaxSystems; i++) {
			ok = false;
			if (gSystems[i].govt >= 0 && type - kLinkAllyBase != gSystems[i].govt) {
				if (type - kLinkAllyBase == govtData[gSystems[i].govt].ally)
					ok = true;
				if (govtData[type - kLinkAllyBase].ally == gSystems[i].govt)
					ok = true;
			}
			if (gSystems[i].loaded && SystemVisible(i) && ok)
				count++;
			if (count >= 1)
				break;
		}
		if (count <= 0)
			return;
		do {
			result = Rand(MaxSystems);
			ok = false;
			if (gSystems[result].govt >= 0 && type - kLinkAllyBase != gSystems[result].govt) {
				if (type - kLinkAllyBase == govtData[gSystems[result].govt].ally)
					ok = true;
				if (govtData[type - kLinkAllyBase].ally == gSystems[result].govt)
					ok = true;
			}
		} while (!gSystems[result].loaded || !SystemVisible(result) || !ok);
	}
	if (type >= kLinkNotGovtBase && type < kLinkEnemyBase) {
		count = 0;
		for (i = 0; i < MaxSystems; i++) {
			if (gSystems[i].loaded && SystemVisible(i) && type - kLinkNotGovtBase != gSystems[i].govt)
				count++;
			if (count >= 1)
				break;
		}
		if (count <= 0)
			return;
		do {
			result = Rand(MaxSystems);
			ok = false;
		} while (!gSystems[result].loaded || !SystemVisible(result) || type - kLinkNotGovtBase == gSystems[result].govt);
	}
	if (type >= kLinkEnemyBase && type < kLinkLimit) {
		count = 0;
		for (i = 0; i < MaxSystems; i++) {
			ok = false;
			if (gSystems[i].govt >= 0 && type - kLinkEnemyBase != gSystems[i].govt) {
				if (type - kLinkAllyBase == govtData[gSystems[i].govt].enemy)	/* BUG: the ally base, not the enemy base */
					ok = true;
				if (govtData[type - kLinkAllyBase].enemy == gSystems[i].govt)
					ok = true;
			}
			if (gSystems[i].loaded && SystemVisible(i) && ok)
				count++;
			if (count >= 1)
				break;
		}
		if (count <= 0)
			return;
		do {
			result = Rand(MaxSystems);
			ok = false;
			if (gSystems[result].govt >= 0 && type - kLinkEnemyBase != gSystems[result].govt) {
				if (type - kLinkAllyBase == govtData[gSystems[result].govt].enemy)
					ok = true;
				if (govtData[type - kLinkAllyBase].enemy == gSystems[result].govt)
					ok = true;
			}
		} while (!gSystems[result].loaded || !SystemVisible(result) || !ok);
	}
	return result;
}

// FUNCTION: EVO 0x1004A4C4
void SetMissionDeadline(DateTimeRec *date, short days)
{
	DateTimeRec	d;
	short	i;

	d.month = gameTime.month;
	d.day = gameTime.day;
	d.year = gameTime.year;
	if (days > 0) {
		for (i = 0; i < days; i++)
			IncrementDate(&d, 0);
		date->month = d.month;
		date->day = d.day;
		date->year = d.year;
	}
}

// FUNCTION: EVO 0x1004A570
Boolean DoMissionAccept(short mission)
{
	short	i;
	short	j;
	short	slot;
	short	saved;
	Rect	r1;
	short	grid[NumMissionBits];
	Rect	r2;

	slot = -1;
	for (i = 0; i < MaxSimultaneousMissions; i++) {
		if (!currentMissionObjectives[i].active) {
			slot = i;
			break;
		}
	}
	if (slot == -1)
		return false;
	if (briefMissionInfo[mission].cargoUnits > 0) {
		if (ShipTotalHolds() < briefMissionInfo[mission].cargoUnits) {
			strncpy(gDialogText, "Your ship doesn\325t have enough cargo space to accept this mission.", 250);
			ShowTextDialog(false, false);
			RefreshSAT();
			return false;
		}
		if (TotalMissionCargoSpace() < briefMissionInfo[mission].cargoUnits) {
			strncpy(gDialogText, "Your ship doesn\325t have enough free cargo space to accept this mission. Sell or jettison some and try again.", 250);
			ShowTextDialog(false, false);
			RefreshSAT();
			return false;
		}
	}
	currentMissionObjectives[slot].active = true;
	currentMissionObjectives[slot].failed = false;
	currentMissionObjectives[slot].returnDone = false;
	currentMissionObjectives[slot].objectivesDone = false;
	currentMissions[slot].specialShipsKilled = 0;
	currentMissions[slot].specialShipsJumpedOut = 0;
	currentMissions[slot].specialShipsJumpedIn = 0;
	currentMissions[slot].specialShipsBoarded = 0;
	currentMissions[slot].specialShipsDisabled = 0;
	strncpy(currentMissions[slot].name, (char *)missionName[mission], 127);
	LoadCurrentMissionData(mission, slot);
	currentMissions[slot].misnIndex = mission;
	if (currentMissions[slot].timeLimit > 0)
		SetMissionDeadline(&currentMissionObjectives[slot].dueDate, currentMissions[slot].timeLimit);
	if (briefMissionInfo[mission].startBitSet >= 0 && briefMissionInfo[mission].startBitSet < kNotBitBase)
		missionBit[briefMissionInfo[mission].startBitSet] = 1;
	else if (briefMissionInfo[mission].startBitSet >= kNotBitBase && briefMissionInfo[mission].startBitSet < kNotBitBase + NumMissionBits)
		missionBit[briefMissionInfo[mission].startBitSet - kNotBitBase] = 0;
	if (missionDialog != NULL)
		HideWindow(missionDialog);
	if (currentMissions[slot].briefDesc != -1) {
		if (PlayMovieById(currentMissions[slot].briefDesc, true)) {
			strncpy(missionDescText, "", 3);
			LoadDescription(missionDescText, currentMissions[slot].briefDesc);
			MungeBriefing(0, slot);
			strncpy(gDialogText, missionDescText, 1023);
			if (missionDialog != NULL)
				HideWindow(missionDialog);
			greenMissionSyst = -1;
			gMissionFlag = true;
			if (currentMissions[slot].flags & kMisnGreenArrow) {
				if (currentMissions[slot].travelStellar != -1)
					greenMissionSyst = stellarData[currentMissions[slot].travelStellar].system;
				else if (currentMissions[slot].returnStellar != -1)
					greenMissionSyst = stellarData[currentMissions[slot].returnStellar].system;
			}
			if (portDialog != NULL && !drawGate)
				PortDialogUpdate();
			ShowTextDialog(true, false);
		} else {
			if (missionDialog != NULL)
				HideWindow(missionDialog);
			if (portDialog != NULL && !drawGate)
				PortDialogUpdate();
		}
		RefreshSAT();
		greenMissionSyst = -1;
		PlayMovieById(currentMissions[slot].briefDesc, false);
	}
	if (currentMissions[slot].pickupMode == kPickupAtStart)
		currentMissions[slot].cargoPickedUp = true;
	if ((shipData->targetStellar == currentMissions[slot].travelStellar && missionDialog != NULL) || currentMissions[slot].travelStellar == -1)
		currentMissionObjectives[slot].returnDone = true;
	if (currentMissions[slot].payment <= -30128 && currentMissions[slot].payment > -40000) {
		i = -30128.0f + FloatAbs(currentMissions[slot].payment);
		if (i >= 0 && i < MaxOutfitItemTypes && outfitCount[i] < 1) {
			outfitCount[i] = 1;
			SystemInfoToShipStats();
		}
	}
	if (!drawGate && missionDialog != NULL) {
		saved = inBar;
		inBar = 0;
		while (inBar <= 1) {
			j = 0;
			for (i = 0; i < NumMissionBits; i++)
				grid[i] = -1;
			for (i = 0; i < NumMissionBits; i++) {
				if (mission != missionGrid[inBar][i] && missionGrid[inBar][i] != -1 && IsMissionAvailable(missionGrid[inBar][i])) {
					grid[j] = missionGrid[inBar][i];
					j++;
				}
			}
			for (i = 0; i < NumMissionBits; i++)
				missionGrid[inBar][i] = grid[i];
			inBar++;
		}
		inBar = saved;
	}
	randomOdds[mission] = 0;
	return true;
}

// FUNCTION: EVO 0x1004ADF8
void LoadCurrentMissionData(short mission, short slot)
{
	short	count;
	Str255	str;
	short	stel;
	Handle	strHandle;

	currentMissions[slot].travelStellar = briefMissionInfo[mission].destStellar;
	currentMissions[slot].returnStellar = briefMissionInfo[mission].returnStellar;
	currentMissions[slot].cargoType = briefMissionInfo[mission].cargoType;
	currentMissions[slot].cargoQty = briefMissionInfo[mission].cargoUnits;
	misnHandle = (MisnResHandle)GetResource(kMisnType, mission + FirstResourceID);
	if (misnHandle != NULL) {
		HLock((Handle)misnHandle);
		currentMissions[slot].pickupMode = (*misnHandle)->pickupMode;
		currentMissions[slot].dropoffMode = (*misnHandle)->dropoffMode;
		currentMissions[slot].scanGovt = (*misnHandle)->scanGovt;
		if (currentMissions[slot].scanGovt != -1)
			currentMissions[slot].scanGovt -= FirstResourceID;
		currentMissions[slot].payment = (*misnHandle)->payVal;
		currentMissions[slot].shipCount = (*misnHandle)->shipCount;
		currentMissions[slot].initialShipCount = currentMissions[slot].shipCount;
		currentMissions[slot].shipDude = (*misnHandle)->shipDude;
		if (currentMissions[slot].shipDude > 0)
			currentMissions[slot].shipDude -= FirstResourceID;
		currentMissions[slot].shipGoal = (*misnHandle)->shipGoal;
		currentMissions[slot].shipBehav = (*misnHandle)->shipBehav;
		currentMissions[slot].compBitSet[0] = (*misnHandle)->compBitSet;
		currentMissions[slot].compBitSet[1] = (*misnHandle)->compBitSet2;
		currentMissions[slot].compBitSet[2] = (*misnHandle)->compBitSet3;
		currentMissions[slot].compBitSet[3] = (*misnHandle)->compBitSet4;
		currentMissions[slot].compGovt = (*misnHandle)->compGovt;
		currentMissions[slot].compReward = (*misnHandle)->compReward;
		if (currentMissions[slot].compGovt >= FirstResourceID)
			currentMissions[slot].compGovt -= FirstResourceID;
		if (currentMissions[slot].compGovt < 0 || currentMissions[slot].compGovt >= MaxGovts)
			currentMissions[slot].compReward = 0;
		currentMissions[slot].failBitSet[0] = (*misnHandle)->failBitSet;
		currentMissions[slot].failBitSet[1] = (*misnHandle)->failBitSet2;
		currentMissions[slot].flags = (*misnHandle)->flags;
		currentMissions[slot].briefDesc = (*misnHandle)->briefText;
		currentMissions[slot].quickBriefDesc = (*misnHandle)->quickBrief;
		currentMissions[slot].loadCargoDesc = (*misnHandle)->loadCargText;
		currentMissions[slot].dropCargoDesc = (*misnHandle)->dropCargText;
		currentMissions[slot].compDesc = (*misnHandle)->compText;
		currentMissions[slot].failDesc = (*misnHandle)->failText;
		currentMissions[slot].refuseDesc = (*misnHandle)->refuseText;
		currentMissions[slot].timeLimit = (*misnHandle)->timeLimit;
		currentMissions[slot].unused44 = (*misnHandle)->unused44;
		currentMissions[slot].auxShipCount = (*misnHandle)->auxShipCount;
		currentMissions[slot].auxShipDude = (*misnHandle)->auxShipDude;
		currentMissions[slot].auxShipSyst = (*misnHandle)->auxShipSyst;
		currentMissions[slot].auxShipDelay = Rand(kMissionSpawnDelay) + kMissionSpawnDelay;
		currentMissions[slot].auxShipsLeft = currentMissions[slot].auxShipCount;
		while (currentMissions[slot].auxShipDude >= FirstResourceID)
			currentMissions[slot].auxShipDude -= FirstResourceID;
		if (currentMissions[slot].auxShipCount <= 0) {
			currentMissions[slot].auxShipCount = -1;
			currentMissions[slot].auxShipsLeft = -1;
		}
		if (currentMissions[slot].shipBehav >= 9)
			currentMissions[slot].shipDelay = Rand(100) + 100;
		else
			currentMissions[slot].shipDelay = -1;
		currentMissions[slot].shipStrList = -1;
		currentMissions[slot].shipStrIndex = -1;
		if (currentMissions[slot].timeLimit < 0)
			currentMissions[slot].timeLimit = -kShortBound;
		if ((*misnHandle)->failIfScan != 0)
			currentMissions[slot].failIfScan = true;
		else
			currentMissions[slot].failIfScan = false;
		if ((*misnHandle)->canAbort != 0)
			currentMissions[slot].canAbort = true;
		else
			currentMissions[slot].canAbort = false;
		stel = (*misnHandle)->shipSyst;
		if (stel == kShipSystInitial)
			currentMissions[slot].shipSyst = shipData->curSystem;
		else if (stel == kShipSystTravel)
			currentMissions[slot].shipSyst = stellarData[briefMissionInfo[mission].destStellar].system;
		else if (stel == kShipSystReturn)
			currentMissions[slot].shipSyst = stellarData[briefMissionInfo[mission].returnStellar].system;
		else if (stel == kShipSystFollow)
			currentMissions[slot].shipSyst = kShipSystFollow;
		else if (stel == kShipSystAdjacent || stel >= 9999)
			currentMissions[slot].shipSyst = SelectMissionSystem(stel, shipData->curSystem);
		else
			currentMissions[slot].shipSyst = stel - FirstResourceID;	/* BUG: kShipSystRandom lands here as system -130 */
		strncpy(currentMissions[slot].shipName, (char *)"\p", 3);
		if ((*misnHandle)->shipNameID != -1) {
			currentMissions[slot].shipStrList = (*misnHandle)->shipNameID;
			strHandle = GetResource('STR#', currentMissions[slot].shipStrList);
			if (strHandle != NULL) {
				HLock(strHandle);
				BlockMove(*strHandle, &count, 2);
				HUnlock(strHandle);
				HPurge(strHandle);
				ReleaseResource(strHandle);
				stel = Rand(count) + 1;
				GetIndString(str, (*misnHandle)->shipNameID, stel);
				strncpy(currentMissions[slot].shipName, (char *)str, 31);
				currentMissions[slot].shipStrIndex = stel;
			}
		}
		currentMissions[slot].cargoPickedUp = false;
		currentMissions[slot].specialShipsKilled = 0;
		currentMissions[slot].specialShipsBoarded = 0;
		currentMissions[slot].specialShipsDisabled = 0;
		currentMissions[slot].specialShipsJumpedOut = 0;
		if (currentMissions[slot].shipBehav >= 9)
			currentMissions[slot].specialShipsJumpedIn = 0;
		else
			currentMissions[slot].specialShipsJumpedIn = currentMissions[slot].initialShipCount;
		currentMissions[slot].auxShipsSpawned = 0;
		HUnlock((Handle)misnHandle);
		HPurge((Handle)misnHandle);
		ReleaseResource((Handle)misnHandle);
	}
	currentMissions[slot].misnIndex = mission;
}

// FUNCTION: EVO 0x1004B970
Boolean DoMissionCargoPickup(short cargoType, short quantity)
{
	Rect	r;
	Rect	r2;

	if (quantity > 0) {
		if (ShipTotalHolds() < quantity) {
			strncpy(gDialogText, "Your ship doesn\325t have enough cargo space to load this cargo.", 250);
			ShowTextDialog(false, false);
			RefreshSAT();
			return false;
		}
		if (TotalMissionCargoSpace() < quantity) {
			strncpy(gDialogText, "Your ship doesn\325t have enough free cargo space to load this cargo. Sell or jettison some and try again.", 250);
			ShowTextDialog(false, false);
			RefreshSAT();
			return false;
		}
	}
	hudRedraw.credits = true;
	return true;
}

// FUNCTION: EVO 0x1004BA48
void DoMissionSuccess(short mission)
{
	Str255	unused;
	Rect	r;
	short	i;
	short	j;
	short	k;

	if (currentMissions[mission].compDesc != -1) {
		if (cursorHidden)
			ShowCursor();
		if (PlayMovieById(currentMissions[mission].compDesc, true)) {
			strncpy(missionDescText, "", 3);
			LoadDescription(missionDescText, currentMissions[mission].compDesc);
			MungeBriefing(0, mission);
			strncpy(gDialogText, missionDescText, 1023);
			ShowTextDialog(false, false);
		}
		RefreshSAT();
		if (cursorHidden)
			HideCursor();
		PlayMovieById(currentMissions[mission].compDesc, false);
	}
	currentMissionObjectives[mission].active = false;
	for (i = 0; i <= 3; i++) {
		if (currentMissions[mission].compBitSet[i] >= 0 && currentMissions[mission].compBitSet[i] < NumMissionBits) {
			missionBit[currentMissions[mission].compBitSet[i]] = 1;
			for (j = 0; j < MaxDisasters; j++) {
				if (disasterData[j].missionBit == currentMissions[mission].compBitSet[i] && disasterData[j].duration > 0)
					disasterData[j].countdown = disasterData[j].duration;
			}
		}
		if (currentMissions[mission].compBitSet[i] >= kNotBitBase && currentMissions[mission].compBitSet[i] < kNotBitBase + NumMissionBits)
			missionBit[currentMissions[mission].compBitSet[i] - kNotBitBase] = 0;
	}
	if (currentMissions[mission].compGovt != -1) {
		for (i = 0; i < MaxSystems; i++) {
			if (gSystems[i].govt == currentMissions[mission].compGovt)
				legalStatus[i] += currentMissions[mission].compReward;
			else if (gSystems[i].govt != -1) {
				if (currentMissions[mission].compGovt == govtData[gSystems[i].govt].enemy
						|| gSystems[i].govt == govtData[currentMissions[mission].compGovt].enemy)
					legalStatus[i] -= 0.5 * currentMissions[mission].compReward;
				else if (currentMissions[mission].compGovt == govtData[gSystems[i].govt].ally
						|| gSystems[i].govt == govtData[currentMissions[mission].compGovt].ally)
					legalStatus[i] += 0.5 * currentMissions[mission].compReward;
			}
		}
	}
	if (currentMissions[mission].payment > 0) {
		shipData->credits += currentMissions[mission].payment;
		hudRedraw.credits = true;
	}
	if (currentMissions[mission].payment <= -10000 && currentMissions[mission].payment > -20000) {
		k = -10128.0f + FloatAbs(currentMissions[mission].payment);
		for (i = 0; i < MaxSystems; i++) {
			if (k == gSystems[i].govt && legalStatus[i] < 0)
				legalStatus[i] = 0;
		}
	}
	if (currentMissions[mission].payment <= -20000) {
		if (currentMissions[mission].payment > -30000) {
			i = -20128.0f + FloatAbs(currentMissions[mission].payment);
			if (i >= 0 && i < MaxOutfitItemTypes) {
				outfitCount[i]++;
				SystemInfoToShipStats();
			}
		}
	}
	if (currentMissions[mission].payment <= -40001 && currentMissions[mission].payment > -40100) {
		i = -40000.0f + FloatAbs(currentMissions[mission].payment);
		shipData->credits -= shipData->credits * (0.01 * i);
	}
	lastMissionStellar = -1;
}

// FUNCTION: EVO 0x1004C300
void DoMissionFailure(short mission)
{
	short	i;
	short	j;

	for (i = 0; i <= 1; i++) {
		if (currentMissions[mission].failBitSet[i] >= 0 && currentMissions[mission].failBitSet[i] < NumMissionBits) {
			missionBit[currentMissions[mission].failBitSet[i]] = 1;
			for (j = 0; j < MaxDisasters; j++) {
				if (disasterData[j].missionBit == currentMissions[mission].failBitSet[i] && disasterData[j].duration > 0)
					disasterData[j].countdown = disasterData[j].duration;
			}
		}
		if (currentMissions[mission].failBitSet[i] >= kNotBitBase && currentMissions[mission].failBitSet[i] < kNotBitBase + NumMissionBits)
			missionBit[currentMissions[mission].failBitSet[i] - kNotBitBase] = 0;
	}
	if (currentMissions[mission].compGovt != -1) {
		for (j = 0; j < MaxSystems; j++) {
			if (gSystems[j].govt == currentMissions[mission].compGovt)
				legalStatus[j] -= currentMissions[mission].compReward / 2;
		}
	}
	currentMissionObjectives[mission].active = false;
	if (currentMissions[mission].failDesc != -1) {
		if (cursorHidden)
			ShowCursor();
		if (PlayMovieById(currentMissions[mission].failDesc, true)) {
			strncpy(missionDescText, "", 3);
			LoadDescription(missionDescText, currentMissions[mission].failDesc);
			MungeBriefing(0, mission);
			strncpy(gDialogText, missionDescText, 1023);
			ShowTextDialog(false, false);
		}
		RefreshSAT();
		if (cursorHidden)
			HideCursor();
		PlayMovieById(currentMissions[mission].failDesc, false);
	}
	if (currentMissions[mission].flags & kMisnRemoveOutfit) {
		if (currentMissions[mission].payment <= -30128) {
			j = -30128.0f + FloatAbs(currentMissions[mission].payment);
			if (j >= 0 && j < MaxOutfitItemTypes && outfitCount[j] > 0) {
				outfitCount[j] = 0;
				SystemInfoToShipStats();
			}
		}
	}
	AbortMission(mission);
	lastMissionStellar = -1;
}

// FUNCTION: EVO 0x1004C780
void AbortMission(short mission)
{
	short	i;
	Rect	r;
	Rect	r2;

	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && mission == shipData[i].specialMission) {
			shipData[i].specialMission = -1;
			if (shipData[i].parentShip != -1) {
				shipData[i].aiType = gShipTypes[shipData[i].type].inherentAI;
				shipData[i].parentShip = -1;
				AIMakeShipLeave(&shipData[i]);
			}
			if (portDialog != NULL)
				shipData[i].active = false;
		}
	}
	currentMissions[mission].cargoPickedUp = false;
	currentMissionObjectives[mission].active = false;
	lastMissionStellar = -1;
}

// FUNCTION: EVO 0x1004C908
void QuickMissionFailure(short mission)
{
	short	i;

	for (i = 0; i <= 1; i++) {
		if (currentMissions[mission].failBitSet[i] != -1) {
			if (currentMissions[mission].failBitSet[i] >= 0 && currentMissions[mission].failBitSet[i] < NumMissionBits)
				missionBit[currentMissions[mission].failBitSet[i]] = 1;
			if (currentMissions[mission].failBitSet[i] >= kNotBitBase && currentMissions[mission].failBitSet[i] < kNotBitBase + NumMissionBits)
				missionBit[currentMissions[mission].failBitSet[i] - kNotBitBase] = 0;
		}
	}
	currentMissionObjectives[mission].failed = true;
	if (currentMissions[mission].canAbort)
		AbortMission(mission);
	lastMissionStellar = -1;
}

// FUNCTION: EVO 0x1004CAD0
pascal Boolean MissionFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	short	old;
	char	key;
	short	button;
	Rect	r;
	Rect	r2;
	short	unused;
	Rect	box;
	short	itemType;
	Point	where;
	Cell	cell;
	Handle	item;

	QuickKeyCheckHandler();
	if (QuickKeyCheck(keyMap[kCtlMap])) {
		*itemHit = kBbsMapItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlPlayerInfo])) {
		*itemHit = kBbsPilotItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlMissions])) {
		*itemHit = kBbsMissionLogItem;
		return true;
	}
	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		key = tolower(key);
		if (key == kReturnCharCode || key == kEnterCharCode) {
			*itemHit = kBbsLeaveItem;
			return true;
		}
		if (key == 'l') {
			*itemHit = kBbsLeaveItem;
			return true;
		}
		if (key == 'a') {
			*itemHit = kOkItem;
			return true;
		}
		if (key == '	') {
			cell.h = 0;
			cell.v = selMissionListItem;
			if (selMissionListItem != -1)
				LSetSelect(false, cell, missionList);
			old = selMissionListItem;
			selMissionListItem++;
			if (missionGrid[inBar][selMissionListItem] == -1)
				selMissionListItem = 0;
			strncpy(missionDescText, "", 3);
			if (selMissionListItem != -1) {
				LoadDescription(missionDescText, missionGrid[inBar][selMissionListItem] + kMissionDescBase);
				MungeBriefing(1, missionGrid[inBar][selMissionListItem]);
				cell.v = selMissionListItem;
				LSetSelect(true, cell, missionList);
			}
			if (selMissionListItem != old) {
				GetDialogItem(missionDialog, kBbsTextItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(missionDialog, kBbsTitleItem, &itemType, &item, &box);
				InvalRect(&box);
			}
			cell.v = selMissionListItem;
			LSetSelect(true, cell, missionList);
		}
		if (key == kDownArrowCharCode && selMissionListItem + 1 < NumMissionBits && missionGrid[inBar][selMissionListItem + 1] != -1) {
			cell.h = 0;
			cell.v = selMissionListItem;
			if (selMissionListItem != -1)
				LSetSelect(false, cell, missionList);
			old = selMissionListItem;
			selMissionListItem++;
			if (missionGrid[inBar][selMissionListItem] == -1)
				selMissionListItem = -1;
			strncpy(missionDescText, "", 3);
			if (selMissionListItem != -1) {
				LoadDescription(missionDescText, missionGrid[inBar][selMissionListItem] + kMissionDescBase);
				MungeBriefing(1, missionGrid[inBar][selMissionListItem]);
				cell.v = selMissionListItem;
				LSetSelect(true, cell, missionList);
			}
			if (selMissionListItem != old) {
				GetDialogItem(missionDialog, kBbsTextItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(missionDialog, kBbsTitleItem, &itemType, &item, &box);
				InvalRect(&box);
			}
		}
		if (key == kUpArrowCharCode && selMissionListItem > 0) {
			cell.h = 0;
			cell.v = selMissionListItem;
			if (selMissionListItem != -1)
				LSetSelect(false, cell, missionList);
			old = selMissionListItem;
			selMissionListItem--;
			if (selMissionListItem < 0)
				selMissionListItem = -1;
			else if (missionGrid[inBar][selMissionListItem] == -1)
				selMissionListItem = -1;
			strncpy(missionDescText, "", 3);
			if (selMissionListItem != -1) {
				LoadDescription(missionDescText, missionGrid[inBar][selMissionListItem] + kMissionDescBase);
				MungeBriefing(1, missionGrid[inBar][selMissionListItem]);
				cell.v = selMissionListItem;
				LSetSelect(true, cell, missionList);
			}
			if (selMissionListItem != old) {
				GetDialogItem(missionDialog, kBbsTextItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(missionDialog, kBbsTitleItem, &itemType, &item, &box);
				InvalRect(&box);
			}
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		GetDialogItem(missionDialog, kBbsListItem, &itemType, &item, &box);
		if (PtInRect(where, &box)) {
			LClick(where, ev->modifiers, missionList);
			old = selMissionListItem;
			selMissionListItem = -1;
			cell.h = 0;
			cell.v = 0;
			while (cell.v < (*missionList)->dataBounds.bottom) {
				if (LGetSelect(false, &cell, missionList)) {
					selMissionListItem = cell.v;
					break;
				}
				cell.v++;
			}
			strncpy(missionDescText, "", 3);
			if (selMissionListItem != -1) {
				LoadDescription(missionDescText, missionGrid[inBar][selMissionListItem] + kMissionDescBase);
				MungeBriefing(1, missionGrid[inBar][selMissionListItem]);
			}
		if (selMissionListItem != old) {
			GetDialogItem(missionDialog, kBbsTextItem, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(missionDialog, kBbsTitleItem, &itemType, &item, &box);
			InvalRect(&box);
		}
		}
		GetDialogItem(missionDialog, kBbsScrollItem, &itemType, &item, &box);
		if (PtInRect(where, &box))
			LClick(where, ev->modifiers, missionList);
		button = TrackMissionButtons(where);
		if (button == 0) {
			*itemHit = kOkItem;
			return true;
		} else if (button == 1) {
			*itemHit = kBbsLeaveItem;
			return true;
		} else {
			*itemHit = -1;
			return true;
		}
	}
	if (ev->what == updateEvt) {
		SetPort(missionDialog);
		BeginUpdate(missionDialog);
		MissionDialogUpdate();
		EndUpdate(missionDialog);
	}
	return false;
}

// FUNCTION: EVO 0x1004D31C
void MissionDialogUpdate(void)
{
	Rect	r1;
	Rect	r2;
	Rect	box;
	Rect	box2;
	Str255	line;
	short	itemType;
	Handle	item;
	char	saved[1024];

	SATSetPort(&loadPort);
	ForeColor(blackColor);
	PaintRect(&missionDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&missionDialog->portRect);
	ForeColor(blackColor);
	RGBForeColor(&rgbDarkGrey);
	GetDialogItem(missionDialog, kBbsListItem, &itemType, &item, &box);
	GetDialogItem(missionDialog, kBbsScrollItem, &itemType, &item, &box2);
	UnionRect(&box, &box2, &box);
	InsetRect(&box, -2, -2);
	FrameRect(&box);
	ForeColor(blackColor);
	GetDialogItem(missionDialog, kOkItem, &itemType, &item, &box);
	if (RectInRgn(&box, missionDialog->visRgn)) {
		PenSize(3, 3);
		InsetRect(&box, -4, -4);
		FrameRoundRect(&box, 16, 16);
		PenSize(1, 1);
	}
	GetDialogItem(missionDialog, kBbsHeaderItem, &itemType, &item, &box);
	if (RectInRgn(&box, missionDialog->visRgn)) {
		ForeColor(blackColor);
		PaintRect(&box);
		TextFont(geneva);
		TextSize(kDialogTextSize);
		RGBForeColor(&rgbLightGrey);
		MoveTo(box.left, box.top + 12);
		DrawString("\pThe following missions are available here:");
		ForeColor(blackColor);
	}
	GetDialogItem(missionDialog, kBbsTitleItem, &itemType, &item, &box);
	if (RectInRgn(&box, missionDialog->visRgn)) {
		if (selMissionListItem != -1) {
			ForeColor(blackColor);
			TextFont(times);
			TextSize(18);
			strncpy((char *)line, (char *)missionName[missionGrid[inBar][selMissionListItem]], 250);
			p2cstr(line);
			strncpy(saved, missionDescText, 250);
			strncpy(missionDescText, (char *)line, 250);
			MungeBriefing(1, missionGrid[inBar][selMissionListItem]);
			strncpy((char *)line, missionDescText, 250);
			strncpy(missionDescText, saved, 250);
			TETextBox(line, strlen((char *)line), &box, teFlushDefault);
			InvertRect(&box);
		} else {
			ForeColor(blackColor);
			PaintRect(&box);
		}
	}
	GetDialogItem(missionDialog, kBbsTextItem, &itemType, &item, &box);
	if (RectInRgn(&box, missionDialog->visRgn)) {
		if (selMissionListItem != -1) {
			ForeColor(blackColor);
			TextFont(geneva);
			TextSize(kDialogTextSize);
			TETextBox(missionDescText, strlen(missionDescText), &box, teFlushDefault);
			InvertRect(&box);
		} else {
			ForeColor(blackColor);
			PaintRect(&box);
		}
	}
	RGBForeColor(&rgbDarkGrey);
	DateToMedString(gameTime, (char *)line);
	c2pstr((char *)line);
	MoveTo(missionDialog->portRect.left + 5, missionDialog->portRect.bottom - 6);
	DrawString(line);
	ForeColor(blackColor);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&missionDialog->portRect);
	ForeColor(blackColor);
	DrawMissionButtons(-1);
	SATSetPortScreen();
	SetPort(missionDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &missionDialog->portBits, &missionDialog->portRect, &missionDialog->portRect, srcCopy, missionDialog->visRgn);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	if (missionList != NULL)
		LUpdate(missionDialog->visRgn, missionList);
}

// FUNCTION: EVO 0x1004D8B8
Boolean IsMissionAvailable(short mission)
{
	Boolean	ok[9];
	short	i;
	long	dead;

	if (missionAvailData[mission].availStel <= -kShortBound)
		return false;
	if (missionAvailData[mission].availRandom <= 0)
		return false;
	if (missionAvailData[mission].availLoc == -1)
		return false;
	if (missionAvailData[mission].availLoc != kAvailLocShip) {
		if (drawGate)
			return false;
		if (inBar != 0 && missionAvailData[mission].availLoc == kAvailLocComputer)
			return false;
		if (inBar == 0 && missionAvailData[mission].availLoc == kAvailLocBar)
			return false;
	} else {
		if (!drawGate)
			return false;
	}
	for (i = 0; i < 9; i++)
		ok[i] = false;
	if (missionAvailData[mission].availStel == -1 || drawGate) {
		ok[0] = true;
	} else if (missionAvailData[mission].availStel >= FirstResourceID && missionAvailData[mission].availStel < kLinkSystBase) {
		if (shipData->targetStellar == missionAvailData[mission].availStel - FirstResourceID)
			ok[0] = true;
	} else if (missionAvailData[mission].availStel >= kLinkSystBase && missionAvailData[mission].availStel < kLinkGovtBase - 1) {
		for (i = 0; i < kMaxSystemLinks; i++) {
			if (gSystems[shipData->curSystem].links[i] == missionAvailData[mission].availStel - kLinkSystBase) {
				ok[0] = true;
				break;
			}
		}
	} else if (missionAvailData[mission].availStel >= kLinkGovtBase - 1 && missionAvailData[mission].availStel < kLinkAllyBase) {
		if (currentSpob->govt == missionAvailData[mission].availStel - kLinkGovtBase)
			ok[0] = true;
	} else if (missionAvailData[mission].availStel >= kLinkAllyBase && missionAvailData[mission].availStel < kLinkNotGovtBase) {
		if (currentSpob->govt != -1) {
			if (currentSpob->govt == govtData[missionAvailData[mission].availStel - kLinkAllyBase].ally)
				ok[0] = true;
			if (govtData[currentSpob->govt].ally == missionAvailData[mission].availStel - kLinkAllyBase)
				ok[0] = true;
		}
	} else if (missionAvailData[mission].availStel >= kLinkNotGovtBase && missionAvailData[mission].availStel < kLinkLimit) {
		if (currentSpob->govt != missionAvailData[mission].availStel - kLinkNotGovtBase)
			ok[0] = true;
	} else if (missionAvailData[mission].availStel >= kLinkEnemyBase && missionAvailData[mission].availStel < kLinkLimit && currentSpob->govt != -1) {
		if (currentSpob->govt == govtData[missionAvailData[mission].availStel - kLinkEnemyBase].enemy)
			ok[0] = true;
		if (govtData[currentSpob->govt].enemy == missionAvailData[mission].availStel - kLinkEnemyBase)
			ok[0] = true;
	}
	if (missionAvailData[mission].availBitSet == -1) {
		ok[1] = true;
	} else if (missionAvailData[mission].availBitSet >= 0 && missionAvailData[mission].availBitSet < NumMissionBits) {
		if (missionBit[missionAvailData[mission].availBitSet] != 0)
			ok[1] = true;
	} else if (missionAvailData[mission].availBitSet >= kNotBitBase && missionAvailData[mission].availBitSet < kNotBitBase + NumMissionBits
			&& missionBit[missionAvailData[mission].availBitSet - kNotBitBase] == 0) {
		ok[1] = true;
	}
	if (missionAvailData[mission].availBitClr == -1) {
		ok[6] = true;
	} else if (missionAvailData[mission].availBitClr >= 0 && missionAvailData[mission].availBitClr < NumMissionBits
			&& missionBit[missionAvailData[mission].availBitClr] == 0) {
		ok[6] = true;
	}
	if (missionAvailData[mission].availRecord == 0) {
		ok[2] = true;
	} else if (missionAvailData[mission].availRecord <= -kShortBound && !drawGate) {
		if (missionAvailData[mission].availRecord == -kShortBound && stellarData[shipData->targetStellar].dominated) {
			ok[2] = true;
		} else if (missionAvailData[mission].availRecord == -32001) {
			for (i = 0; i < MaxStellarObjects; i++) {
				if (stellarData[i].loaded && stellarData[i].dominated) {
					ok[2] = true;
					break;
				}
			}
		}
	} else if (missionAvailData[mission].availRecord > 0) {
		if (legalStatus[shipData->curSystem] >= missionAvailData[mission].availRecord)
			ok[2] = true;
	} else if (legalStatus[shipData->curSystem] <= missionAvailData[mission].availRecord) {
		ok[2] = true;
	}
	if (missionAvailData[mission].availRating <= 0)
		ok[3] = true;
	else if (playerKills >= missionAvailData[mission].availRating)
		ok[3] = true;
	if (missionAvailData[mission].availRandom >= 100) {
		ok[4] = true;
	} else if (randomOdds[mission] <= missionAvailData[mission].availRandom) {
		ok[4] = true;
	}
	if (drawGate) {
		if (missionAvailData[mission].cargoQty <= 0)
			ok[5] = true;
		else if (TotalMissionCargoSpace() < missionAvailData[mission].cargoQty)
			ok[5] = false;
		else
			ok[5] = true;
	} else {
		ok[5] = true;
	}
	if (gDebugB)
		ok[4] = true;
	if (missionAvailData[mission].availShipType >= FirstResourceID && missionAvailData[mission].availShipType <= 192) {
		if (shipData->type == missionAvailData[mission].availShipType - FirstResourceID)
			ok[7] = true;
	} else if (missionAvailData[mission].availShipType >= 1128 && missionAvailData[mission].availShipType <= 1192) {
		if (shipData->type != missionAvailData[mission].availShipType - 1128)
			ok[7] = true;
	} else if (missionAvailData[mission].availShipType >= 2128 && missionAvailData[mission].availShipType <= 2256) {
		if (gShipTypes[shipData->type].inherentGovt == missionAvailData[mission].availShipType - 2128)
			ok[7] = true;
	} else if (missionAvailData[mission].availShipType >= 3128 && missionAvailData[mission].availShipType <= 3256) {
		if (gShipTypes[shipData->type].inherentGovt != missionAvailData[mission].availShipType - 3128)
			ok[7] = true;
	} else {
		ok[7] = true;
	}
	ok[8] = true;
	if ((missionAvailData[mission].flags & kMisnNoFreighters) && gShipTypes[shipData->type].inherentAI <= kAIBraveTrader)
		ok[8] = false;
	if ((missionAvailData[mission].flags & kMisnNoFreighters) && gShipTypes[shipData->type].inherentAI >= kAIWarship)
		ok[8] = false;
	for (i = 0; i < 9; i++) {
		if (!ok[i])
			return false;
	}
	for (i = 0; i < MaxSimultaneousMissions; i++) {
		if (currentMissionObjectives[i].active && mission == currentMissions[i].misnIndex)
			return false;
	}
	return true;
}

// FUNCTION: EVO 0x1004E404
void MissionHandler(void)
{
	short	i;

	for (i = 0; i < MaxSimultaneousMissions; i++)
		MissionObjectivesCheck(i);
}

// FUNCTION: EVO 0x1004E44C
void MissionLandCheck(void)
{
	short	i;

	for (i = 0; i < MaxSimultaneousMissions; i++) {
		if (currentMissionObjectives[i].active && !currentMissionObjectives[i].failed) {
			MissionObjectivesCheck(i);
			MissionLandCargoCheck(i);
			if (shipData->targetStellar == currentMissions[i].returnStellar) {
				if (currentMissions[i].returnStellar == currentMissions[i].travelStellar)
					currentMissionObjectives[i].returnDone = true;
				if (currentMissions[i].travelStellar == -1)
					currentMissionObjectives[i].returnDone = true;
				if (currentMissionObjectives[i].failed)
					DoMissionFailure(i);
				else if (currentMissionObjectives[i].returnDone && currentMissionObjectives[i].objectivesDone)
					DoMissionSuccess(i);
			}
			MissionObjectivesCheck(i);
			if (currentMissionObjectives[i].failed && currentMissions[i].failDesc == -1)
				AbortMission(i);
		}
	}
}

// FUNCTION: EVO 0x1004E648
void MissionLandCargoCheck(short mission)
{
	if (shipData->targetStellar == currentMissions[mission].travelStellar) {
		currentMissionObjectives[mission].returnDone = true;
		if (currentMissions[mission].pickupMode == kPickupAtTravelStel && !currentMissions[mission].cargoPickedUp
				&& DoMissionCargoPickup(currentMissions[mission].cargoType, currentMissions[mission].cargoQty)) {
			currentMissions[mission].cargoPickedUp = true;
			if (currentMissions[mission].loadCargoDesc != -1) {
				if (cursorHidden)
					ShowCursor();
				if (PlayMovieById(currentMissions[mission].loadCargoDesc, true)) {
					strncpy(missionDescText, "", 3);
					LoadDescription(missionDescText, currentMissions[mission].loadCargoDesc);
					MungeBriefing(0, mission);
					strncpy(gDialogText, missionDescText, 1023);
					ShowTextDialog(false, false);
				}
				RefreshSAT();
				PlayMovieById(currentMissions[mission].loadCargoDesc, false);
			}
		}
		if (currentMissions[mission].dropoffMode == kDropoffAtTravelStel && currentMissions[mission].cargoPickedUp) {
			currentMissions[mission].cargoPickedUp = false;
			if (currentMissions[mission].dropCargoDesc != -1) {
				if (cursorHidden)
					ShowCursor();
				if (PlayMovieById(currentMissions[mission].dropCargoDesc, true)) {
					strncpy(missionDescText, "", 3);
					LoadDescription(missionDescText, currentMissions[mission].dropCargoDesc);
					MungeBriefing(0, mission);
					strncpy(gDialogText, missionDescText, 1023);
					ShowTextDialog(false, false);
				}
				RefreshSAT();
				PlayMovieById(currentMissions[mission].dropCargoDesc, false);
			}
			hudRedraw.credits = true;
		}
	}
	if (shipData->targetStellar == currentMissions[mission].returnStellar && currentMissions[mission].dropoffMode == kDropoffAtReturnStel) {
		currentMissions[mission].cargoPickedUp = false;
		if (currentMissions[mission].dropCargoDesc != -1) {
			if (cursorHidden)
				ShowCursor();
			if (PlayMovieById(currentMissions[mission].dropCargoDesc, true)) {
				strncpy(missionDescText, "", 3);
				LoadDescription(missionDescText, currentMissions[mission].dropCargoDesc);
				MungeBriefing(0, mission);
				strncpy(gDialogText, missionDescText, 1023);
				ShowTextDialog(false, false);
			}
			RefreshSAT();
			if (cursorHidden)
				HideCursor();
			PlayMovieById(currentMissions[mission].dropCargoDesc, false);
		}
		hudRedraw.credits = true;
	}
}

// FUNCTION: EVO 0x1004EAD4
void MissionObjectivesCheck(short mission)
{
	short	count;
	short	i;

	if (currentMissionObjectives[mission].active) {
		if (currentMissions[mission].shipGoal == kMisnGoalNone) {
			currentMissionObjectives[mission].objectivesDone = true;
		} else if (currentMissions[mission].initialShipCount > 0) {
			if (currentMissions[mission].specialShipsJumpedIn >= currentMissions[mission].initialShipCount) {
				if (currentMissions[mission].shipGoal == kMisnGoalDestroy && currentMissions[mission].specialShipsKilled >= currentMissions[mission].initialShipCount)
					currentMissionObjectives[mission].objectivesDone = true;
				if (currentMissions[mission].shipGoal == kMisnGoalDisable) {
					if (currentMissions[mission].specialShipsKilled > 0) {
						currentMissionObjectives[mission].failed = true;
					} else {
						if (currentMissions[mission].specialShipsDisabled >= currentMissions[mission].initialShipCount)
							currentMissionObjectives[mission].objectivesDone = true;
					}
				}
				if ((currentMissions[mission].shipGoal == kMisnGoalBoard || currentMissions[mission].shipGoal == kMisnGoalRescue)
						&& currentMissions[mission].specialShipsBoarded >= currentMissions[mission].initialShipCount)
					currentMissionObjectives[mission].objectivesDone = true;
				if (currentMissions[mission].shipGoal == kMisnGoalEscort) {
					if (currentMissions[mission].specialShipsKilled > 0 || currentMissions[mission].specialShipsDisabled > 0) {
						currentMissionObjectives[mission].failed = true;
					} else {
						count = 0;
						for (i = 1; i < MaxShipsInSystem; i++) {
							if (mission == shipData[i].specialMission && shipData[i].active)
								count++;
						}
						if (!count)
							currentMissionObjectives[mission].objectivesDone = false;
						else
							currentMissionObjectives[mission].objectivesDone = true;
					}
				}
				if (currentMissions[mission].shipGoal == kMisnGoalObserve
						&& (shipData->curSystem == currentMissions[mission].shipSyst || currentMissions[mission].shipSyst == kShipSystFollow)
						&& currentMissions[mission].specialShipsJumpedIn >= currentMissions[mission].shipCount)
					currentMissionObjectives[mission].objectivesDone = true;
				if (currentMissions[mission].shipGoal == kMisnGoalChaseOff) {
					if (currentMissions[mission].specialShipsKilled + currentMissions[mission].specialShipsJumpedOut >= currentMissions[mission].initialShipCount)
						currentMissionObjectives[mission].objectivesDone = true;
					else
						currentMissionObjectives[mission].objectivesDone = false;
				}
			} else {
				currentMissionObjectives[mission].objectivesDone = false;
			}
		} else {
			currentMissionObjectives[mission].objectivesDone = true;
		}
		if (!currentMissionObjectives[mission].failed && currentMissions[mission].timeLimit <= 0
				&& currentMissions[mission].timeLimit > -kShortBound && portDialog == 0) {
			currentMissionObjectives[mission].failed = true;
			PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
			DisplayComm("\pTime limit exceeded - mission failed.", 240, 0, 12, &rgbWhite, false, false);
			QuickMissionFailure(mission);
		}
	}
}

// FUNCTION: EVO 0x1004F078
void MungeBriefing(Boolean available, short mission)
{
	char	dead1[64];
	char	destStel[64];
	char	destSyst[64];
	char	retStel[64];
	char	retSyst[64];
	char	cargoName[64];
	char	cargoQty[64];
	char	shipNm[64];
	char	dueDate[64];
	char	pilot[64];
	char	shipStr[64];
	char	persName[64];
	Str255	numStr;
	short	i;
	short	textMax;
	short	idx;
	Str255	dead;

	if (!gDebugA) {
		descTextHandle = NULL;
		descTextHandle = NewHandle(strlen(missionDescText));
		if (descTextHandle != NULL) {
			BlockMoveData(missionDescText, *descTextHandle, GetHandleSize(descTextHandle));
			textMax = 1024;
			for (i = 0; i < textMax; i++)
				missionDescText[i] = 0;
			strncpy(destStel, "[Error]", 10);
			strncpy(destSyst, "[Error]", 10);
			strncpy(retStel, "[Error]", 10);
			strncpy(retSyst, "[Error]", 10);
			strncpy(cargoName, "[Error]", 10);
			strncpy(cargoQty, "[Error]", 10);
			strncpy(shipNm, "[Error]", 10);
			strncpy(dueDate, "[Error]", 10);
			strncpy(persName, "[Error]", 10);
			if (available) {
				idx = mission;
				if (idx != -1) {
					if (briefMissionInfo[idx].destStellar >= 0 && briefMissionInfo[idx].destStellar < MaxStellarObjects) {
						strncpy(destStel, (char *)stellarData[briefMissionInfo[idx].destStellar].name, 63);
						p2cstr((StringPtr)destStel);
					}
					if (briefMissionInfo[idx].destSystem >= 0 && briefMissionInfo[idx].destSystem < MaxSystems) {
						strncpy(destSyst, (char *)gSystems[briefMissionInfo[idx].destSystem].name, 63);
						p2cstr((StringPtr)destSyst);
					}
					if (briefMissionInfo[idx].returnStellar >= 0 && briefMissionInfo[idx].returnStellar < MaxStellarObjects) {
						strncpy(retStel, (char *)stellarData[briefMissionInfo[idx].returnStellar].name, 63);
						p2cstr((StringPtr)retStel);
					}
					if (briefMissionInfo[idx].returnSystem >= 0 && briefMissionInfo[idx].returnSystem < MaxSystems) {
						strncpy(retSyst, (char *)gSystems[briefMissionInfo[idx].returnSystem].name, 63);
						p2cstr((StringPtr)retSyst);
					}
					if (briefMissionInfo[idx].cargoType >= 0 && briefMissionInfo[idx].cargoType < MaxCargoTypes) {
						strncpy(cargoName, lcCargoName[briefMissionInfo[idx].cargoType], 63);
						p2cstr((StringPtr)cargoName);
						NumToString(briefMissionInfo[idx].cargoUnits, numStr);
						strncpy(cargoQty, (char *)numStr, 63);
						p2cstr((StringPtr)cargoQty);
					}
					if (gameTime.month != briefMissionInfo[idx].dueDate.month
							|| gameTime.day != briefMissionInfo[idx].dueDate.day
							|| gameTime.year != briefMissionInfo[idx].dueDate.year) {
						DateToLongString(briefMissionInfo[idx].dueDate, dueDate);
					}
				}
			} else {
				if (mission != -1 && currentMissionObjectives[mission].active) {
					if (currentMissions[mission].travelStellar >= 0 && currentMissions[mission].travelStellar < MaxStellarObjects) {
						strncpy(destStel, (char *)stellarData[currentMissions[mission].travelStellar].name, 63);
						p2cstr((StringPtr)destStel);
						strncpy(destSyst, (char *)gSystems[stellarData[currentMissions[mission].travelStellar].system].name, 63);
						p2cstr((StringPtr)destSyst);
					}
					if (currentMissions[mission].returnStellar >= 0 && currentMissions[mission].returnStellar < MaxStellarObjects) {
						strncpy(retStel, (char *)stellarData[currentMissions[mission].returnStellar].name, 63);
						p2cstr((StringPtr)retStel);
						strncpy(retSyst, (char *)gSystems[stellarData[currentMissions[mission].returnStellar].system].name, 63);
						p2cstr((StringPtr)retSyst);
					}
					if (currentMissions[mission].cargoType >= 0 && currentMissions[mission].cargoType < MaxCargoTypes) {
						strncpy(cargoName, lcCargoName[currentMissions[mission].cargoType], 63);
						p2cstr((StringPtr)cargoName);
						NumToString(currentMissions[mission].cargoQty, numStr);
						strncpy(cargoQty, (char *)numStr, 63);
						p2cstr((StringPtr)cargoQty);
					}
					if (gameTime.month != currentMissionObjectives[mission].dueDate.month
							|| gameTime.day != currentMissionObjectives[mission].dueDate.day
							|| gameTime.year != currentMissionObjectives[mission].dueDate.year) {
						DateToLongString(currentMissionObjectives[mission].dueDate, dueDate);
					}
					strncpy(shipNm, currentMissions[mission].shipName, 63);
					p2cstr((StringPtr)shipNm);
				}
			}
			if (speakerShip >= 0 && speakerShip < MaxShipsInSystem
					&& shipData[speakerShip].persType >= 0 && shipData[speakerShip].persType < MaxPersonTypes) {
				strncpy(persName, (char *)personData[shipData[speakerShip].persType].name, 63);
				p2cstr((StringPtr)persName);
			}
			strncpy(pilot, (char *)playerName, 63);
			p2cstr((StringPtr)pilot);
			strncpy(shipStr, (char *)shipName, 63);
			p2cstr((StringPtr)shipStr);
			MungeForSymbol("<DST>", destStel);
			MungeForSymbol("<DSY>", destSyst);
			MungeForSymbol("<RST>", retStel);
			MungeForSymbol("<RSY>", retSyst);
			MungeForSymbol("<CT>", cargoName);
			MungeForSymbol("<CQ>", cargoQty);
			MungeForSymbol("<SN>", shipNm);
			MungeForSymbol("<DL>", dueDate);
			MungeForSymbol("<PN>", pilot);
			MungeForSymbol("<PSN>", shipStr);
			MungeForSymbol("<OSN>", persName);
			if (GetHandleSize(descTextHandle) <= 1024UL)
				BlockMoveData(*descTextHandle, missionDescText, GetHandleSize(descTextHandle));
			else
				BlockMoveData(*descTextHandle, missionDescText, 1024);
			HPurge(descTextHandle);
			DisposeHandle(descTextHandle);
			descTextHandle = NULL;
		}
	}
}

// FUNCTION: EVO 0x1004FA08
void MungeForSymbol(char *find, char *replace)
{
	short	offset;
	short	findLen;
	short	replaceLen;
	long	unused[4];

	findLen = strlen(find);
	replaceLen = strlen(replace);
	offset = 0;
	do {
		offset = Munger(descTextHandle, offset, find, findLen, replace, replaceLen);
	} while (offset >= 0);
}

// FUNCTION: EVO 0x1004FA88
void DoMissionInfoDialog(void)
{
	Boolean	done;
	short	i;
	short	n;
	short	k;
	Rect	frame;
	short	itemHit;
	short	unused1;
	short	unused2;
	short	halfH;
	short	zero;
	short	savedStellar;
	Rect	box;
	Cell	cell;
	Str255	text;
	Str255	numStr;
	ModalFilterUPP	filterUPP;

	done = false;
	filterUPP = NewModalFilterProc(MissionInfoFilter);
	PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
	zero = 0;
	missionInfoPics[0] = GetPicture(kDone121ButtonPict);
	missionInfoPics[1] = GetPicture(kDone121ButtonPict + 1);
	missionInfoPics[2] = GetPicture(kAbortMissionButtonPict);
	missionInfoPics[3] = GetPicture(kAbortMissionButtonPict + 1);
	missionInfoDialog = NULL;
	missionInfoDialog = GetNewDialog(kMissionInfoDlogID, NULL, (WindowPtr)-1);
	if (missionInfoDialog != NULL) {
		NewDialogHook(missionInfoDialog, 0);
		CenterWindow(missionInfoDialog);
		ShowWindow(missionInfoDialog);
		SelectWindow(missionInfoDialog);
		SetPort(missionInfoDialog);
		FlushEvents(everyEvent - diskMask, 0);
		InvalRect(&missionInfoDialog->portRect);
		SetupMissionInfoList();
		missionInfoSel = 0;
		LoadDescription(missionDescText, currentMissions[missionInfoMap[missionInfoSel]].quickBriefDesc);
		MungeBriefing(0, missionInfoMap[missionInfoSel]);
		do {
			ModalDialog(filterUPP, &itemHit);
			if (itemHit == kOkItem)
				done = true;
			if (itemHit == kMinfoMapItem && missionDialog == NULL) {
				greenMissionSyst = -1;
				gMissionFlag = false;
				if (missionInfoSel != -1 && (currentMissions[missionInfoSel].flags & kMisnGreenArrow)) {
					if (currentMissions[missionInfoMap[missionInfoSel]].travelStellar != -1) {
						greenMissionSyst = stellarData[currentMissions[missionInfoMap[missionInfoSel]].travelStellar].system;
					} else {
						if (currentMissions[missionInfoMap[missionInfoSel]].returnStellar != -1)
							greenMissionSyst = stellarData[currentMissions[missionInfoMap[missionInfoSel]].returnStellar].system;
					}
				}
				savedStellar = shipData->targetStellar;
				DoSystemMap();
				if (missionDialog != NULL)
					shipData->targetStellar = savedStellar;
				if (portDialog != NULL) {
					SetPort(portDialog);
					InvalRect(&portDialog->portRect);
					PortDialogUpdate();
				}
				greenMissionSyst = -1;
				gMissionFlag = true;
				SetPort(missionInfoDialog);
				InvalRect(&missionInfoDialog->portRect);
				MissionInfoDialogUpdate();
			}
			if (itemHit == kMinfoAbortItem) {
				if (missionInfoSel == -1) {
					PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
					DrawMissionInfoButtons(-1);
				} else {
					if ((currentMissions[missionInfoMap[missionInfoSel]].flags & kMisnRemoveOutfit)
							&& currentMissions[missionInfoMap[missionInfoSel]].payment <= -30128) {
						i = -30128.0f + FloatAbs(currentMissions[missionInfoMap[missionInfoSel]].payment);
						if (i >= 0 && i < MaxOutfitItemTypes && outfitCount[i] > 0) {
							outfitCount[i] = 0;
							SystemInfoToShipStats();
						}
					}
					if ((currentMissions[missionInfoMap[missionInfoSel]].flags & kMisnAbortPenalty)
							&& currentMissions[missionInfoMap[missionInfoSel]].compGovt != -1) {
						for (i = 0; i < MaxSystems; i++) {
							if (gSystems[i].govt == currentMissions[missionInfoMap[missionInfoSel]].compGovt)
								legalStatus[i] -= currentMissions[missionInfoMap[missionInfoSel]].compReward * 5;
						}
					}
					AbortMission(missionInfoMap[missionInfoSel]);
					n = 0;
					for (i = 0; i < MaxSimultaneousMissions; i++) {
						if (currentMissionObjectives[i].active)
							n++;
					}
					if (n > 0) {
						if (missionInfoList != NULL)
							LDispose(missionInfoList);
						SetupMissionInfoList();
						missionInfoSel = -1;
						InvalRect(&missionInfoDialog->portRect);
						cell.h = cell.v = 0;
						while (cell.v < (*missionInfoList)->dataBounds.bottom) {
							LSetSelect(false, cell, missionInfoList);
							cell.v++;
						}
						for (i = 0; i < MaxSimultaneousMissions; i++)
							missionInfoMap[i] = -1;
						k = 0;
						for (i = 0; i < MaxSimultaneousMissions; i++) {
							if (currentMissionObjectives[i].active) {
								missionInfoMap[k] = i;
								k++;
							}
						}
					} else {
						done = true;
					}
					DrawStatusCargo();
					SetPort(missionInfoDialog);
				}
			}
		} while (!done);
		if (missionInfoList != NULL)
			LDispose(missionInfoList);
		n = (missionInfoDialog->portRect.right - missionInfoDialog->portRect.left) / 2;
		halfH = (missionInfoDialog->portRect.bottom - missionInfoDialog->portRect.top) / 2;
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(missionInfoDialog);
		missionInfoDialog = NULL;
		RefreshSAT();
		for (i = 0; i <= 3; i++) {
			if (missionInfoPics[i] != NULL) {
				HPurge((Handle)missionInfoPics[i]);
				ReleaseResource((Handle)missionInfoPics[i]);
			}
		}
		PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
	}
}

// FUNCTION: EVO 0x10050230
pascal Boolean MissionInfoFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	char	key;
	short	old;
	short	count;
	short	i;
		Rect	box;
	short	itemType;
	Point	where;
	Cell	cell;
	Handle	item;

	QuickKeyCheckHandler();
	if (QuickKeyCheck(keyMap[kCtlMap])) {
		*itemHit = kMinfoMapItem;
		return true;
	}
	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		if (key == kReturnCharCode || key == kEnterCharCode) {
			*itemHit = kOkItem;
			return true;
		}
		if (key == '	' || key == kDownArrowCharCode) {
			cell.h = 0;
			cell.v = missionInfoSel;
			if (missionInfoSel != -1)
				LSetSelect(false, cell, missionInfoList);
			old = missionInfoSel;
			count = 0;
			for (i = 0; i < MaxSimultaneousMissions; i++) {
				if (currentMissionObjectives[i].active)
					count++;
			}
			if (count > 0) {
				do {
					missionInfoSel++;
					if (missionInfoSel >= 8)
						missionInfoSel = 0;
				} while (missionInfoMap[missionInfoSel] == -1);
			}
			strncpy(missionDescText, "", 3);
			if (missionInfoSel != -1) {
				LoadDescription(missionDescText, currentMissions[missionInfoMap[missionInfoSel]].quickBriefDesc);
				MungeBriefing(0, missionInfoMap[missionInfoSel]);
				cell.v = missionInfoSel;
				LSetSelect(true, cell, missionInfoList);
			}
			if (missionInfoSel != old) {
				GetDialogItem(missionInfoDialog, kMinfoTextItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(missionInfoDialog, kMinfoAbortItem, &itemType, &item, &box);
				InvalRect(&box);
			}
		}
		if (key == kUpArrowCharCode) {
			cell.h = 0;
			cell.v = missionInfoSel;
			if (missionInfoSel != -1)
				LSetSelect(false, cell, missionInfoList);
			old = missionInfoSel;
			count = 0;
			for (i = 0; i < MaxSimultaneousMissions; i++) {
				if (currentMissionObjectives[i].active)
					count++;
			}
			if (count > 0) {
				do {
					missionInfoSel--;
					if (missionInfoSel < 0)
						missionInfoSel = 7;
				} while (missionInfoMap[missionInfoSel] == -1);
			}
			strncpy(missionDescText, "", 3);
			if (missionInfoSel != -1) {
				LoadDescription(missionDescText, currentMissions[missionInfoMap[missionInfoSel]].quickBriefDesc);
				MungeBriefing(0, missionInfoMap[missionInfoSel]);
				cell.v = missionInfoSel;
				LSetSelect(true, cell, missionInfoList);
			}
			if (missionInfoSel != old) {
				GetDialogItem(missionInfoDialog, kMinfoTextItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(missionInfoDialog, kMinfoAbortItem, &itemType, &item, &box);
				InvalRect(&box);
			}
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		GetDialogItem(missionInfoDialog, kMinfoListItem, &itemType, &item, &box);
		if (PtInRect(where, &box)) {
			LClick(where, ev->modifiers, missionInfoList);
			old = missionInfoSel;
			missionInfoSel = -1;
			cell.h = 0;
			cell.v = 0;
			while (cell.v < (*missionInfoList)->dataBounds.bottom) {
				if (LGetSelect(false, &cell, missionInfoList)) {
					missionInfoSel = cell.v;
					break;
				}
				cell.v++;
			}
			strncpy(missionDescText, "", 3);
			if (missionInfoSel != -1) {
				LoadDescription(missionDescText, currentMissions[missionInfoMap[missionInfoSel]].quickBriefDesc);
				MungeBriefing(0, missionInfoMap[missionInfoSel]);
			}
			if (missionInfoSel != old) {
				GetDialogItem(missionInfoDialog, kMinfoTextItem, &itemType, &item, &box);
				InvalRect(&box);
				GetDialogItem(missionInfoDialog, kMinfoAbortItem, &itemType, &item, &box);
				InvalRect(&box);
			}
		}
		count = TrackMissionInfoButtons(where);
		if (count == 0) {
			*itemHit = kOkItem;
			return true;
		} else if (count == 1) {
			*itemHit = kMinfoAbortItem;
			return true;
		} else {
			*itemHit = -1;
			return true;
		}
	}
	if (ev->what == updateEvt) {
		SetPort(missionInfoDialog);
		BeginUpdate(missionInfoDialog);
		MissionInfoDialogUpdate();
		EndUpdate(missionInfoDialog);
	}
	return false;
}

// FUNCTION: EVO 0x10050834
void MissionInfoDialogUpdate(void)
{
	Rect	box;
	short	itemType;
	short	unused1;
	short	unused2;
	short	unused3;
	Handle	item;
	Str255	line;

	SATSetPort(&loadPort);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	ForeColor(blackColor);
	PaintRect(&missionInfoDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&missionInfoDialog->portRect);
	ForeColor(blackColor);
	ForeColor(blackColor);
	GetDialogItem(missionInfoDialog, kMinfoListItem, &itemType, &item, &box);
	PaintRect(&box);
	RGBForeColor(&rgbDarkGrey);
	GetDialogItem(missionInfoDialog, kMinfoListItem, &itemType, &item, &box);
	InsetRect(&box, -2, -2);
	FrameRect(&box);
	ForeColor(blackColor);
	ForeColor(blackColor);
	GetDialogItem(missionInfoDialog, kMinfoHeaderItem, &itemType, &item, &box);
	RGBForeColor(&rgbMedGrey);
	MoveTo(box.left, box.bottom);
	DrawString("\pCurrently active missions:");
	ForeColor(blackColor);
	RGBForeColor(&rgbDarkGrey);
	DateToMedString(gameTime, (char *)line);
	c2pstr((char *)line);
	MoveTo(missionInfoDialog->portRect.right - StringWidth(line) - 5, box.bottom);
	DrawString(line);
	ForeColor(blackColor);
	GetDialogItem(missionInfoDialog, kMinfoTextItem, &itemType, &item, &box);
	if (RectInRgn(&box, missionInfoDialog->visRgn)) {
		if (missionInfoSel != -1) {
			ForeColor(blackColor);
			TextFont(geneva);
			TextSize(kDialogTextSize);
			TETextBox(missionDescText, strlen(missionDescText), &box, teFlushDefault);
			InvertRect(&box);
		} else {
			ForeColor(blackColor);
			PaintRect(&box);
		}
	}
	DrawMissionInfoButtons(-1);
	SATSetPortScreen();
	SetPort(missionInfoDialog);
	CopyBits(&loadPort.port->portBits, &missionInfoDialog->portBits, &missionInfoDialog->portRect, &missionInfoDialog->portRect, srcCopy, missionInfoDialog->visRgn);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	ForeColor(blackColor);
	if (missionInfoList != NULL)
		LUpdate(missionInfoDialog->visRgn, missionInfoList);
}

// FUNCTION: EVO 0x10050B7C
Boolean RunSingleMissionDialog(short mission)
{
	Boolean	done;
	Boolean	accepted;
	short	i;
	short	resBit;
	short	resDesc;
	Rect	box;
	short	itemHit;
	short	pad1;
	short	pad2;
	short	savedStellar;
	short	pad3;
	short	pad4;
	ModalFilterUPP	filterUPP;
	Str255	buf;

	accepted = false;
	done = false;
	filterUPP = NewModalFilterProc(SingleMissionFilter);
	if (mission < 0 || mission >= NumMissionBits)
		return;
	if (missionAvailData[mission].flags & kMisnCantRefuse) {
		canRefuseMission = false;
		for (i = 0; i <= 1; i++)
			missionPics[i] = GetPicture(i + kOkayButtonPict);
		for (i = 2; i <= 3; i++)
			missionPics[i] = NULL;
	} else {
		canRefuseMission = true;
		for (i = 0; i <= 3; i++)
			missionPics[i] = GetPicture(i + kYesButtonPict);
	}
	missionDialog = NULL;
	missionDialog = GetNewDialog(kMissionOfferDlogID, NULL, (WindowPtr)-1);
	if (missionDialog == NULL)
		return;
	NewDialogHook(missionDialog, 0);
	CenterWindow(missionDialog);
	ShowWindow(missionDialog);
	SelectWindow(missionDialog);
	SetPort(missionDialog);
	FlushEvents(everyEvent - diskMask, 0);
	ForeColor(blackColor);
	InvalRect(&missionDialog->portRect);
	strncpy(missionDescText, "", 3);
	LoadDescription(missionDescText, mission + kMissionDescBase);
	MungeBriefing(1, mission);
	do {
		ModalDialog(filterUPP, &itemHit);
		if (canRefuseMission) {
			if (itemHit == kOkItem) {
				/* BUG: event kinds passed as masks; only the low event bits are flushed */
				FlushEvents(keyDown, 0);
				FlushEvents(autoKey, 0);
				FlushEvents(mouseDown, 0);
				FlushEvents(mouseUp, 0);
				HideWindow(missionDialog);
				RefreshSAT();
				accepted = DoMissionAccept(mission);
				SingleMissionDialogUpdate();
				LoadDescription(missionDescText, mission + kMissionDescBase);
				MungeBriefing(1, mission);
				box = gSAT.wind.bounds;
				box.right -= kStatusPanelWidth;
				RefreshSAT();
				SetPort(missionDialog);
				done = true;
			}
			if (itemHit == kOfferRefuseItem) {
				misnHandle = (MisnResHandle)GetResource(kMisnType, mission + FirstResourceID);
				if (misnHandle != NULL) {
					HLock((Handle)misnHandle);
					resDesc = (*misnHandle)->refuseText;
					if (GetHandleSize((Handle)misnHandle) < sizeof(MisnResource))
						resBit = -1;
					else
						resBit = (*misnHandle)->refuseBitSet;
					HUnlock((Handle)misnHandle);
					HPurge((Handle)misnHandle);
					ReleaseResource((Handle)misnHandle);
					if (resDesc != -1) {
						HideWindow(missionDialog);
						RefreshSAT();
						if (PlayMovieById(resDesc, true)) {
							strncpy(missionDescText, "", 3);
							LoadDescription(missionDescText, resDesc);
							MungeBriefing(0, mission);
							strncpy(gDialogText, missionDescText, 1023);
							ShowTextDialog(false, false);
						}
						RefreshSAT();
						PlayMovieById(resDesc, false);
						if (resBit >= 0 && resBit < NumMissionBits)
							missionBit[resBit] = 1;
						else if (resBit >= kNotBitBase && resBit < kNotBitBase + NumMissionBits)
							missionBit[resBit - kNotBitBase] = 0;
					}
				}
				done = true;
			}
		} else {
			if (itemHit == kOkItem || itemHit == kOfferRefuseItem) {
				/* BUG: event kinds passed as masks; only the low event bits are flushed */
				FlushEvents(keyDown, 0);
				FlushEvents(autoKey, 0);
				FlushEvents(mouseDown, 0);
				FlushEvents(mouseUp, 0);
				accepted = DoMissionAccept(mission);
				LoadDescription(missionDescText, mission + kMissionDescBase);
				MungeBriefing(1, mission);
				SingleMissionDialogUpdate();
				done = true;
			}
		}
		if (itemHit == kOfferMapItem) {
			/* BUG: event kinds passed as masks; only the low event bits are flushed */
			FlushEvents(keyDown, 0);
			FlushEvents(autoKey, 0);
			FlushEvents(mouseDown, 0);
			FlushEvents(mouseUp, 0);
			greenMissionSyst = -1;
			gMissionFlag = true;
			if (missionAvailData[mission].flags & kMisnGreenArrow) {
				if (briefMissionInfo[mission].destStellar != -1) {
					greenMissionSyst = stellarData[briefMissionInfo[mission].destStellar].system;
				} else {
					if (briefMissionInfo[mission].returnStellar != -1)
						greenMissionSyst = stellarData[briefMissionInfo[mission].returnStellar].system;
				}
			}
			savedStellar = shipData->targetStellar;
			DoSystemMap();
			shipData->targetStellar = savedStellar;
			greenMissionSyst = -1;
			if (!drawGate) {
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
				PortDialogUpdate();
			}
			SetPort(missionDialog);
			InvalRect(&missionDialog->portRect);
		}
		if (itemHit == kOfferPilotItem) {
			/* BUG: event kinds passed as masks; only the low event bits are flushed */
			FlushEvents(keyDown, 0);
			FlushEvents(autoKey, 0);
			FlushEvents(mouseDown, 0);
			FlushEvents(mouseUp, 0);
			DoPlayerInfoDialog();
			if (!drawGate) {
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
				PortDialogUpdate();
			}
			SetPort(missionDialog);
			InvalRect(&missionDialog->portRect);
		}
		if (itemHit == kOfferMissionLogItem) {
			/* BUG: event kinds passed as masks; only the low event bits are flushed */
			FlushEvents(keyDown, 0);
			FlushEvents(autoKey, 0);
			FlushEvents(mouseDown, 0);
			FlushEvents(mouseUp, 0);
			DoMissionInfoDialog();
			LoadDescription(missionDescText, mission + kMissionDescBase);
			MungeBriefing(1, mission);
			if (!drawGate) {
				SetPort(portDialog);
				InvalRect(&portDialog->portRect);
				PortDialogUpdate();
			}
			SetPort(missionDialog);
			InvalRect(&missionDialog->portRect);
		}
	} while (!done);
	for (i = 0; i <= 3; i++) {
		if (missionPics[i] != NULL) {
			HPurge((Handle)missionPics[i]);
			ReleaseResource((Handle)missionPics[i]);
		}
	}
	DisposeRoutineDescriptor(filterUPP);
	DisposeDialog(missionDialog);
	missionDialog = NULL;
	RefreshSAT();
	return accepted;
}

// FUNCTION: EVO 0x100513E4
pascal Boolean SingleMissionFilter(DialogPtr dlg, EventRecord *ev, short *itemHit)
{
	short	button;
	char	key;
	Rect	r1;
	Point	where;
	Rect	r2;

	QuickKeyCheckHandler();
	if (QuickKeyCheck(keyMap[kCtlMap])) {
		*itemHit = kOfferMapItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlPlayerInfo])) {
		*itemHit = kOfferPilotItem;
		return true;
	}
	if (QuickKeyCheck(keyMap[kCtlMissions])) {
		*itemHit = kOfferMissionLogItem;
		return true;
	}
	if (ev->what == keyDown || ev->what == autoKey) {
		key = ev->message & charCodeMask;
		key = tolower(key);
		if (key == kReturnCharCode || key == kEnterCharCode || key == 'y' || key == 'o') {
			*itemHit = kOkItem;
			return true;
		}
		if (key == 'n' && canRefuseMission) {
			*itemHit = kOfferRefuseItem;
			return true;
		}
	}
	if (ev->what == mouseDown) {
		where = ev->where;
		GlobalToLocal(&where);
		button = TrackSingleMissionButtons(where);
		if (button == 0) {
			*itemHit = kOkItem;
			return true;
		} else if (button == 1) {
			*itemHit = kOfferRefuseItem;
			return true;
		} else {
			*itemHit = -1;
			return true;
		}
	}
	if (ev->what == updateEvt) {
		SetPort(missionDialog);
		BeginUpdate(missionDialog);
		SingleMissionDialogUpdate();
		EndUpdate(missionDialog);
	}
	return false;
}

// FUNCTION: EVO 0x100515D8
void SingleMissionDialogUpdate(void)
{
	Rect	box;
	short	itemType;
	short	unused1;
	short	unused2;
	short	unused3;
	Handle	item;
	Str255	unused;

	SATSetPort(&loadPort);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	ForeColor(blackColor);
	PaintRect(&missionDialog->portRect);
	RGBForeColor(&rgbMedGrey);
	FrameRect(&missionDialog->portRect);
	ForeColor(blackColor);
	GetDialogItem(missionDialog, kOfferTextItem, &itemType, &item, &box);
	if (RectInRgn(&box, missionDialog->visRgn)) {
		ForeColor(blackColor);
		TextFont(geneva);
		TextSize(kDialogTextSize);
		TETextBox(missionDescText, strlen(missionDescText), &box, teFlushDefault);
		InvertRect(&box);
	}
	DrawSingleMissionButtons(-1);
	SATSetPortScreen();
	SetPort(missionDialog);
	CopyBits(&loadPort.port->portBits, &missionDialog->portBits, &missionDialog->portRect, &missionDialog->portRect, srcCopy, missionDialog->visRgn);
}

// FUNCTION: EVO 0x1005174C
Boolean ValidAuxShipSystem(short syst, short mission)
{
	short	s;
	short	i;

	s = currentMissions[mission].auxShipSyst;
	if ((s == kAuxSystPlayer || s == -6) && syst == shipData->curSystem)
		return true;
	if (s == kAuxSystTravel && currentMissions[mission].travelStellar != -1
			&& syst == stellarData[currentMissions[mission].travelStellar].system)
		return true;
	if (s == kAuxSystReturn && currentMissions[mission].returnStellar != -1
			&& syst == stellarData[currentMissions[mission].returnStellar].system)
		return true;
	if (s >= FirstResourceID && s < 1128 && syst == s - FirstResourceID)
		return true;
	if (s >= kLinkSystBase && s < kLinkGovtBase) {
		if (syst == s - kLinkSystBase)
			return true;
		for (i = 0; i < kMaxSystemLinks; i++) {
			if (syst == gSystems[s - kLinkSystBase].links[i])
				return true;
		}
	}
	if (s >= kLinkGovtBase - 1 && s < kLinkAllyBase && gSystems[syst].govt == s - kLinkGovtBase)
		return true;
	if (s >= kLinkAllyBase && s < kLinkNotGovtBase && gSystems[syst].govt >= 0) {
		if (gSystems[syst].govt == s - kLinkAllyBase)
			return true;
		if (govtData[gSystems[syst].govt].ally != -1 && govtData[s - kLinkAllyBase].ally != -1) {
			if (govtData[gSystems[syst].govt].ally == s - kLinkAllyBase)
				return true;
			if (govtData[s - kLinkAllyBase].ally == gSystems[syst].govt)
				return true;
		}
	}
	if (s >= kLinkNotGovtBase && s < kLinkEnemyBase && gSystems[syst].govt != s - kLinkNotGovtBase)
		return true;
	if (s >= kLinkEnemyBase && s < kLinkLimit && gSystems[syst].govt >= 0) {
		if ((govtData[s - kLinkEnemyBase].flags & kGovtXenophobic) && gSystems[syst].govt != s - kLinkEnemyBase
				&& govtData[gSystems[syst].govt].ally != s - kLinkEnemyBase
				&& govtData[s - kLinkEnemyBase].ally != gSystems[syst].govt)
			return true;
		if (govtData[gSystems[syst].govt].enemy != -1 && govtData[s - kLinkEnemyBase].enemy != -1) {
			if (govtData[gSystems[syst].govt].enemy == s - kLinkEnemyBase)
				return true;
			if (govtData[s - kLinkEnemyBase].enemy == gSystems[syst].govt)
				return true;
		}
	}
	return false;
}

// FUNCTION: EVO 0x10051C90
void AutoAbortMission(short mission)
{
	short	i;
	short	j;

	for (i = 0; i <= 3; i++) {
		if (currentMissions[mission].compBitSet[i] != -1) {
			if (currentMissions[mission].compBitSet[i] >= 0 && currentMissions[mission].compBitSet[i] < NumMissionBits) {
				missionBit[currentMissions[mission].compBitSet[i]] = 1;
				for (j = 0; j < MaxDisasters; j++) {
					if (disasterData[j].missionBit == currentMissions[mission].compBitSet[i] && disasterData[j].duration > 0)
						disasterData[j].countdown = disasterData[j].duration;
				}
			}
			if (currentMissions[mission].compBitSet[i] >= kNotBitBase && currentMissions[mission].compBitSet[i] < kNotBitBase + NumMissionBits)
				missionBit[currentMissions[mission].compBitSet[i] - kNotBitBase] = 0;
		}
	}
	AbortMission(mission);
}

// FUNCTION: EVO 0x10051E9C
Boolean SystemVisible(short syst)
{
	if (!gSystems[syst].loaded)
		return false;
	if (gSystems[syst].visibility != -1) {
		if (gSystems[syst].visibility < NumMissionBits && gSystems[syst].visibility >= 0) {
			if (!missionBit[gSystems[syst].visibility])
				return false;
		} else if (gSystems[syst].visibility >= kNotBitBase && gSystems[syst].visibility < kNotBitBase + NumMissionBits) {
			if (missionBit[gSystems[syst].visibility - kNotBitBase])
				return false;
		}
	}
	return true;
}
