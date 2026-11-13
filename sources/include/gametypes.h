/* gametypes.h -- game limits and the small shared types. Notes: Research/notes/structs.md */
#ifndef GAMETYPES_H
#define GAMETYPES_H

#include <MacTypes.h>
#include <OSUtils.h>

#define MaxShipsInSystem        36
#define MaxStellarObjects       1500
#define MaxSystems              1000
#define MaxShipClasses          64
#define MaxStellarClasses       64
#define JumpDistance            1000
#define MaxWeaponTypes          64
#define MaxOutfitItemTypes      128
#define MaxBeamsOnScreen        8
#define MaxDudeTypes            128
#define MaxGovts                128
#define MaxExplosionsOnScreen   16
#define MaxMissions             512
#define NumMissionBits          512
#define MaxCargoTypes           64
#define MaxPersonTypes          512
#define MaxShotsOnScreen        128
#define MaxAsteroids            10
#define MaxNebulae              4
#define MaxSimultaneousMissions 8
#define MaxDisasters            128
#define MaxFleets               128
#define MaxMovies               128
#define MaxOutfitMods           2

#define NumCommodities          6
#define kNotBitBase             1000
#define kShortBound             32000
#define FirstResourceID         128
#define kSlotFree               (-2)	/* life of an unused shot, explosion or beam slot */
#define kGameYearOffset         250	/* the game's calendar runs this many years ahead of the clock */

#pragma options align=mac68k

typedef struct FloatPoint {
    float x;
    float y;
} FloatPoint;

typedef struct ShortPoint {
    short h;
    short v;
} ShortPoint;

typedef struct BeamRecord {
    ShortPoint head;
    ShortPoint tail;
    ShortPoint lastHead;
    ShortPoint lastTail;
    short life;
    short weapType;
    short owner;
    short target;
    short fixedRange;
    Boolean sourceShip;
} BeamRecord;

typedef struct AsteroidObj {
    FloatPoint pos;
    FloatPoint vel;
    short type;
    short frame;
    short direction;
    short timer;
    Boolean active;
    Boolean spawned;
} AsteroidObj;

typedef struct PodObj {
    FloatPoint pos;
    FloatPoint vel;
    short life;
    short curSystem;
    short frame;
    short spinDir;
} PodObj;

typedef struct StarSlot {
    FloatPoint pos;
    short type;
    short frame;
    double depth;
} StarSlot;

typedef struct MissionRecord {
    short travelStellar;
    char unk_02[2];
    short returnStellar;
    short shipCount;
    short shipDude;
    short shipGoal;
    short shipBehav;
    short shipSyst;
    short cargoType;
    short cargoQty;
    short pickupMode;
    short dropoffMode;
    short scanGovt;
    short compBitSet[4];
    short compGovt;
    short compReward;
    short failBitSet[2];
    long payment;
    short specialShipsKilled;
    short specialShipsBoarded;
    short specialShipsDisabled;
    short specialShipsJumpedIn;
    short specialShipsJumpedOut;
    short initialShipCount;
    Boolean failIfScan;
    Boolean canAbort;
    Boolean cargoPickedUp;
    short briefDesc;
    short quickBriefDesc;
    short loadCargoDesc;
    short dropCargoDesc;
    short compDesc;
    short failDesc;
    short refuseDesc;
    short timeLimit;
    short shipStrList;
    short shipStrIndex;
    short unused44;
    short shipDelay;
    short misnIndex;
    unsigned short flags;
    short auxShipCount;
    short auxShipDude;
    short auxShipSyst;
    short auxShipsSpawned;
    short auxShipDelay;
    short auxShipsLeft;
    char shipName[32];
    char name[256];
} MissionRecord;

typedef struct BriefMissionRec {
    short destStellar;
    short destSystem;
    short cargoType;
    short cargoUnits;
    short returnStellar;
    short returnSystem;
    short startBitSet;
    DateTimeRec dueDate;
} BriefMissionRec;

typedef struct MissionAvailRec {
    short availStel;
    short availBitSet;
    short availBitClr;
    short availLoc;
    short availRecord;
    short availRating;
    short availRandom;
    short cargoQty;
    short availShipType;
    short flags;
} MissionAvailRec;

typedef struct MissionState {
    Boolean active;
    Boolean returnDone;
    Boolean objectivesDone;
    Boolean failed;
    DateTimeRec dueDate;
} MissionState;

#pragma options align=reset

#endif
