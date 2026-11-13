/* Player.c -- the player's hail key and weapon firing, and the per-frame ship physics and AI step. Notes: Research/notes/Handlers.md */
#include <Memory.h>
#include <Resources.h>
#include <TextUtils.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "evo.h"


extern short			commStellar, blinkTimer;
extern Boolean			playerThreatened, oldPlayerThreatened;
extern Ptr				redAlertSnd, warpUpSnd, warpOutSnd, airlockSnd, klaxonSnd, ejectSnd;
extern SpritePtr		hudBlinkSprite;
extern short			lastMissionStellar, bribeRandom, adviceRandom, gDaysUsed, outfitCount[MaxOutfitItemTypes];
extern Boolean			weapLawFlag, smuggleFlag;
extern float			maxVel[2];

Boolean				warpSoundDone;
Boolean				modKeys[2];
short				lowestPrice;

#define STELLAR		(stellarData[ship->targetStellar])
#define TGT			(shipData[ship->targetShip])
#define kDockWaiting		749	/* dockPermission counts up from here while the stellar answers */
#define kDockGranted		750
#define kDockGiveUp			2048
#define kDockRange			250
#define kWorldEdge			20256.0f	/* the player is held inside this square */


// FUNCTION: EVO 0x10027830
void HandlePlayer(ShipObj *ship)
{
	static long memFree;
	static Size grow;
	static int arrivalAlert;
	static Boolean navKey, routeKey, numKey, jumpCancel, cloakKey, escortKey, targetKey, infoKey, mapKey, jumpKey, weaponKey, landKey, boardKey, spawnKey, nearKey, missionKey, hailKey;
	short i, j, n;
	float f, capf, dist;
	FloatPoint fp2;
	Rect r;
	char s1[256], s2[256], s3[256];
	short adx, sz;
	FloatPoint pc, pb, v;
	FloatPoint fp1;
	float g;
	short fuelCap, mis, k, radius, newSyst, m, dir, cmd;
	short x, y, dx, dy;
	Boolean jumping, canJump;
	Byte station;
	Boolean burning, inhabited, boarded, ok, done;
	Boolean linkOk[kMaxSystemLinks];

	if (showAll && ship->shield < 0)
		ship->shield *= -1;
	if (ship->hasTargetLock == 1 && ship->targetShip != -1
			&& (!shipData[ship->targetShip].hasWorldSpriteNode || IsDying(&shipData[ship->targetShip]))) {
		ship->targetShip = -1;
		hudRedraw.target = true;
	}
	if (!ship->hasWorldSpriteNode) {
		ship->targetShip = -1;
		ship->targetStellar = -1;
		if (ship->deathTimer > -240) {
			ship->deathTimer--;
		} else {
			if (ship->type != kEscapePodShip && strictPlay)
				DeletePilotFile(playerName);
			gAbortGame = true;
		}
		if (QuickKeyCheck(keyMap[kCtlResetPlayer])) {
			ResetPlayer(true);
			ship->targetShip = -1;
			ship->selectedWeapon = -1;
			RedrawStatus();
			RedrawComm(false);
		}
		return;
	}
	if (frameCount % 60 == 0) {
		oldPlayerThreatened = playerThreatened;
		playerThreatened = IsPlayerThreatened();
		if (playerThreatened && !oldPlayerThreatened && gTargetStellar <= 0) {
			PlaySound(redAlertSnd, 5, kMidVolume, kMidVolume);
			if (settings.soundVolume <= 1)
				blinkTimer = 30;
		}
	}
	ship->desiredHeading = ship->heading;
	if (gTargetStellar > 0) {
		AdjustedAccel(ship->heading, ShipAccelRate(ship), &ship->vel, ShipMaxSpeed(ship));
		ship->pos.x += ship->vel.x * timeScale;
		ship->pos.y += ship->vel.y * timeScale;
		gTargetStellar--;
		if (gTargetStellar == 0) {
			FadeScreenOut(16, &rgbBlack);
			SATSetPortScreen();
			ForeColor(blackColor);
			PaintRect(&gSAT.wind.bounds);
			FadeScreenIn(8);
			ShowCursor();
			LoadDescription(gDialogText, 1900);
			ShowTextDialog(false, false);
			HideCursor();
			SATSetPortScreen();
			ForeColor(blackColor);
			PaintRect(&gSAT.wind.bounds);
			FadeScreenOut(8, &rgbBlack);
			for (i = 0; i < MaxSimultaneousMissions; i++) {
				if (currentMissionObjectives[i].active) {
					AbortMission(i);
					currentMissionObjectives[i].active = false;
				}
			}
			ResetPlayer(false);
			DeleteAllComputerShips(true, true);
			SetupShipsInSystem(ship->curSystem);
			for (i = 0; i <= 29; i++)
				IncrementGameTime();
			HandleStellarSystemVisibility();
			strcpy((char *)shipName, (char *)gShipTypes[0].name);
			p2cstr(shipName);
			strcat((char *)shipName, " ");
			for (i = 0; i <= 3; i++) {
				NumToString(Rand(9) + 1, (StringPtr)s1);
				p2cstr((StringPtr)s1);
				strcat((char *)shipName, s1);
			}
			c2pstr((char *)shipName);
			SetMonitorClut(gameClut, false);
			ForeColor(blackColor);
			PaintRect(&gSAT.wind.bounds);
			RedrawStatus();
			RedrawComm(false);
			SATSkip();
			SATRun(true);
			RefreshSAT();
			TwoStepRepaintGameWindow();
			hudRedraw.target = hudRedraw.nav = hudRedraw.ammo = hudRedraw.fuel = hudRedraw.shield = true;
			for (i = 0; i < MaxSystems; i++) {
				if (gSystems[i].govt >= 0)
					legalStatus[i] = govtData[gSystems[i].govt].initialRec;
				else
					legalStatus[i] = 0;
			}
			if (strictPlay) {
				ok = false;
				for (i = 0; i <= 3; i++) {
					if (gSystems[ship->curSystem].navDefault[i] != -1) {
						SavePilotFile(gSystems[ship->curSystem].navDefault[i]);
						ok = true;
						break;
					}
				}
				if (!ok)
					SavePilotFile(0);
			}
			FadeScreenIn(64);
		}
		return;
	}
	if (ship->jumpTimer <= 0 && gTargetStellar <= 0
			&& ((!IsDying(ship) && QuickKeyCheck(kKeyQ) && !modKeys[1] && modKeys[0])
				|| (QuickKeyCheck(kKeyCommand) && QuickKeyCheck(kKeyQ) && !modKeys[0] && !modKeys[1]))) {
		PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
		ShowCursor();
		if (EVYesNoDialog("\pAre you sure you want to quit?")) {
			gQuit = true;
			Bye();
		} else {
			RedrawStatus();
		}
		HideCursor();
		RefreshSAT();
	}
	if (QuickKeyCheck(kKeyCommand))
		modKeys[0] = true;
	else
		modKeys[0] = false;
	if (QuickKeyCheck(kKeyQ))
		modKeys[1] = true;
	else
		modKeys[1] = false;
	if ((QuickKeyCheck(keyMap[kCtlNavOff]) || QuickKeyCheck(keyMap[kCtlNavHyperspace])) && ship->jumpTimer <= 0) {
		if (!navKey) {
			navKey = true;
			hudRedraw.nav = true;
			dockTarget = -1;
			PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
			if (QuickKeyCheck(keyMap[kCtlNavOff])) {
				ship->navTargetKind = kNavOff;
				ship->targetStellar = -1;
			}
			if (QuickKeyCheck(keyMap[kCtlNavHyperspace]) && ship->navTargetKind != kNavHyperspace) {
				ship->navTargetKind = kNavHyperspace;
				ship->targetStellar = -1;
				CueNextHyperRouteDest();
			}
		}
	} else {
		navKey = false;
	}
	if (QuickKeyCheck(keyMap[kCtlNextLink])) {
		if (!routeKey && ship->jumpTimer <= 0 && ship->navTargetKind == kNavHyperspace) {
			PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
			routeKey = true;
			hudRedraw.nav = true;
			n = 0;
			for (i = 0; i < kMaxSystemLinks; i++) {
				linkOk[i] = false;
				if (gSystems[ship->curSystem].links[i] != -1 && gSystems[gSystems[ship->curSystem].links[i]].loaded) {
					linkOk[i] = true;
					n++;
				}
			}
			if (n > 0) {
				do {
					ship->targetStellar++;
					if (ship->targetStellar >= kMaxSystemLinks)
						ship->targetStellar = 0;
					if (ship->targetStellar < 0)
						ship->targetStellar = kMaxSystemLinks - 1;
				} while (linkOk[ship->targetStellar] == false);
			} else {
				ship->targetStellar = -1;
			}
		}
	} else {
		routeKey = false;
	}
	if (ship->jumpTimer <= 0) {
		j = -1;
		if (QuickKeyCheck(kKeyDigit1))
			j = 0;
		if (QuickKeyCheck(kKeyDigit2))
			j = 1;
		if (QuickKeyCheck(kKeyDigit3))
			j = 2;
		if (QuickKeyCheck(kKeyDigit4))
			j = 3;
		for (i = 0; i <= 3; i++) {
			if (QuickKeyCheck(*(keyMap + kCtlNavDefault + i)))
				j = i;
		}
		if (QuickKeyCheck(kKeyF5) || (QuickKeyCheck(kKeyCommand) && QuickKeyCheck(kKeyDigit5)))
			j = 5;
		if (j != -1) {
			if (!numKey) {
				numKey = true;
				if (j != 5)
					n = gSystems[ship->curSystem].navDefault[j];
				else
					n = FindNearestStellar(shipData);
				if (n != -1 && ship->curSystem == stellarData[n].system) {
					PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
					ship->targetStellar = n;
					hudRedraw.nav = true;
					ship->navTargetKind = kNavStellar;
					dockTarget = -1;
				}
			}
		} else {
			numKey = false;
		}
	}
	if (QuickKeyCheck(keyMap[kCtlNextTarget])) {
		if (!targetKey) {
			targetKey = true;
			hudRedraw.target = true;
			if (ship->hasTargetLock == 1) {
				PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
				if (QuickKeyCheck(kKeyShift))
					n = FindPrevShipInSystem(ship->targetShip, ship->curSystem);
				else
					n = FindNextShipInSystem(ship->targetShip, ship->curSystem);
				if (n != ship->targetShip && n != ship->index)
					ship->targetShip = n;
				else
					ship->targetShip = -1;
			}
		}
	} else {
		targetKey = false;
	}
	if (QuickKeyCheck(keyMap[kCtlMap])) {
		if (ship->jumpTimer <= 0 && !fadePending[0] && !fadePending[1] && frameCount >= 0) {
			RefreshSAT();
			PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
			ShowCursor();
			DoSystemMap();
			PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
			HideCursor();
			SATSetPortScreen();
			RedrawStatus();
			RedrawComm(false);
		} else if (!mapKey) {
			mapKey = true;
		}
	} else {
		mapKey = false;
	}
	if (QuickKeyCheck(keyMap[kCtlLand]) && ship->deathTimer <= 0 && gTargetStellar <= 0 && ship->jumpTimer <= 0) {
		if (ship->navTargetKind != kNavStellar || ship->targetStellar == -1) {
			n = FindNearestStellar(ship);
			if (n != -1) {
				hudRedraw.nav = true;
				ship->navTargetKind = kNavStellar;
				ship->targetStellar = n;
			}
		}
		if (!landKey && ship->navTargetKind == kNavStellar && ship->targetStellar != -1) {
			landKey = true;
			if ((STELLAR.flags & kSpobCanLand) && ship->curSystem == STELLAR.system && STELLAR.loaded) {
				if (gInvertedColors) {
					DisplayComm("\pDisengage cloaking device first.", 250, 0, kCommTextSize, &rgbWhite, false, false);
					PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
				} else if (!IsDying(ship)) {
					station = STELLAR.flags & kSpobStation;
					inhabited = !(STELLAR.flags & kSpobUninhabited);
					if (inhabited == false) {
						dockPermission = kDockGranted;
						ship->aiTimer = 0;
						dockTarget = ship->targetStellar;
						DisplayComm("\pNo response.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
					}
					if (dockTarget == ship->targetStellar) {
						v.x = FloatAbs(ship->vel.x);
						v.y = FloatAbs(ship->vel.y);
						if (dockPermission >= kDockGranted && ship->aiTimer <= 0 && v.x <= 0.75 && v.y <= 0.75) {
							n = abs((short)(ship->pos.x - STELLAR.pos.h));
							k = abs((short)(ship->pos.y - STELLAR.pos.v));
							if (spobFaces[STELLAR.type] != 0)
								radius = 1.75 * GetSpriteYSize(spobFaces[STELLAR.type]);
							else
								radius = 75;
							if (n < radius && k < radius) {
								if (inhabited)
									DoAutoRefuel();
								else
									PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
								commCount = 0;
								hudBlinkSprite->face = nil;
								blinkTimer = 0;
								HandleComm();
								RefreshSAT();
								DrawStatus();
								KillVoiceSound();
								ShowCursor();
								cursorHidden = false;
								SATSetPortScreen();
								hudRedraw.credits = true;
								DrawStatus();
								SATSetPortScreen();
								DoPortDialog(&STELLAR, ship->targetStellar);
								BankRobberyNewsEvent();
								SATSetPortScreen();
								RedrawStatus();
								ship->vel.x = ship->vel.y = 0;
								if (ship->targetStellar != -1) {
									ship->pos.x = STELLAR.pos.h;
									ship->pos.y = STELLAR.pos.v;
								} else {
									ship->pos.x = ship->pos.y = 0.0;
								}
								ship->shield = ShipShieldCapacity(ship);
								ship->targetShip = -1;
								hudRedraw.radar = true;
								hudRedraw.target = true;
								hudRedraw.ammo = true;
								hudRedraw.nav = true;
								hudRedraw.shield = true;
								hudRedraw.credits = true;
								IncrementGameTime();
								HandleGlobalPriceFlux();
								DoEscortLand(ship);
								if (gSystems[ship->curSystem].visited < 2)
									gSystems[ship->curSystem].visited = 2;
								ExploreNebulaeFromSystem(ship->curSystem);
								for (i = 0; i < MaxSimultaneousMissions; i++) {
									if (currentMissionObjectives[i].active) {
										if (currentMissions[i].flags & kMisnInfiniteAux)
											currentMissions[i].auxShipsLeft = currentMissions[i].auxShipCount;
										currentMissions[i].auxShipDelay = Rand(kMissionSpawnDelay) + kMissionSpawnDelay;
										currentMissions[i].auxShipsSpawned = 0;
									}
								}
								HideCursor();
								cursorHidden = true;
								if (lowestPrice > 2) {
									n = Rand(5);
									if (n == 0)
										strncpy(s1, "Launching from ", 25);
									if (n == 1)
										strncpy(s1, "Blasting off from ", 25);
									if (n == 2)
										strncpy(s1, "Taking off from ", 25);
									if (n == 3)
										strncpy(s1, "Leaving ", 25);
									if (n == 4)
										strncpy(s1, "Departing ", 25);
									strncpy(s2, (char *)stellarData[dockTarget].name, 63);
									p2cstr((StringPtr)s2);
									strcat(s1, s2);
									strcat(s1, " on ");
									DateToLongString(gameTime, s2);
									strcat(s1, s2);
									strcat(s1, ".");
									c2pstr(s1);
									DisplayComm((StringPtr)s1, kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
								} else {
									lowestPrice = -1;
								}
								DeleteAllComputerShips(false, true);
								SavePilotFile(ship->targetStellar);
								dockTarget = -1;
								dockPermission = 0;
								ship->heading = Rand(kFullCircle);
								ship->targetShip = -1;
								hailShip = Rand(30) + 30;
								SetupShipsInSystem(ship->curSystem);
								HandleStellarSystemVisibility();
								ship->targetStellar = -1;
								CueNextHyperRouteDest();
								SpawnBountyHunter();
								killShots = true;
								killBoxes = true;
								killExplods = true;
								killSmoke = true;
								killAsteroids = true;
								frameCount = -15;
							} else {
								PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
								if (station)
									DisplayComm("\pYou\325re too far away to dock at this station.", 360, 0, kCommTextSize, &rgbWhite, false, false);
								else
									DisplayComm("\pYou\325re too far away to land on this planet.", 360, 0, kCommTextSize, &rgbWhite, false, false);
							}
						} else {
							PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
							n = abs((short)(ship->pos.x - STELLAR.pos.h));
							k = abs((short)(ship->pos.y - STELLAR.pos.v));
							if (spobFaces[STELLAR.type] != 0)
								radius = 1.75 * GetSpriteYSize(spobFaces[STELLAR.type]);
							else
								radius = 75;
							if (n >= radius || k >= radius) {
								if (station)
									DisplayComm("\pYou\325re too far away to dock at this station.", 360, 0, kCommTextSize, &rgbWhite, false, false);
								else
									DisplayComm("\pYou\325re too far away to land on this planet.", 360, 0, kCommTextSize, &rgbWhite, false, false);
							} else {
								if (v.x > 0.75 || v.y > 0.75) {
									if (station)
										DisplayComm("\pYou\325re moving too quickly to dock at this station.", 360, 0, kCommTextSize, &rgbWhite, false, false);
									else
										DisplayComm("\pYou\325re moving too quickly to land on this planet.", 360, 0, kCommTextSize, &rgbWhite, false, false);
								}
							}
						}
					} else {
						dockTarget = ship->targetStellar;
						n = abs((short)(ship->pos.x - stellarData[dockTarget].pos.h));
						k = abs((short)(ship->pos.y - stellarData[dockTarget].pos.v));
						ok = false;
						if (legalStatus[ship->curSystem] >= stellarData[dockTarget].minStatus)
							ok = true;
						if (stellarData[dockTarget].dominated != false)
							ok = true;
						if (dockPermission >= kDockGranted)
							ok = true;
						if (!ok) {
							for (i = 0; i < MaxSimultaneousMissions; i++) {
								if (currentMissionObjectives[i].active) {
									if (shipData->targetStellar == currentMissions[i].travelStellar)
										ok = true;
									if (shipData->targetStellar == currentMissions[i].returnStellar)
										ok = true;
								}
							}
						}
						if (ok) {
							if (n < kDockRange && k < kDockRange) {
								dockPermission = kDockWaiting;
							} else {
								dockPermission = 0;
								PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
								if (stellarData[dockTarget].flags & kSpobStation) {
									if (Rand(3) == 0) {
										strncpy(s1, (char *)stellarData[shipData->targetStellar].name, 31);
										p2cstr((StringPtr)s1);
										strcat(s1, " dockmaster reads you");
									} else {
										strcpy(s1, "Docking request received");
									}
								} else {
									if (Rand(3) == 0) {
										strncpy(s1, (char *)stellarData[shipData->targetStellar].name, 31);
										p2cstr((StringPtr)s1);
										strcat(s1, " traffic control reads you");
									} else {
										strcpy(s1, "Landing request received");
									}
								}
								if (Rand(2) == 0) {
									strncpy(s2, (char *)shipName, 64);
									p2cstr((StringPtr)s2);
									strcat(s1, ", ");
									strcat(s1, s2);
								}
								strcat(s1, ". Begin initial approach.");
								c2pstr(s1);
								DisplayComm((StringPtr)s1, 250, 0, kCommTextSize, &rgbWhite, false, false);
							}
						} else {
							PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
							if (stellarData[dockTarget].flags & kSpobStation)
								strcpy(s1, (char *)"\pDocking request denied.");
							else
								strcpy(s1, (char *)"\pLanding request denied.");
							DisplayComm((StringPtr)s1, 250, 0, kCommTextSize, &rgbWhite, false, false);
							dockTarget = -1;
						}
					}
				}
			} else {
				strcpy(s1, "Your ship is unable to ");
				if (STELLAR.flags & kSpobStation)
					strcat(s1, "dock at ");
				else
					strcat(s1, "land on ");
				strcpy(s2, (char *)STELLAR.name);
				p2cstr((StringPtr)s2);
				strcat(s1, s2);
				strcat(s1, ". The ");
				if (STELLAR.flags & kSpobStation)
					strcat(s1, "station\325s hull integrity is too unstable.");
				else
					strcat(s1, "planet\325s environment is too hostile.");
				c2pstr(s1);
				DisplayComm((StringPtr)s1, 250, 0, kCommTextSize, &rgbWhite, false, false);
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
			}
		}
	} else {
		landKey = false;
	}
	if (dockTarget != -1) {
		if (ship->curSystem == stellarData[dockTarget].system) {
			if (dockPermission > -1) {
				inhabited = !(stellarData[dockTarget].flags & kSpobUninhabited);
				ok = false;
				if (stellarData[dockTarget].govt == -1)
					ok = true;
				else if (stellarData[dockTarget].govt != -1 && legalStatus[ship->curSystem] >= stellarData[dockTarget].minStatus)
					ok = true;
				if (stellarData[dockTarget].dominated != false)
					ok = true;
				if (dockPermission >= kDockGranted)
					ok = true;
				if (!ok) {
					for (i = 0; i < MaxSimultaneousMissions; i++) {
						if (currentMissionObjectives[i].active) {
							if (shipData->targetStellar == currentMissions[i].travelStellar)
								ok = true;
							if (shipData->targetStellar == currentMissions[i].returnStellar)
								ok = true;
						}
					}
				}
				if (inhabited && ok) {
					if (dockPermission >= kDockWaiting)
						dockPermission++;
					n = abs((short)(ship->pos.x - stellarData[shipData->targetStellar].pos.h));
					k = abs((short)(ship->pos.y - stellarData[shipData->targetStellar].pos.v));
					if (n < kDockRange && k < kDockRange && dockPermission < kDockGranted)
						dockPermission = kDockGranted;
					if (dockPermission == kDockGranted) {
						PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
						if (stellarData[shipData->targetStellar].flags & kSpobStation) {
							j = Rand(3);
							if (j == 0) {
								strcpy(s1, "Cleared to dock, ");
								strncpy(s2, (char *)shipName, 64);
								p2cstr((StringPtr)s2);
								strcat(s1, s2);
								strcat(s1, ". ");
							} else if (j == 1) {
								strncpy(s1, (char *)shipName, 64);
								p2cstr((StringPtr)s1);
								strcat(s1, ", you\325re cleared to dock. ");
							} else {
								strcpy(s1, "You are cleared to dock. ");
							}
						} else {
							j = Rand(3);
							if (j == 0) {
								strcpy(s1, "Cleared to land, ");
								strncpy(s2, (char *)shipName, 64);
								p2cstr((StringPtr)s2);
								strcat(s1, s2);
								strcat(s1, ". ");
							} else if (j == 1) {
								strncpy(s1, (char *)shipName, 64);
								p2cstr((StringPtr)s1);
								strcat(s1, ", you\325re cleared to land. ");
							} else {
								strcpy(s1, "You are cleared to land. ");
							}
						}
						if (Rand(2) == 0) {
							strcpy(s2, "Commence final approach.");
						} else {
							strcpy(s2, "Welcome to ");
							strncpy(s3, (char *)stellarData[shipData->targetStellar].name, 31);
							p2cstr((StringPtr)s3);
							strcat(s2, s3);
							strcat(s2, ". ");
						}
						strcat(s1, s2);
						c2pstr(s1);
						DisplayComm((StringPtr)s1, 250, 0, kCommTextSize, &rgbWhite, false, false);
					}
					if (dockPermission >= kDockGiveUp) {
						dockPermission = -1;
						dockTarget = -1;
					}
				} else if (ok) {
					if (dockPermission >= kDockWaiting)
						dockPermission++;
					n = abs((short)(ship->pos.x - stellarData[dockTarget].pos.h));
					k = abs((short)(ship->pos.y - stellarData[dockTarget].pos.v));
					if (n < kDockRange && k < kDockRange && dockPermission < kDockGranted)
						dockPermission = kDockGranted;
					if (dockPermission == kDockGranted && landKey) {
						PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
						DisplayComm("\pNo response.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
					}
					if (dockPermission >= kDockGiveUp) {
						dockPermission = -1;
						dockTarget = -1;
					}
				}
			}
		} else {
			dockTarget = -1;
			dockPermission = -1;
		}
	}
	if (QuickKeyCheck(keyMap[kCtlBoard])) {
		if (!boardKey && ship->targetShip != -1 && !gInvertedColors) {
			boardKey = true;
			boarded = false;
			if (!TGT.boarded && IsDisabled(&TGT) && TGT.active && ship->curSystem == TGT.curSystem
					&& TGT.persType != kPersAmbrosiaMascot && !IsDying(ship)) {
				if (FloatAbs(TGT.vel.x - ship->vel.x) <= 0.5 && FloatAbs(TGT.vel.y - ship->vel.y) <= 0.5) {
					if (FloatAbs(TGT.pos.x - ship->pos.x) <= (float)(0.5 * GetSpriteXSize(shipFaces[TGT.type][TGT.heading / 10]))
							&& FloatAbs(TGT.pos.y - ship->pos.y) <= (float)(0.5 * GetSpriteYSize(shipFaces[TGT.type][TGT.heading / 10]))) {
						if (abs(ship->heading - TGT.heading) <= 30 || abs(ship->heading - (TGT.heading + kHalfCircle) % kFullCircle) <= 30) {
							if (TGT.specialMission != -1) {
								mis = TGT.specialMission;
								if (currentMissions[mis].pickupMode == kPickupOnBoarding && currentMissionObjectives[mis].active) {
									ShowCursor();
									ok = DoMissionCargoPickup(currentMissions[mis].cargoType, currentMissions[mis].cargoQty);
									HideCursor();
									if (ok) {
										if (TGT.specialMission != -1 && currentMissionObjectives[TGT.specialMission].active)
											currentMissions[TGT.specialMission].specialShipsBoarded++;
										TGT.boarded = true;
										currentMissions[mis].cargoPickedUp = true;
										hudRedraw.credits = true;
										strcpy(s1, "You retreived the ");
										strcpy(s2, lcCargoName[currentMissions[mis].cargoType]);
										p2cstr((StringPtr)s2);
										strcat(s1, s2);
										strcat(s1, " from this ship.");
										c2pstr(s1);
										DisplayComm((StringPtr)s1, 250, 0, kCommTextSize, &rgbWhite, false, false);
										PlaySound(beepSnd[kBeepMessage], 8, kMidVolume, kMidVolume);
									} else {
										boarded = true;
									}
								} else {
									ok = true;
								}
							} else {
								ok = true;
							}
							if (ok) {
								if (!boarded)
									TGT.boarded = true;
								ship->vel.x = TGT.vel.x;
								ship->vel.y = TGT.vel.y;
								k = 0;
								if (TGT.specialMission == -1)
									k = 1;
								else if (currentMissionObjectives[TGT.specialMission].active
										&& currentMissionObjectives[TGT.specialMission].failed)
									k = 1;
								if (k == 1 && TGT.persType != kPersAmbrosiaMascot && TGT.persType != kPersBountyHunter) {
									SlapWithPenalty(shipData->curSystem, TGT.govt, 2, TGT.specialMission);
									if (TGT.persType != -1) {
										if (personData[TGT.persType].linkMission != -1) {
											if (personData[TGT.persType].flags & kPersOfferOnBoard) {
												drawGate = true;
												speakerShip = ship->targetShip;
												if (IsMissionAvailable(personData[TGT.persType].linkMission)) {
													if (cursorHidden)
														ShowCursor();
													PlaySound(airlockSnd, 8, kMidVolume, kMidVolume);
													RunSingleMissionDialog(personData[TGT.persType].linkMission);
													if (cursorHidden)
														HideCursor();
													SATSetPortScreen();
													r = gSAT.wind.bounds;
													r.right -= kStatusPanelWidth;
													ForeColor(blackColor);
													PaintRect(&r);
													RedrawComm(false);
													if (personData[TGT.persType].flags & kPersDeactivate)
														personData[TGT.persType].loaded = false;
												} else {
													PlaySound(airlockSnd, 8, kMidVolume, kMidVolume);
													DoPlunderDialog();
												}
												drawGate = false;
												speakerShip = -1;
											} else {
												if (CanBuyFighter(TGT.type, -1, -1)) {
													DisplayComm("\pYou added this ship to your fighter bay.", 250, 0, kCommTextSize, &rgbWhite, false, false);
													PlaySound(beepSnd[kBeepMessage], 8, kMidVolume, kMidVolume);
													TGT.aiType = kAIFighter;
													TGT.parentShip = 0;
													TGT.shield = 0;
													TGT.targetStellar = -1;
													AIMakeFighterReturn(&TGT);
												} else {
													PlaySound(airlockSnd, 8, kMidVolume, kMidVolume);
													DoPlunderDialog();
												}
											}
										} else {
											if (CanBuyFighter(TGT.type, -1, -1)) {
												DisplayComm("\pYou added this ship to your fighter bay.", 250, 0, kCommTextSize, &rgbWhite, false, false);
												PlaySound(beepSnd[kBeepMessage], 8, kMidVolume, kMidVolume);
												TGT.aiType = kAIFighter;
												TGT.parentShip = 0;
												TGT.shield = 0;
												TGT.targetStellar = -1;
												AIMakeFighterReturn(&TGT);
											} else {
												PlaySound(airlockSnd, 8, kMidVolume, kMidVolume);
												DoPlunderDialog();
											}
										}
									} else {
										if (CanBuyFighter(TGT.type, -1, -1)) {
											DisplayComm("\pYou added this ship to your fighter bay.", 250, 0, kCommTextSize, &rgbWhite, false, false);
											PlaySound(beepSnd[kBeepMessage], 8, kMidVolume, kMidVolume);
											TGT.aiType = kAIFighter;
											TGT.parentShip = 0;
											TGT.shield = 0;
											TGT.targetStellar = -1;
											AIMakeFighterReturn(&TGT);
										} else {
											PlaySound(airlockSnd, 8, kMidVolume, kMidVolume);
											DoPlunderDialog();
										}
									}
									DoGoodSamaritan(&TGT, ship);
								} else if (TGT.specialMission == -1) {
									PlaySound(airlockSnd, 8, kMidVolume, kMidVolume);
									DoPlunderDialog();
								}
							} else {
								PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
								DisplayComm("\pYou can\325t board this ship.", 360, 0, kCommTextSize, &rgbWhite, false, false);
							}
						}
					} else {
						PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
						DisplayComm("\pYou\325re not close enough to board this ship.", 360, 0, kCommTextSize, &rgbWhite, false, false);
					}
				} else {
					PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
					DisplayComm("\pYou\325re moving too fast to board this ship.", 360, 0, kCommTextSize, &rgbWhite, false, false);
				}
			} else {
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
				DisplayComm("\pYou can\325t board this ship.", 360, 0, kCommTextSize, &rgbWhite, false, false);
			}
		}
	} else {
		boardKey = false;
	}
	if (QuickKeyCheck(keyMap[kCtlNearestTarget])) {
		if (!nearKey) {
			if (QuickKeyCheck(kKeyOption))
				n = FindNearestShipToPlayer();
			else
				n = FindNearestThreatToPlayer();
			nearKey = true;
			if (n != -1 && ship->hasTargetLock == 1 && n != ship->targetShip) {
				PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
				hudRedraw.target = true;
				ship->hasTargetLock = 1;
				ship->targetShip = n;
			} else {
				if (n == -1)
					PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
			}
		}
	} else {
		nearKey = false;
	}
	if (ship->jumpTimer <= 0 && QuickKeyCheck(keyMap[kCtlFirePrimary])
			&& ship->aiTimer <= 0 && !gInvertedColors) {
		for (i = 0; i < MaxWeaponTypes; i++) {
			if (ship->weap[i].count > 0 && !(gWeapons[i].flags & kWeapSecondTrigger))
				FirePlayerWeapon(i);
		}
	}
	if (ship->jumpTimer <= 0 && QuickKeyCheck(keyMap[kCtlFireSecondary]) && ship->selectedWeapon != -1
			&& ship->aiTimer <= 0 && !gInvertedColors)
		FirePlayerWeapon(ship->selectedWeapon);
	if (QuickKeyCheck(keyMap[kCtlNextWeapon])) {
		if (!weaponKey) {
			weaponKey = true;
			hudRedraw.ammo = true;
			PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
			n = 0;
			for (i = 0; i < MaxWeaponTypes; i++) {
				if (ship->weap[i].count > 0 && (gWeapons[i].flags & kWeapSecondTrigger))
					n++;
			}
			if (n > 0) {
				if (QuickKeyCheck(kKeyOption))
					k = -1;
				else
					k = 1;
				ok = false;
				while (!ok) {
					ship->selectedWeapon += k;
					while (ship->selectedWeapon >= MaxWeaponTypes)
						ship->selectedWeapon -= MaxWeaponTypes;
					while (ship->selectedWeapon < 0)
						ship->selectedWeapon += MaxWeaponTypes;
					if (ship->selectedWeapon >= 0 && ship->selectedWeapon < MaxWeaponTypes) {
						if (ship->weap[ship->selectedWeapon].count > 0
								&& (gWeapons[ship->selectedWeapon].flags & kWeapSecondTrigger))
							ok = true;
					} else {
						ok = false;
					}
				}
			}
		}
	} else {
		weaponKey = false;
	}
	if (QuickKeyCheck(keyMap[kCtlClearWeapon]) && ship->selectedWeapon != -1) {
		ship->selectedWeapon = -1;
		hudRedraw.ammo = true;
		PlaySound(beepSnd[kBeepClose], 1, kMidVolume, kMidVolume);
	}
	for (i = 0; i < MaxWeaponTypes; i++) {
		if (ship->weap[i].reload > 0)
			ship->weap[i].reload -= 1.0f;
		else
			ship->weap[i].reload = 0.0f;
	}
	if (QuickKeyCheck(keyMap[kCtlFaceTarget]) && ship->jumpTimer <= 0) {
		if (ship->targetShip != -1) {
			if (ship->targetStellar == -1) {
				ship->desiredHeading = CalcInterceptAngle(ship->pos, TGT.pos);
			} else {
				if (QuickKeyCheck(kKeyOption)) {
					ship->desiredHeading = CalcInterceptAngle(ship->pos, XYShortToFloat(stellarData[ship->targetStellar].pos));
				} else {
					ship->desiredHeading = CalcInterceptAngle(ship->pos, TGT.pos);
				}
			}
		} else if (ship->targetStellar != -1) {
			ship->desiredHeading = CalcInterceptAngle(ship->pos, XYShortToFloat(stellarData[ship->targetStellar].pos));
		}
	}
	if (ship->navTargetKind == kNavHyperspace) {
		dockTarget = -1;
		if (ship->targetStellar != -1 && ship->jumpTimer > 0
				&& abs((short)ship->vel.x) < 2 && abs((short)ship->vel.y) < 2) {
			pc.x = gSystems[ship->curSystem].xPos;
			pc.y = gSystems[ship->curSystem].yPos;
			j = gSystems[ship->curSystem].links[ship->targetStellar];
			pb.x = gSystems[j].xPos;
			pb.y = gSystems[j].yPos;
			ship->desiredHeading = CalcInterceptAngle(pc, pb);
		}
	}
	jumping = false;
	if (QuickKeyCheck(keyMap[kCtlJump]) || ship->jumpTimer > 0) {
		if (ship->navTargetKind == kNavHyperspace && ship->targetStellar != -1 && ship->jumpTimer != kJumpWindupArriving && ship->fuel >= 100) {
			canJump = true;
			for (i = 0; i <= 3; i++) {
				if (gSystems[ship->curSystem].navDefault[i] != -1) {
					v.x = v.y = 0.0;
					dist = ShipHyperSafeDist(ship);
					if (FloatAbs(XYFloatSquaredDist(&v, &ship->pos)) <= dist)
						canJump = false;
				}
			}
			if (canJump || ship->jumpTimer > 0) {
				if (abs((short)ship->vel.x) < 2 && abs((short)ship->vel.y) < 2) {
					jumping = true;
					if (ship->jumpTimer <= 1) {
						ship->jumpTimer = 2;
						ship->jumpTick = TickCount();
						jumpCancel = false;
						if (gInvertedColors)
							DoShipUncloak();
						hudRedraw.nav = true;
						lowestPrice = kShortMax;
						if (CountMatchingSoundVoices((long)warpUpSnd) == 0) {
							AdjustWarpSoundSpeed(gShipTypes[ship->type].jumpSpeedScale);
							EnqueueSoundVoice(&alarmRequest);
							warpSoundDone = false;
						}
					}
				} else {
					ship->jumpTick = TickCount();
					jumpCancel = false;
					if (gInvertedColors)
						DoShipUncloak();
					ship->jumpTimer = 1;
					pc.x = pc.y = 0.0;
					pb.x = 100.0 * ship->vel.x;
					pb.y = 100.0 * ship->vel.y;
					ship->desiredHeading = CalcInterceptAngle(pb, pc);
					if (abs(ship->desiredHeading - ship->heading) < ShipTurnRate(ship) + 1)
						Accel(ship->heading, ShipAccelRate(ship), &ship->vel);
				}
			} else {
				if (!jumpKey) {
					PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
					DisplayComm("\pCan\325t initiate hyperspace jump - not yet far enough away from system center.", 360, 0, kCommTextSize, &rgbWhite, false, false);
				} else {
					ship->jumpTimer = 0;
				}
			}
		} else {
			if (!jumpKey) {
				if (lowestPrice <= 2) {
					if (ship->navTargetKind != kNavHyperspace || ship->targetStellar == -1) {
						PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
						DisplayComm("\pYou have to select a destination before you can start a hyperspace jump.", 360, 0, kCommTextSize, &rgbWhite, false, false);
					}
				} else if (ship->navTargetKind == kNavHyperspace && ship->targetStellar != -1 && ship->jumpTimer != kJumpWindupArriving && ship->fuel < 100) {
					PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
					DisplayComm("\pInsufficent fuel for hyperspace jump.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
				}
			}
		}
		jumpKey = true;
	} else {
		jumpKey = false;
	}
	if (jumping && !jumpCancel) {
		MakeEscortsHyperWithParent(ship);
		ship->jumpTimer++;
		ship->vel.x *= 0.98;
		ship->vel.y *= 0.98f;
		if (ship->jumpTimer > 30 && CountMatchingSoundVoices((long)warpUpSnd) == 0)
			warpSoundDone = true;
		if (ship->jumpTimer >= 30 && warpSoundDone) {
			ship->pos.x = ship->pos.y = 0.0;
			pc.x = gSystems[gSystems[ship->curSystem].links[ship->targetStellar]].xPos;
			pc.y = gSystems[gSystems[ship->curSystem].links[ship->targetStellar]].yPos;
			pb.x = gSystems[ship->curSystem].xPos;
			pb.y = gSystems[ship->curSystem].yPos;
			Accel((CalcInterceptAngle(pb, pc) + kHalfCircle) % kFullCircle, 1350.0, &ship->pos);
			ship->vel.x = ship->vel.y = 0.0;
			Accel(ship->heading, ShipMaxSpeed(ship), &ship->vel);
			newSyst = gSystems[ship->curSystem].links[ship->targetStellar];
			ship->prevSystem = ship->curSystem;
			ship->curSystem = newSyst;
			ExploreNebulaeFromSystem(ship->curSystem);
			ship->navTargetKind = kNavOff;
			ship->targetStellar = -1;
			ship->targetShip = -1;
			ship->fuel -= 100;
			r = gSAT.wind.bounds;
			r.right = r.left + (gSAT.wind.bounds.right - gSAT.wind.bounds.left) - kStatusPanelWidth;
			InvertRect(&r);
			InvertRect(&r);
			RandomizeStars();
			hudRedraw.nav = true;
			hudRedraw.target = true;
			hudRedraw.credits = true;
			hudRedraw.ammo = true;
			hudRedraw.fuel = true;
			dockTarget = -1;
			dockPermission = -1;
			lastMissionStellar = -1;
			bribeRandom = -1;
			adviceRandom = Rand(1500);
			weapLawFlag = false;
			smuggleFlag = false;
			lowestPrice = kShortMax;
			hailShip = Rand(30) + 30;
			hudBlinkSprite->face = nil;
			blinkTimer = 0;
			mapCenter.h = gSystems[ship->curSystem].xPos;
			mapCenter.v = gSystems[ship->curSystem].yPos;
			DoEscortPayment(1);
			n = ShipHyperTransitTime(ship);
			for (i = 0; i < n; i++)
				IncrementGameTime();
			HandleGlobalPriceFlux();
			HandleLocalPriceFlux();
			for (i = 0; i < MaxSimultaneousMissions; i++) {
				if (currentMissionObjectives[i].active) {
					if (currentMissions[i].shipCount > 0
							&& (ship->curSystem == currentMissions[i].shipSyst || currentMissions[i].shipSyst == kShipSystFollow)
							&& currentMissions[i].shipBehav >= 9) {
						currentMissions[i].shipDelay = Rand(100) + 100;
						currentMissions[i].specialShipsJumpedIn = 0;
					}
					if (currentMissions[i].flags & kMisnInfiniteAux)
						currentMissions[i].auxShipsLeft = currentMissions[i].auxShipCount;
					currentMissions[i].auxShipDelay = Rand(kMissionSpawnDelay) + kMissionSpawnDelay;
					currentMissions[i].auxShipsSpawned = 0;
				}
			}
			ship->jumpTimer = kJumpWindupArriving;
			SetupShipsInSystem(ship->curSystem);
			SpawnBountyHunter();
			HandleStellarSystemVisibility();
			ship->jumpTimer = 0;
			if (gSystems[newSyst].message != -1) {
				DisplayMessage(gSystems[newSyst].message);
			} else {
				n = Rand(3);
				if (n == 0)
					strcpy(s1, "Entering the ");
				if (n == 1)
					strcpy(s1, "Jumping into the ");
				if (n == 2)
					strcpy(s1, "Arriving in the ");
				strcpy(s2, (char *)gSystems[newSyst].name);
				p2cstr((StringPtr)s2);
				strcat(s1, s2);
				strcat(s1, " system on ");
				DateToLongString(gameTime, s2);
				strcat(s1, s2);
				strcat(s1, ".");
				n = 0;
				for (i = 0; i <= 3; i++) {
					if (gSystems[ship->curSystem].navDefault[i] != -1)
						n++;
				}
				if (n == 0)
					strcat(s1, " No stellar objects present.");
				c2pstr(s1);
				DisplayComm((StringPtr)s1, kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
			}
			SpoolNextHyperRouteDest();
			CueNextHyperRouteDest();
			killShots = true;
			killBoxes = true;
			killExplods = true;
			killSmoke = true;
			killAsteroids = true;
			SATSkip();
			CreateAsteroids(false);
			warpSoundDone = false;
		} else {
			if (ship->jumpTimer > 30) {
				if (TickCount() - ship->jumpTick > 466.0f / gShipTypes[ship->type].jumpSpeedScale) {
					if ((TickCount() - ship->jumpTick) > 500.0f / gShipTypes[ship->type].jumpSpeedScale && !warpSoundDone) {
						warpSoundDone = true;
						PlaySound(warpOutSnd, 50, kMidVolume, kMidVolume);
					}
					if (CountMatchingSoundVoices((long)warpUpSnd) == 0)
						warpSoundDone = true;
				}
			}
		}
	} else {
		if (ship->jumpTimer == kJumpWindupArriving)
			ship->jumpTimer = 0;
	}
	if (QuickKeyCheck(keyMap[kCtlReverse]) && ship->jumpTimer <= 0
			&& (FloatAbs(ship->vel.x) >= 0.05 || FloatAbs(ship->vel.y) >= 0.05)) {
		pc.x = pc.y = 0.0;
		pb.x = 100.0 * ship->vel.x;
		pb.y = 100.0 * ship->vel.y;
		ship->desiredHeading = (CalcInterceptAngle(pc, pb) + kHalfCircle) % kFullCircle;
	}
	m = ShipTurnRate(ship);
	if (ship->desiredHeading != ship->heading) {
		n = abs(ship->desiredHeading - ship->heading);
		if (n >= m) {
			f = ship->desiredHeading - ship->heading;
			while (f >= kFullCircle)
				f -= kFullCircle;
			while (f < 0)
				f += kFullCircle;
			if (f > kHalfCircle)
				ship->heading -= (float)m;
			else
				ship->heading += (float)m;
		}
	} else {
		if (ship->jumpTimer <= 0) {
			if (QuickKeyCheck(keyMap[kCtlTurnLeft]))
				ship->heading -= m;
			if (QuickKeyCheck(keyMap[kCtlTurnRight]))
				ship->heading += m;
		}
	}
	while (ship->heading >= kFullCircle)
		ship->heading -= kFullCircle;
	while (ship->heading < 0)
		ship->heading += kFullCircle;
	if (ship->aiTimer <= 0 && QuickKeyCheck(keyMap[kCtlThrust]) && ship->jumpTimer <= 0)
		AdjustedAccel(ship->heading, ShipAccelRate(ship), &ship->vel, ShipMaxSpeed(ship));
	capf = ShipShieldCapacity(ship);
	if (ship->shield < capf && !IsDying(ship)) {
		k = ShipShieldRechargeRate(ship);
		if (frameCount % k == 0) {
			ship->shield += 0.01 * capf;
			if (ship->shield > capf)
				ship->shield = capf;
			hudRedraw.shield = true;
		}
	}
	if (ship->jumpTimer > 1) {
		pc.x = gSystems[ship->curSystem].xPos;
		pc.y = gSystems[ship->curSystem].yPos;
		j = gSystems[ship->curSystem].links[ship->targetStellar];
		pb.x = gSystems[j].xPos;
		pb.y = gSystems[j].yPos;
		n = CalcInterceptAngle(pc, pb);
		if (abs(ship->heading - n) <= ShipTurnRate(ship)) {
			capf = (gShipTypes[ship->type].jumpSpeedScale * (TickCount() - ship->jumpTick)) / 4.6
					- 35.0 / gShipTypes[ship->type].jumpSpeedScale;
			if (capf > 0.0)
				Accel(ship->heading, capf, &ship->pos);
		}
	}
	if (ship->vel.x > maxVel[0])
		ship->vel.x = maxVel[0];
	if (ship->vel.x < -maxVel[0])
		ship->vel.x = -maxVel[0];
	if (ship->vel.y > maxVel[1])
		ship->vel.y = maxVel[1];
	if (ship->vel.y < -maxVel[1])
		ship->vel.y = -maxVel[1];
	ship->pos.x += ship->vel.x * timeScale;
	ship->pos.y += ship->vel.y * timeScale;
	v.x = FloatAbs(ship->pos.x);
	v.y = FloatAbs(ship->pos.y);
	if (v.y > 20000.0f || v.y > 20000.0f) {
		v.x = v.y = 0.0;
		AdjustedAccel(CalcInterceptAngle(ship->pos, v), 1.1 * ShipAccelRate(ship), &ship->vel, 0.1 * ShipMaxSpeed(ship));
	}
	if (ship->pos.x > kWorldEdge)
		ship->pos.x = kWorldEdge;
	if (ship->pos.x < -kWorldEdge)
		ship->pos.x = -kWorldEdge;
	if (ship->pos.y > kWorldEdge)
		ship->pos.y = kWorldEdge;
	if (ship->pos.y < -kWorldEdge)
		ship->pos.y = -kWorldEdge;
	if (ship->navTargetKind == kNavHyperspace && ship->targetStellar != -1) {
		canJump = true;
		for (i = 0; i <= 3; i++) {
			if (gSystems[ship->curSystem].navDefault[i] != -1) {
				v.x = v.y = 0.0;
				dist = ShipHyperSafeDist(ship);
				if (FloatAbs(XYFloatSquaredDist(&v, &ship->pos)) <= dist)
					canJump = false;
			}
		}
		if (canJump) {
			if (arrivalAlert == 0 && ship->jumpTimer <= 0) {
				PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
				hudRedraw.nav = true;
			}
			arrivalAlert = 1;
		} else {
			arrivalAlert = 0;
		}
	}
	if (ship->aiTimer > 0)
		ship->aiTimer--;
	if (ship->deathTimer > 0) {
		ship->deathTimer--;
		if (gInvertedColors)
			DoShipUncloak();
	}
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		for (j = 0; j <= MaxOutfitMods - 1; j++) {
			if (gOutfits[i].modType[j] == kOutfModFuelScoop && outfitCount[i] > 0 && Rand(abs(gOutfits[i].modVal[j])) == 0
					&& ship->jumpTimer <= 0) {
				if (gOutfits[i].modVal[j] < 0)
					ship->fuel -= 1.0f;
				else
					ship->fuel += 1.0f;
				hudRedraw.fuel = true;
			}
		}
	}
	if (gShipTypes[shipData->type].fuelRegen > 0 && (gShipTypes[shipData->type].flags & kShipFuelRegen)
			&& Rand(abs(gShipTypes[shipData->type].fuelRegen)) == 0) {
		ship->fuel++;
		hudRedraw.fuel = true;
	}
	fuelCap = ShipFuelCapacity(ship);
	if (ship->fuel > fuelCap)
		ship->fuel = fuelCap;
	if (ship->fuel < 0.0f)
		ship->fuel = 0.0f;
	if (QuickKeyCheck(keyMap[kCtlSkipMessage]) && commCount > 0)
		commCount = 0;
	if ((QuickKeyCheck(keyMap[kCtlRefillAmmo]) || QuickKeyCheck(kKeyF15)) && showAll) {
		PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
		for (i = 0; i < MaxWeaponTypes; i++) {
			if (gShipTypes[ship->type].weapAmmo[i] != -1)
				ship->weap[i].ammo = gShipTypes[ship->type].weapAmmo[i];
		}
		for (i = 0; i < MaxNebulae; i++)
			nebulaData[i].explored = true;
		ship->weap[MaxWeaponTypes - 1].count = 1;
		AutoSetExploration(shipData->curSystem, 1024);
		if (QuickKeyCheck(kKeyOption))
			ship->fuel = 0;
		else
			ship->fuel = ShipFuelCapacity(ship);
		ship->shield = ShipShieldCapacity(ship);
		ship->deathTimer = -1;
		hudRedraw.fuel = true;
		hudRedraw.shield = true;
		hudRedraw.ammo = true;
	}
	if (showAll) {
		if ((QuickKeyCheck(kKeyPadDivide) || QuickKeyCheck(kKeyF11)) && ship->targetShip != -1 && TGT.active)
			AIMakeShipAttackPlayer(&TGT);
		if (QuickKeyCheck(kKeyPadEquals) && ship->targetShip != -1) {
			ship->vel.x = TGT.vel.x;
			ship->vel.y = TGT.vel.y;
		}
	}
	if (QuickKeyCheck(keyMap[kCtlEscortAttack]) || QuickKeyCheck(keyMap[kCtlEscortRecall]) || QuickKeyCheck(keyMap[kCtlEscortHold])) {
		if (!escortKey && ship->jumpTimer == 0) {
			escortKey = true;
			cmd = -1;
			if (QuickKeyCheck(keyMap[kCtlEscortRecall])) {
				if (QuickKeyCheck(kKeyOption))
					cmd = 0;
				else
					cmd = 1;
			}
			if (QuickKeyCheck(keyMap[kCtlEscortAttack]))
				cmd = 2;
			if (QuickKeyCheck(keyMap[kCtlEscortHold]))
				cmd = 3;
			n = 0;
			for (i = 1; i < MaxShipsInSystem; i++) {
				if (ship->index == shipData[i].parentShip && shipData[i].active
						&& shipData[i].jumpTimer > -900 && shipData[i].jumpTimer <= 0) {
					if (cmd == 0 && !IsShipDocking(&shipData[i]) && shipData[i].aiType == kAIFighter) {
						AIMakeFighterReturn(&shipData[i]);
						n++;
					}
					if (cmd == 1 && !IsShipInForm(&shipData[i])) {
						AIMakeEscortFlyInForm(&shipData[i]);
						if (shipData[i].aiType == kAIEscort)
							shipData[i].aiTimer = Rand(150) + 100;
						n++;
					}
					if (cmd == 2 && !IsShipAttacking(&shipData[i]) && shipData[i].aiType == kAIFighter
							&& ship->targetShip != -1 && ship->index != ship->targetShip
							&& shipData[ship->targetShip].parentShip != 0) {
						AIMakeShipAttackShip(&shipData[i], &shipData[ship->targetShip]);
						n++;
					}
					if (cmd == 3 && !IsShipWaiting(&shipData[i])) {
						AIMakeShipWait(&shipData[i]);
						n++;
					}
				}
			}
			if (n > 0) {
				if (cmd == 0) {
					DisplayComm("\pAcknowledged - fighters returning to mothership.", 120, 0, kCommTextSize, &rgbWhite, false, false);
					SpeakVoiceMessage(0);
				}
				if (cmd == 1) {
					DisplayComm("\pAcknowledged - escorts returning to formation.", 120, 0, kCommTextSize, &rgbWhite, false, false);
					SpeakVoiceMessage(0);
				}
				if (cmd == 2) {
					NumToString(n, (StringPtr)s1);
					DisplayComm("\pAffirmative - fighters engaging new target.", 120, 0, kCommTextSize, &rgbWhite, false, false);
					SpeakVoiceMessage(1);
				}
				if (cmd == 3) {
					DisplayComm("\pAffirmative - escorts holding position.", 120, 0, kCommTextSize, &rgbWhite, false, false);
					SpeakVoiceMessage(3);
				}
			}
		}
	} else {
		escortKey = false;
	}
	if (showAll) {
		if (QuickKeyCheck(kKeyPadDecimal) && CountMatchingSoundVoices((long)warpUpSnd) != 0) {
			FlushMixQueueEntries((long)warpUpSnd);
			WarpUpCallback(1, 0, 0);
		}
		if (QuickKeyCheck(keyMap[kCtlShowMemory]) || QuickKeyCheck(kKeyF10)) {
			memFree = MaxMem(&grow);
			NumToString(memFree, (StringPtr)s1);
			p2cstr((StringPtr)s1);
			strcat(s1, " bytes free, gSpeedMult = ");
			NumToString(100.0 * timeScale, (StringPtr)s2);
			p2cstr((StringPtr)s2);
			strcat(s1, s2);
			strcat(s1, "%, ");
			NumToString(gDaysUsed, (StringPtr)s2);
			p2cstr((StringPtr)s2);
			strcat(s1, s2);
			strcat(s1, " days played");
			c2pstr(s1);
			DisplayComm((StringPtr)s1, 60, 0, kCommTextSize, &rgbWhite, false, false);
		}
		if (QuickKeyCheck(keyMap[kCtlSpawnPerson]) || QuickKeyCheck(kKeyF6)) {
			if (!spawnKey) {
				spawnKey = true;
				if (QuickKeyCheck(kKeyOption))
					SpawnPerson(shipData->curSystem, true, 44);
				else
					ship->targetShip = HyperShipSpawn(ship->curSystem);
				hudRedraw.target = true;
				PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
			}
		} else {
			spawnKey = false;
		}
		if ((QuickKeyCheck(keyMap[kCtlDropTarget]) || QuickKeyCheck(kKeyF7)) && ship->targetShip != -1) {
			TGT.active = false;
			hudRedraw.target = true;
			PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
		}
		if ((QuickKeyCheck(keyMap[kCtlWreckTarget]) || QuickKeyCheck(kKeyF8))
				&& ship->targetShip != -1 && TGT.shield > -gShipTypes[TGT.type].armor) {
			TGT.shield = -gShipTypes[TGT.type].armor;
			hudRedraw.target = true;
			PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
		}
		if ((QuickKeyCheck(keyMap[kCtlKillTarget]) || QuickKeyCheck(kKeyF9))
				&& ship->targetShip != -1 && TGT.shield >= -gShipTypes[TGT.type].armor) {
			if (QuickKeyCheck(kKeyShift)) {
				SlapWithPenalty(shipData->curSystem, TGT.govt, 3, TGT.specialMission);
				if (playerKills + gShipTypes[ship->type].crew > 0 && playerKills + gShipTypes[ship->type].crew < kShortBound)
					playerKills += gShipTypes[TGT.type].crew;
			}
			TGT.shield = -(gShipTypes[TGT.type].armor + 1);
			hudRedraw.target = true;
			PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
		}
		if (QuickKeyCheck(keyMap[kCtlNextShipType]) || QuickKeyCheck(kKeyEquals)) {
			do {
			} while (KeyCheck(keyMap[kCtlNextShipType]) != 0);
			ship->type++;
			if (ship->type >= MaxShipClasses)
				ship->type = 0;
			PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
			ship->shield = ShipShieldCapacity(ship);
			ship->fuel = gShipTypes[ship->type].fuel;
			hudRedraw.fuel = hudRedraw.shield = hudRedraw.ammo = hudRedraw.credits = true;
			for (i = 0; i < MaxOutfitItemTypes; i++)
				outfitCount[i] = 0;
			for (i = 0; i < MaxWeaponTypes; i++) {
				ship->weap[i].count = gShipTypes[ship->type].weapCount[i];
				ship->weap[i].ammo = gShipTypes[ship->type].weapAmmo[i];
			}
			ship->selectedWeapon = -1;
		}
		if (QuickKeyCheck(keyMap[kCtlPrevShipType]) || QuickKeyCheck(kKeyMinus)) {
			do {
			} while (KeyCheck(keyMap[kCtlPrevShipType]) != 0);
			ship->type--;
			if (ship->type < 0)
				ship->type = kEscapePodShip;
			PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
			ship->shield = ShipShieldCapacity(ship);
			ship->fuel = gShipTypes[ship->type].fuel;
			hudRedraw.fuel = hudRedraw.shield = hudRedraw.ammo = hudRedraw.credits = true;
			for (i = 0; i < MaxOutfitItemTypes; i++)
				outfitCount[i] = 0;
			for (i = 0; i < MaxWeaponTypes; i++) {
				ship->weap[i].count = gShipTypes[ship->type].weapCount[i];
				ship->weap[i].ammo = gShipTypes[ship->type].weapAmmo[i];
			}
			ship->selectedWeapon = -1;
		}
		if (QuickKeyCheck(kKeyShift) && QuickKeyCheck(keyMap[kCtlAdjustCredits])) {
			PlaySound(beepSnd[kBeepSelect], 1, kMidVolume, kMidVolume);
			if (QuickKeyCheck(kKeyOption)) {
				if (ship->credits >= 1000)
					ship->credits -= 1000;
				else
					ship->credits = 0;
			} else {
				ship->credits += 10000;
			}
			hudRedraw.credits = true;
		}
	}
	if (ship->credits < 0) {
		ship->credits = kMaxCredits;
		hudRedraw.credits = true;
	}
	if (ship->credits > kMaxCredits) {
		ship->credits = kMaxCredits;
		hudRedraw.credits = true;
	}
	for (i = 0; i <= NumCommodities - 1; i++) {
		if (ship->cargo[i] < 0) {
			ship->cargo[i] = 0;
			hudRedraw.credits = true;
		}
	}
	for (i = 0; i < kMaxJunkTypes; i++) {
		if (junkData[i].held < 0) {
			junkData[i].held = 0;
			hudRedraw.credits = true;
		}
	}
	if (ship->deathTimer > 1 && CountMatchingSoundVoices((long)klaxonSnd) == 0)
		PlaySound(klaxonSnd, 15, kMidVolume, kMidVolume);
	if (ship->deathTimer > 1) {
		ok = false;
		if ((ship->deathTimer <= 0.5 * gShipTypes[ship->type].deathDelay || ship->deathTimer <= 30)
				&& HasAutoEject(ship))
			ok = true;
		if (((QuickKeyCheck(kKeyCommand) && QuickKeyCheck(keyMap[kCtlEject])) || ok)
				&& HasEscapePod(ship)) {
			n = GenericRandomShipSpawn(ship->curSystem, 0);
			if (n != -1) {
				for (i = 1; i < MaxShipsInSystem; i++) {
					if (shipData[i].active && shipData[i].targetShip == 0)
						shipData[i].targetShip = -1;
				}
				shipData[n].aiType = -1;
				shipData[n].pos = ship->pos;
				shipData[n].vel = ship->vel;
				shipData[n].heading = ship->heading;
				shipData[n].type = ship->type;
				shipData[n].deathTimer = ship->deathTimer;
				shipData[n].shield = ship->shield;
				ship->type = kEscapePodShip;
				ship->deathTimer = 0;
				ship->targetShip = -1;
				ship->targetStellar = -1;
				ship->shield = gShipTypes[ship->type].shield;
				hudRedraw.fuel = hudRedraw.nav = hudRedraw.target = hudRedraw.credits = hudRedraw.ammo = true;
				for (i = 0; i < MaxWeaponTypes; i++) {
					ship->weap[i].count = gShipTypes[ship->type].weapCount[i];
					ship->weap[i].ammo = gShipTypes[ship->type].weapAmmo[i];
				}
				for (i = 0; i < MaxOutfitItemTypes; i++)
					outfitCount[i] = 0;
				PlaySound(ejectSnd, 50, kMidVolume, kMidVolume);
				if (CountMatchingSoundVoices((long)warpUpSnd) != 0)
					FlushMixQueueEntries((long)warpUpSnd);
				ship->jumpTimer = -1;
				gTargetStellar = 420;
			}
		}
	}
	burning = false;
	if (QuickKeyCheck(keyMap[kCtlAfterburner]) && HasAfterburner(ship) && ship->jumpTimer <= 0
			&& ship->fuel > 0 && ship->aiTimer <= 0)
		burning = true;
	if (burning) {
		maxVel[0] = 1.8 * ShipMaxSpeed(ship);
		maxVel[1] = 1.8 * ShipMaxSpeed(ship);
		Accel(ship->heading, 2.75 * ShipAccelRate(ship), &ship->vel);
		if (frameCount % 4 == 0) {
			ship->fuel -= 5;
			hudRedraw.fuel = true;
		}
	} else {
		capf = ShipMaxSpeed(ship);
		dist = 0.4 * ShipAccelRate(ship);
		if (maxVel[0] > capf)
			maxVel[0] -= dist;
		if (maxVel[1] > capf)
			maxVel[1] -= dist;
		if (maxVel[0] < capf)
			maxVel[0] = capf;
		if (maxVel[1] < capf)
			maxVel[1] = capf;
	}
	if (QuickKeyCheck(kKeyCommand) && QuickKeyCheck(keyMap[kCtlJettison])) {
		if (QuickKeyCheck(kKeyOption))
			JettisonCargo(false);
		else
			JettisonCargo(true);
		hudRedraw.credits = true;
	}
	if (QuickKeyCheck(kKeyCommand) && QuickKeyCheck(keyMap[kCtlSelfDestruct]) && commStellar < 0
			&& !IsDisabled(ship) && !IsDying(ship)) {
		PlaySound(beepSnd[kBeepDenied], 15, kMidVolume, kMidVolume);
		DisplayComm("\pSelf-destruct sequence initiated.", 40, 0, kCommTextSize, &rgbWhite, false, false);
		commStellar = 360;
	}
	if (commStellar > 0) {
		commStellar--;
		if (commStellar % 30 == 0 && commStellar <= 300) {
			strcpy(s1, "Self-destruct in ");
			NumToString(commStellar / 30, (StringPtr)s2);
			p2cstr((StringPtr)s2);
			strcat(s1, s2);
			strcat(s1, " second");
			if (commStellar > 30)
				strcat(s1, "s");
			strcat(s1, ".");
			c2pstr(s1);
			PlaySound(beepSnd[kBeepClose], 15, kMidVolume, kMidVolume);
			DisplayComm((StringPtr)s1, 40, 0, kCommTextSize, &rgbWhite, false, false);
		}
	}
	if (commStellar == 0) {
		if (QuickKeyCheck(kKeyCommand) && QuickKeyCheck(keyMap[kCtlSelfDestruct])) {
			PlaySound(beepSnd[kBeepClick], 15, kMidVolume, kMidVolume);
			DisplayComm("\pHave a nice day.", 40, 0, kCommTextSize, &rgbWhite, false, false);
			ship->shield = kDeadMark;
		} else {
			PlaySound(beepSnd[kBeepClick], 15, kMidVolume, kMidVolume);
			DisplayComm("\pSelf-destruct sequence cancelled.", 40, 0, kCommTextSize, &rgbWhite, false, false);
		}
		commStellar = -1;
	}
	if (QuickKeyCheck(keyMap[kCtlPlayerInfo])) {
		if (!infoKey) {
			infoKey = true;
			if (ship->jumpTimer <= 0 && gTargetStellar <= 0 && frameCount >= 0 && !IsDying(ship)) {
				ShowCursor();
				DoPlayerInfoDialog();
				HideCursor();
				RedrawStatus();
				RedrawComm(false);
			} else {
				PlaySound(beepSnd[kBeepDenied], 5, kMidVolume, kMidVolume);
			}
		}
	} else {
		infoKey = false;
	}
	if (QuickKeyCheck(keyMap[kCtlHail])) {
		if (!hailKey) {
			hailKey = true;
			HandlePlayerCommunication();
		}
	} else {
		hailKey = false;
	}
	if (QuickKeyCheck(keyMap[kCtlMissions]) && ship->jumpTimer <= 0 && gTargetStellar <= 0
			&& !IsDying(ship) && ship->deathTimer <= 0 && frameCount >= 0) {
		if (!missionKey) {
			missionKey = true;
			n = 0;
			for (i = 0; i < MaxSimultaneousMissions; i++) {
				if (currentMissionObjectives[i].active)
					n++;
			}
			if (n > 0) {
				ShowCursor();
				DoMissionInfoDialog();
				HideCursor();
				RedrawStatus();
				RedrawComm(false);
			} else {
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
				DisplayComm("\pYou have no active missions.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
			}
		}
	} else {
		missionKey = false;
	}
	if (QuickKeyCheck(keyMap[kCtlCloak]) && ship->deathTimer <= 0 && ship->jumpTimer <= 0) {
		if (!cloakKey) {
			cloakKey = true;
			if (HasCloak(ship)) {
				if (!gInvertedColors) {
					if (ship->fuel > 0)
						DoShipCloak();
					else
						PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
				} else {
					DoShipUncloak();
				}
			} else {
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
			}
		}
	} else {
		cloakKey = false;
	}
	if (gInvertedColors) {
		if (ship->fuel <= 0) {
			DoShipUncloak();
		} else {
			if (frameCount % 8 == 0) {
				ship->fuel--;
				hudRedraw.fuel = true;
			}
		}
	}
	if (frameCount == 0 || frameCount == 250 || frameCount == 500 || frameCount == 750) {
		for (i = 0; i < kMaxJunkTypes; i++) {
			if ((junkData[i].flags & kJunkTribbles) && junkData[i].held > 0) {
				n = TotalFleetHolds() - TotalCargo(ship);
				if (n > 0) {
					junkData[i].held++;
					hudRedraw.credits = true;
				}
			}
		}
	}
	if (lowestPrice < 2 && frameCount % 60 == 0) {
		v.x = v.y = 0;
		dist = FloatAbs(XYFloatSquaredDist(&ship->pos, &v));
		if (lowestPrice == -3) {
			if (gSystems[shipData->curSystem].navDefault[0] != -1) {
				PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
				if (stellarData[gSystems[shipData->curSystem].navDefault[0]].flags & kSpobStation)
					strcpy(s1, "Welcome to EV Override - it would be a good idea to start by docking at ");
				else
					strcpy(s1, "Welcome to EV Override - it would be a good idea to start by landing on ");
				strcpy(s2, (char *)stellarData[gSystems[shipData->curSystem].navDefault[0]].name);
				p2cstr((StringPtr)s2);
				strcat(s1, s2);
				strcat(s1, " and checking out the prices. Hit \324");
				GetIndString((StringPtr)s2, 129, keyMap[kCtlLand] + 1);
				p2cstr((StringPtr)s2);
				strcat(s1, s2);
				strcat(s1, "\325 to request landing clearance, then hit it again to land.");
				c2pstr(s1);
				DisplayComm((StringPtr)s1, 512, 0, kCommTextSize, &rgbWhite, false, false);
			}
			lowestPrice = -2;
		}
		if (lowestPrice == -1) {
			if (gSystems[shipData->curSystem].navDefault[0] != -1) {
				PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
				strcpy(s1, "Now you should probably hyperspace to another system. Hit \324");
				GetIndString((StringPtr)s2, 129, keyMap[kCtlMap] + 1);
				p2cstr((StringPtr)s2);
				strcat(s1, s2);
				strcat(s1, "\325 to access the map, select a nearby system, then move outward and hit \324");
				GetIndString((StringPtr)s2, 129, keyMap[kCtlJump] + 1);
				p2cstr((StringPtr)s2);
				strcat(s1, s2);
				strcat(s1, "\325 to begin your jump.");
				c2pstr(s1);
				DisplayComm((StringPtr)s1, 512, 0, kCommTextSize, &rgbWhite, false, false);
			}
			lowestPrice = 0;
		}
		if (dist > 2000000.0 && lowestPrice <= 0) {
			PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
			DisplayComm("\pYou\325re well beyond safe hyperspace range - you can initiate hyperjump at any time.", 512, 0, kCommTextSize, &rgbWhite, false, false);
			lowestPrice = 1;
		} else {
			if (dist > 5000000.0 && lowestPrice == 1) {
				PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
				DisplayComm("\pThere\325s nothing to find out here - if you want to get to another system, you\325ll have to access the map, set a hyperdrive destination, and start your jump.", 512, 0, kCommTextSize, &rgbWhite, false, false);
				lowestPrice = 2;
			}
		}
	}
	if (frameCount == 500 || frameCount == 1000) {
		m = TotalFleetHolds();
		capf = TotalCargo(ship);
		if (capf > m) {
			for (i = 0; i <= NumCommodities - 1; i++)
				ship->cargo[i] *= (double)m / capf;
			hudRedraw.credits = true;
		}
	}
	if (ship->credits < 0) {
		ship->credits = 0;
		hudRedraw.credits = true;
	}
	if (ship->fuel < 0) {
		ship->fuel = 0;
		hudRedraw.fuel = true;
	}
	if (gSystems[ship->curSystem].visited < 1)
		gSystems[ship->curSystem].visited = 1;
}

// FUNCTION: EVO 0x1002EF00
void HandlePlayerCommunication(void)
{
	ShipObj *ship;
	short k;
	Boolean accepted;
	Boolean canLand;
	Boolean ok;
	short noHail;
	short spawned;
	short found;
	short i;
	Rect r;

	ship = shipData;
	if (!gInvertedColors && !IsDisabled(ship) && !IsDying(ship) && ship->jumpTimer <= 0) {
		if (ship->targetShip == -1 || QuickKeyCheck(kKeyOption)) {
			if (ship->navTargetKind == kNavStellar && ship->targetStellar != -1) {
				canLand = !SpobUninhabited(&stellarData[ship->targetStellar]);
				if (!SpobCanLand(&stellarData[ship->targetStellar]))
					canLand = false;
				if (!stellarData[ship->targetStellar].loaded || !canLand) {
					PlaySound(beepSnd[kBeepDenied], 5, kMidVolume, kMidVolume);
					DisplayComm("\pNo response.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
				} else {
					DoPlanetCommDialog(ship->targetStellar);
				}
			} else {
				PlaySound(beepSnd[kBeepDenied], 5, kMidVolume, kMidVolume);
			}
		} else if (shipData[ship->targetShip].jumpTimer <= 0) {
			ok = true;
			if (shipData[ship->targetShip].govt != -1) {
				noHail = GovtNoHail(&govtData[shipData[ship->targetShip].govt]);
				if (noHail)
					ok = false;
			}
			if (shipData[ship->targetShip].type == kEscapePodShip)
				ok = false;
			if (IsDisabled(&shipData[ship->targetShip]))
				ok = false;
			if (shipData[ship->targetShip].persType == kPersAmbrosiaMascot)
				ok = false;
			if (ok) {
				if (shipData[ship->targetShip].persType != -1) {
					if (personData[shipData[ship->targetShip].persType].linkMission != -1) {
						if (!(personData[shipData[ship->targetShip].persType].flags & kPersOfferOnBoard)) {
							drawGate = true;
							speakerShip = ship->targetShip;
							if (IsMissionAvailable(personData[shipData[ship->targetShip].persType].linkMission)) {
								PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
								if (cursorHidden)
									ShowCursor();
								accepted = RunSingleMissionDialog(personData[shipData[ship->targetShip].persType].linkMission);
								if (cursorHidden)
									HideCursor();
								SATSetPortScreen();
								r = gSAT.wind.bounds;
								r.right -= kStatusPanelWidth;
								ForeColor(blackColor);
								PaintRect(&r);
								RedrawComm(false);
								if (accepted) {
									if (personData[shipData[ship->targetShip].persType].flags & kPersDeactivate)
										personData[shipData[ship->targetShip].persType].loaded = false;
									if (personData[shipData[ship->targetShip].persType].flags & kPersLeaveAfter)
										AIMakeShipLeave(&shipData[ship->targetShip]);
									found = -1;
									for (i = 0; i < MaxSimultaneousMissions; i++) {
										if (currentMissionObjectives[i].active
												&& currentMissions[i].misnIndex == personData[shipData[ship->targetShip].persType].linkMission) {
											found = i;
											break;
										}
									}
									if (found >= 0 && (personData[shipData[ship->targetShip].persType].flags & kPersReplaceSpecial)
											&& currentMissions[found].shipCount == 1) {
										spawned = MissionDudeSpawn(currentMissions[found].shipDude, shipData->curSystem, found);
										if (spawned != -1) {
											shipData[spawned].pos.x = shipData[ship->targetShip].pos.x;
											shipData[spawned].pos.y = shipData[ship->targetShip].pos.y;
											shipData[spawned].vel.x = shipData[ship->targetShip].vel.x;
											shipData[spawned].vel.y = shipData[ship->targetShip].vel.y;
											shipData[spawned].heading = shipData[ship->targetShip].heading;
											AIMakeEscortFlyInForm(&shipData[spawned]);
											shipData[ship->targetShip].active = false;
											ship->targetShip = spawned;
											hudRedraw.target = true;
										}
									}
								}
							} else {
								DoCommDialog(ship->targetShip);
							}
							drawGate = false;
							speakerShip = ship->targetShip;
						} else {
							DoCommDialog(ship->targetShip);
						}
					} else {
						DoCommDialog(ship->targetShip);
					}
				} else {
					DoCommDialog(ship->targetShip);
				}
				RedrawStatus();
				RedrawComm(false);
			} else {
				PlaySound(beepSnd[kBeepDenied], 5, kMidVolume, kMidVolume);
				DisplayComm("\pNo response.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
			}
		} else {
			if (IsDisabled(&shipData[ship->targetShip])) {
				PlaySound(beepSnd[kBeepDenied], 5, kMidVolume, kMidVolume);
				DisplayComm("\pNo response.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
			} else {
				PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
				DisplayComm("\pUnable to send hail - target ship is entering hyperspace.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
			}
		}
	} else {
		PlaySound(beepSnd[kBeepDenied], 1, kMidVolume, kMidVolume);
	}
}

// FUNCTION: EVO 0x1002F710
void FirePlayerWeapon(short slot)
{
	Boolean fired, audible, noSpread;
	short shots, nshots, i, diff, angle;

	if (shipData->weap[slot].reload <= 0.0f && shipData->jumpTimer == 0) {
		shots = 0;
		if (gWeapons[slot].flags & kWeapFireTogether)
			nshots = shipData->weap[slot].count;
		else
			nshots = 1;
		for (i = 0; i < nshots; i++) {
			fired = false;
			if (WeaponHasAmmo(shipData, slot)) {
				if (gWeapons[slot].guidance == kGuidanceUnguided || gWeapons[slot].guidance == kGuidanceHoming
						|| gWeapons[slot].guidance == kGuidanceFreefall || gWeapons[slot].guidance == kGuidanceRocket) {
					SpawnShot(0, shipData->targetShip, slot);
					shots++;
					fired = true;
				}
				if (gWeapons[slot].guidance == kGuidanceBeam) {
					short spread;

					spread = WeaponShotSpread(0, slot);
					if (spread) noSpread = false; else noSpread = true;
					SpawnBeam(0, shipData->targetShip, slot, shipData->heading, noSpread, spread);
					shots++;
					fired = true;
				}
				if (gWeapons[slot].guidance == kGuidanceTurretBeam && shipData->targetShip != -1
						&& !TurretBlindSpot(shipData, &shipData[shipData->targetShip], slot)) {
					SpawnBeam(0, shipData->targetShip, slot, CalcInterceptAngle(shipData->pos, shipData[shipData->targetShip].pos), 0, 0);
					shots++;
					fired = true;
				}
				if (gWeapons[slot].guidance == kGuidanceTurret && shipData->targetShip != -1
						&& !TurretBlindSpot(shipData, &shipData[shipData->targetShip], slot)) {
					SpawnShot(0, shipData->targetShip, slot);
					shots++;
					fired = true;
				}
				if (gWeapons[slot].guidance == kGuidanceFrontQuad || gWeapons[slot].guidance == kGuidanceRearQuad) {
					if (shipData->targetShip != -1) {
						angle = CalcInterceptAngle(shipData->pos, shipData[shipData->targetShip].pos);
						if (gWeapons[slot].guidance == kGuidanceFrontQuad)
							diff = abs(angle - shipData->heading) % kFullCircle;
						if (gWeapons[slot].guidance == kGuidanceRearQuad)
							diff = abs(angle - (shipData->heading + kHalfCircle) % kFullCircle) % kFullCircle;
						if (diff <= 45) {
							SpawnShot(0, shipData->targetShip, slot);
							shots++;
							fired = true;
						} else if (gWeapons[slot].guidance == kGuidanceFrontQuad) {
							SpawnShot(0, -1, slot);
							shots++;
							fired = true;
						}
					} else {
						if (gWeapons[slot].guidance == kGuidanceFrontQuad) {
							SpawnShot(0, -1, slot);
							shots++;
							fired = true;
						}
					}
				}
				if (gWeapons[slot].guidance == kGuidanceBay && LaunchFighter(shipData, slot)) {
					shots++;
					fired = true;
				}
				if (fired) {
					if (shipData->altFireSide < 0)
						shipData->altFireSide = 1;
					else
						shipData->altFireSide = -1;
					if (gWeapons[slot].ammoType != -1) {
						if (gWeapons[slot].ammoType > 0) {
							shipData->weap[slot].ammo--;
							if (shipData->weap[slot].ammo < 0)
								shipData->weap[slot].ammo = 0;
							hudRedraw.ammo = true;
						} else if (gWeapons[slot].ammoType <= -1000) {
							shipData->fuel -= abs(gWeapons[slot].ammoType) - 1000;
							if (shipData->fuel < 0.0f)
								shipData->fuel = 0.0f;
							hudRedraw.fuel = true;
						}
					}
				}
			}
		}
		if (shots > 0) {
			audible = true;
			if ((gWeapons[slot].flags & kWeapLoopSound) != 0 && gWeapons[slot].sound >= 0
					&& CountMatchingSoundVoices((long)weapSnd[gWeapons[slot].sound]) != 0)
				audible = false;
			if (gWeapons[slot].sound >= 0 && audible
					&& (gWeapons[slot].guidance == kGuidanceBeam || gWeapons[slot].guidance == kGuidanceTurretBeam)) {
				PlaySoundDistance(2, weapSnd[gWeapons[slot].sound], 6, &shipData->pos, &shipData->pos);
				audible = false;
			}
			if (gWeapons[slot].sound < 0)
				audible = false;
			if (audible) {
				if (gWeapons[slot].guidance == kGuidanceBay)
					PlaySoundDistance(-1, weapSnd[gWeapons[slot].sound], 6, &shipData->pos, &shipData->pos);
				else if ((gWeapons[slot].flags & kWeapSecondTrigger) != 0)
					PlaySoundDistance(-1, weapSnd[gWeapons[slot].sound], 6, &shipData->pos, &shipData->pos);
				else
					PlaySoundDistance(-1, weapSnd[gWeapons[slot].sound], 5, &shipData->pos, &shipData->pos);
			}
			shipData->weap[slot].reload = shots * ((float)gWeapons[slot].reload / (float)shipData->weap[slot].count);
		}
	}
}
