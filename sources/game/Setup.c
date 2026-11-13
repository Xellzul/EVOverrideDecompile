/* Setup.c -- start-up and shut-down: Toolbox, window and SAT, arrays, offscreens, colours, sounds, new-game state. Notes: Research/notes/Setup.md */
#include <Types.h>
#include <Quickdraw.h>
#include <QDOffscreen.h>
#include <Fonts.h>
#include <Windows.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>
#include <Memory.h>
#include <Resources.h>
#include <OSUtils.h>
#include <Gestalt.h>
#include <TextUtils.h>
#include <string.h>
#include <LowMem.h>

#include "evo.h"


enum { kVPositionSort = 0, kLayerSort, kNoSort };
enum { kKindCollision = 0, kForwardCollision, kBackwardCollision, kNoCollision };


#define kSATStrList			25000
#define kBasePriceStrList	4004
#define kBasePricePluginStr	9300
#define kStatusPanelPictID	128
#define kStatusPanelPictHeight	480
#define kArmorPictID		160
#define kHudSpriteLayer		99
#define kEscortSpriteLayer	7
#define kDockSpriteLayer	3

#define kWarpUpSndID		128
#define kWarpOutSndID		130
#define kBeepSndBase		150
#define kWeapSndBase		200
#define kExplodeSndBase		300
#define kCloakSndBase		340
#define kKlaxonSndID		350
#define kEjectSndID			360
#define kRedAlertSndID		370
#define kAirlockSndID		390
#define kVoiceSndBase		700

#define kNumStars	26
#define kStackReserve	(120L * 1024)
#define kMinFreeMem	9200000

#define kColorFull			0xFFFF
#define kColorThreeQuarter	0xC000
#define kColorHalf			0x8000
#define kColorQuarter		0x4000

extern Rect			statusRect;


extern SpritePtr	hudFlagSprite, hudBlinkSprite, escortSprite, dockSprite;

extern Ptr airlockSnd;
extern SoundRequest	alarmRequest;

extern char lastPlayerName[256];
extern short		screenDepth;
extern Boolean		hasQuickTime;
extern short commStellar;
extern Boolean		playerThreatened, oldPlayerThreatened;
extern Boolean weapLawFlag;
extern Boolean smuggleFlag;
extern short		saveSlot;



WindowPtr gWind;
Rect mainWindRect;

// FUNCTION: EVO 0x10051fbc
void SetupToolbox(void)
{
	short i;
	EventRecord event;

	InitGraf(&qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs(nil);
	InitCursor();
	FlushEvents(everyEvent, 0);
#if !GENERATINGPOWERPC
	{
		long saveStack;

		saveStack = LMGetDefltStack();
		LMSetDefltStack(kStackReserve);
		SetApplLimit((Ptr)LMGetCurStackBase() - kStackReserve);
		LMSetDefltStack(saveStack);
	}
#endif
	MaxApplZone();
	for (i = 0; i < 6; i++)
		MoreMasters();
	Rand(0);
}

// FUNCTION: EVO 0x1005206c
void SetupSAT(void)
{
	short i;
	Rect r;
	Str255 str[8];

	mBarHidden = 1;
	gWind = NewCWindow(nil, &mainWindRect, "\pEscape Velocity", false, plainDBox, nil, true, -1);
	ShowWindow(gWind);
	HideTheMenuBar();
	r = mainWindRect;
	r.right -= kStatusPanelWidth;
	SetPort(gWind);
	SATConfigure(true, kLayerSort, kKindCollision, 32);
	HLock((Handle)gDevice);
	SATCustomInit(0, 0, &r, gWind, gDevice, true, false, false, false, true);
	HUnlock((Handle)gDevice);
	gSAT.wind.bounds = mainWindRect;
	statusRect = r;
	statusRect.top -= 75;
	statusRect.left -= 75;
	for (i = 0; i <= 7; i++)
		GetIndString(str[i], kSATStrList, i + 1);
	SATSetStrings(str[0], str[1], str[2], str[3], str[4], str[5], str[6], str[7]);
	SATInstallEmergency(EmergencyBail);
}

// FUNCTION: EVO 0x10052224
void SetupWind(void)
{
	ShowWindow(gSAT.wind.port);
	SelectWindow(gSAT.wind.port);
	SATRedraw();
	HideTheMenuBar();
	mBarHidden = 1;
	SATSetPortScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
	screenCenter.h = (gSAT.wind.bounds.right - gSAT.wind.bounds.left - kStatusPanelWidth) * 0.5;
	screenCenter.v = (gSAT.wind.bounds.bottom - gSAT.wind.bounds.top) * 0.5;
}

// FUNCTION: EVO 0x1005232c
void InitArrays(void)
{
	char msg[256];
	Str255 num;

	shipData = (ShipObj *)NewPtr(sizeof(ShipObj) * MaxShipsInSystem);
	CheckNoMem((Ptr)shipData);
	stellarData = (SpobRecord *)NewPtr(sizeof(SpobRecord) * MaxStellarObjects);
	CheckNoMem((Ptr)stellarData);
	gSystems = (SystRecord *)NewPtr(sizeof(SystRecord) * MaxSystems);
	CheckNoMem((Ptr)gSystems);
	starData = (StarSlot *)NewPtr(sizeof(StarSlot) * kNumStars);
	CheckNoMem((Ptr)starData);
	gShipTypes = (ShipRec *)NewPtr(sizeof(ShipRec) * MaxShipClasses);
	CheckNoMem((Ptr)gShipTypes);
	gWeapons = (WeapRecord *)NewPtr(sizeof(WeapRecord) * MaxWeaponTypes);
	CheckNoMem((Ptr)gWeapons);
	beamData = (BeamRecord *)NewPtr(sizeof(BeamRecord) * MaxBeamsOnScreen);
	CheckNoMem((Ptr)beamData);
	gOutfits = (OutfRecord *)NewPtr(sizeof(OutfRecord) * MaxOutfitItemTypes);
	CheckNoMem((Ptr)gOutfits);
	dudeData = (DudeRecord *)NewPtr(sizeof(DudeRecord) * MaxDudeTypes);
	CheckNoMem((Ptr)dudeData);
	govtData = (GovtRecord *)NewPtr(sizeof(GovtRecord) * MaxGovts);
	CheckNoMem((Ptr)govtData);
	personData = (PersRecord *)NewPtr(sizeof(PersRecord) * MaxPersonTypes);
	CheckNoMem((Ptr)personData);
	asteroidData = (AsteroidObj *)NewPtr(sizeof(AsteroidObj) * MaxAsteroids);
	CheckNoMem((Ptr)asteroidData);
	disasterData = (OopsRecord *)NewPtr(sizeof(OopsRecord) * MaxDisasters);
	CheckNoMem((Ptr)disasterData);
	nebulaData = (NebuRecord *)NewPtr(sizeof(NebuRecord) * MaxNebulae);
	CheckNoMem((Ptr)nebulaData);
	fleetData = (FletRecord *)NewPtr(sizeof(FletRecord) * MaxFleets);
	CheckNoMem((Ptr)fleetData);
	podData = (PodObj *)NewPtr(sizeof(PodObj) * MaxExplosionsOnScreen);
	CheckNoMem((Ptr)podData);
	shotData = (ShotObj *)NewPtr(sizeof(ShotObj) * MaxShotsOnScreen);
	CheckNoMem((Ptr)shotData);
	junkData = (JunkRecord *)NewPtr(sizeof(JunkRecord) * kMaxJunkTypes);
	CheckNoMem((Ptr)junkData);
	movieData = (DeqtRecord *)NewPtr(sizeof(DeqtRecord) * MaxMovies);
	CheckNoMem((Ptr)movieData);
	currentMissions = (MissionRecord *)NewPtr(sizeof(MissionRecord) * MaxSimultaneousMissions);
	CheckNoMem((Ptr)currentMissions);
	currentMissionObjectives = (MissionState *)NewPtr(sizeof(MissionState) * MaxSimultaneousMissions);
	CheckNoMem((Ptr)currentMissionObjectives);
	if (EVBugResourceFieldTest(4)) {
		strcpy(msg, "After creating arrays, stackspace is ");
		NumToString(StackSpace(), num);
		p2cstr(num);
		strcat(msg, (char *)num);
		strcat(msg, " bytes");
		c2pstr(msg);
		DebugStr((StringPtr)msg);
	}
}

// FUNCTION: EVO 0x100526cc
void InitOffscreen(void)
{
	PicHandle pic;
	Rect r;
	OSErr err;

	r = mainWindRect;
	OffsetRect(&r, -mainWindRect.left, -mainWindRect.top);
	SATMakeOffscreen(&loadPort, &r);
	if (loadPort.port == nil) {
		CloseMonitorTool();
		HideWindow(gSAT.wind.port);
		ShowTheMenuBar();
		ShowCursor();
		EVNoteAlert("\pSorry, EV ran out of memory while creating an offscreen buffer. Please increase EV\325s memory allocation and try again.");
		TeardownSoundSubsystem();
		ExitToShell();
	}
	r.right = r.left + kStatusPanelWidth;
	SATMakeOffscreen(&statusPort, &r);
	if (statusPort.port == nil) {
		CloseMonitorTool();
		HideWindow(gSAT.wind.port);
		ShowTheMenuBar();
		ShowCursor();
		EVNoteAlert("\pSorry, EV ran out of memory while creating an offscreen buffer. Please increase EV\325s memory allocation and try again.");
		TeardownSoundSubsystem();
		ExitToShell();
	}
	pic = GetPicture(kStatusPanelPictID);
	if (pic != nil) {
		SATSetPort(&statusPort);
		ForeColor(blackColor);
		PaintRect(&statusPort.bounds);
		r = statusPort.bounds;
		r.bottom = r.top + kStatusPanelPictHeight;
		DrawPicture(pic, &r);
		HPurge((Handle)pic);
		ReleaseResource((Handle)pic);
	}
	SetRect(&r, 0, 0, 53, 35);
	SATMakeOffscreen(&armorPort, &r);
	if (armorPort.port == nil) {
		CloseMonitorTool();
		HideWindow(gSAT.wind.port);
		ShowTheMenuBar();
		ShowCursor();
		EVNoteAlert("\pSorry, EV ran out of memory while creating an offscreen buffer. Please increase EV\325s memory allocation and try again.");
		TeardownSoundSubsystem();
		ExitToShell();
	}
	pic = GetPicture(kArmorPictID);
	if (pic != nil) {
		SATSetPort(&armorPort);
		ForeColor(blackColor);
		PaintRect(&armorPort.bounds);
		r = armorPort.bounds;
		DrawPicture(pic, &r);
		HPurge((Handle)pic);
		ReleaseResource((Handle)pic);
	}
	SATSetPortScreen();
}

// FUNCTION: EVO 0x1005296c
void Bye(void)
{
	FadeScreenOut(8, &rgbBlack);
	TeardownSoundSubsystem();
	ShowTheMenuBar();
	RestoreControlStrip();
	SATSetPortScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
	HideWindow(gWind);
	if (cursorHidden)
		ShowCursor();
	FadeScreenIn(8);
	CloseMonitorTool();
	CloseShareWareRegistrationSession();
	FlushEvents(everyEvent - diskMask, 0);
	SATSetPortScreen();
	ExitToShell();
}

// FUNCTION: EVO 0x10052a3c
void InitColors(void)
{
	bkgndGreen.red = bkgndGreen.blue = 0;
	bkgndGreen.green = 0x2200;
	dimGreen.red = dimGreen.blue = 0;
	dimGreen.green = 0x9900;
	dimmerGreen.red = dimmerGreen.blue = 0;
	dimmerGreen.green = 0x6666;
	brightGreen.red = brightGreen.blue = 0;
	brightGreen.green = kColorFull;
	rgbWhite.red = rgbWhite.green = rgbWhite.blue = kColorFull;
	rgbDarkGrey.red = rgbDarkGrey.green = rgbDarkGrey.blue = kColorQuarter;
	rgbMedGrey.red = rgbMedGrey.green = rgbMedGrey.blue = kColorHalf;
	rgbLightGrey.red = rgbLightGrey.green = rgbLightGrey.blue = kColorThreeQuarter;
	rgbRealDarkBlue.red = rgbRealDarkBlue.green = 0;
	rgbRealDarkBlue.blue = 0x428f;
	rgbBlack.red = rgbBlack.green = rgbBlack.blue = 0;
}

// FUNCTION: EVO 0x10052b38
void InitSprites(void)
{
	short i;

	hudFlagSprite = SATNewSprite(0, 0, 0, nil);
	if (hudFlagSprite != nil) {
		hudFlagSprite->task = HandleDoubleTimeSprite;
		hudFlagSprite->layer = kHudSpriteLayer;
		hudFlagSprite->kind = 0;
		hudFlagSprite->mode = 0;
	}
	hudBlinkSprite = SATNewSprite(0, 0, 0, nil);
	if (hudBlinkSprite != nil) {
		hudBlinkSprite->task = HandleAlertSprite;
		hudBlinkSprite->layer = kHudSpriteLayer;
		hudBlinkSprite->kind = 0;
		hudBlinkSprite->mode = 0;
	}
	for (i = 0; i <= 3; i++) {
		escortSprite = SATNewSprite(0, 0, 0, SetupEscortSprite);
		if (escortSprite != nil) {
			escortSprite->mode = 0;
			escortSprite->appLong = i;
			escortSprite->task = HandleWeapTargetSprites;
			escortSprite->layer = kEscortSpriteLayer;
			escortSprite->kind = 0;
		}
		dockSprite = SATNewSprite(0, 0, 0, SetupDockSprite);
		if (dockSprite != nil) {
			dockSprite->mode = 0;
			dockSprite->appLong = i;
			dockSprite->task = HandleNavTargetSprites;
			dockSprite->layer = kDockSpriteLayer;
			dockSprite->kind = 0;
		}
	}
	SetRect(&msgRect, gSAT.wind.bounds.left + 25, gSAT.wind.bounds.bottom - 5,
		gSAT.wind.bounds.right - 194, gSAT.wind.bounds.bottom - 5);
	msgRect.top -= 29470 / (msgRect.right - msgRect.left);
	msgRgn = NewRgn();
}

// FUNCTION: EVO 0x10052d68
void LoadSounds(void)
{
	short i, j;
	Ptr snd;
	Size grow;

	warpUpSnd = LoadSndResource(kWarpUpSndID);
	warpOutSnd = LoadSndResource(kWarpOutSndID);
	alarmRequest.snd = warpUpSnd;
	alarmRequest.pitch = kFixedOne;
	alarmRequest.callback = (SoundCallback)WarpUpCallback;
	alarmRequest.priority = 32000;
	alarmRequest.volLeft = kMidVolume;
	alarmRequest.volRight = kMidVolume;
	for (i = 0; i <= 4; i++)
		beepSnd[i] = LoadSndResource(kBeepSndBase + i);
	for (i = 0; i < MaxWeaponTypes; i++)
		weapSnd[i] = LoadSndResource(kWeapSndBase + i);
	for (i = 0; i <= 3; i++)
		explodeSnd[i] = LoadSndResource(kExplodeSndBase + i);
	klaxonSnd = LoadSndResource(kKlaxonSndID);
	ejectSnd = LoadSndResource(kEjectSndID);
	redAlertSnd = LoadSndResource(kRedAlertSndID);
	airlockSnd = LoadSndResource(kAirlockSndID);
	cloakSnd[0] = LoadSndResource(kCloakSndBase);
	cloakSnd[1] = LoadSndResource(kCloakSndBase + 1);
	voiceSnd = nil;
	pendingVoiceMsg = -1;
	for (i = 0; i <= 3; i++) {
		voiceCount[i] = 0;
		j = 0;
		do {
			snd = LoadSndResource(kVoiceSndBase + i * 10 + j);
			if (snd == nil)
				break;
			DisposePtr(snd);
			voiceCount[i]++;
			j++;
		} while (j < 9);
	}
	MaxMem(&grow);
}

// FUNCTION: EVO 0x10052fa4
void InitObjects(void)
{
	short i, j;
	long value;
	Str255 str;

	for (i = 0; i < MaxShipsInSystem; i++) {
		shipData[i].index = i;
		shipData[i].heading = 0;
		shipData[i].desiredHeading = 0;
		shipData[i].curSystem = -1;
		shipData[i].type = 0;
		shipData[i].hasWorldSpriteNode = 0;
		shipData[i].boarded = 0;
		shipData[i].active = 0;
		shipData[i].credits = 0;
		shipData[i].defenseStellar = -1;
		shipData[i].hasTargetLock = 1;
		shipData[i].targetShip = -1;
		shipData[i].selectedWeapon = -1;
		shipData[i].shield = 0;
		shipData[i].aiType = 0;
		shipData[i].aiTimer = 0;
		shipData[i].navTargetKind = kNavOff;
		shipData[i].targetStellar = -1;
		shipData[i].lastInspected = -1;
		shipData[i].lastStellar = -2;
		shipData[i].jumpTimer = 0;
		shipData[i].fuel = 100;
		shipData[i].parentShip = -1;
		shipData[i].deathTimer = 0;
		shipData[i].skill = 1.0;
		shipData[i].altFireSide = 1;
		for (j = 0; j < MaxWeaponTypes; j++) {
			shipData[i].weap[j].reload = 0;
			shipData[i].weap[j].count = 0;
			shipData[i].weap[j].ammo = 0;
		}
		for (j = 0; j <= 5; j++)
			shipData[i].cargo[j] = 0;
	}
	for (i = 0; i < MaxShipClasses; i++) {
		gShipTypes[i].techLevel = 9999;
		gShipTypes[i].shield = 1;
		for (j = 0; j < MaxWeaponTypes; j++) {
			gShipTypes[i].weapCount[j] = 0;
			gShipTypes[i].weapAmmo[j] = 0;
		}
	}
	for (i = 0; i < MaxStellarObjects; i++) {
		stellarData[i].spawned = 0;
		stellarData[i].loaded = 0;
		stellarData[i].type = 0;
		stellarData[i].techLevel = 0;
		stellarData[i].dominated = 0;
		stellarData[i].daysDominated = 0;
		stellarData[i].system = -1;
		for (j = 0; j <= 2; j++)
			stellarData[i].specialTech[j] = 0;
	}
	for (i = 0; i < MaxSystems; i++) {
		gSystems[i].loaded = 0;
		gSystems[i].govt = kLowestValue;
		gSystems[i].asteroidCount = 0;
		gSystems[i].interference = 0;
		gSystems[i].visited = 0;
		gSystems[i].govt = -1;
		gSystems[i].xPos = gSystems[i].yPos = 0;
		for (j = 0; j < kMaxSystemLinks; j++)
			gSystems[i].links[j] = -1;
		for (j = 0; j <= 3; j++)
			gSystems[i].navDefault[j] = -1;
		for (j = 0; j <= 3; j++) {
			gSystems[i].dudeTypes[j] = 0;
			gSystems[i].dudeProbs[j] = 25;
		}
		strncpy((char *)gSystems[i].name, "", 31);
	}
	for (i = 0; i < MaxPersonTypes; i++) {
		personData[i].loaded = 0;
		personData[i].linkSyst = 0;
		for (j = 0; j < MaxWeaponTypes; j++) {
			personData[i].weapCount[j] = 0;
			personData[i].weapAmmo[j] = 0;
		}
	}
	for (i = 0; i < MaxFleets; i++) {
		fleetData[i].leadShipType = -1;
		fleetData[i].govt = -1;
		fleetData[i].missionBit = -1;
	}
	for (i = 0; i < MaxAsteroids; i++) {
		asteroidData[i].active = 0;
		asteroidData[i].spawned = 0;
	}
	for (i = 0; i < MaxNebulae; i++)
		nebulaData[i].explored = 0;
	for (i = 0; i < kMaxJunkTypes; i++) {
		junkData[i].boughtAt = -1;
		junkData[i].soldAt = -1;
	}
	for (i = 0; i < MaxShotsOnScreen; i++)
		shotData[i].life = kSlotFree;
	for (i = 0; i < MaxExplosionsOnScreen; i++)
		podData[i].life = kSlotFree;
	for (i = 0; i < MaxBeamsOnScreen; i++) {
		beamData[i].life = kSlotFree;
		beamData[i].owner = -1;
	}
	for (i = 0; i < kMaxHyperRoute; i++)
		hyperRoute[i] = -1;
	RandomizeStars();
	for (i = 0; i <= 5; i++) {
		if (!LoadPluginString(i + kBasePricePluginStr, str))
			GetIndString(str, kBasePriceStrList, i + 1);
		StringToNum(str, &value);
		basePrice[i] = value;
	}
	for (i = 0; i < MaxMissions; i++)
		randomOdds[i] = Rand(100) + 1;
	for (i = 0; i < MaxGovts; i++)
		govtData[i].ally = govtData[i].enemy = -1;
	strictPlay = 1;
	blinkTimer = -1;
	dockPermission = dockTarget = -1;
	GetTime(&gameTime);
	gameTime.year += kGameYearOffset;
	strcpy(lastPlayerName, "");
	strncpy((char *)playerName, (char *)"\pNo Name", 10);
	strncpy((char *)shipName, (char *)"\pNo Name", 10);
}

// FUNCTION: EVO 0x10053ab0
void ResetPlayer(Boolean newPilot)
{
	short i;
	short j;
	Str255 str1;
	Str255 str2;
	double unused;

	shipData->pos.x = shipData->pos.y = 50.0;
	shipData->vel.x = shipData->vel.y = 0.0;
	shipData->heading = 0;
	shipData->type = 0;
	shipData->active = 1;
	shipData->navTargetKind = kNavOff;
	shipData->govt = -1;
	shipData->aiType = -1;
	shipData->curSystem = 0;
	shipData->shield = ShipShieldCapacity(shipData);
	shipData->fuel = ShipFuelCapacity(shipData);
	shipData->targetShip = -1;
	shipData->targetStellar = -1;
	shipData->navTargetKind = kNavOff;
	shipData->selectedWeapon = -1;
	shipData->parentShip = -1;
	shipData->specialMission = -1;
	shipData->persType = -1;
	shipData->aiState = shipData->aiSubstate = kManeuverLoJack;
	shipData->deathTimer = -999;
	shipData->skill = 1.0;
	shipData->jumpTimer = 0;
	shipData->prevSystem = -1;
	for (i = 0; i < MaxWeaponTypes; i++) {
		shipData->weap[i].count = gShipTypes[shipData->type].weapCount[i];
		shipData->weap[i].ammo = gShipTypes[shipData->type].weapAmmo[i];
	}
	for (i = 0; i < kMaxHyperRoute; i++)
		hyperRoute[i] = -1;
	for (i = 0; i < MaxOutfitItemTypes; i++)
		outfitCount[i] = 0;
	for (i = 0; i <= 5; i++)
		shipData->cargo[i] = 0;
	for (i = 0; i < kMaxJunkTypes; i++)
		junkData[i].held = 0;
	hailShip = -1;
	lastMissionStellar = -1;
	gTargetStellar = -1;
	commStellar = -1;
	dockTarget = -1;
	dockPermission = -1;
	adviceRandom = Rand(1500);
	bribeRandom = -1;
	playerThreatened = oldPlayerThreatened = 0;
	fadePending[0] = fadePending[1] = 0;
	lowestPrice = kShortMax;
	blinkTimer = -1;
	killShots = 1;
	killBoxes = 1;
	killExplods = 1;
	killSmoke = 1;
	weapLawFlag = 0;
	smuggleFlag = 0;
	gInvertedColors = 0;
	greenMissionSyst = -1;
	commCount = 0;
	for (j = 0; j < MaxSimultaneousMissions; j++) {
		if (currentMissionObjectives[j].active)
			AbortMission(j);
		currentMissionObjectives[j].active = 0;
	}
	for (j = 0; j < MaxMissions; j++)
		randomOdds[j] = Rand(100) + 1;
	for (i = 0; i <= 1; i++) {
		globalPriceFlux[i] = 100;
		localPriceFlux[i] = 100;
	}
	if (newPilot) {
		shipData->credits = 10000;
		for (i = 0; i < MaxSystems; i++) {
			gSystems[i].visited = 0;
			if (gSystems[i].govt >= 0 && gSystems[i].govt < MaxGovts)
				legalStatus[i] = govtData[gSystems[i].govt].initialRec;
			else
				legalStatus[i] = 0;
		}
		for (i = 0; i < kMaxSystemLinks; i++) {
			if (gSystems[shipData->curSystem].links[i] != -1
					&& gSystems[gSystems[shipData->curSystem].links[i]].loaded)
				gSystems[gSystems[shipData->curSystem].links[i]].visited = 1;
		}
		for (i = 0; i < MaxNebulae; i++)
			nebulaData[i].explored = 0;
	}
	mapCenter.h = gSystems[shipData->curSystem].xPos;
	mapCenter.v = gSystems[shipData->curSystem].yPos;
}

// FUNCTION: EVO 0x10054158
void ResetPlayerRecord(Boolean newPilot)
{
	short i;
	short minStatus;

	if (newPilot)
		playerKills = 0;
	for (i = 0; i < MaxSystems; i++) {
		if (gSystems[i].govt >= 0)
			minStatus = govtData[gSystems[i].govt].initialRec;
		else
			minStatus = 0;
		if (legalStatus[i] < minStatus)
			legalStatus[i] = minStatus;
	}
	for (i = 0; i < MaxStellarObjects; i++)
		stellarData[i].dominated = 0;
}

// FUNCTION: EVO 0x10054240
void InitStarSprites(void)
{
	SpritePtr sp;
	short i;
	StarSlot *star;

	for (i = 0; i < kNumStars; i++) {
		sp = nil;
		sp = SATNewSprite(0, 0, 0, nil);
		if (sp != nil) {
			sp->task = HandleStars;
			sp->layer = 0;
			sp->kind = 0;
			sp->mode = 0;
			star = &starData[i];
			sp->appPtr = (Ptr)star;
		}
	}
}

// FUNCTION: EVO 0x100542d8
void RandomizeStars(void)
{
	short i;
	short width, height;
	Point delta;

	width = gSAT.wind.bounds.right - gSAT.wind.bounds.left - kStatusPanelWidth;
	height = gSAT.wind.bounds.bottom - gSAT.wind.bounds.top;
	for (i = 0; i < kNumStars; i++) {
		starData[i].pos.x = Rand(width);
		starData[i].pos.y = Rand(height);
		starData[i].type = Rand(2);
		starData[i].frame = Rand(360);
		starData[i].depth = Rand(width * 0.5);
		delta.h = starData[i].pos.x - shipData->pos.x;
		delta.v = starData[i].pos.y - shipData->pos.y;
		if (delta.h < 0)
			starData[i].pos.x += gSAT.wind.bounds.right - gSAT.wind.bounds.left - kStatusPanelWidth;
		if (delta.h > gSAT.wind.bounds.right - gSAT.wind.bounds.left - kStatusPanelWidth)
			starData[i].pos.x -= gSAT.wind.bounds.right - gSAT.wind.bounds.left - kStatusPanelWidth;
		if (delta.v < 0)
			starData[i].pos.y += gSAT.wind.bounds.bottom - gSAT.wind.bounds.top;
		/* BUG: delta.v is tested against the width, not the height */
		if (delta.v > gSAT.wind.bounds.right - gSAT.wind.bounds.left - kStatusPanelWidth)
			starData[i].pos.y -= gSAT.wind.bounds.bottom - gSAT.wind.bounds.top;
	}
}

// FUNCTION: EVO 0x10054624
pascal void SetupDockSprite(SpritePtr sp)
{
	sp->task = HandleNavTargetSprites;
	sp->layer = kDockSpriteLayer;
	sp->kind = 0;
	sp->mode = 0;
}

// FUNCTION: EVO 0x10054648
pascal void SetupEscortSprite(SpritePtr sp)
{
	sp->task = HandleWeapTargetSprites;
	sp->layer = kEscortSpriteLayer;
	sp->kind = 0;
	sp->mode = 0;
}

// FUNCTION: EVO 0x1005466c
void CheckSystem(void)
{
	EventRecord event;
	long response;
	long version;
	SysEnvRec env;

	SysEnvirons(curSysEnvVers, &env);
	if (env.systemVersion < 0x0700) {
		CloseMonitorTool();
		EVNoteAlert("\pSorry, EV requires System 7 or greater.");
		ShowTheMenuBar();
		TeardownSoundSubsystem();
		ExitToShell();
	}
	screenDepth = 8;
	if (Gestalt(gestaltQuickTime, &response))
		hasQuickTime = false;
	else
		hasQuickTime = true;
	mainWindRect = (**gDevice).gdRect;
}

// FUNCTION: EVO 0x10054734
void BailCheck(void)
{
	long freeMem;
	Size grow;
	char msg[256];
	char num[256];

	freeMem = MaxMem(&grow);
	if (freeMem < kMinFreeMem) {
		CloseMonitorTool();
		StopIntroMusic(false);
		strcpy(msg, "Sorry, EV needs at least an additional ");
		NumToString(((kMinFreeMem - freeMem) / 10000 + 1) * 10, (StringPtr)num);
		p2cstr((StringPtr)num);
		strcat(msg, num);
		strcat(msg, "K of memory to run in this resolution. Please increase its memory allocation and try again.");
		c2pstr(msg);
		FadeScreenIn(16);
		SATSetPortScreen();
		HideWindow(gSAT.wind.port);
		ShowTheMenuBar();
		ShowCursor();
		EVNoteAlert((StringPtr)msg);
		TeardownSoundSubsystem();
		ExitToShell();
	}
}

// FUNCTION: EVO 0x1005484c
void DefaultPrefs(void)
{
	settings.introMusic = 1;
	settings.voicesOn = 1;
	settings.soundVolume = 3;
	settings.highDetail = 0;
	settings.noMovies = 0;
	prefsChanged = 1;
	strcpy(lastPlayerName, "");
}

// FUNCTION: EVO 0x100548b8
void DefaultKeys(void)
{
	keyMap[kCtlNextWeapon] = kKeyW;
	keyMap[kCtlClearWeapon] = kKeyS;
	keyMap[kCtlFirePrimary] = kKeySpace;
	keyMap[kCtlFireSecondary] = kKeyControl;
	keyMap[kCtlHail] = kKeyY;
	keyMap[kCtlLand] = kKeyL;
	keyMap[kCtlSkipMessage] = kKeyReturn;
	keyMap[kCtlFaceTarget] = kKeyA;
	keyMap[kCtlNavOff] = kKeyGrave;
	keyMap[kCtlMap] = kKeyM;
	keyMap[kCtlNextTarget] = kKeyTab;
	keyMap[kCtlNearestTarget] = kKeyR;
	keyMap[kCtlNavHyperspace] = kKeyH;
	keyMap[kCtlNextLink] = kKeyBackslash;
	keyMap[kCtlJump] = kKeyJ;
	keyMap[kCtlBoard] = kKeyB;
	keyMap[kCtlEscortAttack] = kKeyF;
	keyMap[kCtlEscortRecall] = kKeyC;
	keyMap[kCtlEscortHold] = kKeyV;
	keyMap[kCtlTurnLeft] = kKeyLeftArrow;
	keyMap[kCtlTurnRight] = kKeyRightArrow;
	keyMap[kCtlThrust] = kKeyUpArrow;
	keyMap[kCtlReverse] = kKeyDownArrow;
	keyMap[26] = kKeyEscape;
	keyMap[kCtlAfterburner] = kKeyZ;
	keyMap[kCtlPlayerInfo] = kKeyP;
	keyMap[kCtlNavDefault] = kKeyF1;
	keyMap[kCtlNavDefault + 1] = kKeyF2;
	keyMap[kCtlNavDefault + 2] = kKeyF3;
	keyMap[kCtlNavDefault + 3] = kKeyF4;
	keyMap[kCtlSpawnPerson] = kKeyHelp;
	keyMap[kCtlDropTarget] = kKeyForwardDelete;
	keyMap[kCtlResetPlayer] = kKeyHome;
	keyMap[kCtlWreckTarget] = kKeyPageUp;
	keyMap[kCtlKillTarget] = kKeyPageDown;
	keyMap[kCtlRefillAmmo] = kKeyPadMultiply;
	keyMap[kCtlAdjustCredits] = kKeyDigit4;
	keyMap[kCtlShowMemory] = kKeyEnd;
	keyMap[kCtlNextShipType] = kKeyPadPlus;
	keyMap[kCtlPrevShipType] = kKeyPadMinus;
	keyMap[kCtlMissions] = kKeyI;
	keyMap[kCtlCloak] = kKeyU;
	keyMap[kCtlJettison] = kKeyK;
	keyMap[kCtlEject] = kKeyE;
	keyMap[kCtlSelfDestruct] = kKeyD;
}

// FUNCTION: EVO 0x10054a30
void SetupMonitor(void)
{
	short err;
	char msg[256];
	char num[256];

	err = OpenMonitorTool(8, 640, 480, true, &gDevice);
	if (err) {
		strcpy(msg, "Sorry, an error occured while trying to open the Monitor Tool.");
		num[0] = num[1] = num[2] = num[3] = '\n';
		num[4] = 0;
		strcat(msg, num);
		strcat(msg, "ID = ");
		NumToString(err, (StringPtr)num);
		p2cstr((StringPtr)num);
		strcat(msg, num);
		c2pstr(msg);
		CloseMonitorTool();
		EVNoteAlert((StringPtr)msg);
		ShowTheMenuBar();
		TeardownSoundSubsystem();
		ExitToShell();
	}
	SetGDevice(gDevice);
	InitCluts();
}

// FUNCTION: EVO 0x10054b44
void ResetGameData(void)
{
	short i;

	for (i = 0; i < MaxPersonTypes; i++) {
		if (personData[i].aiType >= 1) {
			personData[i].loaded = 1;
			personData[i].grudge = 0;
		}
	}
	for (i = 0; i < MaxStellarObjects; i++)
		stellarData[i].dominated = 0;
	for (i = 0; i < NumMissionBits; i++)
		missionBit[i] = 0;
	for (i = 0; i < MaxDisasters; i++)
		disasterData[i].countdown = 0;
	for (i = 0; i < MaxBeamsOnScreen; i++)
		beamData[i].life = kSlotFree;
	for (i = 0; i < MaxShotsOnScreen; i++)
		shotData[i].life = kSlotFree;
	for (i = 0; i < MaxExplosionsOnScreen; i++)
		podData[i].life = kSlotFree;
	for (i = 0; i < MaxNebulae; i++)
		nebulaData[i].explored = 0;
	RandomizeStars();
	dockPermission = dockTarget = -1;
	GetTime(&gameTime);
	gameTime.year += kGameYearOffset;
	strcpy(lastPlayerName, "");
	strcpy((char *)playerName, (char *)"\pNo Name");
	strcpy((char *)shipName, (char *)"\pNo Name");
	saveSlot = 1;
	seenIntroScreen = 0;
}

// FUNCTION: EVO 0x10054db0
void MeasureProcessorSpeed(void)
{
	long i;
	long total;
	unsigned long start;
	long count[10];

	ShowWaitDialog();
	total = 0;
	for (i = 0; i <= 9; i++) {
		count[i] = 0;
		start = TickCount();
		while (TickCount() <= start + 2)
			count[i] += 1;
		total += count[i];
	}
	total /= 10.0;
	gameSpeed = 7200.0f / (float)total;
	if (gameSpeed > 3.0)
		gameSpeed = 3.0;
	else if (gameSpeed < 1.05)
		gameSpeed = 1.0;
	CloseWaitDialog();
}

// FUNCTION: EVO 0x10054efc
pascal void EmergencyBail(void)
{
	TeardownSoundSubsystem();
	CloseMonitorTool();
}
