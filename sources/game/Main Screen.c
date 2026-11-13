/* Main Screen.c -- the title screen: main menu loop, about box, prefs and new-pilot dialogs. Notes: Research/notes/Main Screen.md */
#define OLDROUTINENAMES 1
#include <Types.h>
#include <Memory.h>
#include <Quickdraw.h>
#include <QDOffscreen.h>
#include <Events.h>
#include <Menus.h>
#include <Windows.h>
#include <Dialogs.h>
#include <Fonts.h>
#include <Devices.h>
#include <Resources.h>
#include <DiskInit.h>
#include <ToolUtils.h>
#include <TextUtils.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define EVO_IMPLICIT_RAND
#include "evo.h"
long	Rand(short range);



#define kVictoryPictID	8011
#define kInterfacePictID	8000
#define kTitleOverlayPictID	8002
#define kIntroScreenPictID	8200
#define kRowRevealPictBase	8300
#define kAppleMenuID	1000
#define kNewPilotNameDlogID	3100
#define kMenuBarID	128
#define kMenuBarHeight	20
#define kStdPicWidth	640
#define kStdPicHeight	480
#define kPromptStrList	128
#define kMainNewButton		0
#define kMainEnterButton	1
#define kMainOpenButton		2
#define kMainPrefsButton	3
#define kMainQuitButton		4
#define kMainAboutButton	5
#define kMainAboutButton2	6

#define kFileMenuID			128
#define kAboutMenuItem		1
#define kQuitMenuItem		1

#define kNameOkItem			1
#define kNameStrictItem		2
#define kNameTextItem		5
#define kNameCancelItem		6

#define kVolumeStep	32
#define kIntroTextID	20000
#define kAboutTextID	20001
#define kOptionAboutTextID	20002
#define kCommandAboutTextID	20003
#define kMainClickSndID		600
#define kCreditsChimeSndID	3000
#define kIntroEndSndID		30003
#define kIntroClutID		1001

#define kPrefsDialogID		4001
#define iPrefsOK			1
#define iUseQuickdraw		2
#define iFirstKey			3
#define iLastKey			33
#define iKeysPict			34
#define iPrefsCancel		35
#define iVolumeText			37
#define iVolumeDown			38
#define iVolumeUp			39
#define iIntroMusic			40
#define iNoMovies			41
#define iGameSpeed			42
#define iNoStreaks			43
#define kKeyNamesID			129
#define kKeysPictID			132
#define kVolumePictID		134
#define kMaxVolume			8

#define kSpeedDialogID		4002
#define iSpeedOK			1
#define iSpeedCancel		2
#define iSpeedSlider		4
#define kSliderPictID		400
#define kSliderMax			175


extern Boolean			gCheatSoundPlayed;


PicHandle		interfacePic;
PicHandle		keyPic;
PicHandle		victoryPic;
PicHandle		prefPic[5];
PicHandle		sliderPic[2];
short			keyDialogSel;
short			newSoundVol;
short			rolloverFrame;
short			rolloverIndex;
short			sliderPos;
DialogPtr		prefsDialog;
DialogPtr		speedDialog;
Rect			rolloverRects[6];
Rect			buttonRects[6];
Rect			standardPicRect;
Rect			interfacePicRect;
Boolean			inBackground;
Boolean			didSpeak;
Boolean			doMainScreenAnim;
EventRecord		gEvent;
RgnHandle		titleRgns[2];
Ptr				mainScreenSnd[4];
unsigned long	lastRolloverAnimTime;
Boolean			mainScreenBackgroundDirty;

static pascal void MainScreenMouse(EventRecord *event);
static pascal void MainScreenKey(char key, char message, short modifiers);
static pascal void MainScreenUpdate(Boolean toScreen);
static pascal void MainScreenActivate(Boolean activate);
static void MainScreenIdle(void);

// FUNCTION: EVO 0x10042628
void DoAboutBox(void)
{
	Boolean confirmed;

	StopIntroMusic(true);
	if (interfacePic != NULL) {
		HPurge((Handle)interfacePic);
		ReleaseResource((Handle)interfacePic);
		interfacePic = NULL;
	}
	if (KeyCheck(kKeyOption)) {
		LoadAndStartSoundPair(kDefaultLoopID);
		confirmed = ShowScrollingText(kOptionAboutTextID);
		FadeScreenOut(16, &rgbBlack);
		StopAndDisposeSoundPair();
		if (confirmed && KeyCheck(kKeyF) && KeyCheck(kKeyControl) && KeyCheck(kKeyCommand))
			PlayCreditsChime();
	} else if (KeyCheck(kKeyCommand)) {
		ShowScrollingText(kCommandAboutTextID);
		FadeScreenOut(16, &rgbBlack);
	} else if (KeyCheck(kKeyShift) && !didSpeak) {
		CheckSpeechMgr();
		SpeakIt("Greetings professor Follkinn. Would you like to play a game?", 1);
		didSpeak = true;
		return;
	} else {
		LoadAndStartSoundPair(kDefaultLoopID);
		ShowScrollingText(kAboutTextID);
		FadeScreenOut(16, &rgbBlack);
		StopAndDisposeSoundPair();
	}
	SATSetPortScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
	InvalRect(&gSAT.wind.bounds);
	FlushEvents(everyEvent - diskMask, 0);
	MainScreenUpdate(false);
	DrawClosedButtons();
	AnimateRowReveal();
	SetupInterfaceBackground();
	SATSetPortScreen();
	InvalRect(&gSAT.wind.bounds);
}

// FUNCTION: EVO 0x10042864
void PlayVictoryAnimation(void)
{
	Rect destRect;
	Rect picRect;
	unsigned long ticks;
	short i;

	PlaySound(klaxonSnd, 1, kMidVolume, kMidVolume);
	victoryPic = GetPicture(kVictoryPictID);
	SATSetPort(&loadPort);
	picRect = gSAT.wind.bounds;
	CenterPict(victoryPic, &picRect);
	DrawPicture(victoryPic, &picRect);
	SATSetPortScreen();
	InsetRect(&picRect, 0, 190);
	destRect = picRect;
	for (i = 0; i <= 9; i++) {
		ticks = TickCount();
		InsetRect(&destRect, 0, -19);
		InsetRect(&picRect, 0, -19);
		CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &picRect, &destRect, srcCopy, NULL);
		while (TickCount() < ticks + 1)
			;
	}
	while (!Button())
		;
	PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
	HPurge((Handle)victoryPic);
	ReleaseResource((Handle)victoryPic);
	SATSetPortScreen();
	FlushEvents(everyEvent - diskMask, 0);
	InvalRect(&destRect);
}

// FUNCTION: EVO 0x10042a1c
short TrackMainScreenButtons(Point where)
{
	Rect hitRects[7];
	Rect unusedRect;
	Rect destRects[6];
	Rect srcRects[6];
	Rect box;
	Point mouse;
	short i;
	short over;
	short hit;

	SATSetPortScreen();
	for (i = 0; i <= 5; i++) {
		hitRects[i] = buttonRects[i];
		destRects[i] = hitRects[i];
	}
	for (i = 0; i <= 4; i += 2)
		destRects[i].left = destRects[i].right - 87;
	OffsetRect(&destRects[2], 0, -1);
	OffsetRect(&destRects[3], 0, -1);
	for (i = 1; i <= 5; i += 2)
		destRects[i].right = destRects[i].left + 87;
	SetRect(&srcRects[0], 0, 0, 87, 59);
	SetRect(&srcRects[1], 87, 0, 174, 59);
	srcRects[2] = srcRects[4] = srcRects[0];
	srcRects[3] = srcRects[5] = srcRects[1];
	OffsetRect(&srcRects[2], 0, 59);
	OffsetRect(&srcRects[3], 0, 59);
	OffsetRect(&srcRects[4], 0, 118);
	OffsetRect(&srcRects[5], 0, 118);
	SetRect(&hitRects[6], standardPicRect.left + 95, standardPicRect.top + 70, standardPicRect.left + 547, standardPicRect.top + 160);
	hit = -1;
	for (i = 0; i <= 6; i++)
		if (PtInRect(where, &hitRects[i]))
			hit = i;
	if (hit != -1) {
		if (hit >= 0 && hit <= 5)
			CopyBits(&gSAT.backScreen.port->portBits, &gSAT.wind.port->portBits,
					 &srcRects[hit], &destRects[hit], srcCopy, NULL);
		while (StillDown()) {
			MainScreenRolloverDraw();
			GetMouse(&mouse);
			over = -1;
			for (i = 0; i <= 6; i++)
				if (PtInRect(mouse, &hitRects[i]))
					over = i;
			if (over != hit) {
				if (over <= 5 && over >= 0)
					CopyBits(&gSAT.backScreen.port->portBits, &gSAT.wind.port->portBits,
							 &srcRects[over], &destRects[over], srcCopy, NULL);
				if (hit <= 5 && hit >= 0)
					CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits,
							 &destRects[hit], &destRects[hit], srcCopy, NULL);
			}
			hit = over;
		}
	}
	InvalRect(&gSAT.wind.bounds);
	return hit;
}

// FUNCTION: EVO 0x10042e50
void InitMainScreen(void)
{
	interfacePic = GetPicture(kInterfacePictID);
	if (interfacePic == NULL) {
		SATSetPortScreen();
		ForeColor(blackColor);
		PaintRect(&gSAT.wind.bounds);
		GraphicsEmergencyBail();
	}
	FlushEvents(keyDownMask, 0);
	titleRgns[0] = NewRgn();
	titleRgns[1] = NewRgn();
	didSpeak = false;
	onPlanet = false;
	doMainScreenAnim = true;
	standardPicRect.left = (gSAT.wind.bounds.left + gSAT.wind.bounds.right) / 2 - kStdPicWidth / 2;
	standardPicRect.top = (gSAT.wind.bounds.top + gSAT.wind.bounds.bottom) / 2 - kStdPicHeight / 2;
	standardPicRect.right = standardPicRect.left + kStdPicWidth;
	standardPicRect.bottom = standardPicRect.top + kStdPicHeight;
	interfacePicRect = gSAT.wind.bounds;
	CenterPict(interfacePic, &interfacePicRect);
}

// FUNCTION: EVO 0x10042f9c
void DoMainScreen(void)
{
	Rect picRect;
	Rect rects[7];
	short count;
	Str31 str;
	long hours;
	Rect r;
	Point where;
	Boolean gotEvent;

	FadeScreenOut(16, &rgbBlack);
	if (gameInProgress) {
		DeleteAllComputerShips(false, true);
		SetupShipsInSystem(shipData->curSystem);
		CreateAsteroids();
		ExploreNebulaeFromSystem(shipData->curSystem);
		HandleStellarSystemVisibility();
		onPlanet = false;
		killShots = killBoxes = killExplods = killSmoke = killAsteroids = true;
	}
	if (abs(gSAT.wind.bounds.top - (**gDevice).gdRect.top) <= kMenuBarHeight) {
		SATSetPortScreen();
		rects[0] = gSAT.wind.bounds;
		rects[0].top -= kMenuBarHeight;
		RectRgn(gSAT.wind.port->clipRgn, &rects[0]);
		RectRgn(gSAT.wind.port->visRgn, &rects[0]);
		ForeColor(blackColor);
		PaintRect(&rects[0]);
		InvalRect(&rects[0]);
	}
	InitShareWareRegistrationSession(true, 2);
	SetMonitorClut(gameClut, false);
	SetupInterfaceBackground();
	inBackground = false;
	if (cursorHidden) {
		cursorHidden = false;
		ShowCursor();
	}
	mainScreenSnd[1] = LoadSndResource(kMainClickSndID);
	picRect = interfacePicRect;
	SATSetPort(&loadPort);
	DrawPicture(interfacePic, &picRect);
	SATSetPortScreen();
	FlushEvents(everyEvent - diskMask, 0);
	SetMonitorClut(gameClut, false);
	DrawClosedButtons();
	CheckShareWareRegistrationMatch(&gRegistered);
	if (!gRegistered)
		ShowNagDialog(true);
	CheckShareWareRegistrationMatch(&gRegistered);
	GetHoursUsed(&hours);
	gDaysUsed = hours / 5.0;
	AnimateRowReveal();
	SetupInterfaceBackground();
	InitTitleRects();
	SATSetPortScreen();
	InvalRect(&picRect);
	while (StillDown())
		;
	/* BUG: event kinds passed as masks; only the low event bits are flushed */
	FlushEvents(keyDown, 0);	/* BUG: an event code, not a mask */
	while (!gQuit) {
		if (mainScreenBackgroundDirty)
			MainScreenUpdate(true);
		gotEvent = WaitNextEvent(everyEvent, &gEvent, 60, NULL);
		if (!gotEvent && gEvent.what == nullEvent)
			MainScreenIdle();
		else switch (gEvent.what) {
		case mouseDown:
			MainScreenMouse(&gEvent);
			break;
		case keyDown:
		case autoKey:
			MainScreenKey(gEvent.message & charCodeMask, gEvent.message, (short)gEvent.modifiers);
			break;
		case updateEvt:
			SATSetPortScreen();
			MainScreenUpdate(true);
			break;
		case diskEvt:
			if (HiWord(gEvent.message) != noErr) {
				where.v = where.h = 100;
				DILoad();
				DIBadMount(where, gEvent.message);
				DIUnload();
			}
			break;
		case osEvt:
			if (gEvent.message & (long)suspendResumeMessage << 24) {
				if (gEvent.message & resumeFlag)
					MainScreenActivate(true);
				else {
					MainScreenActivate(false);
					InitCursor();
				}
			}
			break;
		}
	}
}

// FUNCTION: EVO 0x100433dc
void InitMenuBar(void)
{
	Handle menuBar;
	MenuHandle appleMenu;

	menuBar = NULL;
	menuBar = GetNewMBar(kMenuBarID);
	if (menuBar != NULL) {
		SetMenuBar(menuBar);
		appleMenu = NULL;
		appleMenu = GetMenuHandle(kAppleMenuID);
		if (appleMenu != NULL)
			AppendResMenu(appleMenu, 'DRVR');
	}
}

// FUNCTION: EVO 0x10043458
static pascal void MainScreenMouse(EventRecord *event)
{
	WindowPtr window;
	long refCon;
	short modifiers;
	Point where;
	unsigned long when;
	short part;
	short button;
	Boolean ok;
	short i;
	short j;

	where = event->where;
	when = event->when;
	modifiers = (short)event->modifiers;
	part = FindWindow(where, &window);
	switch (part) {
	case inDesk:
		SysBeep(1);
		break;
	case inMenuBar:
		InitCursor();
		DoMenuClick(MenuSelect(event->where));
		break;
	case inSysWindow:
		SystemClick(event, window);
		break;
	case inContent:
		button = TrackMainScreenButtons(where);
		if (button == kMainNewButton) {
			PlaySound(mainScreenSnd[1], 10, kMidVolume, kMidVolume);
			if (gameInProgress) {
				ok = EVYesNoDialog("\pThere\325s already a pilot file loaded. Are you sure you want to create a new one?");
				CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits,
						 &gSAT.wind.bounds, &gSAT.wind.bounds, srcCopy, NULL);
			} else
				ok = true;
			if (ok) {
				DoNewPilot();
				SATSetPortScreen();
				InvalRect(&gSAT.wind.bounds);
			}
		}
		if (button == kMainOpenButton) {
			PlaySound(mainScreenSnd[1], 10, kMidVolume, kMidVolume);
			ok = OpenPilotFile();
			if (ok) {
				gameInProgress = true;
				InvalRect(&gSAT.wind.bounds);
				DeleteAllComputerShips(false, true);
				SetupShipsInSystem(shipData->curSystem);
				HandleStellarSystemVisibility();
				CreateAsteroids();
				for (i = 0; i < MaxSystems; i++)
					if (gSystems[i].loaded && gSystems[i].visited > 0)
						ExploreNebulaeFromSystem(i);
				for (j = 0; j < MaxSimultaneousMissions; j++) {
					if (currentMissionObjectives[j].active) {
						/* BUG: indexes the missions with i (MaxSystems after the loop above), not j */
						if (currentMissions[i].flags & kMisnInfiniteAux)
							currentMissions[i].auxShipsLeft = currentMissions[i].auxShipCount;
						if (currentMissions[i].auxShipsLeft > 0) {
							currentMissions[i].auxShipDelay = (short)Rand(kMissionSpawnDelay) + kMissionSpawnDelay;
							currentMissions[i].auxShipsSpawned = 0;
						}
					}
				}
				onPlanet = false;
				killShots = killBoxes = killExplods = killSmoke = killAsteroids = true;
				SATSkip();
			}
		}
		if (button == kMainQuitButton) {
			PlaySound(mainScreenSnd[1], 10, kMidVolume, kMidVolume);
			Delay(15, &tix);
			gQuit = true;
		}
		if (button == kMainEnterButton) {
			if (gameInProgress) {
				PlaySound(mainScreenSnd[1], 1, kMidVolume, kMidVolume);
				if (!IsDying(shipData))
					EnterGameFromMainScreen();
				else
					SysBeep(0);
			} else
				PlaySound(beepSnd[kBeepDenied], 10, kMidVolume, kMidVolume);
		}
		if (button == kMainPrefsButton) {
			PlaySound(mainScreenSnd[1], 10, kMidVolume, kMidVolume);
			DoPrefsDialog();
			SATSetPortScreen();
			MainScreenUpdate(true);
			SATSetPortScreen();
			InvalRect(&gSAT.wind.bounds);
		}
		if (button == kMainAboutButton || button == kMainAboutButton2) {
			PlaySound(mainScreenSnd[1], 10, kMidVolume, kMidVolume);
			DoAboutBox();
		}
		break;
	}
}

// FUNCTION: EVO 0x10043900
static pascal void MainScreenKey(char key, char message, short modifiers)
{
	Point where;
	EventRecord event;

	where.h = where.v = 1;
	key = tolower(key);
	if (key == 'n') {
		where.h = buttonRects[0].left + 1;
		where.v = buttonRects[0].top + 1;
	}
	if (key == 'o' && !KeyCheck(kKeyS) && !KeyCheck(kKeyL)) {
		where.h = buttonRects[2].left + 1;
		where.v = buttonRects[2].top + 1;
	}
	if (key == 'q') {
		where.h = buttonRects[4].left + 1;
		where.v = buttonRects[4].top + 1;
	}
	if (key == 'e') {
		where.h = buttonRects[1].left + 1;
		where.v = buttonRects[1].top + 1;
	}
	if (key == 'p') {
		where.h = buttonRects[3].left + 1;
		where.v = buttonRects[3].top + 1;
	}
	if (key == 'a') {
		where.h = buttonRects[5].left + 1;
		where.v = buttonRects[5].top + 1;
	}
	if (key == 'x')
		PlayVictoryAnimation();
	else {
		event.where = where;
		event.when = TickCount();
		event.modifiers = modifiers;
		MainScreenMouse(&event);
	}
}

// FUNCTION: EVO 0x10043a8c
static pascal void MainScreenUpdate(Boolean toScreen)
{
	Rect infoRect;
	Rect picRect;
	long value;
	Str31 numStr;
	Str255 dateStr;
	Str255 nameStr;
	Str255 tempStr;

	SATSetPort(&loadPort);
	BeginUpdate(gSAT.wind.port);
	ForeColor(blackColor);
	PaintRect(&loadPort.bounds);
	picRect = interfacePicRect;
	if (interfacePic == NULL)
		interfacePic = GetPicture(kInterfacePictID);
	if (interfacePic != NULL)
		DrawPicture(interfacePic, &picRect);
	else {
		SATSetPortScreen();
		ForeColor(blackColor);
		PaintRect(&gSAT.wind.bounds);
		GraphicsEmergencyBail();
	}
	SetRect(&infoRect, standardPicRect.left + 277, standardPicRect.top + 266, standardPicRect.left + 369, standardPicRect.top + 446);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	if (gameInProgress) {
		if (!IsDying(shipData)) {
			MoveTo(infoRect.left, infoRect.top + 12);
			RGBForeColor(&dimGreen);
			DrawString("\pPilot Name:");
			MoveTo(infoRect.left + 5, infoRect.top + 24);
			RGBForeColor(&brightGreen);
			DrawString(playerName);
			MoveTo(infoRect.left, infoRect.top + 39);
			RGBForeColor(&dimGreen);
			DrawString("\pShip Name:");
			MoveTo(infoRect.left + 5, infoRect.top + 51);
			RGBForeColor(&brightGreen);
			DrawString(shipName);
			MoveTo(infoRect.left, infoRect.top + 66);
			RGBForeColor(&dimGreen);
			DrawString("\pShip Type:");
			MoveTo(infoRect.left + 5, infoRect.top + 78);
			RGBForeColor(&brightGreen);
			DrawString(shipyardName[shipData->type]);
			RGBForeColor(&dimGreen);
			MoveTo(infoRect.left, infoRect.top + 93);
			DrawString("\pLegal status in");
			MoveTo(infoRect.left, infoRect.top + 105);
			DrawString(gSystems[shipData->curSystem].name);
			DrawString("\p system:");
			MoveTo(infoRect.left + 5, infoRect.top + 117);
			RGBForeColor(&brightGreen);
			if (SystemIsInhabited(shipData->curSystem))
				DrawLegalStatusString(shipData->curSystem);
			else
				DrawString("\pN/A");
			MoveTo(infoRect.left, infoRect.top + 132);
			RGBForeColor(&dimGreen);
			DrawString("\pCombat Rating:");
			MoveTo(infoRect.left + 5, infoRect.top + 144);
			RGBForeColor(&brightGreen);
			DrawCombatRatingString();
			MoveTo(infoRect.left, infoRect.top + 159);
			RGBForeColor(&dimGreen);
			DrawString("\pCurrent Date:");
			MoveTo(infoRect.left + 5, infoRect.top + 171);
			RGBForeColor(&brightGreen);
			DateToMedString(gameTime, (char *)dateStr);
			c2pstr((char *)dateStr);
			DrawString(dateStr);
		} else {
			RGBForeColor(&dimGreen);
			if (playerName[1] == 'K' && playerName[2] == 'e' && playerName[3] == 'n'
					&& playerName[4] == 'n' && playerName[5] == 'y') {
				DrawCenteredText("\pOh my God!", infoRect.left, infoRect.right,
								   (infoRect.top + infoRect.bottom) / 2 - 12);
				DrawCenteredText("\pThey killed Kenny!", infoRect.left, infoRect.right,
								   (infoRect.top + infoRect.bottom) / 2);
			} else {
				DrawCenteredText(playerName, infoRect.left, infoRect.right,
								   (infoRect.top + infoRect.bottom) / 2 - 12);
				DrawCenteredText("\phas been killed", infoRect.left, infoRect.right,
								   (infoRect.top + infoRect.bottom) / 2);
			}
		}
	} else {
		RGBForeColor(&dimGreen);
		DrawCenteredText("\pNo Pilot File Loaded", infoRect.left, infoRect.right,
						   (infoRect.top + infoRect.bottom) / 2 - 20);
	}
	ForeColor(blackColor);
	SATSetPortScreen();
	ForeColor(blackColor);
	if (toScreen)
		CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits,
				 &loadPort.bounds, &gSAT.wind.bounds, srcCopy, NULL);
	EndUpdate(gSAT.wind.port);
}

// FUNCTION: EVO 0x10044048
static pascal void MainScreenActivate(Boolean activate)
{
	GrafPtr savePort;

	inBackground = !activate;
	if (activate) {
		SetMasterVolume(settings.soundVolume * kVolumeStep);
		SwitchedIn();
		SetCursor(&qd.arrow);
		SATSetPortScreen();
		ShowTheMenuBar();
		mBarHidden = false;
		ShowWindow(gSAT.wind.port);
		SelectWindow(gSAT.wind.port);
		SATSetPortScreen();
		InvalRect(&gSAT.wind.bounds);
	} else {
		ShowTheMenuBar();
		SwitchedOut();
		HideWindow(gSAT.wind.port);
	}
}

// FUNCTION: EVO 0x10044130
static void MainScreenIdle(void)
{
	Rect menuRect;
	Point mouse;

	if (cursorHidden) {
		ShowCursor();
		cursorHidden = false;
	}
	menuRect = gSAT.wind.bounds;
	menuRect.bottom = menuRect.top + kMenuBarHeight;
	GetMouse(&mouse);
	if (PtInRect(mouse, &menuRect)) {
		if (mBarHidden) {
			ShowTheMenuBar();
			mBarHidden = false;
		}
	} else if (!mBarHidden && !inBackground) {
		InvalRect(&menuRect);
		HideTheMenuBar();
		mBarHidden = true;
	}
	if (KeyCheck(kKeyS) && KeyCheck(kKeyO) && KeyCheck(kKeyL)) {
		if (!gCheatSoundPlayed)
			PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
		gCheatSoundPlayed = true;
	}
	MainScreenRolloverDraw();
	if (!inBackground)
		ColorCheck();
}

// FUNCTION: EVO 0x100442ac
void DoMenuClick(long menuResult)
{
	short menuID;
	short item;
	Str255 name;

	menuID = HiWord(menuResult);
	item = LoWord(menuResult);
	switch (menuID) {
	case kAppleMenuID:
		if (item == kAboutMenuItem)
			DoAboutBox();
		else {
			GetMenuItemText(GetMenuHandle(kAppleMenuID), item, name);
			OpenDeskAcc(name);
		}
		break;
	case kFileMenuID:
		switch (item) {
		case kQuitMenuItem:
			gQuit = true;
			break;
		}
		break;
	}
	HiliteMenu(0);
}

// FUNCTION: EVO 0x10044370
void SetupInterfaceBackground(void)
{
	Rect r;
	PicHandle pic;

	SATSetPort(&loadPort);
	r = loadPort.bounds;
	ForeColor(blackColor);
	PaintRect(&r);
	SATSetPortScreen();
	pic = GetPicture(kTitleOverlayPictID);
	if (pic != NULL) {
		SATSetPortBackScreen();
		ForeColor(blackColor);
		PaintRect(&gSAT.backScreen.bounds);
		SetRect(&r, 0, 0, 174, 177);
		DrawPicture(pic, &r);
		HPurge((Handle)pic);
		ReleaseResource((Handle)pic);
	}
	SATSetPortScreen();
}

// FUNCTION: EVO 0x1004445c
void DoPrefsDialog(void)
{
	RunPrefsDialog();
}

// FUNCTION: EVO 0x10044480
void RunPrefsDialog(void)
{
	Rect itemRect;
	short itemHit;
	short oldVolume;
	short itemType;
	Handle itemHandle;
	Boolean voicesOn;
	Boolean highDetail;
	ModalFilterUPP filterUPP;
	ControlHandle ctl;
	Boolean conflict;
	short j;
	short i;
	Boolean introMusic;
	Boolean noMovies;
	Boolean useQuickdraw;
	Boolean noStreaks;
	Boolean done;

	done = false;
	(void)prefPic;
	filterUPP = NewModalFilterProc(KeyFilter);
	keyDialogSel = 0;
	SaveKeyAssignments();
	newSoundVol = settings.soundVolume;
	oldVolume = settings.soundVolume;
	introMusic = settings.introMusic;
	voicesOn = settings.voicesOn;
	highDetail = settings.highDetail;
	noMovies = settings.noMovies;
	useQuickdraw = settings.useQuickdraw;
	noStreaks = settings.noStreaks;
	sliderPos = gameSpeed * 100.0 + -50.0;
	keyPic = GetPicture(kKeysPictID);
	for (i = 0; i <= 4; i++)
		prefPic[i] = GetPicture(i + kVolumePictID);
	prefsDialog = NULL;
	prefsDialog = GetNewDialog(kPrefsDialogID, NULL, (WindowPtr)-1);
	if (prefsDialog == NULL)
		return;
	NewDialogHook(prefsDialog, gSAT.wind.port);
	ShowWindow(prefsDialog);
	SelectWindow(prefsDialog);
	SetPort(prefsDialog);
	GetDialogItem(prefsDialog, iUseQuickdraw, &itemType, &itemHandle, &itemRect);
	ctl = (ControlHandle)itemHandle;
	if (useQuickdraw)
		SetControlValue(ctl, 1);
	else
		SetControlValue(ctl, 0);
	GetDialogItem(prefsDialog, iIntroMusic, &itemType, &itemHandle, &itemRect);
	ctl = (ControlHandle)itemHandle;
	if (introMusic)
		SetControlValue(ctl, 1);
	else
		SetControlValue(ctl, 0);
	GetDialogItem(prefsDialog, iNoMovies, &itemType, &itemHandle, &itemRect);
	ctl = (ControlHandle)itemHandle;
	if (!noMovies)
		SetControlValue(ctl, 1);
	else
		SetControlValue(ctl, 0);
	GetDialogItem(prefsDialog, iNoStreaks, &itemType, &itemHandle, &itemRect);
	ctl = (ControlHandle)itemHandle;
	if (noStreaks)
		SetControlValue(ctl, 1);
	else
		SetControlValue(ctl, 0);
	FlushEvents(everyEvent - diskMask, 0);
	GetDialogItem(prefsDialog, iVolumeText, &itemType, &itemHandle, &itemRect);
	InvalRect(&itemRect);
	do {
		ModalDialog(filterUPP, &itemHit);
		if (itemHit == iPrefsOK) {
			conflict = false;
			for (j = 0; j <= kNumKeys - 1; j++) {
				if (DupKeyCheck(newKeyPrefs[j], j)) {
					GetDialogItem(prefsDialog, keyDialogSel + iFirstKey, &itemType, &itemHandle, &itemRect);
					InvalRect(&itemRect);
					keyDialogSel = j;
					GetDialogItem(prefsDialog, keyDialogSel + iFirstKey, &itemType, &itemHandle, &itemRect);
					InvalRect(&itemRect);
					SysBeep(0);
					conflict = true;
					break;
				}
			}
			if (!conflict) {
				LoadKeyAssignments();
				settings.soundVolume = newSoundVol;
				settings.introMusic = introMusic;
				settings.voicesOn = voicesOn;
				settings.noMovies = noMovies;
				settings.useQuickdraw = useQuickdraw;
				settings.noStreaks = noStreaks;
				gameSpeed = (sliderPos + 50) / 100.0;
				done = true;
				SavePrefs();
			}
		}
		if (itemHit == iPrefsCancel)
			done = true;
		if (itemHit >= iFirstKey && itemHit <= iLastKey) {
			GetDialogItem(prefsDialog, keyDialogSel + iFirstKey, &itemType, &itemHandle, &itemRect);
			InvalRect(&itemRect);
			keyDialogSel = itemHit - iFirstKey;
			GetDialogItem(prefsDialog, keyDialogSel + iFirstKey, &itemType, &itemHandle, &itemRect);
			InvalRect(&itemRect);
		}
		if (itemHit == iVolumeUp) {
			GetDialogItem(prefsDialog, iVolumeUp, &itemType, &itemHandle, &itemRect);
			DrawPicture(prefPic[2], &itemRect);
			Delay(5, &tix);
			DrawPicture(prefPic[0], &itemRect);
			if (newSoundVol < kMaxVolume) {
				GetDialogItem(prefsDialog, iVolumeText, &itemType, &itemHandle, &itemRect);
				InvalRect(&itemRect);
				newSoundVol++;
				SetMasterVolume(newSoundVol * kVolumeStep);
				PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
			} else
				SysBeep(0);
		}
		if (itemHit == iVolumeDown) {
			GetDialogItem(prefsDialog, iVolumeDown, &itemType, &itemHandle, &itemRect);
			DrawPicture(prefPic[3], &itemRect);
			Delay(5, &tix);
			DrawPicture(prefPic[1], &itemRect);
			if (newSoundVol > 0) {
				GetDialogItem(prefsDialog, iVolumeText, &itemType, &itemHandle, &itemRect);
				InvalRect(&itemRect);
				newSoundVol--;
				SetMasterVolume(newSoundVol * kVolumeStep);
				PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
			} else
				SysBeep(0);
		}
		if (itemHit == iUseQuickdraw) {
			useQuickdraw = !useQuickdraw;
			GetDialogItem(prefsDialog, iUseQuickdraw, &itemType, &itemHandle, &itemRect);
			ctl = (ControlHandle)itemHandle;
			if (useQuickdraw)
				SetControlValue(ctl, 1);
			else
				SetControlValue(ctl, 0);
		}
		if (itemHit == iIntroMusic) {
			introMusic = !introMusic;
			GetDialogItem(prefsDialog, iIntroMusic, &itemType, &itemHandle, &itemRect);
			ctl = (ControlHandle)itemHandle;
			if (introMusic)
				SetControlValue(ctl, 1);
			else
				SetControlValue(ctl, 0);
		}
		if (itemHit == iNoMovies) {
			noMovies = !noMovies;
			GetDialogItem(prefsDialog, iNoMovies, &itemType, &itemHandle, &itemRect);
			ctl = (ControlHandle)itemHandle;
			if (!noMovies)
				SetControlValue(ctl, 1);
			else
				SetControlValue(ctl, 0);
		}
		if (itemHit == iGameSpeed) {
			DoGameSpeedDialog();
			SetPort(prefsDialog);
			InvalRect(&prefsDialog->portRect);
		}
		if (itemHit == iNoStreaks) {
			noStreaks = !noStreaks;
			GetDialogItem(prefsDialog, iNoStreaks, &itemType, &itemHandle, &itemRect);
			ctl = (ControlHandle)itemHandle;
			if (noStreaks)
				SetControlValue(ctl, 1);
			else
				SetControlValue(ctl, 0);
		}
	} while (!done);
	if (!settings.introMusic)
		StopIntroMusic(false);
	SATSetPortScreen();
	for (i = 0; i <= 4; i++) {
		if (prefPic[i] != NULL) {
			HPurge((Handle)prefPic[i]);
			ReleaseResource((Handle)prefPic[i]);
		}
	}
	if (keyPic != NULL) {
		HPurge((Handle)keyPic);
		ReleaseResource((Handle)keyPic);
	}
	DisposeRoutineDescriptor(filterUPP);
	DisposeDialog(prefsDialog);
	SetupInterfaceBackground();
}

// FUNCTION: EVO 0x10044d68
pascal Boolean KeyFilter(DialogPtr theDialog, EventRecord *event, short *itemHit)
{
	Rect itemRect;
	Rect box;
	Point mouse;
	short i;
	short itemType;
	long choice;
	Point where;
	Point pt;
	Handle itemHandle;
	short key;
	short oldSel;

	if (event->what == mouseDown) {
		SetPort(prefsDialog);
		where = event->where;
		GlobalToLocal(&where);
	}
	if (event->what == keyDown || event->what == nullEvent || event->what == updateEvt) {
		key = FirstKeyDown();
		if (key != -1 && keyDialogSel != -1) {
			newKeyPrefs[keyDialogSel] = key;
			oldSel = keyDialogSel;
			keyDialogSel = (keyDialogSel + 1) % kNumKeys;
			GetDialogItem(prefsDialog, oldSel + iFirstKey, &itemType, &itemHandle, &itemRect);
			InvalRect(&itemRect);
			GetDialogItem(prefsDialog, keyDialogSel + iFirstKey, &itemType, &itemHandle, &itemRect);
			InvalRect(&itemRect);
			while (KeyCheck(key))
				;
			return true;
		}
	}
	if (event->what == updateEvt) {
		SetPort(prefsDialog);
		BeginUpdate(prefsDialog);
		KeyDialogUpdate();
		DrawDialog(prefsDialog);
		EndUpdate(prefsDialog);
	}
	return false;
}

// FUNCTION: EVO 0x10044ef4
void KeyDialogUpdate(void)
{
	Rect itemRect;
	Rect inner;
	Rect picRect;
	short itemType;
	Str255 name;
	Handle itemHandle;
	short i;

	SATSetPort(&loadPort);
	ForeColor(blackColor);
	TextFont(systemFont);
	TextSize(12);
	GetDialogItem(prefsDialog, iKeysPict, &itemType, &itemHandle, &picRect);
	DrawPicture(keyPic, &picRect);
	for (i = iFirstKey; i <= iLastKey; i++) {
		GetDialogItem(prefsDialog, i, &itemType, &itemHandle, &itemRect);
		if (RectInRgn(&itemRect, prefsDialog->visRgn)) {
			if (keyDialogSel + iFirstKey == i) {
				inner = itemRect;
				InsetRect(&inner, 1, 1);
				ForeColor(whiteColor);
				FrameRect(&itemRect);
				InsetRect(&inner, 1, 1);
				ForeColor(blackColor);
				PaintRect(&inner);
				ForeColor(blackColor);
				FrameRect(&itemRect);
				ForeColor(whiteColor);
				MoveTo(itemRect.left + 5, itemRect.top + 15);
			} else {
				inner = itemRect;
				InsetRect(&inner, 1, 1);
				ForeColor(whiteColor);
				PaintRect(&inner);
				ForeColor(blackColor);
				FrameRect(&itemRect);
				ForeColor(blackColor);
				MoveTo(itemRect.left + 5, itemRect.top + 15);
			}
			GetIndString(name, kKeyNamesID, newKeyPrefs[i - iFirstKey] + 1);
			if (name[0] == 0)
				DrawString("\pUnknown");
			else
				DrawString(name);
		}
	}
	GetDialogItem(prefsDialog, iVolumeText, &itemType, &itemHandle, &itemRect);
	SetDialogItemText(itemHandle, volumeNames[newSoundVol]);
	ForeColor(blackColor);
	SATSetPortScreen();
	SetPort(prefsDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &prefsDialog->portBits, &picRect, &picRect, srcCopy, prefsDialog->visRgn);
}

// FUNCTION: EVO 0x100451f0
void DoGameSpeedDialog(void)
{
	Rect itemRect;
	Rect thumbRect;
	short itemHit;
	short itemType;
	Handle itemHandle;
	long value;
	Point mouse;
	short i;
	Boolean done;
	short oldPos;

	done = false;
	oldPos = sliderPos;
	for (i = 0; i <= 1; i++) {
		sliderPic[i] = NULL;
		sliderPic[i] = GetPicture(i + kSliderPictID);
		if (sliderPic[i] != NULL)
			HNoPurge((Handle)sliderPic[i]);
	}
	speedDialog = NULL;
	speedDialog = GetNewDialog(kSpeedDialogID, NULL, (WindowPtr)-1);
	if (speedDialog == NULL)
		return;
	NewDialogHook(speedDialog, gSAT.wind.port);
	ShowWindow(speedDialog);
	SelectWindow(speedDialog);
	SetPort(speedDialog);
	DrawDialog(speedDialog);
	FrameDefaultButton(speedDialog, iSpeedOK);
	DrawGameSpeedSlider();
	do {
		ModalDialog(NULL, &itemHit);
		if (itemHit == iSpeedOK)
			done = true;
		if (itemHit == iSpeedCancel) {
			sliderPos = oldPos;
			done = true;
		}
		if (itemHit == iSpeedSlider && StillDown()) {
			GetDialogItem(speedDialog, iSpeedSlider, &itemType, &itemHandle, &itemRect);
			thumbRect = itemRect;
			thumbRect.top = itemRect.bottom - 14;
			thumbRect.bottom = thumbRect.top + 11;
			itemRect.top = itemRect.bottom - 18;
			GetMouse(&mouse);
			if (PtInRect(mouse, &itemRect)) {
				sliderPos = mouse.h - thumbRect.left - 5;
				if (sliderPos < 0)
					sliderPos = 0;
				if (sliderPos > kSliderMax)
					sliderPos = kSliderMax;
				DrawGameSpeedSlider();
				SetPort(speedDialog);
				while (StillDown()) {
					GetMouse(&mouse);
					sliderPos = mouse.h - thumbRect.left - 5;
					if (sliderPos < 0)
						sliderPos = 0;
					if (sliderPos > kSliderMax)
						sliderPos = kSliderMax;
					DrawGameSpeedSlider();
					SetPort(speedDialog);
				}
			}
		}
	} while (!done);
	for (i = 0; i < 1; i++) {	/* BUG: releases only the first of the two slider pictures */
		if (sliderPic[i] != NULL) {
			HPurge((Handle)sliderPic[i]);
			ReleaseResource((Handle)sliderPic[i]);
		}
	}
	DisposeDialog(speedDialog);
}

// FUNCTION: EVO 0x10045504
void DrawGameSpeedSlider(void)
{
	Rect sliderRect;
	Rect r;
	short itemType;
	Str255 numStr;
	Handle itemHandle;

	SetPort(prefsDialog);
	SATSetPort(&loadPort);
	ForeColor(blackColor);
	TextFont(geneva);
	TextSize(kDialogTextSize);
	GetDialogItem(speedDialog, iSpeedSlider, &itemType, &itemHandle, &sliderRect);
	ForeColor(whiteColor);
	PaintRect(&sliderRect);
	ForeColor(blackColor);
	r = sliderRect;
	r.top = sliderRect.bottom - 11;
	r.bottom = r.top + 5;
	if (sliderPic[0] != NULL)
		DrawPicture(sliderPic[0], &r);
	else {
		ForeColor(blackColor);
		FrameRect(&r);
	}
	r.top = sliderRect.bottom - 17;
	r.bottom = sliderRect.bottom;
	r.left = sliderRect.left + sliderPos;
	r.right = r.left + 9;
	if (sliderPic[1] != NULL)
		DrawPicture(sliderPic[1], &r);
	else {
		ForeColor(blueColor);
		PaintRect(&r);
		ForeColor(blackColor);
	}
	ForeColor(blackColor);
	MoveTo(sliderRect.left, sliderRect.bottom - 25);
	DrawString("\pGame Speed:");
	NumToString(sliderPos + 50, numStr);
	MoveTo(sliderRect.right - (StringWidth(numStr) + StringWidth("\p%")), sliderRect.bottom - 25);
	DrawString(numStr);
	DrawString("\p%");
	SATSetPortScreen();
	SetPort(speedDialog);
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &speedDialog->portBits, &sliderRect, &sliderRect, srcCopy, NULL);
}

// FUNCTION: EVO 0x10045778
void EnterGameFromMainScreen(void)
{
	Rect r;
	long grow;
	short i;

	HideTheCursor();
	if (seenIntroScreen)
		StopIntroMusic(true);
	else
		StopIntroMusic(false);
	SetMasterVolume(settings.soundVolume * kVolumeStep);
	for (i = 1; i <= 3; i++) {
		if (mainScreenSnd[i] != NULL) {
			FlushMixQueueEntries((long)mainScreenSnd[i]);
			DisposePtr(mainScreenSnd[i]);
			mainScreenSnd[i] = NULL;
		}
	}
	if (interfacePic != NULL) {
		HPurge((Handle)interfacePic);
		ReleaseResource((Handle)interfacePic);
		interfacePic = NULL;
	}
	if (!seenIntroScreen) {
		ShowIntroScreen();
		FadeScreenOut(16, &rgbBlack);
		StopAndDisposeSoundPair();
		seenIntroScreen = true;
	} else
		FadeScreenOut(16, &rgbBlack);
	CheckShareWareRegistrationMatch(&gRegistered);
	MaxMem(&grow);
	SATRedraw();
	SATSetPortOffScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.offScreen.bounds);
	SATSetPortBackScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.backScreen.bounds);
	SATSetPortScreen();
	r = gSAT.wind.bounds;
	r.top -= kMenuBarHeight;
	RectRgn(gSAT.wind.port->clipRgn, &r);
	RectRgn(gSAT.wind.port->visRgn, &r);
	HideTheMenuBar();
	RedrawStatus();
	RedrawComm(false);
	hudRedraw.credits = true;
	mBarHidden = true;
	gAbortGame = false;
	SATSetPortScreen();
	SetMasterVolume(settings.soundVolume * kVolumeStep);
	if (gInvertedColors) {
		SetHudColorsJammed();
		ReapplyCloakPalette();
	}
	if (!EVBugResourceFieldTest(10))
		PlayGame();
	KillVoiceSound();
	RestoreOrigClut();
	SetHudColorsNormal();
	SATSetPortScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
	SetRect(&r, 0, 0, gSAT.wind.bounds.right, kMenuBarHeight);
	PaintRect(&r);
	InvalRect(&r);
	InvalRect(&gSAT.wind.bounds);
	interfacePic = GetPicture(kInterfacePictID);
	if (!gQuit) {
		SetupInterfaceBackground();
		InitTitleRects();
		while (StillDown())
			;
		/* BUG: event kinds passed as masks; only the low event bits are flushed */
		FlushEvents(keyDown, 0);	/* BUG: an event code, not a mask */
	}
	if (cursorHidden) {
		ShowCursor();
		cursorHidden = false;
	}
	SATRedraw();
	HideTheMenuBar();
	SATSetPortScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
	InvalRect(&gSAT.wind.bounds);
	if (!gQuit) {
		DrawClosedButtons();
		AnimateRowReveal();
		SetupInterfaceBackground();
		MainScreenUpdate(true);
	}
	/* BUG: event kinds passed as masks; only the low event bits are flushed */
	FlushEvents(keyDown, 0);	/* BUG: event codes, not masks */
	FlushEvents(autoKey, 0);
	FlushEvents(mouseDown, 0);
	FlushEvents(mouseUp, 0);
}

// FUNCTION: EVO 0x10045b9c
void DoNewPilot(void)
{
	Str255 prompt;
	Str255 text;
	Str255 newPilotName;
	Str255 newShipName;
	Boolean strict;
	Boolean ok;
	short i;
	short j;

	SATSetPortScreen();
	InvalRect(&gSAT.wind.bounds);
	strict = false;
	GetIndString(prompt, kPromptStrList, (short)Rand(3) + 1);
	ok = DoNewPilotNameDialog(prompt, 24, &strict);
	if (!ok)
		return;
	CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits,
			 &gSAT.wind.bounds, &gSAT.wind.bounds, srcCopy, NULL);
	strcpy((char *)newPilotName, (char *)gTextInputResult);
	CullNameString(newPilotName);
	if (DoesPilotExist(newPilotName)) {
		SATSetPortScreen();
		MainScreenUpdate(true);
		ok = EVYesNoDialog("\pA pilot file by that name already exists. Is it okay to replace it with this new pilot?");
		if (!ok)
			return;
		CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits,
				 &gSAT.wind.bounds, &gSAT.wind.bounds, srcCopy, NULL);
	}
	strcpy((char *)prompt, "Now, please christen your brand-new ");
	strcpy((char *)text, (char *)longShipName);
	p2cstr(text);
	strcat((char *)prompt, (char *)text);
	strcat((char *)prompt, ": ");
	c2pstr((char *)prompt);
	GetIndString(text, kPromptStrList, (short)Rand(3) + 4);
	ok = EVTextInputDialog(prompt, text, 20);
	if (!ok)
		return;
	onPlanet = false;
	ResetPlayer(true);
	ResetPlayerRecord(true);
	ResetGameData();
	LoadObjectData();
	DeleteAllComputerShips(true, true);
	ExploreNebulaeFromSystem(shipData->curSystem);
	mapCenter.h = gSystems[shipData->curSystem].xPos;
	mapCenter.v = gSystems[shipData->curSystem].yPos;
	killShots = killBoxes = killExplods = killSmoke = killAsteroids = true;
	strcpy((char *)newShipName, (char *)gTextInputResult);
	CullNameString(newShipName);
	strcpy((char *)shipName, (char *)newShipName);
	strcpy((char *)playerName, (char *)newPilotName);
	GetTime(&gameTime);
	gameTime.year += ReadStoredVersionStamp();
	gameInProgress = true;
	strictPlay = strict;
	for (i = 0; i < MaxOutfitItemTypes; i++)
		outfitCount[i] = 0;
	for (i = 0; i < MaxOutfitItemTypes; i++)
		outfitCount[i] = 0;
	for (i = 0; i < MaxWeaponTypes; i++) {
		shipData->weap[i].count = gShipTypes[shipData->type].weapCount[i];
		shipData->weap[i].ammo = gShipTypes[shipData->type].weapAmmo[i];
	}
	ShipStatsToSystemInfo();
	i = -1;
	for (j = 0; j <= 3; j++) {
		if (gSystems[0].navDefault[j] != -1) {
			i = gSystems[0].navDefault[j];
			break;
		}
	}
	shipData->deathTimer = -1;
	gTargetStellar = -1;
	lowestPrice = -3;
	if (i != -1)
		SavePilotFile(i);
	else
		SavePilotFile(0);
	SetupShipsInSystem(shipData->curSystem);
	HandleStellarSystemVisibility();
	CreateAsteroids();
}

// FUNCTION: EVO 0x10046054
Boolean DoNewPilotNameDialog(StringPtr name, short maxLen, Boolean *strictPlay)
{
	Rect box;
	short itemHit;
	short itemType;
	short i;
	Handle item;
	Str255 text;
	DialogPtr dialog;
	ControlHandle checkBox;
	Boolean strict;
	Boolean done;
	Boolean result;
	short len;

	done = false;
	strict = *strictPlay;
	dialog = NULL;
	dialog = GetNewDialog(kNewPilotNameDlogID, NULL, (WindowPtr)-1);
	if (dialog == NULL)
		return false;
	SetDialogDefaultItem(dialog, 1);
	NewDialogHook(dialog, NULL);
	ShowWindow(dialog);
	SelectWindow(dialog);
	SetPort(dialog);
	DrawDialog(dialog);
	FrameDefaultButton(dialog, 1);
	GetDialogItem(dialog, kNameStrictItem, &itemType, &item, &box);
	checkBox = (ControlHandle)item;
	if (strict)
		SetControlValue(checkBox, 1);
	else
		SetControlValue(checkBox, 0);
	strncpy((char *)text, (char *)name, 254);
	GetDialogItem(dialog, kNameTextItem, &itemType, &item, &box);
	SetDialogItemText(item, text);
	SelectDialogItemText(dialog, kNameTextItem, 0, 254);
	FlushEvents(everyEvent - diskMask, 0);
	do {
		ModalDialog(NULL, &itemHit);
		if (itemHit == kNameOkItem) {
			GetDialogItem(dialog, kNameTextItem, &itemType, &item, &box);
			GetDialogItemText(item, text);
			p2cstr(text);
			len = strlen((char *)text);
			if (len <= maxLen) {
				*strictPlay = strict;
				result = true;
				done = true;
			} else {
				SysBeep(0);
				SelectDialogItemText(dialog, kNameTextItem, 0, maxLen - 1);
			}
		}
		if (itemHit == kNameStrictItem) {
			if (strict)
				strict = false;
			else
				strict = true;
			GetDialogItem(dialog, kNameStrictItem, &itemType, &item, &box);
			checkBox = (ControlHandle)item;
			if (strict)
				SetControlValue(checkBox, 1);
			else
				SetControlValue(checkBox, 0);
		}
		if (itemHit == kNameCancelItem) {
			result = false;
			done = true;
		}
	} while (!done);
	GetDialogItem(dialog, kNameTextItem, &itemType, &item, &box);
	GetDialogItemText(item, gTextInputResult);
	SATSetPortScreen();
	DisposeDialog(dialog);
	SATSetPortScreen();
	return result;
}

// FUNCTION: EVO 0x10046348
void PlayCreditsChime(void)
{
	Ptr snd;

	if (shipData->weap[MaxWeaponTypes - 1].count <= 0) {
		shipData->weap[MaxWeaponTypes - 1].count = 1;
		shipData->weap[MaxWeaponTypes - 1].ammo = 50;
		snd = LoadSndResource(kCreditsChimeSndID);
		PlaySound(snd, 42, kMidVolume, kMidVolume);
		if (snd != NULL) {
			while (CountMatchingSoundVoices((long)snd))
				;
			DisposePtr(snd);
		}
	}
}

// FUNCTION: EVO 0x100463f0
void ShowIntroScreen(void)
{
	CTabHandle clut;
	Ptr snd;
	PicHandle pic;
	Rect bounds;
	unsigned long start;
	Rect r;
	Str255 text;
	EventRecord event;
	Rect box;

	FadeToColor(16, &rgbBlack);
	if (PlayMovieById(-32767, true)) {
		pic = GetPicture(kIntroScreenPictID);
		clut = GetCTable(kIntroClutID);
		SATSetPortScreen();
		SetMonitorClut(clut, false);
		ForeColor(blackColor);
		r = gSAT.wind.bounds;
		r.top -= kMenuBarHeight;
		RectRgn(gSAT.wind.port->clipRgn, &r);
		RectRgn(gSAT.wind.port->visRgn, &r);
		PaintRect(&r);
		r = gSAT.wind.bounds;
		CenterPict(pic, &r);
		DrawPicture(pic, &r);
		if (pic != NULL) {
			HPurge((Handle)pic);
			ReleaseResource((Handle)pic);
		}
		SATSetPortScreen();
		SetClutToColor(&rgbBlack, false);
		snd = NULL;
		snd = LoadSndResource(kIntroEndSndID);
		if (snd != NULL)
			PlaySound(snd, 10, kMidVolume, kMidVolume);
		if (Button())
			FadeToClut(16, clut);
		else
			FadeToClut(384, clut);
		SetMonitorClut(gameClut, false);
		start = TickCount();
		while (TickCount() - start < 600 && !Button() && !KeyCheck(kKeyReturn) && !KeyCheck(kKeySpace))
			;
		if (snd != NULL) {
			FlushMixQueueEntries((long)snd);
			DisposePtr(snd);
		}
		SetMonitorClut(clut, false);
	}
	LoadAndStartSoundPair(kDefaultLoopID);
	ShowScrollingText(kIntroTextID);
	ForeColor(blackColor);
	SATSetPortScreen();
	ForeColor(blackColor);
}

// FUNCTION: EVO 0x10046674
void InitTitleRects(void)
{
	Rect r1;
	Rect r2;

	SetRect(&rolloverRects[0], standardPicRect.left + 126, standardPicRect.top + 261, standardPicRect.left + 151, standardPicRect.top + 286);
	SetRect(&rolloverRects[1], standardPicRect.right - kStatusPanelWidth, standardPicRect.top + 259, standardPicRect.right - 121, standardPicRect.top + 284);
	rolloverRects[2] = rolloverRects[4] = rolloverRects[0];
	rolloverRects[3] = rolloverRects[5] = rolloverRects[1];
	OffsetRect(&rolloverRects[2], 0, 71);
	OffsetRect(&rolloverRects[4], 0, 139);
	OffsetRect(&rolloverRects[3], -1, 71);
	OffsetRect(&rolloverRects[5], -2, 139);
	SetRect(&buttonRects[0], standardPicRect.left, standardPicRect.top + 244, standardPicRect.left + 240, standardPicRect.top + 303);
	SetRect(&buttonRects[1], standardPicRect.right - 237, standardPicRect.top + 241, standardPicRect.right + 3, standardPicRect.top + 300);
	buttonRects[2] = buttonRects[4] = buttonRects[0];
	buttonRects[3] = buttonRects[5] = buttonRects[1];
	OffsetRect(&buttonRects[2], 0, 71);
	OffsetRect(&buttonRects[4], 0, 139);
	OffsetRect(&buttonRects[3], 0, 71);
	OffsetRect(&buttonRects[5], 0, 139);
	rolloverFrame = 0;
	lastRolloverAnimTime = TickCount();
	rolloverIndex = -1;
}

// FUNCTION: EVO 0x100468a8
void MainScreenRolloverDraw(void)
{
	Rect faceRect;
	Rect eraseRect;
	Point mouse;
	Point where;
	short i;
	short over;

	if (TickCount() >= lastRolloverAnimTime + 2) {
		lastRolloverAnimTime = TickCount();
		rolloverFrame++;
		if (rolloverFrame >= 4)
			rolloverFrame = 0;
	}
	SetRect(&faceRect, 0, 100, 25, 125);
	OffsetRect(&faceRect, rolloverFrame * 25, 0);
	GetMouse(&mouse);
	over = -1;
	for (i = 0; i <= 5; i++)
		if (PtInRect(mouse, &buttonRects[i]))
			over = i;
	if (over != rolloverIndex && rolloverIndex >= 0) {
		eraseRect = rolloverRects[rolloverIndex];
		InsetRect(&eraseRect, -3, -3);
		CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &eraseRect, &eraseRect, srcCopy, NULL);
	}
	if (over != -1 && !inBackground) {
		where.h = rolloverRects[over].left;
		where.v = rolloverRects[over].top;
		SATPlotFaceToScreen(rolloverFaces[rolloverFrame], where, true);
	}
	rolloverIndex = over;
}

// FUNCTION: EVO 0x10046a88
void DrawClosedButtons(void)
{
	Rect picRect;
	Rect destRects[7];
	Rect srcRects[2];
	PicHandle pic;
	short i;
	short row;
	short n;

	picRect = interfacePicRect;
	SATSetPort(&loadPort);
	DrawPicture(interfacePic, &picRect);
	SATSetPortScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
	DrawPicture(interfacePic, &picRect);
	if (doMainScreenAnim && Button())
		doMainScreenAnim = false;
	if (doMainScreenAnim) {
		pic = GetPicture(kRowRevealPictBase);
		if (pic != NULL) {
			SATSetPortBackScreen();
			SetRect(&destRects[0], 0, 0, 480, 59);
			DrawPicture(pic, &destRects[0]);
			HPurge((Handle)pic);
			ReleaseResource((Handle)pic);
			SATSetPortScreen();
			SetRect(&destRects[0], standardPicRect.left, standardPicRect.top + 244, standardPicRect.left + 240, standardPicRect.top + 303);
			SetRect(&destRects[1], standardPicRect.right - 237, standardPicRect.top + 241, standardPicRect.right + 3, standardPicRect.top + 300);
			destRects[2] = destRects[4] = destRects[0];
			destRects[3] = destRects[5] = destRects[1];
			OffsetRect(&destRects[2], 0, 71);
			OffsetRect(&destRects[4], 0, 139);
			OffsetRect(&destRects[3], 0, 71);
			OffsetRect(&destRects[5], 0, 139);
			SetRect(&srcRects[0], 240, 0, 480, 59);
			SetRect(&srcRects[1], 0, 0, 240, 59);
			for (i = 0; i <= 1; i++)
				for (row = 0; row <= 4; row += 2)
					CopyBits(&gSAT.backScreen.port->portBits, &gSAT.wind.port->portBits,
							 &srcRects[i], &destRects[i + row], srcCopy, NULL);
		}
	}
	FadeScreenIn(16);
	if (doMainScreenAnim)
		CopyBits(&gSAT.wind.port->portBits, &loadPort.port->portBits, &picRect, &picRect, srcCopy, NULL);
	else
		MainScreenUpdate(true);
}

// FUNCTION: EVO 0x10046da0
void AnimateRowReveal(void)
{
	Rect picRect;
	Rect destRects[7];
	Rect srcRects[2];
	PicHandle pic;
	unsigned long ticks;
	short i;
	short row;
	short frame;
	long n;

	if (doMainScreenAnim) {
		CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits,
				 &gSAT.wind.bounds, &gSAT.wind.bounds, srcCopy, NULL);
		picRect = interfacePicRect;
		SATSetPort(&loadPort);
		DrawPicture(interfacePic, &picRect);
		SATSetPortScreen();
		if (interfacePic != NULL) {
			HPurge((Handle)interfacePic);
			ReleaseResource((Handle)interfacePic);
			interfacePic = NULL;
		}
		for (frame = 1; frame <= 3; frame++)
			mainScreenSnd[frame] = LoadSndResource(frame + (kMainClickSndID - 1));
		SetRect(&destRects[0], standardPicRect.left, standardPicRect.top + 244, standardPicRect.left + 240, standardPicRect.top + 303);
		SetRect(&destRects[1], standardPicRect.right - 237, standardPicRect.top + 241, standardPicRect.right + 3, standardPicRect.top + 300);
		destRects[2] = destRects[4] = destRects[0];
		destRects[3] = destRects[5] = destRects[1];
		OffsetRect(&destRects[2], 0, 71);
		OffsetRect(&destRects[4], 0, 139);
		OffsetRect(&destRects[3], 0, 71);
		OffsetRect(&destRects[5], 0, 139);
		SetRect(&srcRects[0], 240, 0, 480, 59);
		SetRect(&srcRects[1], 0, 0, 240, 59);
		for (row = 0; row <= 4; row += 2) {
			if (Button())
				break;
			PlaySound(mainScreenSnd[2], 1, kMidVolume, kMidVolume);
			for (frame = 1; frame <= 16; frame++) {
				if (Button())
					break;
				ticks = TickCount();
				if (frame == 12)
					PlaySound(mainScreenSnd[3], 1, kMidVolume, kMidVolume);
				if (frame == 14)
					FlushMixQueueEntries((long)mainScreenSnd[2]);
				pic = GetPicture(frame + kRowRevealPictBase);
				if (pic != NULL) {
					SATSetPortBackScreen();
					SetRect(&destRects[6], 0, 0, 480, 59);
					DrawPicture(pic, &destRects[6]);
					HPurge((Handle)pic);
					ReleaseResource((Handle)pic);
					SATSetPortScreen();
					for (i = 0; i <= 1; i++)
						CopyBits(&gSAT.backScreen.port->portBits, &gSAT.wind.port->portBits,
								 &srcRects[i], &destRects[i + row], srcCopy, NULL);
					while (TickCount() <= ticks + 1)
						;
				}
			}
			for (i = 0; i <= 1; i++)
				CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits,
						 &destRects[i + row], &destRects[i + row], srcCopy, NULL);
		}
		CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &picRect, &picRect, srcCopy, NULL);
		if (!Button()) {
			SetRect(&destRects[0], standardPicRect.left + 265, standardPicRect.top + 260, standardPicRect.left + 380, standardPicRect.top + 450);
			destRects[1] = destRects[0];
			destRects[1].top = destRects[1].bottom = (destRects[0].top + destRects[0].bottom) / 2;
			while (destRects[1].bottom < destRects[0].bottom) {
				if (Button())
					destRects[1] = destRects[0];
				InsetRect(&destRects[1], 0, -5);
				CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits,
						 &destRects[1], &destRects[1], srcCopy, NULL);
			}
		} else {
			for (frame = 2; frame <= 3; frame++) {
				if (mainScreenSnd[frame] != NULL) {
					FlushMixQueueEntries((long)mainScreenSnd[frame]);
					DisposePtr(mainScreenSnd[frame]);
					mainScreenSnd[frame] = NULL;
				}
			}
			MainScreenUpdate(true);
		}
	}
	/* BUG: stops at 2, so mainScreenSnd[3] (loaded above) is never disposed on this path */
	for (frame = 2; frame <= 2; frame++) {
		if (mainScreenSnd[frame] != NULL) {
			while (CountMatchingSoundVoices((long)mainScreenSnd[frame])) {
				if (Button()) {
					FlushMixQueueEntries((long)mainScreenSnd[frame]);
					break;
				}
			}
			DisposePtr(mainScreenSnd[frame]);
			mainScreenSnd[frame] = NULL;
		}
	}
	SATSetPortBackScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.backScreen.bounds);
	SATSetPortScreen();
}
