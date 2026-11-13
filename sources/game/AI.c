/* AI.c -- the NPC ship AI: per-behaviour state machines, weapon picking, targeting. Notes: Research/notes/AI.md */
#include <Types.h>
#include <stdlib.h>
#include <string.h>
#include <TextUtils.h>
#include <Quickdraw.h>
#include <Events.h>

#include "evo.h"

#define InEngagedState(s)	((s)->aiState != kStateInspect && (s)->aiState != kStateRefuel && (s)->aiState != kStateFlyPr \
		&& (s)->aiState != kStateHypPr && (s)->aiState != kStateGoHome && (s)->aiState != kStateProtect)

#define kMascotStrList		30000
#define kTrialDays		30
#define kAfterburnRange		82
#define kStrafeRange		123


extern void		AiEngage(ShipObj *ship);
extern void		AiReact(ShipObj *ship);

Boolean asteroidsDirty;

// FUNCTION: EVO 0x1000001C
void AIDispatch(ShipObj *ship)
{
	short i;

	if (ship->type == kEscapePodShip) {
		ship->thrust = ShipAccelRate(ship);
		ship->speedLimit = ShipMaxSpeed(ship);
		ship->desiredHeading = ship->heading;
		ship->aiTimer--;
		if (ship->aiTimer < 0)
			ship->active = false;
		return;
	}
	if (ship->specialMission != -1 && !currentMissionObjectives[ship->specialMission].active) {
		ship->specialMission = -1;
		if (ship->parentShip != -1) {
			ship->aiType = gShipTypes[ship->type].inherentAI;
			ship->parentShip = -1;
			AIMakeShipLeave(ship);
		}
	}
	if (ship->jumpTimer < kJumpArrivalCutoff) {
		ship->aiState = kStateJumpIn;
	} else if (ship->aiSubstate != kManeuverHypJmp && ship->aiSubstate != kManeuverJmpPar) {
		if (ship->defenseStellar != -1)
			DefenseShipAI(ship);
		else if (ship->aiType == kAIWimpyTrader)
			WimpyTraderAI(ship);
		else if (ship->aiType == kAIBraveTrader)
			BraveTraderAI(ship);
		else if (ship->aiType == kAIWarship)
			WarshipAI(ship);
		else if (ship->aiType == kAIInterceptor)
			InterceptorAI(ship);
		else if (ship->aiType == kAIFighter)
			AiFighter(ship);
		else if (ship->aiType == kAIEscort)
			EscortAI(ship);
	}
	HighLevelAIHandler(ship);
	LowLevelAIHandler(ship);
}

// FUNCTION: EVO 0x10000210
void ScanPlayer(ShipObj *ship)
{
	short i;
	Boolean found;
	char text[256];

	if ((ship->aiType == kAIWarship || ship->aiType == kAIInterceptor)
			&& ship->govt != -1 && govtData[ship->govt].penalties[kPenaltySmuggle]) {
		found = false;
		for (i = 0; i < 8; i++) {
			if (ship->govt == currentMissions[i].scanGovt) {
				found = true;
				if (found && currentMissions[i].cargoPickedUp && currentMissions[i].cargoType != -1
						&& FloatAbs(ship->pos.x - shipData->pos.x) <= 180.0f
						&& FloatAbs(ship->pos.y - shipData->pos.y) <= 180.0f) {
					if (currentMissions[i].failIfScan && !currentMissionObjectives[i].failed) {
						currentMissionObjectives[i].failed = true;
						TriggerSoundPlay(beepSnd[kBeepClick], 1, kMidVolume);
						DisplayComm("\pYour ship has been scanned - mission failed.", 400, 0, kCommTextSize, &rgbWhite, false, false);
						QuickMissionFailure(i);
						ship->targetShip = 0;
						ship->targetStellar = -1;
						ship->aiState = kStateFightSh;
						return;
					} else {
						strncpy(text, (char *)shipName, 64);
						p2cstr((StringPtr)text);
						strcat(text, ", your attempt to smuggle illegal cargo through this system has been detected.");
						c2pstr(text);
						TriggerSoundPlay(beepSnd[kBeepMessage], 1, kMidVolume);
						DisplayComm((StringPtr)text, 400, 0, kCommTextSize, &rgbWhite, false, false);
						ship->targetShip = 0;
						ship->targetStellar = -1;
						ship->aiState = kStateFightSh;
						return;
					}
				}
			}
		}
	}
}

// FUNCTION: EVO 0x100004A8
void AISetup(ShipObj *ship)
{
	ship->aiState = kStateHiJack;
	ship->aiSubstate = kManeuverLoJack;
	ship->lastStellar = -2;
	ship->lastInspected = -1;
}

// FUNCTION: EVO 0x100004CC
void WimpyTraderAI(ShipObj *ship)
{
	if (!IsDisabled(ship) && ship->aiState != kStateRefuel) {
		if (ship->aiState == kStateHiJack) {
			if (RecentlyVisitedStellarInSystem(ship, ship->curSystem)) {
				AIMakeShipLeave(ship);
			} else {
				ship->targetStellar = -1;
				SelectRandomStellarDest(ship);
				if (ship->targetStellar == -1)
					AIMakeShipLeave(ship);
			}
		}
		if (ship->grudge > 0 && ship->targetShip != -1) {
			if (ship->parentShip == 0) {
				ship->aiState = kStateFlyPr;
				ship->targetStellar = 0;
			} else {
				ship->aiState = kStateDefRet;
			}
		}
		if (ship->aiState == kStateDefRet)
			AICallForHelp(ship);
	}
}

// FUNCTION: EVO 0x100005CC
void BraveTraderAI(ShipObj *ship)
{
	short dx, dy;

	if (!IsDisabled(ship) && ship->aiState != kStateRefuel) {
		if (ship->aiState == kStateHiJack) {
			if (RecentlyVisitedStellarInSystem(ship, ship->curSystem)) {
				AIMakeShipLeave(ship);
			} else {
				ship->targetStellar = -1;
				SelectRandomStellarDest(ship);
				if (ship->targetStellar == -1)
					AIMakeShipLeave(ship);
			}
		}
		if (ship->grudge > 0 && ship->targetShip != -1) {
			dx = abs((short)(ship->pos.x - shipData[ship->targetShip].pos.x));
			dy = abs((short)(ship->pos.y - shipData[ship->targetShip].pos.y));
			if (dx <= 1250 && dy <= 1250) {
				if (ship->jumpTimer <= 0)
					ship->aiState = kStateFightSh;
			} else if (ship->parentShip == 0) {
				ship->aiState = kStateFlyPr;
				ship->targetStellar = 0;
			} else {
				ship->aiState = kStateDefRet;
			}
		}
		if (ship->aiState == kStateDefRet) {
			AICallForHelp(ship);
		} else if (ship->aiState == kStateFightSh) {
			if (ship->targetShip != -1 && ship->targetShip != 0
					&& shipData[ship->targetShip].aiType >= kAIWarship)
				AICallForHelp(ship);
		}
	}
}

// FUNCTION: EVO 0x100007D4
void WarshipAI(ShipObj *ship)
{
	short i, count;
	long threshold;
	Boolean neverAttacks;

	if (!IsDisabled(ship) && ship->aiState != kStateRefuel) {
		if (ship->govt != -1 && ship->jumpTimer > kJumpArrivalCutoff && ship->jumpTimer <= 0) {
			if (govtData[ship->govt].flags & kGovtAlwaysAttacks) {
				if (!IsThreatToPlayer(ship)) {
					AIMakeShipAttackPlayer(ship);
					ship->targetShip = 0;
					ship->grudge = 1;
				}
			} else {
				neverAttacks = govtData[ship->govt].flags & kGovtNeverAttacks;
				if (neverAttacks && IsThreatToPlayer(ship)) {
					ship->targetShip = -1;
					ship->aiState = kStateHiJack;
				}
			}
		}
		if (ship->aiState == kStateHiJack) {
			if (ship->targetShip != -1) {
				if (ship->jumpTimer <= 0 && ship->aiState != kStateInspect) {
					ship->aiState = kStateFightSh;
					AIPlunderIfPossible(ship);
				}
			} else if (ship->jumpTimer <= 0 && ship->aiState != kStateDefRet) {
				SelectWarshipTarget(ship);
				if (ship->targetShip != -1) {
					ship->aiState = kStateFightSh;
					AIPlunderIfPossible(ship);
				} else if (RecentlyVisitedStellarInSystem(ship, ship->curSystem)) {
					AIMakeShipLeave(ship);
				} else {
					ship->targetStellar = -1;
					SelectRandomStellarDest(ship);
					if (ship->targetStellar == -1)
						AIMakeShipLeave(ship);
				}
			}
		}
		if (ship->grudge > 0 && ship->targetShip != -1 && ship->jumpTimer <= 0
				&& ship->aiState != kStateInspect && ship->aiState != kStateDefRet) {
			ship->aiState = kStateFightSh;
			AIPlunderIfPossible(ship);
		}
		if (ship->aiState == kStateGoStel || ship->aiState == kStateHypOut) {
			if (ship->targetShip != -1) {
				if (ship->jumpTimer <= 0 && ship->aiState != kStateInspect) {
					ship->aiState = kStateFightSh;
					AIPlunderIfPossible(ship);
				}
			} else if (ship->aiState != kStateDefRet) {
				SelectWarshipTarget(ship);
				if (ship->targetShip == -1) {
					count = 0;
					for (i = 1; i < MaxShipsInSystem; i++) {
						if (ship->index == shipData[i].parentShip
								&& shipData[i].aiType == kAIFighter
								&& shipData[i].active
								&& shipData[i].defenseStellar == -1
								&& !IsDisabled(&shipData[i]))
							count++;
					}
					if (count > 0)
						ship->aiState = kStateWait;
				} else if (ship->jumpTimer <= 0) {
					ship->aiState = kStateFightSh;
					AIPlunderIfPossible(ship);
				}
			}
		}
		if (ship->aiState == kStateWait) {
			ship->targetShip = -1;
			SelectWarshipTarget(ship);
			if (ship->targetShip == -1) {
				count = 0;
				for (i = 1; i < MaxShipsInSystem; i++) {
					if (ship->index == shipData[i].parentShip
							&& shipData[i].active
							&& shipData[i].defenseStellar == -1
							&& !IsDisabled(&shipData[i]))
						count++;
				}
				if (count == 0)
					ship->aiState = kStateHiJack;
			}
		}
		if (ship->grudge > 0 && ship->targetShip != -1 && ship->jumpTimer <= 0) {
			if (ship->govt != -1) {
				if (ship->shield <= 0.25 * ShipShieldCapacity(ship)
						&& (govtData[ship->govt].flags & kGovtNoPersPod))
					ship->aiState = kStateDefRet;
			}
			if (ship->aiState == kStateHypOut) {
				ship->aiState = kStateFightSh;
				AIPlunderIfPossible(ship);
			} else if (ship->aiState != kStateInspect && ship->aiState != kStateDefRet && ship->aiState != kStateHypPr) {
				ship->aiState = kStateFightSh;
				AIPlunderIfPossible(ship);
			}
		}
		if (ship->targetShip != -1 && ship->aiState != kStateInspect) {
			if (ship->jumpTimer <= 0) {
				ship->aiState = kStateFightSh;
				AIPlunderIfPossible(ship);
			}
			if (ship->persType == -1) {
				threshold = kLowestValue;
				if (ship->aggression == 1)
					threshold = 0.3 * ShipShieldCapacity(ship);
				if (ship->aggression == 2)
					threshold = 0.15 * ShipShieldCapacity(ship);
				if (ship->aggression == 4)
					threshold = kLowestValue;
			} else {
				threshold = 0.01 * personData[ship->persType].coward * ShipShieldCapacity(ship);
			}
			if (ship->shield < threshold && ship->parentShip == -1 && ship->govt != -1
					&& (govtData[ship->govt].flags & kGovtRetreats)
					&& ship->aiState != kStateHypOut && ship->aiState != kStateDefRet && ship->aiState != kStateHypPr)
				ship->aiState = kStateDefRet;
		}
	}
}

// FUNCTION: EVO 0x10000F1C
void InterceptorAI(ShipObj *ship)
{
	short i, k, count, cand, mission, phrase;
	char text1[256];
	char text2[256];
	Boolean neverAttacks;

	if (!IsDisabled(ship)) {
		if (ship->persType == kPersAmbrosiaMascot) {
			if (ship->jumpTimer <= 0 && ship->aiTimer <= 0) {
				if (ship->lastInspected != 0) {
					ship->aiState = kStateInspect;
					ship->targetShip = 0;
				} else if (ship->aiState != kStateFightSh) {
					AIMakeShipLeave(ship);
				}
			}
			return;
		}
		if (ship->aiState != kStateRefuel) {
			if (ship->govt != -1 && ship->jumpTimer > kJumpArrivalCutoff && ship->jumpTimer <= 0) {
				if (govtData[ship->govt].flags & kGovtAlwaysAttacks) {
					if (!IsThreatToPlayer(ship)) {
						AIMakeShipAttackPlayer(ship);
						ship->targetShip = 0;
						ship->grudge = 1;
					}
				} else {
					neverAttacks = govtData[ship->govt].flags & kGovtNeverAttacks;
					if (neverAttacks && IsThreatToPlayer(ship)) {
						ship->targetShip = -1;
						ship->aiState = kStateHiJack;
					}
				}
			}
			if (ship->grudge > 0 && ship->targetShip != -1 && ship->aiState != kStateInspect
					&& ship->aiState != kStateRefuel && ship->jumpTimer <= 0)
				ship->aiState = kStateFightSh;
			for (i = 0; i < MaxShipsInSystem; i++) {
				if (!shipData[i].active && i == ship->lastInspected)
					ship->lastInspected = -1;
			}
			if (ship->aiState == kStateHiJack || ship->aiState == kStateGoStel) {
				if (ship->aiTimer <= 0) {
					if (ship->targetShip != -1) {
						if (ship->jumpTimer <= 0 && shipData[ship->targetShip].active
								&& ship->grudge > 0)
							ship->aiState = kStateFightSh;
					} else {
						ship->targetShip = -1;
						SelectWarshipTarget(ship);
						if (ship->targetShip != -1 && ship->jumpTimer <= 0)
							ship->aiState = kStateFightSh;
					}
					if (ship->targetShip == -1) {
						count = 0;
						for (i = 0; i < MaxShipsInSystem; i++) {
							if (i != ship->index && shipData[i].active && i != ship->lastInspected
									&& shipData[i].aiType != kAIInterceptor
									&& (i != 0 || !gInvertedColors)
									&& ship->curSystem == shipData[i].curSystem)
								count++;
						}
						if (count > 0) {
							while (ship->targetShip == -1) {
								cand = Rand(MaxShipsInSystem);
								if (cand != ship->index && shipData[cand].active
										&& cand != ship->lastInspected
										&& shipData[cand].aiType != kAIInterceptor
										&& (cand != 0 || !gInvertedColors)
										&& ship->curSystem == shipData[cand].curSystem) {
									ship->targetShip = cand;
									ship->lastInspected = cand;
									ship->aiState = kStateInspect;
									if (cand == 0) {
										mission = -1;
										for (k = 0; k < 8; k++) {
											if (currentMissionObjectives[k].active
													&& currentMissions[k].cargoPickedUp
													&& currentMissions[k].scanGovt != -1
													&& ship->govt == currentMissions[k].scanGovt) {
												mission = k;
												break;
											}
										}
										if (mission != -1) {
											strncpy(text1, (char *)gShipTypes[ship->type].name, 63);
											p2cstr((StringPtr)text1);
											strcat(text1, ": ");
											strncpy(text2, (char *)shipName, 63);
											p2cstr((StringPtr)text2);
											strcat(text1, text2);
											phrase = Rand(3);
											if (phrase == 0)
												strcat(text1, ", prepare to be scanned.");
											if (phrase == 1)
												strcat(text1, ", prepare for a cargo inspection.");
											if (phrase == 2)
												strcat(text1, ", hold position for a cargo inspection.");
											c2pstr(text1);
											TriggerSoundPlay(beepSnd[kBeepMessage], 1, kMidVolume);
											DisplayComm((StringPtr)text1, 400, 0, kCommTextSize, &rgbWhite, false, false);
										}
									}
								}
							}
						}
					}
					if (ship->targetShip == -1 && ship->aiState == kStateHiJack) {
						ship->lastStellar = -2;
						cand = gSystems[ship->curSystem].navDefault[0];
						if (cand == -1) {
							ship->aiState = kStateWait;
						} else {
							ship->targetStellar = cand;
							ship->aiState = kStateGoStel;
						}
					}
				}
			}
		}
	}
}

// FUNCTION: EVO 0x10001578
void AiFighter(ShipObj *ship)
{
	short i;

	if (IsDisabled(ship)) {
		if (ship->parentShip != -1 && ship->defenseStellar == -1) {
			ship->aiState = kStateGoHome;
			ship->aiSubstate = kManeuverLoJack;
			ship->targetStellar = ship->parentShip;
		} else {
			ship->aiState = kStateWait;
		}
	} else {
		if (ship->targetShip >= 0 && ship->targetShip != ship->parentShip) {
			ship->aiState = kStateFightSh;
			if (ship->parentShip != -1 && shipData[ship->parentShip].parentShip == 0) {
				ship->targetShip = -1;
				ship->aiState = kStateHiJack;
				ship->aiSubstate = kManeuverLoJack;
				AIFightThreatToPlayer(ship);
				if (ship->targetShip == -1)
					AIFightThreatToParent(ship);
				if (ship->targetShip == -1) {
					ship->targetShip = ship->parentShip;
					ship->aiState = kStateGoHome;
				}
			}
		}
		if (ship->targetShip == -1 && ship->aiState != kStateWait && ship->aiState != kStateGoHome
				&& ship->parentShip != -1 && ship->defenseStellar == -1) {
			if (shipData[ship->parentShip].targetShip == -1 && ship->parentShip != 0) {
				ship->aiState = kStateGoHome;
				ship->targetStellar = ship->parentShip;
			} else if (ship->parentShip == 0) {
				ship->aiState = kStateFlyPr;
				ship->targetStellar = ship->parentShip;
			} else if (shipData[ship->parentShip].aiType == kAIWarship) {
				if (ship->index != shipData[ship->parentShip].targetShip) {
					ship->aiState = kStateFightSh;
					ship->targetShip = shipData[ship->parentShip].targetShip;
				}
			}
		}
		if (shipData[ship->parentShip].jumpTimer > 0)
			ship->aiState = kStateHypPr;
	}
}

// FUNCTION: EVO 0x100017BC
void EscortAI(ShipObj *ship)
{
	short i, dx, dy;

	if (ship->parentShip == -1) {
		ship->aiType = gShipTypes[ship->type].inherentAI;
		ship->aiState = kStateHiJack;
		ship->aiSubstate = kManeuverLoJack;
	} else if (!shipData[ship->parentShip].active || IsDisabled(&shipData[ship->parentShip])) {
		ship->parentShip = -1;
		ship->aiType = gShipTypes[ship->type].inherentAI;
		ship->aiState = kStateHiJack;
		ship->aiSubstate = kManeuverLoJack;
	} else if (ship->parentShip != 0 && AIIsShipJumping(&shipData[ship->parentShip])) {
		for (i = 1; i < MaxShipsInSystem; i++) {
			if (ExtendedIsThreatToPlayer(&shipData[i]) && shipData[i].parentShip != 0) {
				AIMakeShipAttackShip(ship, &shipData[i]);
				break;
			}
		}
		return;
	} else {
		if ((ship->targetShip == -1 || ship->aiState == kStateHiJack || ship->aiState == kStateFlyPr)
				&& (ship->aiState == kStateFlyPr || ship->aiState == kStateProtect)
				&& gShipTypes[ship->type].inherentAI >= kAIWarship && ship->aiTimer <= 0) {
			if (ship->targetShip == -1 || ship->targetShip == ship->parentShip)
				ship->grudge = 0;
			if (ship->parentShip == 0)
				AIFightThreatToPlayer(ship);
			else
				AIFightThreatToParent(ship);
		}
		if (ship->grudge > 0 && ship->targetShip != -1 && ship->targetShip != ship->parentShip) {
			if (gShipTypes[ship->type].inherentAI <= kAIBraveTrader) {
				dx = abs((short)(ship->pos.x - shipData[ship->targetShip].pos.x));
				dy = abs((short)(ship->pos.y - shipData[ship->targetShip].pos.y));
				if (dx <= 250 && dy <= 250) {
					if (gShipTypes[ship->type].inherentAI == kAIWimpyTrader && ship->aiState != kStateHypOut
							&& ship->aiState != kStateDefRet && ship->aiState != kStateHypPr)
						ship->aiState = kStateDefRet;
					if (gShipTypes[ship->type].inherentAI == kAIBraveTrader)
						ship->aiState = kStateFightSh;
				} else {
					ship->grudge = 0;
					ship->aiState = kStateFlyPr;
					ship->targetStellar = ship->parentShip;
					ship->targetShip = -1;
				}
			} else {
				ship->aiState = kStateFightSh;
				if (ship->parentShip != -1 && shipData[ship->parentShip].parentShip == 0) {
					ship->targetShip = -1;
					ship->aiState = kStateHiJack;
					ship->aiSubstate = kManeuverLoJack;
				}
			}
		}
		if (ship->targetShip == -1 && ship->aiState != kStateWait && ship->aiState != kStateHypPr) {
			ship->aiState = kStateFlyPr;
			ship->targetStellar = ship->parentShip;
		}
		if (ship->aiState == kStateFlyPr && ship->parentShip == 0) {
			AIFightThreatToPlayer(ship);
			ship->aiState = kStateFlyPr;
			ship->targetStellar = 0;
		}
	}
}

// FUNCTION: EVO 0x10001BF4
void DefenseShipAI(ShipObj *ship)
{
	short i;
	long best;
	float dist;
	short a, b;
	Str31 name;

	a = 0;
	b = -1;
	best = -1;
	for (i = 0; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && (i == 0 || shipData[i].parentShip == 0)) {
			dist = FloatAbs(XYFloatSquaredDist(&ship->pos, &shipData[i].pos));
			if (dist <= 2400 * 3000) {
				if (i == 0)
					a = 1;
				if (dist < best || best == -1) {
					best = dist;
					b = i;
				}
			}
		}
	}
	ship->aiTimer = 0;
	ship->targetStellar = -1;
	ship->aiState = kStateFightSh;
	if (ship->targetShip == -1) {
		ship->targetShip = 0;
	} else if (ship->targetShip != -1 && ship->targetShip != 0
			&& shipData[ship->targetShip].parentShip != 0) {
		ship->targetShip = 0;
	}
}

// FUNCTION: EVO 0x10001D64
void HighLevelAIHandler(ShipObj *ship)
{
	short kind, range, zone;
	short dx, dy;
	Boolean flag;
	FloatPoint d;
	FloatPoint unused;
	char pad[4];
	char text1[256];
	char text2[256];

	if (ship->aiTimer > 0 && ship->aiState != kStateFlyPr)
		return;
	if (ship->targetShip != -1 && !shipData[ship->targetShip].active)
		ship->targetShip = -1;
	if (ship->aiState != kStateHypPr && ship->aiState != kStateHypOut && ship->aiState != kStateDefRet)
		ship->jumpTimer = 0;
	if (ship->jumpTimer > 0) {
		if (ship->parentShip == 0)
			ship->aiState = kStateHypPr;
		else if (ship->targetShip != -1)
			ship->aiState = kStateDefRet;
		else
			ship->aiState = kStateHypOut;
	}
	if (ship->specialMission != -1 && ship->aiState != kStateJumpIn && ship->aiState != kStateHypPr) {
		kind = currentMissions[ship->specialMission].shipBehav;
		while (kind >= 9)
			kind -= 10;
		if (kind == 1) {
			if (currentMissionObjectives[ship->specialMission].active && !IsDisabled(ship)) {
				ship->aiType = kAIEscort;
				ship->parentShip = 0;
			} else {
				ship->aiType = gShipTypes[ship->type].inherentAI;
				ship->parentShip = -1;
			}
		}
		if (kind == 0)
			AIMakeShipAttackPlayer(ship);
	}
	if (ship->targetShip == 0 && gInvertedColors) {
		if (ship->aiType <= kAIBraveTrader) {
			AIMakeShipLeave(ship);
		} else {
			ship->aiState = kStateWait;
			ship->aiSubstate = kManeuverKillSpd;
			ship->jumpTimer = 0;
			ship->targetStellar = -1;
			return;
		}
	}
	if (ship->aiState == kStateGoStel && ship->targetStellar != -1) {
		ship->jumpTimer = 0;
		d.x = (short)(stellarData[ship->targetStellar].pos.h - ship->pos.x);
		d.y = (short)(stellarData[ship->targetStellar].pos.v - ship->pos.y);
		zone = (8 - ShipTurnRate(ship)) * 48;
		if (FloatAbs(d.x) > zone || FloatAbs(d.y) > zone) {
			ship->aiSubstate = kManeuverFlyStel;
		} else if (FloatAbs(ship->vel.x) >= kStillSpeed || FloatAbs(ship->vel.y) >= kStillSpeed) {
			ship->aiSubstate = kManeuverKillSpd;
		} else {
			ship->vel.x = ship->vel.y = 0.0;
			ship->lastStellar = ship->targetStellar;
			ship->aiSubstate = kManeuverLoJack;
			ship->aiState = kStateHiJack;
			ship->aiTimer = Rand(200) + 300;
		}
	}
	if (ship->aiState == kStateHypOut) {
		flag = true;
		d.x = d.y = 0.0;
		if (FloatAbs(XYFloatSquaredDist(&d, &ship->pos)) <= 1000000.0)
			flag = false;
		if (flag) {
			if (FloatAbs(ship->vel.x) >= 0.7 || FloatAbs(ship->vel.y) >= 0.7)
				ship->aiSubstate = kManeuverKillSpd;
			else
				ship->aiSubstate = kManeuverHypJmp;
		} else {
			ship->aiSubstate = kManeuverFlyHypD;
		}
	}
	if (ship->aiState == kStateHypPr) {
		ship->targetStellar = shipData[ship->parentShip].targetStellar;
		ship->aiSubstate = kManeuverJmpPar;
	}
	if (ship->aiState == kStateDefRet) {
		if (ship->jumpTimer > 0) {
			flag = true;
			d.x = d.y = 0.0;
			if (FloatAbs(XYFloatSquaredDist(&d, &ship->pos)) <= 1000000.0)
				flag = false;
			if (flag) {
				ship->aiSubstate = kManeuverHypJmp;
			} else {
				ship->jumpTimer = 0;
				ship->aiSubstate = kManeuverFlyHypD;
			}
		} else if (ship->targetShip != -1) {
			dx = abs((short)(ship->pos.x - shipData[ship->targetShip].pos.x));
			dy = abs((short)(ship->pos.y - shipData[ship->targetShip].pos.y));
			if (dx <= 250 && dy <= 250 && ship->aiSubstate != kManeuverHypJmp) {
				if (ship->jumpTimer <= 0)
					ship->aiSubstate = kManeuverRunAwy;
			} else {
				flag = true;
				d.x = d.y = 0.0;
				if (FloatAbs(XYFloatSquaredDist(&d, &ship->pos)) <= 1000000.0)
					flag = false;
				if (flag) {
					if (FloatAbs(ship->vel.x) >= kStillSpeed || FloatAbs(ship->vel.y) >= kStillSpeed)
						ship->aiSubstate = kManeuverKillSpd;
					else
						ship->aiSubstate = kManeuverHypJmp;
				} else {
					ship->aiSubstate = kManeuverFlyHypD;
				}
			}
		} else {
			ship->aiState = kStateHiJack;
			ship->grudge = 0;
		}
	}
	if (ship->aiState == kStateFightSh) {
		ship->jumpTimer = 0;
		if (ship->targetShip != -1 && ship->aiTimer <= 0) {
			if (ship->parentShip != -1) {
				if (ship->targetShip == ship->parentShip) {
					ship->aiState = kStateHiJack;
					ship->aiSubstate = kManeuverLoJack;
					ship->targetShip = -1;
					return;
				}
				if (ship->parentShip == shipData[ship->targetShip].parentShip) {
					ship->aiState = kStateHiJack;
					ship->aiSubstate = kManeuverLoJack;
					ship->targetShip = -1;
					return;
				}
				if (shipData[ship->targetShip].parentShip != -1
						&& shipData[shipData[ship->targetShip].parentShip].parentShip != -1) {
					if (shipData[ship->targetShip].parentShip
							== shipData[shipData[ship->targetShip].parentShip].parentShip) {
						ship->aiState = kStateHiJack;
						ship->aiSubstate = kManeuverLoJack;
						ship->targetShip = -1;
						return;
					}
					if (shipData[ship->parentShip].parentShip != -1
							&& shipData[shipData[ship->parentShip].parentShip].parentShip
								== shipData[shipData[ship->targetShip].parentShip].parentShip) {
						ship->aiState = kStateHiJack;
						ship->aiSubstate = kManeuverLoJack;
						ship->targetShip = -1;
						return;
					}
				}
			}
			if (ship->parentShip == ship->targetShip && ship->defenseStellar == -1) {
				ship->grudge = 0;
				ship->targetShip = -1;
				ship->aiState = kStateHiJack;
			}
			if (shipData[ship->targetShip].active && !IsDying(&shipData[ship->targetShip])) {
				if (FloatAbs(ship->pos.x - shipData[ship->targetShip].pos.x) > 165.0f
						|| FloatAbs(ship->pos.y - shipData[ship->targetShip].pos.y) > 165.0f) {
					if (AIEvalHopelessChase(ship)) {
						if (ship->aiType >= kAIWarship) {
							ship->aiSubstate = kManeuverWaitTarg;
						} else {
							ship->aiState = kStateDefRet;
							ship->aiSubstate = kManeuverRunAwy;
						}
					} else if (ship->aiSubstate != kManeuverAfterBurn) {
						ship->aiSubstate = kManeuverMissle;
					}
				} else if (ship->aiSubstate != kManeuverVeerOff) {
					ship->aiSubstate = kManeuverTurnFire;
				}
				if (!IsDisabled(ship))
					AILaunchFighter(ship);
			} else {
				ship->targetShip = -1;
				ship->grudge = 0;
				ship->aiState = kStateHiJack;
			}
		} else {
			ship->aiState = kStateHiJack;
		}
	}
	if (ship->aiState == kStatePlunder) {
		ship->jumpTimer = 0;
		if (ship->targetShip != -1 && ship->aiTimer <= 0) {
			if (ship->parentShip != -1) {
				if (ship->targetShip == ship->parentShip) {
					ship->aiState = kStateHiJack;
					ship->aiSubstate = kManeuverLoJack;
					ship->targetShip = -1;
					return;
				}
				if (ship->parentShip == shipData[ship->targetShip].parentShip) {
					ship->aiState = kStateHiJack;
					ship->aiSubstate = kManeuverLoJack;
					ship->targetShip = -1;
					return;
				}
				if (shipData[ship->targetShip].parentShip != -1
						&& shipData[shipData[ship->targetShip].parentShip].parentShip != -1) {
					if (shipData[ship->targetShip].parentShip
							== shipData[shipData[ship->targetShip].parentShip].parentShip) {
						ship->aiState = kStateHiJack;
						ship->aiSubstate = kManeuverLoJack;
						ship->targetShip = -1;
						return;
					}
					if (shipData[ship->parentShip].parentShip != -1
							&& shipData[shipData[ship->parentShip].parentShip].parentShip
								== shipData[shipData[ship->targetShip].parentShip].parentShip) {
						ship->aiState = kStateHiJack;
						ship->aiSubstate = kManeuverLoJack;
						ship->targetShip = -1;
						return;
					}
				}
			}
			if (ship->parentShip == ship->targetShip && ship->defenseStellar == -1) {
				ship->grudge = 0;
				ship->targetShip = -1;
				ship->aiState = kStateHiJack;
			}
			if (shipData[ship->targetShip].active) {
				if (IsDisabled(&shipData[ship->targetShip])) {
					if (shipData[ship->targetShip].boarded) {
						ship->aiState = kStateHiJack;
						ship->aiSubstate = kManeuverLoJack;
						ship->targetShip = -1;
						ship->targetStellar = -1;
					} else {
						ship->targetStellar = ship->targetShip;
						range = (10 - gShipTypes[ship->type].turnRate) * 30;
						if (FloatAbs(ship->pos.x - shipData[ship->targetShip].pos.x) <= range
								&& FloatAbs(ship->pos.y - shipData[ship->targetShip].pos.y) <= range) {
							ship->aiSubstate = kManeuverBoard;
						} else if (FloatAbs(ship->pos.x - shipData[ship->targetShip].pos.x) <= range * 2
								&& FloatAbs(ship->pos.y - shipData[ship->targetShip].pos.y) <= range * 2) {
							ship->aiSubstate = kManeuverChasSlw;
						} else {
							ship->aiSubstate = kManeuverChase;
						}
					}
				} else {
					ship->targetStellar = -1;
					if (FloatAbs(ship->pos.x - shipData[ship->targetShip].pos.x) > 165.0f
							|| FloatAbs(ship->pos.y - shipData[ship->targetShip].pos.y) > 165.0f) {
						if (AIEvalHopelessChase(ship)) {
							if (ship->aiType >= kAIWarship) {
								ship->aiSubstate = kManeuverWaitTarg;
							} else {
								ship->aiState = kStateDefRet;
								ship->aiSubstate = kManeuverRunAwy;
							}
						} else if (ship->aiSubstate != kManeuverAfterBurn) {
							ship->aiSubstate = kManeuverMissle;
						}
					} else if (ship->aiSubstate != kManeuverVeerOff && ship->aiSubstate != kManeuverAfterBurn) {
						ship->aiSubstate = kManeuverTurnFire;
					}
				}
			} else {
				ship->targetShip = -1;
				ship->targetStellar = -1;
				ship->grudge = 0;
				ship->aiState = kStateHiJack;
			}
		} else {
			ship->aiState = kStateHiJack;
		}
	}
	if (ship->aiState == kStateGoHome && ship->parentShip != -1 && ship->defenseStellar == -1) {
		ship->targetStellar = ship->parentShip;
		range = (10 - gShipTypes[ship->type].turnRate) * 50;
		if (FloatAbs(ship->pos.x - shipData[ship->parentShip].pos.x) <= range
				&& FloatAbs(ship->pos.y - shipData[ship->parentShip].pos.y) <= range)
			ship->aiSubstate = kManeuverDock;
		else
			ship->aiSubstate = kManeuverChasSlw;
		if (ship->parentShip == 0 && gInvertedColors)
			ship->aiSubstate = kManeuverKillSpd;
	}
	if (ship->aiState == kStateFlyPr && ship->parentShip != -1 && ship->defenseStellar == -1) {
		ship->targetStellar = ship->parentShip;
		range = (10 - gShipTypes[ship->type].turnRate) * 30;
		if (ship->parentShip == 0 && gInvertedColors) {
			ship->aiSubstate = kManeuverKillSpd;
		} else if (FloatAbs(ship->pos.x - shipData[ship->parentShip].pos.x) <= range
				&& FloatAbs(ship->pos.y - shipData[ship->parentShip].pos.y) <= range) {
			ship->aiSubstate = kManeuverFormFly;
		} else if (FloatAbs(ship->pos.x - shipData[ship->parentShip].pos.x) <= range * 2
				&& FloatAbs(ship->pos.y - shipData[ship->parentShip].pos.y) <= range * 2) {
			ship->aiSubstate = kManeuverChasSlw;
		} else {
			ship->aiSubstate = kManeuverChase;
		}
	}
	if (ship->aiState == kStateProtect) {
		ship->jumpTimer = 0;
		ship->targetStellar = 0;
		ship->targetShip = -1;
		range = (10 - gShipTypes[ship->type].turnRate) * 30;
		if (shipData[0].jumpTimer > 0) {
			ship->aiState = kStateHypPr;
		} else if (gInvertedColors) {
			ship->aiSubstate = kManeuverKillSpd;
		} else if (FloatAbs(ship->pos.x - shipData[0].pos.x) <= range
				&& FloatAbs(ship->pos.y - shipData[0].pos.y) <= range) {
			ship->aiSubstate = kManeuverFormFly;
		} else {
			range = (10 - gShipTypes[ship->type].turnRate) * 60;
			if (FloatAbs(ship->pos.x - shipData[0].pos.x) <= range
					&& FloatAbs(ship->pos.y - shipData[0].pos.y) <= range)
				ship->aiSubstate = kManeuverChasSlw;
			else
				ship->aiSubstate = kManeuverChase;
		}
	}
	if (ship->aiState == kStateWait) {
		ship->jumpTimer = 0;
		ship->aiSubstate = kManeuverKillSpd;
	}
	if (ship->aiState == kStateInspect) {
		if (ship->targetShip != -1 && ship->aiTimer <= 0) {
			if (shipData[ship->targetShip].active) {
				if (ship->targetShip == 0)
					asteroidsDirty = true;
				if (FloatAbs(ship->pos.x - shipData[ship->targetShip].pos.x) > 180.0f
						|| FloatAbs(ship->pos.y - shipData[ship->targetShip].pos.y) > 180.0f) {
					ship->aiSubstate = kManeuverChase;
				} else if (ship->targetShip == 0) {
					d.x = ship->vel.x - shipData[0].vel.x;
					d.y = ship->vel.y - shipData[0].vel.y;
					if (ship->persType == kPersAmbrosiaMascot)
						d.x = d.y = 0.0;
					if (FloatAbs(d.x) < kStillSpeed && FloatAbs(d.y) < kStillSpeed) {
						if (ship->persType == kPersAmbrosiaMascot) {
							ship->lastInspected = 0;
							if (gDaysUsed > kTrialDays) {
								TriggerSoundPlay(beepSnd[kBeepMessage], 1, kMidVolume);
								GetIndString((StringPtr)text1, kMascotStrList, Rand(3) + 4);
								DisplayComm((StringPtr)text1, 360, 0, kCommTextSize, &rgbWhite, false, false);
								ship->aiState = kStateFightSh;
								ship->targetShip = 0;
								ship->targetStellar = -1;
								ship->grudge = 1;
							} else {
								TriggerSoundPlay(beepSnd[kBeepMessage], 1, kMidVolume);
								GetIndString((StringPtr)text1, kMascotStrList, Rand(3) + 1);
								DisplayComm((StringPtr)text1, 360, 0, kCommTextSize, &rgbWhite, false, false);
								AIMakeShipLeave(ship);
							}
						} else {
							ship->targetStellar = -1;
							ship->targetShip = -1;
							ship->aiState = kStateHiJack;
							ship->aiSubstate = kManeuverLoJack;
							ScanPlayer(ship);
						}
					} else {
						ship->targetStellar = 0;
						ship->aiSubstate = kManeuverFormFly;
					}
				} else {
					ship->aiState = kStateHiJack;
					ship->targetShip = -1;
				}
			} else {
				ship->targetShip = -1;
				ship->aiState = kStateHiJack;
			}
		} else {
			ship->aiState = kStateHiJack;
		}
	}
	if (ship->aiState == kStateJumpIn) {
		ship->jumpTimer = kJumpWindupArriving;
		ship->aiSubstate = kManeuverZoom;
	}
	if (ship->aiState == kStateRefuel && ship->targetShip != -1) {
		ship->aiSubstate = kManeuverChasSlw;
		if (FloatAbs(ship->pos.x - shipData[ship->targetShip].pos.x) > 180.0f
				|| FloatAbs(ship->pos.y - shipData[ship->targetShip].pos.y) > 180.0f) {
			ship->aiSubstate = kManeuverChasSlw;
		} else if (FloatAbs(ship->vel.x) >= kStillSpeed || FloatAbs(ship->vel.y) >= kStillSpeed) {
			ship->aiSubstate = kManeuverKillSpd;
		} else {
			ship->vel.x = ship->vel.y = 0.0;
			if (shipData[ship->targetShip].fuel < 100) {
				shipData[ship->targetShip].fuel++;
				if (ship->targetShip == 0)
					hudRedraw.fuel = true;
			} else {
				if (ship->targetShip == 0) {
					TriggerSoundPlay(beepSnd[kBeepClick], 1, kMidVolume);
					strncpy(text1, (char *)gShipTypes[ship->type].name, 62);
					p2cstr((StringPtr)text1);
					strcat(text1, ": Fuel transfer complete, ");
					strncpy(text2, (char *)shipName, 62);
					p2cstr((StringPtr)text2);
					strcat(text1, text2);
					strcat(text1, ".");
					c2pstr(text1);
					DisplayComm((StringPtr)text1, kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
				}
				ship->aiState = kStateHiJack;
				ship->aiSubstate = kManeuverLoJack;
				ship->targetShip = -1;
				ship->targetStellar = -1;
			}
		}
	}
	if (ship->aiState == kStateHiJack)
		ship->aiSubstate = kManeuverLoJack;
}

// FUNCTION: EVO 0x1000366C
void LowLevelAIHandler(ShipObj *ship)
{
	short i, ok, angle, dir, range, n;
	Boolean disabled;
	FloatPoint zero, scaled, c, delta;
	FloatPoint gap;

	ship->thrust = 0.0;
	ship->fireQueued = false;
	if (ship->speedLimit >= 0.0)
		ship->speedLimit = ShipMaxSpeed(ship);
	disabled = IsDisabled(ship);
	ship->desiredHeading = ship->heading;
	if (ship->aiSubstate == kManeuverLoJack)
		AIEscortFireUnprovoked(ship);
	if (ship->aiSubstate == kManeuverKillSpd && !disabled) {
		if (FloatAbs(ship->vel.x) >= kStillSpeed || FloatAbs(ship->vel.y) >= kStillSpeed) {
			zero.x = zero.y = 0.0;
			scaled.x = 100.0 * ship->vel.x;
			scaled.y = 100.0 * ship->vel.y;
			ship->desiredHeading = (CalcInterceptAngle(zero, scaled) + kHalfCircle) % kFullCircle;
			if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1) {
				if (FloatAbs(ship->vel.x) >= 1.75 || FloatAbs(ship->vel.y) >= 1.75) {
					ship->thrust = ShipAccelRate(ship);
				} else {
					ship->thrust = 0.5 * ShipAccelRate(ship);
					ship->vel.x *= 0.94;
					ship->vel.y *= 0.94;
				}
			}
		} else {
			ship->vel.x *= kBrakeDamping;
			ship->vel.y *= kBrakeDamping;
			ship->aiSubstate = kManeuverLoJack;
		}
		AIEscortFireUnprovoked(ship);
	}
	if (ship->aiSubstate == kManeuverFlyStel && ship->targetStellar != -1 && !disabled) {
		ship->desiredHeading = CalcInterceptAngle(ship->pos, XYShortToFloat(stellarData[ship->targetStellar].pos));
		if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1) {
			range = 500;
			zero.x = ship->pos.x - stellarData[ship->targetStellar].pos.h;
			zero.y = ship->pos.y - stellarData[ship->targetStellar].pos.v;
			if (FloatAbs(zero.x) < range && FloatAbs(zero.y) < range) {
				ship->thrust = ShipAccelRate(ship);
				ship->speedLimit = 0.25 * ShipMaxSpeed(ship);
			} else {
				ship->thrust = ShipAccelRate(ship);
				ship->speedLimit = 0;
			}
		}
	}
	if (ship->aiSubstate == kManeuverFlyHypD && !disabled) {
		zero.x = zero.y = 0;
		ship->desiredHeading = CalcInterceptAngle(zero, ship->pos);
		if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1) {
			ship->thrust = ShipAccelRate(ship);
			ship->speedLimit = 0;
		}
	}
	if (ship->aiSubstate == kManeuverHypJmp && !disabled) {
		zero.x = zero.y = 0;
		ship->desiredHeading = CalcInterceptAngle(zero, ship->pos);
		if (ship->jumpTimer <= 0) {
			ship->jumpTimer = 1;
			ship->jumpTick = TickCount();
		}
		ship->jumpTimer++;
		if (TickCount() - ship->jumpTick >= 466.0f / gShipTypes[ship->type].jumpSpeedScale) {
			if (ship->specialMission != -1
					&& currentMissions[ship->specialMission].shipSyst != kShipSystFollow
					&& currentMissions[ship->specialMission].shipGoal == kMisnGoalChaseOff) {
				currentMissions[ship->specialMission].specialShipsJumpedOut++;
				if (currentMissions[ship->specialMission].shipCount > 0)
					currentMissions[ship->specialMission].shipCount--;
			}
			ship->jumpTimer = 0;
			ship->active = false;
			ship->curSystem = -1;
		}
	}
	if (ship->aiSubstate == kManeuverJmpPar && !disabled) {
		ship->targetStellar = shipData[ship->parentShip].targetStellar;
		ship->desiredHeading = shipData[ship->parentShip].heading;
		if (ship->jumpTimer == 0)
			ship->jumpTick = TickCount();
		ship->vel.x *= kBrakeDamping;
		ship->vel.y *= kBrakeDamping;
		ship->thrust = 0.0;
		ship->speedLimit = -1.0;
		ship->jumpTimer++;
	}
	if (ship->aiSubstate == kManeuverRunAwy && ship->targetShip != -1 && !disabled) {
		ship->desiredHeading = CalcInterceptAngle(shipData[ship->targetShip].pos, ship->pos);
		if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1) {
			ship->thrust = ShipAccelRate(ship);
			ship->speedLimit = 0;
		}
		if (ship->aiState == kStateDefRet)
			AIFireTurret(ship);
	}
	if (ship->aiSubstate == kManeuverTurnFire && ship->targetShip != -1 && !disabled) {
		if (ship->selectedWeapon != -1) {
			if (gWeapons[ship->selectedWeapon].guidance == -1)
				ship->desiredHeading = CalcLeadAngle(ship, &shipData[ship->targetShip], ship->selectedWeapon);
			else
				ship->desiredHeading = CalcInterceptAngle(ship->pos, shipData[ship->targetShip].pos);
		} else {
			ship->desiredHeading = CalcInterceptAngle(ship->pos, shipData[ship->targetShip].pos);
		}
		AIFireTurret(ship);
		if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1) {
			ship->thrust = ShipAccelRate(ship);
			ship->speedLimit = 0;
		}
		if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) * 3)
			AIFireGun(ship);
		if ((ship->targetShip != 0 && Rand(2) == 0) || AIPerformsComplexManeuvers(ship)) {
			n = kStrafeRange;
			if (gShipTypes[ship->type].inherentAI >= kAIWarship && gShipTypes[ship->type].mass < 200
					&& abs(ship->pos.x - shipData[ship->targetShip].pos.x) < n
					&& abs(ship->pos.y - shipData[ship->targetShip].pos.y) < n) {
				angle = CalcInterceptAngle(ship->pos, shipData[ship->targetShip].pos);
				if (AngularDifference(ship->heading, angle) <= 30
						&& AngularDifference(shipData[ship->targetShip].heading, (angle + kHalfCircle) % kFullCircle) <= 30) {
					ship->aiSubstate = kManeuverVeerOff;
					if (ship->index % 2 == 0)
						ship->veerHeading = ship->heading + 135;
					else
						ship->veerHeading = ship->heading - 135;
					if (ship->veerHeading < 0)
						ship->veerHeading += kFullCircle;
					if (ship->veerHeading >= kFullCircle)
						ship->veerHeading -= kFullCircle;
					if (ship->desiredHeading < 0)
						ship->desiredHeading += kFullCircle;
					if (ship->desiredHeading >= kFullCircle)
						ship->desiredHeading -= kFullCircle;
				}
			}
		}
		if (ship->hasAfterburner) {
			n = kAfterburnRange;
			if (abs(ship->pos.x - shipData[ship->targetShip].pos.x) > n
					&& abs(ship->pos.y - shipData[ship->targetShip].pos.y) > n) {
				angle = CalcInterceptAngle(ship->pos, shipData[ship->targetShip].pos);
				if (AngularDifference(ship->heading, angle) <= 30)
					ship->aiSubstate = kManeuverAfterBurn;
			}
		}
	}
	if (ship->aiSubstate == kManeuverVeerOff && ship->targetShip != -1 && !disabled) {
		ship->desiredHeading = ship->veerHeading;
		ship->thrust = 1.5 * ShipAccelRate(ship);
		ship->speedLimit = 0.0f;
		if (abs(ship->pos.x - shipData[ship->targetShip].pos.x) < 165
				&& abs(ship->pos.y - shipData[ship->targetShip].pos.y) < 165)
			AIFireTurret(ship);
		if (AngularDifference(ship->heading, ship->veerHeading) < ShipTurnRate(ship) * 3)
			ship->aiSubstate = kManeuverTurnFire;
	}
	if (ship->aiSubstate == kManeuverAfterBurn && ship->targetShip != -1 && !disabled) {
		ship->thrust = 2.75 * ShipAccelRate(ship);
		ship->speedLimit = 1.8 * ShipMaxSpeed(ship);
		ship->desiredHeading = CalcInterceptAngle(ship->pos, shipData[ship->targetShip].pos);
		if (abs(ship->pos.x - shipData[ship->targetShip].pos.x) < 165
				&& abs(ship->pos.y - shipData[ship->targetShip].pos.y) < 165) {
			AIFireTurret(ship);
			if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) * 3)
				AIFireGun(ship);
		} else {
			if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) * 3)
				AIFireMissile(ship);
		}
		if ((abs(ship->pos.x - shipData[ship->targetShip].pos.x) <= 165
				&& abs(ship->pos.y - shipData[ship->targetShip].pos.y) <= 165) || Rand(100) == 0)
			ship->aiSubstate = kManeuverTurnFire;
	}
	if (ship->aiSubstate == kManeuverMissle && ship->targetShip != -1 && !disabled) {
		ship->desiredHeading = CalcInterceptAngle(ship->pos, shipData[ship->targetShip].pos);
		if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1) {
			ship->thrust = ShipAccelRate(ship);
			ship->speedLimit = 0;
			AIFireMissile(ship);
		}
		if (ship->hasAfterburner) {
			n = kAfterburnRange;
			if (abs(ship->pos.x - shipData[ship->targetShip].pos.x) > n
					&& abs(ship->pos.y - shipData[ship->targetShip].pos.y) > n) {
				angle = CalcInterceptAngle(ship->pos, shipData[ship->targetShip].pos);
				if (AngularDifference(ship->heading, angle) <= 30)
					ship->aiSubstate = kManeuverAfterBurn;
			}
		}
	}
	if (ship->aiSubstate == kManeuverBoard && ship->targetStellar != -1 && !disabled) {
		delta.x = ship->vel.x - shipData[ship->targetStellar].vel.x;
		delta.y = ship->vel.y - shipData[ship->targetStellar].vel.y;
		if (FloatAbs(delta.x) >= kStillSpeed * 1.5 || FloatAbs(delta.y) >= kStillSpeed * 1.5) {
			zero.x = zero.y = 0.0;
			scaled.x = 100.0 * delta.x;
			scaled.y = 100.0 * delta.y;
			ship->desiredHeading = (CalcInterceptAngle(zero, scaled) + kHalfCircle) % kFullCircle;
			if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1) {
				ship->thrust = ShipAccelRate(ship);
				ship->speedLimit = 0;
			}
			if (FloatAbs(delta.x) <= 1.75 || FloatAbs(delta.y) <= 1.75) {
				delta.x *= kBrakeDamping;
				delta.y *= kBrakeDamping;
				ship->vel.x = delta.x + shipData[ship->targetStellar].vel.x;
				ship->vel.y = delta.y + shipData[ship->targetStellar].vel.y;
			}
		} else {
			ship->desiredHeading = shipData[ship->targetStellar].heading;
			i = 0.5 * GetSpriteXSize(shipFaces[shipData[ship->targetStellar].type][0]);
			zero.x = FloatAbs(ship->pos.x - shipData[ship->targetStellar].pos.x);
			zero.y = FloatAbs(ship->pos.y - shipData[ship->targetStellar].pos.y);
			if (zero.x <= 3.0f && zero.y <= 3.0f) {
				if (!shipData[ship->targetStellar].boarded) {
					shipData[ship->targetStellar].boarded = true;
					ship->aiTimer = Rand(150) + 300;
					ship->vel.x = shipData[ship->targetStellar].vel.x;
					ship->vel.y = shipData[ship->targetStellar].vel.y;
				} else if (ship->aiTimer <= 0) {
					ship->targetShip = -1;
					ship->targetStellar = -1;
					ship->aiState = kStateHiJack;
					ship->aiSubstate = kManeuverLoJack;
				}
				if (ship->aiTimer > 0 && ship->aiTimer <= 100) {
					Accel(ship->heading, ShipAccelRate(ship), &ship->vel);
				} else {
					ship->vel.x = shipData[ship->targetStellar].vel.x;
					ship->vel.y = shipData[ship->targetStellar].vel.y;
				}
			} else {
				if (ship->aiTimer > 0 && ship->aiTimer <= 100) {
					Accel(ship->heading, ShipAccelRate(ship), &ship->vel);
				} else {
					ship->vel.x = shipData[ship->targetStellar].vel.x;
					ship->vel.y = shipData[ship->targetStellar].vel.y;
					zero.x = ship->pos.x;
					zero.y = ship->pos.y;
					scaled.x = shipData[ship->targetStellar].pos.x;
					scaled.y = shipData[ship->targetStellar].pos.y;
					Accel(CalcInterceptAngle(zero, scaled), 0.1 * ShipMaxSpeed(ship), &ship->vel);
				}
			}
		}
	}
	if (ship->aiSubstate == kManeuverWaitTarg && ship->targetShip != -1 && !disabled) {
		if (FloatAbs(ship->vel.x) >= kStillSpeed || FloatAbs(ship->vel.y) >= kStillSpeed) {
			zero.x = zero.y = 0.0;
			scaled.x = 100.0 * ship->vel.x;
			scaled.y = 100.0 * ship->vel.y;
			ship->desiredHeading = (CalcInterceptAngle(zero, scaled) + kHalfCircle) % kFullCircle;
			if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1)
				ship->thrust = ShipAccelRate(ship);
		} else {
			ship->vel.x *= kBrakeDamping;
			ship->vel.y *= kBrakeDamping;
			ship->desiredHeading = CalcInterceptAngle(ship->pos, shipData[ship->targetShip].pos);
		}
		AIFireTurret(ship);
		AILaunchFighter(ship);
		AIFireMissile(ship);
	}
	if (ship->aiSubstate == kManeuverChasSlw && (ship->targetShip != -1 || ship->targetStellar != -1) && !disabled) {
		if (ship->targetShip >= 0)
			n = ship->targetShip;
		else if (ship->targetStellar >= 0)
			n = ship->targetStellar;
		dir = CalcInterceptAngle(ship->pos, shipData[n].pos);
		if (FloatAbs(ship->pos.x - shipData[n].pos.x) <= 360.0f
				&& FloatAbs(ship->pos.y - shipData[n].pos.y) <= 360.0f) {
			zero.x = zero.y = 0;
			scaled.x = 100.0 * ship->vel.x;
			scaled.y = 100.0 * ship->vel.y;
			angle = CalcInterceptAngle(zero, scaled);
			if (abs(angle - dir) > 15) {
				c.x = c.y = 0.0;
				Accel(dir, ShipMaxSpeed(ship), &c);
				delta.x = c.x - ship->vel.x;
				delta.y = c.y - ship->vel.y;
				if (FloatAbs(delta.x) > kStillSpeed || FloatAbs(delta.y) > kStillSpeed)
					ship->desiredHeading = CalcInterceptAngle(zero, delta);
			}
		} else {
			ship->desiredHeading = dir;
		}
		if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1) {
			ship->thrust = ShipAccelRate(ship);
			if (FloatAbs(ship->pos.x - shipData[n].pos.x) <= 180.0f
					&& FloatAbs(ship->pos.y - shipData[n].pos.y) <= 180.0f)
				ship->speedLimit = 0.5 * ShipMaxSpeed(ship);
			else
				ship->speedLimit = 0;
		}
		AIEscortFireUnprovoked(ship);
	}
	if (ship->aiSubstate == kManeuverChase && (ship->targetShip != -1 || ship->targetStellar != -1) && !disabled) {
		if (ship->targetShip >= 0)
			n = ship->targetShip;
		else if (ship->targetStellar >= 0)
			n = ship->targetStellar;
		dir = CalcInterceptAngle(ship->pos, shipData[n].pos);
		if (FloatAbs(ship->pos.x - shipData[n].pos.x) <= 360.0f
				&& FloatAbs(ship->pos.y - shipData[n].pos.y) <= 360.0f) {
			zero.x = zero.y = 0;
			scaled.x = 100.0 * ship->vel.x;
			scaled.y = 100.0 * ship->vel.y;
			angle = CalcInterceptAngle(zero, scaled);
			if (abs(angle - dir) > 15) {
				c.x = c.y = 0.0;
				Accel(dir, ShipMaxSpeed(ship), &c);
				delta.x = c.x - ship->vel.x;
				delta.y = c.y - ship->vel.y;
				if (FloatAbs(delta.x) > kStillSpeed || FloatAbs(delta.y) > kStillSpeed)
					ship->desiredHeading = CalcInterceptAngle(zero, delta);
			}
		} else {
			ship->desiredHeading = dir;
		}
		if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1) {
			ship->thrust = ShipAccelRate(ship);
			ship->speedLimit = 0;
		}
		AIEscortFireUnprovoked(ship);
	}
	if (ship->aiSubstate == kManeuverFormFly && ship->targetStellar != -1 && !disabled) {
		delta.x = ship->vel.x - shipData[ship->targetStellar].vel.x;
		delta.y = ship->vel.y - shipData[ship->targetStellar].vel.y;
		if (FloatAbs(delta.x) >= kStillSpeed * 1.5 || FloatAbs(delta.y) >= kStillSpeed * 1.5) {
			zero.x = zero.y = 0.0;
			scaled.x = 100.0 * delta.x;
			scaled.y = 100.0 * delta.y;
			ship->desiredHeading = (CalcInterceptAngle(zero, scaled) + kHalfCircle) % kFullCircle;
			if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1) {
				ship->thrust = 0.66 * ShipAccelRate(ship);
				ship->speedLimit = 0;
			}
			if (FloatAbs(delta.x) <= 1.75 || FloatAbs(delta.y) <= 1.75) {
				delta.x *= kBrakeDamping;
				delta.y *= kBrakeDamping;
				ship->vel.x = delta.x + shipData[ship->targetStellar].vel.x;
				ship->vel.y = delta.y + shipData[ship->targetStellar].vel.y;
			}
		} else {
			ship->vel.x = shipData[ship->targetStellar].vel.x;
			ship->vel.y = shipData[ship->targetStellar].vel.y;
			zero.x = zero.y = 0.0;
			scaled.x = 100.0 * ship->vel.x;
			scaled.y = 100.0 * ship->vel.y;
			ship->desiredHeading = CalcInterceptAngle(zero, scaled);
		}
		AIEscortFireUnprovoked(ship);
	}
	if (ship->aiSubstate == kManeuverDock) {
		if (ship->targetStellar != -1 && !IsDying(ship)) {
			if (shipData[ship->targetStellar].active
					&& ship->curSystem == shipData[ship->targetStellar].curSystem) {
				if (!disabled) {
					ship->desiredHeading = CalcInterceptAngle(ship->pos, shipData[ship->targetStellar].pos);
					if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1) {
						ship->thrust = ShipAccelRate(ship);
						ship->speedLimit = 0;
					}
				}
				n = (double)GetSpriteXSize(shipFaces[shipData[ship->targetStellar].type][shipData[ship->targetStellar].heading / 10]);
				if (FloatAbs(shipData[ship->targetStellar].pos.x - ship->pos.x) <= n
						&& FloatAbs(shipData[ship->targetStellar].pos.y - ship->pos.y) <= n) {
					if (ship->index != 0) {
						ok = 1;
					} else {
						if (CanBuyFighter(ship->type, -1, -1))
							ok = 1;
						else
							ok = 0;
					}
					if (ok != 1) {
						ship->aiState = kStateProtect;
						ship->aiSubstate = kManeuverLoJack;
						ship->targetShip = -1;
						ship->targetStellar = -1;
					} else {
						for (i = 0; i < MaxWeaponTypes; i++) {
							if (ship->type + kAmmoShipBase == gWeapons[i].ammoType && gWeapons[i].guidance == kGuidanceBay) {
								shipData[ship->targetStellar].weap[i].ammo++;
								if (ship->parentShip == 0)
									hudRedraw.ammo = true;
								ship->active = false;
								ship->parentShip = -1;
								ship->aiType = -1;
								break;
							}
						}
					}
				}
			} else {
				ship->aiType = kAIWarship;
				ship->aiState = kStateHiJack;
			}
		} else {
			ship->aiState = kStateHiJack;
			ship->aiSubstate = kManeuverLoJack;
		}
	}
	if (ship->aiSubstate == kManeuverZoom) {
		if (ship->speedLimit >= 0.0)
			ship->speedLimit = -50.0;
		ship->thrust = -kJumpInDecel;
	}
}

// FUNCTION: EVO 0x10005838
void SelectRandomStellarDest(ShipObj *ship)
{
	short i, count, pick;
	short uninhabited[4];
	short canLand[4];
	Boolean hostile[4];

	count = 0;
	for (i = 0; i <= 3; i++) {
		hostile[i] = false;
		canLand[i] = 0;
		uninhabited[i] = 0;
		if (gSystems[ship->curSystem].navDefault[i] != -1) {
			if (SpobCanLand(&stellarData[gSystems[ship->curSystem].navDefault[i]]))
				canLand[i] = 1;
			if (SpobUninhabited(&stellarData[gSystems[ship->curSystem].navDefault[i]]))
				uninhabited[i] = 1;
			if (ship->govt != -1 && stellarData[gSystems[ship->curSystem].navDefault[i]].govt != -1) {
				if (govtData[ship->govt].enemy == stellarData[gSystems[ship->curSystem].navDefault[i]].govt
						|| ship->govt == govtData[stellarData[gSystems[ship->curSystem].navDefault[i]].govt].enemy)
					hostile[i] = true;
			}
			if (canLand[i] && !uninhabited[i] && !hostile[i])
				count++;
		}
	}
	if (count > 0) {
		pick = 0;
		do {
			pick = Rand(4);
		} while (uninhabited[pick] || !canLand[pick] || hostile[pick]);
		ship->navTargetKind = kNavStellar;
		ship->aiState = kStateGoStel;
		ship->targetStellar = gSystems[ship->curSystem].navDefault[pick];
	}
}

// FUNCTION: EVO 0x10005B48
Boolean RecentlyVisitedStellarInSystem(ShipObj *ship, short syst)
{
	short i, count;

	count = 0;
	for (i = 0; i <= 3; i++) {
		if (gSystems[syst].navDefault[i] != -1)
			count++;
	}
	if (count == 0)
		return true;
	for (i = 0; i <= 3; i++) {
		if (ship->lastStellar == gSystems[syst].navDefault[i] && gSystems[syst].navDefault[i] != -1)
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x10005C38
void AIFireTurret(ShipObj *ship)
{
	short i, j, angle, diff, dmg, sel;
	Boolean ok;
	short bestSlot[2];
	short score[2];

	bestSlot[0] = bestSlot[1] = -1;
	score[0] = score[1] = 0;
	if (ship->targetShip != -1 && shipData[ship->targetShip].active) {
		sel = -1;
		for (i = 0; i < MaxWeaponTypes; i++) {
			if (ship->weap[i].count > 0) {
				if (gWeapons[i].guidance == kGuidanceTurretBeam || gWeapons[i].guidance == kGuidanceTurret
						|| gWeapons[i].guidance == kGuidanceFrontQuad || gWeapons[i].guidance == kGuidanceRearQuad) {
					angle = CalcInterceptAngle(ship->pos, shipData[ship->targetShip].pos);
					if (gWeapons[i].guidance == kGuidanceFrontQuad || gWeapons[i].guidance == kGuidanceRearQuad) {
						ok = false;
						if (gWeapons[i].guidance == kGuidanceFrontQuad)
							diff = abs(angle - ship->heading) % kFullCircle;
						if (gWeapons[i].guidance == kGuidanceRearQuad)
							diff = abs(angle - (ship->heading + kHalfCircle) % kFullCircle) % kFullCircle;
						if (diff <= 45)
							ok = true;
					} else {
						ok = true;
					}
					if (TurretBlindSpot(ship, &shipData[ship->targetShip], angle))
						ok = false;
					if (WeaponHasAmmo(ship, i) && ok) {
						for (j = 0; j <= 1; j++) {
							dmg = (&gWeapons[i].massDmg)[j];
							if (dmg <= 0)
								dmg = 1;
							if (dmg > score[j]) {
								score[j] = dmg;
								bestSlot[j] = i;
							}
						}
					}
				}
			}
		}
		if (shipData[ship->targetShip].shield >= 0)
			sel = bestSlot[1];
		else
			sel = bestSlot[0];
		if (sel != -1) {
			ship->selectedWeapon = sel;
			ship->fireQueued = true;
		}
	}
}

// FUNCTION: EVO 0x10005F70
void AIFireMissile(ShipObj *ship)
{
	short i, found;
	float dist, range;

	if (ship->targetShip == -1)
		return;
	if (ship->curSystem != shipData[ship->targetShip].curSystem || !shipData[ship->targetShip].active)
		return;
	if (!SufficentTargetedDamage(&shipData[ship->targetShip])) {
		found = -1;
		dist = FloatAbs(XYFloatSquaredDist(&ship->pos, &shipData[ship->targetShip].pos));
		for (i = 0; i < MaxWeaponTypes; i++) {
			if (ship->weap[i].count > 0 && (ship->weap[i].ammo > 0 || ship->weap[i].ammo == -1)
					&& gWeapons[i].guidance == kGuidanceHoming
					&& SuitableMissileType(i, &shipData[ship->targetShip])
					&& WeaponHasAmmo(ship, i)) {
				range = 1.5 * (gWeapons[i].speed * gWeapons[i].lifetime);
				if (range * range >= 0.95 * dist) {
					found = i;
					break;
				}
			}
		}
		if (found != -1) {
			ship->selectedWeapon = found;
			ship->fireQueued = true;
		} else {
			ship->fireQueued = false;
		}
	}
}

// FUNCTION: EVO 0x10006194
void AIFireGun(ShipObj *ship)
{
	short i, j, dmg, sel;
	short dist;
	short bestSlot[2];
	short score[2];

	bestSlot[0] = bestSlot[1] = -1;
	score[0] = score[1] = 0;
	if (ship->targetShip == -1)
		return;
	if (ship->curSystem != shipData[ship->targetShip].curSystem || !shipData[ship->targetShip].active)
		return;
	sel = -1;
	for (i = 0; i < MaxWeaponTypes; i++) {
		if (ship->weap[i].count > 0) {
			if (!((ship->weap[i].ammo > 0 && gWeapons[i].ammoType > kAmmoFuelBase)
					|| (ship->fuel >= abs(gWeapons[i].ammoType) + kAmmoFuelBase && gWeapons[i].ammoType <= kAmmoFuelBase)
					|| gWeapons[i].ammoType == -1))
				continue;
			if (gWeapons[i].guidance != kGuidanceUnguided && gWeapons[i].guidance != kGuidanceBeam
					&& gWeapons[i].guidance != kGuidanceRocket)
				continue;
			if (WeaponHasAmmo(ship, i) && ship->weap[i].reload <= 0) {
				for (j = 0; j <= 1; j++) {
					dmg = (&gWeapons[i].massDmg)[j];
					if (dmg <= 0)
						dmg = 1;
					if (gWeapons[i].guidance == kGuidanceRocket && gWeapons[i].proxRadius > 0) {
						dist = FloatAbs(XYFloatSquaredDist(&ship->pos, &shipData[ship->targetShip].pos));
						if (FloatAbs(ship->pos.x - shipData[ship->targetShip].pos.x) >= 2.5 * gWeapons[i].proxRadius
								&& FloatAbs(ship->pos.y - shipData[ship->targetShip].pos.y) >= 2.5 * gWeapons[i].proxRadius) {
							if (dmg > score[j]) {
								score[j] = dmg;
								bestSlot[j] = i;
							}
						}
					} else {
						if (dmg > score[j]) {
							score[j] = dmg;
							bestSlot[j] = i;
						}
					}
				}
			}
		}
	}
	if (shipData[ship->targetShip].shield >= 0)
		sel = bestSlot[1];
	else
		sel = bestSlot[0];
	if (sel >= 0)
		ship->selectedWeapon = sel;
	if (ship->selectedWeapon != -1)
		ship->fireQueued = true;
}

// FUNCTION: EVO 0x100065D8
void AILaunchFighter(ShipObj *ship)
{
	short i;

	if (ship->targetShip == -1)
		return;
	if (ship->curSystem != shipData[ship->targetShip].curSystem || !shipData[ship->targetShip].active)
		return;
	for (i = 0; i < MaxWeaponTypes; i++) {
		if (ship->weap[i].count > 0 && ship->weap[i].ammo > 0
				&& gWeapons[i].guidance == kGuidanceBay) {
			if (ship->weap[i].reload <= 0) {
				ship->selectedWeapon = i;
				if (LaunchFighter(ship, i)) {
					PlaySoundDistance(-1, weapSnd[gWeapons[ship->selectedWeapon].sound], 5, &ship->pos, &shipData->pos);
					ship->weap[i].reload = gWeapons[i].reload / ship->weap[i].count;
					ship->weap[i].ammo--;
				}
			}
			break;
		}
	}
}

// FUNCTION: EVO 0x1000679C
void DrawHighGoalAbbreviation(short state)
{
	if (state == kStateHiJack)
		DrawString("\pHiJack");
	if (state == kStateGoStel)
		DrawString("\pGoStel");
	if (state == kStateHypOut)
		DrawString("\pHypOut");
	if (state == kStateDefRet)
		DrawString("\pDefRet");
	if (state == kStateFightSh)
		DrawString("\pFightSh");
	if (state == kStateGoHome)
		DrawString("\pGoHome");
	if (state == kStateWait)
		DrawString("\pWait");
	if (state == kStateInspect)
		DrawString("\pInspect");
	if (state == kStateJumpIn)
		DrawString("\pJumpIn");
	if (state == kStateRefuel)
		DrawString("\pRefuel");
	if (state == kStateFlyPr)
		DrawString("\pFlyPr");
	if (state == kStateHypPr)
		DrawString("\pHypPr");
	if (state == kStateProtect)
		DrawString("\pProtect");
	if (state == kStatePlunder)
		DrawString("\pPlunder");
}

// FUNCTION: EVO 0x10006910
void DrawLowGoalAbbreviation(short maneuver)
{
	if (maneuver == kManeuverLoJack)
		DrawString("\pLoJack");
	if (maneuver == kManeuverKillSpd)
		DrawString("\pKillSpd");
	if (maneuver == kManeuverFlyStel)
		DrawString("\pFlyStel");
	if (maneuver == kManeuverFlyHypD)
		DrawString("\pFlyHypD");
	if (maneuver == kManeuverHypJmp)
		DrawString("\pHypJmp");
	if (maneuver == kManeuverRunAwy)
		DrawString("\pRunAwy");
	if (maneuver == kManeuverTurnFire)
		DrawString("\pT+F");
	if (maneuver == kManeuverMissle)
		DrawString("\pMissle");
	if (maneuver == kManeuverDock)
		DrawString("\pDock");
	if (maneuver == kManeuverChase)
		DrawString("\pChase");
	if (maneuver == kManeuverZoom)
		DrawString("\pZoom");
	if (maneuver == kManeuverChasSlw)
		DrawString("\pChasSlw");
	if (maneuver == kManeuverFormFly)
		DrawString("\pFormFly");
	if (maneuver == kManeuverJmpPar)
		DrawString("\pJmpPar");
	if (maneuver == kManeuverWaitTarg)
		DrawString("\pWaitTarg");
	if (maneuver == kManeuverBoard)
		DrawString("\pBoard");
	if (maneuver == kManeuverVeerOff)
		DrawString("\pVeerOff");
	if (maneuver == kManeuverAfterBurn)
		DrawString("\pAfterBurn");
}

// FUNCTION: EVO 0x10006AE4
void SelectWarshipTarget(ShipObj *ship)
{
	short i, j, k, n, behav;
	long cands[MaxShipsInSystem];
	Boolean found, neverAttacks, inRange, nosy;
	float dx, dy;

	if (ship->targetShip != -1 && shipData[ship->targetShip].active)
		return;
	if (ship->persType != -1) {
		if (ship->persType == kPersBountyHunter) {
			AIMakeShipAttackPlayer(ship);
			return;
		}
		if ((personData[ship->persType].flags & kPersGrudge) && personData[ship->persType].grudge) {
			AIMakeShipAttackPlayer(ship);
			return;
		}
	}
	if (ship->specialMission != -1 && currentMissionObjectives[ship->specialMission].active) {
		behav = currentMissions[ship->specialMission].shipBehav;
		if ((behav == kMisnBehavAttack || behav == kMisnBehavAttack + kMisnBehavVariant) && !gInvertedColors) {
			AIMakeShipAttackPlayer(ship);
			return;
		}
		if (behav == kMisnBehavProtect || behav == kMisnBehavProtect + kMisnBehavVariant) {
			if (ship->targetShip == 0)
				ship->targetShip = -1;
			if (ship->targetShip == -1) {
				AIFightThreatToPlayer(ship);
				if (ship->targetShip == -1) {
					ship->aiState = kStateProtect;
					ship->targetStellar = 0;
				}
			}
			return;
		}
	}
	if (ship->govt != -1) {
		if (govtData[ship->govt].flags & kGovtXenophobic) {
			n = 0;
			for (i = 0; i < MaxShipsInSystem; i++) {
				if (!IsDying(&shipData[i]) && shipData[i].type != kEscapePodShip) {
					if (i == 0) {
						neverAttacks = govtData[ship->govt].flags & kGovtNeverAttacks;
						if (!gInvertedColors && !neverAttacks) {
							if (ship->govt == gSystems[shipData[0].curSystem].govt) {
								if (legalStatus[shipData[0].curSystem] <= 0)
									n++;
							} else {
								n++;
							}
						}
					} else if (i != ship->index && !IsDisabled(&shipData[i]) && shipData[i].active
							&& ship->index != shipData[i].parentShip
							&& ship->curSystem == shipData[i].curSystem
							&& shipData[i].defenseStellar == -1 && shipData[i].persType != kPersAmbrosiaMascot
							&& shipData[i].type != kEscapePodShip && IsShipMyEnemy(ship, &shipData[i])) {
						n++;
					}
				}
			}
			if (n > 0) {
				while (ship->targetShip == -1) {
					k = Rand(MaxShipsInSystem);
					if (k != ship->index && shipData[k].active && !IsDisabled(&shipData[k])
							&& ship->index != shipData[k].parentShip
							&& ship->curSystem == shipData[k].curSystem
							&& shipData[k].defenseStellar == -1 && shipData[k].persType != kPersAmbrosiaMascot
							&& shipData[k].type != kEscapePodShip) {
						found = false;
						if (!IsDying(&shipData[k])) {
							if (k == 0) {
								neverAttacks = govtData[ship->govt].flags & kGovtNeverAttacks;
								if (!gInvertedColors && !neverAttacks) {
									if (ship->govt == gSystems[shipData[0].curSystem].govt) {
										if (legalStatus[shipData[0].curSystem] <= 0)
											found = true;
									} else {
										found = true;
									}
								}
							} else {
								found = IsShipMyEnemy(ship, &shipData[k]);
							}
						}
						if (found) {
							ship->targetShip = k;
							if (k == 0)
								AIMakeShipAttackPlayer(ship);
							return;
						}
					}
				}
			}
		} else {
			if (ship->targetShip == -1) {
				inRange = false;
				nosy = govtData[ship->govt].flags & kGovtNosy;
				dx = FloatAbs(ship->pos.x - shipData[0].pos.x);
				dy = FloatAbs(ship->pos.y - shipData[0].pos.y);
				n = ship->aggression * 600;
				if (dx <= n && dy <= n && !gInvertedColors && !IsDying(&shipData[0]))
					inRange = true;
				neverAttacks = govtData[ship->govt].flags & kGovtNeverAttacks;
				if (inRange && !neverAttacks) {
					if (ship->govt == gSystems[shipData[0].curSystem].govt) {
						if (legalStatus[shipData[0].curSystem] < -govtData[ship->govt].crimeTol) {
							AIMakeShipAttackPlayer(ship);
							return;
						}
					} else if (gSystems[shipData[0].curSystem].govt >= 0) {
						if (govtData[ship->govt].enemy == gSystems[shipData[0].curSystem].govt
								|| ship->govt == govtData[gSystems[shipData[0].curSystem].govt].enemy
								|| (govtData[gSystems[shipData[0].curSystem].govt].flags & kGovtXenophobic)) {
							if (legalStatus[shipData[0].curSystem] > govtData[ship->govt].crimeTol) {
								AIMakeShipAttackPlayer(ship);
								return;
							}
						} else if (govtData[ship->govt].ally == gSystems[shipData[0].curSystem].govt
								|| ship->govt == govtData[gSystems[shipData[0].curSystem].govt].ally) {
							if (legalStatus[shipData[0].curSystem] < 1.5 * -govtData[ship->govt].crimeTol) {
								AIMakeShipAttackPlayer(ship);
								return;
							}
						} else if (nosy && legalStatus[shipData[0].curSystem] < -govtData[ship->govt].crimeTol * 2) {
							AIMakeShipAttackPlayer(ship);
							return;
						}
					} else if (nosy && legalStatus[shipData[0].curSystem] < -govtData[ship->govt].crimeTol * 2) {
						AIMakeShipAttackPlayer(ship);
						return;
					}
				}
				if (ship->targetShip == -1 && ship->parentShip != 0 && ship->govt != -1
						&& ship->specialMission == -1 && ship->auxMission == -1
						&& gShipTypes[shipData[0].type].inherentGovt != -1
						&& (govtData[ship->govt].enemy == gShipTypes[shipData[0].type].inherentGovt
							|| ship->govt == govtData[gShipTypes[shipData[0].type].inherentGovt].enemy)) {
					if (Rand(50) == 0 && !IsDying(&shipData[0])) {
						AIMakeShipAttackPlayer(ship);
						return;
					}
				}
			}
		}
		for (i = 1; i < MaxShipsInSystem; i++) {
			if (i != ship->index && shipData[i].active && !IsDisabled(&shipData[i])
					&& IsShipMyEnemy(ship, &shipData[i])) {
				ship->targetShip = i;
				return;
			}
		}
	}
	if (ship->aiType == kAIEscort) {
		k = 0;
		n = ship->aggression * 600;
		for (i = 1; i < MaxShipsInSystem; i++) {
			if (shipData[i].active && i != ship->index && i != shipData[ship->parentShip].index
					&& IsThreatToPlayer(&shipData[i]) && ship->curSystem == shipData[i].curSystem)
				k++;
		}
		if (k > 0) {
			do {
				j = Rand(MaxShipsInSystem);
			} while (!shipData[j].active || j == ship->index
					|| j == shipData[ship->parentShip].index
					|| !IsThreatToPlayer(&shipData[j])
					|| ship->curSystem != shipData[j].curSystem);
			ship->targetShip = j;
			ship->aiState = kStateFightSh;
		}
	}
}

// FUNCTION: EVO 0x1000778C
Boolean IsThreatToPlayer(ShipObj *ship)
{
	if (ship->active && ship->aiTimer <= 0 && !IsDisabled(ship) && ship->targetShip == 0
			&& ship->aiState != kStateInspect && ship->aiState != kStateRefuel && ship->aiState != kStateFlyPr
			&& ship->aiState != kStateHypPr && ship->aiState != kStateGoHome && ship->aiState != kStateProtect)
		return true;
	return false;
}

// FUNCTION: EVO 0x10007840
Boolean ExtendedIsThreatToPlayer(ShipObj *ship)
{
	short i;

	if (ship->active && !IsDisabled(ship) && ship->parentShip != 0 && ship->targetShip != -1) {
		if (ship->defenseStellar != -1)
			return true;
		if (ship->targetShip == 0 && ship->aiTimer <= 0 && InEngagedState(ship))
			return true;
		if (shipData[ship->targetShip].parentShip == 0 && ship->aiTimer <= 0 && InEngagedState(ship))
			return true;
		if (ship->targetShip != -1 && ship->aiTimer <= 0 && InEngagedState(ship)
				&& shipData[ship->targetShip].parentShip == 0)
			return true;
		for (i = 1; i < MaxShipsInSystem; i++) {
			if (ship->index == shipData[i].parentShip && shipData[i].aiTimer <= 0
					&& shipData[i].active && !IsDisabled(&shipData[i])) {
				if (shipData[i].targetShip == 0 && InEngagedState(ship))
					return true;
				if (shipData[i].targetShip != -1 && InEngagedState(ship)
						&& shipData[shipData[i].targetShip].parentShip == 0)
					return true;
			}
		}
	}
	return false;
}

// FUNCTION: EVO 0x10007BC4
Boolean ExtendedIsThreatToShip(ShipObj *observer, ShipObj *cand)
{
	short i;

	if (cand->active && !IsDisabled(cand) && cand->parentShip != observer->index) {
		if (cand->targetShip == observer->index && cand->aiState != kStateInspect && cand->aiState != kStateRefuel
				&& cand->aiState != kStateFlyPr && cand->aiState != kStateHypPr && cand->aiState != kStateGoHome
				&& cand->aiState != kStateProtect)
			return true;
		for (i = 1; i < MaxShipsInSystem; i++) {
			if (cand->index == shipData[i].parentShip && shipData[i].active && cand->targetShip == i
					&& i != cand->index && cand->aiState != kStateInspect && cand->aiState != kStateRefuel
					&& cand->aiState != kStateFlyPr && cand->aiState != kStateHypPr && cand->aiState != kStateGoHome
					&& cand->aiState != kStateProtect)
				return true;
		}
	}
	return false;
}

// FUNCTION: EVO 0x10007D64
Boolean IsShipThreatened(ShipObj *ship)
{
	short i;

	if (ship->active && !IsDisabled(ship)) {
		for (i = 1; i < MaxShipsInSystem; i++) {
			if (shipData[i].active && ship->targetShip == ship->index && i != ship->index
					&& (short)shipData[i].aiState != kStateInspect && (short)shipData[i].aiState != kStateRefuel
					&& (short)shipData[i].aiState != kStateFlyPr && (short)shipData[i].aiState != kStateHypPr
					&& (short)shipData[i].aiState != kStateGoHome && (short)shipData[i].aiState != kStateProtect)
				return true;
		}
	}
	return false;
}

// FUNCTION: EVO 0x10007EC8
Boolean AIDoesShipLikePlayer(ShipObj *ship)
{
	Boolean xenophobic, nosy;

	if (ExtendedIsThreatToPlayer(ship))
		return false;
	if (ship->parentShip == 0)
		return true;
	if (ship->specialMission != -1) {
		if (currentMissionObjectives[ship->specialMission].active) {
			if (currentMissions[ship->specialMission].shipGoal != kMisnGoalEscort
					&& currentMissions[ship->specialMission].shipGoal != kMisnGoalObserve)
				return false;
			if (currentMissions[ship->specialMission].shipBehav != -1) {
				if (currentMissions[ship->specialMission].shipBehav == kMisnBehavProtect
						|| currentMissions[ship->specialMission].shipBehav == kMisnBehavProtect + kMisnBehavVariant)
					return true;
				if (currentMissions[ship->specialMission].shipBehav == kMisnBehavAttack
						|| currentMissions[ship->specialMission].shipBehav == kMisnBehavAttack + kMisnBehavVariant)
					return false;
			}
		}
	} else if (ship->govt != -1) {
		xenophobic = govtData[ship->govt].flags & kGovtXenophobic;
		nosy = govtData[ship->govt].flags & kGovtNosy;
		if (xenophobic) {
			if (ship->govt == gSystems[shipData->curSystem].govt) {
				if (legalStatus[shipData->curSystem] > govtData[gSystems[shipData->curSystem].govt].crimeTol)
					return true;
			} else if (gSystems[shipData->curSystem].govt >= 0) {
				if (legalStatus[shipData->curSystem] > govtData[gSystems[shipData->curSystem].govt].crimeTol)
					return false;
			} else {
				if (legalStatus[shipData->curSystem] > govtData->crimeTol)
					return false;
			}
		}
		if (gSystems[shipData->curSystem].govt >= 0) {
			if (ship->govt == govtData[gSystems[shipData->curSystem].govt].ally
					|| govtData[ship->govt].ally == gSystems[shipData->curSystem].govt) {
				if (legalStatus[shipData->curSystem] >= -govtData[gSystems[shipData->curSystem].govt].crimeTol)
					return true;
			} else if (ship->govt == govtData[gSystems[shipData->curSystem].govt].enemy
					|| govtData[ship->govt].enemy == gSystems[shipData->curSystem].govt) {
				if (legalStatus[shipData->curSystem] < -govtData[gSystems[shipData->curSystem].govt].crimeTol)
					return true;
			} else if (nosy) {
				if (legalStatus[shipData->curSystem] >= -govtData[gSystems[shipData->curSystem].govt].crimeTol)
					return true;
			} else {
				return true;
			}
		} else if (nosy) {
			if (legalStatus[shipData->curSystem] >= -govtData[ship->govt].crimeTol)
				return true;
		} else {
			return true;
		}
	} else {
		return true;
	}
	return false;
}

// FUNCTION: EVO 0x100083C0
Boolean IsPlayerThreatened(void)
{
	short i;

	for (i = 1; i < MaxShipsInSystem; i++) {
		if (ExtendedIsThreatToPlayer(&shipData[i]))
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x10008430
Boolean IsPlayerThreatenedByMyEnemy(ShipObj *ship)
{
	short i;

	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && i != ship->index && ExtendedIsThreatToPlayer(&shipData[i])
				&& IsShipMyEnemy(ship, &shipData[i]))
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x100084FC
Boolean IsShipMyEnemy(ShipObj *a, ShipObj *b)
{
	short i, j, k;
	Boolean hostile[MaxGovts];

	if (a->index == b->index)
		return false;
	if (!a->active || !b->active)
		return false;
	if (b->persType == kPersAmbrosiaMascot)
		return false;
	if (a->govt != -1 && b->govt != -1) {
		if (a->govt == b->govt)
			return false;
		if (b->govt == govtData[a->govt].enemy || a->govt == govtData[b->govt].enemy
				|| (GovtXenophobic(&govtData[b->govt]) && b->govt != govtData[a->govt].ally
					&& a->govt != govtData[b->govt].ally))
			return true;
		for (i = 0; i < MaxGovts; i++) {
			hostile[i] = false;
			if (a->govt == govtData[i].ally || i == govtData[a->govt].ally) {
				for (j = 0; j < MaxGovts; j++) {
					if (j == govtData[i].enemy)
						hostile[j] = true;
				}
			}
		}
		if (hostile[b->govt])
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x1000870C
void DoGoodSamaritan(ShipObj *ship, ShipObj *attacker)
{
	short i;
	Boolean respond, ally, nosy, xeno;

	if (ship->persType == kPersAmbrosiaMascot || attacker->persType == kPersBountyHunter)
		return;
	if (ship->govt != -1 && (govtData[ship->govt].flags & kGovtStartDisabled))
		return;
	respond = false;
	for (i = 1; i < MaxShipsInSystem; i++) {
		if ((shipData[i].aiType == kAIWarship || shipData[i].aiType == kAIInterceptor)
				&& shipData[i].aiState != kStateFightSh) {
			if (shipData[i].persType != kPersAmbrosiaMascot)
				respond = true;
			else
				respond = false;
			if (IsShipMyEnemy(&shipData[i], ship))
				return;
			if (shipData[i].govt != -1 && ship->govt != -1) {
				if (ship->govt == govtData[shipData[i].govt].enemy)
					respond = false;
				if (govtData[ship->govt].enemy == shipData[i].govt)
					respond = false;
				xeno = govtData[ship->govt].flags & kGovtXenophobic;
				if (xeno && ship->govt != shipData[i].govt)
					respond = false;
				nosy = govtData[shipData[i].govt].flags & kGovtNosy;
				ally = false;
				if (ship->govt == govtData[shipData[i].govt].ally && govtData[shipData[i].govt].ally != -1)
					ally = true;
				if (govtData[ship->govt].ally == shipData[i].govt && govtData[ship->govt].ally != -1)
					ally = true;
				if (ship->govt == shipData[i].govt)
					ally = true;
				if (!nosy && !ally)
					respond = false;
				if (attacker->govt == govtData[shipData[i].govt].ally && govtData[shipData[i].govt].ally != -1)
					respond = false;
				if (attacker->govt != -1 && govtData[attacker->govt].ally == shipData[i].govt
						&& govtData[attacker->govt].ally != -1)
					respond = false;
			}
			if (shipData[i].govt != -1 && ship->govt == -1) {
				nosy = govtData[shipData[i].govt].flags & kGovtNosy;
				if (!nosy)
					respond = false;
			}
			if (ship->govt != -1) {
				if (govtData[ship->govt].flags & kGovtStartDisabled)
					respond = false;
				if (govtData[ship->govt].flags & kGovtNoSamaritan)
					respond = false;
			}
			if (shipData[i].govt == -1 && ship->govt != -1) {
				xeno = govtData[ship->govt].flags & kGovtXenophobic;
				if (xeno)
					respond = false;
			}
			if (respond) {
				shipData[i].aiState = kStateFightSh;
				shipData[i].targetShip = attacker->index;
			}
		}
	}
}

// FUNCTION: EVO 0x10008C14
void AIMakeShipLeave(ShipObj *ship)
{
	ship->aiState = kStateHypOut;
	ship->jumpTimer = 1;
	ship->targetShip = -1;
	ship->jumpTick = TickCount();
}

// FUNCTION: EVO 0x10008C60
void AIMakeShipJumpWithParent(ShipObj *ship)
{
	ship->aiState = kStateHypPr;
	ship->grudge = 0;
	ship->targetShip = -1;
	if (ship->jumpTimer <= 0) {
		ship->jumpTimer = 1;
		ship->jumpTick = TickCount();
	}
}

// FUNCTION: EVO 0x10008CC0
void AIMakeShipAttackPlayer(ShipObj *ship)
{
	if (!ExtendedIsThreatToPlayer(ship) && ship->persType != -1 && ship->specialMission == -1
			&& (personData[ship->persType].flags & kPersQuoteOnAttack)) {
		speakerShip = ship->index;
		commCount = -1;
		ShowPersonHailQuote(personData[ship->persType].hailQuote);
		speakerShip = -1;
	}
	ship->aiState = kStateFightSh;
	ship->targetStellar = -1;
	ship->targetShip = 0;
}

// FUNCTION: EVO 0x10008D8C
void AIMakeShipAttackShip(ShipObj *ship, ShipObj *target)
{
	ship->aiState = kStateFightSh;
	ship->targetShip = target->index;
}

// FUNCTION: EVO 0x10008DA0
void AIMakeShipWait(ShipObj *ship)
{
	ship->aiState = kStateWait;
	ship->targetShip = -1;
	ship->targetStellar = -1;
}

// FUNCTION: EVO 0x10008DBC
void AIFightThreatToPlayer(ShipObj *ship)
{
	short i, count;

	count = 0;
	for (i = 1; i < MaxShipsInSystem; i++) {
		if (i != ship->index && i != 0 && !IsDisabled(&shipData[i]) && !IsDying(&shipData[i])
				&& ship->curSystem == shipData[i].curSystem && IsThreatToPlayer(&shipData[i]))
			count++;
	}
	if (count > 0) {
		for (;;) {
			i = Rand(MaxShipsInSystem - 1) + 1;
			if (i == ship->index || i == 0 || IsDisabled(&shipData[i]) || IsDying(&shipData[i])
					|| ship->curSystem != shipData[i].curSystem || !IsThreatToPlayer(&shipData[i]))
				continue;
			ship->targetStellar = -1;
			ship->targetShip = i;
			ship->aiState = kStateFightSh;
			break;
		}
	} else {
		ship->targetShip = -1;
	}
}

// FUNCTION: EVO 0x10008F80
void AIFightThreatToParent(ShipObj *ship)
{
	short i, count;

	count = 0;
	for (i = 0; i < MaxShipsInSystem; i++) {
		if (i != ship->index && i != ship->parentShip && !IsDisabled(&shipData[i])
				&& ship->curSystem == shipData[i].curSystem) {
			if (i != 0) {
				if (ship->parentShip == 0) {
					if (ExtendedIsThreatToPlayer(&shipData[i]))
						count++;
				} else if (ExtendedIsThreatToShip(&shipData[ship->parentShip], &shipData[i])) {
					count++;
				}
			} else if (ExtendedIsThreatToPlayer(&shipData[ship->parentShip])) {
				count++;
			}
		}
	}
	if (count > 0) {
		for (;;) {
			i = Rand(MaxShipsInSystem);
			if (i == ship->index || i == ship->parentShip || IsDisabled(&shipData[i])
					|| ship->curSystem != shipData[i].curSystem)
				continue;
			if (i != 0) {
				if (ship->parentShip == 0) {
					if (!ExtendedIsThreatToPlayer(&shipData[i]))
						continue;
					ship->targetStellar = -1;
					ship->targetShip = i;
					ship->aiState = kStateFightSh;
					break;
				} else {
					if (!ExtendedIsThreatToShip(&shipData[ship->parentShip], &shipData[i]))
						continue;
					ship->targetStellar = -1;
					ship->targetShip = i;
					ship->aiState = kStateFightSh;
					break;
				}
			} else {
				if (!ExtendedIsThreatToPlayer(&shipData[ship->parentShip]))
					continue;
				ship->targetStellar = -1;
				ship->targetShip = 0;
				ship->aiState = kStateFightSh;
				break;
			}
		}
	} else {
		ship->targetShip = -1;
	}
}

// FUNCTION: EVO 0x10009210
void AIFightEnemyThreatToPlayer(ShipObj *ship)
{
	short i, count;

	count = 0;
	for (i = 1; i < MaxShipsInSystem; i++) {
		if (i != ship->index && i != 0 && !IsDisabled(&shipData[i]) && !IsDying(&shipData[i])
				&& shipData[i].targetShip == 0 && ((short)shipData[i].aiState == kStateFightSh || (short)shipData[i].aiState == kStateDefRet)
				&& ship->curSystem == shipData[i].curSystem)
			count++;
	}
	if (count > 0) {
		for (;;) {
			i = Rand(MaxShipsInSystem - 1) + 1;
			if (i == ship->index || i == 0 || IsDisabled(&shipData[i]) || IsDying(&shipData[i])
					|| shipData[i].targetShip != 0 || ((short)shipData[i].aiState != kStateFightSh && (short)shipData[i].aiState != kStateDefRet)
					|| ship->curSystem != shipData[i].curSystem)
				continue;
			ship->targetShip = i;
			ship->aiState = kStateFightSh;
			break;
		}
	} else {
		ship->targetShip = -1;
	}
}

// FUNCTION: EVO 0x1000943C
void AIMakeShipRefuelPlayer(ShipObj *ship)
{
	ship->grudge = 0;
	ship->jumpTimer = 0;
	ship->targetShip = 0;
	ship->aiState = kStateRefuel;
	ship->aiSubstate = kManeuverLoJack;
	ship->aiTimer = -1;
}

// FUNCTION: EVO 0x10009470
void AIMakeFighterReturn(ShipObj *ship)
{
	ship->targetShip = -1;
	ship->targetStellar = ship->parentShip;
	ship->aiState = kStateGoHome;
}

// FUNCTION: EVO 0x1000948C
void AIMakeEscortFlyInForm(ShipObj *ship)
{
	short i;

	ship->targetShip = -1;
	ship->targetStellar = ship->parentShip;
	if (ship->aiType == kAIFighter)
		ship->aiState = kStateFlyPr;
	else if (ship->parentShip == 0)
		ship->aiState = kStateProtect;
	else
		ship->aiState = kStateFlyPr;
	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && i != ship->index && ship->index == shipData[i].parentShip
				&& shipData[i].aiType == kAIFighter)
			AIMakeFighterReturn(&shipData[i]);
	}
}

// FUNCTION: EVO 0x100095A4
void AIMakeShipStopJumping(ShipObj *ship)
{
	if (ship->aiState != kStateRefuel) {
		ship->aiState = kStateHiJack;
		ship->aiSubstate = kManeuverLoJack;
	}
	ship->targetStellar = -1;
	ship->jumpTimer = 0;
	ship->thrust = 0.0;
	ship->speedLimit = 0.0;
}

// FUNCTION: EVO 0x100095E4
void AIMakeShipJumpIn(ShipObj *ship)
{
	ship->aiState = kStateJumpIn;
	ship->jumpTimer = kJumpWindupArriving;
}

// FUNCTION: EVO 0x100095F8
Boolean AIIsShipJumping(ShipObj *ship)
{
	if (ship->aiState == kStateHypOut || ship->aiState == kStateHypPr || ship->aiState == kStateDefRet) {
		if (ship->aiSubstate == kManeuverHypJmp || ship->aiSubstate == kManeuverJmpPar)
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x10009644
Boolean IsShipDocking(ShipObj *ship)
{
	if (ship->aiState == kStateGoHome)
		return true;
	return false;
}

// FUNCTION: EVO 0x10009660
Boolean IsShipInForm(ShipObj *ship)
{
	if (ship->aiState == kStateFlyPr)
		return true;
	return false;
}

// FUNCTION: EVO 0x1000967C
Boolean IsShipAttacking(ShipObj *ship)
{
	if (ship->aiState == kStateFightSh)
		return true;
	return false;
}

// FUNCTION: EVO 0x10009698
Boolean IsShipWaiting(ShipObj *ship)
{
	if (ship->aiState == kStateWait)
		return true;
	return false;
}

// FUNCTION: EVO 0x100096B4
Boolean AIEvalHopelessChase(ShipObj *ship)
{
	short i, found, headingVel, headingPos;
	float dist, range;
	FloatPoint unused;
	FloatPoint origin;
	FloatPoint delta;

	if (ship->targetShip == -1)
		return false;
	if (ship->curSystem != shipData[ship->targetShip].curSystem || !shipData[ship->targetShip].active)
		return false;
	if (shipData[ship->targetShip].index != 0 && shipData[ship->targetShip].aiState != kStateDefRet)
		return false;
	if (gShipTypes[ship->type].mass < 100)
		return false;
	origin.x = origin.y = 0;
	delta.x = shipData[ship->targetShip].vel.x - ship->vel.x;
	delta.y = shipData[ship->targetShip].vel.y - ship->vel.y;
	if (FloatAbs(delta.x) <= kStillSpeed && FloatAbs(delta.y) <= kStillSpeed)
		return false;
	headingVel = CalcInterceptAngle(origin, delta);
	headingPos = CalcInterceptAngle(shipData[ship->targetShip].pos, ship->pos);
	if (abs(headingVel - headingPos) < 90)
		return false;
	found = -1;
	dist = FloatAbs(XYFloatSquaredDist(&ship->pos, &shipData[ship->targetShip].pos));
	for (i = 0; i < MaxWeaponTypes; i++) {
		if (ship->weap[i].count > 0 && (ship->weap[i].ammo > 0 || ship->weap[i].ammo == -1)
				&& gWeapons[i].guidance == kGuidanceHoming
				&& SuitableMissileType(i, &shipData[ship->targetShip])
				&& WeaponHasAmmo(ship, i)) {
			range = 1.5 * (gWeapons[i].speed * gWeapons[i].lifetime);
			if (range * range >= 0.8 * dist) {
				found = i;
				break;
			}
		}
	}
	if (found == -1) {
		if (gShipTypes[ship->type].maxSpeed > gShipTypes[shipData[ship->targetShip].type].maxSpeed)
			return false;
		return true;
	} else {
		if (gShipTypes[ship->type].maxSpeed >= gShipTypes[shipData[ship->targetShip].type].maxSpeed)
			return false;
		return true;
	}
}

// FUNCTION: EVO 0x10009AB0
Boolean AIIsShipBusy(ShipObj *ship)
{
	if (ship->aiState != kStateHiJack && ship->aiState != kStateHypOut && ship->aiState != kStateGoStel && ship->aiState != kStateInspect)
		return true;
	return false;
}

// FUNCTION: EVO 0x10009AF0
Boolean AIIsShipLeaving(ShipObj *ship)
{
	if (ship->aiState == kStateHypOut)
		return true;
	return false;
}

// FUNCTION: EVO 0x10009B0C
void AICallForHelp(ShipObj *ship)
{
	char text2[256];
	char text1[256];

	if (ship->persType == -1 && commCount <= 0 && AIDoesShipLikePlayer(ship)
			&& !ExtendedIsThreatToShip(ship, shipData) && !IsDying(shipData)) {
		PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
		strncpy(text1, (char *)gShipTypes[ship->type].name, 63);
		if (ship->specialMission != -1) {
			if (currentMissions[ship->specialMission].shipName[0] > 0)
				strncpy(text1, currentMissions[ship->specialMission].shipName, 63);
		} else if (ship->persType != -1) {
			strncpy(text1, (char *)personData[ship->persType].name, 63);
		} else {
			strncpy(text1, (char *)gShipTypes[ship->type].name, 63);
		}
		p2cstr((StringPtr)text1);
		strcpy(text2, text1);
		strcat(text2, ": ");
		strcpy(text1, (char *)shipHelpRequests[Rand(20)]);
		p2cstr((StringPtr)text1);
		strcat(text2, text1);
		c2pstr(text2);
		DisplayComm((StringPtr)text2, kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
	}
}

// FUNCTION: EVO 0x10009D2C
void AIEscortFireUnprovoked(ShipObj *ship)
{
	if (ship->parentShip != 0 || ship->aiType != kAIEscort)
		return;
	if (ship->targetShip != -1) {
		if (shipData[ship->targetShip].active)
			AIFireTurret(ship);
		else
			ship->targetShip = -1;
	}
}

// FUNCTION: EVO 0x10009DB0
Boolean AIIsShipAttemptingDisable(ShipObj *ship)
{
	if (ship->aiState == kStatePlunder)
		return true;
	return false;
}

// FUNCTION: EVO 0x10009DCC
void AIPlunderIfPossible(ShipObj *ship)
{
	if (ship->targetShip <= 0 || ship->govt == -1)
		return;
	if ((govtData[ship->govt].flags & kGovtPlunders) && shipData[ship->targetShip].aiType < kAIWarship
			&& shipData[ship->targetShip].specialMission == -1 && !shipData[ship->targetShip].boarded)
		ship->aiState = kStatePlunder;
}

// FUNCTION: EVO 0x10009E74
void AIMakeShipStopRefuelling(ShipObj *ship)
{
	if (ship->aiState != kStateRefuel)
		return;
	ship->targetStellar = -1;
	ship->targetShip = -1;
	ship->aiTimer = 0;
	ship->aiState = kStateHiJack;
	ship->aiSubstate = kManeuverLoJack;
}
