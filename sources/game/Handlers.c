/* Handlers.c -- the HUD and world sprite task procs, projectile physics and AI, laser trails, the per-frame world update. Notes: Research/notes/Handlers.md */
#include <Memory.h>
#include <Resources.h>
#include <TextUtils.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "evo.h"

extern Boolean		doubleTime;
extern FacePtr		doubleSpeedFace;

// FUNCTION: EVO 0x1001EB2C
pascal void HandleShipDisplay(SpritePtr sprite)
{
	ShipObj *s;
	short kind, h;
	short count;
	short radius, i;
	int dmg, dmg2;
	long padA[4];
	FloatPoint where, d;
	short r;
	long pad[4];

	if (sprite->appPtr == NULL) {
		sprite->task = NULL;
		return;
	}
	s = (ShipObj *)sprite->appPtr;
	if (!s->active) {
		s->hasWorldSpriteNode = false;
		if (shipData->targetShip == s->index) {
			shipData->targetShip = -1;
			hudRedraw.target = true;
		}
		sprite->task = NULL;
		return;
	}
	if (s->curSystem == shipData->curSystem && s->active) {
		if (gShipTypes[s->type].cost != -1) {
			if (s->index == 0) {
				sprite->face = shipFaces[s->type][s->heading / kHeadingStep];
			} else {
				d.x = FloatAbs(s->pos.x);
				d.y = FloatAbs(s->pos.y);
				if (d.x < 32700.0f && d.y < 32700.0f)
					sprite->face = shipFaces[s->type][s->heading / kHeadingStep];
				else
					sprite->face = NULL;
			}
		} else {
			sprite->face = NULL;
		}
		count = 0.25 * GetSpriteXSize(sprite->face);
		h = 0.25 * GetSpriteYSize(sprite->face);
		sprite->hotRect.left = count;
		sprite->hotRect.top = h;
		sprite->hotRect.right = count * 3;
		sprite->hotRect.bottom = h * 3;
		sprite->position.h = screenCenter.h + (s->pos.x - shipData->pos.x) - GetSpriteXSize(sprite->face) / 2;
		sprite->position.v = screenCenter.v + (s->pos.y - shipData->pos.y) - GetSpriteYSize(sprite->face) / 2;
		if (settings.highDetail)
			RefreshSprite(sprite);
		if (!IsDying(s))
			return;
		{
			if (s->deathTimer <= 0) {
				s->deathTimer = gShipTypes[s->type].deathDelay;
				if (s->index != 0)
					return;
				s->deathTimer *= 3;
				return;
			}
			if (s->deathTimer <= 2) {
				if (s->index == 0) {
					if (CountMatchingSoundVoices((long)klaxonSnd))
						FlushMixQueueEntries((long)klaxonSnd);
					if (CountMatchingSoundVoices((long)warpUpSnd))
						FlushMixQueueEntries((long)warpUpSnd);
					if (CountMatchingSoundVoices((long)warpOutSnd))
						FlushMixQueueEntries((long)warpOutSnd);
				}
				if (gShipTypes[s->type].mass < 100) {
					radius = 0;
					dmg = 1;
					dmg2 = 0;
				} else {
					radius = 0.15 * gShipTypes[s->type].mass + 50.0;
					dmg = 1000;
					dmg2 = 0.075 * gShipTypes[s->type].mass + 25.0;
				}
				if (radius > 0) {
					for (i = 0; i < MaxShipsInSystem; i++) {
						if (i != s->index && shipData[i].active) {
							d.x = FloatAbs(shipData[i].pos.x - s->pos.x);
							d.y = FloatAbs(shipData[i].pos.y - s->pos.y);
							if (d.x <= radius && d.y <= radius && s->persType != kPersAmbrosiaMascot)
								DamageShip(&shipData[i], s->pos, dmg, dmg2, dmg2, s->index, 0, 0, 1, 1);
						}
					}
					r = radius;
					count = 3.0f * (float)((float)radius / 50.0);
					for (i = 0; i < count; i++) {
						d.x = s->pos.x + Rand(r) - 0.5 * r;
						d.y = s->pos.y + Rand(r) - 0.5 * r;
						SpawnExplod(d, s->index, 1, Rand(8) + 4);
					}
					count = 10.0f * (float)((float)radius / 50.0);
					for (i = 0; i < count; i++) {
						d.x = s->pos.x + Rand(r) - Rand(0.5 * r);
						d.y = s->pos.y + Rand(r) - Rand(0.5 * r);
						SpawnExplod(d, s->index, 0, Rand(16) + 8);
					}
				}
				if (s->specialMission != -1 && s->index != 0) {
					if (currentMissionObjectives[s->specialMission].active
							&& currentMissions[s->specialMission].specialShipsKilled == 0
							&& (currentMissions[s->specialMission].shipGoal == kMisnGoalDisable
								|| (currentMissions[s->specialMission].shipGoal == kMisnGoalBoard && !s->boarded)
								|| (currentMissions[s->specialMission].shipGoal == kMisnGoalRescue && !s->boarded)
								|| currentMissions[s->specialMission].shipGoal == kMisnGoalEscort)
							&& !currentMissionObjectives[s->specialMission].failed) {
						PlaySound(beepSnd[kBeepClick], 1, kMidVolume, kMidVolume);
						DisplayComm("\pMission failed.", kCommShowTicks, 0, kCommTextSize, &rgbWhite, false, false);
						QuickMissionFailure(s->specialMission);
					}
					currentMissions[s->specialMission].specialShipsKilled++;
					if (currentMissions[s->specialMission].shipCount > 0)
						currentMissions[s->specialMission].shipCount--;
				}
				if (s->parentShip == 0 && s->aiType == kAIEscort && s->specialMission == -1
						&& gShipTypes[s->type].inherentAI <= kAIBraveTrader) {
					DestroyPartialFleetCargo(s->index);
					ShipStatsToSystemInfo();
					hudRedraw.credits = true;
				}
				if (s->persType != -1) {
					if (s->persType == kPersBountyHunter) {
						if (Rand(8) == 0)
							personData[s->persType].loaded = false;
					} else if ((personData[s->persType].flags & kPersPodAndAfterburner) == 0) {
						personData[s->persType].loaded = false;
					}
				}
				SpawnExplod(s->pos, s->index, 2, 0);
				PlaySoundDistance(-1, explodeSnd[3], 10, &s->pos, &shipData->pos);
				sprite->face = NULL;
				sprite->task = NULL;
				s->active = false;
				s->hasWorldSpriteNode = false;
				s->parentShip = -1;
				return;
			} else {
				if (s->deathTimer < 20)
					count = 1;
				else if (s->deathTimer < 40)
					count = 2;
				else if (s->deathTimer < 60)
					count = 4;
				else
					count = 8;
				if (Rand(count) == 0) {
					h = 0.25 * GetSpriteXSize(sprite->face);
					if (h <= 0)
						h = 1;
					where.x = s->pos.x + Rand(h * 2) - h;
					where.y = s->pos.y + Rand(h * 2) - h;
					if (count <= 2 && gShipTypes[s->type].deathDelay >= 60)
						kind = Rand(2);
					else
						kind = 0;
					SpawnExplod(where, s->index, kind, 0);
					if (Rand(4) == 0)
						PlaySoundDistance(-1, explodeSnd[2], 1, &s->pos, &shipData->pos);
				}
			}
		}
	} else {
		sprite->face = NULL;
		sprite->task = NULL;
		s->active = false;
		s->hasWorldSpriteNode = false;
		s->parentShip = -1;
	}
}

// FUNCTION: EVO 0x1001F9AC
pascal void HandleBoxes(SpritePtr sprite)
{
	PodObj *pod;
	short i;

	if (sprite->appPtr == NULL) {
		sprite->task = NULL;
	} else {
		pod = (PodObj *)sprite->appPtr;
		pod->life--;
		if (pod->life < 0 || killBoxes) {
			pod->life = kSlotFree;
			sprite->face = NULL;
			sprite->task = NULL;
		}
		pod->frame += pod->spinDir;
		while (pod->frame >= kHeadingFrames)
			pod->frame -= kHeadingFrames;
		while (pod->frame < 0)
			pod->frame += kHeadingFrames;
		sprite->face = boxFaces[pod->frame];
		pod->pos.x += pod->vel.x * timeScale;
		pod->pos.y += pod->vel.y * timeScale;
		sprite->position.h = screenCenter.h + (pod->pos.x - shipData->pos.x) - GetSpriteXSize(sprite->face) / 2;
		sprite->position.v = screenCenter.v + (pod->pos.y - shipData->pos.y) - GetSpriteYSize(sprite->face) / 2;
		if (settings.highDetail)
			RefreshSprite(sprite);
	}
}

// FUNCTION: EVO 0x1001FBF8
pascal void HandleExplods(SpritePtr sprite)
{
	short last;
	short i;

	sprite->appLong--;
	if (sprite->appLong < 0 || killExplods) {
		numExplosions--;
		sprite->face = NULL;
		sprite->task = NULL;
	} else {
		last = 7;
		if (sprite->mode == 1)
			last = 14;
		if (sprite->mode == 2)
			last = 28;
		sprite->face = NULL;
		if (sprite->appLong <= last) {
			if (sprite->mode == 0)
				sprite->face = explodeFaces[0][sprite->appLong];
			if (sprite->mode == 1)
				sprite->face = explodeFaces[1][sprite->appLong / 2];
			if (sprite->mode == 2)
				sprite->face = explodeFaces[2][sprite->appLong / 4];
		}
		sprite->position.h = screenCenter.h + (sprite->speed.h - shipData->pos.x) - GetSpriteXSize(sprite->face) / 2;
		sprite->position.v = screenCenter.v + (sprite->speed.v - shipData->pos.y) - GetSpriteYSize(sprite->face) / 2;
		if (settings.highDetail)
			RefreshSprite(sprite);
	}
}

// FUNCTION: EVO 0x1001FE88
pascal void HandleSmoke(SpritePtr sprite)
{
	short frame;

	if (sprite->appLong < 0 || killSmoke) {
		numStreaks--;
		sprite->face = NULL;
		sprite->task = NULL;
	} else {
		if (sprite->mode == 1 || sprite->mode == 3) {
			if (sprite->mode == 3)
				frame = sprite->appLong / 2;
			else
				frame = sprite->appLong;
			if (frame <= 2) {
				sprite->face = streakFaces[sprite->layer][frame];
			} else if (frame <= 5) {
				sprite->face = streakFaces[sprite->layer][5 - frame];
			} else {
				sprite->face = NULL;
				sprite->appLong = -1;
			}
		} else {
			if (sprite->mode == 2)
				frame = sprite->appLong / 2;
			else
				frame = sprite->appLong;
			if (frame < 8) {
				sprite->face = streakFaces[sprite->layer][frame];
			} else {
				sprite->face = NULL;
				sprite->appLong = -1;
			}
		}
		sprite->position.h = screenCenter.h + (sprite->speed.h - shipData->pos.x) - GetSpriteXSize(sprite->face) / 2;
		sprite->position.v = screenCenter.v + (sprite->speed.v - shipData->pos.y) - GetSpriteYSize(sprite->face) / 2;
		if (thirdFrame)
			sprite->appLong++;
		if (settings.highDetail)
			RefreshSprite(sprite);
	}
}

// FUNCTION: EVO 0x10020184
pascal void HandleStars(SpritePtr sprite)
{
	StarSlot *star;
	short w, h, dw, dh;
	Boolean offscreen;
	short i;

	if (sprite->appPtr == NULL) {
		sprite->task = NULL;
	} else {
		star = (StarSlot *)sprite->appPtr;
		if (star->type == 1)
			sprite->face = starFaces[1];
		else
			sprite->face = starFaces[0];
		w = GetSpriteXSize(sprite->face);
		h = GetSpriteYSize(sprite->face);
		dw = (gSAT.wind.bounds.right - kStatusPanelWidth) - gSAT.wind.bounds.left;
		dh = gSAT.wind.bounds.bottom - gSAT.wind.bounds.top;
		sprite->position.h = star->pos.x - shipData->pos.x;
		sprite->position.v = star->pos.y - shipData->pos.y;
		offscreen = false;
		if (sprite->position.h < gSAT.wind.bounds.left - w) {
			offscreen = true;
			star->pos.x += dw + w * 2;
		}
		if (sprite->position.h > gSAT.wind.bounds.right - kStatusPanelWidth + w) {
			offscreen = true;
			star->pos.x -= dw + w * 2;
		}
		if (sprite->position.v < gSAT.wind.bounds.top - h) {
			offscreen = true;
			star->pos.y += dh + h * 2;
		}
		if (sprite->position.v > gSAT.wind.bounds.bottom + h) {
			offscreen = true;
			star->pos.y -= dh + h * 2;
		}
		if (offscreen) {
			sprite->face = NULL;
		} else {
			sprite->position.h = star->pos.x - shipData->pos.x;
			sprite->position.v = star->pos.y - shipData->pos.y;
		}
		if (settings.highDetail)
			RefreshSprite(sprite);
	}
}

// FUNCTION: EVO 0x1002043c
pascal void HandleDoubleTimeSprite(SpritePtr sprite)
{
	if (doubleTime) {
		sprite->face = doubleSpeedFace;
		sprite->position.h = sprite->position.v = 0;
	} else {
		sprite->face = NULL;
	}
}

// FUNCTION: EVO 0x10020474
pascal void HandleAlertSprite(SpritePtr sprite)
{
	if (blinkTimer > 0) {
		if (blinkTimer % 2 == 0)
			sprite->face = hudOrbFaces[0];
		else
			sprite->face = hudOrbFaces[1];
		sprite->position.h = screenCenter.h * 2 - GetSpriteXSize(sprite->face);
		sprite->position.v = 0;
		if (frameCount % 5 == 0)
			blinkTimer--;
	} else {
		sprite->face = NULL;
	}
}

// FUNCTION: EVO 0x10020548
pascal void HandleStellarSprites(SpritePtr sprite)
{
	SpobRecord *spob;
	short hw, hh;

	sprite->face = NULL;
	if (sprite->appPtr == NULL) {
		sprite->task = NULL;
	} else {
		spob = (SpobRecord *)sprite->appPtr;
		if (spob->system == shipData->curSystem && spob->loaded) {
			sprite->face = spobFaces[spob->type];
			hw = GetSpriteXSize(sprite->face) / 2;
			hh = GetSpriteYSize(sprite->face) / 2;
			sprite->position.h = screenCenter.h + (spob->pos.h - shipData->pos.x) - hw;
			sprite->position.v = screenCenter.v + (spob->pos.v - shipData->pos.y) - hh;
		} else {
			sprite->face = NULL;
			sprite->task = NULL;
			spob->spawned = false;
		}
	}
}

// FUNCTION: EVO 0x10020728
pascal void HandleNavTargetSprites(SpritePtr sprite)
{
	short dw, dh, size;

	sprite->face = NULL;
	if (shipData->navTargetKind == kNavStellar && shipData->targetStellar != -1
			&& shipData->curSystem == stellarData[shipData->targetStellar].system) {
		if (stellarData[shipData->targetStellar].dominated)
			sprite->face = dockFaces[1][sprite->appLong];
		else
			sprite->face = dockFaces[0][sprite->appLong];
		sprite->position.h = (screenCenter.h + (stellarData[shipData->targetStellar].pos.h - shipData->pos.x)) - 0.5 * GetSpriteXSize(sprite->face);
		sprite->position.v = (screenCenter.v + (stellarData[shipData->targetStellar].pos.v - shipData->pos.y)) - 0.5 * GetSpriteYSize(sprite->face);
		dw = GetSpriteXSize(spobFaces[stellarData[shipData->targetStellar].type]) / 2 - GetSpriteXSize(sprite->face) / 2 + 3;
		dh = GetSpriteYSize(spobFaces[stellarData[shipData->targetStellar].type]) / 2 - GetSpriteYSize(sprite->face) / 2 + 3;
		if (dw > dh)
			size = dw;
		else
			size = dh;
		if (size < 16)
			size = 16;
		if (sprite->appLong == 0) {
			sprite->position.h -= size;
			sprite->position.v -= size;
		}
		if (sprite->appLong == 1) {
			sprite->position.h += size;
			sprite->position.v -= size;
		}
		if (sprite->appLong == 2) {
			sprite->position.h += size;
			sprite->position.v += size;
		}
		if (sprite->appLong == 3) {
			sprite->position.h -= size;
			sprite->position.v += size;
		}
		if (settings.highDetail)
			RefreshSprite(sprite);
	}
}

// FUNCTION: EVO 0x10020AD4
pascal void HandleWeapTargetSprites(SpritePtr sprite)
{
	short hw, hh, size;
	short i;

	sprite->face = NULL;
	if (shipData->hasTargetLock == 1 && shipData->targetShip != -1
			&& shipData->curSystem == shipData[shipData->targetShip].curSystem
			&& shipData[shipData->targetShip].active
			&& shipData[shipData->targetShip].hasWorldSpriteNode) {
		if (IsDisabled(&shipData[shipData->targetShip])) {
			sprite->face = targetFaces[3][sprite->appLong];
		} else if (shipData[shipData->targetShip].parentShip == 0 && shipData[shipData->targetShip].defenseStellar == -1) {
			sprite->face = targetFaces[2][sprite->appLong];
		} else if (IsThreatToPlayer(&shipData[shipData->targetShip])) {
			sprite->face = targetFaces[0][sprite->appLong];
		} else {
			sprite->face = targetFaces[1][sprite->appLong];
		}
		sprite->position.h = screenCenter.h + (shipData[shipData->targetShip].pos.x - shipData->pos.x) - GetSpriteXSize(sprite->face) / 2;
		sprite->position.v = screenCenter.v + (shipData[shipData->targetShip].pos.y - shipData->pos.y) - GetSpriteYSize(sprite->face) / 2;
		hw = GetSpriteXSize(shipFaces[shipData[shipData->targetShip].type][shipData[shipData->targetShip].heading / kHeadingStep]) / 2;
		hh = GetSpriteYSize(shipFaces[shipData[shipData->targetShip].type][shipData[shipData->targetShip].heading / kHeadingStep]) / 2;
		if (hw > hh)
			size = hw;
		else
			size = hh;
		if (sprite->appLong == 0) {
			sprite->position.h -= size;
			sprite->position.v -= size;
		}
		if (sprite->appLong == 1) {
			sprite->position.h += size;
			sprite->position.v -= size;
		}
		if (sprite->appLong == 2) {
			sprite->position.h += size;
			sprite->position.v += size;
		}
		if (sprite->appLong == 3) {
			sprite->position.h -= size;
			sprite->position.v += size;
		}
		if (settings.highDetail)
			RefreshSprite(sprite);
	}
}

// FUNCTION: EVO 0x10020F20
void HandleComm(void)
{
	if (commCount >= 0) {
		commCount--;
		if (commCount <= 0) {
			SATSetPortBackScreen();
			ForeColor(blackColor);
			PaintRect(&msgRect);
			SATSetPortScreen();
			CopyBits(&gSAT.backScreen.port->portBits, &gSAT.wind.port->portBits, &msgRect, &msgRect, srcCopy, NULL);
			CopyBits(&gSAT.backScreen.port->portBits, &gSAT.offScreen.port->portBits, &msgRect, &msgRect, srcCopy, NULL);
		}
	}
}

// FUNCTION: EVO 0x10020FFC
void HandleBeams(void)
{
	short angle;
	long unused1;
	short zero, massDmg, energyDmg;
	short unused2;
	short i, j, hit, n, h, k;
	short m, limit, arc;
	long reach, best, d2;
	Boolean ok;
	FloatPoint spot, tmp, head, start;

	SATSetPortScreen();
	for (i = 0; i < MaxBeamsOnScreen; i++) {
		if (beamData[i].life > -1 && beamData[i].owner != -1) {
			beamData[i].life--;
			if (killShots)
				beamData[i].life = -1;
			beamData[i].lastHead = beamData[i].head;
			beamData[i].lastTail = beamData[i].tail;
			angle = shipData[beamData[i].owner].heading;
			if (beamData[i].target != -1 && shipData[beamData[i].target].active)
				angle = CalcInterceptAngle(shipData[beamData[i].owner].pos, shipData[beamData[i].target].pos);
			if (gWeapons[beamData[i].weapType].inaccuracy > 0)
				angle += Rand(gWeapons[beamData[i].weapType].inaccuracy << 1) - gWeapons[beamData[i].weapType].inaccuracy;
			start.x = shipData[beamData[i].owner].pos.x;
			start.y = shipData[beamData[i].owner].pos.y;
			if (beamData[i].fixedRange != 0) {
				Accel((angle + 90) % kFullCircle, beamData[i].fixedRange, &start);
			} else if (beamData[i].sourceShip) {
				h = 0.5 * GetSpriteYSize(shipFaces[shipData[beamData[i].owner].type][shipData[beamData[i].owner].heading / kHeadingStep]);
				Accel(angle, h, &start);
			}
			head.x = screenCenter.h + (start.x - shipData->pos.x);
			head.y = screenCenter.v + (start.y - shipData->pos.y);
			beamData[i].head.h = head.x;
			beamData[i].head.v = head.y;
			if (beamData[i].life >= 0) {
				long dist;
				char unused3[20];
				if (beamData[i].head.h > gSAT.wind.bounds.right - 152)
					beamData[i].head.h -= beamData[i].head.h - (gSAT.wind.bounds.right - 152);
				hit = -1;
				for (j = 0; j < MaxShipsInSystem; j++) {
					ok = true;
					if (shipData[j].parentShip != -1) {
						if (shipData[j].parentShip == beamData[i].owner)
							ok = false;
						if (AlliedWithPlayer(&shipData[j]) && AlliedWithPlayer(&shipData[beamData[i].owner]))
							ok = false;
					}
					if (j == shipData[beamData[i].owner].parentShip)
						ok = false;
					if (j != beamData[i].owner && ok && shipData[j].active
							&& shipData[j].curSystem == shipData[beamData[i].owner].curSystem && shipData[j].type != kEscapePodShip) {
						h = 0.66 * GetSpriteYSize(shipFaces[shipData[j].type][0]);
						reach = 2.0f * gWeapons[beamData[i].weapType].speed + h / 2;
						if (XYFloatSquaredDist(&shipData[j].pos, &start) <= reach * reach) {
							zero = 0;
							spot.x = shipData[beamData[i].owner].pos.x;
							spot.y = shipData[beamData[i].owner].pos.y;
							k = CalcInterceptAngle(shipData[beamData[i].owner].pos, shipData[j].pos);
							arc = h * 10 / 32;
							if (abs(k - angle) <= arc) {
								d2 = XYFloatSquaredDist(&shipData[j].pos, &shipData[beamData[i].owner].pos);
								if (d2 < best || hit == -1) {
									best = d2;
									hit = j;
								}
							}
						}
					}
				}
				if (hit != -1) {
					j = hit;
					spot.x = -25.0f + (shipData[j].pos.x + Rand(50));
					spot.y = -25.0f + (shipData[j].pos.y + Rand(50));
					if (gWeapons[beamData[i].weapType].explodType == kExplodSmall)
						SpawnExplod(spot, beamData[i].owner, 0, 0);
					if (gWeapons[beamData[i].weapType].explodType == kExplodBig)
						SpawnExplod(spot, beamData[i].owner, 1, 0);
					if (gWeapons[beamData[i].weapType].explodType == kExplodHuge) {
						SpawnExplod(spot, beamData[i].owner, 2, 0);
						n = gWeapons[beamData[i].weapType].blastRadius;
						limit = 2.0f * (float)((float)n / 50.0);
						for (m = 0; m < limit; m++) {
							tmp.x = spot.x + Rand(0.5 * n) - 0.25 * n;
							tmp.y = spot.y + Rand(0.5 * n) - 0.25 * n;
							SpawnExplod(tmp, beamData[i].owner, 1, Rand(8) + 4);
						}
						limit = 8.0f * (float)((float)n / 50.0);
						for (m = 0; m < limit; m++) {
							tmp.x = spot.x + Rand(n) - 0.5 * n;
							tmp.y = spot.y + Rand(n) - 0.5 * n;
							SpawnExplod(tmp, beamData[i].owner, 0, Rand(16) + 8);
						}
					}
					massDmg = gWeapons[beamData[i].weapType].massDmg;
					energyDmg = gWeapons[beamData[i].weapType].energyDmg;
					if (shipData[j].persType != kPersAmbrosiaMascot) {
						if (beamData[i].target != -1) {
							if (j == beamData[i].target)
								DamageShip(&shipData[j], shipData[beamData[i].owner].pos, gWeapons[beamData[i].weapType].impact,
										massDmg, energyDmg, beamData[i].owner, 1, 1, 0, 0);
							else
								DamageShip(&shipData[j], shipData[beamData[i].owner].pos, gWeapons[beamData[i].weapType].impact,
										massDmg, energyDmg, beamData[i].owner, 1, 0, 0, 0);
						} else {
							if (j == shipData[beamData[i].owner].targetShip)
								DamageShip(&shipData[j], shipData[beamData[i].owner].pos, gWeapons[beamData[i].weapType].impact,
										massDmg, energyDmg, beamData[i].owner, 1, 1, 0, 0);
							else
								DamageShip(&shipData[j], shipData[beamData[i].owner].pos, gWeapons[beamData[i].weapType].impact,
										massDmg, energyDmg, beamData[i].owner, 1, 0, 0, 0);
						}
						if (gWeapons[beamData[i].weapType].impact < 0 && j != 0)
							shipData[j].isTractored = true;
						if (beamData[i].owner == 0 && (gWeapons[beamData[i].weapType].massDmg > 0
								|| gWeapons[beamData[i].weapType].energyDmg > 0))
							DoGoodSamaritan(&shipData[j], &shipData[beamData[i].owner]);
						if (shipData[j].aiType >= kAIWimpyTrader
								&& (shipData[j].govt != shipData[beamData[i].owner].govt || shipData[j].govt == -1)
								&& beamData[i].owner != -1 && beamData[i].owner != shipData[j].parentShip
								&& shipData[j].jumpTimer <= 0) {
							shipData[j].targetShip = beamData[i].owner;
							if (beamData[i].target == -1)
								shipData[j].grudge = 1;
							else if (j == beamData[i].target)
								shipData[j].grudge = 1;
						}
					}
					if (j == 0)
						hudRedraw.shield = true;
					dist = sqrt(XYFloatSquaredDist(&start, &shipData[hit].pos)) - 0.3 * GetSpriteYSize(shipFaces[shipData[hit].type][0]);
					Accel(angle, dist, &spot);
				} else {
					dist = 2.0 * gWeapons[beamData[i].weapType].speed;
				}
				spot = head;
				Accel(angle, dist, &spot);
				beamData[i].tail.h = spot.x;
				beamData[i].tail.v = spot.y;
				if (beamData[i].tail.h >= gSAT.wind.bounds.right - 152)
					beamData[i].tail.h = gSAT.wind.bounds.right - 153;
			}
				} else {
			if (beamData[i].life == -1)
				beamData[i].life--;
			else
				beamData[i].owner = -1;
		}
	}
}

// FUNCTION: EVO 0x10022648
void EraseBeams(void)
{
	short i;
	short j;

	for (i = 0; i < MaxBeamsOnScreen; i++) {
		if (beamData[i].life >= -1) {
			if (beamData[i].owner != -1) {
				if (beamData[i].life > 0 && beamData[i].head.h == beamData[i].lastHead.h
						&& beamData[i].head.v == beamData[i].lastHead.v) {
					if (frameCount % 3 != 0)
						continue;
				}
				if (beamData[i].lastHead.h <= gSAT.wind.bounds.right - 149) {
					if (beamData[i].lastTail.h <= gSAT.wind.bounds.right - 152) {
						SATSetPortScreen();
						ForeColor(blackColor);
						MoveTo(beamData[i].lastHead.h, beamData[i].lastHead.v);
						PenSize(gWeapons[beamData[i].weapType].proxRadius + 2, gWeapons[beamData[i].weapType].proxRadius + 2);
						LineTo(beamData[i].lastTail.h, beamData[i].lastTail.v);
						PenSize(1, 1);
					}
				}
			}
		}
	}
	SATSetPortScreen();
	PenSize(1, 1);
	ForeColor(blackColor);
}

// FUNCTION: EVO 0x100228AC
void DrawBeams(void)
{
	short i;
	long j[3];

	for (i = 0; i < MaxBeamsOnScreen; i++) {
		if (beamData[i].life >= 0) {
			if (beamData[i].owner != -1) {
				if (beamData[i].head.h <= gSAT.wind.bounds.right - 149) {
					if (beamData[i].tail.h <= gSAT.wind.bounds.right - 152) {
						SATSetPortScreen();
						if (gWeapons[beamData[i].weapType].graphic >= kBeamYellow) {
							if (gWeapons[beamData[i].weapType].graphic == kBeamRed)
								ForeColor(redColor);
							else if (gWeapons[beamData[i].weapType].graphic == kBeamGreen)
								ForeColor(greenColor);
							else if (gWeapons[beamData[i].weapType].graphic == kBeamBlue)
								ForeColor(blueColor);
							else if (gWeapons[beamData[i].weapType].graphic == kBeamCyan)
								ForeColor(cyanColor);
							else if (gWeapons[beamData[i].weapType].graphic == kBeamMagenta)
								ForeColor(magentaColor);
							else if (gWeapons[beamData[i].weapType].graphic == kBeamYellow)
								ForeColor(yellowColor);
							MoveTo(beamData[i].head.h, beamData[i].head.v);
							PenSize(gWeapons[beamData[i].weapType].proxRadius, gWeapons[beamData[i].weapType].proxRadius);
							LineTo(beamData[i].tail.h, beamData[i].tail.v);
						} else {
							if (gWeapons[beamData[i].weapType].graphic < kBeamYellow) {
								if (gWeapons[beamData[i].weapType].graphic == kBeamMagentaRed)
									ForeColor(redColor);
								if (gWeapons[beamData[i].weapType].graphic == kBeamYellowGreen)
									ForeColor(greenColor);
								if (gWeapons[beamData[i].weapType].graphic == kBeamCyanBlue)
									ForeColor(blueColor);
								MoveTo(beamData[i].head.h, beamData[i].head.v);
								PenSize(gWeapons[beamData[i].weapType].proxRadius, gWeapons[beamData[i].weapType].proxRadius);
								LineTo(beamData[i].tail.h, beamData[i].tail.v);
								if (gWeapons[beamData[i].weapType].graphic == kBeamMagentaRed)
									ForeColor(magentaColor);
								if (gWeapons[beamData[i].weapType].graphic == kBeamYellowGreen)
									ForeColor(yellowColor);
								if (gWeapons[beamData[i].weapType].graphic == kBeamCyanBlue)
									ForeColor(cyanColor);
								MoveTo(beamData[i].head.h, beamData[i].head.v);
								PenSize(0.5 * gWeapons[beamData[i].weapType].proxRadius, 0.5 * gWeapons[beamData[i].weapType].proxRadius);
								LineTo(beamData[i].tail.h, beamData[i].tail.v);
							}
						}
						PenSize(1, 1);
						ForeColor(blackColor);
					}
				}
			}
		}
	}
	SATSetPortScreen();
	PenSize(1, 1);
	ForeColor(blackColor);
}

// FUNCTION: EVO 0x10022F44
void SpawnBeam(short owner, short target, short weapType, short angle, Boolean sourceShip, short fixedRange)
{
	short i;

	for (i = 0; i < MaxBeamsOnScreen; i++) {
		if (beamData[i].life < -1) {
			beamData[i].life = gWeapons[weapType].lifetime;
			beamData[i].weapType = weapType;
			beamData[i].sourceShip = sourceShip;
			beamData[i].owner = owner;
			beamData[i].fixedRange = fixedRange;
			if (gWeapons[weapType].guidance == kGuidanceTurretBeam)
				beamData[i].target = target;
			else
				beamData[i].target = -1;
			return;
		}
	}
}

// FUNCTION: EVO 0x10023060
void NoOp(void)
{
}

// FUNCTION: EVO 0x10023064
void HandleVoiceSound(void)
{
	if (voiceSnd != NULL && CountMatchingSoundVoices((long)voiceSnd) == 0) {
		DisposePtr(voiceSnd);
		voiceSnd = NULL;
	}
	if (pendingVoiceMsg != -1) {
		if (voiceSnd != NULL) {
			if (CountMatchingSoundVoices((long)voiceSnd)) {
				FlushMixQueueEntries((long)voiceSnd);
			} else {
				DisposePtr(voiceSnd);
				voiceSnd = NULL;
			}
		} else {
			if (pendingVoiceMsg >= 0 && pendingVoiceMsg <= 3 && voiceCount[pendingVoiceMsg] > 0) {
				voiceSnd = LoadSndResource(pendingVoiceMsg * 10 + 700 + Rand(voiceCount[pendingVoiceMsg]));
				PlaySound(voiceSnd, 15, kMidVolume, kMidVolume);
			}
			pendingVoiceMsg = -1;
		}
	}
}

// FUNCTION: EVO 0x100231A4
void KillVoiceSound(void)
{
	if (voiceSnd != NULL) {
		FlushMixQueueEntries((long)voiceSnd);
		CountMatchingSoundVoices((long)voiceSnd);
		DisposePtr(voiceSnd);
		voiceSnd = NULL;
	}
	pendingVoiceMsg = -1;
}

// FUNCTION: EVO 0x10023210
void WarpUpCallback(long which, long a, long b)
{
	if (which == 1 && !warpSoundDone) {
		warpSoundDone = true;
		PlaySound(warpOutSnd, 50, kMidVolume, kMidVolume);
	}
}

// FUNCTION: EVO 0x10023274
void HandleGlobalPriceFlux(void)
{
	short i, r;

	for (i = 0; i <= 1; i++) {
		r = Rand(3);
		if (r == 0)
			globalPriceFlux[i]--;
		if (r == 1)
			globalPriceFlux[i]++;
		if (globalPriceFlux[i] > 115)
			globalPriceFlux[i] = 115;
		if (globalPriceFlux[i] < 85)
			globalPriceFlux[i] = 85;
	}
}

// FUNCTION: EVO 0x10023358
void HandleLocalPriceFlux(void)
{
	short i;

	for (i = 0; i <= 1; i++)
		localPriceFlux[i] = Rand(21) + 90;
}

// FUNCTION: EVO 0x100233BC
void HandleShotGuidance(short s)
{
	short bearing, rel, rem, j, i;
	short diff, bitB;
	double turn;
	long pad;
	FloatPoint v, d;

	if (shotData[s].guidanceState == kShotStateNormal) {
		if ((shotData[s].target == -1
					|| (shipData[shotData[s].target].active && shotData[s].curSystem == shipData[shotData[s].target].curSystem))
				&& gWeapons[shotData[s].weapType].guidance == kGuidanceHoming) {
			if (shotData[s].target != -1)
				bearing = CalcInterceptAngle(shotData[s].pos, shipData[shotData[s].target].pos);
			else
				bearing = shotData[s].angle;
			if (1.5 * gWeapons[shotData[s].weapType].lifetime - shotData[s].life > 15.0 * timeScale) {
				turn = CategorizeWeapon(shotData[s].weapType);
				diff = kSeekJamHalf1;
				bitB = kSeekJamFull1;
				if (shotData[s].target != -1) {
					for (j = 0; j <= 3; j++) {
						if ((diff & gWeapons[shotData[s].weapType].seeker) && ShipECM(&shipData[shotData[s].target], j) != 0
								&& Rand(2) == 0) {
							if (turn > 0.0)
								turn *= -1.0;
							if ((gWeapons[shotData[s].weapType].seeker & kSeekHitParent) && Rand(1000) == 0
									&& shotData[s].owner != -1) {
								shotData[s].target = shotData[s].owner;
								shotData[s].owner = -1;
							}
						}
						if ((bitB & gWeapons[shotData[s].weapType].seeker) && ShipECM(&shipData[shotData[s].target], j) != 0) {
							if (turn > 0.0)
								turn *= -1.0;
							if ((gWeapons[shotData[s].weapType].seeker & kSeekHitParent) && Rand(500) == 0
									&& shotData[s].owner != -1) {
								shotData[s].target = shotData[s].owner;
								shotData[s].owner = -1;
							}
						}
						diff = diff << 1;
						bitB = bitB << 1;
					}
				}
				if (shotData[s].guidanceState == kShotStateNormal) {
					if (shotData[s].target == 0 && gInvertedColors) {
						turn = 0.0;
						if ((gWeapons[shotData[s].weapType].seeker & kSeekHitParent) && Rand(1000) == 0
								&& shotData[s].owner != -1) {
							shotData[s].target = shotData[s].owner;
							shotData[s].owner = -1;
						}
					}
					if ((gWeapons[shotData[s].weapType].seeker & kSeekLoseLock) && shotData[s].target != -1
							&& abs((short)(shipData[shotData[s].target].pos.x - shotData[s].pos.x)) < 250
							&& abs((short)(shipData[shotData[s].target].pos.y - shotData[s].pos.y)) < 250) {
						rem = abs(CalcInterceptAngle(shotData[s].pos, shipData[shotData[s].target].pos) - shotData[s].angle) % kFullCircle;
						if (rem > 45)
							shotData[s].target = -1;
					}
				}
				diff = abs(bearing - shotData[s].angle);
				if (diff > abs(turn)) {
					rel = bearing - shotData[s].angle;
					if (rel >= kFullCircle)
						rel -= kFullCircle;
					if (rel < 0)
						rel = rel + kFullCircle;
					if (rel > kHalfCircle)
						shotData[s].angle -= turn * timeScale;
					else
						shotData[s].angle += turn * timeScale;
				}
			}
			if (shotData[s].angle >= kFullCircle)
				shotData[s].angle -= kFullCircle;
			if (shotData[s].angle < 0)
				shotData[s].angle += kFullCircle;
			shotData[s].vel.x = shotData[s].vel.y = 0.0;
			Accel(shotData[s].angle, gWeapons[shotData[s].weapType].speed, &shotData[s].vel);
			SpeedLimit(&shotData[s].vel, gWeapons[shotData[s].weapType].speed);
		}
		if (gWeapons[shotData[s].weapType].guidance == kGuidanceHoming
				&& (gWeapons[shotData[s].weapType].seeker & kSeekAsteroidDecoy) && Rand(10) == 0) {
			for (i = 0; i < MaxAsteroids; i++) {
				if (asteroidData[i].active
						&& abs((short)(asteroidData[i].pos.x - shotData[s].pos.x)) < 200
						&& abs((short)(asteroidData[i].pos.y - shotData[s].pos.y)) < 200) {
					rem = abs(CalcInterceptAngle(shotData[s].pos, asteroidData[i].pos) - shotData[s].angle) % kFullCircle;
					if (rem <= 15) {
						shotData[s].guidanceState = kShotStateAsteroidDecoy;
						shotData[s].target = i;
						break;
					}
				}
			}
		}
		if (shotData[s].guidanceState == kShotStateNormal && (gWeapons[shotData[s].weapType].seeker & kSeekFlareDecoy)
				&& gWeapons[shotData[s].weapType].guidance == kGuidanceHoming && Rand(15) == 0) {
			for (i = 0; i < MaxShotsOnScreen; i++) {
				if (shotData[i].life > 0 && shotData[i].owner != shotData[s].owner
						&& (gWeapons[shotData[i].weapType].flags & kWeapDecoy)
						&& abs((short)(shotData[i].pos.x - shotData[s].pos.x)) < 200
						&& abs((short)(shotData[i].pos.y - shotData[s].pos.y)) < 200) {
					rem = abs(CalcInterceptAngle(shotData[s].pos, shotData[i].pos) - shotData[s].angle) % kFullCircle;
					if (rem <= 10) {
						shotData[s].guidanceState = kShotStateFlareDecoy;
						shotData[s].target = i;
						break;
					}
				}
			}
		}
	} else if (shotData[s].guidanceState == kShotStateConfused) {
		if (1.5 * gWeapons[shotData[s].weapType].lifetime - shotData[s].life > 15.0) {
			if (frameCount % 200 < 100)
				shotData[s].angle -= CategorizeWeapon(shotData[s].weapType);
			else
				shotData[s].angle += CategorizeWeapon(shotData[s].weapType);
		}
		if (shotData[s].angle >= kFullCircle)
			shotData[s].angle -= kFullCircle;
		if (shotData[s].angle < 0)
			shotData[s].angle += kFullCircle;
		shotData[s].vel.x = shotData[s].vel.y = 0.0;
		Accel(shotData[s].angle, gWeapons[shotData[s].weapType].speed, &shotData[s].vel);
		SpeedLimit(&shotData[s].vel, gWeapons[shotData[s].weapType].speed);
		if ((gWeapons[shotData[s].weapType].seeker & kSeekHitParent) && Rand(1000) == 0 && shotData[s].owner != -1) {
			shotData[s].guidanceState = kShotStateNormal;
			shotData[s].target = shotData[s].owner;
			shotData[s].owner = -1;
		}
	} else {
		if (shotData[s].guidanceState == kShotStateAsteroidDecoy) {
			if (shotData[s].target != -1) {
				if (asteroidData[shotData[s].target].active)
					bearing = CalcInterceptAngle(shotData[s].pos, asteroidData[shotData[s].target].pos);
				else
					shotData[s].target = -1;
			} else
				bearing = shotData[s].angle;
		}
		if (shotData[s].guidanceState == kShotStateFlareDecoy) {
			if (shotData[s].target != -1) {
				if (shipData[shotData[s].target].active)
					bearing = CalcInterceptAngle(shotData[s].pos, shotData[shotData[s].target].pos);
				else
					shotData[s].target = -1;
			} else
				bearing = shotData[s].angle;
		}
		if (1.5 * gWeapons[shotData[s].weapType].lifetime - shotData[s].life > 15.0) {
			diff = abs(bearing - shotData[s].angle);
			turn = CategorizeWeapon(shotData[s].weapType);
			if (diff > turn) {
				rel = bearing - shotData[s].angle;
				if (rel >= kFullCircle)
					rel -= kFullCircle;
				if (rel < 0)
					rel += kFullCircle;
				if (rel > kHalfCircle)
					shotData[s].angle -= turn;
				else
					shotData[s].angle += turn;
			}
		}
		if (shotData[s].angle >= kFullCircle)
			shotData[s].angle -= kFullCircle;
		if (shotData[s].angle < 0)
			shotData[s].angle += kFullCircle;
		shotData[s].vel.x = shotData[s].vel.y = 0.0;
		Accel(shotData[s].angle, gWeapons[shotData[s].weapType].speed, &shotData[s].vel);
		SpeedLimit(&shotData[s].vel, gWeapons[shotData[s].weapType].speed);
	}
	if (gWeapons[shotData[s].weapType].guidance == kGuidanceRocket) {
		v.x = shotData[s].vel.x;
		v.y = shotData[s].vel.y;
		d.x = d.y = 0;
		Accel(shotData[s].angle, gWeapons[shotData[s].weapType].speed, &d);
		shotData[s].vel.x = 0.01 * (95.0f * v.x + 5.0f * d.x);
		shotData[s].vel.y = 0.01 * (95.0f * v.y + 5.0f * d.y);
	}
	if (gWeapons[shotData[s].weapType].guidance == kGuidanceFreefall) {
		v.x = v.y = 0;
		d.x = 1000.0 * shotData[s].vel.x;
		d.y = 1000.0 * shotData[s].vel.y;
		bearing = CalcInterceptAngle(v, d);
		diff = abs(bearing - shotData[s].angle);
		if (diff > 0) {
			rel = bearing - shotData[s].angle;
			if (rel >= kFullCircle)
				rel -= kFullCircle;
			if (rel < 0)
				rel += kFullCircle;
			if (rel > kHalfCircle)
				shotData[s].angle--;
			else
				shotData[s].angle++;
		}
	}
}

// FUNCTION: EVO 0x10024C38
void HandleStellarSystemVisibility(void)
{
	short i, j, k;
	Boolean wasVisible;

	for (i = 0; i < MaxSystems; i++) {
		wasVisible = gSystems[i].loaded;
		gSystems[i].loaded = false;
		if (gSystems[i].govt > kNoSystemGovt) {
			if (gSystems[i].visibility <= -1) {
				gSystems[i].loaded = true;
			} else if (gSystems[i].visibility < NumMissionBits && gSystems[i].visibility >= 0) {
				if (missionBit[gSystems[i].visibility])
					gSystems[i].loaded = true;
				else
					gSystems[i].loaded = false;
			} else if (gSystems[i].visibility >= kNotBitBase && gSystems[i].visibility < kNotBitBase + NumMissionBits) {
				if (missionBit[gSystems[i].visibility - kNotBitBase] == 0)
					gSystems[i].loaded = true;
				else
					gSystems[i].loaded = false;
			}
		}
		if (gSystems[i].loaded) {
			for (j = 0; j < MaxSystems; j++) {
				if (j != i && gSystems[j].xPos == gSystems[i].xPos && gSystems[j].yPos == gSystems[i].yPos
						&& gSystems[j].visited > gSystems[i].visited)
					gSystems[i].visited = gSystems[j].visited;
			}
		} else if (wasVisible) {
			if (shipData->navTargetKind == kNavHyperspace && shipData->targetStellar == i) {
				shipData->targetStellar = -1;
				hudRedraw.nav = true;
			}
			for (j = 0; j < kMaxHyperRoute; j++) {
				if (i == hyperRoute[j]) {
					for (k = j; k < kMaxHyperRoute; k++)
						hyperRoute[k] = -1;
					break;
				}
			}
		}
	}
	for (i = 0; i < MaxStellarObjects; i++) {
		stellarData[i].loaded = false;
		if (stellarData[i].system >= 0 && stellarData[i].system < MaxSystems
				&& gSystems[stellarData[i].system].loaded != 0)
			stellarData[i].loaded = true;
	}
}

// FUNCTION: EVO 0x10025060
void HandleTimeAdjustment(void)
{
	timeScale = gameSpeed;
}

// FUNCTION: EVO 0x10025074
void HandleShip(ShipObj *ship)
{
	Rect deadRect[4];
	short recharge;
	short range, i, heading, fired;
	Str255 deadText;
	short deadPad[2];
	short k, diff, shots, spread;
	Boolean fromShip;
	Boolean ok;
	Boolean pod;
	float d, turn, delta, reload;
	short slot;

	if (ship->targetShip != -1 && !shipData[ship->targetShip].active)
		ship->targetShip = -1;
	for (i = 0; i < MaxWeaponTypes; i++) {
		if (ship->weap[i].reload > 0)
			ship->weap[i].reload -= 1.0f;
		else
			ship->weap[i].reload = 0.0f;
	}
	if (IsDisabled(ship)) {
		ship->vel.x *= 0.995;
		ship->vel.y *= 0.995;
		if (ship->specialMission == -1 && ship->defenseStellar == -1 && ship->parentShip == 0) {
			if (gShipTypes[ship->type].inherentAI <= kAIBraveTrader)
				DestroyPartialFleetCargo(ship->index);
			ship->parentShip = -1;
			ship->hired = false;
			ship->boarded = true;
			ship->aiType = gShipTypes[ship->type].inherentAI;
			ShipStatsToSystemInfo();
			hudRedraw.credits = true;
		}
	}
	if (ship->persType != -1 && personData[ship->persType].hailQuote != -1
			&& personData[ship->persType].loaded
			&& shipData->jumpTimer <= 0 && !gInvertedColors
			&& !IsDying(ship) && !AIIsShipJumping(ship)) {
		ok = true;
		if (IsDisabled(ship)) {
			if (!(personData[ship->persType].flags & kPersQuoteIfDisabled))
				ok = false;
		} else if (personData[ship->persType].flags & kPersQuoteIfDisabled)
			ok = false;
		if (personData[ship->persType].flags & kPersQuoteOnAttack)
			ok = false;
		if ((personData[ship->persType].flags & kPersQuoteIfGrudge) && !personData[ship->persType].grudge)
			ok = false;
		if ((personData[ship->persType].flags & kPersQuoteIfLikes) && !AIDoesShipLikePlayer(ship))
			ok = false;
		if (personData[ship->persType].linkMission != -1 && (personData[ship->persType].flags & kPersQuietNoMission)) {
			drawGate = true;
			speakerShip = ship->index;
			if (!IsMissionAvailable(personData[ship->persType].linkMission))
				ok = false;
			drawGate = false;
			speakerShip = -1;
		}
		if ((personData[ship->persType].flags & kPersLeaveAfter) && AIIsShipLeaving(ship))
			ok = false;
		if ((personData[ship->persType].flags & kPersNoWimpyFreighter) && gShipTypes[shipData->type].inherentAI == kAIWimpyTrader)
			ok = false;
		if ((personData[ship->persType].flags & kPersNoBeefyFreighter) && gShipTypes[shipData->type].inherentAI == kAIBraveTrader)
			ok = false;
		if ((personData[ship->persType].flags & kPersNoWarship) && gShipTypes[shipData->type].inherentAI >= kAIWarship)
			ok = false;
		if ((personData[ship->persType].flags & kPersQuoteOnce) && ship->hailed)
			ok = false;
		if (ok && Rand(140) == 0 && commCount <= 0) {
			speakerShip = ship->index;
			ShowPersonHailQuote(personData[ship->persType].hailQuote);
			speakerShip = -1;
			ship->hailed = true;
		}
	}
	if (ship->isTractored) {
		d = 0.25 * ShipAccelRate(shipData);
		if (ship->vel.x > shipData->vel.x + d)
			ship->vel.x -= d;
		if (ship->vel.x < shipData->vel.x - d)
			ship->vel.x += d;
		if (ship->vel.y > shipData->vel.y + d)
			ship->vel.y -= d;
		if (ship->vel.y < shipData->vel.y - d)
			ship->vel.y += d;
		if (FloatAbs(ship->vel.x - shipData->vel.x) < d)
			ship->vel.x = shipData->vel.x;
		if (FloatAbs(ship->vel.y - shipData->vel.y) < d)
			ship->vel.y = shipData->vel.y;
	}
	ship->pos.x += ship->vel.x * timeScale;
	ship->pos.y += ship->vel.y * timeScale;
	if (!IsDisabled(ship)) {
		if (ship->aiTimer <= 0) {
			delta = FloatAbs((float)ship->desiredHeading - ship->heading);
			if (delta > ShipTurnRate(ship)) {
				turn = ship->desiredHeading - ship->heading;
				if (turn >= kFullCircle)
					turn -= kFullCircle;
				if (turn < 0)
					turn += kFullCircle;
				if (turn > kHalfCircle)
					ship->heading -= ShipTurnRate(ship);
				else
					ship->heading += ShipTurnRate(ship);
			} else {
				ship->heading = ship->desiredHeading;
			}
		}
		d = ShipShieldCapacity(ship);
		if (ship->shield < d) {
			recharge = ShipShieldRechargeRate(ship);
			if (recharge > 0 && frameCount % recharge == 0)
				ship->shield += 0.01 * d;
		}
	}
	if (ship->heading >= kFullCircle)
		ship->heading -= kFullCircle;
	if (ship->heading < 0)
		ship->heading += kFullCircle;
	if (ship->jumpTimer > 0 && !IsDisabled(ship)
			&& abs(ship->heading - ship->desiredHeading) <= ShipTurnRate(ship)) {
		if (AIIsShipJumping(ship)) {
			ship->vel.x *= 0.95;
			ship->vel.y *= 0.95;
			d = (gShipTypes[ship->type].jumpSpeedScale * (TickCount() - ship->jumpTick) /4.6)
					- 50.0 / gShipTypes[ship->type].jumpSpeedScale;
			if (d > 0.0)
				Accel(ship->heading, d, &ship->pos);
		} else {
			ship->jumpTimer = 0;
		}
	}
	if (ship->aiTimer <= 0 || ship->type == kEscapePodShip) {
		if (ship->thrust != 0.0) {
			if (ship->speedLimit == 0.0) {
				AdjustedAccel(ship->heading, ship->thrust, &ship->vel, ShipMaxSpeed(ship));
			} else if (ship->speedLimit > 0.0) {
				AdjustedAccel(ship->heading, ship->thrust, &ship->vel, (double)ship->speedLimit);
			} else {
				ship->vel.x = ship->vel.y = 0.0;
				Accel(ship->heading, FloatAbs(ship->speedLimit), &ship->vel);
				ship->speedLimit += FloatAbs(ship->thrust);
				if (ship->parentShip != -1 && ship->parentShip < MaxShipsInSystem) {
					if (ShipMaxSpeed(ship) < ShipMaxSpeed(&shipData[ship->parentShip]))
						d = ShipMaxSpeed(ship);
					else
						d = ShipMaxSpeed(&shipData[ship->parentShip]);
				} else {
					d = ShipMaxSpeed(ship);
				}
				if (ship->speedLimit >= -d) {
					AIMakeShipStopJumping(ship);
					if (ship->parentShip == -1)
						ship->aiTimer = Rand(30) + 60;
				}
			}
		}
		fired = 0;
		if (ship->fireQueued && ship->selectedWeapon != -1) {
			if (ship->weap[ship->selectedWeapon].reload <= 0 && ship->weap[ship->selectedWeapon].count > 0) {
				if (gWeapons[ship->selectedWeapon].flags & kWeapFireTogether)
					shots = ship->weap[ship->selectedWeapon].count;
				else
					shots = 1;
				for (k = 0; k < shots; k++) {
					if (WeaponHasAmmo(ship, ship->selectedWeapon)) {
						if (ship->targetShip != -1
								&& (gWeapons[ship->selectedWeapon].guidance == kGuidanceTurretBeam || gWeapons[ship->selectedWeapon].guidance == kGuidanceTurret)
								&& !TurretBlindSpot(ship, &shipData[ship->targetShip], ship->selectedWeapon)) {
							if (gWeapons[ship->selectedWeapon].guidance == kGuidanceTurretBeam)
								range = gWeapons[ship->selectedWeapon].speed;
							if (gWeapons[ship->selectedWeapon].guidance == kGuidanceTurret)
								range = 1.5 * (gWeapons[ship->selectedWeapon].speed * gWeapons[ship->selectedWeapon].lifetime);
							if (FloatAbs(ship->pos.x - shipData[ship->targetShip].pos.x) < range) {
								if (FloatAbs(ship->pos.y - shipData[ship->targetShip].pos.y) < range) {
									if (gWeapons[ship->selectedWeapon].guidance == kGuidanceTurretBeam)
										SpawnBeam(ship->index, ship->targetShip, ship->selectedWeapon,
											CalcInterceptAngle(ship->pos, shipData[ship->targetShip].pos), 0, 0);
									else
										SpawnShot(ship->index, ship->targetShip, ship->selectedWeapon);
									fired++;
								}
							}
						}
						if (ship->targetShip != -1
								&& (gWeapons[ship->selectedWeapon].guidance == kGuidanceFrontQuad || gWeapons[ship->selectedWeapon].guidance == kGuidanceRearQuad)) {
							range = 1.5 * (gWeapons[ship->selectedWeapon].speed * gWeapons[ship->selectedWeapon].lifetime);
							if (FloatAbs(ship->pos.x - shipData[ship->targetShip].pos.x) < range) {
								if (FloatAbs(ship->pos.y - shipData[ship->targetShip].pos.y) < range) {
									heading = CalcInterceptAngle(ship->pos, shipData[ship->targetShip].pos);
									if (gWeapons[ship->selectedWeapon].guidance == kGuidanceFrontQuad)
										diff = abs(heading - ship->heading) % kFullCircle;
									if (gWeapons[ship->selectedWeapon].guidance == kGuidanceRearQuad)
										diff = abs(heading - (ship->heading + kHalfCircle) % kFullCircle) % kFullCircle;
									if (diff <= 45) {
										SpawnShot(ship->index, ship->targetShip, ship->selectedWeapon);
										fired++;
									}
								}
							}
						}
						if (ship->targetShip != -1 && gWeapons[ship->selectedWeapon].guidance == kGuidanceHoming) {
							SpawnShot(ship->index, ship->targetShip, ship->selectedWeapon);
							fired++;
						}
						if (gWeapons[ship->selectedWeapon].guidance == kGuidanceUnguided || gWeapons[ship->selectedWeapon].guidance == kGuidanceBeam
								|| gWeapons[ship->selectedWeapon].guidance == kGuidanceRocket) {
							if (gWeapons[ship->selectedWeapon].guidance == kGuidanceBeam)
								range = gWeapons[ship->selectedWeapon].speed;
							if (gWeapons[ship->selectedWeapon].guidance == kGuidanceUnguided || gWeapons[ship->selectedWeapon].guidance == kGuidanceRocket)
								range = 1.5 * (gWeapons[ship->selectedWeapon].speed * gWeapons[ship->selectedWeapon].lifetime);
							if (gWeapons[ship->selectedWeapon].guidance == kGuidanceBeam) {
								spread = WeaponShotSpread(ship->index, ship->selectedWeapon);
								if (spread)
									fromShip = false;
								else
									fromShip = true;
								SpawnBeam(ship->index, ship->targetShip, ship->selectedWeapon, ship->heading, fromShip, spread);
							} else
								SpawnShot(ship->index, ship->targetShip, ship->selectedWeapon);
							fired++;
						}
						if (gWeapons[ship->selectedWeapon].ammoType != -1 && ship->persType != kPersAmbrosiaMascot) {
							if (gWeapons[ship->selectedWeapon].ammoType > 0) {
								ship->weap[ship->selectedWeapon].ammo--;
							} else if (gWeapons[ship->selectedWeapon].ammoType <= kAmmoFuelBase)
								ship->fuel -= abs(gWeapons[ship->selectedWeapon].ammoType) - 1000;
						}
					}
				}
			}
			if (fired > 0) {
				if (gWeapons[ship->selectedWeapon].sound >= 0) {
					if (gWeapons[ship->selectedWeapon].flags & kWeapLoopSound) {
						if (CountMatchingSoundVoices((long)weapSnd[gWeapons[ship->selectedWeapon].sound]) == 0)
							PlaySoundDistance(-1, weapSnd[gWeapons[ship->selectedWeapon].sound], 4, &ship->pos, &shipData->pos);
					} else {
						if (gWeapons[ship->selectedWeapon].guidance == kGuidanceBeam || gWeapons[ship->selectedWeapon].guidance == kGuidanceTurretBeam)
							PlaySoundDistance(3, weapSnd[gWeapons[ship->selectedWeapon].sound], 4, &ship->pos, &shipData->pos);
						else
							PlaySoundDistance(-1, weapSnd[gWeapons[ship->selectedWeapon].sound], 4, &ship->pos, &shipData->pos);
					}
				}
				reload = fired * ((float)(double)gWeapons[ship->selectedWeapon].reload / (float)ship->weap[ship->selectedWeapon].count);
				if (ship->targetShip == 0) {
					if (playerKills < 100)
						reload *= 1.75f;
					else if (playerKills < 400)
						reload *= 1.5f;
					else if (playerKills < 800)
						reload *= 1.25f;
					else if (playerKills < 1600)
						reload *= 1.1f;
				}
				ship->weap[ship->selectedWeapon].reload = reload;
				if (ship->altFireSide < 0)
					ship->altFireSide = 1;
				else
					ship->altFireSide = -1;
			}
		}
	} else
		ship->aiTimer--;
	if (ship->deathTimer > 0) {
		ship->deathTimer--;
		if (ship->parentShip == 0) {
			if (ship->aiType == kAIEscort && gShipTypes[ship->type].inherentAI <= kAIBraveTrader) {
				DestroyPartialFleetCargo(ship->index);
				hudRedraw.credits = true;
				DrawStatusCargo();
			}
			ship->parentShip = -1;
			ship->aiType = gShipTypes[ship->type].inherentAI;
		}
		if (ship->deathTimer == 0.5 * gShipTypes[ship->type].deathDelay && ship->persType != -1) {
			if (personData[ship->persType].flags & kPersPodAndAfterburner) {
				if (ship->govt != -1) {
					if (govtData[ship->govt].flags & kGovtNoPersPod)
						pod = false;
					else
						pod = true;
				} else
					pod = true;
			} else
				pod = false;
			if (pod) {
				k = GenericRandomShipSpawn(shipData->curSystem, 2);
				if (k != -1) {
					shipData[k].pos.x = ship->pos.x;
					shipData[k].pos.y = ship->pos.y;
					shipData[k].vel.x = 0.0;
					shipData[k].vel.y = 0.0;
					shipData[k].heading = ship->heading;
					shipData[k].type = kEscapePodShip;
					shipData[k].govt = -1;
					shipData[k].parentShip = -1;
					shipData[k].aiType = kAIWimpyTrader;
					shipData[k].aiTimer = 1000;
					shipData[k].shield = ShipShieldCapacity(&shipData[k]);
					PlaySoundDistance(-1, ejectSnd, 5, &shipData[k].pos, &shipData->pos);
				}
			}
		}
	}
	if (gShipTypes[ship->type].fuelRegen > 0 && Rand(abs(gShipTypes[ship->type].fuelRegen)) == 0)
		ship->fuel += 1.0f;
	delta = ShipFuelCapacity(ship);
	if (ship->fuel > delta)
		ship->fuel = delta;
	if (ship->fuel < 0.0f)
		ship->fuel = 0.0f;
	ship->isTractored = false;
}

void	HandleShotGuidance(short s);
void	SpawnSmoke(FloatPoint where, short mode, short layer);

// FUNCTION: EVO 0x100269A4
pascal void HandleShot(SpritePtr sprite)
{
	short s, n, w, i;
	short radius;
	Rect r1;
	FloatPoint where;
	Rect r2;
	Boolean hit, spin;

	if (sprite->appLong < 0 || sprite->appLong >= MaxShotsOnScreen) {
		sprite->task = nil;
	} else {
		s = sprite->appLong;
		if (shipData->curSystem == shotData[s].curSystem && shotData[s].life > 0) {
			shotData[s].life -= 1.0 / timeScale;
			if (killShots)
				shotData[s].life = kDeadMark;
			HandleShotGuidance(s);
			shotData[s].pos.x += timeScale * shotData[s].vel.x;
			shotData[s].pos.y += timeScale * shotData[s].vel.y;
			spin = gWeapons[shotData[s].weapType].flags & kWeapSpinGraphic;
			if (spin) {
				n = shotData[s].frame;
				shotData[s].frame++;
				if (shotData[s].frame > kHeadingFrames)
					shotData[s].frame = 0;
			} else {
				n = shotData[s].angle / kHeadingStep;
				if (n > kHeadingFrames - 1)
					n = kHeadingFrames - 1;
				if (n < 0)
					n = 0;
			}
			sprite->face = weapFaces[gWeapons[shotData[s].weapType].graphic][n];
			w = GetSpriteXSize(sprite->face) / 2;
			sprite->position.h = screenCenter.h + (shotData[s].pos.x - shipData->pos.x) - w;
			sprite->position.v = screenCenter.v + (shotData[s].pos.y - shipData->pos.y) - w;
			if (thirdFrame) {
				n = -1;
				if (gWeapons[shotData[s].weapType].flags & kWeapSmallSmoke)
					n = 1;
				else if (gWeapons[shotData[s].weapType].flags & kWeapBigSmoke)
					n = 0;
				if (n != -1) {
					if (gWeapons[shotData[s].weapType].flags & kWeapLongSmoke)
						n += 2;
					SpawnSmoke(shotData[s].pos, n, gWeapons[shotData[s].weapType].smokeSet);
				}
			}
			if (gWeapons[shotData[s].weapType].decay > 0) {
				shotData[s].damageFalloffTimer++;
				if (shotData[s].damageFalloffTimer > gWeapons[shotData[s].weapType].decay) {
					shotData[s].damageFalloffTimer = 0;
					shotData[s].damageFalloffSteps++;
				}
			}
			if (settings.highDetail)
				RefreshSprite(sprite);
		} else {
			if (shipData->curSystem == shotData[s].curSystem && shotData[s].life > kDeadMark) {
				if (gWeapons[shotData[s].weapType].flags & kWeapDetonates) {
					if (gWeapons[shotData[s].weapType].explodType == kExplodSmall)
						SpawnExplod(shotData[s].pos, -1, 0, 0);
					if (gWeapons[shotData[s].weapType].explodType == kExplodBig) {
						where.x = shotData[s].pos.x;
						where.y = shotData[s].pos.y;
						PlaySoundDistance(-1, explodeSnd[1], 6, &where, &shipData->pos);
						SpawnExplod(shotData[s].pos, -1, 1, 0);
					}
					if (gWeapons[shotData[s].weapType].explodType == kExplodHuge) {
						where.x = shotData[s].pos.x;
						where.y = shotData[s].pos.y;
						PlaySoundDistance(-1, explodeSnd[0], 6, &where, &shipData->pos);
						SpawnExplod(shotData[s].pos, -1, 2, 0);
						radius = gWeapons[shotData[s].weapType].blastRadius;
						n = 2.0f * (float)((float)radius / 50.0);
						for (i = 0; i < n; i++) {
							where.x = (shotData[s].pos.x + Rand(0.5 * radius)) - radius / 4;
							where.y = (shotData[s].pos.y + Rand(0.5 * radius)) - radius / 4;
							SpawnExplod(where, -1, 1, Rand(8) + 4);
						}
						n = 8.0f * (float)((float)radius / 50.0);
						for (i = 0; i < n; i++) {
							where.x = (shotData[s].pos.x + Rand(0.5 * radius)) - radius;
							where.y = (shotData[s].pos.y + Rand(0.5 * radius)) - radius;
							SpawnExplod(where, -1, 0, Rand(16) + 8);
						}
					}
					if (gWeapons[shotData[s].weapType].blastRadius > 0) {
						for (i = 0; i < MaxShipsInSystem; i++) {
							hit = true;
							if (i == shotData[s].owner
									&& ((gWeapons[shotData[s].weapType].flags & kWeapSafeForPlayer) || shotData[s].owner != 0))
								hit = false;
							if (shipData[i].active && hit) {
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
				} else {
					if (gWeapons[shotData[s].weapType].explodType >= 1)
						SpawnExplod(shotData[s].pos, 0, 0, 0);
				}
			}
			shotData[s].life = kSlotFree;
			sprite->appLong = -1;
			sprite->face = nil;
			sprite->task = nil;
		}
	}
}
