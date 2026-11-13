/* Util.c -- ship and sector utilities: nearest-ship searches, ship statistics, dates, jamming, menu bar. Notes: Research/notes/Util.md */
#include <Types.h>
#include <Quickdraw.h>
#include <Windows.h>
#include <Dialogs.h>
#include <FixMath.h>
#include <Movies.h>
#include <Files.h>
#include <Aliases.h>
#include <Events.h>
#include <Memory.h>
#include <TextUtils.h>
#include <Fonts.h>
#include <Resources.h>
#include <Palettes.h>
#include <string.h>
#include <ctype.h>
#include <ToolUtils.h>
#include <math.h>
#include <Speech.h>
#include <Gestalt.h>
#include <LowMem.h>
#include <stdlib.h>
#include <limits.h>

#include "evo.h"




#define kMessageBuoyStrList	1000
#define kOldOsDlogID			3010

Boolean systemFlag[MaxSystems];
Boolean speechAvailable;
Boolean menuBarHidden;
short maxAutoExploreJumps;
short savedMBarHeight;
unsigned short keyMapBits[8];
DialogPtr oldOsDialog;
CTabHandle workClut;
CTabHandle gameClut;
CTabHandle hueCluts[7];

/* HACK: unused in the binary (dead-stripped); makes stellarData the first-referenced
   global of the TU, which fixes the cached-global prologue order of every later function. */
static SpobRecord *StellarAt(short id)
{
	return &stellarData[id];
}

// FUNCTION: EVO 0x100583c4
void NewDialogHook(DialogPtr dlg, WindowPtr parent)
{
	Point a, b;
	short i;

	/* dead on both targets; the 68k frame and registers show it was code */
	if (0) {
		for (i = 0; i < 4; i++) {
			SetPort(dlg);
			MoveWindow(dlg, parent->portRect.left, parent->portRect.top, false);
			SelectWindow(dlg);
			ShowWindow(dlg);
			SetPort(parent);
			GetMouse(&a);
			GetMouse(&b);
		}
	}
}

// FUNCTION: EVO 0x100583c8
void CenterWindow(DialogPtr dlg)
{
	Point size;
	Point c;
	Point b;
	Point a;
	Point d;
	GrafPtr savePort;
	long dead;

	if (dlg != nil) {
		GetPort(&savePort);
		SetPort(dlg);
		size.h = dlg->portRect.right - dlg->portRect.left;
		size.v = dlg->portRect.bottom - dlg->portRect.top;
		a.h = dlg->portRect.left;
		a.v = dlg->portRect.top;
		b.h = dlg->portRect.right;
		c.h = dlg->portRect.left;
		LocalToGlobal(&c);
		LocalToGlobal(&b);
		LocalToGlobal(&a);
		SATSetPortScreen();
		GlobalToLocal(&c);
		GlobalToLocal(&b);
		if (b.h > gSAT.wind.bounds.right - 149) {
			if (c.h - gSAT.wind.bounds.left > b.h - (gSAT.wind.bounds.right - 149))
				MoveWindow(dlg, a.h - (b.h - (gSAT.wind.bounds.right - 149)), a.v, false);
			else
				MoveWindow(dlg, gSAT.wind.bounds.left, a.v, false);
		}
		SetPort(dlg);
		b.h = dlg->portRect.right;
		c.h = dlg->portRect.left;
		a.h = dlg->portRect.left;
		a.v = dlg->portRect.top;
		LocalToGlobal(&c);
		LocalToGlobal(&b);
		LocalToGlobal(&a);
		SATSetPortScreen();
		GlobalToLocal(&c);
		GlobalToLocal(&b);
		if (c.h >= gSAT.wind.bounds.left + 5 && b.h <= gSAT.wind.bounds.right - 149)
			MoveWindow(dlg, gSAT.wind.bounds.left + (5 + (c.h - gSAT.wind.bounds.left) + ((gSAT.wind.bounds.right - 149) - b.h)) / 2, a.v, false);
		SetPort(dlg);
		b.h = dlg->portRect.right;
		c.h = dlg->portRect.left;
		d.v = dlg->portRect.bottom;
		a.h = dlg->portRect.left;
		a.v = dlg->portRect.top;
		LocalToGlobal(&c);
		LocalToGlobal(&b);
		LocalToGlobal(&d);
		LocalToGlobal(&a);
		SATSetPortScreen();
		GlobalToLocal(&c);
		GlobalToLocal(&b);
		GlobalToLocal(&d);
		if (d.v > gSAT.wind.bounds.top + 370 && b.h > gSAT.wind.bounds.right - 149)
			MoveWindow(dlg, a.h, a.v - (d.v - (gSAT.wind.bounds.top + 370)), false);
		SetPort(savePort);
	}
}

// FUNCTION: EVO 0x100586f0
void Accel(short angle, float dist, FloatPoint *pt)
{
	long dead[4];

	TableSin(angle);
	pt->x += trigResult * dist;
	TableCos(angle);
	pt->y -= trigResult * dist;
}

// FUNCTION: EVO 0x10058770
void AdjustedAccel(short angle, float accel, FloatPoint *vel, float maxSpeed)
{
	FloatPoint limit;
	FloatPoint thrust;
	FloatPoint dead;
	ShortPoint velSign;
	ShortPoint thrustSign;

	TableSin(angle);
	limit.x = FloatAbs(trigResult * maxSpeed);
	TableCos(angle);
	limit.y = FloatAbs(trigResult * maxSpeed);
	TableSin(angle);
	thrust.x = trigResult * accel;
	TableCos(angle);
	thrust.y = -trigResult * accel;
	velSign.h = velSign.v = thrustSign.h = thrustSign.v = 0;
	if (vel->x < 0.0)
		velSign.h = -1;
	else
		velSign.h = 1;
	if (vel->y < 0.0)
		velSign.v = -1;
	else
		velSign.v = 1;
	if (thrust.x < 0.0)
		thrustSign.h = -1;
	else
		thrustSign.h = 1;
	if (thrust.y < 0.0)
		thrustSign.v = -1;
	else
		thrustSign.v = 1;
	if (FloatAbs(vel->x) < FloatAbs(limit.x) || velSign.h != thrustSign.h)
		vel->x += thrust.x;
	if (FloatAbs(vel->y) < FloatAbs(limit.y) || velSign.v != thrustSign.v)
		vel->y += thrust.y;
}

// FUNCTION: EVO 0x10058970
float FloatAbs(float x)
{
	float r;

	if (x < 0.0)
		r = -1.0 * x;
	else
		r = x;
	return r;
}

// FUNCTION: EVO 0x100589a0
short CalcInterceptAngle(FloatPoint from, FloatPoint to)
{
	long dead1[3];
	FloatPoint d;
	short heading;
	long dead;

	d.x = from.x - to.x;
	d.y = from.y - to.y;
	heading = TableArcTan(d.x, d.y);
	return heading;
}

// FUNCTION: EVO 0x10058a0c
void SpeedLimit(FloatPoint *p, float limit)
{
	if (p->x > limit)
		p->x = limit;
	if (p->x < -limit)
		p->x = -limit;
	if (p->y > limit)
		p->y = limit;
	if (p->y < -limit)
		p->y = -limit;
}

// FUNCTION: EVO 0x10058a60
float XYFloatSquaredDist(FloatPoint *a, FloatPoint *b)
{
	float dx;
	float dy;
	float d;

	dx = FloatAbs(a->x - b->x);
	dy = FloatAbs(a->y - b->y);
	d = dx * dx + dy * dy;
	return d;
}

// FUNCTION: EVO 0x10058ae0
short FindNextShipInSystem(short cur, short syst)
{
	Boolean a[MaxShipsInSystem];
	Boolean b[MaxShipsInSystem];
	Boolean shift;
	short i;
	short v;

	if (KeyCheck(kKeyOption))
		shift = true;
	else
		shift = false;
	for (i = 1; i < MaxShipsInSystem; i++) {
		a[i] = 0;
		b[i] = 0;
		if (shipData[i].specialMission != -1) {
			if (currentMissionObjectives[shipData[i].specialMission].active) {
				for (v = currentMissions[shipData[i].specialMission].shipBehav; v >= 9; v -= 10)
					;
				if (v == 1)
					a[i] = 1;
			}
		} else {
			if (shipData[i].parentShip != -1 && shipData[i].parentShip != 0 && shipData[i].defenseStellar == -1 &&
					shipData[shipData[i].parentShip].parentShip == 0)
				b[i] = 1;
		}
	}
	if (cur != -1) {
		for (i = cur + 1; i < MaxShipsInSystem; i++) {
			if (shipData[i].active && !IsDying(&shipData[i]) && syst == shipData[i].curSystem) {
				if ((shipData[i].parentShip != 0 && !b[i]) || shift || a[i] || shipData[i].defenseStellar != -1)
					return i;
			}
		}
	} else {
		for (i = 1; i < MaxShipsInSystem; i++) {
			if (shipData[i].active && !IsDying(&shipData[i]) && syst == shipData[i].curSystem) {
				if ((shipData[i].parentShip != 0 && !b[i]) || shift || a[i] || shipData[i].defenseStellar != -1)
					return i;
			}
		}
	}
	return cur;
}

// FUNCTION: EVO 0x10058e78
short FindPrevShipInSystem(short cur, short syst)
{
	Boolean a[MaxShipsInSystem];
	Boolean b[MaxShipsInSystem];
	Boolean shift;
	short i;
	short v;

	if (KeyCheck(kKeyOption))
		shift = true;
	else
		shift = false;
	for (i = 1; i < MaxShipsInSystem; i++) {
		a[i] = 0;
		b[i] = 0;
		if (shipData[i].specialMission != -1) {
			if (currentMissionObjectives[shipData[i].specialMission].active) {
				for (v = currentMissions[shipData[i].specialMission].shipBehav; v >= 9; v -= 10)
					;
				if (v == 1)
					a[i] = 1;
			}
		} else {
			if (shipData[i].parentShip != -1 && shipData[i].parentShip != 0 && shipData[i].defenseStellar == -1 &&
					shipData[shipData[i].parentShip].parentShip == 0)
				b[i] = 1;
		}
	}
	if (cur != -1) {
		for (i = cur - 1; i > 0; i--) {
			if (shipData[i].active && !IsDying(&shipData[i]) && syst == shipData[i].curSystem) {
				if ((shipData[i].parentShip != 0 && !b[i]) || shift || a[i] || shipData[i].defenseStellar != -1)
					return i;
			}
		}
	} else {
		for (i = MaxShipsInSystem - 1; i >= 1; i--) {
			if (shipData[i].active && !IsDying(&shipData[i]) && syst == shipData[i].curSystem) {
				if ((shipData[i].parentShip != 0 && !b[i]) || shift || a[i] || shipData[i].defenseStellar != -1)
					return i;
			}
		}
	}
	return cur;
}

// FUNCTION: EVO 0x1005920c
void DrawCenteredText(StringPtr s, short left, short right, short y)
{
	long dead[4];
	short w;

	w = StringWidth(s);
	MoveTo((left + right) / 2 - w / 2, y);
	DrawString(s);
}

// FUNCTION: EVO 0x10059294
short GetSpriteXSize(FacePtr face)
{
	if (face == nil)
		return 0;
	return face->iconMask.bounds.right - face->iconMask.bounds.left;
}

// FUNCTION: EVO 0x100592b4
short GetSpriteYSize(FacePtr face)
{
	if (face == nil)
		return 0;
	return face->iconMask.bounds.bottom - face->iconMask.bounds.top;
}

// FUNCTION: EVO 0x100592d4
short TotalCargo(ShipObj *ship)
{
	short total;
	short i;

	total = 0;
	for (i = 0; i <= 5; i++)
		total += ship->cargo[i];
	if (ship->index == 0) {
		for (i = 0; i < MaxSimultaneousMissions; i++) {
			if (currentMissionObjectives[i].active && currentMissions[i].cargoPickedUp)
				total += currentMissions[i].cargoQty;
		}
		for (i = 0; i < kMaxJunkTypes; i++) {
			if (junkData[i].held > 0)
				total += junkData[i].held;
		}
	}
	return total;
}

// FUNCTION: EVO 0x100593dc
short ShipTotalHolds(void)
{
	short total;
	short i;

	total = gShipTypes[shipData->type].cargo;
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		if (gOutfits[i].modType[0] == kOutfModCargo)
			total += gOutfits[i].modVal[0] * outfitCount[i];
		if (gOutfits[i].modType[1] == kOutfModCargo)
			total += gOutfits[i].modVal[1] * outfitCount[i];
	}
	return total;
}

float DeadA(short v)
{
	return v;
}

// FUNCTION: EVO 0x100594c8
short FindNearestShipToPlayer(void)
{
	short best;
	float bestDist;
	short i;
	float d;
	float dx;
	float dy;

	best = -1;
	bestDist = -1;
	for (i = 0; i < MaxShipsInSystem; i++) {
		if (i != 0 && shipData[i].active && !IsDying(&shipData[i]) && shipData->curSystem == shipData[i].curSystem) {
			dx = FloatAbs(shipData->pos.x - shipData[i].pos.x);
			dy = FloatAbs(shipData->pos.y - shipData[i].pos.y);
			d = dx * dx + dy * dy;
			if (d < bestDist || bestDist < 0.0f) {
				bestDist = d;
				best = i;
			}
		}
	}
	return best;
}

float DeadB(void)
{
	return 1000.0f;
}

// FUNCTION: EVO 0x1005961c
short FindNearestThreatToPlayer(void)
{
	short best;
	float bestDist;
	short i;
	Boolean engage;
	float d;
	float dx;
	float dy;

	best = -1;
	bestDist = -1;
	for (i = 0; i < MaxShipsInSystem; i++) {
		if (i != 0 && shipData[i].active && !IsDying(&shipData[i]) && shipData->curSystem == shipData[i].curSystem) {
			engage = false;
			if (IsThreatToPlayer(&shipData[i]))
				engage = true;
			if (engage) {
				dx = FloatAbs(shipData->pos.x - shipData[i].pos.x);
				dy = FloatAbs(shipData->pos.y - shipData[i].pos.y);
				d = dx * dx + dy * dy;
				if (d < bestDist || bestDist < 0.0f) {
					bestDist = d;
					best = i;
				}
			}
		}
	}
	return best;
}

// FUNCTION: EVO 0x100597a8
short FindNearestStellar(ShipObj *ship)
{
	short best;
	float bestDist;
	short i;
	short s;
	float d;
	float dx;
	float dy;

	best = -1;
	bestDist = -1;
	for (i = 0; i <= 3; i++) {
		s = gSystems[ship->curSystem].navDefault[i];
		if (s != -1 && stellarData[s].loaded && (stellarData[s].flags & kSpobCanLand)) {
			dx = FloatAbs(ship->pos.x - stellarData[s].pos.h);
			dy = FloatAbs(ship->pos.y - stellarData[s].pos.v);
			d = dx * dx + dy * dy;
			if (d < bestDist || bestDist < 0.0f) {
				bestDist = d;
				best = s;
			}
		}
	}
	return best;
}

// FUNCTION: EVO 0x10059920
void ShipStatsToSystemInfo(void)
{
	short i;
	short j;

	for (i = 0; i < MaxWeaponTypes; i++) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			if (gOutfits[j].modType[0] == kOutfModWeapon && i == gOutfits[j].modVal[0])
				outfitCount[j] = shipData->weap[i].count;
			if (gOutfits[j].modType[0] == kOutfModAmmo && i == gOutfits[j].modVal[0])
				outfitCount[j] = shipData->weap[i].ammo;
			if (outfitCount[j] < 0)
				outfitCount[j] = 0;
		}
	}
}

// FUNCTION: EVO 0x10059a44
void SystemInfoToShipStats(void)
{
	short i;

	for (i = 0; i < MaxWeaponTypes; i++)
		shipData->weap[i].count = 0;
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		if (gOutfits[i].modType[0] == kOutfModWeapon)
			shipData->weap[gOutfits[i].modVal[0]].count = outfitCount[i];
		if (gOutfits[i].modType[0] == kOutfModAmmo)
			shipData->weap[gOutfits[i].modVal[0]].ammo = outfitCount[i];
	}
}

// FUNCTION: EVO 0x10059b54
short ShipFreeMass(void)
{
	short mass;
	short i;

	mass = gShipTypes[shipData->type].freeMass;
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		if (outfitCount[i] > 0)
			mass -= gOutfits[i].mass * outfitCount[i];
	}
	return mass;
}

// FUNCTION: EVO 0x10059bf8
FloatPoint XYShortToFloat(ShortPoint pt)
{
	FloatPoint f;

	f.x = pt.h;
	f.y = pt.v;
	return f;
}

// FUNCTION: EVO 0x10059c58
Boolean IsDisabled(ShipObj *ship)
{
	if (ship->index == 0)
		return false;
	if (ship->govt != -1 && (govtData[ship->govt].flags & kGovtStartDisabled))
		return true;
	if (ship->defenseStellar != -1)
		return false;
	if (ship->shield < 0) {
		if (gShipTypes[ship->type].flags & kShipDisableAt10) {
			if (ship->shield * 100 < -90.0 * ShipArmorCapacity(ship))
				return true;
		} else {
			if (ship->shield * 100 < -33.333 * ShipArmorCapacity(ship))
				return true;
		}
	}
	return false;
}

// FUNCTION: EVO 0x10059ddc
Boolean IsDying(ShipObj *ship)
{
	if (ship->deathTimer > 0)
		return true;
	if (ship->shield < 0) {
		if (ship->shield < -ShipArmorCapacity(ship))
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x10059e54
long ShipShieldCapacity(ShipObj *ship)
{
	long cap;
	long j;
	short k;

	cap = gShipTypes[ship->type].shield;
	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k <= 1; k++) {
				if (gOutfits[j].modType[k] == kOutfModShield && outfitCount[j] > 0)
					cap += gOutfits[j].modVal[k] * outfitCount[j];
			}
		}
	} else if (ship->persType != -1) {
		cap *= personData[ship->persType].shieldMod;
	}
	return cap;
}

// FUNCTION: EVO 0x10059f70
short ShipShieldRechargeRate(ShipObj *ship)
{
	short rech;
	short j;
	short k;

	rech = gShipTypes[ship->type].shieldRech;
	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k <= 1; k++) {
				if (gOutfits[j].modType[k] == kOutfModShieldRech && outfitCount[j] > 0)
					rech += gOutfits[j].modVal[k] * outfitCount[j];
			}
		}
	} else if (ship->parentShip != 0)
		rech *= 0.65;
	rech /= timeScale;
	if (rech < 1)
		rech = 1;
	return rech;
}

// FUNCTION: EVO 0x1005a0d8
short ShipArmorCapacity(ShipObj *ship)
{
	short cap;
	short j;
	short k;

	cap = gShipTypes[ship->type].armor;
	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModArmor)
					cap += gOutfits[j].modVal[k] * outfitCount[j];
			}
		}
	}
	return cap;
}

// FUNCTION: EVO 0x1005a1a0
short ShipFuelCapacity(ShipObj *ship)
{
	short cap;
	short j;
	short k;

	cap = gShipTypes[ship->type].fuel;
	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModFuel && outfitCount[j] > 0)
					cap += gOutfits[j].modVal[k] * outfitCount[j];
			}
		}
	}
	return cap;
}

// FUNCTION: EVO 0x1005a27c
short ShipTurnRate(ShipObj *ship)
{
	short turn;
	short j;
	short k;

	turn = gShipTypes[ship->type].turnRate;
	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModTurn)
					turn += gOutfits[j].modVal[k] * outfitCount[j];
			}
		}
	} else if (ship->isTractored) {
		turn *= 0.333;
	}
	if (ship->persType == kPersAmbrosiaMascot && turn < 6)
		turn++;
	turn *= timeScale;
	if (gShipTypes[ship->type].turnRate >= 1 && turn < 1)
		turn = 1;
	return turn;
}

// FUNCTION: EVO 0x1005a40c
float ShipAccelRate(ShipObj *ship)
{
	float accel;
	short j;
	short k;

	accel = gShipTypes[ship->type].accel;
	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModAccel)
					accel += (float)(double)((float)(gOutfits[j].modVal[k] / 10000.0) * (float)outfitCount[j]);
			}
		}
		accel *= 2.0;
	} else {
		accel *= ship->skill;
		if (ship->isTractored)
			accel *= 0.333;
		else if (ship->parentShip == 0)
			accel *= 2.0;
		else
			accel *= 2.0;
	}
	if (ship->persType == kPersAmbrosiaMascot)
		accel *= 2.0;
	return accel;
}

// FUNCTION: EVO 0x1005a59c
float ShipMaxSpeed(ShipObj *ship)
{
	float speed;
	short j;
	short k;

	speed = gShipTypes[ship->type].maxSpeed;
	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModSpeed)
					speed += (float)(double)((float)(gOutfits[j].modVal[k] / 100.0) * (float)outfitCount[j]);
			}
		}
	} else {
		speed *= ship->skill;
		if (ship->isTractored)
			speed *= 0.333;
	}
	if (ship->persType == kPersAmbrosiaMascot)
		speed *= 2.0;
	if ((ship->index == 0 || ship->parentShip == 0) && !strictPlay)
		speed *= 1.5f;
	return speed;
}

// FUNCTION: EVO 0x1005a730
Boolean HasEscapePod(ShipObj *ship)
{
	short j;
	short k;

	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModEscapePod && outfitCount[j] > 0)
					return true;
			}
		}
	}
	return false;
}

// FUNCTION: EVO 0x1005a7e0
Boolean HasAutoEject(ShipObj *ship)
{
	short j;
	short k;

	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModAutoEject && outfitCount[j] > 0)
					return true;
			}
		}
	}
	return false;
}

// FUNCTION: EVO 0x1005a890
Boolean HasAfterburner(ShipObj *ship)
{
	short j;
	short k;

	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModAfterburner && outfitCount[j] > 0)
					return true;
			}
		}
	}
	return false;
}

// FUNCTION: EVO 0x1005a940
short ShipECM(ShipObj *ship, short kind)
{
	short i;
	short j;
	short level;

	level = 0;
	if (ship->index == 0) {
		for (i = 0; i < MaxOutfitItemTypes; i++) {
			for (j = 0; j < MaxOutfitMods; j++) {
				if (gOutfits[i].modType[j] == kOutfModJammer && outfitCount[i] > 0) {
					if (kind == 0 && (gOutfits[i].modVal[j] & kJamWeak1))
						level = 1;
					if (kind == 1 && (gOutfits[i].modVal[j] & kJamWeak2))
						level = 1;
					if (kind == 2 && (gOutfits[i].modVal[j] & kJamWeak3))
						level = 1;
					if (kind == 3 && (gOutfits[i].modVal[j] & kJamWeak4))
						level = 1;
				}
			}
		}
		for (i = 0; i < MaxOutfitItemTypes; i++) {
			for (j = 0; j < MaxOutfitMods; j++) {
				if (gOutfits[i].modType[j] == kOutfModJammer && outfitCount[i] > 0) {
					if (kind == 0 && (gOutfits[i].modVal[j] & kJamStrong1))
						level = 2;
					if (kind == 1 && (gOutfits[i].modVal[j] & kJamStrong2))
						level = 2;
					if (kind == 2 && (gOutfits[i].modVal[j] & kJamStrong3))
						level = 2;
					if (kind == 3 && (gOutfits[i].modVal[j] & kJamStrong4))
						level = 2;
				}
			}
		}
	} else if (ship->govt != -1) {
		if (kind == 0 && (govtData[ship->govt].inherentJam & kJamWeak1))
			level = 1;
		if (kind == 1 && (govtData[ship->govt].inherentJam & kJamWeak2))
			level = 1;
		if (kind == 2 && (govtData[ship->govt].inherentJam & kJamWeak3))
			level = 1;
		if (kind == 3 && (govtData[ship->govt].inherentJam & kJamWeak4))
			level = 1;
		if (kind == 0 && (govtData[ship->govt].inherentJam & kJamStrong1))
			level = 2;
		if (kind == 1 && (govtData[ship->govt].inherentJam & kJamStrong2))
			level = 2;
		if (kind == 2 && (govtData[ship->govt].inherentJam & kJamStrong3))
			level = 2;
		if (kind == 3 && (govtData[ship->govt].inherentJam & kJamStrong4))
			level = 2;
		if (ship->aiType == kAIWimpyTrader || ship->aiType == kAIBraveTrader) {
			if (govtData[ship->govt].inherentJam & kJamFreightersHalf) {
				level--;
				if (level < 0)
					level = 0;
			}
		}
	}
	if (level == 2)
		return level;
	if (level == 1 && Rand(2) == 0)
		return 1;
	return 0;
}

// FUNCTION: EVO 0x1005ae04
Boolean HasCloak(ShipObj *ship)
{
	short j;
	short k;

	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModCloak && outfitCount[j] > 0)
					return true;
			}
		}
	}
	return false;
}

// FUNCTION: EVO 0x1005aeb4
Boolean HasDensityScanner(ShipObj *ship)
{
	short j;
	short k;

	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModDensityScanner && outfitCount[j] > 0)
					return true;
			}
		}
	}
	return false;
}

// FUNCTION: EVO 0x1005af64
Boolean HasIFF(ShipObj *ship)
{
	short j;
	short k;

	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModIFF && outfitCount[j] > 0)
					return true;
			}
		}
	}
	return false;
}

// FUNCTION: EVO 0x1005b014
short ShipHyperTransitTime(ShipObj *ship)
{
	short slots;
	short j;
	short k;

	slots = 1;
	if (gShipTypes[ship->type].mass >= 100)
		slots++;
	if (gShipTypes[ship->type].mass >= 200)
		slots++;
	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModHyperSpeed && outfitCount[j] > 0)
					slots += gOutfits[j].modVal[k] * outfitCount[j];
			}
		}
	}
	if (slots < 1)
		slots = 1;
	return slots;
}

// FUNCTION: EVO 0x1005b134
long ShipHyperSafeDist(ShipObj *ship)
{
	long dist;
	short j;
	short k;

	dist = JumpDistance;
	if (ship->index == 0) {
		for (j = 0; j < MaxOutfitItemTypes; j++) {
			for (k = 0; k < MaxOutfitMods; k++) {
				if (gOutfits[j].modType[k] == kOutfModHyperDist && outfitCount[j] > 0)
					dist += gOutfits[j].modVal[k] * outfitCount[j];
			}
		}
	}
	if (dist < 0)
		dist = 0;
	return dist * dist;
}

// FUNCTION: EVO 0x1005b208
short CalcLeadAngle(ShipObj *shooter, ShipObj *target, short weapType)
{
	short angle;
	float dx;
	float dy;
	float time;
	float speed;
	float dist;
	long dead[4];
	FloatPoint aim;

	angle = CalcInterceptAngle(shooter->pos, target->pos);
	if (weapType != -1) {
		if (gWeapons[weapType].guidance == kGuidanceUnguided || gWeapons[weapType].guidance == kGuidanceTurret ||
				gWeapons[weapType].guidance == kGuidanceFrontQuad || gWeapons[weapType].guidance == kGuidanceRearQuad) {
			dx = target->pos.x - shooter->pos.x;
			dy = target->pos.y - shooter->pos.y;
			speed = gWeapons[weapType].speed;
			dist = sqrt(dx * dx + dy * dy);
			time = dist / speed;
			aim.x = target->pos.x + time * (target->vel.x - shooter->vel.x);
			aim.y = target->pos.y + time * (target->vel.y - shooter->vel.y);
			angle = CalcInterceptAngle(shooter->pos, aim);
		}
	}
	return angle;
}

// FUNCTION: EVO 0x1005b388
Boolean HasMaxOfItem(short id)
{
	short guns;
	short turrets;
	short i;

	if (id != -1) {
		if (outfitCount[id] >= gOutfits[id].maxCount)
			return true;
		guns = turrets = 0;
		for (i = 0; i < MaxOutfitItemTypes; i++) {
			if (gOutfits[i].flags & kOutfFixedGun)
				guns += outfitCount[i];
			if (gOutfits[i].flags & kOutfTurret)
				turrets += outfitCount[i];
		}
		if ((gOutfits[id].flags & kOutfFixedGun) && guns >= gShipTypes[shipData->type].maxGuns)
			return true;
		if ((gOutfits[id].flags & kOutfTurret) && turrets >= gShipTypes[shipData->type].maxTurrets)
			return true;
		return false;
	}
	/* BUG: no return value for id == -1 */
}

// FUNCTION: EVO 0x1005b4f8
void DrawCommaNumber(long n)
{
	Str255 s;

	if (n < 1000) {
		NumToString(n, s);
		DrawString(s);
	} else if (n < 1000000) {
		NumToString(n / 1000, s);
		DrawString(s);
		DrawString("\p,");
		if (n % 1000 < 100)
			DrawString("\p0");
		if (n % 1000 < 10)
			DrawString("\p0");
		NumToString(n % 1000, s);
		DrawString(s);
	} else {
		NumToString(n / 1000000, s);
		DrawString(s);
		DrawString("\p.");
		NumToString((n % 1000000) / 10000, s);
		if ((n % 1000000) / 10000 < 10)
			DrawString("\p0");
		DrawString(s);
		DrawString("\pM");
	}
}

// FUNCTION: EVO 0x1005b698
Boolean CanBuyGoods(short i)
{
	if (shipData->credits < commodityPrice[i])
		return false;
	if (TotalCargo(shipData) >= TotalFleetHolds())
		return false;
	return true;
}

// FUNCTION: EVO 0x1005b71c
Boolean CanSellGoods(short i)
{
	if (i <= 5) {
		if (shipData->cargo[i] > 0)
			return true;
	} else {
		if (junkSlot[i - 6] != -1 && junkData[junkSlot[i - 6]].held > 0)
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x1005b7ac
void ColorCodeShip(ShipObj *ship)
{
	if (ship->index == 0) {
		ForeColor(cyanColor);
	} else if (IsDisabled(ship)) {
		RGBForeColor(&rgbDarkGrey);
	} else if (IsThreatToPlayer(ship)) {
		ForeColor(redColor);
	} else if (ship->parentShip == 0 && ship->defenseStellar == -1) {
		RGBForeColor(&brightGreen);
	} else if (ship->parentShip != -1) {
		if (shipData[ship->parentShip].parentShip == 0)
			RGBForeColor(&brightGreen);
		else
			ForeColor(blueColor);
	} else {
		ForeColor(blueColor);
	}
}

// FUNCTION: EVO 0x1005b8b4
void ColorCodeStellar(SpobRecord *spob)
{
	Boolean uninhabited;
	RGBColor orange;

	uninhabited = spob->flags & kSpobUninhabited;
	orange.red = 0xFFFF;
	orange.green = 0x6666;
	orange.blue = 0;
	if (!(spob->flags & kSpobCanLand)) {
		RGBForeColor(&rgbMedGrey);
	} else if (uninhabited) {
		ForeColor(blueColor);
	} else if (!spob->dominated) {
		if (spob->govt == -1) {
			ForeColor(yellowColor);
		} else if (spob->govt != -1) {
			if (legalStatus[spob->system] < spob->minStatus) {
				if (legalStatus[spob->system] >= 0)
					RGBForeColor(&orange);
				else
					ForeColor(redColor);
			} else {
				ForeColor(yellowColor);
			}
		} else {
			ForeColor(yellowColor);
		}
	} else {
		ForeColor(greenColor);
	}
}

// FUNCTION: EVO 0x1005b9f4
void ColorCodeSystem(short syst)
{
	RGBColor orange;
	short idle, trading, state, j;

	orange.red = 0xFFFF;
	orange.green = 0x6666;
	orange.blue = 0;
	if (gSystems[syst].visited > 0) {
		if (SystemIsInhabited(syst)) {
			ForeColor(blueColor);
			trading = idle = 0;
			state = 0;
			for (j = 0; j < 3; j++) {
				if (gSystems[syst].navDefault[j] != -1 &&
						stellarData[gSystems[syst].navDefault[j]].loaded &&
						!SpobUninhabited(&stellarData[gSystems[syst].navDefault[j]]) &&
						SpobCanLand(&stellarData[gSystems[syst].navDefault[j]])) {
					if (legalStatus[syst] < stellarData[gSystems[syst].navDefault[j]].minStatus) {
						if (legalStatus[syst] >= 0 && state < 1)
							state = 1;
						if (legalStatus[syst] < 0)
							state = 2;
					}
					if (stellarData[gSystems[syst].navDefault[j]].dominated)
						trading++;
					else
						idle++;
				}
				if (state == 1)
					RGBForeColor(&orange);
				if (state == 2)
					ForeColor(redColor);
			}
			if (trading > 0)
				RGBForeColor(&brightGreen);
		} else {
			RGBForeColor(&rgbLightGrey);
		}
	} else {
		RGBForeColor(&rgbDarkGrey);
	}
}

// FUNCTION: EVO 0x1005bca0
void CenterPict(PicHandle pic, Rect *r)
{
	Rect a;
	Rect b;

	if (pic == nil)
		return;
	a = *r;
	b = (*pic)->picFrame;
	r->top = a.top + ((a.bottom - a.top) - (b.bottom - b.top)) / 2;
	r->bottom = r->top + (b.bottom - b.top);
	r->left = a.left + ((a.right - a.left) - (b.right - b.left)) / 2;
	r->right = r->left + (b.right - b.left);
}

// FUNCTION: EVO 0x1005bd60
void HideTheCursor(void)
{
	if (!cursorHidden) {
		cursorHidden = true;
		HideCursor();
	}
}

// FUNCTION: EVO 0x1005bda0
void CullNameString(StringPtr name)
{
	short dead1[7];
	Str255 buf;
	Str255 out;
	short dead2;
	short i;
	short newLen;
	short skip;
	short len;

	strcpy((char *)buf, (char *)name);
	p2cstr(buf);
	skip = 0;
	len = strlen((char *)buf);
	newLen = len;
	if (tolower(buf[0]) == 't' && tolower(buf[1]) == 'h' && tolower(buf[2]) == 'e' && buf[3] == ' ') {
		skip += 4;
		newLen -= 4;
	}
	for (i = 0; i < newLen; i++)
		out[i] = buf[i + skip];
	out[i] = 0;
	c2pstr((char *)out);
	strcpy((char *)name, (char *)out);
}

// FUNCTION: EVO 0x1005beb4
short FirstKeyDown(void)
{
	unsigned short keys[8];
	short i;
	short word;
	short bit;

	GetKeys((unsigned long *)keys);
	for (i = 0; i <= 127; i++) {
		word = (i >> 4) & 0xF;
		bit = i & 0xF;
		if (i != kKeyDigit1 && i != kKeyDigit2 && i != kKeyDigit3 && i != kKeyDigit4 && i != kKeyPower && i != kKeyCapsLock && i != kKeyF1 &&
				i != kKeyF2 && i != kKeyF4 && i != kKeyF3 && i != kKeyF5) {
			if ((1 << bit) & keys[word])
				return i;
		}
	}
	return -1;
}

// FUNCTION: EVO 0x1005bfc8
void SaveKeyAssignments(void)
{
	newKeyPrefs[0] = keyMap[24];
	newKeyPrefs[1] = keyMap[25];
	newKeyPrefs[2] = keyMap[23];
	newKeyPrefs[3] = keyMap[22];
	newKeyPrefs[4] = keyMap[27];
	newKeyPrefs[5] = keyMap[7];
	newKeyPrefs[6] = keyMap[12];
	newKeyPrefs[7] = keyMap[13];
	newKeyPrefs[8] = keyMap[14];
	newKeyPrefs[9] = keyMap[8];
	newKeyPrefs[10] = keyMap[17];
	newKeyPrefs[11] = keyMap[18];
	newKeyPrefs[12] = keyMap[19];
	newKeyPrefs[13] = keyMap[2];
	newKeyPrefs[14] = keyMap[3];
	newKeyPrefs[15] = keyMap[0];
	newKeyPrefs[16] = keyMap[1];
	newKeyPrefs[17] = keyMap[10];
	newKeyPrefs[18] = keyMap[11];
	newKeyPrefs[19] = keyMap[26];
	newKeyPrefs[20] = keyMap[6];
	newKeyPrefs[21] = keyMap[4];
	newKeyPrefs[22] = keyMap[5];
	newKeyPrefs[23] = keyMap[15];
	newKeyPrefs[24] = keyMap[16];
	newKeyPrefs[25] = keyMap[20];
	newKeyPrefs[26] = keyMap[21];
	newKeyPrefs[27] = keyMap[9];
	newKeyPrefs[28] = keyMap[28];
	newKeyPrefs[29] = keyMap[43];
	newKeyPrefs[30] = keyMap[44];
}

// FUNCTION: EVO 0x1005c0dc
void LoadKeyAssignments(void)
{
	keyMap[24] = newKeyPrefs[0];
	keyMap[25] = newKeyPrefs[1];
	keyMap[23] = newKeyPrefs[2];
	keyMap[22] = newKeyPrefs[3];
	keyMap[27] = newKeyPrefs[4];
	keyMap[7] = newKeyPrefs[5];
	keyMap[12] = newKeyPrefs[6];
	keyMap[13] = newKeyPrefs[7];
	keyMap[14] = newKeyPrefs[8];
	keyMap[8] = newKeyPrefs[9];
	keyMap[17] = newKeyPrefs[10];
	keyMap[18] = newKeyPrefs[11];
	keyMap[19] = newKeyPrefs[12];
	keyMap[2] = newKeyPrefs[13];
	keyMap[3] = newKeyPrefs[14];
	keyMap[0] = newKeyPrefs[15];
	keyMap[1] = newKeyPrefs[16];
	keyMap[10] = newKeyPrefs[17];
	keyMap[11] = newKeyPrefs[18];
	keyMap[26] = newKeyPrefs[19];
	keyMap[6] = newKeyPrefs[20];
	keyMap[4] = newKeyPrefs[21];
	keyMap[5] = newKeyPrefs[22];
	keyMap[15] = newKeyPrefs[23];
	keyMap[16] = newKeyPrefs[24];
	keyMap[20] = newKeyPrefs[25];
	keyMap[21] = newKeyPrefs[26];
	keyMap[9] = newKeyPrefs[27];
	keyMap[28] = newKeyPrefs[28];
	keyMap[43] = newKeyPrefs[29];
	keyMap[44] = newKeyPrefs[30];
}

// FUNCTION: EVO 0x1005c1f0
Boolean DupKeyCheck(short key, short slot)
{
	short i;

	for (i = 0; i <= 30; i++) {
		if (key == newKeyPrefs[i] && i != slot && i != 23 && i != 25 && i != 26)
			return true;
	}
	for (i = 23; i <= 26; i++) {
		if (key == newKeyPrefs[i] && i != slot && i != 24)
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x1005c2cc
void IncrementDate(DateTimeRec *date, Boolean unused)
{
	long dead[2];
	DateTimeRec dm;
	long dead2;

	if (date->month == 1)
		dm.day = 31;
	if (date->month == 3)
		dm.day = 31;
	if (date->month == 4)
		dm.day = 30;
	if (date->month == 5)
		dm.day = 31;
	if (date->month == 6)
		dm.day = 30;
	if (date->month == 7)
		dm.day = 31;
	if (date->month == 8)
		dm.day = 31;
	if (date->month == 9)
		dm.day = 30;
	if (date->month == 10)
		dm.day = 31;
	if (date->month == 11)
		dm.day = 30;
	if (date->month == 12)
		dm.day = 31;
	if (date->month == 2) {
		if (date->year % 4 == 0)
			dm.day = 29;
		else
			dm.day = 28;
	}
	date->day++;
	if (date->day > dm.day) {
		date->day = 1;
		date->month++;
		if (date->month > 12) {
			date->month = 1;
			date->year++;
		}
	}
}

// FUNCTION: EVO 0x1005c438
void IncrementGameTime(void)
{
	short i;
	short j;
	short limit;
	long dead[4];

	IncrementDate(&gameTime, 0);
	for (i = 0; i < MaxSimultaneousMissions; i++) {
		if (currentMissions[i].timeLimit > 0)
			currentMissions[i].timeLimit--;
	}
	DoStellarTimePassage();
	DisasterHandler();
	for (j = 0; j < MaxStellarObjects; j++) {
		if (stellarData[j].loaded && stellarData[j].dominated) {
			if (stellarData[j].defCount > 1000)
				limit = stellarData[j].defCount % 1000;
			else
				limit = stellarData[j].defCount;
			if (stellarData[j].tribute < limit) {
				if (Rand(450) == 0)
					stellarData[j].tribute++;
			}
		}
	}
}

// FUNCTION: EVO 0x1005c5cc
void SlapWithPenalty(short a, short b, short c, short mode)
{
	long dead[4];
	short i;

	for (i = 0; i < MaxSystems; i++)
		systemFlag[i] = 0;
	if (mode == -1)
		RecursivePenaltySlap(a, b, c, 1.0);
}

// FUNCTION: EVO 0x1005c654
void RecursivePenaltySlap(short syst, short victim, short crime, long double scale)
{
	float delta;
	short vFlag, sFlag1, sFlag2;
	short i;

	if (!systemFlag[syst]) {
		systemFlag[syst] = 1;
		delta = 0.0;
		vFlag = 0;
		sFlag1 = 0;
		sFlag2 = 0;
		if (victim != -1)
			vFlag = govtData[victim].flags & kGovtXenophobic;
		else
			vFlag = 0;
		if (gSystems[syst].govt != -1) {
			sFlag1 = govtData[gSystems[syst].govt].flags & kGovtXenophobic;
			sFlag2 = govtData[gSystems[syst].govt].flags & kGovtNosy;
		}
		if (gSystems[syst].govt == -1) {
			if (victim == -1)
				delta = 0.5 * govtData->penalties[crime];
			else if (vFlag)
				delta = 0.5 * -govtData[victim].penalties[crime];
			else
				delta = 0.25 * govtData[victim].penalties[crime];
		} else if (victim == gSystems[syst].govt) {
			delta = govtData[victim].penalties[crime];
		} else if (victim == -1) {
			if (sFlag1)
				delta = 0.5 * -govtData[gSystems[syst].govt].penalties[crime];
			else if (sFlag2)
				delta = 0.5 * govtData[gSystems[syst].govt].penalties[crime];
		} else if (govtData[victim].ally != gSystems[syst].govt && victim != govtData[gSystems[syst].govt].ally && vFlag) {
			delta = 0.5 * -govtData[gSystems[syst].govt].penalties[crime];
		} else if (govtData[victim].enemy != gSystems[syst].govt && victim != govtData[gSystems[syst].govt].enemy && !vFlag) {
			if (govtData[victim].ally == gSystems[syst].govt || victim == govtData[gSystems[syst].govt].ally)
				delta = govtData[victim].penalties[crime];
			else
				delta = 0.5 * govtData[victim].penalties[crime];
		} else {
			delta = 0.5 * -govtData[gSystems[syst].govt].penalties[crime];
		}
		delta *= scale;
		if (!(delta < 1.0 && delta > -1.0)) {
			legalStatus[syst] -= delta;
			if (legalStatus[syst] > kShortBound)
				legalStatus[syst] = kShortBound;
			if (legalStatus[syst] < -kShortBound)
				legalStatus[syst] = -kShortBound;
			scale *= 0.65;
			for (i = 0; i < kMaxSystemLinks; i++) {
				if (gSystems[syst].links[i] != -1)
					RecursivePenaltySlap(gSystems[syst].links[i], victim, crime, scale);
			}
		}
	}
}

// FUNCTION: EVO 0x1005cd50
void AutoSetExploration(short start, short limit)
{
	short i;

	for (i = 0; i < MaxSystems; i++)
		systemFlag[i] = 0;
	maxAutoExploreJumps = limit;
	RecursiveAutoExplore(start, 0);
}

// FUNCTION: EVO 0x1005cdc8
void RecursiveAutoExplore(short syst, short depth)
{
	short i;
	long dead[4];

	if (depth <= maxAutoExploreJumps && !systemFlag[syst]) {
		systemFlag[syst] = 1;
		ExploreNebulaeFromSystem(syst);
		if (gSystems[syst].visited < 2)
			gSystems[syst].visited = 2;
		for (i = 0; i < kMaxSystemLinks; i++) {
			if (gSystems[syst].links[i] != -1 && gSystems[gSystems[syst].links[i]].loaded)
				RecursiveAutoExplore(gSystems[syst].links[i], depth + 1);
		}
	}
}

// FUNCTION: EVO 0x1005cf28
void ExploreNebulaeFromSystem(short syst)
{
	Rect r;
	Point pt;
	short i;

	for (i = 0; i < 4; i++) {
		if (!nebulaData[i].explored && nebulaData[i].width > 0 && nebulaData[i].height > 0) {
			SetRect(&r, nebulaData[i].xPos, nebulaData[i].yPos, nebulaData[i].xPos + nebulaData[i].width,
					nebulaData[i].yPos + nebulaData[i].height);
			InsetRect(&r, 8, 8);
			pt.h = gSystems[syst].xPos;
			pt.v = gSystems[syst].yPos;
			if (PtInRect(pt, &r))
				nebulaData[i].explored = true;
		}
	}
}

// FUNCTION: EVO 0x1005d0d0
void DisplayMessage(short n)
{
	Str255 s;

	if (!LoadPluginString(n + 999, s))
		GetIndString(s, kMessageBuoyStrList, n);
	DisplayComm(s, 480, 0, 12, &rgbWhite, 0, 0);
}

// FUNCTION: EVO 0x1005d148
void FadeScreenOut(short speed, RGBColor *color)
{
	FadeOut(speed, color);
}

// FUNCTION: EVO 0x1005d17c
void FadeScreenIn(short speed)
{
	FadeIn(speed);
}

// FUNCTION: EVO 0x1005d1a8
void InitCluts(void)
{
	RGBColor c;
	short i;

	gameClut = GetCTable(8);
	workClut = GetCTable(8);
	c.red = c.blue = c.green = 0xFFFF;
	workClut = GetCTable(8);
	SetClutToColor(&c, 0);
	workClut = GetCurrentClut();
	SetMonitorClut(gameClut, 1);
	for (i = 1; i <= 6; i++) {
		hueCluts[i] = GetCTable(8);
		HueScale(i, 0);
		hueCluts[i] = GetCurrentClut();
		SetMonitorClut(gameClut, 1);
	}
}

// FUNCTION: EVO 0x1005d2c4
void RestoreOrigClut(void)
{
	SetMonitorClut(gameClut, 0);
}

// FUNCTION: EVO 0x1005d2f4
void SetHudColorsJammed(void)
{
	dimGreen.red = dimGreen.green = dimGreen.blue = 0xFFFF;
	dimmerGreen.red = dimmerGreen.green = dimmerGreen.blue = 0xFFFF;
	brightGreen.red = brightGreen.green = brightGreen.blue = 0xFFFF;
}

// FUNCTION: EVO 0x1005d358
void SetHudColorsNormal(void)
{
	dimGreen.red = dimGreen.blue = 0;
	dimGreen.green = 0x9900;
	dimmerGreen.red = dimmerGreen.blue = 0;
	dimmerGreen.green = 0x6666;
	brightGreen.red = brightGreen.blue = 0;
	brightGreen.green = 0xFFFF;
}

// FUNCTION: EVO 0x1005d3c4
void DoShipCloak(void)
{
	short hue;
	short i;
	short j;

	if (!gInvertedColors) {
		PlaySound(cloakSnd[1], 8, kMaxVoiceVolume, kMaxVoiceVolume);
		hue = -1;
		for (i = 0; i < MaxOutfitItemTypes; i++) {
			for (j = 0; j < MaxOutfitMods; j++) {
				if (gOutfits[i].modType[j] == kOutfModCloak && outfitCount[i] > 0)
					hue = gOutfits[i].modVal[j];
			}
		}
		if (hue != -1) {
			SetHudColorsJammed();
			SetMonitorClut(hueCluts[hue], 1);
		}
		gInvertedColors = true;
		if (shipData->shield > 0)
			shipData->shield = 0;
		hudRedraw.nav = hudRedraw.shield = hudRedraw.credits = hudRedraw.ammo = hudRedraw.fuel = hudRedraw.radar = hudRedraw.target = true;
	}
}

// FUNCTION: EVO 0x1005d52c
void DoShipUncloak(void)
{
	if (gInvertedColors) {
		PlaySound(cloakSnd[0], 8, kMaxVoiceVolume, kMaxVoiceVolume);
		SetHudColorsNormal();
		hudRedraw.shield = hudRedraw.fuel = hudRedraw.nav = hudRedraw.ammo = true;
		DrawStatus();
		SetMonitorClut(gameClut, 1);
		gInvertedColors = false;
		RedrawStatus();
	}
}

// FUNCTION: EVO 0x1005d5c8
void ReapplyCloakPalette(void)
{
	short i;
	short j;

	for (i = 0; i < MaxOutfitItemTypes; i++) {
		for (j = 0; j < MaxOutfitMods; j++) {
			if (gOutfits[i].modType[j] == kOutfModCloak && outfitCount[i] > 0) {
				SetMonitorClut(hueCluts[gOutfits[i].modVal[j]], 1);
				return;
			}
		}
	}
}

// FUNCTION: EVO 0x1005d69c
Boolean SystemTradesGoods(short syst, short type)
{
	short i;

	for (i = 0; i <= 3; i++) {
		if (gSystems[syst].navDefault[i] != -1 && stellarData[gSystems[syst].navDefault[i]].loaded &&
				!(stellarData[gSystems[syst].navDefault[i]].flags & kSpobUninhabited) &&
				(stellarData[gSystems[syst].navDefault[i]].flags & kSpobCanLand) &&
				(stellarData[gSystems[syst].navDefault[i]].flags & kSpobTradeCenter) &&
				CalcPortDemand(type, stellarData[gSystems[syst].navDefault[i]].flags))
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x1005d840
Boolean SystemHasService(short syst, short type)
{
	short i;
	long mask;

	for (i = 0; i <= 3; i++) {
		if (gSystems[syst].navDefault[i] != -1 && stellarData[gSystems[syst].navDefault[i]].loaded &&
				!(stellarData[gSystems[syst].navDefault[i]].flags & kSpobUninhabited) &&
				(stellarData[gSystems[syst].navDefault[i]].flags & kSpobCanLand)) {
			if (type == 0)
				mask = 2;
			if (type == 1)
				mask = 4;
			if (type == 2)
				mask = 8;
			if (stellarData[gSystems[syst].navDefault[i]].flags & mask)
				return true;
		}
	}
	return false;
}

static unsigned long	randSeed;

// FUNCTION: EVO 0x1005d9c4
short Rand(short range)
{
	unsigned long t, lo, hi, seed;
	short result;

	if (range == 0) {
		randSeed = LMGetTicks() + LMGetTime();
	} else {
		seed = randSeed;
		lo = 16807;
		hi = lo;
		lo *= seed & 0xFFFF;
		t = lo;
		t = t >> 16;
		hi *= seed >> 16;
		hi += t;
		t = hi;
		t += t;
		t = t >> 16;
		lo = lo;
		lo = lo & 0xFFFF;
		lo -= LONG_MAX;
		hi = hi & 0x7FFF;
		hi = (hi << 16) + (hi >> 16);
		hi += t;
		lo += hi;
		/* BUG: lo is unsigned, so the carry fold never runs */
		if (lo < 0)
			lo += LONG_MAX;
		randSeed = lo;
		if ((lo & 0xFFFF) == 0x8000)
			lo &= 0xFFFF0000;
		t = range;
		t *= lo & 0xFFFF;
		t = t >> 16;
		result = t;
	}
	return result;
}

// FUNCTION: EVO 0x1005da9c
void DateToShortString(DateTimeRec date, char *out)
{
	Str255 num;

	strcpy(out, "");
	NumToString(date.month, num);
	p2cstr(num);
	strcat(out, (char *)num);
	strcat(out, "/");
	NumToString(date.day, num);
	p2cstr(num);
	strcat(out, (char *)num);
	strcat(out, "/");
	NumToString(date.year % 100, num);
	p2cstr(num);
	strcat(out, (char *)num);
}

// FUNCTION: EVO 0x1005db98
void DateToMedString(DateTimeRec date, char *out)
{
	Str255 num;

	strcpy(out, "");
	if (date.month == 1)
		strcat(out, "Jan.");
	if (date.month == 2)
		strcat(out, "Feb.");
	if (date.month == 3)
		strcat(out, "Mar.");
	if (date.month == 4)
		strcat(out, "Apr.");
	if (date.month == 5)
		strcat(out, "May");
	if (date.month == 6)
		strcat(out, "June");
	if (date.month == 7)
		strcat(out, "July");
	if (date.month == 8)
		strcat(out, "Aug.");
	if (date.month == 9)
		strcat(out, "Sept.");
	if (date.month == 10)
		strcat(out, "Oct.");
	if (date.month == 11)
		strcat(out, "Nov.");
	if (date.month == 12)
		strcat(out, "Dec.");
	strcat(out, " ");
	NumToString(date.day, num);
	p2cstr(num);
	strcat(out, (char *)num);
	strcpy((char *)num, "th");
	if (date.day % 10 == 1)
		strcpy((char *)num, "st");
	if (date.day % 10 == 2)
		strcpy((char *)num, "nd");
	if (date.day % 10 == 3)
		strcpy((char *)num, "rd");
	if (date.day >= 11 && date.day <= 13)
		strcpy((char *)num, "th");
	strcat(out, (char *)num);
	strcat(out, ", ");
	NumToString(date.year, num);
	p2cstr(num);
	strcat(out, (char *)num);
}

// FUNCTION: EVO 0x1005de74
void DateToLongString(DateTimeRec date, char *out)
{
	Str255 num;

	strcpy(out, "");
	if (date.month == 1)
		strcat(out, "January");
	if (date.month == 2)
		strcat(out, "February");
	if (date.month == 3)
		strcat(out, "March");
	if (date.month == 4)
		strcat(out, "April");
	if (date.month == 5)
		strcat(out, "May");
	if (date.month == 6)
		strcat(out, "June");
	if (date.month == 7)
		strcat(out, "July");
	if (date.month == 8)
		strcat(out, "August");
	if (date.month == 9)
		strcat(out, "Sept.");
	if (date.month == 10)
		strcat(out, "October");
	if (date.month == 11)
		strcat(out, "November");
	if (date.month == 12)
		strcat(out, "December");
	strcat(out, " ");
	NumToString(date.day, num);
	p2cstr(num);
	strcat(out, (char *)num);
	strcpy((char *)num, "th");
	if (date.day % 10 == 1)
		strcpy((char *)num, "st");
	if (date.day % 10 == 2)
		strcpy((char *)num, "nd");
	if (date.day % 10 == 3)
		strcpy((char *)num, "rd");
	if (date.day >= 11 && date.day <= 13)
		strcpy((char *)num, "th");
	strcat(out, (char *)num);
	strcat(out, ", ");
	NumToString(date.year, num);
	p2cstr(num);
	strcat(out, (char *)num);
}

// FUNCTION: EVO 0x1005e150
Boolean CanHireEscorts(void)
{
	short count;
	short i;

	count = 0;
	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && shipData[i].parentShip == 0 && shipData[i].aiType == kAIEscort &&
				shipData[i].specialMission == -1)
			count++;
	}
	if (count < 6)
		return true;
	return false;
}

// FUNCTION: EVO 0x1005e218
Boolean WeaponHasAmmo(ShipObj *ship, short type)
{
	if (gWeapons[type].ammoType >= 0 && ship->weap[type].ammo <= 0)
		return false;
	if (gWeapons[type].ammoType <= kAmmoFuelBase) {
		if (ship->fuel < abs(gWeapons[type].ammoType) + kAmmoFuelBase)
			return false;
	}
	return true;
}

// FUNCTION: EVO 0x1005e2d8
Boolean SystemIsInhabited(short syst)
{
	short count;
	short i;

	count = 0;
	for (i = 0; i <= 3; i++) {
		if (gSystems[syst].navDefault[i] != -1 && !(stellarData[gSystems[syst].navDefault[i]].flags & kSpobUninhabited))
			count++;
	}
	if (count == 0)
		return false;
	return true;
}

// FUNCTION: EVO 0x1005e398
Boolean StellarsAreDistant(SpobRecord *a, SpobRecord *b)
{
	short i;

	if (a->system == b->system)
		return false;
	if (!gSystems[a->system].loaded)
		return true;
	if (!gSystems[b->system].loaded)
		return true;
	for (i = 0; i < kMaxSystemLinks; i++) {
		if (gSystems[a->system].links[i] != -1 && b->system == gSystems[a->system].links[i])
			return false;
		if (gSystems[b->system].links[i] != -1 && a->system == gSystems[b->system].links[i])
			return false;
	}
	return true;
}

// FUNCTION: EVO 0x1005e4e4
void DrawLegalStatusString(short syst)
{
	long status;
	long tol;
	short cat;
	short busy, idle;
	short i;

	status = legalStatus[syst];
	cat = 0;
	if (gSystems[syst].govt >= 0 && gSystems[syst].govt < MaxGovts)
		tol = govtData[gSystems[syst].govt].crimeTol;
	else
		tol = govtData->crimeTol;
	if (status < 0)
		cat = 2;
	if (status < -tol)
		cat = 3;
	if (status < -tol * 4)
		cat = 4;
	if (status < -tol * 16)
		cat = 5;
	if (status < -tol * 64)
		cat = 6;
	if (status < -tol * 256)
		cat = 7;
	if (status < -tol * 1024)
		cat = 8;
	if (status < -tol * 4096)
		cat = 9;
	if (status == 0)
		cat = 1;
	if (status > 0)
		cat = 10;
	if (status > tol << 2)
		cat = 11;
	if (status > tol << 4)
		cat = 12;
	if (status > tol << 6)
		cat = 13;
	if (status > tol << 8)
		cat = 14;
	if (status > tol << 10)
		cat = 15;
	busy = idle = 0;
	for (i = 0; i < 3; i++) {
		if (gSystems[syst].navDefault[i] != -1) {
			if (stellarData[gSystems[syst].navDefault[i]].loaded) {
				if (!(stellarData[gSystems[syst].navDefault[i]].flags & kSpobUninhabited) && (stellarData[gSystems[syst].navDefault[i]].flags & kSpobCanLand)) {
					if (stellarData[gSystems[syst].navDefault[i]].dominated)
						busy++;
					else
						idle++;
				}
			}
		}
	}
	if (busy > 0) {
		if (idle > 0)
			cat = 16;
		else
			cat = 17;
	}
	if (gSystems[syst].govt >= 0) {
		if (govtData[gSystems[syst].govt].flags & kGovtXenophobic)
			cat = 0;
	}
	if (cat == 0)
		DrawString("\pN/A");
	else
		DrawString(legalStrings[cat]);
}

// FUNCTION: EVO 0x1005e85c
void DrawCombatRatingString(void)
{
	short tier;

	tier = 0;
	if (playerKills > 0)
		tier = 1;
	if (playerKills >= 100)
		tier = 2;
	if (playerKills >= 200)
		tier = 3;
	if (playerKills >= 400)
		tier = 4;
	if (playerKills >= 800)
		tier = 5;
	if (playerKills >= 1600)
		tier = 6;
	if (playerKills >= 3200)
		tier = 7;
	if (playerKills >= 6400)
		tier = 8;
	if (playerKills >= 12800)
		tier = 9;
	if (playerKills >= 25600)
		tier = 10;
	DrawString(ratingStrings[tier]);
}

// FUNCTION: EVO 0x1005e948
long PlayerShipTradeInPrice(void)
{
	short i;
	long value;

	value = 0.25 * gShipTypes[shipData->type].cost;
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		if (outfitCount[i] > 0 && !gOutfits[i].persistent)
			value += 0.95 * (gOutfits[i].cost * outfitCount[i]);
	}
	return value;
}

// FUNCTION: EVO 0x1005ea80
Boolean PlayerCanUseAmmoType(short ammoType)
{
	short i;

	for (i = 0; i < MaxWeaponTypes; i++) {
		if (ammoType == gWeapons[i].ammoType && shipData->weap[i].count > 0)
			return true;
	}
	return false;
}

// FUNCTION: EVO 0x1005eb04
Boolean PlaySoundDistance(short pan, Ptr snd, short priority, FloatPoint *where, FloatPoint *listener)
{
	double dx;
	double dy;
	short right;
	short left;

	dx = where->x - listener->x;
	dy = where->y - listener->y;
	left = right = kMaxVoiceVolume;
	if (FloatAbs(dx) > 400.0f || FloatAbs(dy) > 400.0f)
		left = right = kMaxVoiceVolume;
	if (FloatAbs(dx) > 1000.0f || FloatAbs(dy) > 1000.0f)
		left = right = kMaxVoiceVolume / 2;
	if (left == kMaxVoiceVolume) {
		if (dx < -400.0)
			right = kMaxVoiceVolume * 3 / 4;
		if (dx > 400.0)
			left = kMaxVoiceVolume * 3 / 4;
	} else if (left == kMaxVoiceVolume) {
		if (dx < -1000.0)
			right = kMaxVoiceVolume / 4;
		if (dx > 1000.0)
			left = kMaxVoiceVolume / 4;
	}
	PlaySound(snd, priority, left, right);
	return true;
}

// FUNCTION: EVO 0x1005ec58
void CheckSpeechMgr(void)
{
	long response;
	short count;
	OSErr e;
	Boolean has;
	Boolean good;

	speechAvailable = false;
	has = Gestalt(gestaltVersion, &response) == noErr;
	if (has) {
		e = Gestalt(gestaltSpeechAttr, &response);
		good = e == noErr && BitTst(&response, 31 - gestaltSpeechMgrPresent);
#if GENERATINGPOWERPC
		if (good)
			good = BitTst(&response, 31 - gestaltSpeechHasPPCGlue);
#endif
		if (good) {
			CountVoices(&count);
			if (count > 0)
				speechAvailable = true;
		}
	}
}

static SpeechChannel	speechChannel;

// FUNCTION: EVO 0x1005ed48
void SpeakIt(char *text, short wait)
{
	long dead[4];
	if (speechAvailable) {
		if (speechChannel != nil) {
			DisposeSpeechChannel(speechChannel);
			speechChannel = nil;
		}
		c2pstr(text);
		SpeakString((StringPtr)text);
		if (wait) {
			while (SpeechBusy())
				;
		}
	}
}

RgnHandle menuBarRgn;
RgnHandle savedGrayRgn;

// FUNCTION: EVO 0x1005edd8
void HideTheMenuBar(void)
{
	RgnHandle temp, gray;
	GrafPtr savePort;

	if (!menuBarHidden) {
		gray = GetGrayRgn();
		temp = NewRgn();
		DiffRgn(menuBarRgn, gray, temp);
		savedGrayRgn = NewRgn();
		CopyRgn(gray, savedGrayRgn);
		UnionRgn(menuBarRgn, gray, gray);
		savedMBarHeight = LMGetMBarHeight();
		LMSetMBarHeight(0);
		menuBarHidden = true;
		GetPort(&savePort);
		CalcVisBehind(FrontWindow(), temp);
		PaintBehind(FrontWindow(), temp);
		SetPort(savePort);
		DisposeRgn(temp);
	}
}

// FUNCTION: EVO 0x1005eef4
void ShowTheMenuBar(void)
{
	RgnHandle gray, temp;
	GrafPtr savePort;

	if (menuBarHidden) {
		gray = GetGrayRgn();
		temp = NewRgn();
		DiffRgn(savedGrayRgn, gray, temp);
		CopyRgn(savedGrayRgn, gray);
		LMSetMBarHeight(savedMBarHeight);
		menuBarHidden = false;
		GetPort(&savePort);
		CalcVisBehind(FrontWindow(), temp);
		PaintBehind(FrontWindow(), temp);
		SetPort(savePort);
		DisposeRgn(temp);
		HiliteMenu(0);
		DrawMenuBar();
	}
}

// FUNCTION: EVO 0x1005eff0
void BuildMenuBarGrayRegion(void)
{
	GrafPtr savePort;
	GrafPtr wmgrPort;

	GetPort(&savePort);
	savedGrayRgn = nil;
	savedMBarHeight = LMGetMBarHeight();
	menuBarHidden = false;
	GetWMgrPort(&wmgrPort);
	SetPort(wmgrPort);
	menuBarRgn = NewRgn();
	OpenRgn();
	FrameRect(&wmgrPort->portRect);
	CloseRgn(menuBarRgn);
	UnionRgn(GetGrayRgn(), menuBarRgn, menuBarRgn);
	SetPort(savePort);
}

// FUNCTION: EVO 0x1005f0b8
Boolean CanBuyFighter(short shipType, short outfit, short bay)
{
	short max, count;
	short i, j;

	if (outfit == -1 && bay == -1) {
		for (i = 0; i < MaxWeaponTypes; i++) {
			if (shipData->weap[i].count > 0 && gWeapons[i].guidance == kGuidanceBay && gWeapons[i].ammoType == shipType + kAmmoShipBase) {
				bay = i;
				break;
			}
		}
		if (bay == -1)
			return false;
		for (i = 0; i < MaxOutfitItemTypes; i++) {
			for (j = 0; j < MaxOutfitMods; j++) {
				if (gOutfits[i].modType[j] == kOutfModAmmo && gOutfits[i].modVal[j] == bay) {
					outfit = i;
					break;
				}
			}
		}
		if (outfit == -1)
			return false;
		count = shipData->weap[bay].ammo;
	} else {
		if (outfit == -1 || bay == -1)
			return false;
		count = outfitCount[outfit];
	}
	max = gOutfits[outfit].maxCount;
	for (j = 1; j < MaxShipsInSystem; j++) {
		if (shipData[j].active && shipData[j].type == shipType && shipData[j].parentShip == 0 &&
				shipData[j].aiType == kAIFighter && shipData[j].specialMission == -1)
			count++;
	}
	if (count < max)
		return true;
	return false;
}

// FUNCTION: EVO 0x1005f33c
short TotalFleetHolds(void)
{
	short total;
	short i;

	total = ShipTotalHolds();
	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && !IsDying(&shipData[i]) && shipData->curSystem == shipData[i].curSystem &&
				shipData[i].parentShip == 0 && shipData[i].aiType == kAIEscort &&
				shipData[i].specialMission == -1 && gShipTypes[shipData[i].type].inherentAI <= kAIBraveTrader)
			total += gShipTypes[shipData[i].type].cargo;
	}
	return total;
}

// FUNCTION: EVO 0x1005f4a4
void DestroyPartialFleetCargo(short ship)
{
	long double have, total, used, ratio, drop;
	short i;

	if (ship == 0)
		have = ShipTotalHolds();
	else
		have = gShipTypes[shipData[ship].type].cargo;
	total = ShipTotalHolds();
	for (i = 1; i < MaxShipsInSystem; i++) {
		if (shipData[i].active && (!IsDying(&shipData[i]) || i == ship) &&
				shipData->curSystem == shipData[i].curSystem && shipData[i].parentShip == 0 &&
				shipData[i].aiType == kAIEscort && shipData[i].specialMission == -1 &&
				gShipTypes[shipData[i].type].inherentAI <= kAIBraveTrader)
			total += gShipTypes[shipData[i].type].cargo;
	}
	used = 0.0;
	for (i = 0; i <= 5; i++)
		used += shipData->cargo[i];
	for (i = 0; i < kMaxJunkTypes; i++)
		used += junkData[i].held;
	ratio = have / total;
	drop = ratio * used;
	for (i = 0; i <= 5; i++) {
		shipData->cargo[i] -= shipData->cargo[i] * ratio;
		if (shipData->cargo[i] < 0)
			shipData->cargo[i] = 0;
	}
	for (i = 0; i < kMaxJunkTypes; i++) {
		junkData[i].held -= junkData[i].held * ratio;
		if (junkData[i].held < 0)
			junkData[i].held = 0;
	}
	hudRedraw.credits = true;
}

// FUNCTION: EVO 0x1005f900
void QuickKeyCheckHandler(void)
{
	GetKeys((unsigned long *)keyMapBits);
}

// FUNCTION: EVO 0x1005f928
short QuickKeyCheck(short key)
{
	short row, col;

	row = (key >> 4) & 0xF;
	col = key & 0xF;
	return (1 << col) & keyMapBits[row];
}

// FUNCTION: EVO 0x1005f964
short KeyCheck(short key)
{
	unsigned short keys[8];

	short row, col;

	GetKeys((unsigned long *)keys);
	row = (key >> 4) & 0xF;
	col = key & 0xF;
	return (1 << col) & keys[row];
}

// FUNCTION: EVO 0x1005f9d0
short CalcPortDemand(short group, unsigned long flags)
{
	switch (group) {
	case 0:
		if (flags & kSpobFoodLow)
			return 1;
		if (flags & kSpobFoodMed)
			return 2;
		if (flags & kSpobFoodHigh)
			return 4;
		break;
	case 1:
		if (flags & kSpobIndustrialLow)
			return 1;
		if (flags & kSpobIndustrialMed)
			return 2;
		if (flags & kSpobIndustrialHigh)
			return 4;
		break;
	case 2:
		if (flags & kSpobMedicalLow)
			return 1;
		if (flags & kSpobMedicalMed)
			return 2;
		if (flags & kSpobMedicalHigh)
			return 4;
		break;
	case 3:
		if (flags & kSpobLuxuryLow)
			return 1;
		if (flags & kSpobLuxuryMed)
			return 2;
		if (flags & kSpobLuxuryHigh)
			return 4;
		break;
	case 4:
		if (flags & kSpobMetalLow)
			return 1;
		if (flags & kSpobMetalMed)
			return 2;
		if (flags & kSpobMetalHigh)
			return 4;
		break;
	case 5:
		if (flags & kSpobEquipmentLow)
			return 1;
		if (flags & kSpobEquipmentMed)
			return 2;
		if (flags & kSpobEquipmentHigh)
			return 4;
		break;
	}
	return 0;
}

// FUNCTION: EVO 0x1005fb30
void SetupPortAvailableShipTypes(SpobRecord *spob)
{
	Boolean ok;
	short n;
	short i;
	short j;

	for (i = 0; i < MaxShipClasses; i++)
		dockedPortShipTypes[i] = -1;
	n = 0;
	i = 0;
	while (i < MaxShipClasses) {
		ok = false;
		if (gShipTypes[i].techLevel <= spob->techLevel)
			ok = true;
		else {
			for (j = 0; j <= 2; j++) {
				if (spob->specialTech[j] == gShipTypes[i].techLevel) {
					ok = true;
					break;
				}
			}
		}
		if (gShipTypes[i].missionBit != -1) {
			if (gShipTypes[i].missionBit >= kNotBitBase) {
				if (missionBit[gShipTypes[i].missionBit - kNotBitBase])
					ok = false;
			} else if (!missionBit[gShipTypes[i].missionBit])
				ok = false;
			if (shipBuyMode != 0)
				ok = false;
		}
		if (showAll)
			ok = true;
		if (ok) {
			dockedPortShipTypes[n] = i;
			n++;
		}
		i++;
	}
}

// FUNCTION: EVO 0x1005fcd4
void SetupPortAvailableItems(SpobRecord *spob)
{
	Boolean ok;
	short n;
	short i;
	short j;

	for (i = 0; i < MaxOutfitItemTypes; i++)
		dockedPortItems[i] = -1;
	n = 0;
	i = 0;
	while (i < MaxOutfitItemTypes) {
		ok = false;
		if (gOutfits[i].techLevel <= spob->techLevel)
			ok = true;
		else {
			for (j = 0; j <= 2; j++) {
				if (spob->specialTech[j] == gOutfits[i].techLevel) {
					ok = true;
					break;
				}
			}
		}
		if (gOutfits[i].missionBit >= kNotBitBase) {
			if (missionBit[gOutfits[i].missionBit - kNotBitBase])
				ok = false;
		} else if (gOutfits[i].missionBit != -1 && !missionBit[gOutfits[i].missionBit])
			ok = false;
		if (showAll)
			ok = true;
		if (ok) {
			dockedPortItems[n] = i;
			n++;
		}
		i++;
	}
}

// FUNCTION: EVO 0x1005fe5c
short TotalMissionCargoSpace(void)
{
	short reserved, holds, adj, fleet, cargo, free;
	short i;

	holds = ShipTotalHolds();
	fleet = TotalFleetHolds();
	cargo = TotalCargo(shipData);
	reserved = 0;
	for (i = 0; i < MaxSimultaneousMissions; i++) {
		if (currentMissionObjectives[i].active && currentMissions[i].cargoPickedUp)
			reserved += currentMissions[i].cargoQty;
	}
	if (fleet > holds) {
		adj = cargo - reserved - (fleet - holds);
		if (adj < 0)
			adj = 0;
		adj += reserved;
		free = holds - adj;
	} else
		free = holds - cargo;
	return free;
}

// FUNCTION: EVO 0x1005ff4c
void RefreshSAT(void)
{
	Rect r;

	if (gSAT.wind.port == nil || gSAT.offScreen.port == nil || gSAT.backScreen.port == nil)
		return;
	r = gSAT.wind.bounds;
	r.right -= kStatusPanelWidth;
	SATSetPortScreen();
	CopyBits(&gSAT.offScreen.port->portBits, &gSAT.wind.port->portBits, &r, &r, srcCopy, nil);
}

// FUNCTION: EVO 0x1005ffe8
void TwoStepRepaintGameWindow(void)
{
	if (gSAT.wind.port == nil || gSAT.offScreen.port == nil || gSAT.backScreen.port == nil)
		return;
	CopyBits(&gSAT.backScreen.port->portBits, &gSAT.offScreen.port->portBits, &msgRect, &msgRect, srcCopy, nil);
	CopyBits(&gSAT.offScreen.port->portBits, &gSAT.wind.port->portBits, &msgRect, &msgRect, srcCopy, nil);
}

// FUNCTION: EVO 0x10060094
void RefreshSprite(SpritePtr sprite)
{
	long n;

	/* dead on both targets; the 68k frame and registers show it was code */
	if (0) {
		n = sprite->appLong;
		sprite->appLong = n;
		n = n + 1;
	}
}

// FUNCTION: EVO 0x10060098
void GraphicsEmergencyBail(void)
{
	ShowCursor();
	FadeScreenIn(4);
	EVNoteAlert("\pEV encountered an error while starting up, probably due to lack of memory. Please increase EVÕs memory allocation and try again.");
	ShowTheMenuBar();
	TeardownSoundSubsystem();
	CloseMonitorTool();
	ExitToShell();
}

// FUNCTION: EVO 0x100600ec
void HideControlStrip(void)
{
}

// FUNCTION: EVO 0x100600f0
void RestoreControlStrip(void)
{
}

// FUNCTION: EVO 0x100600f4
Boolean OldOsCheck(void)
{
	return true;
}

// FUNCTION: EVO 0x100600fc
void ShowOldOsWarningIfNeeded(void)
{
	Boolean done;
	short d0;
	short itemHit;
	Str255 dead;
	long d1[3];
	SysEnvRec env;

	done = false;
	if (!GENERATINGPOWERPC)
		return;
	SysEnvirons(2, &env);
	if (env.systemVersion >= 0x750) {
		if (!prefsChanged) {
			if (OldOsCheck())
				prefsChanged = true;
		} else if (!OldOsCheck()) {
			oldOsDialog = nil;
			oldOsDialog = GetNewDialog(kOldOsDlogID, nil, (WindowPtr)-1);
			if (oldOsDialog != nil) {
				SetPort(oldOsDialog);
				SetDialogDefaultItem(oldOsDialog, 1);
				ShowWindow(oldOsDialog);
				SetPort(oldOsDialog);
				SelectWindow(oldOsDialog);
				SetPort(oldOsDialog);
				DrawDialog(oldOsDialog);
				FrameDefaultButton(oldOsDialog, 1);
				FlushEvents(everyEvent - diskMask, 0);
				do {
					ModalDialog(nil, &itemHit);
					if (itemHit == kOkItem)
						done = true;
					if (itemHit == 2) {
						prefsChanged = false;
						done = true;
					}
				} while (!done);
				HideWindow(oldOsDialog);
				DisposeDialog(oldOsDialog);
			}
		}
	}
}

// FUNCTION: EVO 0x10060288
void PlaySound(Ptr snd, short priority, short left, short right)
{
	TriggerSoundPlay(snd, priority, (left + right) / 2);
}

// FUNCTION: EVO 0x100602d8
Boolean PlayMovieById(short id, Boolean after)
{
	Boolean result;
	short i;

	result = true;
	for (i = 0; i < MaxMovies; i++) {
		if (id == movieData[i].movieId) {
			if (!after || (movieData[i].flags & kMovieInstead)) {
				if (missionDialog != nil) {
					SetPort(missionDialog);
					InvalRect(&missionDialog->portRect);
					if (drawGate || barDialog != nil)
						SingleMissionDialogUpdate();
					else
						MissionDialogUpdate();
				}
				if (portDialog != nil) {
					SetPort(portDialog);
					InvalRect(&portDialog->portRect);
					PortDialogUpdate();
				}
				if (barDialog != nil) {
					SetPort(barDialog);
					InvalRect(&barDialog->portRect);
					BarDialogUpdate();
				}
			}
			if (after) {
				if ((movieData[i].flags & kMovieInstead) || !(movieData[i].flags & kMovieAfter)) {
					PlayQTMovie(movieData[i].name, 0, 0);
					if (movieData[i].flags & kMovieInstead)
						result = false;
				}
			}
			if (!after && (movieData[i].flags & kMovieAfter)) {
				PlayQTMovie(movieData[i].name, 0, 0);
				if (movieData[i].flags & kMovieInstead)
					result = false;
			}
		}
	}
	return result;
}

// FUNCTION: EVO 0x10060504
void PlayQTMovie(StringPtr name, Boolean noFrame, Boolean unused)
{
	Movie movie;
	long dead2;
	FSSpec spec;
	short fileRef;
	short resId;
	short dead;
	short vRef;
	Rect box;
	long dirID;
	WindowPtr win;
	OSErr e;

	if (!settings.noMovies) {
		e = EnterMovies();
		if (e == noErr) {
			e = GetCatalogStartDir(appResFile, "\pEV Plug-Ins", &dirID, &vRef);
			if (e != noErr) {
				ExitMovies();
				return;
			}
			FSMakeFSSpec(vRef, dirID, name, &spec);
			e = OpenMovieFile(&spec, &fileRef, fsRdPerm);
			if (e != noErr) {
				ExitMovies();
				return;
			}
			resId = 0;
			NewMovieFromFile(&movie, fileRef, &resId, nil, newMovieActive, (Boolean *)0);
			/* BUG: the result is not stored, e still holds the OpenMovieFile result */
			if (e != noErr) {
				CloseMovieFile(fileRef);
				ExitMovies();
				return;
			}
			CloseMovieFile(fileRef);
			GetMovieBox(movie, &box);
			OffsetRect(&box, -box.left, -box.top);
			OffsetRect(&box, (gSAT.wind.bounds.right - gSAT.wind.bounds.left) / 2 - (box.right - box.left) / 2,
					(gSAT.wind.bounds.bottom - gSAT.wind.bounds.top) / 2 - (box.bottom - box.top) / 2);
			SetMovieBox(movie, &box);
			InsetRect(&box, -1, -1);
			win = NewCWindow(nil, &box, "\p", false, plainDBox, nil, true, -1);
			InsetRect(&box, 1, 1);
			if (!noFrame) {
				NewDialogHook(win, 0);
				CenterWindow(win);
			}
			ShowWindow(win);
			SelectWindow(win);
			SetPort(win);
			ForeColor(blackColor);
			PaintRect(&box);
			ForeColor(whiteColor);
			FrameRect(&box);
			ForeColor(blackColor);
			SetMovieGWorld(movie, (CGrafPtr)win, nil);
			OffsetRect(&box, -box.left, -box.top);
			OffsetRect(&box, 1, 1);
			SetMovieBox(movie, &box);
			GoToBeginningOfMovie(movie);
			SetMovieRate(movie, kFixedOne);
			StartMovie(movie);
			while (!IsMovieDone(movie)) {
				MoviesTask(movie, 0);
				if (Button()) {
					/* BUG: event kinds passed as masks; only the low event bits are flushed */
					FlushEvents(3, 0);
					break;
				}
			}
			CloseWindow(win);
			DisposeMovie(movie);
			ExitMovies();
			SATSetPortScreen();
			ForeColor(blackColor);
		}
	}
}

// FUNCTION: EVO 0x100608a4
short CategorizeWeapon(short weap)
{
	short cat;

	if (gWeapons[weap].guidance != kGuidanceHoming)
		return 0;
	cat = 1;
	if (gWeapons[weap].seeker & kSeekTurn30)
		cat += 2;
	if (gWeapons[weap].seeker & kSeekTurn60)
		cat += 4;
	return cat;
}

// FUNCTION: EVO 0x10060928
void AdjustWarpSoundSpeed(double scale)
{
	alarmRequest.pitch = FixRatio(1000.0 * scale, 1000);
}

// FUNCTION: EVO 0x10060974
short ShipAntiInterference(void)
{
	short total;
	short i, j;

	total = 0;
	for (i = 0; i < MaxOutfitItemTypes; i++) {
		for (j = 0; j < MaxOutfitMods; j++) {
			if (gOutfits[i].modType[j] == kOutfModInterference)
				total += gOutfits[i].modVal[j] * outfitCount[i];
		}
	}
	return total;
}

// FUNCTION: EVO 0x10060a1c
void SpawnNagShip(void)
{
	short i, j, slot;
	float dist, speed;

	if (personData[kPersAmbrosiaMascot].shipType >= 0) {
		for (i = 1; i < MaxShipsInSystem; i++) {
			if (shipData[i].active && shipData[i].persType == kPersAmbrosiaMascot)
				return;
		}
		if (gDaysUsed <= 14)
			j = 10000;
		else if (gDaysUsed <= 30)
			j = 700;
		else if (gDaysUsed <= 60)
			j = 4000;
		else
			j = 2000;
		if (playerKills >= 400)
			j *= 0.5;
		if (Rand(j * 3) == 0) {
			slot = GenericRandomShipSpawn(shipData->curSystem, 2);
			if (slot != -1) {
				shipData[slot].persType = kPersAmbrosiaMascot;
				shipData[slot].type = personData[kPersAmbrosiaMascot].shipType;
				shipData[slot].aiType = kAIInterceptor;
				dist = 0.0;
				speed = 50;
				while (speed > 0.0f) {
					dist += speed;
					speed -= 1.165;
				}
				shipData[slot].pos.x = shipData->pos.x;
				shipData[slot].pos.y = shipData->pos.y;
				Accel(shipData->heading, -(500.0 + dist), &shipData[slot].pos);
				shipData[slot].heading = shipData->heading;
				shipData[slot].dude = -1;
				shipData[slot].aggression = 4;
				shipData[slot].govt = -1;
				for (j = 0; j < MaxWeaponTypes; j++) {
					shipData[slot].weap[j].count = gShipTypes[shipData[slot].type].weapCount[j];
					shipData[slot].weap[j].ammo = gShipTypes[shipData[slot].type].weapAmmo[j];
					shipData[slot].weap[j].count += personData[kPersAmbrosiaMascot].weapCount[j];
					shipData[slot].weap[j].ammo += personData[kPersAmbrosiaMascot].weapAmmo[j];
					if (gDaysUsed <= 37)
						shipData[slot].weap[j].ammo = 0;
				}
				shipData[slot].shield = personData[kPersAmbrosiaMascot].shieldMod * gShipTypes[shipData[slot].type].shield;
				AISetup(&shipData[slot]);
				AIMakeShipJumpIn(&shipData[slot]);
				shipData[slot].lastInspected = -1;
				if (gDaysUsed > 60)
					shipData[slot].shield *= 10.0;
			}
		}
	}
}

// FUNCTION: EVO 0x10060f28
FacePtr FaceFromCIcon(short id)
{
	CIconHandle icon;
	FacePtr face;
	Rect r;

	icon = GetCIcon(id);
	if (icon != nil) {
		HLock((Handle)icon);
		r = (*icon)->iconMask.bounds;
		OffsetRect(&r, -r.left, -r.top);
		face = SATNewFace(&r);
		SATSetPortFace(face);
		ForeColor(blackColor);
		PlotCIcon(&r, icon);
		SATSetPortMask(face);
		ForeColor(blackColor);
		CopyBits(&(*icon)->iconMask, &qd.thePort->portBits, &(*icon)->iconMask.bounds, &r, srcCopy, nil);
		SATSetPortScreen();
		SATChangedFace(face);
		HUnlock((Handle)icon);
		DisposeCIcon(icon);
	} else {
		face = nil;
	}
	return face;
}

// FUNCTION: EVO 0x10061064
short AngularDifference(short a, short b)
{
	short d;

	d = abs(a - b);
	if ((a >= 180) != (b >= 180))
		d = 360 - d;
	return d;
}

// FUNCTION: EVO 0x100610c0
Boolean AIHasAfterburner(ShipObj *ship)
{
	if (gShipTypes[ship->type].flags & kShipAfterburner)
		return true;
	if ((gShipTypes[ship->type].flags & kShipAfterburnerRated) && playerKills >= Rand(1344) + 256)
		return true;
	return false;
}

// FUNCTION: EVO 0x1006115c
Boolean AIPerformsComplexManeuvers(void)
{
	if (playerKills >= Rand(1344) + 256)
		return true;
	return false;
}

// FUNCTION: EVO 0x100611a4
Boolean TurretBlindSpot(ShipObj *ship, ShipObj *target, short weap)
{
	short ang;
	Boolean ok;

	ok = false;
	ang = AngularDifference(ship->heading, CalcInterceptAngle(ship->pos, target->pos)) % 180;
	if (ang <= 45) {
		if (gWeapons[weap].flags & kWeapBlindFront)
			ok = true;
		if (gShipTypes[ship->type].flags & kShipTurretBlindFront)
			ok = true;
	} else if (ang <= 135) {
		if (gWeapons[weap].flags & kWeapBlindSide)
			ok = true;
		if (gShipTypes[ship->type].flags & kShipTurretBlindSide)
			ok = true;
	} else {
		if (gWeapons[weap].flags & kWeapBlindRear)
			ok = true;
		if (gShipTypes[ship->type].flags & kShipTurretBlindRear)
			ok = true;
	}
	return ok;
}

// FUNCTION: EVO 0x100612fc
short WeaponShotSpread(short ship, short weap)
{
	short spread;

	spread = gShipTypes[shipData[ship].type].shotXOffset;
	if (gWeapons[weap].flags & kWeapNoXOffset)
		spread = 0;
	if (spread > 0 && shipData[ship].weap[weap].count > 1) {
		if (shipData[ship].altFireSide < 0)
			return spread;
		return -spread;
	}
	return 0;
}

// FUNCTION: EVO 0x100613c0
Boolean SuitableMissileType(short weap, ShipObj *ship)
{
	short turn;

	turn = ShipTurnRate(ship);
	if ((gWeapons[weap].flags & kWeapNoFastTargets) && turn > 3)
		return false;
	if (CategorizeWeapon(weap) <= 2 && turn > 3)
		return false;
	return true;
}

// FUNCTION: EVO 0x10061460
short SelectShipFieldFromDude(DudeRecord *dude)
{
	short avail[4];
	Boolean ok[4];
	short total, pick, i, j, r;

	total = 0;
	pick = -1;
	for (i = 0; i <= 3; i++) {
		avail[i] = 0;
		if (dude->missionBit[i] == -1)
			ok[i] = true;
		else {
			ok[i] = false;
			if (dude->missionBit[i] >= 0 && dude->missionBit[i] < NumMissionBits) {
				if (missionBit[dude->missionBit[i]])
					ok[i] = true;
			} else if (dude->missionBit[i] >= kNotBitBase && dude->missionBit[i] < kNotBitBase + NumMissionBits && !missionBit[dude->missionBit[i] - kNotBitBase])
				ok[i] = true;
		}
		if (ok[i]) {
			total += dude->shipProbs[i];
			for (j = 0; j <= i; j++) {
				if (ok[j])
					avail[i] += dude->shipProbs[j];
			}
		}
	}
	if (total <= 0)
		return -1;
	while (pick == -1) {
		r = Rand(total) + 1;
		for (i = 3; i >= 0; i--) {
			if (r <= avail[i] && ok[i] && dude->shipTypes[i] >= 0)
				pick = i;
		}
	}
	return pick;
}

// FUNCTION: EVO 0x100616c8
float RandomSkillLevel(short type)
{
	return 0.01 * (100 - gShipTypes[type].skillVar + Rand(gShipTypes[type].skillVar * 2 + 1));
}

// FUNCTION: EVO 0x10061760
Boolean AlliedWithPlayer(ShipObj *ship)
{
	if (ship->index == 0)
		return true;
	if (ship->parentShip == 0)
		return true;
	if (ship->parentShip != -1 && shipData[ship->parentShip].parentShip == 0)
		return true;
	return false;
}
