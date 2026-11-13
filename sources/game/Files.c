/* Files.c -- the data files and plug-ins, the object loader, descriptions, prefs, the pilot file, sprite sheets. Notes: Research/notes/Files.md */
#include <Errors.h>
#include <Files.h>
#include <Folders.h>
#include <Aliases.h>
#include <Finder.h>
#include <Resources.h>
#include <Memory.h>
#include <Quickdraw.h>
#include <Script.h>
#include <StandardFile.h>
#include <TextUtils.h>
#include <string.h>
#include <stddef.h>

#include "evo.h"

#define kLastPrefKey		35
#define kOldMaxOutfits		64
#define kOldMaxWeapons		36
#define kOldMaxPersons		256
#define kOldMissionBits		192

#pragma options align=mac68k

typedef struct PrefsResource {
	short		version;
	short		introMusic;
	short		voicesOn;
	short		soundVolume;
	short		keys[kLastPrefKey + 1];
	short		gameSpeed;
	DateTimeRec	date;
	short		prefsChanged;
	short		noMovies;
	short		useQuickdraw;
	short		noStreaks;
	short		spare[6];
} PrefsResource, *PrefsResPtr, **PrefsResHandle;

typedef struct PilotRec {
	short			stellar;
	short			shipType;
	short			cargo[NumCommodities];
	short			shield;
	short			fuel;
	short			month;
	short			day;
	short			year;
	short			visited[MaxSystems];
	short			outfits[MaxOutfitItemTypes];
	short			legalStatus[MaxSystems];
	short			weapCount[MaxWeaponTypes];
	short			weapAmmo[MaxWeaponTypes];
	long			credits;
	MissionState	objectives[MaxSimultaneousMissions];
	MissionRecord	missions[MaxSimultaneousMissions];
	char			missionBits[NumMissionBits];
	Boolean			dominated[MaxStellarObjects];
	short			escortType[MaxShipsInSystem];
	short			fighterType[MaxShipsInSystem];
	long			kills;
} PilotRec, *PilotPtr, **PilotHandle;

typedef struct PilotAuxRec {
	short		version;
	short		strict;
	short		tribute[MaxStellarObjects];
	short		persActive[MaxPersonTypes];
	short		persAccepted[MaxPersonTypes];
	short		unused13BC[64];
	short		tributeTicks[MaxStellarObjects];
	Boolean		valid;
	short		disasterCountdown[MaxDisasters];
	short		disasterStellar[MaxDisasters];
	short		junkHeld[kMaxJunkTypes];
	short		globalPriceFlux[2];
	short		localPriceFlux[2];
} PilotAuxRec, *PilotAuxPtr, **PilotAuxHandle;

typedef struct OldPilotRec {
	short			stellar;
	short			shipType;
	short			cargo[NumCommodities];
	short			shield;
	short			fuel;
	short			month;
	short			day;
	short			year;
	short			visited[MaxSystems];
	short			outfits[kOldMaxOutfits];
	short			legalStatus[MaxSystems];
	short			weapCount[kOldMaxWeapons];
	short			weapAmmo[kOldMaxWeapons];
	long			credits;
	MissionState	objectives[MaxSimultaneousMissions];
	MissionRecord	missions[MaxSimultaneousMissions];
	char			missionBits[kOldMissionBits];
	Boolean			dominated[MaxStellarObjects];
	short			escortType[MaxShipsInSystem];
	short			fighterType[MaxShipsInSystem];
	long			kills;
} OldPilotRec, *OldPilotPtr, **OldPilotHandle;

typedef struct OldPilotAuxRec {
	short		version;
	short		strict;
	short		tribute[MaxStellarObjects];
	short		persActive[kOldMaxPersons];
	short		persAccepted[kOldMaxPersons];
	short		unusedFBC[64];
	short		tributeTicks[MaxStellarObjects];
	Boolean		valid;
	short		disasterCountdown[MaxDisasters];
	short		disasterStellar[MaxDisasters];
	short		junkHeld[kMaxJunkTypes];
	short		globalPriceFlux[2];
	short		localPriceFlux[2];
} OldPilotAuxRec, *OldPilotAuxPtr, **OldPilotAuxHandle;


#pragma options align=reset

#define kBugType			0x91627567	/* '√´bug' */
#define kBugID				128
#define kNoSavePilotBug		12
#define kNoUpdatePilotBug	13
#define kLastBugField		31

#define kDataFileType		0x4F448A74	/* 'ODät' */
#define kPluginFileType		0x4F709566	/* 'Opïf' */
#define kPilotFileType		0x4F70954C	/* 'OpïL' */
#define kDataFileStrList	130
#define kPrefsType			0x4D70A8C4	/* 'Mp¨Ä' */
#define kPrefsFileType		0x4F70A8C4	/* 'Op¨Ä' */
#define kCreator			0x45738D4F	/* 'EsçO' */
#define kPrefsID			128
#define kPrefsVersion		104
#define kPrefsNameIndex		7
#define kPilotVersion		107
#define kOldPilotVersion	105
#define kPilotKey			0xABCD1234
#define kPilotID			128
#define kPilotAuxID			129
#define kHiredEscortBase		1000
#define kStellarSpinBase	300
#define kWeaponSpinBase		200
#define kExplosionSpinBase	400
#define kBoxSpinID			500
#define kStarSpinID			700
#define kSmallRockSpinID	800
#define kBigRockSpinID		801
#define kRolloverSpinID		900
#define kShipPictBase		3000
#define kOldPilotErr		-45
#define kWrongPilotErr		-42
#define kRowBytesMask		0x3FFF
#define kStaticPatID		128
#define kArmorPatID			200
#define kVersionID			128
#define kDockIconID			10000
#define kDockIcon2ID		10004
#define kTargetIconID		10008
#define kStreakIconID		1000
#define kHudOrbIconID		18000
#define kDoubleSpeedIconID	20000

extern FacePtr	doubleSpeedFace;
extern char	lcCargoName[MaxCargoTypes][256];
extern Str255	monthNames[16];

Boolean		hiResSprites;
Boolean		noSavePilotFiles;
Boolean		noUpdatePilotData;
SpobResHandle	spobHandle;
SystResHandle	systHandle;
ShipResHandle	shipHandle;
WeapResHandle	weapHandle;
OutfResHandle	outfHandle;
SpinResHandle	spinHandle;
DudeResHandle	dudeHandle;
GovtResHandle	govtHandle;
PersResHandle	persHandle;
OopsResHandle	oopsHandle;
NebuResHandle	nebuHandle;
FletResHandle	fletHandle;
JunkResHandle	junkHandle;
Handle		descHandle;
DeqtResHandle	deqtHandle;
PilotHandle		pilotHandle;
PilotAuxHandle	pilotAuxHandle;
PrefsResHandle	prefsHandle;
short		dataRefNum[2];
short		graphicsRefNum;
short		soundRefNum;
short		titlesRefNum;
short		musicRefNum;
short		prefsRefNum;
short		pilotRefNum;
CTabHandle	clut1;
CTabHandle	clut8;
SATPort		sheetPorts[kMaxSheets];
SATPort		maskPorts[kMaxSheets];
short		nextSheet;
short		curSheet;
short		prefsVRefNum;
short		pilotVRefNum;
long		prefsDirID;
long		pilotDirID;


// FUNCTION: EVO 0x10015ABC
void InitDataFiles(void)
{
	appResFile = CurResFile();
	noSavePilotFiles = EVBugResourceFieldTest(kNoSavePilotBug);
	noUpdatePilotData = EVBugResourceFieldTest(kNoUpdatePilotBug);
	ResolveMacFileAlias(appResFile, "\pPilots", &pilotDirID, &pilotVRefNum);
	FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder, &prefsVRefNum, &prefsDirID);
}

// FUNCTION: EVO 0x10015B4C
void OpenDataFiles(void)
{
	StandardFileReply	reply;
	FInfo	fInfo;
	FSSpec	spec;
	short	vRefNum;
	long	dirID;
	Str255	name;
	OSErr	err;

	err = GetVol(name, &vRefNum);
	if (err == noErr) {
		err = FSMakeFSSpec(vRefNum, 0, "\p:Pilots", &spec);
		if (err != noErr) {
			FadeScreenIn(8);
			EVNoteAlert("\pEV couldn\325t locate the Pilots folder. Please make sure the folder called \322Pilots\323 is in the same folder as EV.");
			ShowTheMenuBar();
			TeardownSoundSubsystem();
			CloseMonitorTool();
			ExitToShell();
		}
	}
	fInfo.fdType = kDataFileType;
	if (0)
		FSpGetFInfo(&spec, &fInfo);
	GetIndString(name, kDataFileStrList, 1);
	graphicsRefNum = OpenResFile(name);
	GetIndString(name, kDataFileStrList, 2);
	soundRefNum = OpenResFile(name);
	GetIndString(name, kDataFileStrList, 3);
	dataRefNum[0] = OpenResFile(name);
	GetIndString(name, kDataFileStrList, 4);
	dataRefNum[1] = OpenResFile(name);
	GetIndString(name, kDataFileStrList, 5);
	titlesRefNum = OpenResFile(name);
	GetIndString(name, kDataFileStrList, 6);
	musicRefNum = OpenResFile(name);
	if (graphicsRefNum == -1) {
		FadeScreenIn(8);
		EVNoteAlert("\pOverride couldn\325t locate its graphics file. Please make sure the file \322Override Graphics\323 is in the same folder as Override.");
		ShowTheMenuBar();
		TeardownSoundSubsystem();
		CloseMonitorTool();
		ExitToShell();
	}
	if (soundRefNum == -1) {
		FadeScreenIn(8);
		EVNoteAlert("\pOverride couldn\325t locate its sounds file. Please make sure the file \322Override Sounds\323 is in the same folder as Override.");
		ShowTheMenuBar();
		TeardownSoundSubsystem();
		CloseMonitorTool();
		ExitToShell();
	}
	if (titlesRefNum == -1) {
		FadeScreenIn(8);
		EVNoteAlert("\pOverride couldn\325t locate its titles file. Please make sure the file \322Override Titles\323 is in the same folder as Override.");
		ShowTheMenuBar();
		TeardownSoundSubsystem();
		CloseMonitorTool();
		ExitToShell();
	}
	if (dataRefNum[0] == -1 || dataRefNum[1] == -1) {
		FadeScreenIn(8);
		EVNoteAlert("\pOverride couldn\325t locate its data files. Please make sure the files \322Override Data 1\323 and \322Override Data 2\323 are in the same folder as Override.");
		ShowTheMenuBar();
		TeardownSoundSubsystem();
		CloseMonitorTool();
		ExitToShell();
	}
	err = OpenResourceForksByType(appResFile, "\pEV Plug-Ins", kPluginFileType);
	if (err != noErr) {
		FadeScreenIn(8);
		EVNoteAlert("\pOverride couldn\325t locate the plugins folder. Please make sure the folder called \322EV Plug-Ins\323 is in the same folder as Override.");
		ShowTheMenuBar();
		TeardownSoundSubsystem();
		CloseMonitorTool();
		ExitToShell();
	}
}

#define kSeekJamHalfAll		(kSeekJamHalf1 | kSeekJamHalf2 | kSeekJamHalf3 | kSeekJamHalf4)
#define kHeatSeekerFlags	(kSeekAsteroidDecoy | kSeekFlareDecoy | kSeekInterference | kSeekJamHalfAll | kSeekTurn30 | kSeekTurn60)

// FUNCTION: EVO 0x10015E70
void LoadObjectData(void)
{
	long	unused1;
	short	loaded;
	short	count;
	short	weapType[4];
	short	weapCount[4];
	short	weapAmmo[4];
	short	resID;
	short	unused3;
	Str255	name;
	ResType	resType;
	long	unused2;
	short	j;
	short	n;
	short	m;
	short	i;

	if (EVBugResourceFieldTest(0))
		return;

	loaded = 0;
	count = CountResources(kSpobType);
	for (i = 0; i < MaxStellarObjects; i++) {
		stellarData[i].system = -1;
		stellarData[i].loaded = false;
		spobHandle = (SpobResHandle)GetResource(kSpobType, i + FirstResourceID);
		if (spobHandle != NULL) {
			HNoPurge((Handle)spobHandle);
			loaded++;
			GetResInfo((Handle)spobHandle, &resID, &resType, name);
			strncpy((char *)stellarData[i].name, (char *)name, 31);
			stellarData[i].pos.h = (*spobHandle)->xPos;
			stellarData[i].pos.v = (*spobHandle)->yPos;
			stellarData[i].type = (*spobHandle)->type;
			stellarData[i].system = (*spobHandle)->system - FirstResourceID;
			stellarData[i].flags = (*spobHandle)->flags;
			stellarData[i].govt = (*spobHandle)->govt;
			if (stellarData[i].govt != -1)
				stellarData[i].govt -= FirstResourceID;
			stellarData[i].minStatus = (*spobHandle)->minCoolness;
			stellarData[i].techLevel = (*spobHandle)->techLevel;
			stellarData[i].customPict = (*spobHandle)->custPicID;
			stellarData[i].customSound = (*spobHandle)->custSndID;
			for (j = 0; j <= 2; j++)
				stellarData[i].specialTech[j] = (*spobHandle)->specialTech[j];
			stellarData[i].loaded = true;
			stellarData[i].dominated = false;
			stellarData[i].defCount = (*spobHandle)->defCount;
			stellarData[i].defenseDude = (*spobHandle)->defDude;
			if (stellarData[i].defCount > 1000) {
				if (stellarData[i].defCount > 10000)
					stellarData[i].tribute = stellarData[i].defCount / 10 - 1000;
				else
					stellarData[i].tribute = stellarData[i].defCount / 10 - 100;
			} else
				stellarData[i].tribute = stellarData[i].defCount;
			if (stellarData[i].defenseDude > 0)
				stellarData[i].defenseDude -= FirstResourceID;
			if (stellarData[i].techLevel < 1)
				stellarData[i].techLevel = 1;
			HPurge((Handle)spobHandle);
			ReleaseResource((Handle)spobHandle);
		}
		if (loaded >= count)
			break;
	}

	loaded = 0;
	count = CountResources(kSystType);
	for (i = 0; i < MaxSystems; i++) {
		gSystems[i].loaded = false;
		gSystems[i].govt = kLowestValue;
		for (j = 0; j < kMaxSystemLinks; j++)
			gSystems[i].links[j] = -1;
		systHandle = (SystResHandle)GetResource(kSystType, i + FirstResourceID);
		if (systHandle != NULL) {
			HNoPurge((Handle)systHandle);
			loaded++;
			GetResInfo((Handle)systHandle, &resID, &resType, name);
			strncpy((char *)gSystems[i].name, (char *)name, 31);
			gSystems[i].loaded = true;
			gSystems[i].dudeTypes[1] = (*systHandle)->dudeTypes[1];
			gSystems[i].dudeTypes[0] = (*systHandle)->dudeTypes[0];
			gSystems[i].dudeTypes[3] = (*systHandle)->dudeTypes[3];
			gSystems[i].dudeTypes[2] = (*systHandle)->dudeTypes[2];
			gSystems[i].dudeProbs[1] = (*systHandle)->prob[1];
			gSystems[i].dudeProbs[0] = (*systHandle)->prob[0];
			gSystems[i].dudeProbs[3] = (*systHandle)->prob[3];
			gSystems[i].dudeProbs[2] = (*systHandle)->prob[2];
			gSystems[i].avgShips = (*systHandle)->avgShips;
			gSystems[i].govt = (*systHandle)->govt;
			gSystems[i].message = (*systHandle)->message;
			gSystems[i].asteroidCount = (*systHandle)->asteroids;
			gSystems[i].interference = (*systHandle)->interference;
			if (gSystems[i].govt != -1)
				gSystems[i].govt -= FirstResourceID;
			gSystems[i].xPos = (*systHandle)->xPos;
			gSystems[i].yPos = (*systHandle)->yPos;
			gSystems[i].visibility = (*systHandle)->visBit;
			for (j = 0; j <= 3; j++) {
				if (gSystems[i].dudeTypes[j] > 0)
					gSystems[i].dudeTypes[j] -= FirstResourceID;
			}
			for (j = 0; j <= 4; j++) {
				if ((*systHandle)->con[j] >= FirstResourceID)
					gSystems[i].links[j] = (*systHandle)->con[j] - FirstResourceID;
				else
					gSystems[i].links[j] = -1;
			}
			for (j = 0; j <= 10; j++) {
				if ((*systHandle)->moreCon[j] >= FirstResourceID)
					gSystems[i].links[j + 5] = (*systHandle)->moreCon[j] - FirstResourceID;
				else
					gSystems[i].links[j + 5] = -1;
			}
			for (j = 0; j <= 3; j++) {
				if ((*systHandle)->nav[j] != -1)
					gSystems[i].navDefault[j] = (*systHandle)->nav[j] - FirstResourceID;
				else
					gSystems[i].navDefault[j] = -1;
			}
			if (GetHandleSize((Handle)systHandle) < offsetof(SystResource, alwaysPers)) {
				gSystems[i].visibility = -1;
				for (j = 5; j <= 15; j++)
					gSystems[i].links[j] = -1;
			}
			if (GetHandleSize((Handle)systHandle) < sizeof(SystResource)) {
				for (j = 0; j < 4; j++)
					gSystems[i].persons[j] = -1;
			} else {
				gSystems[i].persons[1] = (*systHandle)->alwaysPers[1];
				gSystems[i].persons[0] = (*systHandle)->alwaysPers[0];
				gSystems[i].persons[3] = (*systHandle)->alwaysPers[3];
				gSystems[i].persons[2] = (*systHandle)->alwaysPers[2];
				for (j = 0; j < 4; j++) {
					if (gSystems[i].persons[j] >= FirstResourceID)
						gSystems[i].persons[j] -= FirstResourceID;
					else
						gSystems[i].persons[j] = -1;
				}
			}
			HPurge((Handle)systHandle);
			ReleaseResource((Handle)systHandle);
		}
		if (loaded >= count)
			break;
	}

	for (i = 0; i < MaxOutfitItemTypes; i++) {
		outfHandle = (OutfResHandle)GetResource(kOutfType, i + FirstResourceID);
		if (outfHandle != NULL) {
			HNoPurge((Handle)outfHandle);
			gOutfits[i].techLevel = (*outfHandle)->techLevel;
			gOutfits[i].cost = (*outfHandle)->cost;
			gOutfits[i].mass = (*outfHandle)->mass;
			gOutfits[i].modType[0] = (*outfHandle)->modType;
			gOutfits[i].modVal[0] = (*outfHandle)->modVal;
			gOutfits[i].maxCount = (*outfHandle)->max;
			gOutfits[i].missionBit = (*outfHandle)->missionBit;
			gOutfits[i].persistent = false;
			gOutfits[i].flags = (*outfHandle)->flags;
			if ((*outfHandle)->flags & kOutfPersistent)
				gOutfits[i].persistent = true;
			GetResInfo((Handle)outfHandle, &resID, &resType, name);
			strncpy((char *)gOutfits[i].name, (char *)name, 63);
			if (GetHandleSize((Handle)outfHandle) < sizeof(OutfResource)) {
				gOutfits[i].modType[1] = -1;
				gOutfits[i].modVal[1] = -1;
			} else {
				gOutfits[i].modType[1] = (*outfHandle)->modType2;
				gOutfits[i].modVal[1] = (*outfHandle)->modVal2;
			}
			for (j = 0; j <= MaxOutfitMods - 1; j++) {
				if ((gOutfits[i].modType[j] == kOutfModWeapon || gOutfits[i].modType[j] == kOutfModAmmo
						|| gOutfits[i].modType[j] == kOutfModCleanRecord)
						&& gOutfits[i].modVal[j] >= FirstResourceID)
					gOutfits[i].modVal[j] -= FirstResourceID;
			}
			HPurge((Handle)outfHandle);
			ReleaseResource((Handle)outfHandle);
		} else {
			gOutfits[i].techLevel = kShortMax;
			gOutfits[i].modType[0] = 0;
			gOutfits[i].modType[1] = 0;
			gOutfits[i].modVal[0] = 0;
			gOutfits[i].modVal[1] = 0;
		}
	}

	for (i = 0; i < MaxShipClasses; i++) {
		shipHandle = (ShipResHandle)GetResource(kShipType, i + FirstResourceID);
		if (shipHandle != NULL) {
			HNoPurge((Handle)shipHandle);
			gShipTypes[i].cargo = (*shipHandle)->holds;
			gShipTypes[i].shield = (*shipHandle)->shield;
			gShipTypes[i].accel = (*shipHandle)->accel / 10000.0;
			gShipTypes[i].maxSpeed = (*shipHandle)->speed / 100.0;
			gShipTypes[i].turnRate = (*shipHandle)->maneuver;
			gShipTypes[i].fuel = (*shipHandle)->fuel;
			gShipTypes[i].freeMass = (*shipHandle)->freeMass;
			gShipTypes[i].armor = (*shipHandle)->armor;
			gShipTypes[i].shieldRech = (*shipHandle)->shieldRe;
			gShipTypes[i].maxGuns = (*shipHandle)->maxGun;
			gShipTypes[i].maxTurrets = (*shipHandle)->maxTur;
			gShipTypes[i].techLevel = (*shipHandle)->techLevel;
			gShipTypes[i].cost = (*shipHandle)->cost;
			gShipTypes[i].deathDelay = (*shipHandle)->deathDelay;
			gShipTypes[i].mass = (*shipHandle)->mass;
			gShipTypes[i].shipLength = (*shipHandle)->length;
			gShipTypes[i].inherentAI = (*shipHandle)->inherentAI;
			gShipTypes[i].crew = (*shipHandle)->crew;
			gShipTypes[i].missionBit = (*shipHandle)->missionBit;
			gShipTypes[i].inherentGovt = (*shipHandle)->inherentGovt;
			gShipTypes[i].flags = (*shipHandle)->flags;
			if (gShipTypes[i].cargo < 0) {
				gShipTypes[i].cargo *= -1;
				gShipTypes[i].negativeHoldsFlag = false;
			} else
				gShipTypes[i].negativeHoldsFlag = true;
			if (gShipTypes[i].shield < 0)
				gShipTypes[i].shield *= -5;
			if (gShipTypes[i].crew <= 0)
				gShipTypes[i].crew = 1;
			if (gShipTypes[i].inherentGovt >= FirstResourceID)
				gShipTypes[i].inherentGovt -= FirstResourceID;
			else
				gShipTypes[i].inherentGovt = -1;
			gShipTypes[i].turretYDisp[0] = (*shipHandle)->turretYDisp[0];
			gShipTypes[i].turretYDisp[1] = (*shipHandle)->turretYDisp[1];
			gShipTypes[i].turretYDisp[2] = (*shipHandle)->turretYDisp[2];
			gShipTypes[i].turretYDisp[3] = (*shipHandle)->turretYDisp[3];
			weapType[1] = (*shipHandle)->weapType[1];
			weapType[0] = (*shipHandle)->weapType[0];
			weapType[3] = (*shipHandle)->weapType[3];
			weapType[2] = (*shipHandle)->weapType[2];
			weapCount[1] = (*shipHandle)->weapCount[1];
			weapCount[0] = (*shipHandle)->weapCount[0];
			weapCount[3] = (*shipHandle)->weapCount[3];
			weapCount[2] = (*shipHandle)->weapCount[2];
			weapAmmo[1] = (*shipHandle)->ammoLoad[1];
			weapAmmo[0] = (*shipHandle)->ammoLoad[0];
			weapAmmo[3] = (*shipHandle)->ammoLoad[3];
			weapAmmo[2] = (*shipHandle)->ammoLoad[2];
			if (GetHandleSize((Handle)shipHandle) < sizeof(ShipResource)) {
				for (j = 0; j < 4; j++) {
					gShipTypes[i].defOutfID[j] = -1;
					gShipTypes[i].defOutfCount[j] = 0;
				}
				gShipTypes[i].fuelRegen = 0;
				gShipTypes[i].skillVar = 10;
			} else {
				gShipTypes[i].defOutfID[0] = (*shipHandle)->defaultItems[0];
				gShipTypes[i].defOutfID[1] = (*shipHandle)->defaultItems[1];
				gShipTypes[i].defOutfID[2] = (*shipHandle)->defaultItems[2];
				gShipTypes[i].defOutfID[3] = (*shipHandle)->defaultItems[3];
				gShipTypes[i].defOutfCount[0] = (*shipHandle)->itemCount[0];
				gShipTypes[i].defOutfCount[1] = (*shipHandle)->itemCount[1];
				gShipTypes[i].defOutfCount[2] = (*shipHandle)->itemCount[2];
				gShipTypes[i].defOutfCount[3] = (*shipHandle)->itemCount[3];
				for (j = 0; j < 4; j++) {
					if (gShipTypes[i].defOutfID[j] >= FirstResourceID)
						gShipTypes[i].defOutfID[j] -= FirstResourceID;
					else {
						gShipTypes[i].defOutfID[j] = -1;
						gShipTypes[i].defOutfID[j] = -1;
						gShipTypes[i].defOutfCount[j] = 0;
					}
				}
				gShipTypes[i].fuelRegen = (*shipHandle)->fuelRegen;
				gShipTypes[i].skillVar = (*shipHandle)->skillVar;
				if (gShipTypes[i].skillVar < 1)
					gShipTypes[i].skillVar = 1;
				if (gShipTypes[i].skillVar > 50)
					gShipTypes[i].skillVar = 50;
			}
			for (j = 0; j <= 3; j++) {
				if (weapType[j] >= FirstResourceID) {
					gShipTypes[i].weapCount[weapType[j] - FirstResourceID] = weapCount[j];
					gShipTypes[i].weapAmmo[weapType[j] - FirstResourceID] = weapAmmo[j];
					for (n = 0; n < MaxOutfitItemTypes; n++) {
						for (m = 0; m <= 1; m++) {
							if (gOutfits[n].modType[m] == kOutfModWeapon && weapCount[j] > 0
									&& gOutfits[n].modVal[m] == weapType[j] - FirstResourceID)
								gShipTypes[i].freeMass += gOutfits[n].mass * weapCount[j];
							if (gOutfits[n].modType[m] == kOutfModAmmo && weapAmmo[j] > 0
									&& gOutfits[n].modVal[m] == weapType[j] - FirstResourceID)
								gShipTypes[i].freeMass += gOutfits[n].mass * weapAmmo[j];
						}
					}
				}
			}
			if (gShipTypes[i].flags & kShipJump75)
				gShipTypes[i].jumpSpeedScale = 0.7;
			else if (gShipTypes[i].flags & kShipJump125)
				gShipTypes[i].jumpSpeedScale = 1.3;
			else if (gShipTypes[i].flags & kShipJump150)
				gShipTypes[i].jumpSpeedScale = 1.6;
			else
				gShipTypes[i].jumpSpeedScale = 1.0;
			if (gShipTypes[i].flags & kShipUseShotXOffset)
				gShipTypes[i].shotXOffset = (*shipHandle)->shotXOffset;
			else
				gShipTypes[i].shotXOffset = 0;
			gShipTypes[i].jumpSpeedScale *= 1.3f;
			GetResInfo((Handle)shipHandle, &resID, &resType, name);
			strncpy((char *)gShipTypes[i].name, (char *)name, 63);
			HPurge((Handle)shipHandle);
			ReleaseResource((Handle)shipHandle);
		} else {
			gShipTypes[i].techLevel = 9999;
			gShipTypes[i].shield = 1;
		}
	}

	for (i = 0; i < MaxWeaponTypes; i++) {
		weapHandle = (WeapResHandle)GetResource(kWeapType, i + FirstResourceID);
		if (weapHandle != NULL) {
			HNoPurge((Handle)weapHandle);
			gWeapons[i].reload = (*weapHandle)->reload;
			gWeapons[i].lifetime = (*weapHandle)->count;
			gWeapons[i].massDmg = (*weapHandle)->massDmg;
			gWeapons[i].energyDmg = (*weapHandle)->energyDmg;
			gWeapons[i].guidance = (*weapHandle)->guidance;
			gWeapons[i].speed = (*weapHandle)->speed / 100.0;
			gWeapons[i].ammoType = (*weapHandle)->ammoType;
			gWeapons[i].graphic = (*weapHandle)->graphic;
			gWeapons[i].inaccuracy = (*weapHandle)->inaccuracy;
			gWeapons[i].sound = (*weapHandle)->sound;
			gWeapons[i].impact = (*weapHandle)->impact;
			gWeapons[i].explodType = (*weapHandle)->explodType;
			gWeapons[i].proxRadius = (*weapHandle)->proxRadius;
			gWeapons[i].blastRadius = (*weapHandle)->blastRadius;
			gWeapons[i].flags = (*weapHandle)->flags;
			gWeapons[i].seeker = (*weapHandle)->seeker;
			GetResInfo((Handle)weapHandle, &resID, &resType, name);
			strcpy((char *)weapNames[i], (char *)name);
			if (GetHandleSize((Handle)weapHandle) < sizeof(WeapResource)) {
				gWeapons[i].smokeSet = 0;
				gWeapons[i].decay = 0;
			} else {
				gWeapons[i].smokeSet = (*weapHandle)->smokeSet;
				gWeapons[i].decay = (*weapHandle)->decay;
			}
			if (gWeapons[i].smokeSet < 0 || gWeapons[i].smokeSet >= 8)
				gWeapons[i].smokeSet = 0;
			if (gWeapons[i].guidance == 2) {
				gWeapons[i].guidance = kGuidanceHoming;
				gWeapons[i].seeker = kHeatSeekerFlags;
			}
			HPurge((Handle)weapHandle);
			ReleaseResource((Handle)weapHandle);
		}
	}

	for (i = 0; i < MaxDudeTypes; i++) {
		dudeHandle = (DudeResHandle)GetResource(kDudeType, i + FirstResourceID);
		if (dudeHandle != NULL) {
			HNoPurge((Handle)dudeHandle);
			dudeData[i].aiType = (*dudeHandle)->aiType;
			dudeData[i].govt = (*dudeHandle)->govt;
			if (dudeData[i].govt != -1)
				dudeData[i].govt -= FirstResourceID;
			dudeData[i].shipTypes[1] = (*dudeHandle)->shipTypes[1];
			dudeData[i].shipTypes[0] = (*dudeHandle)->shipTypes[0];
			dudeData[i].shipTypes[3] = (*dudeHandle)->shipTypes[3];
			dudeData[i].shipTypes[2] = (*dudeHandle)->shipTypes[2];
			dudeData[i].shipProbs[1] = (*dudeHandle)->prob[1];
			dudeData[i].shipProbs[0] = (*dudeHandle)->prob[0];
			dudeData[i].shipProbs[3] = (*dudeHandle)->prob[3];
			dudeData[i].shipProbs[2] = (*dudeHandle)->prob[2];
			dudeData[i].flags = (*dudeHandle)->booty;
			dudeData[i].adviceFlags = (*dudeHandle)->infoTypes;
			for (j = 0; j <= 3; j++) {
				if (dudeData[i].shipTypes[j] != -1)
					dudeData[i].shipTypes[j] -= FirstResourceID;
				else
					dudeData[i].missionBit[j] = -1;
			}
			if (GetHandleSize((Handle)dudeHandle) < sizeof(DudeResource)) {
				for (j = 0; j <= 3; j++)
					dudeData[i].missionBit[j] = -1;
			} else {
				dudeData[i].missionBit[1] = (*dudeHandle)->missionBit[1];
				dudeData[i].missionBit[0] = (*dudeHandle)->missionBit[0];
				dudeData[i].missionBit[3] = (*dudeHandle)->missionBit[3];
				dudeData[i].missionBit[2] = (*dudeHandle)->missionBit[2];
			}
			HPurge((Handle)dudeHandle);
			ReleaseResource((Handle)dudeHandle);
		}
	}

	for (i = 0; i < MaxGovts; i++) {
		govtHandle = (GovtResHandle)GetResource(kGovtType, i + FirstResourceID);
		if (govtHandle != NULL) {
			HNoPurge((Handle)govtHandle);
			govtData[i].flags = (*govtHandle)->flags;
			govtData[i].ally = (*govtHandle)->ally;
			govtData[i].enemy = (*govtHandle)->enemy;
			govtData[i].crimeTol = (*govtHandle)->crimeTol;
			govtData[i].initialRec = (*govtHandle)->initialRec;
			govtData[i].inherentJam = (*govtHandle)->inherentJam;
			if (govtData[i].ally != -1)
				govtData[i].ally -= FirstResourceID;
			if (govtData[i].enemy != -1)
				govtData[i].enemy -= FirstResourceID;
			for (j = 0; j <= 4; j++)
				govtData[i].penalties[j] = (&(*govtHandle)->smugPenalty)[j];
			GetResInfo((Handle)govtHandle, &resID, &resType, name);
			strncpy((char *)govtData[i].name, (char *)name, 31);
			HPurge((Handle)govtHandle);
			ReleaseResource((Handle)govtHandle);
		}
	}

	for (i = 0; i < MaxPersonTypes - 1; i++) {
		personData[i].loaded = false;
		personData[i].aiType = -1;
		persHandle = (PersResHandle)GetResource(kPersType, i + FirstResourceID);
		if (persHandle != NULL) {
			HNoPurge((Handle)persHandle);
			personData[i].linkSyst = (*persHandle)->linkSyst;
			personData[i].govt = (*persHandle)->govt;
			personData[i].aiType = (*persHandle)->aiType;
			personData[i].aggress = (*persHandle)->aggress;
			personData[i].coward = (*persHandle)->coward;
			personData[i].shipType = (*persHandle)->shipType - FirstResourceID;
			personData[i].credits = (*persHandle)->credits;
			personData[i].shieldMod = (*persHandle)->shieldMod;
			personData[i].shieldMod = personData[i].shieldMod / 100.0;
			personData[i].commQuote = (*persHandle)->commQuote;
			personData[i].hailQuote = (*persHandle)->hailQuote;
			personData[i].linkMission = (*persHandle)->linkMission;
			personData[i].flags = (*persHandle)->flags;
			personData[i].missionBit = (*persHandle)->missionBit;
			if (personData[i].linkMission >= FirstResourceID)
				personData[i].linkMission -= FirstResourceID;
			else
				personData[i].linkMission = -1;
			if (personData[i].govt != -1)
				personData[i].govt -= FirstResourceID;
			weapType[1] = (*persHandle)->weapType[1];
			weapType[0] = (*persHandle)->weapType[0];
			weapType[3] = (*persHandle)->weapType[3];
			weapType[2] = (*persHandle)->weapType[2];
			weapCount[1] = (*persHandle)->weapCount[1];
			weapCount[0] = (*persHandle)->weapCount[0];
			weapCount[3] = (*persHandle)->weapCount[3];
			weapCount[2] = (*persHandle)->weapCount[2];
			weapAmmo[1] = (*persHandle)->ammoLoad[1];
			weapAmmo[0] = (*persHandle)->ammoLoad[0];
			weapAmmo[3] = (*persHandle)->ammoLoad[3];
			weapAmmo[2] = (*persHandle)->ammoLoad[2];
			for (j = 0; j < MaxWeaponTypes; j++) {
				personData[i].weapCount[j] = 0;
				personData[i].weapAmmo[j] = 0;
			}
			for (j = 0; j <= 3; j++) {
				if (weapType[j] >= FirstResourceID) {
					personData[i].weapCount[weapType[j] - FirstResourceID] = weapCount[j];
					personData[i].weapAmmo[weapType[j] - FirstResourceID] = weapAmmo[j];
				}
			}
			personData[i].loaded = true;
			GetResInfo((Handle)persHandle, &resID, &resType, name);
			strncpy((char *)personData[i].name, (char *)name, 30);
			HPurge((Handle)persHandle);
			ReleaseResource((Handle)persHandle);
		}
	}
	i = MaxPersonTypes - 1;
	personData[i].linkSyst = -1;
	personData[i].govt = -1;
	personData[i].aiType = kAIWarship;
	personData[i].aggress = 4;
	personData[i].coward = -1;
	personData[i].shipType = 15;
	personData[i].credits = 0;
	personData[i].shieldMod = 10.0;
	personData[i].commQuote = 8;
	personData[i].hailQuote = -1;
	personData[i].linkMission = -1;
	personData[i].flags = kPersPodAndAfterburner;
	personData[i].missionBit = -1;
	personData[i].weapCount[8] += 2;
	personData[i].weapAmmo[8] += 100;
	personData[i].weapCount[14] += 4;
	personData[i].weapAmmo[14] += 0;
	personData[i].weapCount[12] += 2;
	personData[i].weapAmmo[12] += 250;
	personData[i].loaded = true;
	strncpy((char *)personData[i].name, (char *)"\pCap'n Hector", 30);

	for (i = 0; i < MaxDisasters; i++) {
		disasterData[i].freq = -1;
		disasterData[i].countdown = -1;
		disasterData[i].stellarID = -1;
		disasterData[i].stellar = kLowestValue;
		disasterData[i].missionBit = -1;
		oopsHandle = (OopsResHandle)GetResource(kOopsType, i + FirstResourceID);
		if (oopsHandle != NULL) {
			HNoPurge((Handle)oopsHandle);
			disasterData[i].stellar = (*oopsHandle)->stellar;
			disasterData[i].commodity = (*oopsHandle)->commodity;
			disasterData[i].amount = (*oopsHandle)->priceDelta;
			disasterData[i].duration = (*oopsHandle)->duration;
			disasterData[i].freq = (*oopsHandle)->freq;
			disasterData[i].countdown = 0;
			disasterData[i].missionBit = (*oopsHandle)->missionBit;
			if (disasterData[i].missionBit < 0 || disasterData[i].missionBit >= NumMissionBits)
				disasterData[i].missionBit = -1;
			GetResInfo((Handle)oopsHandle, &resID, &resType, name);
			strncpy((char *)disasterData[i].name, (char *)name, 63);
			HPurge((Handle)oopsHandle);
			ReleaseResource((Handle)oopsHandle);
		}
	}

	for (i = 0; i < MaxNebulae; i++) {
		nebulaData[i].width = nebulaData[i].height = 0;
		nebulaData[i].explored = false;
		nebuHandle = (NebuResHandle)GetResource(kNebuType, i + FirstResourceID);
		if (nebuHandle != NULL) {
			HNoPurge((Handle)nebuHandle);
			nebulaData[i].xPos = (*nebuHandle)->xPos;
			nebulaData[i].yPos = (*nebuHandle)->yPos;
			nebulaData[i].width = (*nebuHandle)->xSize;
			nebulaData[i].height = (*nebuHandle)->ySize;
			HPurge((Handle)nebuHandle);
			ReleaseResource((Handle)nebuHandle);
		}
	}

	for (i = 0; i < MaxFleets; i++) {
		fleetData[i].leadShipType = -1;
		fletHandle = (FletResHandle)GetResource(kFletType, i + FirstResourceID);
		if (fletHandle != NULL) {
			HNoPurge((Handle)fletHandle);
			fleetData[i].leadShipType = (*fletHandle)->leadShipType - FirstResourceID;
			fleetData[i].govt = (*fletHandle)->govt;
			fleetData[i].linkSyst = (*fletHandle)->linkSyst;
			if (fleetData[i].govt >= FirstResourceID)
				fleetData[i].govt -= FirstResourceID;
			for (j = 0; j <= 3; j++) {
				fleetData[i].escortTypes[j] = (*fletHandle)->escortType[j] - FirstResourceID;
				fleetData[i].escortMin[j] = (*fletHandle)->min[j];
				fleetData[i].escortMax[j] = (*fletHandle)->max[j];
			}
			if (GetHandleSize((Handle)fletHandle) < sizeof(FletResource))
				fleetData[i].missionBit = -1;
			else
				fleetData[i].missionBit = (*fletHandle)->missionBit;
			HPurge((Handle)fletHandle);
			ReleaseResource((Handle)fletHandle);
		}
	}

	for (i = 0; i < kMaxJunkTypes; i++) {
		junkData[i].boughtAt = -1;
		junkData[i].soldAt = -1;
		junkData[i].held = 0;
		junkHandle = (JunkResHandle)GetResource(kJunkType, i + FirstResourceID);
		if (junkHandle != NULL) {
			HNoPurge((Handle)junkHandle);
			junkData[i].boughtAt = (*junkHandle)->boughtAt;
			junkData[i].soldAt = (*junkHandle)->soldAt;
			junkData[i].basePrice = (*junkHandle)->basePrice;
			junkData[i].flags = (*junkHandle)->flags;
			if (junkData[i].boughtAt >= FirstResourceID)
				junkData[i].boughtAt -= FirstResourceID;
			if (junkData[i].soldAt >= FirstResourceID)
				junkData[i].soldAt -= FirstResourceID;
			GetResInfo((Handle)junkHandle, &resID, &resType, name);
			strncpy((char *)junkData[i].name, (char *)name, 63);
			HPurge((Handle)junkHandle);
			ReleaseResource((Handle)junkHandle);
		}
	}

	for (i = 0; i < MaxMovies; i++) {
		movieData[i].flags = 0;
		movieData[i].movieId = -1;
	}
	m = CountResources(kDeqtType);
	if (m > 0) {
		for (i = 0; i < m; i++) {
			if (i >= MaxMovies)
				return;
			deqtHandle = (DeqtResHandle)GetIndResource(kDeqtType, i + 1);
			if (deqtHandle != NULL) {
				HNoPurge((Handle)deqtHandle);
				movieData[i].flags = (*deqtHandle)->flags;
				GetResInfo((Handle)deqtHandle, &resID, &resType, name);
				strncpy((char *)movieData[i].name, (char *)name, 63);
				movieData[i].movieId = resID;
				HPurge((Handle)deqtHandle);
				ReleaseResource((Handle)deqtHandle);
			}
		}
	}
}

// FUNCTION: EVO 0x100197D8
void LoadDescription(char *text, short id)
{
	descHandle = GetResource(kDescType, id);
	if (descHandle != NULL) {
		HNoPurge(descHandle);
		strcpy(text, *descHandle);
		HPurge(descHandle);
		ReleaseResource(descHandle);
	} else
		strcpy(text, "");
}

#define kPlugCargoName	9000
#define kStrsCargoName	4000
#define kPlugStatusCargoName	9400
#define kStrsStatusCargoName	4003
#define kPlugLcCargoName	9100
#define kStrsLcCargoName	4001
#define kPlugOutfitShortName	3000
#define kStrsOutfitShortName	5000
#define kPlugShipyardName	3600
#define kStrsShipyardName	5001
#define kPlugLongShipName	3700
#define kStrsLongShipName	5002
#define kPlugLcOutfitName	3200
#define kStrsLcOutfitName	5004
#define kPlugLcOutfitPluralName	3400
#define kStrsLcOutfitPluralName	5005
#define kPlugStatusJunkName	8000
#define kStrsStatusJunkName	4005
#define kPlugLcJunkName	8200
#define kStrsLcJunkName	4006
#define kPlugGovtCode	4000
#define kStrsGovtCode	6000
#define kPlugCargoAbbrev	9200
#define kStrsCargoAbbrev	4002
#define kStrsShipHelp	5003
#define kStrsLegal	134
#define kStrsVolumeNames	136
#define kStrsMonthNames	137
#define kStrsRatings	138

// FUNCTION: EVO 0x10019880
void LoadStrings(void)
{
	Str255	str;
	long	unused;
	short	i;

	for (i = 0; i < MaxCargoTypes; i++) {
		if (!LoadPluginString(i + kPlugCargoName, cargoNames[i])) {
			GetIndString(str, kStrsCargoName, i + 1);
			strcpy((char *)cargoNames[i], (char *)str);
		}
	}
	for (i = 0; i <= 5; i++) {
		if (!LoadPluginString(i + kPlugStatusCargoName, statusCargoName[i])) {
			GetIndString(str, kStrsStatusCargoName, i + 1);
			strcpy((char *)statusCargoName[i], (char *)str);
		}
	}
	for (i = 0; i < MaxCargoTypes; i++) {
		if (!LoadPluginString(i + kPlugLcCargoName, (StringPtr)lcCargoName[i])) {
			GetIndString(str, kStrsLcCargoName, i + 1);
			strcpy(lcCargoName[i], (char *)str);
		}
	}
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		if (!LoadPluginString(i + kPlugOutfitShortName, (StringPtr)outfitShortName[i])) {
			GetIndString(str, kStrsOutfitShortName, i + 1);
			strcpy(outfitShortName[i], (char *)str);
		}
	}
	for (i = 0; i < MaxShipClasses; i++) {
		if (!LoadPluginString(i + kPlugShipyardName, shipyardName[i])) {
			GetIndString(str, kStrsShipyardName, i + 1);
			strcpy((char *)shipyardName[i], (char *)str);
		}
	}
	for (i = 0; i < MaxShipClasses; i++) {
		if (!LoadPluginString(i + kPlugLongShipName, longShipName[i])) {
			GetIndString(str, kStrsLongShipName, i + 1);
			strcpy((char *)longShipName[i], (char *)str);
		}
	}
	for (i = 0; i < 20; i++) {
		GetIndString(str, kStrsShipHelp, i + 1);
		strcpy((char *)shipHelpRequests[i], (char *)str);
	}
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		if (!LoadPluginString(i + kPlugLcOutfitName, lcOutfitName[i])) {
			GetIndString(str, kStrsLcOutfitName, i + 1);
			strcpy((char *)lcOutfitName[i], (char *)str);
		}
	}
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		if (!LoadPluginString(i + kPlugLcOutfitPluralName, lcOutfitPluralName[i])) {
			GetIndString(str, kStrsLcOutfitPluralName, i + 1);
			strcpy((char *)lcOutfitPluralName[i], (char *)str);
		}
	}
	for (i = 0; i < kMaxJunkTypes; i++) {
		if (!LoadPluginString(i + kPlugStatusJunkName, statusJunkName[i])) {
			GetIndString(str, kStrsStatusJunkName, i + 1);
			strcpy((char *)statusJunkName[i], (char *)str);
		}
	}
	for (i = 0; i < kMaxJunkTypes; i++) {
		if (!LoadPluginString(i + kPlugLcJunkName, (StringPtr)lcJunkName[i])) {
			GetIndString(str, kStrsLcJunkName, i + 1);
			strcpy(lcJunkName[i], (char *)str);
		}
	}
	for (i = 0; i <= 17; i++) {
		GetIndString(str, kStrsLegal, i + 1);
		strcpy((char *)legalStrings[i], (char *)str);
	}
	for (i = 0; i <= 8; i++) {
		GetIndString(str, kStrsVolumeNames, i + 1);
		strcpy((char *)volumeNames[i], (char *)str);
	}
	for (i = 0; i <= 15; i++) {
		GetIndString(str, kStrsMonthNames, i + 1);
		strcpy((char *)monthNames[i], (char *)str);
	}
	for (i = 0; i <= 10; i++) {
		GetIndString(str, kStrsRatings, i + 1);
		strcpy((char *)ratingStrings[i], (char *)str);
	}
	for (i = 0; i < MaxGovts; i++) {
		if (!LoadPluginString(i + kPlugGovtCode, govtTransponderCode[i])) {
			GetIndString(str, kStrsGovtCode, i + 1);
			strcpy((char *)govtTransponderCode[i], (char *)str);
		}
	}
	for (i = 0; i < MaxCargoTypes; i++) {
		if (!LoadPluginString(i + kPlugCargoAbbrev, cargoAbbrev[i])) {
			GetIndString(str, kStrsCargoAbbrev, i + 1);
			strcpy((char *)cargoAbbrev[i], (char *)str);
		}
	}
}

// FUNCTION: EVO 0x10019EEC
Boolean LoadPluginString(short id, Str255 str)
{
	Handle	h;

	h = NULL;
	h = GetResource('STR ', id);
	if (h != NULL) {
		HNoPurge(h);
		BlockMoveData(*h, str, GetHandleSize(h));
		HPurge(h);
		ReleaseResource(h);
		return true;
	}
	return false;
}

// FUNCTION: EVO 0x10019F88
void LoadPrefs(void)
{
	FSSpec		spec;
	long		unused;
	Str255		name;
	DateTimeRec	date;
	OSErr		err;
	short		version;
	short		i;

	GetTime(&date);
	GetIndString(name, kDataFileStrList, kPrefsNameIndex);
	err = FSMakeFSSpec(prefsVRefNum, prefsDirID, name, &spec);
	if (err == fnfErr) {
		DefaultPrefs();
		DefaultKeys();
		MeasureProcessorSpeed();
	} else if (err == noErr) {
		prefsRefNum = FSpOpenResFile(&spec, fsRdWrPerm);
		if (prefsRefNum != -1) {
			prefsHandle = NULL;
			prefsHandle = (PrefsResHandle)GetResource(kPrefsType, kPrefsID);
			if (prefsHandle != NULL) {
				HNoPurge((Handle)prefsHandle);
				HLock((Handle)prefsHandle);
				settings.introMusic = settings.voicesOn = settings.highDetail = false;
				settings.noMovies = settings.useQuickdraw = settings.noStreaks = false;
				version = (*prefsHandle)->version;
				if (version == kPrefsVersion) {
					if ((*prefsHandle)->introMusic != 0)
						settings.introMusic = true;
					if ((*prefsHandle)->voicesOn != 0)
						settings.voicesOn = true;
					if ((*prefsHandle)->noMovies != 0)
						settings.noMovies = true;
					if ((*prefsHandle)->useQuickdraw != 0)
						settings.useQuickdraw = true;
					if ((*prefsHandle)->noStreaks != 0)
						settings.noStreaks = true;
					settings.soundVolume = (*prefsHandle)->soundVolume;
					if ((*prefsHandle)->prefsChanged == 0)
						prefsChanged = false;
					else
						prefsChanged = true;
					DefaultKeys();
					for (i = 0; i <= kLastPrefKey; i++)
						newKeyPrefs[i] = (*prefsHandle)->keys[i];
					LoadKeyAssignments();
					gameSpeed = (*prefsHandle)->gameSpeed;
					gameSpeed *= 0.01;
				}
				HUnlock((Handle)prefsHandle);
				HPurge((Handle)prefsHandle);
				CloseResFile(prefsRefNum);
				if (version != kPrefsVersion) {
					err = FSMakeFSSpec(prefsVRefNum, prefsDirID, "\pEscape Velocity Prefs", &spec);
					if (err == noErr) {
						err = FSpDelete(&spec);
						if (err == noErr) {
							DefaultPrefs();
							MeasureProcessorSpeed();
							DefaultKeys();
							FadeScreenIn(8);
							EVNoteAlert("\pThe Override Preferences file in your system folder appears to have been created with an older version of Override. It\325s now been updated, but you\325ll have to redo your settings in the preferences dialog.");
							FadeScreenOut(8, &rgbBlack);
						} else {
							FadeScreenIn(8);
							EVNoteAlert("\pThe Override Preferences file in your system folder was created with an older version of Override. We tried to delete it but couldn\325t - please remove it and restart Override.");
							ShowTheMenuBar();
							TeardownSoundSubsystem();
							CloseMonitorTool();
							ExitToShell();
						}
					} else {
						FadeScreenIn(8);
						EVNoteAlert("\pThe Override Preferences file in your system folder was created with an older version of Override. We tried to delete it but couldn\325t - please remove it and restart Override.");
						ShowTheMenuBar();
						TeardownSoundSubsystem();
						CloseMonitorTool();
						ExitToShell();
					}
				}
			} else {
				CloseResFile(prefsRefNum);
				DefaultPrefs();
				DefaultKeys();
				MeasureProcessorSpeed();
			}
		} else {
			DefaultPrefs();
			DefaultKeys();
			MeasureProcessorSpeed();
		}
	} else {
		DefaultPrefs();
		DefaultKeys();
		MeasureProcessorSpeed();
	}
}

// FUNCTION: EVO 0x1001A3B8
void SavePrefs(void)
{
	Str255	name;
	FSSpec	spec;
	long	unused;
	short	saveRes;
	OSErr	err;
	short	i;

	prefsHandle = NULL;
	saveRes = CurResFile();
	GetIndString(name, kDataFileStrList, kPrefsNameIndex);
	err = FSMakeFSSpec(prefsVRefNum, prefsDirID, name, &spec);
	if (err == fnfErr) {
		FSpCreateResFile(&spec, kCreator, kPrefsFileType, smRoman);
		prefsRefNum = FSpOpenResFile(&spec, fsRdWrPerm);
		if (prefsRefNum == -1)
			return;
	} else {
		err = FSpDelete(&spec);
		if (err == noErr) {
			FSpCreateResFile(&spec, kCreator, kPrefsFileType, smRoman);
			prefsRefNum = FSpOpenResFile(&spec, fsRdWrPerm);
			if (prefsRefNum == -1)
				return;
		} else
			return;
	}
	prefsHandle = (PrefsResHandle)NewHandle(sizeof(PrefsResource));
	if (prefsHandle != NULL) {
		HNoPurge((Handle)prefsHandle);
		HLock((Handle)prefsHandle);
		(*prefsHandle)->version = kPrefsVersion;
		if (settings.introMusic)
			(*prefsHandle)->introMusic = 1;
		else
			(*prefsHandle)->introMusic = 0;
		if (settings.voicesOn)
			(*prefsHandle)->voicesOn = 1;
		else
			(*prefsHandle)->voicesOn = 0;
		if (settings.noMovies)
			(*prefsHandle)->noMovies = 1;
		else
			(*prefsHandle)->noMovies = 0;
		if (settings.useQuickdraw)
			(*prefsHandle)->useQuickdraw = 1;
		else
			(*prefsHandle)->useQuickdraw = 0;
		if (settings.noStreaks)
			(*prefsHandle)->noStreaks = 1;
		else
			(*prefsHandle)->noStreaks = 0;
		(*prefsHandle)->soundVolume = settings.soundVolume;
		SaveKeyAssignments();
		for (i = 0; i <= kLastPrefKey; i++)
			(*prefsHandle)->keys[i] = newKeyPrefs[i];
		(*prefsHandle)->gameSpeed = 100.0 * gameSpeed;
		if (prefsChanged)
			(*prefsHandle)->prefsChanged = 1;
		else
			(*prefsHandle)->prefsChanged = 0;
		for (i = 0; i <= 5; i++)
			(*prefsHandle)->spare[i] = 0;
		UseResFile(prefsRefNum);
		AddResource((Handle)prefsHandle, kPrefsType, kPrefsID, "\pEV Prefs");
		UseResFile(saveRes);
		HUnlock((Handle)prefsHandle);
		HPurge((Handle)prefsHandle);
	}
	UpdateResFile(prefsRefNum);
	CloseResFile(prefsRefNum);
	FlushVol(NULL, prefsVRefNum);
}

// FUNCTION: EVO 0x1001A778
short SavePilotFile(short stellar)
{
	FSSpec	spec;
	OSErr	err;

	if (noSavePilotFiles)
		return 0;
	err = FSMakeFSSpec(pilotVRefNum, pilotDirID, playerName, &spec);
	if (err == noErr) {
		err = FSpDelete(&spec);
		if (err == noErr) {
			err = FSMakeFSSpec(pilotVRefNum, pilotDirID, playerName, &spec);
			if (err == fnfErr)
				err = WritePilotData(stellar);
		}
	} else if (err == fnfErr)
		err = WritePilotData(stellar);
	return err;
}

// FUNCTION: EVO 0x1001A868
short WritePilotData(short stellar)
{
	long	unused1;
	short	saveRes;
	OSErr	err;
	FSSpec	spec;
	long	unused2;
	Str255	name;
	short	i;
	short	j;

	if (noUpdatePilotData)
		return;
	pilotHandle = NULL;
	pilotAuxHandle = NULL;
	saveRes = CurResFile();
	err = FSMakeFSSpec(pilotVRefNum, pilotDirID, playerName, &spec);
	FSpCreateResFile(&spec, kCreator, kPilotFileType, smRoman);
	pilotRefNum = FSpOpenResFile(&spec, fsRdWrPerm);
	if (pilotRefNum == -1)
		return fnfErr;
	pilotHandle = (PilotHandle)NewHandle(sizeof(PilotRec));
	pilotAuxHandle = (PilotAuxHandle)NewHandle(sizeof(PilotAuxRec));
	if (pilotHandle != NULL && pilotAuxHandle != NULL) {
		HLock((Handle)pilotHandle);
		HLock((Handle)pilotAuxHandle);
		HNoPurge((Handle)pilotHandle);
		HNoPurge((Handle)pilotAuxHandle);
		(*pilotHandle)->stellar = stellar;
		(*pilotHandle)->shipType = shipData->type;
		for (i = 0; i <= 5; i++)
			(*pilotHandle)->cargo[i] = shipData->cargo[i];
		(*pilotHandle)->shield = shipData->shield;
		(*pilotHandle)->fuel = shipData->fuel;
		(*pilotHandle)->month = gameTime.month;
		(*pilotHandle)->day = gameTime.day;
		(*pilotHandle)->year = gameTime.year;
		for (i = 0; i < MaxSystems; i++)
			(*pilotHandle)->visited[i] = gSystems[i].visited;
		for (i = 0; i < MaxOutfitItemTypes; i++)
			(*pilotHandle)->outfits[i] = outfitCount[i];
		for (i = 0; i < MaxSystems; i++)
			(*pilotHandle)->legalStatus[i] = legalStatus[i];
		for (i = 0; i < MaxWeaponTypes; i++) {
			(*pilotHandle)->weapCount[i] = shipData->weap[i].count;
			(*pilotHandle)->weapAmmo[i] = shipData->weap[i].ammo;
		}
		(*pilotHandle)->credits = shipData->credits;
		for (j = 0; j < MaxSimultaneousMissions; j++) {
			(*pilotHandle)->objectives[j] = currentMissionObjectives[j];
			(*pilotHandle)->missions[j] = currentMissions[j];
		}
		for (i = 0; i < NumMissionBits; i++)
			(*pilotHandle)->missionBits[i] = missionBit[i];
		for (i = 0; i < MaxStellarObjects; i++)
			(*pilotHandle)->dominated[i] = stellarData[i].dominated;
		for (i = 0; i < MaxShipsInSystem; i++) {
			(*pilotHandle)->escortType[i] = -1;
			(*pilotHandle)->fighterType[i] = -1;
		}
		for (i = 0; i < MaxShipsInSystem; i++) {
			if (shipData[i].active && shipData[i].parentShip == 0 && shipData[i].aiType == kAIFighter)
				(*pilotHandle)->fighterType[i] = shipData[i].type;
			else
				(*pilotHandle)->fighterType[i] = -1;
		}
		for (i = 0; i < MaxShipsInSystem; i++) {
			if (shipData[i].active && shipData[i].parentShip == 0 && shipData[i].aiType == kAIEscort
					&& shipData[i].specialMission == -1) {
				(*pilotHandle)->escortType[i] = shipData[i].type;
				if (shipData[i].hired)
					(*pilotHandle)->escortType[i] += kHiredEscortBase;
			} else
				(*pilotHandle)->escortType[i] = -1;
		}
		(*pilotHandle)->kills = playerKills;
		(*pilotAuxHandle)->version = kPilotVersion;
		if (strictPlay)
			(*pilotAuxHandle)->strict = 1;
		else
			(*pilotAuxHandle)->strict = 0;
		for (i = 0; i < MaxStellarObjects; i++) {
			if (stellarData[i].loaded && stellarData[i].dominated) {
				(*pilotAuxHandle)->tribute[i] = stellarData[i].tribute;
				(*pilotAuxHandle)->tributeTicks[i] = stellarData[i].daysDominated;
			} else {
				(*pilotAuxHandle)->tribute[i] = stellarData[i].tribute;
				(*pilotAuxHandle)->tributeTicks[i] = 0;
			}
		}
		for (i = 0; i < MaxPersonTypes; i++) {
			if (personData[i].loaded && personData[i].aiType >= kAIWimpyTrader) {
				(*pilotAuxHandle)->persActive[i] = 1;
				if (personData[i].grudge)
					(*pilotAuxHandle)->persAccepted[i] = 1;
				else
					(*pilotAuxHandle)->persAccepted[i] = 0;
			} else {
				(*pilotAuxHandle)->persActive[i] = 0;
				(*pilotAuxHandle)->persAccepted[i] = 0;
			}
		}
		(*pilotAuxHandle)->valid = true;
		for (i = 0; i < MaxDisasters; i++) {
			(*pilotAuxHandle)->disasterCountdown[i] = disasterData[i].countdown;
			(*pilotAuxHandle)->disasterStellar[i] = disasterData[i].stellarID;
		}
		for (i = 0; i < kMaxJunkTypes; i++)
			(*pilotAuxHandle)->junkHeld[i] = junkData[i].held;
		for (i = 0; i <= 1; i++) {
			(*pilotAuxHandle)->globalPriceFlux[i] = globalPriceFlux[i];
			(*pilotAuxHandle)->localPriceFlux[i] = localPriceFlux[i];
		}
		strncpy((char *)name, (char *)shipName, 32);
		UseResFile(pilotRefNum);
		DoEncryption((Handle)pilotHandle, kPilotKey);
		DoEncryption((Handle)pilotAuxHandle, kPilotKey);
		AddResource((Handle)pilotHandle, kPilotFileType, kPilotID, "\pPilot Data");
		AddResource((Handle)pilotAuxHandle, kPilotFileType, kPilotAuxID, name);
		UseResFile(saveRes);
		UpdateResFile(pilotRefNum);
		HUnlock((Handle)pilotHandle);
		HUnlock((Handle)pilotAuxHandle);
		HPurge((Handle)pilotHandle);
		HPurge((Handle)pilotAuxHandle);
		CloseResFile(pilotRefNum);
	}
	MakeAlias(spec);
	UseResFile(saveRes);
	FlushVol(NULL, pilotVRefNum);
	return noErr;
}

// FUNCTION: EVO 0x1001B410
Boolean OpenPilotFile(void)
{
	SFTypeList			typeList;
	StandardFileReply	reply;
	OSErr				err;

	typeList[0] = kPilotFileType;
	StandardGetFile(NULL, 1, typeList, &reply);
	if (reply.sfGood) {
		DeleteAllComputerShips(true, true);
		ResetPlayer(true);
		ResetPlayerRecord(true);
		DeleteAllComputerShips(true, true);
		err = LoadPilotData(reply.sfFile.vRefNum, reply.sfFile.parID, reply.sfFile.name);
		SetCursor(&qd.arrow);
		if (err == kOldPilotErr) {
			err = LoadOldPilotData(reply.sfFile.vRefNum, reply.sfFile.parID, reply.sfFile.name);
			if (err == noErr)
				EVNoteAlert("\pThis old pilot file was imported successfully. We reccomend you start a new pilot to use with this version of EV, so don\325t be surprised if things act differently.");
			else
				EVNoteAlert("\pAn error occured while trying to import this old pilot file!");
		}
		if (err == noErr) {
			shipData->deathTimer = -1;
			gTargetStellar = -1;
			return true;
		}
		if (err == kWrongPilotErr)
			EVNoteAlert("\pThis pilot file was created with a different version of EV, and can\325t be used.");
		return false;
	}
	SetCursor(&qd.arrow);
	return false;
}

// FUNCTION: EVO 0x1001B56C
void OpenLastPilotFile(void)
{
	Str255	name;
	long	size;
	OSErr	err;
	short	i;
	short	j;

	SATSetPortScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
	FadeScreenIn(4);
	if (DoesPilotExist("\pLast Pilot")) {
		DeleteAllComputerShips(true, true);
		ResetPlayer(true);
		ResetPlayerRecord(true);
		err = LoadPilotData(pilotVRefNum, pilotDirID, "\pLast Pilot");
		if (err == noErr) {
			gameInProgress = true;
			for (i = 0; i < MaxSystems; i++) {
				if (gSystems[i].loaded && gSystems[i].visited > 0)
					ExploreNebulaeFromSystem(i);
			}
			for (j = 0; j < MaxSimultaneousMissions; j++) {
				if (currentMissionObjectives[j].active) {
					if (currentMissions[j].flags & kMisnInfiniteAux)
						currentMissions[j].auxShipsLeft = currentMissions[j].auxShipCount;
					currentMissions[j].auxShipDelay = Rand(kMissionSpawnDelay) + kMissionSpawnDelay;
					currentMissions[j].auxShipsSpawned = 0;
				}
			}
		}
	}
	FadeScreenOut(4, &rgbBlack);
}

// FUNCTION: EVO 0x1001B758
OSErr LoadPilotData(short vRefNum, long dirID, StringPtr fileName)
{
	short	resID;
	short	stellar;
	long	parID;
	FSSpec	spec;
	Str255	name;
	ResType	type;
	Handle	h;
	Boolean	isFolder;
	Boolean	wasAliased;
	Boolean	unused;
	Boolean	keepAux;
	OSErr	err;
	short	slot;
	short	i;
	short	j;

	pilotHandle = NULL;
	pilotAuxHandle = NULL;
	keepAux = false;
	strncpy((char *)name, (char *)fileName, 31);
	err = FSMakeFSSpec(vRefNum, dirID, name, &spec);
	if (err == fnfErr)
		return err;
	if (err == noErr) {
		parID = spec.parID;
		err = ResolveAliasFile(&spec, true, &isFolder, &wasAliased);
		if (err == noErr && !isFolder && spec.parID == parID) {
			pilotRefNum = FSpOpenResFile(&spec, fsRdWrPerm);
			if (pilotRefNum != -1) {
				pilotAuxHandle = (PilotAuxHandle)GetResource(kPilotFileType, kPilotAuxID);
				if (pilotAuxHandle != NULL) {
					HNoPurge((Handle)pilotAuxHandle);
					err = DoEncryption((Handle)pilotAuxHandle, kPilotKey);
					if (err == noErr) {
						if ((*pilotAuxHandle)->version == kOldPilotVersion) {
							HUnlock((Handle)pilotAuxHandle);
							HPurge((Handle)pilotAuxHandle);
							CloseResFile(pilotRefNum);
							return kOldPilotErr;
						}
						if ((*pilotAuxHandle)->version != kPilotVersion) {
							HUnlock((Handle)pilotAuxHandle);
							HPurge((Handle)pilotAuxHandle);
							CloseResFile(pilotRefNum);
							return kWrongPilotErr;
						}
						if ((*pilotAuxHandle)->strict == 1)
							strictPlay = true;
						else
							strictPlay = false;
						for (i = 0; i < MaxStellarObjects; i++) {
							if (!stellarData[i].dominated) {
								stellarData[i].tribute = (*pilotAuxHandle)->tribute[i];
								stellarData[i].daysDominated = (*pilotAuxHandle)->tributeTicks[i];
							} else {
								stellarData[i].tribute = 0;
								stellarData[i].daysDominated = 0;
							}
						}
						for (i = 0; i < MaxPersonTypes; i++) {
							if (personData[i].aiType >= kAIWimpyTrader) {
								if ((*pilotAuxHandle)->persActive[i] != 0) {
									personData[i].loaded = true;
									if ((*pilotAuxHandle)->persAccepted[i] != 0)
										personData[i].grudge = true;
									else
										personData[i].grudge = false;
								} else {
									personData[i].loaded = false;
									personData[i].grudge = false;
								}
							} else
								personData[i].loaded = false;
						}
						seenIntroScreen = true;
						for (i = 0; i < MaxDisasters; i++) {
							disasterData[i].countdown = (*pilotAuxHandle)->disasterCountdown[i];
							disasterData[i].stellarID = (*pilotAuxHandle)->disasterStellar[i];
						}
						for (i = 0; i < kMaxJunkTypes; i++)
							junkData[i].held = (*pilotAuxHandle)->junkHeld[i];
						for (i = 0; i <= 1; i++) {
							globalPriceFlux[i] = (*pilotAuxHandle)->globalPriceFlux[i];
							localPriceFlux[i] = (*pilotAuxHandle)->localPriceFlux[i];
						}
						HPurge((Handle)pilotAuxHandle);
						if (!keepAux)
							ReleaseResource((Handle)pilotAuxHandle);
					}
				} else {
					CloseResFile(pilotRefNum);
					return fnfErr;
				}
				pilotHandle = (PilotHandle)GetResource(kPilotFileType, kPilotID);
				if (pilotHandle != NULL) {
					HNoPurge((Handle)pilotHandle);
					err = DoEncryption((Handle)pilotHandle, kPilotKey);
					if (err == noErr) {
						stellar = (*pilotHandle)->stellar;
						shipData->lastStellar = stellar;
						if (stellar == -1)
							stellar = 0;
						shipData->curSystem = stellarData[stellar].system;
						shipData->pos.x = stellarData[stellar].pos.h;
						shipData->pos.y = stellarData[stellar].pos.v;
						shipData->heading = Rand(kFullCircle);
						mapCenter.h = gSystems[shipData->curSystem].xPos;
						mapCenter.v = gSystems[shipData->curSystem].yPos;
						shipData->type = (*pilotHandle)->shipType;
						for (i = 0; i <= 5; i++)
							shipData->cargo[i] = (*pilotHandle)->cargo[i];
						for (i = 0; i < MaxOutfitItemTypes; i++)
							outfitCount[i] = (*pilotHandle)->outfits[i];
						shipData->shield = ShipShieldCapacity(shipData);
						shipData->fuel = (*pilotHandle)->fuel;
						gameTime.month = (*pilotHandle)->month;
						gameTime.day = (*pilotHandle)->day;
						gameTime.year = (*pilotHandle)->year;
						for (i = 0; i < MaxSystems; i++)
							gSystems[i].visited = (*pilotHandle)->visited[i];
						for (i = 0; i < MaxSystems; i++)
							legalStatus[i] = (*pilotHandle)->legalStatus[i];
						for (i = 0; i < MaxWeaponTypes; i++) {
							shipData->weap[i].count = (*pilotHandle)->weapCount[i];
							shipData->weap[i].ammo = (*pilotHandle)->weapAmmo[i];
						}
						shipData->credits = (*pilotHandle)->credits;
						playerKills = (*pilotHandle)->kills;
						for (j = 0; j < MaxSimultaneousMissions; j++) {
							currentMissionObjectives[j] = (*pilotHandle)->objectives[j];
							currentMissions[j] = (*pilotHandle)->missions[j];
							if (currentMissionObjectives[j].active) {
								SetMissionDeadline(&currentMissionObjectives[j].dueDate, currentMissions[j].timeLimit);
								strncpy(currentMissions[j].shipName, (char *)"\p", 31);
								if (currentMissions[j].shipStrList >= FirstResourceID && currentMissions[j].shipStrIndex >= 1) {
									GetIndString(name, currentMissions[j].shipStrList, currentMissions[j].shipStrIndex);
									strncpy(currentMissions[j].shipName, (char *)name, 31);
								}
								if (currentMissions[j].flags & kMisnInfiniteAux)
									currentMissions[j].auxShipsLeft = currentMissions[j].auxShipCount;
								currentMissions[j].auxShipDelay = Rand(kMissionSpawnDelay) + kMissionSpawnDelay;
								currentMissions[j].auxShipsSpawned = 0;
							}
						}
						for (i = 0; i < NumMissionBits; i++)
							missionBit[i] = (*pilotHandle)->missionBits[i];
						for (i = 0; i < MaxStellarObjects; i++)
							stellarData[i].dominated = (*pilotHandle)->dominated[i];
						DeleteAllComputerShips(true, true);
						for (i = 0; i < MaxShipsInSystem; i++) {
							if ((*pilotHandle)->escortType[i] >= 0) {
								if ((*pilotHandle)->escortType[i] < kHiredEscortBase) {
									slot = SpawnEscort((*pilotHandle)->escortType[i], -1);
									shipData[slot].hired = false;
								} else {
									slot = SpawnEscort((*pilotHandle)->escortType[i] - kHiredEscortBase, -1);
									shipData[slot].hired = true;
								}
							}
							if ((*pilotHandle)->fighterType[i] >= 0 && (*pilotHandle)->fighterType[i] < MaxShipClasses) {
								slot = SpawnEscort((*pilotHandle)->fighterType[i], -1);
								if (slot != -1) {
									shipData[slot].aiType = kAIFighter;
									shipData[slot].hired = false;
									AISetup(&shipData[slot]);
								}
							}
						}
						HPurge((Handle)pilotHandle);
						ReleaseResource((Handle)pilotHandle);
					}
				} else {
					CloseResFile(pilotRefNum);
					return fnfErr;
				}
				h = GetResource(kPilotFileType, kPilotAuxID);
				if (h != NULL) {
					HNoPurge(h);
					GetResInfo(h, &resID, &type, name);
					strncpy((char *)shipName, (char *)name, 64);
					HPurge(h);
					ReleaseResource(h);
				}
				strncpy((char *)playerName, (char *)spec.name, 31);
				CloseResFile(pilotRefNum);
				MakeAlias(spec);
			} else
				return fnfErr;
		} else
			return fnfErr;
	} else
		return err;
	return noErr;
}

// FUNCTION: EVO 0x1001C5FC
OSErr LoadOldPilotData(short vRefNum, long dirID, StringPtr fileName)
{
	short			resID;
	short			stellar;
	long			parID;
	FSSpec			spec;
	Str255			name;
	ResType			type;
	Handle			h;
	Boolean			isFolder;
	Boolean			wasAliased;
	Boolean			unused;
	OSErr			err;
	short			slot;
	OldPilotHandle	pilot;
	OldPilotAuxHandle	aux;
	short			i;
	short			j;

	pilot = NULL;
	strcpy((char *)name, (char *)fileName);
	err = FSMakeFSSpec(vRefNum, dirID, name, &spec);
	if (err == fnfErr)
		return err;
	if (err == noErr) {
		parID = spec.parID;
		err = ResolveAliasFile(&spec, true, &isFolder, &wasAliased);
		if (err == noErr && !isFolder && spec.parID == parID) {
			pilotRefNum = FSpOpenResFile(&spec, fsRdWrPerm);
			if (pilotRefNum != -1) {
				aux = (OldPilotAuxHandle)GetResource(kPilotFileType, kPilotAuxID);
				if (aux != NULL) {
					HNoPurge((Handle)aux);
					err = DoEncryption((Handle)aux, kPilotKey);
					if (err == noErr) {
						if ((*aux)->strict == 1)
							strictPlay = true;
						else
							strictPlay = false;
						for (i = 0; i < MaxStellarObjects; i++) {
							if (!stellarData[i].dominated) {
								stellarData[i].tribute = (*aux)->tribute[i];
								stellarData[i].daysDominated = (*aux)->tributeTicks[i];
							} else {
								stellarData[i].tribute = 0;
								stellarData[i].daysDominated = 0;
							}
						}
						for (i = 0; i < MaxPersonTypes; i++) {
							personData[i].loaded = false;
							personData[i].grudge = false;
						}
						for (i = 0; i < kOldMaxPersons; i++) {
							if (personData[i].aiType >= kAIWimpyTrader) {
								if ((*aux)->persActive[i] != 0) {
									personData[i].loaded = true;
									if ((*aux)->persAccepted[i] != 0)
										personData[i].grudge = true;
									else
										personData[i].grudge = false;
								} else {
									personData[i].loaded = false;
									personData[i].grudge = false;
								}
							} else
								personData[i].loaded = false;
						}
						seenIntroScreen = true;
						for (i = 0; i < MaxDisasters; i++) {
							disasterData[i].countdown = (*aux)->disasterCountdown[i];
							disasterData[i].stellarID = (*aux)->disasterStellar[i];
						}
						for (i = 0; i < kMaxJunkTypes; i++)
							junkData[i].held = (*aux)->junkHeld[i];
						for (i = 0; i <= 1; i++) {
							globalPriceFlux[i] = (*aux)->globalPriceFlux[i];
							localPriceFlux[i] = (*aux)->localPriceFlux[i];
						}
						HPurge((Handle)aux);
						ReleaseResource((Handle)aux);
					}
				} else {
					CloseResFile(pilotRefNum);
					return fnfErr;
				}
				pilot = (OldPilotHandle)GetResource(kPilotFileType, kPilotID);
				if (pilot != NULL) {
					HNoPurge((Handle)pilot);
					err = DoEncryption((Handle)pilot, kPilotKey);
					if (err == noErr) {
						stellar = (*pilot)->stellar;
						shipData->lastStellar = stellar;
						if (stellar == -1)
							stellar = 0;
						shipData->curSystem = stellarData[stellar].system;
						shipData->pos.x = stellarData[stellar].pos.h;
						shipData->pos.y = stellarData[stellar].pos.v;
						shipData->heading = Rand(kFullCircle);
						mapCenter.h = gSystems[shipData->curSystem].xPos;
						mapCenter.v = gSystems[shipData->curSystem].yPos;
						shipData->type = (*pilot)->shipType;
						for (i = 0; i <= 5; i++)
							shipData->cargo[i] = (*pilot)->cargo[i];
						shipData->shield = (*pilot)->shield;
						shipData->fuel = (*pilot)->fuel;
						gameTime.month = (*pilot)->month;
						gameTime.day = (*pilot)->day;
						gameTime.year = (*pilot)->year;
						for (i = 0; i < MaxSystems; i++)
							gSystems[i].visited = (*pilot)->visited[i];
						for (i = 0; i < MaxOutfitItemTypes; i++)
							outfitCount[i] = 0;
						for (i = 0; i < kOldMaxOutfits; i++)
							outfitCount[i] = (*pilot)->outfits[i];
						for (i = 0; i < MaxSystems; i++)
							legalStatus[i] = (*pilot)->legalStatus[i];
						for (i = 0; i < MaxWeaponTypes; i++) {
							shipData->weap[i].count = 0;
							shipData->weap[i].ammo = 0;
						}
						for (i = 0; i < kOldMaxWeapons; i++) {
							shipData->weap[i].count = (*pilot)->weapCount[i];
							shipData->weap[i].ammo = (*pilot)->weapAmmo[i];
						}
						shipData->credits = (*pilot)->credits;
						playerKills = (*pilot)->kills;
						for (j = 0; j < MaxSimultaneousMissions; j++) {
							currentMissionObjectives[j] = (*pilot)->objectives[j];
							currentMissions[j] = (*pilot)->missions[j];
							SetMissionDeadline(&currentMissionObjectives[j].dueDate, currentMissions[j].timeLimit);
							strncpy(currentMissions[j].shipName, (char *)"\p", 31);
							if (currentMissions[j].shipStrList >= FirstResourceID && currentMissions[j].shipStrIndex >= 1) {
								GetIndString(name, currentMissions[j].shipStrList, currentMissions[j].shipStrIndex);
								strncpy(currentMissions[j].shipName, (char *)name, 31);
							}
						}
						for (i = 0; i < NumMissionBits; i++)
							missionBit[i] = 0;
						for (i = 0; i < kOldMissionBits; i++)
							missionBit[i] = (*pilot)->missionBits[i];
						for (i = 0; i < MaxStellarObjects; i++)
							stellarData[i].dominated = (*pilot)->dominated[i];
						DeleteAllComputerShips(true, true);
						for (i = 0; i < MaxShipsInSystem; i++) {
							if ((*pilot)->escortType[i] >= 0) {
								if ((*pilot)->escortType[i] < kHiredEscortBase) {
									slot = SpawnEscort((*pilot)->escortType[i], -1);
									shipData[slot].hired = false;
								} else {
									slot = SpawnEscort((*pilot)->escortType[i] - kHiredEscortBase, -1);
									shipData[slot].hired = true;
								}
							}
							if ((*pilot)->fighterType[i] >= 0 && (*pilot)->fighterType[i] < MaxShipClasses) {
								slot = SpawnEscort((*pilot)->fighterType[i], -1);
								if (slot != -1) {
									shipData[slot].aiType = kAIFighter;
									shipData[slot].hired = false;
									AISetup(&shipData[slot]);
								}
							}
						}
						HPurge((Handle)pilot);
						ReleaseResource((Handle)pilot);
					}
				} else {
					CloseResFile(pilotRefNum);
					return fnfErr;
				}
				h = GetResource(kPilotFileType, kPilotAuxID);
				if (h != NULL) {
					HNoPurge(h);
					GetResInfo(h, &resID, &type, name);
					strncpy((char *)shipName, (char *)name, 64);
					HPurge(h);
					ReleaseResource(h);
				}
				strncpy((char *)playerName, (char *)spec.name, 31);
				CloseResFile(pilotRefNum);
				MakeAlias(spec);
			} else
				return fnfErr;
		} else
			return fnfErr;
	} else
		return err;
	return noErr;
}

// FUNCTION: EVO 0x1001D394
Boolean DoesPilotExist(StringPtr name)
{
	OSErr	err;
	short	vRefNum;
	FSSpec	spec;
	Str255	fileName;
	Str255	volName;

	err = GetVol(fileName, &vRefNum);
	if (err == noErr) {
		strcpy((char *)fileName, (char *)name);
		err = FSMakeFSSpec(pilotVRefNum, pilotDirID, fileName, &spec);
		if (err == fnfErr)
			return false;
		if (err == noErr)
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x1001D434
void DeletePilotFile(StringPtr name)
{
	FSSpec	spec;
	Str255	fileName;
	OSErr	err;

	if (DoesPilotExist(name)) {
		strcpy((char *)fileName, (char *)name);
		err = FSMakeFSSpec(pilotVRefNum, pilotDirID, fileName, &spec);
		if (err == noErr)
			err = FSpDelete(&spec);
	}
}

// FUNCTION: EVO 0x1001D4BC
void MakeAlias(FSSpec target)
{
	OSErr		err;
	Point		where = {84, 50};
	FXInfo		fxInfo;
	FInfo		finfo;
	Str255		aliasName;
	FSSpec		spec;
	AliasHandle	alias;
	short		saveRes;
	short		refNum;

	spec = target;
	strcpy((char *)spec.name, (char *)"\pLast Pilot");
	FSpDelete(&spec);
	err = NewAlias(NULL, &target, &alias);
	FSpGetFInfo(&target, &finfo);
	saveRes = CurResFile();
	FSpCreateResFile(&spec, finfo.fdCreator, finfo.fdType, smRoman);
	refNum = FSpOpenResFile(&spec, fsCurPerm);
	if (refNum != -1) {
		UseResFile(refNum);
		AddResource((Handle)alias, rAliasType, 0, spec.name);
		WriteResource((Handle)alias);
		CloseResFile(refNum);
		finfo.fdFlags = kIsAlias;
		FSpSetFInfo(&spec, &finfo);
	}
	UseResFile(saveRes);
}

// FUNCTION: EVO 0x1001D634
void LoadSprites(void)
{
	short	progress;
	short	loadedID[MaxShipClasses];
	Str255	str;
	Str255	num;
	Rect	unusedRects[3];
	long	grow;
	Rect	unusedRect;
	long	unusedLong;
	short	spritesID;
	short	masksID;
	short	xSize;
	short	ySize;
	short	xTiles;
	short	yTiles;
	short	i;
	short	n;
	short	x;
	short	y;
	short	k;
	Boolean	found;

	clut1 = GetCTable(1);
	clut8 = GetCTable(8);
	HNoPurge((Handle)clut1);
	HNoPurge((Handle)clut8);
	if (EVBugResourceFieldTest(2))
		return;
	hiResSprites = EVBugResourceFieldTest(3);
	for (i = 0; i < MaxShipClasses; i++)
		loadedID[i] = -1;
	nextSheet = 0;
	progress = 0;

	for (i = 0; i < MaxStellarClasses; i++) {
		spinHandle = NULL;
		spinHandle = (SpinResHandle)GetResource(kSpinType, i + kStellarSpinBase);
		if (spinHandle != NULL) {
			HLock((Handle)spinHandle);
			spritesID = (*spinHandle)->spritesID;
			masksID = (*spinHandle)->masksID;
			xSize = (*spinHandle)->xSize;
			ySize = (*spinHandle)->ySize;
			HUnlock((Handle)spinHandle);
			ReleaseResource((Handle)spinHandle);
			LoadSpriteSheet(spritesID, masksID, xSize, ySize, 1, 1);
			NewSheetFace(&spobFaces[i], xSize, ySize, 0, 0);
			if (spobFaces[i] == NULL)
				GraphicsEmergencyBail();
			DeltaIntroProgressBar(1.0);
		}
	}

	for (i = 0; i < MaxShipClasses; i++) {
		spinHandle = (SpinResHandle)GetResource(kSpinType, i + FirstResourceID);
		if (spinHandle != NULL) {
			progress = 0;
			spritesID = (*spinHandle)->spritesID;
			masksID = (*spinHandle)->masksID;
			xSize = (*spinHandle)->xSize;
			ySize = (*spinHandle)->ySize;
			xTiles = (*spinHandle)->xTiles;
			yTiles = (*spinHandle)->yTiles;
			if (spritesID != -1 && masksID != -1) {
				found = false;
				for (n = 0; n < i; n++) {
					if ((*spinHandle)->spritesID == loadedID[n]) {
						for (k = 0; k <= 35; k++)
							shipFaces[i][k] = shipFaces[n][k];
						found = true;
						break;
					}
				}
				if (!found) {
					LoadSpriteSheet(spritesID, masksID, xSize, ySize, xTiles, yTiles);
					n = 0;
					for (y = 0; y < yTiles; y++) {
						for (x = 0; x < xTiles; x++) {
							NewSheetFace(&shipFaces[i][n], xSize, ySize, x * xSize, y * ySize);
							n++;
						}
					}
					loadedID[i] = spritesID;
					progress += xTiles * yTiles;
				}
			}
			ReleaseResource((Handle)spinHandle);
			DeltaIntroProgressBar(progress);
		}
	}

	for (i = 0; i < MaxWeaponTypes; i++) {
		spinHandle = (SpinResHandle)GetResource(kSpinType, i + kWeaponSpinBase);
		if (spinHandle != NULL) {
			spritesID = (*spinHandle)->spritesID;
			masksID = (*spinHandle)->masksID;
			xSize = (*spinHandle)->xSize;
			ySize = (*spinHandle)->ySize;
			xTiles = (*spinHandle)->xTiles;
			yTiles = (*spinHandle)->yTiles;
			ReleaseResource((Handle)spinHandle);
			LoadSpriteSheet(spritesID, masksID, xSize, ySize, xTiles, yTiles);
			n = 0;
			for (y = 0; y < yTiles; y++) {
				for (x = 0; x < xTiles; x++) {
					NewSheetFace(&weapFaces[i][n], xSize, ySize, x * xSize, y * ySize);
					n++;
				}
			}
			DeltaIntroProgressBar(n);
		}
	}

	progress = 0;
	for (i = 0; i <= 2; i++) {
		spinHandle = (SpinResHandle)GetResource(kSpinType, i + kExplosionSpinBase);
		if (spinHandle != NULL) {
			spritesID = (*spinHandle)->spritesID;
			masksID = (*spinHandle)->masksID;
			xSize = (*spinHandle)->xSize;
			ySize = (*spinHandle)->ySize;
			xTiles = (*spinHandle)->xTiles;
			yTiles = (*spinHandle)->yTiles;
			ReleaseResource((Handle)spinHandle);
			LoadSpriteSheet(spritesID, masksID, xSize, ySize, xTiles, yTiles);
			n = 0;
			for (y = 0; y < yTiles; y++) {
				for (x = 0; x < xTiles; x++) {
					NewSheetFace(&explodeFaces[i][n], xSize, ySize, x * xSize, y * ySize);
					n++;
				}
			}
			DeltaIntroProgressBar(n);
		}
	}

	progress = 0;
	spinHandle = (SpinResHandle)GetResource(kSpinType, kBoxSpinID);
	if (spinHandle != NULL) {
		spritesID = (*spinHandle)->spritesID;
		masksID = (*spinHandle)->masksID;
		xSize = (*spinHandle)->xSize;
		ySize = (*spinHandle)->ySize;
		xTiles = (*spinHandle)->xTiles;
		yTiles = (*spinHandle)->yTiles;
		ReleaseResource((Handle)spinHandle);
		LoadSpriteSheet(spritesID, masksID, xSize, ySize, xTiles, yTiles);
		n = 0;
		for (y = 0; y < yTiles; y++) {
			for (x = 0; x < xTiles; x++) {
				NewSheetFace(&boxFaces[n], xSize, ySize, x * xSize, y * ySize);
				n++;
			}
		}
		DeltaIntroProgressBar(n);
	}

	progress = 0;
	spinHandle = (SpinResHandle)GetResource(kSpinType, kStarSpinID);
	if (spinHandle != NULL) {
		spritesID = (*spinHandle)->spritesID;
		masksID = (*spinHandle)->masksID;
		xSize = (*spinHandle)->xSize;
		ySize = (*spinHandle)->ySize;
		xTiles = (*spinHandle)->xTiles;
		yTiles = (*spinHandle)->yTiles;
		ReleaseResource((Handle)spinHandle);
		LoadSpriteSheet(spritesID, masksID, xSize, ySize, xTiles, yTiles);
		NewSheetFace(&starFaces[0], xSize, ySize, 0, 0);
		NewSheetFace(&starFaces[1], xSize, ySize, xSize, 0);
		DeltaIntroProgressBar(2.0);
	}

	progress = 0;
	spinHandle = (SpinResHandle)GetResource(kSpinType, kSmallRockSpinID);
	if (spinHandle != NULL) {
		spritesID = (*spinHandle)->spritesID;
		masksID = (*spinHandle)->masksID;
		xSize = (*spinHandle)->xSize;
		ySize = (*spinHandle)->ySize;
		xTiles = (*spinHandle)->xTiles;
		yTiles = (*spinHandle)->yTiles;
		ReleaseResource((Handle)spinHandle);
		LoadSpriteSheet(spritesID, masksID, xSize, ySize, xTiles, yTiles);
		n = 0;
		for (y = 0; y < yTiles; y++) {
			for (x = 0; x < xTiles; x++) {
				NewSheetFace(&smallRockFaces[n], xSize, ySize, x * xSize, y * ySize);
				n++;
			}
		}
		DeltaIntroProgressBar(n);
	}

	progress = 0;
	spinHandle = (SpinResHandle)GetResource(kSpinType, kBigRockSpinID);
	if (spinHandle != NULL) {
		spritesID = (*spinHandle)->spritesID;
		masksID = (*spinHandle)->masksID;
		xSize = (*spinHandle)->xSize;
		ySize = (*spinHandle)->ySize;
		xTiles = (*spinHandle)->xTiles;
		yTiles = (*spinHandle)->yTiles;
		ReleaseResource((Handle)spinHandle);
		LoadSpriteSheet(spritesID, masksID, xSize, ySize, xTiles, yTiles);
		n = 0;
		for (y = 0; y < yTiles; y++) {
			for (x = 0; x < xTiles; x++) {
				NewSheetFace(&bigRockFaces[n], xSize, ySize, x * xSize, y * ySize);
				n++;
			}
		}
		DeltaIntroProgressBar(n);
	}

	progress = 0;
	spinHandle = (SpinResHandle)GetResource(kSpinType, kRolloverSpinID);
	if (spinHandle != NULL) {
		spritesID = (*spinHandle)->spritesID;
		masksID = (*spinHandle)->masksID;
		xSize = (*spinHandle)->xSize;
		ySize = (*spinHandle)->ySize;
		xTiles = (*spinHandle)->xTiles;
		yTiles = (*spinHandle)->yTiles;
		ReleaseResource((Handle)spinHandle);
		LoadSpriteSheet(spritesID, masksID, xSize, ySize, xTiles, yTiles);
		n = 0;
		for (y = 0; y < yTiles; y++) {
			for (x = 0; x < xTiles; x++) {
				NewSheetFace(&rolloverFaces[n], xSize, ySize, x * xSize, y * ySize);
				n++;
			}
		}
		DeltaIntroProgressBar(n);
	}

	for (i = 0; i < MaxShipClasses; i++)
		shipPics[i] = GetPicture(i + kShipPictBase);
	for (i = 0; i < 10; i++)
		staticPats[i] = GetPixPat(i + kStaticPatID);
	armorPat = GetPixPat(kArmorPatID);
	for (i = 0; i <= 3; i++) {
		dockFaces[0][i] = FaceFromCIcon(i + kDockIconID);
		dockFaces[1][i] = FaceFromCIcon(i + kDockIcon2ID);
	}
	for (i = 0; i < 8; i++)
		for (n = 0; n < 8; n++)
			streakFaces[i][n] = FaceFromCIcon(kStreakIconID + i * 8 + n);
	for (i = 0; i <= 1; i++)
		hudOrbFaces[i] = FaceFromCIcon(i + kHudOrbIconID);
	for (i = 0; i <= 3; i++)
		for (n = 0; n <= 3; n++)
			targetFaces[i][n] = FaceFromCIcon(kTargetIconID + i * 4 + n);
	doubleSpeedFace = FaceFromCIcon(kDoubleSpeedIconID);
	if (EVBugResourceFieldTest(4)) {
		strcpy((char *)str, "After loading graphics, stackspace is ");
		NumToString(StackSpace(), num);
		p2cstr(num);
		strcat((char *)str, (char *)num);
		strcat((char *)str, " bytes");
		c2pstr((char *)str);
		DebugStr(str);
	}
	SATSetPortScreen();
	ForeColor(blackColor);
	MaxMem(&grow);
}

// FUNCTION: EVO 0x1001E4FC
void LoadSpriteSheet(short pictID, short maskID, short width, short height, short xTiles, short yTiles)
{
	Rect		r;
	PicHandle	pic;
	OSErr		err;

	pic = NULL;
	r.left = r.top = 0;
	r.right = width * xTiles;
	r.bottom = height * yTiles;
	curSheet = nextSheet;
	nextSheet++;
	if (curSheet >= kMaxSheets)
		GraphicsEmergencyBail();
	err = SATMakeOffscreen2(&sheetPorts[curSheet], &r);
	if (err != noErr)
		GraphicsEmergencyBail();
	pic = NULL;
	pic = GetPicture(pictID);
	if (pic == NULL)
		GraphicsEmergencyBail();
	SATSetPort(&sheetPorts[curSheet]);
	DrawPicture(pic, &r);
	SATSetPortScreen();
	ForeColor(blackColor);
	ReleaseResource((Handle)pic);
	CreateBWOffscreen(&maskPorts[curSheet], r);
	pic = NULL;
	pic = GetPicture(maskID);
	if (pic == NULL)
		GraphicsEmergencyBail();
	SATSetPort(&maskPorts[curSheet]);
	DrawPicture(pic, &r);
	ForeColor(blackColor);
	InvertRect(&r);
	SATSetPortScreen();
	ReleaseResource((Handle)pic);
}

// FUNCTION: EVO 0x1001E6D8
void NewSheetFace(FacePtr *face, short width, short height, short x, short y)
{
	Rect	src;
	Rect	unused;
	Rect	r;
	FacePtr	f;

	SetRect(&r, 0, 0, width, height);
	f = (FacePtr)NewPtr(sizeof(Face));
	if (f == NULL)
		GraphicsEmergencyBail();
	src = r;
	OffsetRect(&src, x, y);
	f->iconMask.rowBytes = maskPorts[curSheet].rowBytes & kRowBytesMask;
	f->iconMask.baseAddr = maskPorts[curSheet].baseAddr + src.top * f->iconMask.rowBytes + src.left / 8;
	f->iconMask.bounds = src;
	OffsetRect(&f->iconMask.bounds, -f->iconMask.bounds.left, -f->iconMask.bounds.top);
	f->rowBytes = sheetPorts[curSheet].rowBytes & kRowBytesMask;
	f->colorData = (Ptr)((long)sheetPorts[curSheet].baseAddr + (long)src.top * sheetPorts[curSheet].rowBytes
		+ src.left * gSAT.initDepth / 8);
	f->maskRgn = NULL;
	f->rows = NULL;
	f->maskRows = NULL;
	f->maskData = NULL;
	f->redrawProc = NULL;
	*face = f;
}

// FUNCTION: EVO 0x1001E89C
Boolean EVBugResourceFieldTest(short field)
{
	Handle	h;
	Boolean	on;
	Str63	unused;

	on = false;
	if (field < 0 || field > kLastBugField)
		return false;
	h = GetResource(kBugType, kBugID);
	if (h != NULL) {
		if (((short *)*h)[field] != 0)
			on = true;
		ReleaseResource(h);
	}
	return on;
}

// FUNCTION: EVO 0x1001E940
OSErr ResolveMacFileAlias(short refNum, StringPtr name, long *dirID, short *vRefNum)
{
	FCBPBRec	pb;
	Str255		fileName;
	Boolean		wasAliased;
	Boolean		isFolder;
	FSSpec		spec;
	CInfoPBPtr	cpb;
	OSErr		err;

	pb.ioRefNum = refNum;
	pb.ioNamePtr = fileName;
	pb.ioFCBIndx = 0;
	err = PBGetFCBInfoSync(&pb);
	if (err == noErr) {
		fileName[0] = 0;
		cpb = (CInfoPBPtr)NewPtrClear(sizeof(CInfoPBRec));
		if (cpb != NULL) {
			spec.vRefNum = pb.ioFCBVRefNum;
			spec.parID = pb.ioFCBParID;
			BlockMove(name, spec.name, name[0] + 1);
			BlockMove(name, fileName, name[0] + 1);
			err = ResolveAliasFile(&spec, true, &isFolder, &wasAliased);
			if (err == noErr && wasAliased) {
				cpb->hFileInfo.ioNamePtr = spec.name;
				cpb->hFileInfo.ioVRefNum = spec.vRefNum;
				cpb->hFileInfo.ioDirID = spec.parID;
			} else {
				cpb->hFileInfo.ioNamePtr = fileName;
				cpb->hFileInfo.ioVRefNum = pb.ioFCBVRefNum;
				cpb->hFileInfo.ioDirID = pb.ioFCBParID;
			}
			cpb->hFileInfo.ioFDirIndex = 0;
			cpb->hFileInfo.ioCompletion = NULL;
			err = PBGetCatInfoSync(cpb);
			if (err == noErr) {
				*dirID = cpb->dirInfo.ioDrDirID;
				*vRefNum = cpb->dirInfo.ioVRefNum;
			}
			DisposePtr((Ptr)cpb);
		}
	}
	return err;
}

// FUNCTION: EVO 0x1001EAC8
short ReadStoredVersionStamp(void)
{
	Handle	h;
	short	version;
	OSErr	err;

	h = GetResource(kYearType, kVersionID);
	if (h != NULL) {
		version = *(short *)*h;
		ReleaseResource(h);
	}
	return version;
}
