/* shipobj.h -- the ship object (shipData[MaxShipsInSystem], 0 is the player) and the shot. Notes: Research/notes/structs.md */
#ifndef SHIPOBJ_H
#define SHIPOBJ_H

#include "gametypes.h"

#pragma options align=mac68k

typedef struct WeapSlot {
    short count;
    short massDmg;
    short energyDmg;
    short guidance;
    short ammo;
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
} WeapSlot;

/* ShipObj.navTargetKind, by the captions of Status.c's nav panel */
#define kNavOff				(-1)
#define kNavStellar			2
#define kNavHyperspace		3

/* ShipObj.aiState, by the labels DrawHighGoalAbbreviation draws */
#define kStateHiJack		0
#define kStateGoStel		1
#define kStateHypOut		2
#define kStateDefRet		3
#define kStateFightSh		4
#define kStateGoHome		5
#define kStateWait			6
#define kStateInspect		7
#define kStateJumpIn		8
#define kStateRefuel		9
#define kStateFlyPr			10
#define kStateHypPr			11
#define kStateProtect		12
#define kStatePlunder		13

#define kJumpWindupArriving	(-999)
#define kJumpArrivalCutoff	(-900)
#define kJumpInSpeed		50.0	/* speed of a ship arriving from hyperspace */
#define kJumpInDecel		1.165	/* it brakes by this much a frame */
#define kJumpInDistance		1000	/* it appears this far out, plus the braking distance */
#define kShipAmmoFactor		1.75	/* ammo a spawned ship carries, times its type's load */
#define kBrakeDamping		0.95
#define kStillSpeed		0.35	/* below this a ship counts as stopped */

/* ShipObj.aiSubstate, by the labels DrawLowGoalAbbreviation draws (TurnFire is "T+F") */
#define kManeuverLoJack			0
#define kManeuverKillSpd		1
#define kManeuverFlyStel		2
#define kManeuverFlyHypD		3
#define kManeuverHypJmp			4
#define kManeuverRunAwy			5
#define kManeuverTurnFire		6
#define kManeuverMissle			7
#define kManeuverDock			8
#define kManeuverChase			9
#define kManeuverZoom			10
#define kManeuverChasSlw		11
#define kManeuverFormFly		12
#define kManeuverJmpPar			13
#define kManeuverWaitTarg		14
#define kManeuverBoard			15
#define kManeuverVeerOff		16
#define kManeuverAfterBurn		17

typedef struct ShipObj {
    FloatPoint pos;
    FloatPoint vel;
    float thrust;
    float speedLimit;
    float fuel;
    float deathTimer;
    float skill;
    short heading;
    short aiTimer;
    short desiredHeading;
    short navTargetKind;
    short targetStellar;
    short hasTargetLock;
    short targetShip;
    short selectedWeapon;
    short curSystem;
    short type;
    short dude;
    short cargo[NumCommodities];
    short index;
    short aiType;
    short auxMission;
    short defenseStellar;
    short veerHeading;
    short lastInspected;
    short lastStellar;
    short turretMountCycle;
    short prevSystem;
    short jumpTimer;
    short grudge;
    short govt;
    short parentShip;
    long credits;
    long jumpTick;
    long shield;
    Boolean hasWorldSpriteNode;
    Boolean active;
    Boolean boarded;
    Boolean fireQueued;
    Boolean isTractored;
    Boolean hired;
    Boolean hailed;
    Boolean hasAfterburner;
    WeapSlot weap[MaxWeaponTypes];
    short aiState;
    short aiSubstate;
    short aggression;
    short incomingDamage;
    short persType;
    short specialMission;
    short altFireSide;
} ShipObj;

/* ShotObj.guidanceState, by the seeker bits that set it */
#define kShotStateNormal		0
#define kShotStateAsteroidDecoy	1
#define kShotStateFlareDecoy	2
#define kShotStateConfused		999

typedef struct ShotObj {
    FloatPoint pos;
    FloatPoint vel;
    short weapType;
    short target;
    short owner;
    short curSystem;
    short guidanceState;
    short life;
    short angle;
    short damageFalloffTimer;
    short damageFalloffSteps;
    short frame;
    Boolean fromGuardingEscort;
} ShotObj;

#pragma options align=reset

#endif
