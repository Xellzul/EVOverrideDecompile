/* Escape Velocity.c -- game start-up, the sim step, damage, NPC and fleet spawning, escorts, pers. Notes: Research/notes/Escape Velocity.md */
#include <Types.h>
#include <Memory.h>
#include <Quickdraw.h>
#include <Events.h>
#include <Resources.h>
#include <TextUtils.h>
#include <OSUtils.h>
#include <Dialogs.h>
#include <Windows.h>
#include <TextEdit.h>
#include <stdlib.h>
#include <string.h>

#include "evo.h"



extern Boolean			doubleTime;
extern Str255			chatterText;



extern Boolean		gameLoaded;


short masterVolume;
Rect sweepRect;
long lastRedrawTicks;

// FUNCTION: EVO 0x10061bb0
void main(void)
{
	SetupToolbox();
	SetupMonitor();
	CheckSystem();
	BuildTrigTable();
	InitColors();
	InitDataFiles();
	LoadPrefs();
	ShowOldOsWarningIfNeeded();
	SavePrefs();
	OpenDataFiles();
	FadeScreenOut(16, &rgbBlack);
	cursorHidden = true;
	InitMenuBar();
	BuildMenuBarGrayRegion();
	HideControlStrip();
	SetupSAT();
	SetupWind();
	HideCursor();
	BootSoundSubsystem(8, 1, 0);
	masterVolume = GetMasterVolume();
	InitOffscreen();
	StartIntroMusic();
	ShowAmbrosiaLogo();
	LoadSounds();
	LoadStrings();
	InitArrays();
	BailCheck();
	InitObjects();
	LoadObjectData();
	InitMissions();
	HideAmbrosiaLogo();
	FadeScreenIn(16);
	ShowSplashScreen();
	InitIntroProgressBar();
	LoadSprites();
	HideSplashScreen();
	InitSprites();
	InitStarSprites();
	ResetPlayer(true);
	ResetPlayerRecord(true);
	gameLoaded = gameInProgress = false;
	InitMainScreen();
	OpenLastPilotFile();
	DoMainScreen();
	ShowCursor();
	Bye();
}

// FUNCTION: EVO 0x10061d74
void SpawnWorldSpriteNodes(void)
{
	short i, j;
	SpritePtr sp;
	ShipObj *ship;
	SpobRecord *spob;

	for (i = 0; i < MaxShipsInSystem; i++) {
		if (shipData[i].active) {
			if (shipData->curSystem == shipData[i].curSystem && !shipData[i].hasWorldSpriteNode) {
				sp = nil;
				sp = SATNewSprite(0, 0, 0, nil);
				if (sp != nil) {
					sp->task = HandleShipDisplay;
					sp->hitTask = RunWeaponHitDispatcher;
					sp->destructTask = nil;
					sp->kind = 1;
					sp->mode = 1;
					ship = &shipData[i];
					sp->appPtr = (Ptr)ship;
					shipData[i].hasWorldSpriteNode = true;
					if (i == 0) {
						sp->layer = 15;
					} else {
						sp->layer = 10;
						if (shipData[i].govt != -1 && (govtData[shipData[i].govt].flags & kGovtPlunders))
							sp->layer = 11;
					}
				}
			}
		} else {
			shipData[i].hasWorldSpriteNode = false;
		}
	}
	for (j = 0; j <= 3; j++) {
		if (gSystems[shipData->curSystem].navDefault[j] != -1) {
			i = gSystems[shipData->curSystem].navDefault[j];
			if (stellarData[i].loaded && shipData->curSystem == stellarData[i].system && !stellarData[i].spawned) {
				sp = nil;
				sp = SATNewSprite(0, 0, 0, nil);
				if (sp != nil) {
					sp->task = HandleStellarSprites;
					sp->hitTask = nil;
					sp->destructTask = nil;
					sp->layer = 2;
					sp->kind = 0;
					sp->mode = 0;
					spob = &stellarData[i];
					sp->appPtr = (Ptr)spob;
					stellarData[i].spawned = true;
				}
			}
		}
	}
	for (i = 0; i < MaxAsteroids; i++) {
		if (asteroidData[i].active) {
			if (!asteroidData[i].spawned) {
				sp = nil;
				sp = SATNewSprite(0, 0, 0, nil);
				if (sp != nil) {
					sp->task = AsteroidTask;
					sp->hitTask = AsteroidHitTask;
					sp->destructTask = nil;
					sp->kind = 50;
					sp->mode = 50;
					sp->layer = 4;
					sp->appLong = i;
					sp->appPtr = nil;
					sp->hotRect.left = sp->hotRect.top = 0;
					if (asteroidData[i].type == 0)
						sp->hotRect.right = sp->hotRect.bottom = GetSpriteXSize(smallRockFaces[0]);
					else
						sp->hotRect.right = sp->hotRect.bottom = GetSpriteXSize(bigRockFaces[0]);
					asteroidData[i].spawned = true;
				}
			}
		} else {
			asteroidData[i].spawned = false;
		}
	}
}

// FUNCTION: EVO 0x100621a8
void PlayGame(void)
{
	Str255 dead;
	Rect dead2, dead3;
	Rect r;
	DateTimeRec date;
	unsigned long ticks;
	Boolean skipTick, skipDraw;

	skipTick = EVBugResourceFieldTest(11);
	skipDraw = EVBugResourceFieldTest(12);
	GetTime(&date);
	SATSetPortBackScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
	SATSetPortOffScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
	SATSetPortScreen();
	SATRedraw();
	RedrawStatus();
	RedrawComm(false);
	HandleStellarSystemVisibility();
	FadeScreenIn(16);
	lastRedrawTicks = TickCount();
	while (!gQuit && !gAbortGame) {
		ticks = TickCount();
		HandleTimeAdjustment();
		if (!skipTick) {
			sweepRect.bottom = sweepRect.top + 64;
			sweepRect.left = gSAT.bounds.left;
			sweepRect.right = gSAT.bounds.right;
			SATSetPortBackScreen();
			ForeColor(blackColor);
			PaintRect(&sweepRect);
			if (SectRect(&sweepRect, &msgRect, &r))
				RedrawComm(true);
			SATBackChanged(&sweepRect);
			SATSetPortScreen();
			OffsetRect(&sweepRect, 0, 64);
			if (sweepRect.bottom < gSAT.bounds.top || sweepRect.top > gSAT.bounds.bottom)
				sweepRect.top = gSAT.bounds.top;
			if (frameCount % 3 == 0) {
				thirdFrame = true;
				lastRedrawTicks = TickCount();
			} else {
				thirdFrame = false;
			}
			DoPlayGameWork(true);
		}
		if (!skipDraw)
			SATRun(!settings.useQuickdraw);
		EraseBeams();
		DrawBeams();
		if (QuickKeyCheck(keyMap[26]) && shipData->jumpTimer <= 0 && shipData->type != kEscapePodShip &&
				shipData->deathTimer <= 0 && shipData->hasWorldSpriteNode && gTargetStellar <= 0)
			gAbortGame = true;
		if (doubleTime && !skipTick) {
			DoPlayGameWork(false);
			SATSkip();
		}
		if (gQuit)
			break;
		frameCount++;
		if (frameCount > 1024) {
			frameCount = 0;
			if (date.day == 30 && date.month == 6 && commCount <= 0)
				DisplayComm("\pHappy birthday Matt", 5, 3, 9, &rgbDarkGrey, false, false);
		}
		killShots = false;
		killBoxes = false;
		killExplods = false;
		killSmoke = false;
		killAsteroids = false;
		if (QuickKeyCheck(kKeyCapsLock)) {
			if (abs(shipData->heading - shipData->desiredHeading) <= 10 && CountMatchingSoundVoices((long)warpUpSnd))
				doubleTime = false;
			else
				doubleTime = true;
		} else {
			doubleTime = false;
		}
		while (ticks > TickCount() - 1)
			;
	}
	RestoreOriginalClut(0);
	FadeScreenOut(16, &rgbBlack);
	FlushEvents(everyEvent - diskMask, 0);
	SATSetPortScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
}

// FUNCTION: EVO 0x10062638
void DoPlayGameWork(Boolean full)
{
	short i, count;

	HideTheCursor();
	SpawnWorldSpriteNodes();
	QuickKeyCheckHandler();
	HandlePlayer(shipData);
	if (full) {
		DrawStatus();
		MissionHandler();
		HandleVoiceSound();
		NoOp();
		HandleComm();
		CalculateTargetedDamage();
		EnterMoreShips(shipData->curSystem);
		SpawnAsteroid(true);
		asteroidsDirty = false;
		for (i = 1; i < MaxShipsInSystem; i++) {
			if (shipData[i].active && shipData[i].aiType >= kAIWimpyTrader && shipData->curSystem == shipData[i].curSystem)
				AIDispatch(&shipData[i]);
		}
	}
	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && shipData->curSystem == shipData[i].curSystem)
			HandleShip(&shipData[i]);
	}
	if (full)
		HandleBeams();
}

// FUNCTION: EVO 0x10062800
pascal void ShotHitTask(SpritePtr a, SpritePtr b)
{
	short s1, s2;

	if ((b->mode == 5 && a->mode == -1) || (a->mode == 5 && b->mode == -1)) {
		s1 = a->appLong;
		s2 = b->appLong;
		if (shotData[s1].owner != shotData[s2].owner && (shotData[s1].guidanceState == kShotStateFlareDecoy || shotData[s2].guidanceState == kShotStateFlareDecoy)) {
			SpawnExplod(shotData[s1].pos, -1, 0, 0);
			PlaySoundDistance(-1, explodeSnd[1], 5, &shotData[s1].pos, &shipData->pos);
			shotData[s1].life = kSlotFree;
			shotData[s2].life = kSlotFree;
		}
	}
}

// FUNCTION: EVO 0x10062960
pascal void RunWeaponHitDispatcher(SpritePtr a, SpritePtr b)
{
	ShipObj *ship;
	short s, i, n, radius, massDmg, energyDmg;
	Boolean wasDead, hit;
	FloatPoint where;
	char dead[544];

	if (b->mode == -1 && b->appLong >= 0 && b->appLong < 128) {
		ship = (ShipObj *)a->appPtr;
		s = b->appLong;
		if (shotData[s].life > 0 && ship->type != kEscapePodShip && ship->index != shotData[s].owner && ship->persType != kPersAmbrosiaMascot
				&& (gWeapons[shotData[s].weapType].guidance != kGuidanceHoming || ship->index == shotData[s].target)) {
			if (shipData[shotData[s].owner].parentShip != -1 && ship->parentShip != -1
					&& ship->parentShip == shipData[shotData[s].owner].parentShip)
				return;
			if (shipData[shotData[s].owner].govt == -1 || ship->govt != shipData[shotData[s].owner].govt) {
				if (shipData[shotData[s].owner].defenseStellar != -1
						&& ship->defenseStellar == shipData[shotData[s].owner].defenseStellar)
					return;
				if (shotData[s].owner != 0 || ship->parentShip == -1 || shipData[ship->parentShip].parentShip != 0) {
					if (ship->index == 0 && shipData[shotData[s].owner].govt != -1
							&& (govtData[shipData[shotData[s].owner].govt].flags & kGovtNeverAttacks))
						return;
					if (ship->govt != -1 && ship->index != 0 && (govtData[ship->govt].flags & kGovtNeverAttacks)) {
						if (shotData[s].owner == 0)
							return;
						if (shotData[s].owner != -1 && shipData[shotData[s].owner].parentShip == 0)
							return;
					}
					if (shipData[shotData[s].owner].parentShip != -1) {
						if (ship->index == shipData[shotData[s].owner].parentShip)
							return;
						if (ship->parentShip == shipData[shotData[s].owner].parentShip && ship->parentShip != -1)
							return;
					}
					if (ship->defenseStellar == -1) {
						if (shipData[shotData[s].owner].parentShip != -1 && ship->index == shipData[shotData[s].owner].parentShip)
							return;
						if (ship->parentShip != -1 && ship->parentShip == shotData[s].owner)
							return;
					}
					if (ship->index == 0 && gInvertedColors)
						DoShipUncloak();
					if (gWeapons[shotData[s].weapType].explodType == kExplodSmall)
						SpawnExplod(shotData[s].pos, ship->index, 0, 0);
					if (gWeapons[shotData[s].weapType].explodType == kExplodBig) {
						where.x = shotData[s].pos.x;
						where.y = shotData[s].pos.y;
						PlaySoundDistance(-1, explodeSnd[1], 6, &where, &shipData->pos);
						SpawnExplod(shotData[s].pos, ship->index, 1, 0);
					}
					if (gWeapons[shotData[s].weapType].explodType == kExplodHuge) {
						where.x = shotData[s].pos.x;
						where.y = shotData[s].pos.y;
						PlaySoundDistance(-1, explodeSnd[0], 6, &where, &shipData->pos);
						SpawnExplod(shotData[s].pos, ship->index, 2, 0);
						radius = gWeapons[shotData[s].weapType].blastRadius;
						n = 2.0f * (float)((float)radius / 50.0);
						for (i = 0; i < n; i++) {
							where.x = (shotData[s].pos.x + Rand(0.5 * radius)) - radius / 4;
							where.y = (shotData[s].pos.y + Rand(0.5 * radius)) - radius / 4;
							SpawnExplod(where, ship->index, 1, Rand(8) + 4);
						}
						n = 8.0f * (float)((float)radius / 50.0);
						for (i = 0; i < n; i++) {
							where.x = (shotData[s].pos.x + Rand(0.5 * radius)) - radius;
							where.y = (shotData[s].pos.y + Rand(0.5 * radius)) - radius;
							SpawnExplod(where, ship->index, 0, Rand(16) + 8);
						}
					}
					massDmg = gWeapons[shotData[s].weapType].massDmg;
					energyDmg = gWeapons[shotData[s].weapType].energyDmg;
					wasDead = IsDying(ship);
					if (shotData[s].damageFalloffSteps > 0) {
						massDmg -= shotData[s].damageFalloffSteps;
						energyDmg -= shotData[s].damageFalloffSteps;
						if (massDmg < 0)
							massDmg = 0;
						if (energyDmg < 0)
							energyDmg = 0;
					}
					if (ship->index == shotData[s].target)
						DamageShip(ship, shotData[s].pos, gWeapons[shotData[s].weapType].impact, massDmg, energyDmg,
								shotData[s].owner, 1, 1, shotData[s].fromGuardingEscort, 0);
					else
						DamageShip(ship, shotData[s].pos, gWeapons[shotData[s].weapType].impact, massDmg, energyDmg,
								shotData[s].owner, 1, 0, shotData[s].fromGuardingEscort, 0);
					if (IsDying(ship) && !wasDead) {
						for (i = 1; i < MaxShipsInSystem; i++) {
							if (ship->index != i && shipData[i].active && shipData[i].parentShip == 0
									&& ship->index == shipData[i].targetShip && shipData[i].aiType >= kAIWarship
									&& IsShipAttacking(&shipData[i]))
								SpeakVoiceMessage(2);
						}
					}
					if (shotData[s].owner == 0)
						DoGoodSamaritan(ship, shipData);
					if (gWeapons[shotData[s].weapType].blastRadius > 0) {
						for (i = 0; i < MaxShipsInSystem; i++) {
							hit = true;
							if (i == shotData[s].owner
									&& ((gWeapons[shotData[s].weapType].flags & kWeapSafeForPlayer) || shotData[s].owner != 0))
								hit = false;
							if (i != ship->index && shipData[i].active && hit) {
								where.x = FloatAbs(shipData[i].pos.x - shotData[s].pos.x);
								where.y = FloatAbs(shipData[i].pos.y - shotData[s].pos.y);
								if (where.x <= gWeapons[shotData[s].weapType].blastRadius
										&& where.y <= gWeapons[shotData[s].weapType].blastRadius)
									DamageShip(&shipData[i], shotData[s].pos, gWeapons[shotData[s].weapType].impact,
											gWeapons[shotData[s].weapType].massDmg, gWeapons[shotData[s].weapType].energyDmg,
											shotData[s].owner, 0, 0, shotData[s].fromGuardingEscort, 0);
							}
						}
					}
					shotData[s].life = kSlotFree;
					b->task = 0;
					b->appLong = -1;
				}
			}
		}
	}
}

// FUNCTION: EVO 0x10063984
void DamageShip(ShipObj *ship, FloatPoint where, short impact, short massDmg, short energyDmg, short owner, Boolean flagA, Boolean flagB, Boolean fromEscort, Boolean flagD)
{
	long dmg;
	Boolean wasDisabled, provoke;
	short big, heading, grand[2];

	if (ship->type != kEscapePodShip) {
		wasDisabled = IsDisabled(ship);
		if (impact > 0 && ship->jumpTimer <= 0) {
			heading = CalcInterceptAngle(where, ship->pos);
			AdjustedAccel(heading, (float)(double)(float)impact / (float)gShipTypes[ship->type].mass,
					&ship->vel, gShipTypes[ship->type].maxSpeed);
			SpeedLimit(&ship->vel, ShipMaxSpeed(ship));
		}
		if (massDmg > energyDmg)
			big = massDmg;
		else
			big = energyDmg;
		dmg = 1;
		if (ship->shield < 0) {
			dmg = massDmg + 0.25 * energyDmg;
			if (dmg < 1 && (massDmg > 0 || energyDmg > 0))
				dmg = 1;
			if (fromEscort && ship->shield - dmg < -ShipArmorCapacity(ship)) {
				if (gShipTypes[ship->type].flags & kShipDisableAt10)
					dmg = 0.85 * ShipArmorCapacity(ship) + ship->shield;
				else
					dmg = 0.6 * ShipArmorCapacity(ship) + ship->shield;
			}
			if (dmg < 0)
				dmg = 0;
			ship->shield -= dmg;
		} else {
			dmg = 0.25 * massDmg + energyDmg;
			if (dmg < 1 && (massDmg > 0 || energyDmg > 0))
				dmg = 1;
			if (fromEscort && ship->shield - dmg < -ShipArmorCapacity(ship)) {
				if (gShipTypes[ship->type].flags & kShipDisableAt10)
					dmg = 0.85 * ShipArmorCapacity(ship) + ship->shield;
				else
					dmg = 0.6 * ShipArmorCapacity(ship) + ship->shield;
			}
			if (flagD && ship->shield - dmg < 0)
				dmg = ship->shield;
			if (dmg < 0)
				dmg = 0;
			ship->shield -= dmg;
		}
		if ((owner == 0 || shipData[owner].parentShip == 0) && ship->defenseStellar == -1
				&& ship->persType != kPersAmbrosiaMascot && ship->persType != kPersBountyHunter) {
			if (IsDisabled(ship) && !wasDisabled)
				SlapWithPenalty(shipData->curSystem, ship->govt, 1, ship->specialMission);
			if (ship->shield < -ShipArmorCapacity(ship))
				SlapWithPenalty(shipData->curSystem, ship->govt, 3, ship->specialMission);
			if (ship->shield < -ShipArmorCapacity(ship) && playerKills + gShipTypes[ship->type].crew > 0
					&& playerKills + gShipTypes[ship->type].crew < kShortBound)
				playerKills = playerKills + gShipTypes[ship->type].crew;
		}
		if (IsDisabled(ship)) {
			if (ship->specialMission != -1 && ship->defenseStellar == -1 && !wasDisabled
					&& currentMissionObjectives[ship->specialMission].active) {
				if (!currentMissionObjectives[ship->specialMission].failed
						&& currentMissions[ship->specialMission].specialShipsDisabled == 0
						&& currentMissions[ship->specialMission].shipGoal == kMisnGoalEscort) {
					PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
					DisplayComm("\pMission failed.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
					QuickMissionFailure(ship->specialMission);
				}
				currentMissions[ship->specialMission].specialShipsDisabled++;
			}
			if (ship->parentShip == 0 && ship->specialMission == -1) {
				ship->parentShip = -1;
				if (ship->aiType == kAIEscort && gShipTypes[ship->type].inherentAI <= kAIBraveTrader) {
					DestroyPartialFleetCargo(ship->index);
					hudRedraw.credits = true;
					DrawStatusCargo();
				}
				ship->aiType = gShipTypes[ship->type].inherentAI;
			}
		}
		if (flagA && ship->aiType > 0
				&& (owner != ship->index || shipData[owner].defenseStellar != -1 || ship->defenseStellar != -1)) {
			provoke = false;
			if (ship->defenseStellar != -1 || shipData[owner].defenseStellar != -1) {
				if (ship->defenseStellar != -1 && shipData[owner].defenseStellar != -1)
					provoke = false;
				else
					provoke = true;
			} else {
				if (ship->govt != shipData[owner].govt)
					provoke = true;
				if (ship->parentShip != -1) {
					if (owner != ship->parentShip)
						provoke = true;
					if (shipData[owner].parentShip != -1 && ship->parentShip != shipData[owner].parentShip)
						provoke = true;
				}
				if (ship->govt == -1) {
					if (owner == 0 || shipData[owner].parentShip == 0)
						provoke = true;
				} else {
					if (gSystems[shipData->curSystem].govt >= 0 && (owner == 0 || shipData[owner].parentShip == 0)
							&& shipData->targetShip != ship->index
							&& legalStatus[shipData->curSystem] >= (govtData[ship->govt].crimeTol << 1))
						provoke = false;
					if (ship->govt == shipData[owner].govt)
						provoke = false;
				}
				if (ship->parentShip != -1 && ship->parentShip == shipData[owner].parentShip
						&& shipData[owner].defenseStellar != -1)
					provoke = false;
				if (!flagB && provoke) {
					if (owner == 0) {
						if (shipData[owner].targetShip != -1) {
							if (ship->index == shipData[owner].targetShip) {
								provoke = true;
							} else {
								if (IsShipMyEnemy(ship, &shipData[shipData[owner].targetShip])) {
									provoke = false;
								} else {
									if (Rand(50) != 0)
										provoke = false;
								}
							}
						}
					} else if (owner > 0 && ship->index != shipData[owner].targetShip) {
						provoke = false;
					}
					if (provoke && ship->govt != -1 && owner == 0 && govtData[ship->govt].penalties[kPenaltyShoot] <= 0
							&& Rand(25) != 0)
						provoke = false;
				}
				if (owner != -1) {
					if (shipData[owner].parentShip == 0 && shipData[owner].aiType == kAIFighter && ship->targetShip == 0
							&& FloatAbs(ship->pos.x - shipData->pos.x) <= 320.0f
							&& FloatAbs(ship->pos.y - shipData->pos.y) <= 320.0f)
						provoke = false;
					if (ship->parentShip == shipData[owner].parentShip && ship->parentShip != -1)
						provoke = false;
				}
				if (ship->jumpTimer > 0)
					provoke = false;
				grand[0] = grand[1] = -1;
				if (ship->parentShip != -1)
					grand[0] = shipData[ship->parentShip].parentShip;
				if (shipData[owner].parentShip != -1 && owner != 0)
					grand[1] = shipData[shipData[owner].parentShip].parentShip;
				if (grand[0] == grand[1] && grand[0] != -1 && grand[1] != -1)
					provoke = false;
			}
			if (provoke) {
				if (ship->defenseStellar == -1) {
					ship->grudge += massDmg + energyDmg;
					ship->targetShip = owner;
				} else {
					ship->grudge += (massDmg + energyDmg) * 30;
					ship->targetShip = 0;
				}
				if (owner == 0)
					AIMakeShipAttackPlayer(ship);
				if (ship->aiTimer > 20)
					ship->aiTimer = 20;
				if (owner == 0 && ship->defenseStellar == -1) {
					if (ship->persType != -1 && (personData[ship->persType].flags & kPersGrudge))
						personData[ship->persType].grudge = true;
					if (owner == 0) {
						AIMakeShipStopRefuelling(ship);
						ship->targetShip = owner;
					}
				}
			}
		}
		if (ship->index == 0)
			hudRedraw.shield = true;
	}
}

// FUNCTION: EVO 0x10064728
void DeleteAllComputerShips(Boolean disabledToo, Boolean b)
{
	short i;
	Boolean doDelete;

	for (i = 1; i < MaxShipsInSystem; i++) {
		doDelete = true;
		if (shipData[i].aiType >= kAIFighter && shipData[i].parentShip == 0 && shipData[i].defenseStellar == -1 && shipData[i].specialMission == -1) {
			if (IsDisabled(&shipData[i]))
				doDelete = true;
			else if (!disabledToo)
				doDelete = false;
		}
		if (doDelete) {
			if (shipData[i].active && shipData[i].auxMission != -1) {
				if (currentMissionObjectives[shipData[i].auxMission].active)
					currentMissions[shipData[i].auxMission].auxShipsLeft++;
				if (currentMissions[shipData[i].auxMission].auxShipsLeft > currentMissions[shipData[i].auxMission].auxShipCount)
					currentMissions[shipData[i].auxMission].auxShipsLeft = currentMissions[shipData[i].auxMission].auxShipCount;
			} else {
				if (shipData[i].active && shipData[i].defenseStellar != -1) {
					stellarData[shipData[i].defenseStellar].tribute++;
					if (stellarData[shipData[i].defenseStellar].tribute > stellarData[shipData[i].defenseStellar].defCount)
						stellarData[shipData[i].defenseStellar].tribute = stellarData[shipData[i].defenseStellar].defCount;
				}
			}
			shipData[i].active = false;
			shipData[i].specialMission = -1;
			shipData[i].defenseStellar = -1;
			shipData[i].isTractored = false;
			shipData[i].hired = false;
			shipData[i].hailed = false;
			shipData[i].hasAfterburner = AIHasAfterburner(&shipData[i]);
			shipData[i].auxMission = -1;
			shipData[i].curSystem = -1;
			shipData[i].parentShip = -1;
		}
	}
}

// FUNCTION: EVO 0x10064b58
void SetupShipsInSystem(short syst)
{
	short i, m, k, e, count, spawned;
	long pad[2];
	short cum[4];
	short dead;

	DeleteAllComputerShips(false, true);
	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].parentShip == 0) {
			if (shipData[i].active) {
				if (IsDisabled(&shipData[i])) {
					shipData[i].active = false;
					if (shipData[i].parentShip == 0 && shipData[i].aiType == kAIEscort && shipData[i].specialMission == -1) {
						if (gShipTypes[shipData[i].type].inherentAI <= kAIBraveTrader)
							DestroyPartialFleetCargo(i);
					}
				} else {
					if (shipData[i].specialMission == -1) {
						if (IsDisabled(&shipData[i])) {
							shipData[i].active = false;
							shipData[i].parentShip = -1;
						} else
							RespawnEscort(&shipData[i]);
					}
				}
			}
		}
	}
	spawned = 0;
	for (m = 0; m < MaxSimultaneousMissions; m++) {
		if (currentMissionObjectives[m].active && (syst == currentMissions[m].shipSyst || currentMissions[m].shipSyst == kShipSystFollow) && currentMissions[m].shipCount > 0 && currentMissions[m].shipDelay < 0) {
			count = currentMissions[m].shipCount;
			if (currentMissions[m].shipSyst == kShipSystFollow) {
				if (currentMissions[m].shipBehav % kMisnBehavVariant == 1) {
					for (k = 1; k < MaxShipsInSystem; k++) {
						if (shipData[k].active && m == shipData[k].specialMission)
							count--;
					}
				}
			}
			if (count > 0) {
				for (i = 0; i < count; i++) {
					e = MissionDudeSpawn(currentMissions[m].shipDude, syst, m);
					if (e != -1) {
						spawned++;
						if (currentMissions[m].shipGoal == kMisnGoalEscort) {
							shipData[e].pos.x = Rand(512) - 256;
							shipData[e].pos.y = Rand(512) - 256;
						}
						if (currentMissions[m].shipGoal == kMisnGoalRescue) {
							shipData[e].heading = Rand(kFullCircle);
							shipData[e].vel.x = shipData[e].vel.y = 0;
							if (gShipTypes[shipData[e].type].flags & kShipDisableAt10)
								shipData[e].shield = -0.85 * gShipTypes[shipData[e].type].armor;
							else
								shipData[e].shield = -0.6 * gShipTypes[shipData[e].type].armor;
							if (currentMissions[m].timeLimit <= 0 && currentMissions[m].timeLimit > -kShortBound)
								shipData[e].boarded = true;
						}
					}
				}
				if (currentMissions[m].flags & kMisnAutoAbort)
					AutoAbortMission(m);
			}
		}
	}
	for (i = 0; i < gSystems[syst].avgShips; i++) {
		count = -1;
		if (Rand(7) == 0) {
			count = SpawnPerson(shipData->curSystem, false, -1);
		} else if (Rand(7) == 0) {
			count = -1;
			SpawnFleet(shipData->curSystem);
		} else {
			e = Rand(100) + 1;
			dead = -1;
			cum[0] = gSystems[syst].dudeProbs[0];
			for (k = 1; k <= 3; k++)
				cum[k] = cum[k - 1] + gSystems[syst].dudeProbs[k];
			for (k = 0; k <= 3; k++) {
				if (e <= cum[k]) {
					if (gSystems[syst].dudeTypes[k] < 0)
						HyperSpawnFleet(syst, abs(gSystems[syst].dudeTypes[k] + FirstResourceID));
					else if (gSystems[shipData->curSystem].dudeTypes[k] >= 0)
						count = RandomShipSpawn(syst, 2);
					break;
				}
			}
		}
		if (count != -1)
			AdjustedAccel(shipData[count].heading, gShipTypes[shipData[count].type].maxSpeed, &shipData[count].vel, gShipTypes[shipData[count].type].maxSpeed);
	}
	for (i = 0; i < 4; i++) {
		if (gSystems[syst].persons[i] != -1 && personData[gSystems[syst].persons[i]].loaded) {
			count = SpawnPerson(shipData->curSystem, false, gSystems[syst].persons[i]);
			if (count != -1)
				AdjustedAccel(shipData[count].heading, gShipTypes[shipData[count].type].maxSpeed, &shipData[count].vel, gShipTypes[shipData[count].type].maxSpeed);
		}
	}
}

// FUNCTION: EVO 0x10065530
short RandomShipSpawn(short syst, short mode)
{
	short s, i, j, k, roll, pick, range;
	short spare[4];
	short cum[4];
	long dead[4];

	mode = abs(mode);
	for (s = 1; s < MaxShipsInSystem - mode; s++) {
		if (!shipData[s].active && !shipData[s].hasWorldSpriteNode) {
			roll = Rand(100) + 1;
			k = -1;
			for (i = 0; i <= 3; i++) {
				cum[i] = 0;
				for (j = 0; j <= i; j++)
					cum[i] += gSystems[syst].dudeProbs[j];
			}
			for (i = 3; i >= 0; i = i - 1) {
				if (roll <= cum[i] && gSystems[syst].dudeTypes[i] >= 0)
					k = i;
			}
			if (k != -1) {
				k = gSystems[syst].dudeTypes[k];
				pick = SelectShipFieldFromDude(&dudeData[k]);
				if (pick >= 0 && pick <= 3) {
					shipData[s].active = true;
					shipData[s].curSystem = syst;
					range = 750;
					shipData[s].dude = k;
					shipData[s].type = dudeData[k].shipTypes[pick];
					shipData[s].govt = dudeData[k].govt;
					if (dudeData[k].aiType >= kAIWimpyTrader)
						shipData[s].aiType = dudeData[k].aiType;
					else
						shipData[s].aiType = gShipTypes[shipData[s].type].inherentAI;
					if (shipData[s].aiType == kAIWarship && gSystems[shipData[s].curSystem].navDefault[0] != -1
							&& gShipTypes[shipData[s].type].maxSpeed == 0.0) {
						shipData[s].pos.x = stellarData[gSystems[shipData[s].curSystem].navDefault[0]].pos.h;
						shipData[s].pos.y = stellarData[gSystems[shipData[s].curSystem].navDefault[0]].pos.v;
						Accel(Rand(kFullCircle), 100, &shipData[s].pos);
					} else {
						shipData[s].pos.x = Rand(range * 2) - range;
						shipData[s].pos.y = Rand(range * 2) - range;
					}
					shipData[s].vel.x = shipData[s].vel.y = 0;
					shipData[s].navTargetKind = kNavOff;
					shipData[s].jumpTimer = 0;
					shipData[s].aiState = kStateHiJack;
					shipData[s].aiSubstate = kManeuverLoJack;
					shipData[s].boarded = false;
					shipData[s].specialMission = -1;
					shipData[s].grudge = 0;
					shipData[s].aiTimer = 0;
					shipData[s].parentShip = -1;
					shipData[s].targetShip = -1;
					shipData[s].targetStellar = -1;
					shipData[s].deathTimer = 0;
					shipData[s].skill = RandomSkillLevel(shipData[s].type);
					shipData[s].credits = 10000;
					shipData[s].aggression = Rand(3) ^ 2;
					shipData[s].shield = gShipTypes[shipData[s].type].shield;
					shipData[s].fuel = gShipTypes[shipData[s].type].fuel;
					shipData[s].defenseStellar = -1;
					shipData[s].isTractored = false;
					shipData[s].hired = false;
					shipData[s].hailed = false;
					shipData[s].hasAfterburner = AIHasAfterburner(&shipData[s]);
					shipData[s].auxMission = -1;
					shipData[s].persType = -1;
					shipData[s].thrust = 0.0;
					shipData[s].speedLimit = 0.0;
					shipData[s].fireQueued = false;
					for (i = 0; i < MaxWeaponTypes; i++) {
						shipData[s].weap[i].count = gShipTypes[shipData[s].type].weapCount[i];
						if (gWeapons[i].guidance != kGuidanceBay)
							shipData[s].weap[i].ammo = kShipAmmoFactor * gShipTypes[shipData[s].type].weapAmmo[i];
						else
							shipData[s].weap[i].ammo = gShipTypes[shipData[s].type].weapAmmo[i];
					}
					AISetup(&shipData[s]);
					return s;
				}
			}
		}
	}
	return -1;
}

// FUNCTION: EVO 0x10065ef4
short HyperShipSpawn(short syst)
{
	short s;
	float dist, speed;
	FloatPoint dead;
	FloatPoint origin;

	if (Rand(7) == 0) {
		s = SpawnPerson(syst, true, -1);
	} else if (Rand(7) == 0) {
		s = -1;
		SpawnFleet(syst);
	} else {
		s = RandomShipSpawn(syst, 2);
	}
	if (s != -1) {
		dist = 0.0;
		speed = 50;
		while (speed > 0) {
			dist += speed;
			speed -= kJumpInDecel;
		}
		shipData[s].pos.x = shipData[s].pos.y = 0;
		Accel(Rand(kFullCircle), kJumpInDistance + dist, &shipData[s].pos);
		origin.x = origin.y = 0.0;
		shipData[s].heading = CalcInterceptAngle(shipData[s].pos, origin);
		shipData[s].lastStellar = -2;
		shipData[s].prevSystem = -1;
		shipData[s].vel.x = shipData[s].vel.y = 0.0;
		shipData[s].jumpTimer = kJumpWindupArriving;
		shipData[s].jumpTick = TickCount();
		Accel(shipData[s].heading, kJumpInSpeed, &shipData[s].vel);
		AIMakeShipJumpIn(&shipData[s]);
		return s;
	}
	return -1;
}

// FUNCTION: EVO 0x1006615c
short SpecificDudeSpawn(short dude, short syst)
{
	short s, pick;
	short i;
	char dead[48];

	s = GenericRandomShipSpawn(syst, true);
	if (s != -1) {
		pick = SelectShipFieldFromDude(&dudeData[dude]);
		if (pick >= 0 && pick <= 3) {
			shipData[s].dude = dude;
			shipData[s].type = dudeData[dude].shipTypes[pick];
			shipData[s].govt = dudeData[dude].govt;
			if (dudeData[dude].aiType >= kAIWimpyTrader)
				shipData[s].aiType = dudeData[dude].aiType;
			else
				shipData[s].aiType = gShipTypes[shipData[s].type].inherentAI;
			AISetup(&shipData[s]);
			for (i = 0; i < MaxWeaponTypes; i++) {
				shipData[s].weap[i].count = gShipTypes[shipData[s].type].weapCount[i];
				if (gWeapons[i].guidance != kGuidanceBay)
					shipData[s].weap[i].ammo = kShipAmmoFactor * gShipTypes[shipData[s].type].weapAmmo[i];
				else
					shipData[s].weap[i].ammo = gShipTypes[shipData[s].type].weapAmmo[i];
			}
			shipData[s].shield = gShipTypes[shipData[s].type].shield;
			return s;
		} else {
			shipData[s].active = false;
			shipData[s].hasWorldSpriteNode = false;
			return -1;
		}
	}
	return -1;
}

// FUNCTION: EVO 0x100664bc
short MissionDudeSpawn(short dude, short syst, short mission)
{
	short s, pick;
	short i, j;
	short roll;
	char dead[26];

	s = GenericRandomShipSpawn(syst, true);
	if (s != -1) {
		roll = Rand(100) + 1;
		pick = -1;
		pick = SelectShipFieldFromDude(&dudeData[dude]);
		if (pick >= 0 && pick <= 3) {
			shipData[s].specialMission = mission;
			shipData[s].dude = dude;
			shipData[s].type = dudeData[dude].shipTypes[pick];
			shipData[s].govt = dudeData[dude].govt;
			shipData[s].boarded = false;
			shipData[s].aiTimer = 0;
			if (dudeData[dude].aiType >= kAIWimpyTrader)
				shipData[s].aiType = dudeData[dude].aiType;
			else
				shipData[s].aiType = gShipTypes[shipData[s].type].inherentAI;
			AISetup(&shipData[s]);
			for (i = 0; i < MaxWeaponTypes; i++) {
				shipData[s].weap[i].count = gShipTypes[shipData[s].type].weapCount[i];
				if ((currentMissions[mission].shipBehav == kMisnBehavAttack || currentMissions[mission].shipBehav == kMisnBehavAttack + kMisnBehavVariant) && gWeapons[i].guidance != kGuidanceBay)
					shipData[s].weap[i].ammo = kShipAmmoFactor * gShipTypes[shipData[s].type].weapAmmo[i];
				else
					shipData[s].weap[i].ammo = gShipTypes[shipData[s].type].weapAmmo[i];
			}
			shipData[s].shield = gShipTypes[shipData[s].type].shield;
			if (currentMissions[mission].shipBehav == kMisnBehavAttack || currentMissions[mission].shipBehav == kMisnBehavAttack + kMisnBehavVariant)
				AIMakeShipAttackPlayer(&shipData[s]);
			return s;
		} else {
			shipData[s].active = false;
			shipData[s].hasWorldSpriteNode = false;
			return -1;
		}
	}
	return -1;
}

// FUNCTION: EVO 0x10066904
void EnterMoreShips(short system)
{
	short m, i, j, k, s, need, count, heading, sum;
	float dist, speed;
	short cum[4];
	FloatPoint offset;
	FloatPoint origin;
	FloatPoint prior;

	s = 0;
	for (m = 0; m < MaxSimultaneousMissions; m++) {
		if (currentMissionObjectives[m].active) {
			if (currentMissions[m].auxShipDelay <= 0 && currentMissions[m].auxShipDude != -1) {
				if (currentMissions[m].auxShipsSpawned < currentMissions[m].auxShipsLeft) {
					need = currentMissions[m].auxShipsLeft - currentMissions[m].auxShipsSpawned;
					if (ValidAuxShipSystem(shipData->curSystem, m) && need > 0) {
						dist = 0.0;
						speed = 50;
						while (speed > 0) {
							dist += speed;
							speed -= kJumpInDecel;
						}
						heading = Rand(kFullCircle);
						offset.x = offset.y = 0;
						Accel(heading, kJumpInDistance + dist, &offset);
						for (k = 0; k < need; k++) {
							s = SpecificDudeSpawn(currentMissions[m].auxShipDude, system);
							if (s != -1) {
								shipData[s].auxMission = m;
								currentMissions[m].auxShipsSpawned++;
								if (!(currentMissions[m].flags & kMisnInfiniteAux))
									currentMissions[m].auxShipsLeft--;
								shipData[s].pos.x = -256 + (offset.x + Rand(512));
								shipData[s].pos.y = -256 + (offset.y + Rand(512));
								dist = 0.0;
								speed = 50;
								while (speed > 0) {
									dist += speed;
									speed -= kJumpInDecel;
								}
								origin.x = origin.y = 0.0;
								shipData[s].heading = CalcInterceptAngle(shipData[s].pos, origin);
								shipData[s].lastStellar = -2;
								shipData[s].prevSystem = -2;
								shipData[s].vel.x = shipData[s].vel.y = 0.0;
								shipData[s].jumpTimer = kJumpWindupArriving;
								shipData[s].jumpTick = TickCount();
								Accel(shipData[s].heading, kJumpInSpeed, &shipData[s].vel);
							}
						}
						if (currentMissions[m].flags & kMisnAutoAbort)
							AutoAbortMission(m);
					}
				}
			}
			s = 0;
			count = 0;
			if (currentMissions[m].shipSyst == kShipSystFollow || shipData->curSystem == currentMissions[m].shipSyst)
				count = currentMissions[m].shipCount;
			if (currentMissions[m].shipBehav >= 9) {
				count = currentMissions[m].shipCount - currentMissions[m].specialShipsJumpedIn;
				if (currentMissions[m].shipSyst != kShipSystFollow) {
					if (shipData->curSystem != currentMissions[m].shipSyst) {
						if (currentMissions[m].specialShipsJumpedIn <= 0)
							count = 0;
					}
				}
			}
			if (count > 0) {
				if (currentMissions[m].shipDelay > 0)
					currentMissions[m].shipDelay--;
				if (currentMissions[m].shipCount > 0 && currentMissions[m].shipDelay == 0) {
					currentMissions[m].shipDelay = -1;
					dist = 0.0;
					speed = 50;
					while (speed > 0) {
						dist += speed;
						speed -= kJumpInDecel;
					}
					if (shipData->prevSystem == -1) {
						heading = Rand(kFullCircle);
					} else {
						origin.x = gSystems[shipData->curSystem].xPos;
						origin.y = gSystems[shipData->curSystem].yPos;
						prior.x = gSystems[shipData->prevSystem].xPos;
						prior.y = gSystems[shipData->prevSystem].yPos;
						heading = CalcInterceptAngle(origin, prior);
					}
					offset.x = offset.y = 0;
					Accel(heading, kJumpInDistance + dist, &offset);
					for (k = 0; k < count; k++) {
						s = MissionDudeSpawn(currentMissions[m].shipDude, system, m);
						if (s != -1) {
							currentMissions[m].specialShipsJumpedIn++;
							shipData[s].pos.x = -256 + (offset.x + Rand(512));
							shipData[s].pos.y = -256 + (offset.y + Rand(512));
							dist = 0.0;
							speed = 50;
							while (speed > 0) {
								dist += speed;
								speed -= kJumpInDecel;
							}
							origin.x = origin.y = 0.0;
							shipData[s].heading = CalcInterceptAngle(shipData[s].pos, origin);
							shipData[s].lastStellar = -2;
							shipData[s].prevSystem = -2;
							shipData[s].vel.x = shipData[s].vel.y = 0.0;
							shipData[s].jumpTimer = kJumpWindupArriving;
							shipData[s].jumpTick = TickCount();
							Accel(shipData[s].heading, kJumpInSpeed, &shipData[s].vel);
						}
					}
					if (currentMissions[m].flags & kMisnAutoAbort)
						AutoAbortMission(m);
				}
			}
			if (currentMissions[m].auxShipDelay > 0)
				currentMissions[m].auxShipDelay--;
		}
	}
	for (k = 1; k < MaxShipsInSystem; k++) {
		/* BUG: s still holds the last spawned ship slot when counting starts */
		if (shipData[k].active && system == shipData[k].curSystem && shipData[k].parentShip != 0)
			s++;
	}
	if (s < gSystems[system].avgShips && Rand(500) == 0) {
		s = Rand(100) + 1;
		dist = -1;
		for (i = 0; i <= 3; i++) {
			cum[i] = 0;
			for (j = 0; j <= i; j++)
				cum[i] += gSystems[system].dudeProbs[j];
		}
		for (i = 3; i >= 0; i = i - 1) {
			if (s <= cum[i]) {
				if (gSystems[system].dudeTypes[i] >= 0)
					dist = i;
			}
		}
		if (dist != -1)
			HyperShipSpawn(system);
	}
	for (i = 0; i <= 3; i++) {
		if (gSystems[system].navDefault[i] != -1 && StellarHasDefenders(gSystems[system].navDefault[i])
				&& stellarData[gSystems[system].navDefault[i]].tribute > 0) {
			s = 0;
			for (k = 1; k < MaxShipsInSystem; k++) {
				if (shipData[k].active && shipData[k].defenseStellar != -1
						&& shipData[k].defenseStellar == gSystems[system].navDefault[i])
					s++;
			}
			m = stellarData[gSystems[system].navDefault[i]].defCount % 10;
			if (s < m) {
				SpawnStellarDefenseShip(gSystems[system].navDefault[i]);
				stellarData[gSystems[system].navDefault[i]].tribute--;
				break;
			}
		}
	}
	if (!gRegistered)
		SpawnNagShip();
	if (hailShip == 0) {
		hailShip = -1;
		sum = 0;
		for (k = 1; k < MaxShipsInSystem; k++) {
			if (shipData[k].active && shipData[k].parentShip == 0 && shipData[k].aiType == kAIEscort
					&& shipData[k].specialMission == -1 && gShipTypes[shipData[k].type].inherentAI <= kAIBraveTrader)
				sum += gShipTypes[shipData[k].type].cargo;
		}
		if (sum >= 200)
			HyperSpawnFleet(shipData->curSystem, 127);
		else if (sum >= 50)
			HyperSpawnFleet(shipData->curSystem, 126);
	} else {
		hailShip--;
	}
}

// FUNCTION: EVO 0x100677f4
void RespawnEscort(ShipObj *esc)
{
	float dist, speed;
	FloatPoint dead1;
	Rect dead2;

	if (esc->parentShip == 0) {
		esc->lastStellar = -2;
		esc->prevSystem = -2;
		esc->thrust = 0.0;
		esc->speedLimit = 0.0;
		esc->fireQueued = false;
		esc->dude = -1;
		esc->persType = -1;
		esc->specialMission = -1;
		esc->curSystem = shipData->curSystem;
		esc->heading = shipData->heading;
		esc->pos.x = shipData->pos.x;
		esc->pos.y = shipData->pos.y;
		Accel(Rand(kFullCircle), Rand(50) + 50, &esc->pos);
		if (shipData->jumpTimer != 0) {
			dist = 0;
			speed = 45;
			while (speed > 0) {
				dist += speed;
				speed -= kJumpInDecel;
			}
			Accel((esc->heading + kHalfCircle) % kFullCircle, dist, &esc->pos);
			esc->vel.x = esc->vel.y = 0.0;
			esc->jumpTimer = kJumpWindupArriving;
			esc->jumpTick = 0;
			Accel(esc->heading, kJumpInSpeed, &esc->vel);
		} else {
			esc->vel.x = esc->vel.y = 0.0;
		}
		AIMakeEscortFlyInForm(esc);
	}
}

// FUNCTION: EVO 0x100679b0
void DisplayComm(StringPtr msg, short count, short font, short size, RGBColor *color, Boolean italicOn, Boolean noBlit)
{
	Str255 buf;

	commCount = count;
	strncpy((char *)chatterText, (char *)msg, 255);
	strncpy((char *)buf, (char *)msg, 254);
	p2cstr(buf);
	SATSetPortBackScreen();
	RGBForeColor(color);
	TextFont(font);
	TextSize(size);
	if (italicOn)
		TextFace(italic);
	BackColor(blackColor);
	TETextBox(buf, strlen((char *)buf), &msgRect, teJustLeft);
	ForeColor(blackColor);
	BackColor(whiteColor);
	if (!noBlit) {
		SATSetPortScreen();
		ForeColor(blackColor);
		BackColor(whiteColor);
		CopyBits(&gSAT.backScreen.port->portBits, &gSAT.offScreen.port->portBits, &msgRect, &msgRect, srcCopy, nil);
		CopyBits(&gSAT.backScreen.port->portBits, &gSAT.wind.port->portBits, &msgRect, &msgRect, srcCopy, nil);
	}
}

// FUNCTION: EVO 0x10067b50
void RedrawComm(Boolean noBlit)
{
	if (commCount > 0)
		DisplayComm(chatterText, commCount, 0, 12, &rgbWhite, false, noBlit);
}

// FUNCTION: EVO 0x10067ba4
Boolean LaunchFighter(ShipObj *launcher, short weapon)
{
	short slot, i, inaccuracy;
	SpritePtr sp;
	ShipObj *ship;

	if (weapon != -1) {
		slot = GenericRandomShipSpawn(launcher->curSystem, 2);
		if (slot != -1) {
			sp = nil;
			sp = SATNewSprite(0, 0, 0, nil);
			if (sp != nil) {
				sp->task = HandleShipDisplay;
				sp->hitTask = RunWeaponHitDispatcher;
				sp->kind = 1;
				sp->mode = 1;
				ship = &shipData[slot];
				sp->appPtr = (Ptr)ship;
				sp->layer = 10;
				shipData[slot].active = true;
				shipData[slot].hasWorldSpriteNode = true;
				shipData[slot].curSystem = launcher->curSystem;
				shipData[slot].pos.x = launcher->pos.x;
				shipData[slot].pos.y = launcher->pos.y;
				shipData[slot].vel.x = launcher->vel.x;
				shipData[slot].vel.y = launcher->vel.y;
				shipData[slot].type = gWeapons[weapon].ammoType - kAmmoShipBase;
				shipData[slot].defenseStellar = -1;
				shipData[slot].isTractored = false;
				shipData[slot].hailed = false;
				shipData[slot].hasAfterburner = AIHasAfterburner(&shipData[slot]);
				shipData[slot].hired = false;
				shipData[slot].auxMission = -1;
				shipData[slot].aiType = kAIFighter;
				shipData[slot].govt = launcher->govt;
				shipData[slot].navTargetKind = kNavOff;
				shipData[slot].jumpTimer = 0;
				shipData[slot].aiState = kStateHiJack;
				shipData[slot].aiSubstate = kManeuverLoJack;
				shipData[slot].aiTimer = 0;
				shipData[slot].boarded = false;
				shipData[slot].grudge = 0;
				shipData[slot].lastStellar = -2;
				shipData[slot].prevSystem = -2;
				shipData[slot].shield = gShipTypes[shipData[slot].type].shield;
				shipData[slot].aiTimer = gWeapons[weapon].lifetime;
				shipData[slot].parentShip = launcher->index;
				shipData[slot].heading = launcher->heading;
				shipData[slot].persType = -1;
				shipData[slot].dude = -1;
				shipData[slot].specialMission = -1;
				if (launcher->index == 0)
					shipData[slot].targetShip = -1;
				else
					shipData[slot].targetShip = launcher->targetShip;
				inaccuracy = gWeapons[weapon].inaccuracy;
				if (inaccuracy > 0)
					shipData[slot].heading += Rand(inaccuracy * 2) - inaccuracy;
				AdjustedAccel(shipData[slot].heading, gWeapons[weapon].speed, &shipData[slot].vel, ShipMaxSpeed(&shipData[slot]));
				for (i = 0; i < MaxWeaponTypes; i++) {
					shipData[slot].weap[i].count = gShipTypes[shipData[slot].type].weapCount[i];
					if (launcher->index == 0)
						shipData[slot].weap[i].ammo = 0.5 * gShipTypes[shipData[slot].type].weapAmmo[i];
					else
						shipData[slot].weap[i].ammo = kShipAmmoFactor * gShipTypes[shipData[slot].type].weapAmmo[i];
				}
				if (shipData[slot].targetShip != -1 && (shipData[slot].parentShip == shipData[slot].targetShip || (shipData[slot].parentShip == shipData[shipData[slot].targetShip].parentShip && shipData[slot].parentShip != -1)))
					shipData[slot].targetShip = -1;
				AISetup(&shipData[slot]);
				return true;
			}
		}
		return false;
	}
	/* BUG: weapon == -1 falls off the end without a return value */
}

// FUNCTION: EVO 0x10068304
void JettisonCargo(Boolean missionCargo)
{
	short i, k, count, m;
	short total, mine, others;
	Boolean failed;
	double hold, cap, share, amount;

	failed = false;
	total = 0;
	for (i = 0; i <= 5; i++)
		total += shipData->cargo[i];
	for (i = 0; i < kMaxJunkTypes; i++) {
		if (junkData[i].held > 0) {
			total += junkData[i].held;
			junkData[i].held = 0;
		}
	}
	others = mine = total;
	if (missionCargo) {
		for (m = 0; m < MaxSimultaneousMissions; m++) {
			if (currentMissionObjectives[m].active && currentMissions[m].cargoPickedUp && currentMissions[m].cargoType != -1) {
				total += currentMissions[m].cargoQty;
				mine += currentMissions[m].cargoQty;
				currentMissions[m].cargoPickedUp = false;
				QuickMissionFailure(m);
				DisplayComm("\pMission failed.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
				failed = true;
				if (total == 0) {
					total++;
					mine++;
				}
			}
		}
	}
	hold = TotalFleetHolds();
	for (i = 0; i < MaxShipsInSystem; i++) {
		if ((shipData[i].parentShip == 0 && shipData[i].aiType == kAIEscort && shipData[i].specialMission == -1 && gShipTypes[shipData[i].type].inherentAI <= kAIBraveTrader) || i == 0) {
			if (i == 0)
				cap = ShipTotalHolds();
			else
				cap = gShipTypes[shipData[i].type].cargo;
			share = cap / hold;
			if (i == 0)
				amount = mine;
			else
				amount = share * others;
			if (amount > 0) {
				count = amount / 5.0;
				if (count <= 0)
					count++;
				if (count > 12)
					count = 12;
				for (k = 0; k < count; k++)
					SpawnBox(i);
				hudRedraw.credits = true;
			}
		}
	}
	for (i = 0; i <= 5; i++)
		shipData->cargo[i] = 0;
	if (total > 0) {
		PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
		if (!failed) {
			if (missionCargo)
				DisplayComm("\pCargo jettisoned.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
			else
				DisplayComm("\pNon-mission cargo jettisoned.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
		}
	}
}

// FUNCTION: EVO 0x100687b0
void SpawnBox(short ship)
{
	short i, slot, n, d, h;
	SpritePtr sp;
	PodObj *pod;

	slot = -1;
	for (i = 0; i < MaxExplosionsOnScreen; i++) {
		if (podData[i].life < -1) {
			slot = i;
			break;
		}
	}
	if (slot != -1) {
		sp = nil;
		sp = SATNewSprite(3, 0, 0, nil);
		if (sp != nil) {
			sp->task = HandleBoxes;
			sp->layer = 2;
			sp->kind = 0;
			sp->mode = 0;
			sp->appLong = 0;
			pod = &podData[slot];
			sp->appPtr = (Ptr)pod;
			pod->pos.x = shipData[ship].pos.x;
			pod->pos.y = shipData[ship].pos.y;
			pod->vel.x = shipData[ship].vel.x;
			pod->vel.y = shipData[ship].vel.y;
			pod->life = Rand(90) + 180;
			pod->curSystem = shipData[ship].curSystem;
			pod->frame = Rand(36);
			n = Rand(4);
			if (n == 0)
				pod->spinDir = -2;
			if (n == 1)
				pod->spinDir = -1;
			if (n == 2)
				pod->spinDir = 1;
			if (n == 3)
				pod->spinDir = 2;
			d = 0.5 * GetSpriteXSize(shipFaces[shipData[ship].type][0]);
			Accel((shipData[ship].heading + kHalfCircle) % kFullCircle, d, &pod->pos);
			n = GetSpriteXSize(boxFaces[0]);
			h = GetSpriteYSize(boxFaces[0]);
			sp->hotRect.left = sp->hotRect.top = 0;
			sp->hotRect.right = n;
			sp->hotRect.bottom = h;
			Accel((shipData[ship].heading + kHalfCircle + Rand(30) - Rand(15)) % kFullCircle, (float)(Rand(40) + 30) / 100.0, &pod->vel);
		}
	}
}

// FUNCTION: EVO 0x10068b10
void SpawnShot(short owner, short target, short weapType)
{
	short s, i, angle, spin, half, w, h;
	Boolean flag;
	SpritePtr sp;
	long dead;

	s = -1;
	for (i = 0; i < MaxShotsOnScreen; i++) {
		if (shotData[i].life < -1) {
			s = i;
			break;
		}
	}
	if (s != -1) {
		flag = false;
		spin = 0;
		sp = nil;
		sp = SATNewSprite(3, 0, 0, nil);
		if (sp != nil) {
			sp->task = HandleShot;
			sp->hitTask = ShotHitTask;
			sp->destructTask = nil;
			sp->appLong = s;
			sp->appPtr = nil;
			sp->face = nil;
			shotData[s].pos.x = shipData[owner].pos.x;
			shotData[s].pos.y = shipData[owner].pos.y;
			shotData[s].vel.x = shipData[owner].vel.x;
			shotData[s].vel.y = shipData[owner].vel.y;
			angle = shipData[owner].heading;
			if (gWeapons[weapType].guidance == kGuidanceTurret || gWeapons[weapType].guidance == kGuidanceFrontQuad || gWeapons[weapType].guidance == kGuidanceRearQuad) {
				shipData[owner].turretMountCycle = (shipData[owner].turretMountCycle + 1) % 4;
				Accel(shipData[owner].heading,
						gShipTypes[shipData[owner].type].turretYDisp[shipData[owner].turretMountCycle], &shotData[s].pos);
				if (target != -1) {
					angle = CalcLeadAngle(&shipData[owner], &shipData[target], weapType);
				} else {
					if (gWeapons[weapType].guidance == kGuidanceFrontQuad)
						angle = shipData[owner].heading;
				}
				sp->layer = 12;
			} else {
				sp->layer = 4;
			}
			if (gWeapons[weapType].guidance == kGuidanceUnguided) {
				spin = WeaponShotSpread(owner, weapType);
				if (spin == 0)
					flag = true;
				else
					flag = false;
			}
			if (gWeapons[weapType].guidance == kGuidanceHoming) {
				spin = 0;
				flag = true;
			}
			if (owner == 0) {
				if (gWeapons[weapType].guidance != kGuidanceTurret && gWeapons[weapType].guidance != kGuidanceFrontQuad && gWeapons[weapType].guidance != kGuidanceRearQuad) {
					shotData[s].vel.x = shipData[owner].vel.x;
					shotData[s].vel.y = shipData[owner].vel.y;
				}
				if (gWeapons[weapType].guidance == kGuidanceFreefall) {
					shotData[s].vel.x *= 0.8f;
					shotData[s].vel.y *= 0.8f;
				}
			}
			shotData[s].weapType = weapType;
			shotData[s].owner = owner;
			shotData[s].target = target;
			shotData[s].fromGuardingEscort = false;
			shotData[s].life = gWeapons[weapType].lifetime / timeScale;
			if (gWeapons[weapType].flags & kWeapStartFirstFrame)
				shotData[s].frame = 0;
			else
				shotData[s].frame = Rand(36);
			shotData[s].damageFalloffSteps = 0;
			if (gWeapons[weapType].decay > 0)
				shotData[s].damageFalloffTimer = 0;
			else
				shotData[s].damageFalloffTimer = -1;
			if (owner != 0) {
				if (AIIsShipAttemptingDisable(&shipData[owner]))
					shotData[s].fromGuardingEscort = true;
			}
			if (owner != 0 && shipData[owner].parentShip != 0)
				shotData[s].life *= 1.5;
			shotData[s].curSystem = shipData[owner].curSystem;
			shotData[s].angle = angle;
			shotData[s].guidanceState = kShotStateNormal;
			w = GetSpriteXSize(weapFaces[gWeapons[weapType].graphic][0]);
			h = GetSpriteYSize(weapFaces[gWeapons[weapType].graphic][0]);
			sp->hotRect.left = sp->hotRect.top = 0;
			sp->hotRect.right = w;
			sp->hotRect.bottom = h;
			if (gWeapons[weapType].guidance == kGuidanceHoming && (gWeapons[weapType].seeker & kSeekInterference)
					&& Rand(100) + 1 <= gSystems[shipData->curSystem].interference)
				shotData[s].guidanceState = kShotStateConfused;
			if (gWeapons[weapType].proxRadius > 0) {
				sp->hotRect.top -= gWeapons[weapType].proxRadius;
				sp->hotRect.left -= gWeapons[weapType].proxRadius;
				sp->hotRect.right += gWeapons[weapType].proxRadius;
				sp->hotRect.bottom += gWeapons[weapType].proxRadius;
			}
			if (spin != 0) {
				Accel((shotData[s].angle + 90) % kFullCircle, spin, &shotData[s].pos);
			} else {
				if (flag) {
					half = 0.5 * GetSpriteXSize(shipFaces[shipData[owner].type][angle / 10]);
					Accel(shotData[s].angle, half, &shotData[s].pos);
				}
			}
			if (gWeapons[shotData[s].weapType].inaccuracy > 0 && gWeapons[shotData[s].weapType].guidance != kGuidanceFreefall)
				shotData[s].angle += Rand(gWeapons[shotData[s].weapType].inaccuracy << 1) - gWeapons[shotData[s].weapType].inaccuracy;
			if (gWeapons[shotData[s].weapType].guidance != kGuidanceRocket || owner != 0)
				Accel(shotData[s].angle, gWeapons[weapType].speed, &shotData[s].vel);
			if (gWeapons[shotData[s].weapType].inaccuracy > 0 && gWeapons[shotData[s].weapType].guidance == kGuidanceFreefall)
				shotData[s].angle += Rand(gWeapons[shotData[s].weapType].inaccuracy << 1) - gWeapons[shotData[s].weapType].inaccuracy;
			if (gWeapons[shotData[s].weapType].flags & kWeapDecoy) {
				sp->kind = 2;
				sp->mode = 5;
			} else {
				sp->kind = -1;
				sp->mode = -1;
			}
			if (gWeapons[shotData[s].weapType].ammoType == kAmmoSelfDestruct && owner != -1) {
				shipData[owner].shield = kDeadMark;
				hudRedraw.target = true;
			}
		}
	}
}

// FUNCTION: EVO 0x100697a0
void SpawnExplod(FloatPoint where, short type, short size, short extra)
{
	SpritePtr sp;

	if (numExplosions < 16) {
		sp = nil;
		sp = SATNewSprite(0, 0, 0, nil);
		if (sp != nil) {
			sp->task = HandleExplods;
			sp->hitTask = nil;
			sp->destructTask = nil;
			sp->kind = 0;
			sp->mode = 0;
			if (type == 0)
				sp->layer = 17;
			else
				sp->layer = 11;
			sp->speed.h = where.x;
			sp->speed.v = where.y;
			sp->mode = size;
			if (size == 0)
				sp->appLong = 7;
			if (size == 1)
				sp->appLong = 14;
			if (size == 2)
				sp->appLong = 28;
			sp->appLong += extra;
			numExplosions++;
		}
	}
}

// FUNCTION: EVO 0x100698e4
void SpawnSmoke(FloatPoint where, short mode, short layer)
{
	SpritePtr sp;

	if (numStreaks < 64 && !settings.noStreaks) {
		sp = nil;
		sp = SATNewSprite(0, 0, 0, nil);
		if (sp != nil) {
			sp->task = HandleSmoke;
			sp->hitTask = nil;
			sp->destructTask = nil;
			sp->mode = 0;
			sp->layer = 4;
			sp->speed.h = where.x;
			sp->speed.v = where.y;
			sp->mode = mode;
			sp->appLong = 0;
			sp->layer = layer;
			numStreaks++;
		}
	}
}

// FUNCTION: EVO 0x100699d8
void CreateAsteroids(void)
{
	short i;
	short w, h;

	if (gSystems[shipData->curSystem].asteroidCount > 0) {
		for (i = 0; i < gSystems[shipData->curSystem].asteroidCount; i++)
			SpawnAsteroid(false);
		w = screenCenter.h + 128;
		h = screenCenter.v + 128;
		for (i = 0; i < MaxAsteroids; i++) {
			asteroidData[i].pos.x = shipData->pos.x + Rand(w) - 0.5 * w;
			asteroidData[i].pos.y = shipData->pos.y + Rand(h) - 0.5 * h;
			asteroidData[i].vel.x = 0.01 * (Rand(400) - 200);
			asteroidData[i].vel.y = 0.01 * (Rand(400) - 200);
		}
	} else {
		killAsteroids = true;
	}
}

// FUNCTION: EVO 0x10069c30
void SpawnAsteroid(Boolean edge)
{
	short i, live, a, b;
	float f;

	if (gSystems[shipData->curSystem].asteroidCount > 0) {
		a = gSystems[shipData->curSystem].asteroidCount;
		live = 0;
		for (i = 0; i < MaxAsteroids; i++) {
			if (asteroidData[i].active || asteroidData[i].spawned)
				live++;
		}
		if (live < a) {
			for (i = 0; i < MaxAsteroids; i++) {
				if (!asteroidData[i].active && !asteroidData[i].spawned) {
					asteroidData[i].active = true;
					asteroidData[i].spawned = false;
					a = screenCenter.h + 128;
					b = screenCenter.v + 128;
					if (edge) {
						if (a > b)
							f = 0.7 * a;
						else
							f = 0.7 * b;
						if (Rand(2) == 0) {
							asteroidData[i].pos.x = shipData->pos.x + f;
							asteroidData[i].vel.x = -0.01 * Rand(200);
						} else {
							asteroidData[i].pos.x = shipData->pos.x - f;
							asteroidData[i].vel.x = 0.01 * Rand(200);
						}
						if (Rand(2) == 0) {
							asteroidData[i].pos.y = shipData->pos.y + f;
							asteroidData[i].vel.y = -0.01 * Rand(200);
						} else {
							asteroidData[i].pos.y = shipData->pos.y - f;
							asteroidData[i].vel.y = 0.01 * Rand(200);
						}
					} else {
						asteroidData[i].pos.x = shipData->pos.x + Rand(a) - 0.5 * a;
						asteroidData[i].pos.y = shipData->pos.y + Rand(b) - 0.5 * b;
						asteroidData[i].vel.x = 0.01 * (Rand(400) - 200);
						asteroidData[i].vel.y = 0.01 * (Rand(400) - 200);
					}
					asteroidData[i].frame = Rand(20);
					if (Rand(6) == 0)
						asteroidData[i].type = 1;
					else
						asteroidData[i].type = 0;
					if (Rand(2) == 0)
						asteroidData[i].direction = 1;
					else
						asteroidData[i].direction = -1;
					if (asteroidData[i].type == 0) {
						asteroidData[i].timer = 350;
						return;
					} else
						asteroidData[i].timer = 1000;
					return;
				}
			}
		}
	}
}

// FUNCTION: EVO 0x1006a284
pascal void AsteroidTask(SpritePtr sp)
{
	short ast, a, b, i;

	if (sp->appLong < 0 || sp->appLong >= MaxAsteroids) {
		sp->face = nil;
		sp->task = nil;
		sp->appLong = -1;
	} else {
		ast = sp->appLong;
		if (!asteroidData[ast].active || asteroidData[ast].timer <= kDeadMark || killAsteroids) {
			asteroidData[ast].active = false;
			asteroidData[ast].spawned = false;
			sp->face = nil;
			sp->task = nil;
			sp->appLong = -1;
		} else {
			asteroidData[ast].pos.x += asteroidData[ast].vel.x;
			asteroidData[ast].pos.y += asteroidData[ast].vel.y;
			if (asteroidData[ast].type == 0) {
				if (frameCount % 4 == 0)
					asteroidData[ast].frame += asteroidData[ast].direction;
				if (asteroidData[ast].frame < 0)
					asteroidData[ast].frame += 20;
				if (asteroidData[ast].frame >= 20)
					asteroidData[ast].frame -= 20;
				sp->face = smallRockFaces[asteroidData[ast].frame];
				a = b = 12;
			} else {
				if (frameCount % 8 == 0)
					asteroidData[ast].frame += asteroidData[ast].direction;
				if (asteroidData[ast].frame < 0)
					asteroidData[ast].frame += 30;
				if (asteroidData[ast].frame >= 30)
					asteroidData[ast].frame -= 30;
				sp->face = bigRockFaces[asteroidData[ast].frame];
				a = b = 16;
			}
			sp->position.h = screenCenter.h + (asteroidData[ast].pos.x - shipData->pos.x) - a;
			sp->position.v = screenCenter.v + (asteroidData[ast].pos.y - shipData->pos.y) - b;
			if (settings.highDetail) {
				if (sp->position.h > screenCenter.h * 2 - a * 2)
					asteroidData[ast].pos.x = a + (shipData->pos.x - screenCenter.h);
				if (sp->position.h < 0)
					asteroidData[ast].pos.x = shipData->pos.x + screenCenter.h - a;
				if (sp->position.v > screenCenter.v * 2 - b * 2)
					asteroidData[ast].pos.y = b + (shipData->pos.y - screenCenter.v);
				if (sp->position.v < 0)
					asteroidData[ast].pos.y = shipData->pos.y + screenCenter.v - b;
			} else {
				if (sp->position.h > screenCenter.h * 2)
					asteroidData[ast].pos.x = shipData->pos.x - screenCenter.h - a;
				if (sp->position.h < -a * 2)
					asteroidData[ast].pos.x = a + (shipData->pos.x + screenCenter.h);
				if (sp->position.v > screenCenter.v * 2)
					asteroidData[ast].pos.y = shipData->pos.y - screenCenter.v - b;
				if (sp->position.v < -b * 2)
					asteroidData[ast].pos.y = b + (shipData->pos.y + screenCenter.v);
			}
		}
	}
}

// FUNCTION: EVO 0x1006aa7c
pascal void AsteroidHitTask(SpritePtr a, SpritePtr b)
{
	short ast, s, r, k, n;
	FloatPoint p;

	if (b->mode == -1) {
		ast = a->appLong;
		s = b->appLong;
		if (!(gWeapons[shotData[s].weapType].seeker & kSeekPassAsteroids)) {
			if (gWeapons[shotData[s].weapType].explodType == kExplodSmall)
				SpawnExplod(shotData[s].pos, -1, 0, 0);
			if (gWeapons[shotData[s].weapType].explodType == kExplodBig) {
				PlaySoundDistance(-1, explodeSnd[1], 6, &shotData[s].pos, &shipData->pos);
				SpawnExplod(shotData[s].pos, -1, 1, 0);
			}
			if (gWeapons[shotData[s].weapType].explodType == kExplodHuge) {
				PlaySoundDistance(-1, explodeSnd[0], 6, &shotData[s].pos, &shipData->pos);
				SpawnExplod(shotData[s].pos, -1, 2, 0);
				r = gWeapons[shotData[s].weapType].blastRadius;
				n = 0.04 * r;
				for (k = 0; k < n; k++) {
					p.x = shotData[s].pos.x + Rand(0.5 * r) - 0.25 * r;
					p.y = shotData[s].pos.y + Rand(0.5 * r) - 0.25 * r;
					SpawnExplod(p, -1, 1, Rand(8) + 4);
				}
				n = 0.16 * r;
				for (k = 0; k < n; k++) {
					p.x = shotData[s].pos.x + Rand(r) - 0.5 * r;
					p.y = shotData[s].pos.y + Rand(r) - 0.5 * r;
					SpawnExplod(p, -1, 0, Rand(16) + 8);
				}
			}
			shotData[s].life = kSlotFree;
			b->task = nil;
			b->appLong = -1;
		}
	}
}

// FUNCTION: EVO 0x1006afb8
short SpawnStellarDefenseShip(short stellar)
{
	short s;
	long dead;

	s = -1;
	if (stellarData[stellar].defenseDude != -1) {
		s = SpecificDudeSpawn(stellarData[stellar].defenseDude, stellarData[stellar].system);
		if (s != -1) {
			shipData[s].defenseStellar = stellar;
			shipData[s].boarded = true;
			shipData[s].aiTimer = 0;
			shipData[s].aiType = kAIWarship;
			shipData[s].specialMission = -1;
			shipData[s].parentShip = -1;
			shipData[s].govt = stellarData[stellar].govt;
			shipData[s].pos.x = stellarData[stellar].pos.h + Rand(80) - 40;
			shipData[s].pos.y = stellarData[stellar].pos.v + Rand(80) - 40;
			shipData[s].heading = Rand(kFullCircle);
			shipData[s].vel.x = shipData[s].vel.y = 0.0;
			Accel(shipData[s].heading, gShipTypes[shipData[s].type].maxSpeed, &shipData[s].vel);
			AIMakeShipAttackPlayer(&shipData[s]);
		}
	}
	return s;
}

// FUNCTION: EVO 0x1006b280
Boolean StellarHasDefenders(short stellar)
{
	short i;

	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && shipData[i].defenseStellar != -1 && stellar == shipData[i].defenseStellar)
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x1006b314
Boolean SufficentTargetedDamage(ShipObj *ship)
{
	if (ship->shield >= 0) {
		if (ship->incomingDamage >= 1.05 * ship->shield)
			return true;
	} else {
		if (ship->incomingDamage >= -1.05 * ship->shield)
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x1006b3e0
void CalculateTargetedDamage(void)
{
	short i, j;

	for (i = 0; i < MaxShipsInSystem; i++) {
		if (shipData[i].active) {
			shipData[i].incomingDamage = 0;
			for (j = 0; j < MaxShotsOnScreen; j++) {
				if (shotData[j].life > 0 && i == shotData[j].target && shotData[j].guidanceState == kShotStateNormal)
					shipData[i].incomingDamage += 0.5 * (gWeapons[shotData[j].weapType].massDmg + gWeapons[shotData[j].weapType].energyDmg);
			}
		}
	}
}

// FUNCTION: EVO 0x1006b564
void MakeEscortsHyperWithParent(ShipObj *leader)
{
	short i;

	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && leader->index == shipData[i].parentShip && shipData[i].defenseStellar == -1) {
			shipData[i].jumpTimer = shipData[shipData[i].parentShip].jumpTimer;
			shipData[i].jumpTick = shipData[shipData[i].parentShip].jumpTick;
			shipData[i].targetShip = -2;
			AIMakeShipJumpWithParent(&shipData[i]);
		}
	}
}

// FUNCTION: EVO 0x1006b6b0
short SpawnEscort(short type, short stellar)
{
	short i, slot;

	slot = GenericRandomShipSpawn(shipData->curSystem, 2);
	if (slot != -1 && type > -1) {
		shipData[slot].type = type;
		shipData[slot].aiType = kAIEscort;
		shipData[slot].parentShip = 0;
		shipData[slot].govt = -1;
		shipData[slot].aggression = 2;
		shipData[slot].shield = gShipTypes[shipData[slot].type].shield;
		shipData[slot].dude = -1;
		shipData[slot].hailed = false;
		shipData[slot].hasAfterburner = AIHasAfterburner(&shipData[slot]);
		shipData[slot].hired = true;
		shipData[slot].specialMission = -1;
		shipData[slot].auxMission = -1;
		if (stellar != -1) {
			shipData[slot].pos.x = stellarData[stellar].pos.h;
			shipData[slot].pos.y = stellarData[stellar].pos.v;
		} else {
			shipData[slot].pos.x = shipData->pos.x;
			shipData[slot].pos.y = shipData->pos.y;
			shipData[slot].heading = shipData->heading;
			Accel(Rand(kFullCircle), Rand(50) + 50, &shipData[slot].pos);
		}
		for (i = 0; i < MaxWeaponTypes; i++) {
			shipData[slot].weap[i].count = gShipTypes[type].weapCount[i];
			shipData[slot].weap[i].ammo = gShipTypes[type].weapAmmo[i];
		}
		AISetup(&shipData[slot]);
		AIMakeEscortFlyInForm(&shipData[slot]);
		return slot;
	}
	return -1;
}

// FUNCTION: EVO 0x1006ba80
void DoEscortLand(ShipObj *leader)
{
	short count;
	short i, j;
	short x;
	Str255 dead;
	Rect dead2;

	count = 0;
	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && leader->index == shipData[i].parentShip && !IsDisabled(&shipData[i])) {
			shipData[i].shield = gShipTypes[shipData[i].type].shield;
			for (j = 0; j < MaxWeaponTypes; j++)
				shipData[i].weap[j].ammo = gShipTypes[shipData[i].type].weapAmmo[j];
			x = 0;
			if (shipData[i].specialMission != -1 && currentMissionObjectives[shipData[i].specialMission].active
					&& (currentMissions[shipData[i].specialMission].shipBehav == kMisnBehavProtect
					|| currentMissions[shipData[i].specialMission].shipBehav == kMisnBehavProtect + kMisnBehavVariant))
				x = 1;
			count++;
		}
	}
	count *= 0.5;
	if (count > 0) {
		for (i = 0; i < count; i++)
			IncrementGameTime();
	}
	if (leader->index == 0)
		DoEscortPayment(true);
}

// FUNCTION: EVO 0x1006bce4
void DoEscortPayment(short ticks)
{
	short i, k;
	long wage;
	Boolean onMission;
	short defected, gone, t;
	Str255 dead;
	Rect dead2;

	gone = 0;
	defected = 0;
	for (t = 0; t < ticks; t++) {
		for (i = 1; i < MaxShipsInSystem; i++) {
			if (shipData[i].active && shipData[i].parentShip == 0 && shipData[i].aiType == kAIEscort && !IsDisabled(&shipData[i])) {
				onMission = false;
				if (shipData[i].specialMission != -1 && currentMissionObjectives[shipData[i].specialMission].active) {
					k = currentMissions[shipData[i].specialMission].shipBehav;
					while (k >= 9)
						k -= 10;
					if (k == 1)
						onMission = true;
				}
				if (!onMission && shipData[i].hired) {
					wage = 0.01 * gShipTypes[shipData[i].type].cost;
					if (shipData->credits >= wage) {
						shipData->credits -= wage;
						hudRedraw.credits = true;
					} else {
						defected++;
						if (shipData[i].aiType == kAIEscort && shipData[i].specialMission == -1 && gShipTypes[shipData[i].type].inherentAI <= kAIBraveTrader)
							DestroyPartialFleetCargo(i);
						hudRedraw.credits = true;
						shipData[i].active = false;
						shipData[i].hasWorldSpriteNode = false;
						shipData[i].parentShip = -1;
						shipData[i].aiType = kAIWimpyTrader;
					}
				}
				gone++;
			}
		}
	}
	if (defected > 0) {
		if (defected == 1)
			strcpy(gDialogText, "Due to lack of pay, one of your escorts has defected.");
		else
			strcpy(gDialogText, "Due to lack of pay, some of your escorts have defected.");
		hudRedraw.credits = true;
		ShowTextDialog(false, false);
		RefreshSAT();
		DrawStatus();
	}
}

// FUNCTION: EVO 0x1006c044
void DoStellarTimePassage(void)
{
	short i;

	for (i = 0; i < MaxStellarObjects; i++) {
		if (stellarData[i].loaded && stellarData[i].dominated) {
			stellarData[i].daysDominated++;
			shipData->credits += stellarData[i].techLevel * 1000;
			hudRedraw.credits = true;
		}
	}
}

// FUNCTION: EVO 0x1006c110
short SpawnPerson(short syst, Boolean flag, short pers)
{
	short count, i, p, s, w;
	Boolean eligible[MaxPersonTypes];
	Str255 dead;

	count = 0;
	if (pers != -1)
		count = 1;
	else {
		for (i = 0; i < MaxPersonTypes - 1; i++) {
			eligible[i] = false;
			if (personData[i].loaded) {
				if (personData[i].aiType >= kAIWimpyTrader) {
					if (personData[i].linkSyst == -1)
						eligible[i] = true;
					if (syst == personData[i].linkSyst)
						eligible[i] = true;
					if (personData[i].linkSyst >= FirstResourceID && personData[i].linkSyst < kLinkGovtBase && syst == personData[i].linkSyst - FirstResourceID)
						eligible[i] = true;
					/* BUG: the range starts at 9999, so linkSyst 9999 matches systems with no government (-1) */
					if (personData[i].linkSyst >= kLinkGovtBase - 1 && personData[i].linkSyst < kLinkAllyBase && personData[i].linkSyst - kLinkGovtBase == gSystems[syst].govt)
						eligible[i] = true;
					if (personData[i].linkSyst >= kLinkAllyBase && personData[i].linkSyst < kLinkNotGovtBase && gSystems[syst].govt >= 0) {
						if (gSystems[syst].govt == govtData[personData[i].linkSyst - kLinkAllyBase].ally)
							eligible[i] = true;
						if (personData[i].linkSyst - kLinkAllyBase == govtData[gSystems[syst].govt].ally)
							eligible[i] = true;
					}
					if (personData[i].linkSyst >= kLinkNotGovtBase && personData[i].linkSyst < kLinkEnemyBase && gSystems[syst].govt >= 0 && personData[i].linkSyst - kLinkNotGovtBase != gSystems[syst].govt)
						eligible[i] = true;
					if (personData[i].linkSyst >= kLinkEnemyBase && personData[i].linkSyst < kLinkLimit && gSystems[syst].govt >= 0) {
						if (gSystems[syst].govt == govtData[personData[i].linkSyst - kLinkEnemyBase].enemy)
							eligible[i] = true;
						if (personData[i].linkSyst - kLinkEnemyBase == govtData[gSystems[syst].govt].enemy)
							eligible[i] = true;
					}
					if (personData[i].govt >= 0 && (govtData[personData[i].govt].flags & kGovtStartDisabled) && flag)
						eligible[i] = false;
					if (personData[i].missionBit != -1) {
						if (personData[i].missionBit >= kNotBitBase) {
							if (missionBit[personData[i].missionBit - kNotBitBase])
								eligible[i] = false;
						} else if (!missionBit[personData[i].missionBit])
							eligible[i] = false;
					}
				}
			}
			if (eligible[i])
				count++;
		}
	}
	if (count > 0) {
		if (pers != -1) {
			p = pers;
			eligible[p] = true;
			personData[p].loaded = true;
		} else
			p = Rand(MaxPersonTypes - 2);
		if (eligible[p]) {
			s = GenericRandomShipSpawn(syst, 2);
			if (s != -1) {
				shipData[s].persType = p;
				shipData[s].type = personData[p].shipType;
				shipData[s].dude = -1;
				shipData[s].govt = personData[p].govt;
				shipData[s].aiType = personData[p].aiType;
				shipData[s].aggression = personData[p].aggress;
				shipData[s].hailed = false;
				shipData[s].hasAfterburner = AIHasAfterburner(&shipData[s]);
				if (personData[p].flags & kPersPodAndAfterburner)
					shipData[s].hasAfterburner = true;
				if (shipData[s].aggression < 1)
					shipData[s].aggression = 1;
				if (shipData[s].aggression >= 3)
					shipData[s].aggression = 4;
				for (w = 0; w < MaxWeaponTypes; w++) {
					shipData[s].weap[w].count = gShipTypes[shipData[s].type].weapCount[w];
					shipData[s].weap[w].ammo = gShipTypes[shipData[s].type].weapAmmo[w];
					shipData[s].weap[w].count += personData[p].weapCount[w];
					shipData[s].weap[w].ammo += personData[p].weapAmmo[w];
				}
				shipData[s].shield = personData[p].shieldMod * gShipTypes[shipData[s].type].shield;
				if (shipData[s].govt != -1 && (govtData[shipData[s].govt].flags & kGovtStartDisabled)) {
					if (gShipTypes[shipData[s].type].flags & kShipDisableAt10)
						shipData[s].shield = -0.85 * gShipTypes[shipData[s].type].armor;
					else
						shipData[s].shield = -0.6 * gShipTypes[shipData[s].type].armor;
					shipData[s].vel.x = shipData[s].vel.y = 0.0;
				}
				AISetup(&shipData[s]);
				if (personData[shipData[s].persType].linkMission != -1)
					RandomizeOneMission(personData[shipData[s].persType].linkMission);
				return s;
			}
		}
	}
	return -1;
}

// FUNCTION: EVO 0x1006cc60
void DoShipCapture(ShipObj *old, Boolean keepAsEscort)
{
	short i, k, s;
	Boolean persistent;

	s = 0;
	s = GenericRandomShipSpawn(shipData->curSystem, 1);
	if (s != -1) {
		if (old->persType != -1)
			personData[old->persType].loaded = false;
		shipData[s].type = shipData->type;
		shipData[s].dude = -1;
		shipData[s].pos = shipData->pos;
		shipData[s].vel = shipData->vel;
		shipData[s].heading = shipData->heading;
		shipData[s].desiredHeading = shipData[s].heading;
		shipData[s].targetStellar = -1;
		shipData[s].targetShip = -1;
		shipData[s].selectedWeapon = -1;
		shipData[s].curSystem = shipData->curSystem;
		shipData[s].govt = -1;
		shipData[s].hired = false;
		shipData[s].hailed = false;
		shipData[s].hasAfterburner = AIHasAfterburner(&shipData[s]);
		shipData[s].auxMission = -1;
		shipData[s].index = s;
		for (i = 0; i <= 5; i++)
			shipData[s].cargo[i] = shipData->cargo[i];
		for (i = 1; i < MaxShipsInSystem; i++) {
			if (shipData[i].active && i != old->index && old->index == shipData[i].parentShip) {
				shipData[i].targetShip = shipData[i].targetStellar = -1;
				shipData[i].grudge = 0;
				shipData[i].parentShip = -1;
				shipData[i].aiType = gShipTypes[shipData[i].type].inherentAI;
			}
		}
		if (keepAsEscort) {
			shipData[s].shield = -gShipTypes[shipData[s].type].armor + 1;
			if (shipData[s].shield >= 0)
				shipData[s].shield = kLowestValue;
			shipData[s].boarded = true;
			shipData[s].hired = false;
			shipData[s].hailed = false;
			shipData[s].hasAfterburner = AIHasAfterburner(&shipData[s]);
			shipData[s].parentShip = 0;
			shipData[s].aiType = kAIEscort;
			DestroyPartialFleetCargo(s);
			shipData[s].parentShip = -1;
			shipData[s].aiType = -1;
		} else {
			shipData[s].shield = 0;
			shipData[s].parentShip = 0;
			shipData[s].fuel = ShipFuelCapacity(&shipData[s]);
			shipData[s].aiType = kAIEscort;
		}
		hudRedraw.credits = true;
		hudRedraw.target = true;
		for (i = 0; i < MaxWeaponTypes; i++) {
			shipData[s].weap[i].count = gShipTypes[shipData[s].type].weapCount[i];
			shipData[s].weap[i].ammo = gShipTypes[shipData[s].type].weapAmmo[i];
		}
		Accel(Rand(kFullCircle), 75, &shipData[s].pos);
		shipData->type = old->type;
		shipData->dude = old->dude;
		shipData->pos = old->pos;
		shipData->vel = old->vel;
		shipData->heading = old->heading;
		shipData->desiredHeading = shipData->heading;
		shipData->targetStellar = -1;
		shipData->targetShip = -1;
		shipData->selectedWeapon = -1;
		for (i = 0; i <= 5; i++)
			shipData->cargo[i] = old->cargo[i];
		shipData->shield = 0;
		shipData->fuel = Rand(ShipFuelCapacity(shipData));
		shipData->aiType = -1;
		shipData->parentShip = -1;
		shipData->govt = -1;
		for (k = 0; k < MaxWeaponTypes; k++) {
			persistent = false;
			for (i = 0; i < MaxOutfitItemTypes; i++) {
				if (gOutfits[i].modType[0] == kOutfModWeapon && k == gOutfits[i].modVal[0]) {
					if (gOutfits[i].persistent) {
						persistent = true;
						break;
					}
				}
				if (gOutfits[i].modType[1] == kOutfModWeapon) {
					if (k == gOutfits[i].modVal[1]) {
						if (gOutfits[i].persistent) {
							persistent = true;
							break;
						}
					}
				}
			}
			if (!persistent) {
				shipData->weap[k].count = 0;
				shipData->weap[k].ammo = 0;
			}
		}
		for (k = 0; k < MaxOutfitItemTypes; k++) {
			if (!gOutfits[k].persistent)
				outfitCount[k] = 0;
		}
		for (i = 0; i < 4; i++) {
			if (gShipTypes[shipData->type].defOutfCount[i] > 0)
				outfitCount[gShipTypes[shipData->type].defOutfID[i]] += gShipTypes[shipData->type].defOutfCount[i];
		}
		SystemInfoToShipStats();
		for (k = 0; k < MaxWeaponTypes; k++) {
			shipData->weap[k].count += gShipTypes[shipData->type].weapCount[k];
			shipData->weap[k].ammo += gShipTypes[shipData->type].weapAmmo[k];
		}
		old->active = false;
		old->hasWorldSpriteNode = false;
		DisplayComm("\pYou retained your old ship as an escort.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
	} else
		DisplayComm("\pYou were unable to retain your old ship as an escort.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
}

// FUNCTION: EVO 0x1006d70c
void DisasterHandler(void)
{
	short i, j;

	for (i = 0; i < MaxDisasters; i++) {
		if (disasterData[i].countdown > 0)
			disasterData[i].countdown--;
		else if (disasterData[i].missionBit != -1) {
			if (disasterData[i].missionBit < kNotBitBase) {
				if (missionBit[disasterData[i].missionBit] && disasterData[i].duration < 0)
					disasterData[i].countdown = 2;
			} else if (disasterData[i].missionBit >= 3 * kNotBitBase && !missionBit[disasterData[i].missionBit - 3 * kNotBitBase] && disasterData[i].duration < 0)
				disasterData[i].countdown = 2;
		} else if (Rand(100) + 1 <= disasterData[i].freq) {
			if (disasterData[i].stellar >= FirstResourceID) {
				if (disasterData[i].missionBit >= kNotBitBase && disasterData[i].missionBit < 2 * kNotBitBase) {
					if (!missionBit[disasterData[i].missionBit - kNotBitBase]) {
						disasterData[i].stellarID = disasterData[i].stellar - FirstResourceID;
						disasterData[i].countdown = disasterData[i].duration;
					}
				} else if (disasterData[i].missionBit >= 2 * kNotBitBase && disasterData[i].missionBit < 3 * kNotBitBase) {
					if (missionBit[disasterData[i].missionBit - 2 * kNotBitBase]) {
						disasterData[i].stellarID = disasterData[i].stellar - FirstResourceID;
						disasterData[i].countdown = disasterData[i].duration;
					}
				} else {
					disasterData[i].stellarID = disasterData[i].stellar - FirstResourceID;
					disasterData[i].countdown = disasterData[i].duration;
				}
			} else if (disasterData[i].stellar == -1) {
				for (j = 0; j < MaxStellarObjects; j++) {
					if (stellarData[j].loaded) {
						if (disasterData[i].missionBit >= kNotBitBase && disasterData[i].missionBit < 2 * kNotBitBase) {
							if (!missionBit[disasterData[i].missionBit - kNotBitBase]) {
								disasterData[i].stellarID = j;
								disasterData[i].countdown = disasterData[i].duration;
							}
						} else if (disasterData[i].missionBit >= 2 * kNotBitBase && disasterData[i].missionBit < 3 * kNotBitBase) {
							if (missionBit[disasterData[i].missionBit - 2 * kNotBitBase]) {
								disasterData[i].stellarID = j;
								disasterData[i].countdown = disasterData[i].duration;
							}
						} else {
							disasterData[i].stellarID = j;
							disasterData[i].countdown = disasterData[i].duration;
						}
						break;
					}
				}
			}
		}
	}
}

// FUNCTION: EVO 0x1006dc58
void DoAutoRefuel(void)
{
	Boolean found;
	short n;
	short i;

	found = false;
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		if ((gOutfits[i].modType[0] == kOutfModAutoRefuel || gOutfits[i].modType[1] == kOutfModAutoRefuel) && outfitCount[i] > 0) {
			found = true;
			break;
		}
	}
	if (found) {
		if (shipData->fuel < ShipFuelCapacity(shipData)) {
			n = ShipFuelCapacity(shipData) - shipData->fuel;
			if (n > shipData->credits)
				n = shipData->credits;
			shipData->fuel += n;
			shipData->credits -= n;
			hudRedraw.fuel = true;
			hudRedraw.credits = true;
		}
	}
}

// FUNCTION: EVO 0x1006ddf8
void SpawnFleet(short syst)
{
	short i, count, pick;
	Boolean eligible[MaxFleets];
	char dead[256];

	count = 0;
	for (i = 0; i < MaxFleets; i++) {
		eligible[i] = false;
		if (fleetData[i].leadShipType != -1) {
			if (fleetData[i].linkSyst == -1)
				eligible[i] = true;
			if (syst == fleetData[i].linkSyst)
				eligible[i] = true;
			if (fleetData[i].linkSyst >= FirstResourceID && fleetData[i].linkSyst < kLinkGovtBase && syst == fleetData[i].linkSyst - FirstResourceID)
				eligible[i] = true;
			if (fleetData[i].linkSyst >= kLinkGovtBase && fleetData[i].linkSyst < kLinkAllyBase && fleetData[i].linkSyst - kLinkGovtBase == gSystems[syst].govt)
				eligible[i] = true;
			if (fleetData[i].linkSyst >= kLinkAllyBase && fleetData[i].linkSyst < kLinkNotGovtBase && gSystems[syst].govt >= 0) {
				if (gSystems[syst].govt == govtData[fleetData[i].linkSyst - kLinkAllyBase].ally)
					eligible[i] = true;
				if (fleetData[i].linkSyst - kLinkAllyBase == govtData[gSystems[syst].govt].ally)
					eligible[i] = true;
			}
			if (fleetData[i].linkSyst >= kLinkNotGovtBase && fleetData[i].linkSyst < kLinkEnemyBase && gSystems[syst].govt >= 0 && fleetData[i].linkSyst - kLinkNotGovtBase != gSystems[syst].govt)
				eligible[i] = true;
			if (fleetData[i].linkSyst >= kLinkEnemyBase && fleetData[i].linkSyst < kLinkLimit && gSystems[syst].govt >= 0) {
				if (gSystems[syst].govt == govtData[fleetData[i].linkSyst - kLinkEnemyBase].enemy)
					eligible[i] = true;
				if (fleetData[i].linkSyst - kLinkEnemyBase == govtData[gSystems[syst].govt].enemy)
					eligible[i] = true;
			}
			if (fleetData[i].missionBit != -1) {
				if (fleetData[i].missionBit < kNotBitBase) {
					if (!missionBit[fleetData[i].missionBit])
						eligible[i] = false;
				} else if (missionBit[fleetData[i].missionBit - kNotBitBase])
					eligible[i] = false;
			}
		}
		if (eligible[i])
			count++;
	}
	if (count > 0) {
		pick = Rand(MaxFleets);
		if (eligible[pick])
			HyperSpawnFleet(syst, pick);
	}
}

// FUNCTION: EVO 0x1006e344
short GenericRandomShipSpawn(short syst, short reserve)
{
	short i, slot, range;

	slot = -1;
	for (i = 1; i < MaxShipsInSystem - abs(reserve); i++) {
		if (!shipData[i].active && !shipData[i].hasWorldSpriteNode) {
			slot = i;
			break;
		}
	}
	if (slot != -1) {
		shipData[slot].active = true;
		shipData[slot].curSystem = syst;
		shipData[slot].dude = -1;
		shipData[slot].type = 0;
		shipData[slot].aiType = kAIWimpyTrader;
		shipData[slot].govt = -1;
		shipData[slot].vel.x = shipData[slot].vel.y = 0;
		shipData[slot].navTargetKind = kNavOff;
		shipData[slot].jumpTimer = 0;
		shipData[slot].aiState = kStateHiJack;
		shipData[slot].aiSubstate = kManeuverLoJack;
		shipData[slot].boarded = false;
		shipData[slot].specialMission = -1;
		shipData[slot].grudge = 0;
		shipData[slot].aiTimer = 0;
		shipData[slot].parentShip = -1;
		shipData[slot].targetShip = -1;
		shipData[slot].targetStellar = -1;
		shipData[slot].deathTimer = 0;
		shipData[slot].skill = RandomSkillLevel(shipData[slot].type);
		shipData[slot].credits = 0;
		shipData[slot].aggression = Rand(3) ^ 2;
		shipData[slot].defenseStellar = -1;
		shipData[slot].isTractored = false;
		shipData[slot].hired = false;
		shipData[slot].hailed = false;
		shipData[slot].auxMission = -1;
		shipData[slot].persType = -1;
		shipData[slot].thrust = 0;
		shipData[slot].speedLimit = 0;
		shipData[slot].fireQueued = false;
		shipData[slot].hasAfterburner = false;
		range = 750;
		shipData[slot].pos.x = Rand(range * 2) - range;
		shipData[slot].pos.y = Rand(range * 2) - range;
	}
	return slot;
}

// FUNCTION: EVO 0x1006e7bc
void HyperSpawnFleet(short syst, short fleet)
{
	short e, w, s, i, j, n;
	float sum, step;
	FloatPoint origin;

	if (fleetData[fleet].leadShipType == -1)
		return;
	if (fleetData[fleet].missionBit != -1) {
		if (fleetData[fleet].missionBit < kNotBitBase) {
			if (!missionBit[fleetData[fleet].missionBit])
				return;
		} else if (missionBit[fleetData[fleet].missionBit - kNotBitBase])
			return;
	}
	s = GenericRandomShipSpawn(syst, 4);
	if (s != -1) {
		shipData[s].type = fleetData[fleet].leadShipType;
		shipData[s].govt = fleetData[fleet].govt;
		shipData[s].aiType = gShipTypes[shipData[s].type].inherentAI;
		shipData[s].shield = gShipTypes[shipData[s].type].shield;
		shipData[s].persType = -1;
		shipData[s].specialMission = -1;
		shipData[s].hasAfterburner = AIHasAfterburner(&shipData[s]);
		AISetup(&shipData[s]);
		AIMakeShipJumpIn(&shipData[s]);
		sum = 0;
		for (step = 50; step > 0; step -= 1.165f)
			sum += step;
		shipData[s].pos.x = shipData[s].pos.y = 0;
		Accel(Rand(kFullCircle), kJumpInDistance + sum, &shipData[s].pos);
		origin.x = origin.y = 0;
		shipData[s].heading = CalcInterceptAngle(shipData[s].pos, origin);
		shipData[s].lastStellar = -2;
		shipData[s].prevSystem = -1;
		shipData[s].vel.x = shipData[s].vel.y = 0;
		shipData[s].jumpTimer = kJumpWindupArriving;
		shipData[s].jumpTick = TickCount();
		Accel(shipData[s].heading, 50, &shipData[s].vel);
		for (i = 0; i < MaxWeaponTypes; i++) {
			shipData[s].weap[i].count = gShipTypes[shipData[s].type].weapCount[i];
			shipData[s].weap[i].ammo = gShipTypes[shipData[s].type].weapAmmo[i];
		}
		for (j = 0; j <= 3; j++) {
			n = fleetData[fleet].escortMin[j] + Rand(fleetData[fleet].escortMax[j] - fleetData[fleet].escortMin[j] + 1);
			if (n > 0) {
				for (i = 0; i < n; i++) {
					e = GenericRandomShipSpawn(syst, 2);
					if (e != -1) {
						shipData[e].aiType = kAIEscort;
						shipData[e].parentShip = s;
						shipData[e].type = fleetData[fleet].escortTypes[j];
						shipData[e].dude = -1;
						shipData[e].persType = -1;
						shipData[e].govt = fleetData[fleet].govt;
						shipData[e].shield = gShipTypes[shipData[e].type].shield;
						AISetup(&shipData[e]);
						for (w = 0; w < MaxWeaponTypes; w++) {
							shipData[e].weap[w].count = gShipTypes[shipData[e].type].weapCount[w];
							shipData[e].weap[w].ammo = gShipTypes[shipData[e].type].weapAmmo[w];
						}
						shipData[e].heading = shipData[s].heading;
						shipData[e].pos.x = -150 + (shipData[s].pos.x + Rand(300));
						shipData[e].pos.y = -150 + (shipData[s].pos.y + Rand(300));
						shipData[e].vel.x = shipData[s].vel.x;
						shipData[e].vel.y = shipData[s].vel.y;
						shipData[e].lastStellar = -2;
						shipData[e].prevSystem = -2;
						shipData[e].jumpTimer = kJumpWindupArriving;
						shipData[e].jumpTick = TickCount();
						AIMakeShipJumpIn(&shipData[e]);
					}
				}
			}
		}
	}
}

// FUNCTION: EVO 0x1006f080
void SpeakVoiceMessage(short id)
{
	pendingVoiceMsg = id;
}

#define kPlugHailQuote		4999
#define kHailQuoteStrList	7101

// FUNCTION: EVO 0x1006f08c
void ShowPersonHailQuote(short quote)
{
	PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
	if (!LoadPluginString(quote + kPlugHailQuote, (StringPtr)missionDescText))
		GetIndString((StringPtr)missionDescText, kHailQuoteStrList, quote);
	p2cstr((StringPtr)missionDescText);
	MungeBriefing(false, -1);
	c2pstr(missionDescText);
	DisplayComm((StringPtr)missionDescText, 420, 0, kCommTextSize, &rgbWhite, false, false);
}

// FUNCTION: EVO 0x1006f150
void SpawnBountyHunter(void)
{
	short count;
	short p;
	Boolean spawn;
	short i;
	Rect dead1, dead2;

	count = 0;
	spawn = false;
	for (i = 0; i < MaxStellarObjects; i++) {
		if (stellarData[i].loaded && stellarData[i].dominated)
			count++;
	}
	if (count == 1 && Rand(10) == 0)
		spawn = true;
	if (count > 1 && Rand(5) == 0)
		spawn = true;
	if (spawn && personData[kPersBountyHunter].loaded) {
		p = SpawnPerson(shipData->curSystem, true, kPersBountyHunter);
		if (p != -1) {
			commCount = -1;
			AIMakeShipAttackPlayer(&shipData[p]);
			speakerShip = p;
			ShowPersonHailQuote(personData[shipData[p].persType].hailQuote);
			speakerShip = -1;
		}
	}
}

// FUNCTION: EVO 0x1006f2c8
void BankRobberyNewsEvent(void)
{
	short odds;
	long loss, millions, thousands;
	char piece[256];
	char dead[256];

	if (!gRegistered && shipData->credits >= 300000) {
		if (shipData->credits >= 1500000)
			odds = 25;
		else if (shipData->credits >= 800000)
			odds = 50;
		else
			odds = 100;
		if (Rand(odds) == 0) {
			loss = shipData->credits * (0.2 + Rand(3) / 10.0);
			if (loss >= 1000) {
				if (loss >= 1000000000)
					loss = 999999999;
				odds = Rand(3);
				if (odds == 0)
					strcpy(gDialogText, "The shareware crusader Cap\325n Hector ");
				if (odds == 1)
					strcpy(gDialogText, "The infamous shareware crusader Cap\325n Hector ");
				if (odds == 2)
					strcpy(gDialogText, "Cap\325n Hector, the notorious shareware crusader, ");
				odds = Rand(2);
				if (odds == 0)
					strcpy(piece, "robbed ");
				if (odds == 1)
					strcpy(piece, "held up ");
				strcat(gDialogText, piece);
				odds = Rand(3);
				if (odds == 0)
					strcpy(piece, "the First Interstellar Bank ");
				if (odds == 1)
					strcpy(piece, "the First Bank of Regulus ");
				if (odds == 2)
					strcpy(piece, "the Arcturan Savings & Loan ");
				strcat(gDialogText, piece);
				strcat(gDialogText, "today, ");
				odds = Rand(2);
				if (odds == 0)
					strcpy(piece, "making off with ");
				if (odds == 1)
					strcpy(piece, "stealing ");
				strcat(gDialogText, piece);
				loss /= 1000;
				loss *= 1000;
				millions = thousands = 0;
				if (loss > 1000000)
					millions = loss / 1000000;
				thousands = (loss - millions * 1000000) / 1000;
				if (millions > 0) {
					NumToString(millions, (StringPtr)piece);
					p2cstr((StringPtr)piece);
					strcat(gDialogText, piece);
					strcat(gDialogText, ",");
					if (thousands < 100)
						strcat(gDialogText, "0");
					if (thousands < 10)
						strcat(gDialogText, "0");
				}
				NumToString(thousands, (StringPtr)piece);
				p2cstr((StringPtr)piece);
				strcat(gDialogText, piece);
				strcat(gDialogText, ",000 of your credits! Authorities have stated that the only way to end this feathered fiend\325s reign of terror is for you to register EV Override.");
				shipData->credits -= loss;
				hudRedraw.credits = true;
				DrawStatus();
				PlaySound(redAlertSnd, 5, kMidVolume, kMidVolume);
				ShowHectorDialog();
				RefreshSAT();
			}
		}
	}
}
