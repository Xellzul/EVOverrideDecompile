/* ai.h -- ship AI. Notes: Research/notes/headers.md */
#ifndef AI_H
#define AI_H

#include "evglobals.h"

void AIDispatch(ShipObj *ship);
void ScanPlayer(ShipObj *ship);
void AISetup(ShipObj *ship);
void WimpyTraderAI(ShipObj *ship);
void BraveTraderAI(ShipObj *ship);
void WarshipAI(ShipObj *ship);
void InterceptorAI(ShipObj *ship);
void AiFighter(ShipObj *ship);
void EscortAI(ShipObj *ship);
void DefenseShipAI(ShipObj *ship);
void HighLevelAIHandler(ShipObj *ship);
void LowLevelAIHandler(ShipObj *ship);
void SelectRandomStellarDest(ShipObj *ship);
Boolean RecentlyVisitedStellarInSystem(ShipObj *ship, short syst);
void AIFireTurret(ShipObj *ship);
void AIFireMissile(ShipObj *ship);
void AIFireGun(ShipObj *ship);
void AILaunchFighter(ShipObj *ship);
void DrawHighGoalAbbreviation(short state);
void DrawLowGoalAbbreviation(short maneuver);
void SelectWarshipTarget(ShipObj *ship);
Boolean IsThreatToPlayer(ShipObj *ship);
Boolean ExtendedIsThreatToPlayer(ShipObj *ship);
Boolean ExtendedIsThreatToShip(ShipObj *observer, ShipObj *cand);
Boolean IsShipThreatened(ShipObj *ship);
Boolean AIDoesShipLikePlayer(ShipObj *ship);
Boolean IsPlayerThreatened(void);
Boolean IsPlayerThreatenedByMyEnemy(ShipObj *ship);
Boolean IsShipMyEnemy(ShipObj *a, ShipObj *b);
void DoGoodSamaritan(ShipObj *ship, ShipObj *attacker);
void AIMakeShipLeave(ShipObj *ship);
void AIMakeShipJumpWithParent(ShipObj *ship);
void AIMakeShipAttackPlayer(ShipObj *ship);
void AIMakeShipAttackShip(ShipObj *ship, ShipObj *target);
void AIMakeShipWait(ShipObj *ship);
void AIFightThreatToPlayer(ShipObj *ship);
void AIFightThreatToParent(ShipObj *ship);
void AIFightEnemyThreatToPlayer(ShipObj *ship);
void AIMakeShipRefuelPlayer(ShipObj *ship);
void AIMakeFighterReturn(ShipObj *ship);
void AIMakeEscortFlyInForm(ShipObj *ship);
void AIMakeShipStopJumping(ShipObj *ship);
void AIMakeShipJumpIn(ShipObj *ship);
Boolean AIIsShipJumping(ShipObj *ship);
Boolean IsShipDocking(ShipObj *ship);
Boolean IsShipInForm(ShipObj *ship);
Boolean IsShipAttacking(ShipObj *ship);
Boolean IsShipWaiting(ShipObj *ship);
Boolean AIEvalHopelessChase(ShipObj *ship);
Boolean AIIsShipBusy(ShipObj *ship);
Boolean AIIsShipLeaving(ShipObj *ship);
void AICallForHelp(ShipObj *ship);
void AIEscortFireUnprovoked(ShipObj *ship);
Boolean AIIsShipAttemptingDisable(ShipObj *ship);
void AIPlunderIfPossible(ShipObj *ship);
void AIMakeShipStopRefuelling(ShipObj *ship);

#endif
