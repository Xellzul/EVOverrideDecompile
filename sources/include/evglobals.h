/* evglobals.h -- the shared globals beyond the record tables. Notes: Research/notes/headers.md */
#ifndef EVGLOBALS_H
#define EVGLOBALS_H

#include <Quickdraw.h>
#include <Dialogs.h>
#include <Controls.h>
#include <Lists.h>
#include <Events.h>
#include <Icons.h>
#include <QuickdrawText.h>
#include <Fonts.h>
#include <Resources.h>
#include "globals.h"
#include "evtypes.h"
#include "sat.h"
#include "sound.h"

extern Boolean asteroidsDirty;
extern GamePrefs settings;
extern short keyMap[45];
extern Ptr warpUpSnd;
extern Ptr warpOutSnd;
extern Ptr redAlertSnd;
#define kBeepSelect 0
#define kBeepClick 1
#define kBeepClose 2
#define kBeepDenied 3
#define kBeepMessage 4
extern Ptr beepSnd[5];
extern Ptr weapSnd[MaxWeaponTypes];
extern Ptr explodeSnd[5];
extern Ptr klaxonSnd;
extern Ptr ejectSnd;
extern Ptr cloakSnd[2];
extern Ptr voiceSnd;
extern FacePtr shipFaces[MaxShipClasses][kHeadingFrames];
extern FacePtr spobFaces[MaxStellarClasses];
extern FacePtr starFaces[3];
extern FacePtr smallRockFaces[20];
extern FacePtr bigRockFaces[30];
extern FacePtr dockFaces[3][4];
extern FacePtr targetFaces[4][4];
extern FacePtr weapFaces[MaxWeaponTypes][kHeadingFrames];
extern FacePtr explodeFaces[3][10];
extern FacePtr boxFaces[kHeadingFrames];
extern FacePtr hudOrbFaces[2];
extern FacePtr rolloverFaces[4];
extern FacePtr streakFaces[8][8];
extern HudRedraw hudRedraw;
extern Boolean gameInProgress;
extern Boolean killShots;
extern Boolean killBoxes;
extern Boolean killExplods;
extern Boolean killAsteroids;
extern Boolean killSmoke;
extern Boolean cursorHidden;
extern Boolean mBarHidden;
extern Boolean strictPlay;
extern Boolean gInvertedColors;
extern Boolean fadePending[2];
extern Boolean seenIntroScreen;
extern Boolean gRegistered;
extern Boolean thirdFrame;
extern PicHandle shipPics[MaxShipClasses];
extern SATPort statusPort;
extern SATPort loadPort;
extern SATPort armorPort;
extern unsigned long tix;
extern short numExplosions;
extern short numStreaks;
extern short frameCount;
extern short gDaysUsed;
extern short appResFile;
extern short selTradeItem;
extern short basePrice[10];
extern short commodityPrice[10];
extern short voiceCount[4];
extern short dockPermission;
extern short dockTarget;
extern short commCount;
extern short dockedPortItems[MaxOutfitItemTypes];
extern short dockedPortShipTypes[MaxShipClasses];
extern short missionGrid[][NumMissionBits];
extern short outfitCount[MaxOutfitItemTypes];
extern ShortPoint mapCenter;
extern ShortPoint screenCenter;
extern Str255 playerName;
extern Str255 shipName;
extern Str255 weapNames[MaxWeaponTypes];
extern Str255 longShipName[MaxShipClasses];
extern Str255 shipHelpRequests[20];
extern Str255 legalStrings[18];
extern char outfitShortName[MaxOutfitItemTypes][256];
extern Str255 shipyardName[MaxShipClasses];
extern Str255 volumeNames[9];
extern Str255 ratingStrings[11];
extern Str255 govtTransponderCode[MaxGovts];
extern Str255 lcOutfitName[MaxOutfitItemTypes];
extern Str255 lcOutfitPluralName[MaxOutfitItemTypes];
extern Str255 statusCargoName[NumCommodities];
extern Str255 statusJunkName[kMaxJunkTypes];
extern Str255 cargoAbbrev[MaxCargoTypes];
extern Str255 cargoNames[MaxCargoTypes];
extern char lcCargoName[MaxCargoTypes][256];
extern char lcJunkName[kMaxJunkTypes][256];
extern char gDialogText[4096];
extern RGBColor bkgndGreen;
extern RGBColor brightGreen;
extern RGBColor dimGreen;
extern RGBColor dimmerGreen;
extern RGBColor rgbWhite;
extern RGBColor rgbLightGrey;
extern RGBColor rgbMedGrey;
extern RGBColor rgbDarkGrey;
extern RGBColor rgbRealDarkBlue;
extern RGBColor rgbBlack;
extern PixPatHandle staticPats[10];
extern PixPatHandle armorPat;
extern Rect msgRect;
extern RgnHandle msgRgn;
extern short legalStatus[MaxSystems];
extern short gTargetStellar;
extern short newKeyPrefs[kNumKeys + 5];
extern short randomOdds[NumMissionBits];
extern short blinkTimer;
extern short hailShip;
extern short globalPriceFlux[2];
extern short localPriceFlux[2];
extern short pendingVoiceMsg;
extern short hyperRoute[kMaxHyperRoute];
extern long playerKills;
extern double timeScale;
extern double gameSpeed;
extern DateTimeRec gameTime;
extern GDHandle gDevice;
extern Boolean prefsChanged;
extern Boolean showAll;
extern Boolean gQuit;
extern Boolean gAbortGame;
extern short lowestPrice;
extern DialogPtr tradeDialog;
extern DialogPtr portDialog;
extern DialogPtr outfitDialog;
extern DialogPtr shipyardDialog;
extern DialogPtr infoDialog;
extern short selShipyardItem;
extern short outfitScroll;
extern short shipyardScroll;
extern short shipBuyMode;
extern short junkSlot[2];
extern PicHandle infoPics[10];
extern PicHandle outfitPics[10];
extern PicHandle tradePics[6];
extern PicHandle shipyardPics[12];
extern PicHandle portPics[14];
extern Boolean canBuy;
extern Boolean canSell;
extern Boolean spobInhabited;
extern Str255 gTextInputResult;
extern Boolean onPlanet;
extern Boolean canRefuseMission;
extern Boolean drawGate;
extern PicHandle missionPics[4];
extern PicHandle missionInfoPics[4];
extern short inBar;
extern short selMissionListItem;
extern short lastMissionStellar;
extern short speakerShip;
extern DialogPtr missionDialog;
extern DialogPtr missionInfoDialog;
extern char missionDescText[];
extern float trigResult;
extern CTabHandle gameClut;
extern QDGlobals qd;
extern SoundRequest alarmRequest;
extern Boolean warpSoundDone;
extern pascal SATglobalsRec gSAT;


#endif
