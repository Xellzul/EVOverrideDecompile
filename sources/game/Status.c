/* Status.c -- the in-flight HUD: status panel, radar, target and shield displays. Notes: Research/notes/Status.md */
#include <Types.h>
#include <Quickdraw.h>
#include <Windows.h>
#include <Events.h>
#include <Memory.h>
#include <TextUtils.h>
#include <Fonts.h>
#include <Resources.h>
#include <string.h>

#include "evo.h"

#define kHudFontID		2020
#define kHudTextSize	14
#define kNoHudTarget	kLowestValue


struct Face;


short hudTargetType;
short hudTargetShield;
short hudTick;
Boolean hudKeyPrev;
Boolean hudKeyDown;

// FUNCTION: EVO 0x10054f28
void RedrawStatus(void)
{
	Rect r;
	Rect r2;
	short windowFront;

	if (FrontWindow() == gSAT.wind.port)
		windowFront = 1;
	else
		windowFront = 0;
	if (gSAT.wind.port == FrontWindow()) {
		SATSetPortScreen();
		SetRect(&r, gSAT.wind.bounds.left, gSAT.wind.bounds.top, gSAT.wind.bounds.right, gSAT.wind.bounds.bottom);
		RectRgn(gSAT.wind.port->clipRgn, &r);
		RectRgn(gSAT.wind.port->visRgn, &r);
		RectRgn(loadPort.port->clipRgn, &r);
		RectRgn(loadPort.port->visRgn, &r);
	}
	SetRect(&r, gSAT.wind.bounds.right - kStatusPanelWidth, gSAT.wind.bounds.top, gSAT.wind.bounds.right, gSAT.wind.bounds.bottom);
	CopyBits(&statusPort.port->portBits, &loadPort.port->portBits, &statusPort.bounds, &r, srcCopy, nil);
	if (HasIFF(shipData)) {
		SATSetPort(&loadPort);
		SetRect(&r2, gSAT.wind.bounds.right - 139, gSAT.wind.bounds.top + 4, gSAT.wind.bounds.right - 6, gSAT.wind.bounds.top + 138);
		ForeColor(blackColor);
		PaintRect(&r2);
		SATSetPortScreen();
	}
	if (shipData->shield <= 0) {
		SetRect(&r2, loadPort.bounds.left + 10, loadPort.bounds.top + 150, loadPort.bounds.left + 49, loadPort.bounds.top + 163);
		SATSetPort(&loadPort);
		MoveTo(r2.left, r2.bottom - 2);
		RGBForeColor(&bkgndGreen);
		PaintRect(&r2);
		RGBForeColor(&dimGreen);
		DrawString("\pFoo");
		ForeColor(blackColor);
		SATSetPortScreen();
	}
	CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &r, &r, srcCopy, nil);
	hudRedraw.nav = true;
	hudRedraw.shield = true;
	hudRedraw.credits = true;
	hudRedraw.ammo = true;
	hudRedraw.fuel = true;
	hudRedraw.radar = true;
	hudRedraw.target = true;
	hudTargetType = kNoHudTarget;
	hudTargetShield = kNoHudTarget;
	hudKeyPrev = false;
	DrawStatus();
}

// FUNCTION: EVO 0x100551f8
void DrawStatus(void)
{
	if (fadePending[0] || fadePending[1])
		return;
	{
		if (shipData->navTargetKind != kNavOff && TickCount() % 30 == 0)
			hudRedraw.nav = true;
		if (shipData->targetShip != -1 && TickCount() % 45 == 0)
			hudRedraw.target = hudRedraw.credits = true;
		if (QuickKeyCheck(kKeyControl))
			hudKeyDown = true;
		else
			hudKeyDown = false;
		hudTick++;
		if (hudTick % 15 == 0) {
			if (hudTick > 30) {
				hudTick = 0;
				if (shipData->targetShip != -1) {
					if (hudTargetShield != shipData[shipData->targetShip].shield ||
							hudTargetType != shipData[shipData->targetShip].type)
						hudRedraw.target = true;
					if (hudKeyPrev != hudKeyDown || hudKeyDown)
						hudRedraw.target = true;
				} else {
					hudTargetType = hudTargetShield = kNoHudTarget;
				}
			}
			hudRedraw.radar = true;
		}
		if (hudRedraw.target) {
			DrawStatusTarg();
			hudRedraw.target = false;
		}
		if (hudRedraw.radar) {
			DrawStatusRadar(0);
			hudRedraw.radar = false;
		}
		if (hudRedraw.ammo) {
			DrawStatusWeap();
			hudRedraw.ammo = false;
		}
		if (hudRedraw.nav) {
			DrawStatusNav();
			hudRedraw.nav = false;
		}
		if (hudRedraw.shield) {
			DrawStatusShield();
			hudRedraw.shield = false;
		}
		if (hudRedraw.fuel) {
			DrawStatusFuel();
			hudRedraw.fuel = false;
		}
		if (hudRedraw.credits) {
			DrawStatusCargo();
			hudRedraw.credits = false;
		}
	}
}

// FUNCTION: EVO 0x100554ac
void DrawStatusRadar(Boolean quiet)
{
	Rect dst;
	Rect src;
	Rect dead1;
	Rect dead2;
	Rect dot;
	long dead3;
	FloatPoint v;
	FloatPoint c;
	long cx, cy;
	Boolean hasIFF, hasDensity;
	long i, s, count, w;
	float dist;

	hasIFF = HasIFF(shipData);
	hasDensity = HasDensityScanner(shipData);
	SATSetPort(&loadPort);
	SetRect(&dst, gSAT.wind.bounds.right - 139, gSAT.wind.bounds.top + 4, gSAT.wind.bounds.right - 6, gSAT.wind.bounds.top + 138);
	SetRect(&src, 5, 4, 138, 138);
	if (Rand(100) + 1 <= gSystems[shipData->curSystem].interference - ShipAntiInterference()) {
		cx = Rand(10);
		if (staticPats[cx] != nil)
			FillCRect(&dst, staticPats[cx]);
	} else {
		ForeColor(blackColor);
		if (hasIFF)
			PaintRect(&dst);
		else
			CopyBits(&statusPort.port->portBits, &loadPort.port->portBits, &src, &dst, srcCopy, nil);
		cx = 0.5 * (dst.left + dst.right);
		cy = 0.5 * (dst.top + dst.bottom);
		if (!quiet) {
			for (i = 0; i <= 3; i++) {
				s = gSystems[shipData->curSystem].navDefault[i];
				if (s != -1) {
					dot.left = dot.right = 0.03125 * (long)(stellarData[s].pos.h - shipData->pos.x) + cx;
					dot.top = dot.bottom = 0.03125 * (long)(stellarData[s].pos.v - shipData->pos.y) + cy;
					if (SpobIsStation(&stellarData[s])) {
						if (hasIFF)
							ColorCodeStellar(&stellarData[s]);
						else
							RGBForeColor(&dimGreen);
						if (hasDensity) {
							InsetRect(&dot, -1, -1);
							FrameRect(&dot);
						} else {
							MoveTo(dot.left, dot.top);
							Line(0, 0);
						}
					} else {
						if (hasIFF)
							ColorCodeStellar(&stellarData[s]);
						else
							RGBForeColor(&dimGreen);
						w = GetSpriteXSize(spobFaces[stellarData[s].type]);
						if (w >= 64)
							InsetRect(&dot, -2, -2);
						else
							InsetRect(&dot, -1, -1);
						FrameOval(&dot);
					}
				}
			}
			for (i = 1; i < MaxShipsInSystem; i++) {
				if (shipData[i].active) {
					if (shipData->curSystem == shipData[i].curSystem) {
						dot.left = dot.right = 0.03125 * (long)(shipData[i].pos.x - shipData->pos.x) + cx;
						dot.top = dot.bottom = 0.03125 * (long)(shipData[i].pos.y - shipData->pos.y) + cy;
						if (shipData->targetShip == i) {
							if (hudTick > 15) {
								if (hasIFF)
									RGBForeColor(&rgbLightGrey);
								else
									RGBForeColor(&brightGreen);
							} else if (hasIFF)
								ColorCodeShip(&shipData[i]);
							else
								RGBForeColor(&dimGreen);
						} else if (hasIFF)
							ColorCodeShip(&shipData[i]);
						else
							RGBForeColor(&dimGreen);
						if (hasDensity && gShipTypes[shipData[i].type].mass >= 100) {
							InsetRect(&dot, -1, -1);
							FrameRect(&dot);
						} else {
							MoveTo(dot.left, dot.top);
							Line(0, 0);
						}
					}
				}
			}
		}
	}
	if (hasIFF)
		ColorCodeShip(shipData);
	else
		RGBForeColor(&brightGreen);
	MoveTo(cx, cy);
	Line(0, 0);
	if (hudTick <= 15 && shipData->navTargetKind != kNavHyperspace) {
		count = 0;
		for (i = 0; i <= 3; i++) {
			if (gSystems[shipData->curSystem].navDefault[i] != -1)
				count++;
		}
		if (count > 0) {
			v.x = v.y = 0.0;
			dist = XYFloatSquaredDist(&shipData->pos, &v);
			if (dist > 7000000.0) {
				w = CalcInterceptAngle(shipData->pos, v);
				v.x = c.x = cx;
				v.y = c.y = cy;
				Accel(w, 25.0, &v);
				Accel(w, 50.0, &c);
				RGBForeColor(&dimGreen);
				MoveTo(v.x, v.y);
				LineTo(c.x, c.y);
				v.x = c.x;
				v.y = c.y;
				Accel(w + 135, 6.0, &v);
				MoveTo(c.x, c.y);
				LineTo(v.x, v.y);
				v.x = c.x;
				v.y = c.y;
				Accel(w - 135, 6.0, &v);
				MoveTo(c.x, c.y);
				LineTo(v.x, v.y);
				ForeColor(blackColor);
			}
		}
	}
	ForeColor(blackColor);
	SATSetPortScreen();
	CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &dst, &dst, srcCopy, nil);
}

// FUNCTION: EVO 0x10055ebc
void DrawStatusNav(void)
{
	Rect dst;
	Rect src;
	Rect dead1;
	Rect dead2;
	FloatPoint origin;
	FloatPoint pos;
	float maxRange;
	float range;
	Boolean clear;
	short i;
	short jumpSyst;

	SATSetPort(&loadPort);
	ForeColor(blackColor);
	SetRect(&dst, gSAT.wind.bounds.right - 139, gSAT.wind.bounds.top + 192, gSAT.wind.bounds.right - 6, gSAT.wind.bounds.top + 227);
	SetRect(&src, 5, 192, 138, 227);
	CopyBits(&statusPort.port->portBits, &loadPort.port->portBits, &src, &dst, srcCopy, nil);
	TextFont(kHudFontID);
	TextSize(kHudTextSize);
	if (shipData->navTargetKind == kNavOff) {
		RGBForeColor(&dimGreen);
		DrawCenteredText("\pNav System Off", dst.left, dst.right, dst.top + 22);
	}
	if (shipData->navTargetKind == kNavStellar) {
		RGBForeColor(&dimGreen);
		DrawCenteredText("\pStellar Navigation", dst.left, dst.right, dst.top + 12);
		if (shipData->targetStellar != -1) {
			RGBForeColor(&brightGreen);
			DrawCenteredText(stellarData[shipData->targetStellar].name, dst.left, dst.right, dst.top + 30);
		} else {
			RGBForeColor(&dimGreen);
			DrawCenteredText("\pNo Destination", dst.left, dst.right, dst.top + 30);
		}
	}
	if (shipData->navTargetKind == kNavHyperspace) {
		if (shipData->jumpTimer > 0)
			RGBForeColor(&brightGreen);
		else
			RGBForeColor(&dimGreen);
		DrawCenteredText("\pHyperspace", dst.left, dst.right, dst.top + 12);
		if (shipData->targetStellar != -1) {
			origin.x = origin.y = 0;
			pos.x = shipData->pos.x;
			pos.y = shipData->pos.y;
			clear = true;
			maxRange = ShipHyperSafeDist(shipData);
			for (i = 0; i <= 3; i++) {
				if (gSystems[shipData->curSystem].navDefault[i] != -1) {
					origin.x = origin.y = 0.0;
					range = ShipHyperSafeDist(shipData);
					if (FloatAbs(XYFloatSquaredDist(&origin, &shipData->pos)) <= range) {
						clear = false;
						break;
					}
				}
			}
			if (clear || shipData->jumpTimer > 0)
				RGBForeColor(&brightGreen);
			else
				RGBForeColor(&dimGreen);
			jumpSyst = gSystems[shipData->curSystem].links[shipData->targetStellar];
			if (gSystems[jumpSyst].visited > 0)
				DrawCenteredText(gSystems[jumpSyst].name, dst.left, dst.right, dst.top + 30);
			else
				DrawCenteredText("\pUnexplored System", dst.left, dst.right, dst.top + 30);
		} else {
			RGBForeColor(&dimGreen);
			DrawCenteredText("\pNo Destination", dst.left, dst.right, dst.top + 30);
		}
	}
	ForeColor(blackColor);
	SATSetPortScreen();
	CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &dst, &dst, srcCopy, nil);
}

// FUNCTION: EVO 0x1005631c
void DrawStatusShield(void)
{
	Rect panel;
	Rect bar;
	Rect barDst;
	Rect src;
	Rect fill;
	Rect dead;
	double cap;

	SATSetPort(&loadPort);
	TextFont(kHudFontID);
	TextSize(kHudTextSize);
	SetRect(&panel, gSAT.wind.bounds.right - 85, gSAT.wind.bounds.top + 154, gSAT.wind.bounds.right - 10, gSAT.wind.bounds.top + 160);
	SetRect(&src, 59, 154, 134, 160);
	CopyBits(&statusPort.port->portBits, &loadPort.port->portBits, &src, &panel, srcCopy, nil);
	bar.left = loadPort.bounds.left + 5;
	bar.top = loadPort.bounds.top + 147;
	bar.right = bar.left + 53;
	bar.bottom = bar.top + 35;
	barDst.top = bar.top;
	barDst.bottom = bar.bottom;
	barDst.left = bar.left + screenCenter.h * 2;
	barDst.right = bar.right + screenCenter.h * 2;
	if (shipData->shield <= 0)
		CopyBits(&armorPort.port->portBits, &gSAT.wind.port->portBits, &armorPort.bounds, &barDst, srcCopy, nil);
	else
		CopyBits(&statusPort.port->portBits, &gSAT.wind.port->portBits, &bar, &barDst, srcCopy, nil);
	if (shipData->shield <= 0 && !IsDying(shipData) && shipData->type != kEscapePodShip) {
		ForeColor(blackColor);
		fill = panel;
		cap = ShipArmorCapacity(shipData);
		if (-shipData->shield <= cap)
			fill.right = fill.left + 76 - 76.0 * (-shipData->shield / cap);
		if (armorPat != nil)
			FillCRect(&fill, armorPat);
	} else if (shipData->shield > 0 && !gInvertedColors) {
		RGBForeColor(&dimGreen);
		fill = panel;
		cap = ShipShieldCapacity(shipData);
		if (shipData->shield <= cap)
			fill.right = 76.0 * (shipData->shield / cap) + fill.left;
		PaintRect(&fill);
	}
	ForeColor(blackColor);
	SATSetPortScreen();
	CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &panel, &panel, srcCopy, nil);
}

// FUNCTION: EVO 0x10056758
void DrawStatusFuel(void)
{
	short d1, d2, d3;
	Rect panel;
	Rect src;
	Rect fill;
	Rect fill2;
	short d4;
	short full;
	short rem;

	SATSetPort(&loadPort);
	TextFont(kHudFontID);
	TextSize(kHudTextSize);
	SetRect(&panel, gSAT.wind.bounds.right - 85, gSAT.wind.bounds.top + 170, gSAT.wind.bounds.right - 10, gSAT.wind.bounds.top + 176);
	SetRect(&src, 59, 170, 134, 176);
	CopyBits(&statusPort.port->portBits, &loadPort.port->portBits, &src, &panel, srcCopy, nil);
	if (shipData->fuel > 0 && !IsDisabled(shipData)) {
		RGBForeColor(&dimGreen);
		fill = panel;
		fill.right = 76.0 * (shipData->fuel / (double)ShipFuelCapacity(shipData)) + fill.left;
		PaintRect(&fill);
		full = shipData->fuel / 100.0;
		rem = shipData->fuel - full;
		if (rem > 0) {
			fill2 = fill;
			fill2.left += 76.0 * (full * 100 / (double)ShipFuelCapacity(shipData));
			RGBForeColor(&dimmerGreen);
			PaintRect(&fill2);
		}
	}
	ForeColor(blackColor);
	SATSetPortScreen();
	CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &panel, &panel, srcCopy, nil);
}

// FUNCTION: EVO 0x10056a2c
void DrawStatusTarg(void)
{
	Rect dst;
	Rect src;
	Rect picRect;
	Str255 buf;
	Str255 dead;
	short cx, cy;
	double cap;
	Boolean isMission;

	if (shipData->targetShip != -1) {
		hudTargetShield = shipData[shipData->targetShip].shield;
		hudTargetType = shipData[shipData->targetShip].type;
	} else {
		hudTargetShield = -1;
		hudTargetType = -1;
	}
	hudKeyPrev = hudKeyDown;
	SATSetPort(&loadPort);
	TextFont(kHudFontID);
	TextSize(kHudTextSize);
	SetRect(&dst, loadPort.bounds.right - 139, loadPort.bounds.top + 263, loadPort.bounds.right - 5, loadPort.bounds.top + 378);
	SetRect(&src, 5, 263, 139, 378);
	ForeColor(blackColor);
	CopyBits(&statusPort.port->portBits, &loadPort.port->portBits, &src, &dst, srcCopy, nil);
	if (hudKeyDown && showAll) {
		if (shipData->targetShip == -1)
			shipData->targetShip = 0;
		MoveTo(dst.right - 15, dst.top + 20);
		RGBForeColor(&dimGreen);
		RGBForeColor(&brightGreen);
		NumToString(shipData->targetShip, buf);
		DrawString(buf);
		MoveTo(dst.left + 5, dst.top + 20);
		RGBForeColor(&dimGreen);
		RGBForeColor(&brightGreen);
		NumToString(shipData[shipData->targetShip].index, buf);
		DrawString(buf);
		MoveTo(dst.left + 5, dst.top + 40);
		RGBForeColor(&dimGreen);
		DrawString("\phg: ");
		RGBForeColor(&brightGreen);
		DrawHighGoalAbbreviation(shipData[shipData->targetShip].aiState);
		MoveTo(dst.left + 70, dst.top + 40);
		RGBForeColor(&dimGreen);
		DrawString("\plg: ");
		RGBForeColor(&brightGreen);
		DrawLowGoalAbbreviation(shipData[shipData->targetShip].aiSubstate);
		MoveTo(dst.left + 5, dst.top + 55);
		RGBForeColor(&dimGreen);
		DrawString("\ptnav: ");
		RGBForeColor(&brightGreen);
		NumToString(shipData[shipData->targetShip].targetStellar, buf);
		DrawString(buf);
		MoveTo(dst.left + 70, dst.top + 55);
		RGBForeColor(&dimGreen);
		DrawString("\ptwep: ");
		RGBForeColor(&brightGreen);
		NumToString(shipData[shipData->targetShip].targetShip, buf);
		DrawString(buf);
		MoveTo(dst.left + 5, dst.top + 70);
		RGBForeColor(&dimGreen);
		DrawString("\pcnt: ");
		RGBForeColor(&brightGreen);
		NumToString(shipData[shipData->targetShip].aiTimer, buf);
		DrawString(buf);
		MoveTo(dst.left + 70, dst.top + 70);
		RGBForeColor(&dimGreen);
		DrawString("\pwp: ");
		RGBForeColor(&brightGreen);
		NumToString(shipData[shipData->targetShip].selectedWeapon, buf);
		DrawString(buf);
		MoveTo(dst.left + 5, dst.top + 85);
		RGBForeColor(&dimGreen);
		DrawString("\psh: ");
		RGBForeColor(&brightGreen);
		NumToString(shipData[shipData->targetShip].shield, buf);
		DrawString(buf);
		MoveTo(dst.left + 60, dst.top + 85);
		RGBForeColor(&dimGreen);
		DrawString("\pai: ");
		RGBForeColor(&brightGreen);
		NumToString(shipData[shipData->targetShip].aiType, buf);
		DrawString(buf);
		MoveTo(dst.left + 45, dst.top + 100);
		RGBForeColor(&brightGreen);
		if (shipData[shipData->targetShip].defenseStellar != -1)
			DrawString("\pDf ");
		else if (shipData[shipData->targetShip].specialMission != -1)
			DrawString("\pSp");
		MoveTo(dst.left + 90, dst.top + 100);
		RGBForeColor(&dimGreen);
		DrawString("\pgov: ");
		RGBForeColor(&brightGreen);
		NumToString(shipData[shipData->targetShip].govt, buf);
		DrawString(buf);
		MoveTo(dst.left + 5, dst.top + 100);
		RGBForeColor(&dimGreen);
		DrawString("\pdc: ");
		RGBForeColor(&brightGreen);
		NumToString(shipData[shipData->targetShip].deathTimer, buf);
		DrawString(buf);
		MoveTo(dst.left + 45, dst.top + 100);
		RGBForeColor(&dimGreen);
		DrawString("\pdu: ");
		NumToString(shipData[shipData->targetShip].dude, buf);
		DrawString(buf);
		if (shipData->targetShip == 0)
			shipData->targetShip = -1;
	} else {
		if (shipData->targetShip != -1) {
			RGBForeColor(&brightGreen);
			isMission = false;
			if (shipData[shipData->targetShip].specialMission != -1) {
				if (currentMissions[shipData[shipData->targetShip].specialMission].shipName[0] > 0)
					isMission = true;
			}
			if (isMission)
				DrawCenteredText((StringPtr)currentMissions[shipData[shipData->targetShip].specialMission].shipName, dst.left, dst.right, dst.top + 16);
			else if (shipData[shipData->targetShip].persType != -1)
				DrawCenteredText(personData[shipData[shipData->targetShip].persType].name, dst.left, dst.right, dst.top + 16);
			else
				DrawCenteredText(gShipTypes[shipData[shipData->targetShip].type].name, dst.left, dst.right, dst.top + 16);
			cx = 0.5 * (dst.left + dst.right);
			cy = 0.5 * (dst.top + dst.bottom);
			SetRect(&picRect, cx - 64, cy - 32, cx + 64, cy + 32);
			ForeColor(blackColor);
			if (shipPics[shipData[shipData->targetShip].type] != nil)
				DrawPicture(shipPics[shipData[shipData->targetShip].type], &picRect);
			if (shipData[shipData->targetShip].persType != kPersAmbrosiaMascot) {
				MoveTo(dst.left + 5, dst.bottom - 6);
				if (shipData[shipData->targetShip].shield <= 0) {
					RGBForeColor(&brightGreen);
					MoveTo(dst.left + 5, dst.bottom - 6);
					if (gShipTypes[shipData[shipData->targetShip].type].flags & kShipShowArmor) {
						RGBForeColor(&dimGreen);
						if (IsDisabled(&shipData[shipData->targetShip])) {
							DrawString("\pDisabled");
						} else {
							DrawString("\pArmor: ");
							RGBForeColor(&brightGreen);
							cap = ShipArmorCapacity(&shipData[shipData->targetShip]);
							if (cap > 0.0) {
								NumToString(100.0 * ((cap + shipData[shipData->targetShip].shield) / cap), buf);
								DrawString(buf);
								DrawString("\p%");
							} else {
								DrawString("\pN/A");
							}
						}
					} else {
						if (gShipTypes[shipData[shipData->targetShip].type].shield <= 0)
							DrawString("\pNo Shields");
						else if (IsDisabled(&shipData[shipData->targetShip]))
							DrawString("\pDisabled");
						else
							DrawString("\pShields Down");
					}
				} else {
					RGBForeColor(&dimGreen);
					DrawString("\pShield: ");
					RGBForeColor(&brightGreen);
					cap = ShipShieldCapacity(&shipData[shipData->targetShip]);
					NumToString(100.0 * (shipData[shipData->targetShip].shield / cap), buf);
					DrawString(buf);
					DrawString("\p%");
				}
				if (shipData[shipData->targetShip].govt >= 0) {
					RGBForeColor(&dimGreen);
					MoveTo(dst.left + 87, dst.bottom - 6);
					DrawString(govtTransponderCode[shipData[shipData->targetShip].govt]);
				} else if (shipData[shipData->targetShip].parentShip == 0) {
					RGBForeColor(&dimGreen);
					MoveTo(dst.left + 87, dst.bottom - 6);
					if (shipData[shipData->targetShip].aiType == kAIFighter)
						DrawString("\pFighter");
					else
						DrawString("\pEscort");
				}
			} else {
				RGBForeColor(&brightGreen);
				DrawCenteredText("\pAmbrosia", dst.left, dst.right, dst.bottom - 6);
			}
		} else {
			RGBForeColor(&dimGreen);
			DrawCenteredText("\pNo Target", dst.left, dst.right, dst.top + 47);
		}
	}
	ForeColor(blackColor);
	SATSetPortScreen();
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &dst, &dst, srcCopy, nil);
}

// FUNCTION: EVO 0x100578c8
void DrawStatusWeap(void)
{
	Rect dst;
	Rect src;
	Rect dead;
	char buf[256];
	Str255 num;

	SATSetPort(&loadPort);
	TextFont(kHudFontID);
	TextSize(kHudTextSize);
	SetRect(&dst, gSAT.wind.bounds.right - 138, gSAT.wind.bounds.top + 237, gSAT.wind.bounds.right - 6, gSAT.wind.bounds.top + 254);
	SetRect(&src, 6, 237, 138, 254);
	CopyBits(&statusPort.port->portBits, &loadPort.port->portBits, &src, &dst, srcCopy, nil);
	if (shipData->selectedWeapon == -1) {
		RGBForeColor(&dimGreen);
		DrawCenteredText("\pNo Secondary Weapon", dst.left, dst.right, dst.top + 12);
	} else {
		RGBForeColor(&brightGreen);
		if (gWeapons[shipData->selectedWeapon].ammoType == -1) {
			DrawCenteredText(weapNames[shipData->selectedWeapon], dst.left, dst.right, dst.top + 12);
		} else if (gWeapons[shipData->selectedWeapon].ammoType > kAmmoFuelBase) {
			strcpy(buf, (char *)weapNames[shipData->selectedWeapon]);
			p2cstr((StringPtr)buf);
			strcat(buf, " - ");
			NumToString(shipData->weap[shipData->selectedWeapon].ammo, num);
			p2cstr(num);
			strcat(buf, (char *)num);
			c2pstr(buf);
			DrawCenteredText((StringPtr)buf, dst.left, dst.right, dst.top + 12);
		} else if (gWeapons[shipData->selectedWeapon].ammoType <= kAmmoFuelBase) {
			DrawCenteredText(weapNames[shipData->selectedWeapon], dst.left, dst.right, dst.top + 12);
		}
	}
	ForeColor(blackColor);
	SATSetPortScreen();
	CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &dst, &dst, srcCopy, nil);
}

// FUNCTION: EVO 0x10057b74
void DrawStatusCargo(void)
{
	Rect dst;
	Rect src;
	Rect dead;
	Str255 num;
	Str255 dead2;
	short i;
	short j;
	short count;

	SATSetPort(&loadPort);
	TextFont(kHudFontID);
	TextSize(kHudTextSize);
	SetRect(&dst, gSAT.wind.bounds.right - 138, gSAT.wind.bounds.top + 387, gSAT.wind.bounds.right - 5, gSAT.wind.bounds.top + 475);
	SetRect(&src, 6, 387, 139, 475);
	CopyBits(&statusPort.port->portBits, &loadPort.port->portBits, &src, &dst, srcCopy, nil);
	for (i = 0; i <= 5; i++) {
		if (shipData->cargo[i] > 0) {
			RGBForeColor(&dimGreen);
			MoveTo(dst.left + 3, dst.top + (i + 1) * 14 - 2);
			DrawString(statusCargoName[i]);
			RGBForeColor(&brightGreen);
			MoveTo(dst.left + 35, dst.top + (i + 1) * 14 - 2);
			NumToString(shipData->cargo[i], num);
			DrawString(num);
		}
	}
	MoveTo(dst.left + 67, dst.top + 12);
	RGBForeColor(&dimGreen);
	DrawString("\pFree:");
	MoveTo(dst.left + 100, dst.top + 12);
	RGBForeColor(&brightGreen);
	count = TotalFleetHolds() - TotalCargo(shipData);
	if (count < 0)
		count = 0;
	NumToString(count, num);
	DrawString(num);
	count = 0;
	for (j = 0; j < MaxSimultaneousMissions; j++) {
		if (currentMissionObjectives[j].active && currentMissions[j].cargoPickedUp && currentMissions[j].cargoType != -1)
			count++;
	}
	for (i = 0; i < kMaxJunkTypes; i++) {
		if (junkData[i].held > 0)
			count++;
	}
	if (count > 0) {
		MoveTo(dst.left + 67, dst.top + 30);
		RGBForeColor(&dimGreen);
		DrawString("\pSpecial:");
		MoveTo(dst.left + 77, dst.top + 46);
		RGBForeColor(&brightGreen);
		if (count == 1) {
			for (j = 0; j < MaxSimultaneousMissions; j++) {
				if (currentMissionObjectives[j].active && currentMissions[j].cargoPickedUp && currentMissions[j].cargoType != -1) {
					DrawString(cargoAbbrev[currentMissions[j].cargoType]);
					break;
				}
			}
			for (i = 0; i < kMaxJunkTypes; i++) {
				if (junkData[i].held > 0) {
					DrawString(statusJunkName[i]);
					break;
				}
			}
		} else {
			DrawString("\pMultiple");
		}
	}
	MoveTo(dst.left + 67, dst.top + 66);
	RGBForeColor(&dimGreen);
	DrawString("\pCredits:");
	MoveTo(dst.left + 77, dst.top + 82);
	RGBForeColor(&brightGreen);
	DrawCommaNumber(shipData->credits);
	ForeColor(blackColor);
	SATSetPortScreen();
	CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &dst, &dst, srcCopy, nil);
}
