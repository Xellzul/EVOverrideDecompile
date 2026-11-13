/* MonitorTool.c -- the display library: find a monitor of a depth, save/restore/fade its colour table. Notes: Research/notes/MonitorTool.md */
// COMPILER: CW7
// COMPILE: -opt l4,schedule604
// COMPILE-WHY: the separately built library part is prebuilt 1996 code (CodeWarrior MWCPPC 1.3/1.4, the MPW tool): Research/notes/library-compiler.md
#include <Quickdraw.h>
#include <QDOffscreen.h>
#include <Palettes.h>
#include <Windows.h>
#include <Menus.h>
#include <Memory.h>
#include <Gestalt.h>
#include <FixMath.h>
#include <ToolUtils.h>
#include <ColorPicker.h>

#include "monitortool.h"

#pragma options align=mac68k
typedef struct MonitorRec {
	Boolean		canSetDepth;
	GDHandle	device;
	short		origDepth;
	CTabHandle	origClut;
	long		origSeed;
	CTabHandle	workClut;
	long		workSeed;
	short		depth;
	Boolean		faded;
	RGBColor	fadeColor;
	long		fill22;
	long		fill26;
	long		fill2A;
	long		fill2E;
} MonitorRec;
#pragma options align=reset

static Boolean		monitorOpen = false;
static MonitorRec	*monitor;

GDHandle	SwitchDepths(short depth, short *origDepth, short width, short height);
GDHandle	TryMainMonitor(short depth, short *origDepth, short width, short height);
void		FadeOut(long steps, RGBColor *c);
void		FadeIn(long steps);
void		FadeToColor(short steps, RGBColor *c);
void		FadeToClut(short steps, CTabHandle target);
void		RestoreClut(Boolean setEntries);
void		RestoreOriginalClut(Boolean setEntries);

// FUNCTION: EVO 0x1006F6D4
OSErr OpenMonitorTool(short depth, short width, short height, Boolean tryMain, GDHandle *dev)
{
	OSErr err;
	SysEnvRec env;
	long size;
	short envErr;
	CTabHandle clut;

	err = 0;
	if (depth > 8)
		return -5;
	if (monitorOpen) {
		err = -2;
	} else {
		envErr = SysEnvirons(1, &env);
		if (envErr != noErr || env.hasColorQD != true) {
			err = -4;
		} else {
			monitor = (MonitorRec *)NewPtr(sizeof(MonitorRec));
			if (monitor == nil) {
				err = -1;
			} else {
				*dev = nil;
				monitor->canSetDepth = DepthAvail();
				monitor->origDepth = 0;
				monitor->faded = 0;
				monitor->fadeColor.red = 0;
				monitor->fadeColor.green = 0;
				monitor->fadeColor.blue = 0;
				if (*dev == nil && tryMain && monitor->canSetDepth)
					*dev = TryMainMonitor(depth, &monitor->origDepth, width, height);
				if (*dev == nil)
					*dev = FindMonitor(depth, width, height);
				if (*dev == nil && tryMain && monitor->canSetDepth)
					*dev = SwitchDepths(depth, &monitor->origDepth, width, height);
				if (*dev == nil) {
					DisposePtr((Ptr)monitor);
					err = -3;
				} else {
					UnprotectMonitor(*dev);
					clut = (*(**dev)->gdPMap)->pmTable;
					size = GetHandleSize((Handle)clut);
					monitor->device = *dev;
					monitor->origClut = (CTabHandle)NewHandle(size);
					if (monitor->origClut == nil) {
						DisposePtr((Ptr)monitor);
						err = -1;
					} else {
						monitorOpen = true;
						SaveCurrentClut(monitor->origClut, &monitor->origSeed);
						monitorOpen = false;
						monitor->workClut = (CTabHandle)NewHandle(size);
						if (monitor->workClut == nil) {
							DisposeHandle((Handle)monitor->origClut);
							DisposePtr((Ptr)monitor);
							err = -1;
						} else {
							monitor->depth = (*(**dev)->gdPMap)->pixelSize;
							monitorOpen = true;
							SaveCurrentClut(monitor->workClut, &monitor->workSeed);
							monitor->fill22 = 0;
							monitor->fill26 = 0;
							monitor->fill2A = 0;
						}
					}
				}
			}
		}
	}
	return err;
}

// FUNCTION: EVO 0x1006F99C
void CloseMonitorTool(void)
{
	short err;

	if (monitorOpen) {
		RestoreOriginalClut(1);
		if (monitor->canSetDepth && monitor->origDepth != 0)
			err = SetDepth(monitor->device, monitor->origDepth, 0, 0);
		ForceScreenUpdate();
		DisposeHandle((Handle)monitor->origClut);
		DisposeHandle((Handle)monitor->workClut);
		DisposePtr((Ptr)monitor);
	}
}

// FUNCTION: EVO 0x1006FA34
void SwitchedOut(void)
{
	if (monitorOpen) {
		RestoreOriginalClut(1);
		ForceMonitorUpdate();
	}
}

// FUNCTION: EVO 0x1006FA70
Boolean SwitchedIn(void)
{
	Boolean bad;

	bad = false;
	if (monitorOpen) {
		bad = ColorCheck();
		RestoreClut(1);
		ForceMonitorUpdate();
	}
	return bad;
}

// FUNCTION: EVO 0x1006FAC8
Boolean ColorCheck(void)
{
	Boolean bad;
	GDHandle dev;
	int err;

	bad = false;
	if (monitorOpen) {
		dev = monitor->device;
		if ((*(*dev)->gdPMap)->pixelSize != monitor->depth
				|| (*(*(*dev)->gdPMap)->pmTable)->ctSeed != monitor->workSeed) {
			if ((*(*dev)->gdPMap)->pixelSize != monitor->depth) {
				if (monitor->canSetDepth) {
					err = SetDepth(monitor->device, monitor->depth, 0, 0);
					if (err != noErr)
						bad = true;
				} else
					bad = true;
			}
			if (!bad)
				RestoreClut(1);
		}
	}
	return bad;
}

// FUNCTION: EVO 0x1006FBAC
void FadeOut(long steps, RGBColor *c)
{
	FractColor *cur;
	FractColor target;
	FractColor *delta;
	long seed;
	CTabHandle clut;
	short last;
	short i;
	char state;
	GDHandle saved;
	GDHandle dev;
	long d;

	if (monitorOpen && !monitor->faded) {
		dev = monitor->device;
		clut = (*(*dev)->gdPMap)->pmTable;
		seed = (*clut)->ctSeed;
		last = (*clut)->ctSize;
		cur = CTabToFractClut(clut);
		if (cur != nil) {
			delta = (FractColor *)NewPtr(kMaxClutEntries * sizeof(FractColor));
			if (delta != nil) {
				target.red = (long)c->red << 14;
				target.green = (long)c->green << 14;
				target.blue = (long)c->blue << 14;
				for (i = 0; i <= last; i++) {
					d = (unsigned long)cur[i].red >> 2;
					d -= target.red;
					d /= steps;
					d <<= 2;
					delta[i].red = d;
					d = (unsigned long)cur[i].green >> 2;
					d -= target.green;
					d /= steps;
					d <<= 2;
					delta[i].green = d;
					d = (unsigned long)cur[i].blue >> 2;
					d -= target.blue;
					d /= steps;
					d <<= 2;
					delta[i].blue = d;
				}
				saved = GetGDevice();
				SetGDevice(dev);
				state = HGetState((Handle)clut);
				HNoPurge((Handle)clut);
				HLock((Handle)clut);
				for (i = 0; i < steps; i++) {
					SubtractFractCluts(cur, delta, last);
					FractClutToCTab(cur, clut);
					SetEntries(0, last, (*clut)->ctTable);
				}
				HSetState((Handle)clut, state);
				(*clut)->ctSeed = seed;
				for (i = 0; i <= last; i++) {
					(*clut)->ctTable[i].rgb.red = (*monitor->workClut)->ctTable[i].rgb.red;
					(*clut)->ctTable[i].rgb.green = (*monitor->workClut)->ctTable[i].rgb.green;
					(*clut)->ctTable[i].rgb.blue = (*monitor->workClut)->ctTable[i].rgb.blue;
				}
				monitor->faded = true;
				monitor->fadeColor = *c;
				SetGDevice(saved);
				DisposePtr((Ptr)delta);
			}
			DisposePtr((Ptr)cur);
		}
	}
}

// FUNCTION: EVO 0x1006FE50
void FadeIn(long steps)
{
	CTabHandle work;
	FractColor *cur;
	FractColor *delta;
	FractColor *target;
	long seed;
	CTabHandle clut;
	short last;
	short i;
	GDHandle dev;
	char state;
	GDHandle saved;
	long d;

	if (monitorOpen && monitor->faded) {
		work = monitor->workClut;
		dev = monitor->device;
		clut = (*(*dev)->gdPMap)->pmTable;
		seed = (*clut)->ctSeed;
		last = (*clut)->ctSize;
		for (i = 0; i <= last; i++) {
			(*clut)->ctTable[i].rgb.red = monitor->fadeColor.red;
			(*clut)->ctTable[i].rgb.green = monitor->fadeColor.green;
			(*clut)->ctTable[i].rgb.blue = monitor->fadeColor.blue;
		}
		target = CTabToFractClut(work);
		if (target != nil) {
			cur = CTabToFractClut(clut);
			if (cur != nil) {
				delta = (FractColor *)NewPtr(kMaxClutEntries * sizeof(FractColor));
				if (delta != nil) {
					for (i = 0; i <= last; i++) {
						d = (unsigned long)cur[i].red >> 2;
						d -= (unsigned long)target[i].red >> 2;
						d /= steps;
						d <<= 2;
						delta[i].red = d;
						d = (unsigned long)cur[i].green >> 2;
						d -= (unsigned long)target[i].green >> 2;
						d /= steps;
						d <<= 2;
						delta[i].green = d;
						d = (unsigned long)cur[i].blue >> 2;
						d -= (unsigned long)target[i].blue >> 2;
						d /= steps;
						d <<= 2;
						delta[i].blue = d;
					}
					saved = GetGDevice();
					SetGDevice(dev);
					state = HGetState((Handle)clut);
					HNoPurge((Handle)clut);
					HLock((Handle)clut);
					for (i = 0; i < steps; i++) {
						SubtractFractCluts(cur, delta, last);
						FractClutToCTab(cur, clut);
						SetEntries(0, last, (*clut)->ctTable);
					}
					HSetState((Handle)clut, state);
					(*clut)->ctSeed = seed;
					monitor->faded = false;
					SetGDevice(saved);
					DisposePtr((Ptr)delta);
				}
				DisposePtr((Ptr)cur);
			}
			DisposePtr((Ptr)target);
		}
	}
}

// FUNCTION: EVO 0x100700D0
void SetMonitorClut(CTabHandle ctab, Boolean setEntries)
{
	CTabHandle clut;
	GDHandle dev;
	short i;
	char state;
	short last;
	GDHandle saved;
	long size;

	if (monitorOpen) {
		dev = monitor->device;
		clut = (*(*dev)->gdPMap)->pmTable;
		saved = GetGDevice();
		SetGDevice(dev);
		state = HGetState((Handle)clut);
		HNoPurge((Handle)clut);
		HLock((Handle)clut);
		size = GetHandleSize((Handle)clut);
		last = (*clut)->ctSize;
		for (i = 0; i <= last; i++) {
			(*clut)->ctTable[i].rgb.red = (*ctab)->ctTable[i].rgb.red;
			(*clut)->ctTable[i].rgb.green = (*ctab)->ctTable[i].rgb.green;
			(*clut)->ctTable[i].rgb.blue = (*ctab)->ctTable[i].rgb.blue;
		}
		if (setEntries)
			SetEntries(0, last, (*clut)->ctTable);
		MakeITable(nil, nil, 0);
		(*clut)->ctSeed = GetCTSeed();
		SaveCurrentClut(monitor->workClut, &monitor->workSeed);
		HSetState((Handle)clut, state);
		SetGDevice(saved);
	}
}

// FUNCTION: EVO 0x10070244
void FadeToColor(short steps, RGBColor *c)
{
	FractColor *cur;
	FractColor *delta;
	CTabHandle clut;
	FractColor target;
	short last;
	short i;
	GDHandle dev;
	char state;
	GDHandle saved;
	long d;
	long seed;

	if (monitorOpen) {
		dev = monitor->device;
		clut = (*(*dev)->gdPMap)->pmTable;
		seed = (*clut)->ctSeed;
		last = (*clut)->ctSize;
		cur = CTabToFractClut(clut);
		if (cur != nil) {
			delta = (FractColor *)NewPtr(kMaxClutEntries * sizeof(FractColor));
			if (delta != nil) {
				target.red = (long)c->red << 14;
				target.green = (long)c->green << 14;
				target.blue = (long)c->blue << 14;
				for (i = 0; i <= last; i++) {
					d = (unsigned long)cur[i].red >> 2;
					d -= target.red;
					d /= steps;
					d <<= 2;
					delta[i].red = d;
					d = (unsigned long)cur[i].green >> 2;
					d -= target.green;
					d /= steps;
					d <<= 2;
					delta[i].green = d;
					d = (unsigned long)cur[i].blue >> 2;
					d -= target.blue;
					d /= steps;
					d <<= 2;
					delta[i].blue = d;
				}
				saved = GetGDevice();
				SetGDevice(dev);
				state = HGetState((Handle)clut);
				HNoPurge((Handle)clut);
				HLock((Handle)clut);
				for (i = 0; i < steps; i++) {
					SubtractFractCluts(cur, delta, last);
					FractClutToCTab(cur, clut);
					SetEntries(0, last, (*clut)->ctTable);
				}
				HSetState((Handle)clut, state);
				MakeITable(nil, nil, 0);
				(*clut)->ctSeed = GetCTSeed();
				SaveCurrentClut(monitor->workClut, &monitor->workSeed);
				SetGDevice(saved);
				DisposePtr((Ptr)delta);
			}
			DisposePtr((Ptr)cur);
		}
	}
}

// FUNCTION: EVO 0x10070478
void FadeToClut(short steps, CTabHandle targetClut)
{
	FractColor *cur;
	FractColor *delta;
	FractColor *target;
	CTabHandle clut;
	short last;
	long d;
	short i;
	GDHandle dev;
	char state;
	GDHandle saved;
	long seed;

	if (monitorOpen) {
		dev = monitor->device;
		clut = (*(*dev)->gdPMap)->pmTable;
		seed = (*clut)->ctSeed;
		last = (*clut)->ctSize;
		target = CTabToFractClut(targetClut);
		if (target != nil) {
			cur = CTabToFractClut(clut);
			if (cur != nil) {
				delta = (FractColor *)NewPtr(kMaxClutEntries * sizeof(FractColor));
				if (delta != nil) {
					for (i = 0; i <= last; i++) {
						d = (unsigned long)cur[i].red >> 2;
						d -= (unsigned long)target[i].red >> 2;
						d /= steps;
						d <<= 2;
						delta[i].red = d;
						d = (unsigned long)cur[i].green >> 2;
						d -= (unsigned long)target[i].green >> 2;
						d /= steps;
						d <<= 2;
						delta[i].green = d;
						d = (unsigned long)cur[i].blue >> 2;
						d -= (unsigned long)target[i].blue >> 2;
						d /= steps;
						d <<= 2;
						delta[i].blue = d;
					}
					saved = GetGDevice();
					SetGDevice(dev);
					state = HGetState((Handle)clut);
					HNoPurge((Handle)clut);
					HLock((Handle)clut);
					for (i = 0; i < steps; i++) {
						SubtractFractCluts(cur, delta, last);
						FractClutToCTab(cur, clut);
						SetEntries(0, last, (*clut)->ctTable);
					}
					HSetState((Handle)clut, state);
					MakeITable(nil, nil, 0);
					(*clut)->ctSeed = GetCTSeed();
					SaveCurrentClut(monitor->workClut, &monitor->workSeed);
					SetGDevice(saved);
					DisposePtr((Ptr)delta);
				}
				DisposePtr((Ptr)cur);
			}
			DisposePtr((Ptr)target);
		}
	}
}
// FUNCTION: EVO 0x100706B4
void HueScale(short which, Boolean mode)
{
	RGBColor c;
	GDHandle dev;

	if (monitorOpen) {
		dev = monitor->device;
		switch (which) {
		case 1:
			c.red = 0xFFFF;
			c.green = 0;
			c.blue = 0;
			break;
		case 2:
			c.red = 0;
			c.green = 0xFFFF;
			c.blue = 0;
			break;
		case 3:
			c.red = 0;
			c.green = 0;
			c.blue = 0xFFFF;
			break;
		case 4:
			c.red = 0;
			c.green = 0xFFFF;
			c.blue = 0xFFFF;
			break;
		case 5:
			c.red = 0xFFFF;
			c.green = 0;
			c.blue = 0xFFFF;
			break;
		case 6:
			c.red = 0xFFFF;
			c.green = 0xFFFF;
			c.blue = 0;
			break;
		case 7:
			c.red = 0x7FFF;
			c.green = 0x7FFF;
			c.blue = 0x7FFF;
			break;
		default:
			return;
		}
		MapToHue(&c, mode);
	}
}

// FUNCTION: EVO 0x100707E0
void SetClutToColor(RGBColor *c, Boolean setEntries)
{
	GDHandle saved;
	char state;
	short last;
	GDHandle dev;
	CTabHandle clut;
	short i;
	long seed;

	if (monitorOpen) {
		dev = monitor->device;
		clut = (*(*dev)->gdPMap)->pmTable;
		seed = (*clut)->ctSeed;
		last = (*clut)->ctSize;
		for (i = 0; i <= last; i++) {
			(*clut)->ctTable[i].rgb.red = c->red;
			(*clut)->ctTable[i].rgb.green = c->green;
			(*clut)->ctTable[i].rgb.blue = c->blue;
		}
		saved = GetGDevice();
		SetGDevice(dev);
		state = HGetState((Handle)clut);
		HNoPurge((Handle)clut);
		HLock((Handle)clut);
		if (setEntries)
			SetEntries(0, last, (*clut)->ctTable);
		HSetState((Handle)clut, state);
		MakeITable(nil, nil, 0);
		(*clut)->ctSeed = GetCTSeed();
		SaveCurrentClut(monitor->workClut, &monitor->workSeed);
		SetGDevice(saved);
	}
}

// FUNCTION: EVO 0x1007093C
void MapToHue(RGBColor *c, Boolean setEntries)
{
	FractColor *fr;
	char state;
	CTabHandle clut;
	GDHandle saved;
	short last;
	short i;
	GDHandle dev;
	Fixed ratio;
	HSLColor target;
	RGBColor rgb;
	HSLColor entry;
	long seed;
	Fixed val;

	if (monitorOpen) {
		dev = monitor->device;
		clut = (*(*dev)->gdPMap)->pmTable;
		seed = (*clut)->ctSeed;
		last = (*clut)->ctSize;
		fr = (FractColor *)NewPtr(kMaxClutEntries * sizeof(FractColor));
		if (fr != nil) {
			RGB2HSL(c, &target);
			for (i = 0; i <= last; i++) {
				rgb = (*clut)->ctTable[i].rgb;
				RGB2HSL(&rgb, &entry);
				ratio = FixRatio(entry.lightness, target.lightness);
				val = Long2Fix(c->red);
				val = FixMul(ratio, val);
				fr[i].red = val;
				val = Long2Fix(c->green);
				val = FixMul(ratio, val);
				fr[i].green = val;
				val = Long2Fix(c->blue);
				val = FixMul(ratio, val);
				fr[i].blue = val;
			}
			saved = GetGDevice();
			SetGDevice(dev);
			state = HGetState((Handle)clut);
			HNoPurge((Handle)clut);
			HLock((Handle)clut);
			FractClutToCTab(fr, clut);
			if (setEntries)
				SetEntries(0, last, (*clut)->ctTable);
			HSetState((Handle)clut, state);
			MakeITable(nil, nil, 0);
			(*clut)->ctSeed = GetCTSeed();
			SaveCurrentClut(monitor->workClut, &monitor->workSeed);
			SetGDevice(saved);
			DisposePtr((Ptr)fr);
		}
	}
}

// FUNCTION: EVO 0x10070B5C
GDHandle FindMonitor(short depth, short width, short height)
{
	GDHandle dev;
	Boolean done;

	done = false;
	dev = GetMainDevice();
	if ((*(*dev)->gdPMap)->pixelSize == depth
			&& (*dev)->gdRect.right - (*dev)->gdRect.left >= width
			&& (*dev)->gdRect.bottom - (*dev)->gdRect.top >= height)
		done = true;
	else {
		dev = GetDeviceList();
		while (!done) {
			if (dev == nil)
				done = true;
			else if ((*(*dev)->gdPMap)->pixelSize == depth
					&& TestDeviceAttribute(dev, screenDevice)
					&& TestDeviceAttribute(dev, screenActive)) {
				if ((*dev)->gdRect.right - (*dev)->gdRect.left >= width
						&& (*dev)->gdRect.bottom - (*dev)->gdRect.top >= height) {
					done = true;
					return dev;
				}
				dev = (GDHandle)(*dev)->gdNextGD;
			} else
				dev = (GDHandle)(*dev)->gdNextGD;
		}
	}
	return dev;
}

// FUNCTION: EVO 0x10070CBC
GDHandle SwitchDepths(short depth, short *origDepth, short width, short height)
{
	GDHandle dev;
	Boolean done;
	short err;

	done = false;
	dev = GetMainDevice();
	err = HasDepth(dev, depth, 0, 0);
	if (err && (*dev)->gdRect.right - (*dev)->gdRect.left >= width
			&& (*dev)->gdRect.bottom - (*dev)->gdRect.top >= height)
		done = true;
	else {
		dev = GetDeviceList();
		while (!done) {
			if (dev == nil)
				done = true;
			else {
				err = HasDepth(dev, depth, 0, 0);
				if (err && TestDeviceAttribute(dev, screenDevice)
						&& TestDeviceAttribute(dev, screenActive)) {
					if ((*dev)->gdRect.right - (*dev)->gdRect.left >= width
							&& (*dev)->gdRect.bottom - (*dev)->gdRect.top >= height)
						done = true;
					else
						dev = (GDHandle)(*dev)->gdNextGD;
				} else
					dev = (GDHandle)(*dev)->gdNextGD;
			}
		}
	}
	if (done && dev != nil) {
		*origDepth = (*(*dev)->gdPMap)->pixelSize;
		err = SetDepth(dev, depth, 0, 0);
		if (err != noErr)
			dev = nil;
	}
	return dev;
}

// FUNCTION: EVO 0x10070E6C
GDHandle TryMainMonitor(short depth, short *origDepth, short width, short height)
{
	GDHandle dev;
	Boolean done;
	short has;

	done = false;
	dev = GetMainDevice();
	if ((*dev)->gdRect.right - (*dev)->gdRect.left >= width
			&& (*dev)->gdRect.bottom - (*dev)->gdRect.top >= height) {
		if ((*(*dev)->gdPMap)->pixelSize != depth) {
			has = HasDepth(dev, depth, 0, 0);
			if (dev != nil && has) {
				*origDepth = (*(*dev)->gdPMap)->pixelSize;
				has = SetDepth(dev, depth, 0, 0);
				if (has != noErr)
					dev = nil;
			} else
				dev = nil;
		}
	} else
		dev = nil;
	return dev;
}
// FUNCTION: EVO 0x10070F74
CTabHandle GetCurrentClut(void)
{
	CTabHandle ctab;
	long size;

	ctab = nil;
	if (monitorOpen) {
		size = GetHandleSize((Handle)monitor->workClut);
		ctab = (CTabHandle)NewHandle(size);
		if (ctab != nil)
			BlockMove(*monitor->workClut, *ctab, size);
	}
	return ctab;
}

// FUNCTION: EVO 0x10070FF8
void SaveCurrentClut(CTabHandle dest, long *seed)
{
	long size;
	short i;
	short n;
	GDHandle dev;
	CTabHandle clut;
	char state;
	GDHandle saved;

	if (monitorOpen) {
		dev = monitor->device;
		clut = (*(*dev)->gdPMap)->pmTable;
		saved = GetGDevice();
		SetGDevice(dev);
		state = HGetState((Handle)clut);
		HNoPurge((Handle)clut);
		HLock((Handle)clut);
		size = GetHandleSize((Handle)clut);
		BlockMove(*clut, *dest, size);
		n = (*clut)->ctSize;
		for (i = 0; i <= n; i++)
			(*dest)->ctTable[i].value = i;
		*seed = (*clut)->ctSeed;
		HSetState((Handle)clut, state);
		SetGDevice(saved);
	}
}

// FUNCTION: EVO 0x10071108
void RestoreClut(Boolean setEntries)
{
	CTabHandle src;
	CTabHandle clut;
	short i;
	char state;
	GDHandle dev;
	GDHandle saved;
	short last;

	if (monitorOpen) {
		dev = monitor->device;
		src = monitor->workClut;
		clut = (*(*dev)->gdPMap)->pmTable;
		saved = GetGDevice();
		SetGDevice(dev);
		state = HGetState((Handle)clut);
		HNoPurge((Handle)clut);
		HLock((Handle)clut);
		last = (*clut)->ctSize;
		for (i = 0; i <= last; i++) {
			(*clut)->ctTable[i].rgb.red = (*src)->ctTable[i].rgb.red;
			(*clut)->ctTable[i].rgb.green = (*src)->ctTable[i].rgb.green;
			(*clut)->ctTable[i].rgb.blue = (*src)->ctTable[i].rgb.blue;
		}
		if (setEntries)
			SetEntries(0, last, (*clut)->ctTable);
		HSetState((Handle)clut, state);
		MakeITable(nil, nil, 0);
		(*clut)->ctSeed = monitor->workSeed;
		SaveCurrentClut(monitor->workClut, &monitor->workSeed);
		SetGDevice(saved);
	}
}

// FUNCTION: EVO 0x1007126C
void RestoreOriginalClut(Boolean setEntries)
{
	CTabHandle src;
	CTabHandle clut;
	short i;
	char state;
	GDHandle dev;
	GDHandle saved;
	short last;

	if (monitorOpen) {
		dev = monitor->device;
		src = monitor->origClut;
		clut = (*(*dev)->gdPMap)->pmTable;
		saved = GetGDevice();
		SetGDevice(dev);
		state = HGetState((Handle)clut);
		HNoPurge((Handle)clut);
		HLock((Handle)clut);
		last = (*clut)->ctSize;
		for (i = 0; i <= last; i++) {
			(*clut)->ctTable[i].rgb.red = (*src)->ctTable[i].rgb.red;
			(*clut)->ctTable[i].rgb.green = (*src)->ctTable[i].rgb.green;
			(*clut)->ctTable[i].rgb.blue = (*src)->ctTable[i].rgb.blue;
		}
		if (setEntries)
			SetEntries(0, last, (*clut)->ctTable);
		HSetState((Handle)clut, state);
		MakeITable(nil, nil, 0);
		(*clut)->ctSeed = monitor->origSeed;
		SetGDevice(saved);
	}
}

// FUNCTION: EVO 0x100713C0
void ForceScreenUpdate(void)
{
	RgnHandle rgn;
	WindowPeek w;
	Boolean done;
	GrafPtr port;

	if (monitorOpen) {
		done = false;
		rgn = NewRgn();
		RectRgn(rgn, &(*monitor->device)->gdRect);
		w = (WindowPeek)FrontWindow();
		while (!done) {
			if (w == nil)
				done = true;
			else if (w->nextWindow == nil)
				done = true;
			else
				w = w->nextWindow;
		}
		GetPort(&port);
		CalcVisBehind((WindowPtr)w, rgn);
		PaintBehind((WindowPtr)w, rgn);
		SetPort(port);
		DisposeRgn(rgn);
		DrawMenuBar();
	}
}

// FUNCTION: EVO 0x100714B4
void ForceMonitorUpdate(void)
{
	RgnHandle rgn;
	WindowPeek w;
	Boolean done;
	GrafPtr port;

	if (monitorOpen) {
		done = false;
		rgn = NewRgn();
		RectRgn(rgn, &(*monitor->device)->gdRect);
		w = (WindowPeek)FrontWindow();
		GetPort(&port);
		CalcVisBehind((WindowPtr)w, rgn);
		PaintBehind((WindowPtr)w, rgn);
		SetPort(port);
		DisposeRgn(rgn);
		DrawMenuBar();
	}
}

// FUNCTION: EVO 0x10071568
void UnprotectMonitor(GDHandle dev)
{
	short last;
	short i;
	GDHandle saved;

	saved = GetGDevice();
	SetGDevice(dev);
	last = (*(*(*dev)->gdPMap)->pmTable)->ctSize;
	for (i = 0; i <= last; i++) {
		ProtectEntry(i, false);
		ReserveEntry(i, false);
		ProtectEntry(i, false);
	}
	SetGDevice(saved);
}

// FUNCTION: EVO 0x10071628
Boolean DepthAvail(void)
{
	unsigned long response;
	Boolean ok;

	ok = false;
	if (Gestalt(gestaltQuickdrawVersion, (long *)&response) == noErr && response >= gestalt32BitQD11)
		ok = true;
	return ok;
}

// FUNCTION: EVO 0x10071680
void SubtractFractCluts(FractColor *a, FractColor *b, short last)
{
	short i;

	for (i = 0; i <= last; i++) {
		a[i].red -= b[i].red;
		a[i].green -= b[i].green;
		a[i].blue -= b[i].blue;
	}
}

// FUNCTION: EVO 0x100716E0
void FractClutToCTab(FractColor *f, CTabHandle ctab)
{
	short i;

	if (ctab == nil)
		return;
	if (f == nil)
		return;
	for (i = 0; i <= (*ctab)->ctSize; i++) {
		(*ctab)->ctTable[i].rgb.red = f[i].red >> 16;
		(*ctab)->ctTable[i].rgb.green = f[i].green >> 16;
		(*ctab)->ctTable[i].rgb.blue = f[i].blue >> 16;
	}
}

// FUNCTION: EVO 0x10071764
FractColor *CTabToFractClut(CTabHandle ctab)
{
	FractColor *f;
	char state;
	short i;

	f = nil;
	if (ctab != nil) {
		state = HGetState((Handle)ctab);
		HNoPurge((Handle)ctab);
		HLock((Handle)ctab);
		f = (FractColor *)NewPtr(kMaxClutEntries * sizeof(FractColor));
		if (f != nil) {
			for (i = 0; i <= (*ctab)->ctSize; i++) {
				f[i].red = (long)(*ctab)->ctTable[i].rgb.red << 16;
				f[i].green = (long)(*ctab)->ctTable[i].rgb.green << 16;
				f[i].blue = (long)(*ctab)->ctTable[i].rgb.blue << 16;
			}
		}
		HSetState((Handle)ctab, state);
	}
	return f;
}
