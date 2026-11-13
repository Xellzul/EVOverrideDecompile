/* evrecords.h -- the data-file records: their in-memory tables and their resource layouts. Notes: Research/notes/structs.md */
#ifndef EVRECORDS_H
#define EVRECORDS_H

#include "gametypes.h"

#define kShipType   0x73689570  /* 'shïp' */
#define kWeapType   0x77916170  /* 'wëap' */
#define kOutfType   0x6f9f7466  /* 'oütf' */
#define kSpobType   0x73709a62  /* 'spöb' */
#define kSystType   0x73d87374  /* 'sÿst' */
#define kGovtType   0x679a7674  /* 'gövt' */
#define kMisnType   0x6d95736e  /* 'mïsn' */
#define kPersType   0x70917273  /* 'përs' */
#define kDudeType   0x649f6465  /* 'düde' */
#define kFletType   0x666c9174  /* 'flët' */
#define kJunkType   0x6a9f6e6b  /* 'jünk' */
#define kOopsType   0x9a6f7073  /* 'öops' */
#define kNebuType   0x6e916275  /* 'nëbu' */
#define kDescType   0x64917363  /* 'dësc' */
#define kDeqtType   0x64917174  /* 'dëqt' */
#define kSpinType   0x7370956e  /* 'spïn' */
#define kSpitType   0x73709574  /* 'spït' */
#define kYearType   0x79918aa8  /* 'yëä®' */

#define kAIWimpyTrader  1
#define kAIBraveTrader  2
#define kAIWarship      3
#define kAIInterceptor  4
#define kAIFighter      5
#define kAIEscort       6

#define kEscapePodShip  63

#pragma options align=mac68k

typedef struct ShipRec {
    short cargo;
    short turnRate;
    short fuel;
    short freeMass;
    short armor;
    short shieldRech;
    short maxGuns;
    short maxTurrets;
    short techLevel;
    short deathDelay;
    short inherentAI;
    short missionBit;
    short defOutfID[4];
    short defOutfCount[4];
    short fuelRegen;
    float accel;
    float maxSpeed;
    float jumpSpeedScale;
    long cost;
    long shield;
    Str63 name;
    short weapCount[MaxWeaponTypes];
    short weapAmmo[MaxWeaponTypes];
    short flags;
    short shotXOffset;
    short turretYDisp[4];
    short mass;
    short shipLength;
    short crew;
    short inherentGovt;
    short skillVar;
    Boolean negativeHoldsFlag;
} ShipRec;

#define kShipJump75             0x0001
#define kShipJump125            0x0002
#define kShipJump150            0x0004
#define kShipFuelRegen          0x0008
#define kShipDisableAt10        0x0010
#define kShipAfterburnerRated   0x0020
#define kShipAfterburner        0x0040
#define kShipUseShotXOffset     0x0080
#define kShipShowArmor          0x0100
#define kShipTurretBlindFront   0x1000
#define kShipTurretBlindSide    0x2000
#define kShipTurretBlindRear    0x4000

#define ShipRegensFuel(s)       ((s)->flags & kShipFuelRegen)
#define ShipDisablesAt10(s)     ((s)->flags & kShipDisableAt10)
#define ShipUsesShotXOffset(s)  ((s)->flags & kShipUseShotXOffset)
#define ShipShowsArmor(s)       ((s)->flags & kShipShowArmor)

typedef struct WeapRecord {
    short lifetime;
    short massDmg;
    short energyDmg;
    short guidance;
    short ammoType;
    short graphic;
    short inaccuracy;
    short sound;
    short impact;
    short explodType;
    short proxRadius;
    short blastRadius;
    short decay;
    unsigned short flags;
    unsigned short seeker;
    unsigned short smokeSet;
    float speed;
    float reload;
} WeapRecord;

#define kGuidanceUnguided   (-1)
#define kGuidanceBeam       0
#define kGuidanceHoming     1
#define kGuidanceTurretBeam 3
#define kGuidanceTurret     4
#define kGuidanceFreefall   5
#define kGuidanceRocket     6
#define kGuidanceFrontQuad  7
#define kGuidanceRearQuad   8
#define kGuidanceBay        99

#define kBeamRed            (-2)
#define kBeamGreen          (-3)
#define kBeamBlue           (-4)
#define kBeamCyan           (-5)
#define kBeamMagenta        (-6)
#define kBeamYellow         (-7)
#define kBeamMagentaRed     (-8)
#define kBeamYellowGreen    (-9)
#define kBeamCyanBlue       (-10)

#define kExplodSmall        0
#define kExplodBig          1
#define kExplodHuge         2

#define kAmmoSelfDestruct   (-999)
#define kAmmoFuelBase       (-1000)
#define kAmmoShipBase       128

#define kWeapSpinGraphic        0x0001
#define kWeapSecondTrigger      0x0002
#define kWeapStartFirstFrame    0x0004
#define kWeapNoFastTargets      0x0008
#define kWeapLoopSound          0x0010
#define kWeapDecoy              0x0020
#define kWeapFireTogether       0x0040
#define kWeapNoXOffset          0x0080
#define kWeapSafeForPlayer      0x0100
#define kWeapSmallSmoke         0x0200
#define kWeapBigSmoke           0x0400
#define kWeapLongSmoke          0x0800
#define kWeapBlindFront         0x1000
#define kWeapBlindSide          0x2000
#define kWeapBlindRear          0x4000
#define kWeapDetonates          0x8000

#define WeapIsSecondary(w)      ((w)->flags & kWeapSecondTrigger)
#define WeapIsDecoy(w)          ((w)->flags & kWeapDecoy)
#define WeapDetonates(w)        ((w)->flags & kWeapDetonates)

#define kSeekPassAsteroids      0x0001
#define kSeekAsteroidDecoy      0x0002
#define kSeekFlareDecoy         0x0004
#define kSeekInterference       0x0008
#define kSeekJamHalf1           0x0010
#define kSeekJamHalf2           0x0020
#define kSeekJamHalf3           0x0040
#define kSeekJamHalf4           0x0080
#define kSeekJamFull1           0x0100
#define kSeekJamFull2           0x0200
#define kSeekJamFull3           0x0400
#define kSeekJamFull4           0x0800
#define kSeekTurn30             0x1000
#define kSeekTurn60             0x2000
#define kSeekLoseLock           0x4000
#define kSeekHitParent          0x8000

typedef struct OutfRecord {
    short techLevel;
    short mass;
    short modType[MaxOutfitMods];
    short modVal[MaxOutfitMods];
    short maxCount;
    short flags;
    short missionBit;
    long cost;
    Str63 name;
    Boolean persistent;
} OutfRecord;

#define kOutfFixedGun           0x0001
#define kOutfTurret             0x0002
#define kOutfPersistent         0x0004
#define kOutfCantSell           0x0008
#define kOutfRemoveAfterBuy     0x0010

#define OutfIsFixedGun(o)       ((o)->flags & kOutfFixedGun)
#define OutfIsTurret(o)         ((o)->flags & kOutfTurret)
#define OutfPersistent(o)       ((o)->flags & kOutfPersistent)
#define OutfCantSell(o)         ((o)->flags & kOutfCantSell)

#define kOutfModWeapon          1
#define kOutfModCargo           2
#define kOutfModAmmo            3
#define kOutfModShield          4
#define kOutfModShieldRech      5
#define kOutfModArmor           6
#define kOutfModAccel           7
#define kOutfModSpeed           8
#define kOutfModTurn            9
#define kOutfModJammer          10
#define kOutfModEscapePod       11
#define kOutfModFuel            12
#define kOutfModDensityScanner  13
#define kOutfModIFF             14
#define kOutfModAfterburner     15
#define kOutfModMap             16
#define kOutfModCloak           17
#define kOutfModFuelScoop       18
#define kOutfModAutoRefuel      19
#define kOutfModAutoEject       20
#define kOutfModCleanRecord     21
#define kOutfModHyperSpeed      22
#define kOutfModHyperDist       23
#define kOutfModInterference    24
#define kOutfModMarines         25
#define kOutfModMissionBit      26

typedef struct SpobRecord {
    ShortPoint pos;
    short system;
    short govt;
    short minStatus;
    short type;
    short techLevel;
    short specialTech[3];
    short customPict;
    short customSound;
    short daysDominated;
    unsigned long flags;
    Boolean spawned;
    Boolean loaded;
    Boolean dominated;
    Str31 name;
    short defenseDude;
    short tribute;
    short defCount;
} SpobRecord;

#define kSpobCanLand            0x00000001
#define kSpobTradeCenter        0x00000002
#define kSpobOutfitter          0x00000004
#define kSpobShipyard           0x00000008
#define kSpobStation            0x00000010
#define kSpobUninhabited        0x00000020
#define kSpobHasBar             0x00000040

#define kSpobPriceLow           0x1
#define kSpobPriceMed           0x2
#define kSpobPriceHigh          0x4

#define kSpobFoodLow                   0x10000000
#define kSpobFoodMed                   0x20000000
#define kSpobFoodHigh                  0x40000000
#define kSpobIndustrialLow             0x01000000
#define kSpobIndustrialMed             0x02000000
#define kSpobIndustrialHigh            0x04000000
#define kSpobMedicalLow                0x00100000
#define kSpobMedicalMed                0x00200000
#define kSpobMedicalHigh               0x00400000
#define kSpobLuxuryLow                 0x00010000
#define kSpobLuxuryMed                 0x00020000
#define kSpobLuxuryHigh                0x00040000
#define kSpobMetalLow                  0x00001000
#define kSpobMetalMed                  0x00002000
#define kSpobMetalHigh                 0x00004000
#define kSpobEquipmentLow              0x00000100
#define kSpobEquipmentMed              0x00000200
#define kSpobEquipmentHigh             0x00000400

#define kJamWeak1               0x0001
#define kJamWeak2               0x0002
#define kJamWeak3               0x0004
#define kJamWeak4               0x0008
#define kJamStrong1             0x0010
#define kJamStrong2             0x0020
#define kJamStrong3             0x0040
#define kJamStrong4             0x0080
#define kJamFreightersHalf      0x0100
#define SpobCanLand(s)          ((s)->flags & kSpobCanLand)
#define SpobHasTrade(s)         ((s)->flags & kSpobTradeCenter)
#define SpobHasOutfitter(s)     ((s)->flags & kSpobOutfitter)
#define SpobHasShipyard(s)      ((s)->flags & kSpobShipyard)
#define SpobIsStation(s)        ((s)->flags & kSpobStation)
#define SpobUninhabited(s)      ((s)->flags & kSpobUninhabited)
#define SpobHasBar(s)           ((s)->flags & kSpobHasBar)

typedef struct SystRecord {
    short xPos;
    short yPos;
    short govt;
    short links[16];
    short navDefault[4];
    short dudeTypes[4];
    short dudeProbs[4];
    short avgShips;
    short visited;
    short message;
    short asteroidCount;
    short interference;
    short visibility;
    short persons[4];
    Str31 name;
    Boolean loaded;
} SystRecord;

typedef struct GovtRecord {
    Str31 name;
    unsigned short flags;
    short ally;
    short enemy;
    short crimeTol;
    short penalties[5];
    short initialRec;
    short inherentJam;
} GovtRecord;

#define kPenaltySmuggle         0
#define kPenaltyDisable         1
#define kPenaltyBoard           2
#define kPenaltyKill            3
#define kPenaltyShoot           4

#define kGovtXenophobic         0x0001
#define kGovtNosy               0x0002
#define kGovtAlwaysAttacks      0x0004
#define kGovtRetreats           0x0010
#define kGovtNoSamaritan        0x0020
#define kGovtNeverAttacks       0x0040
#define kGovtNoPersPod          0x0100
#define kGovtWarshipBribes      0x0200
#define kGovtNoHail             0x0400
#define kGovtStartDisabled      0x0800
#define kGovtPlunders           0x1000
#define kGovtFreighterBribes    0x2000
#define kGovtPlanetBribes       0x4000
#define kGovtGreedy             0x8000

#define GovtXenophobic(g)       ((g)->flags & kGovtXenophobic)
#define GovtAlwaysAttacks(g)    ((g)->flags & kGovtAlwaysAttacks)
#define GovtNeverAttacks(g)     ((g)->flags & kGovtNeverAttacks)
#define GovtNoHail(g)           ((g)->flags & kGovtNoHail)

typedef struct PersRecord {
    short linkSyst;
    short govt;
    short aiType;
    short aggress;
    short coward;
    short shipType;
    short commQuote;
    short hailQuote;
    short linkMission;
    short missionBit;
    unsigned short flags;
    short weapCount[MaxWeaponTypes];
    char unk_96[0x80];
    short weapAmmo[MaxWeaponTypes];
    long credits;
    float shieldMod;
    Boolean loaded;
    Boolean grudge;
    Str31 name;
} PersRecord;

#define kPersBountyHunter       510
#define kPersAmbrosiaMascot     511

#define kLinkGovtBase           10000	/* linkSyst: 128 + n a system, then the govt of the system */
#define kLinkAllyBase           15000	/* allies of govt n */
#define kLinkNotGovtBase        20000	/* any govt but n */
#define kLinkEnemyBase          25000	/* enemies of govt n */
#define kLinkLimit              30000

#define kMisnBehavAttack        0
#define kMisnBehavProtect       1
#define kMisnBehavVariant       10	/* added to the behaviour code */

#define kPersGrudge             0x0001
#define kPersPodAndAfterburner  0x0002
#define kPersQuoteIfGrudge      0x0004
#define kPersQuoteIfLikes       0x0008
#define kPersQuoteOnAttack      0x0010
#define kPersQuoteIfDisabled    0x0020
#define kPersReplaceSpecial     0x0040
#define kPersQuoteOnce          0x0080
#define kPersDeactivate         0x0100
#define kPersOfferOnBoard       0x0200
#define kPersQuietNoMission     0x0400
#define kPersLeaveAfter         0x0800
#define kPersNoWimpyFreighter   0x1000
#define kPersNoBeefyFreighter   0x2000
#define kPersNoWarship          0x4000
#define kPersDisasterInfo       0x8000

#define PersHoldsGrudge(p)      ((p)->flags & kPersGrudge)
#define PersQuoteOnce(p)        ((p)->flags & kPersQuoteOnce)

typedef struct DudeRecord {
    short aiType;
    short govt;
    short shipTypes[4];
    short shipProbs[4];
    short missionBit[4];
    unsigned short flags;
    unsigned short adviceFlags;
} DudeRecord;

#define kBootyFood              0x0001
#define kBootyIndustrial        0x0002
#define kBootyMedical           0x0004
#define kBootyLuxury            0x0008
#define kBootyMetal             0x0010
#define kBootyEquipment         0x0020
#define kBootyMoney             0x0040

typedef struct FletRecord {
    short leadShipType;
    short govt;
    short escortTypes[4];
    short escortMin[4];
    short escortMax[4];
    short linkSyst;
    short missionBit;
} FletRecord;

typedef struct JunkRecord {
    short soldAt;
    short boughtAt;
    short basePrice;
    short held;
    unsigned short flags;
    Str63 name;
} JunkRecord;

#define kJunkTribbles           0x0001

typedef struct OopsRecord {
    short stellar;
    short stellarID;
    short commodity;
    short amount;
    short duration;
    short freq;
    short countdown;
    short missionBit;
    Str63 name;
} OopsRecord;

typedef struct NebuRecord {
    short xPos;
    short yPos;
    short width;
    short height;
    Boolean explored;
} NebuRecord;

typedef struct DeqtRecord {
    short flags;
    short movieId;
    Str255 name;
} DeqtRecord;

#define kMovieBefore            0x0000
#define kMovieAfter             0x0001
#define kMovieInstead           0x0002

typedef struct ShipResource {
    short holds;
    short shield;
    short accel;
    short speed;
    short maneuver;
    short fuel;
    short freeMass;
    short armor;
    short shieldRe;
    short weapType[4];
    short weapCount[4];
    short ammoLoad[4];
    short maxGun;
    short maxTur;
    short techLevel;
    long cost;
    short deathDelay;
    short turretYDisp[4];
    short mass;
    short length;
    short inherentAI;
    short crew;
    short missionBit;
    short inherentGovt;
    unsigned short flags;
    short shotXOffset;
    short defaultItems[4];
    short itemCount[4];
    short fuelRegen;
    short skillVar;
    short unused[8];
} ShipResource, *ShipResPtr, **ShipResHandle;

typedef struct WeapResource {
    short reload;
    short count;
    short massDmg;
    short energyDmg;
    short guidance;
    short speed;
    short ammoType;
    short graphic;
    short inaccuracy;
    short sound;
    short impact;
    short explodType;
    short proxRadius;
    short blastRadius;
    unsigned short flags;
    unsigned short seeker;
    unsigned short smokeSet;
    short decay;
    short unused[8];
} WeapResource, *WeapResPtr, **WeapResHandle;

typedef struct OutfResource {
    short missionBit;
    short mass;
    short techLevel;
    short modType;
    short modVal;
    short max;
    unsigned short flags;
    long cost;
    short modType2;
    short modVal2;
    short unused[8];
} OutfResource, *OutfResPtr, **OutfResHandle;

typedef struct SpobResource {
    short xPos;
    short yPos;
    short type;
    unsigned long flags;
    short system;
    short techLevel;
    short specialTech[3];
    short govt;
    short minCoolness;
    short custPicID;
    short custSndID;
    short defDude;
    short defCount;
    short unused[8];
} SpobResource, *SpobResPtr, **SpobResHandle;

typedef struct SystResource {
    short xPos;
    short yPos;
    short con[5];
    short nav[4];
    short dudeTypes[4];
    short prob[4];
    short avgShips;
    short govt;
    short message;
    short asteroids;
    short interference;
    short visBit;
    short moreCon[11];
    short alwaysPers[4];
    short unused[8];
} SystResource, *SystResPtr, **SystResHandle;

typedef struct GovtResource {
    unsigned short inherentJam;
    unsigned short flags;
    short ally;
    short enemy;
    short crimeTol;
    short smugPenalty;
    short disabPenalty;
    short boardPenalty;
    short killPenalty;
    short shootPenalty;
    short initialRec;
    short unused[8];
} GovtResource, *GovtResPtr, **GovtResHandle;

typedef struct MisnResource {
    short availStel;
    short availBitSet;
    short availLoc;
    short availRecord;
    short availRating;
    short availRandom;
    short travelStel;
    short returnStel;
    short cargoType;
    short cargoQty;
    short pickupMode;
    short dropoffMode;
    short scanGovt;
    short failIfScan;
    long payVal;
    short shipCount;
    short shipSyst;
    short shipDude;
    short shipGoal;
    short shipBehav;
    short shipNameID;
    short compBitSet;
    short compGovt;
    short compReward;
    short failBitSet;
    short briefText;
    short quickBrief;
    short loadCargText;
    short dropCargText;
    short compText;
    short failText;
    short timeLimit;
    short canAbort;
    short unused44;
    short availBitClr;
    short auxShipCount;
    short auxShipDude;
    short auxShipSyst;
    short compBitSet2;
    unsigned short flags;
    short compBitSet3;
    short compBitSet4;
    short failBitSet2;
    short refuseText;
    short availShipType;
    short refuseBitSet;
    short startBitSet;
    short unused[8];
} MisnResource, *MisnResPtr, **MisnResHandle;

#define kMisnAutoAbort          0x0001
#define kMisnNoArrows           0x0002
#define kMisnCantRefuse         0x0004
#define kMisnInfiniteAux        0x0010
#define kMisnRemoveOutfit       0x0020
#define kMisnAbortPenalty       0x0040
#define kMisnJettisonPenalty    0x0080
#define kMisnGreenArrow         0x0100
#define kMisnShipSystArrow      0x0200
#define kMisnCritical           0x1000
#define kMisnNoFreighters       0x2000
#define kMisnNoWarships         0x4000

#define kMisnGoalNone           (-1)
#define kMisnGoalDestroy        0
#define kMisnGoalDisable        1
#define kMisnGoalBoard          2
#define kMisnGoalEscort         3
#define kMisnGoalObserve        4
#define kMisnGoalRescue         5
#define kMisnGoalChaseOff       6

#define kAvailLocComputer       0
#define kAvailLocBar            1
#define kAvailLocShip           2

#define kPickupAtStart          0
#define kPickupAtTravelStel     1
#define kPickupOnBoarding       2

#define kDropoffAtTravelStel    0
#define kDropoffAtReturnStel    1

#define kCargoTypeRandom        1000

#define kShipSystInitial        (-1)
#define kShipSystRandom         (-2)
#define kShipSystTravel         (-3)
#define kShipSystReturn         (-4)
#define kShipSystAdjacent       (-5)
#define kShipSystFollow         (-6)

#define kAuxSystPlayer          (-1)
#define kAuxSystTravel          (-2)
#define kAuxSystReturn          (-3)

#define MisnAutoAborts(m)       ((m)->flags & kMisnAutoAbort)
#define MisnCantRefuse(m)       ((m)->flags & kMisnCantRefuse)
#define MisnIsCritical(m)       ((m)->flags & kMisnCritical)

typedef struct PersResource {
    short linkSyst;
    short govt;
    short aiType;
    short aggress;
    short coward;
    short shipType;
    short weapType[4];
    short weapCount[4];
    short ammoLoad[4];
    long credits;
    short shieldMod;
    short missionBit;
    short commQuote;
    short hailQuote;
    short linkMission;
    unsigned short flags;
    short unused[8];
} PersResource, *PersResPtr, **PersResHandle;

typedef struct DudeResource {
    short aiType;
    short shipTypes[4];
    short prob[4];
    short govt;
    unsigned short booty;
    unsigned short infoTypes;
    short missionBit[4];
    short unused[8];
} DudeResource, *DudeResPtr, **DudeResHandle;

typedef struct FletResource {
    short leadShipType;
    short escortType[4];
    short min[4];
    short max[4];
    short govt;
    short linkSyst;
    short missionBit;
    short unused[8];
} FletResource, *FletResPtr, **FletResHandle;

typedef struct JunkResource {
    short soldAt;
    short boughtAt;
    short basePrice;
    unsigned short flags;
} JunkResource, *JunkResPtr, **JunkResHandle;

typedef struct OopsResource {
    short stellar;
    short commodity;
    short priceDelta;
    short duration;
    short freq;
    short missionBit;
    short unused[8];
} OopsResource, *OopsResPtr, **OopsResHandle;

typedef struct NebuResource {
    short xPos;
    short yPos;
    short xSize;
    short ySize;
} NebuResource, *NebuResPtr, **NebuResHandle;

typedef struct SpinResource {
    short spritesID;
    short masksID;
    short xSize;
    short ySize;
    short xTiles;
    short yTiles;
} SpinResource, *SpinResPtr, **SpinResHandle;

typedef struct DeqtResource {
    unsigned short flags;
} DeqtResource, *DeqtResPtr, **DeqtResHandle;

typedef struct YearResource {
    short yearOffset;
    short unused[8];
} YearResource, *YearResPtr, **YearResHandle;

#pragma options align=reset

#endif
