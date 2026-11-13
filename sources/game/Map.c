/* Map.c -- the galaxy map dialog: drawing, scrolling, the nebula backgrounds and the hyperspace route. Notes: Research/notes/Map.md */
#define OLDROUTINENAMES 1
#include <Types.h>
#include <Memory.h>
#include <Quickdraw.h>
#include <Dialogs.h>
#include <Events.h>
#include <Resources.h>
#include <ToolUtils.h>
#include <TextUtils.h>
#include <Icons.h>

#include <ctype.h>

#include "evo.h"

#define kMapDlogID			2000
#define kNebulaPictBase		9500
#define kMapCursorID	128
#define kHereIconID	15000
#define kDestIconID	15001
#define kMissionSystSlots	16
#define kBackslashCharCode	'\\'

double		mapZoom;
CIconHandle	hereIcon;
CIconHandle	destIcon;
RgnHandle	mapCursorSwitchRgn;
PicHandle	mapPics[8];
PicHandle	nebulaPicts[MaxNebulae][kNebulaZooms];
Boolean		mapCursorChanged;
Boolean		gMapScrolling;
Boolean		didUpdateMap;
Boolean		canZoomIn;
Boolean		canZoomOut;
Boolean		showClearRouteButton;
Boolean		gMissionFlag;
short		missionSyst[kMissionSystSlots];
short		greenMissionSyst;
short		selSystem;
DialogPtr	mapDialog;
Rect		nebulaRect[MaxNebulae];
CursHandle	mapCursor;

// FUNCTION: EVO 0x10030014
void DoSystemMap(void)
{
	short			i;
	short			j;
	short			k;
	short			found;
	short			routeLen;
	Boolean			inRoute;
	Boolean			selected;
	Boolean			linked;
	Rect			box;
	Rect			r;
	short			itemHit;
	short			itemType;
	short			sys;
	short			unused70;
	short			savedStellar;
	short			centerV;
	short			link;
	short			w;
	short			h;
	short			lastClickSys;
	Handle			item;
	long			unused5C;
	ShortPoint		localPt;
	ShortPoint		startCenter;
	ShortPoint		sysPt;
	long			lastClickTime;
	short			unused4A;
	Point			mouse;
	Point			lastMouse;
	Point			startMouse;
	Boolean			done;
	ModalFilterUPP	filter;

	done = false;
	filter = NewModalFilterProc(MapFilter);
	mapDialog = nil;
	mapDialog = GetNewDialog(kMapDlogID, nil, (WindowPtr)-1);
	if (mapDialog == nil)
		return;
	savedStellar = shipData->targetStellar;
	if (mapZoom == 0.0)
		mapZoom = 1.0;
	CompactHyperRoute();
	showClearRouteButton = false;
	for (i = 1; i < kMaxHyperRoute; i++)
		if (hyperRoute[i] != -1)
			showClearRouteButton = true;
	for (i = 0; i < MaxNebulae; i++)
		for (j = 0; j <= 2; j++)
			nebulaPicts[i][j] = GetPicture(kNebulaPictBase + i * kNebulaZooms + j);
	for (i = 0; i <= 1; i++)
		mapPics[i] = GetPicture(i + kDone78ButtonPict);
	for (i = 2; i <= 5; i++)
		mapPics[i] = GetPicture(i + (kMinusButtonPict - 2));
	mapPics[6] = GetPicture(kClearRouteButtonPict);
	mapPics[7] = GetPicture(kClearRouteButtonPict + 1);
	if (mapCursor == nil)
		mapCursor = GetCursor(kMapCursorID);
	mapCursorChanged = false;
	mapCursorSwitchRgn = nil;
	gMapScrolling = false;
	msgRgn = NewRgn();
	for (i = 0; i < kMissionSystSlots; i++)
		missionSyst[i] = -1;
	k = 0;
	for (i = 0; i < MaxSimultaneousMissions; i++) {
		if (currentMissionObjectives[i].active) {
			sys = -1;
			if (currentMissions[i].travelStellar >= 0 && stellarData[currentMissions[i].travelStellar].loaded)
				sys = stellarData[currentMissions[i].travelStellar].system;
			if (currentMissions[i].returnStellar >= 0
					&& currentMissions[i].travelStellar != currentMissions[i].returnStellar
					&& currentMissionObjectives[i].returnDone
					&& stellarData[currentMissions[i].returnStellar].loaded)
				sys = stellarData[currentMissions[i].returnStellar].system;
			if (sys != -1 && !(currentMissions[i].flags & kMisnNoArrows)) {
				missionSyst[k] = sys;
				k++;
			}
			if ((currentMissions[i].flags & kMisnShipSystArrow) && !(currentMissions[i].flags & kMisnNoArrows)
					&& currentMissions[i].shipCount > 0 && currentMissions[i].shipSyst >= 0
					&& stellarData[currentMissions[i].shipSyst].loaded) {	/* BUG: a system index used on the stellar table */
				missionSyst[k] = currentMissions[i].shipSyst;
				k++;
			}
		}
	}
	if (hereIcon == nil)
		hereIcon = GetCIcon(kHereIconID);
	if (destIcon == nil)
		destIcon = GetCIcon(kDestIconID);
	if (shipData->navTargetKind == kNavHyperspace && shipData->targetStellar != -1 && !onPlanet)
		selSystem = gSystems[shipData->curSystem].links[shipData->targetStellar];
	else
		selSystem = shipData->curSystem;
	RedrawNebulaeOffscreen();
	NewDialogHook(mapDialog, 0);
	CenterWindow(mapDialog);
	ShowWindow(mapDialog);
	SelectWindow(mapDialog);
	SATSetPortScreen();
	SetPort(mapDialog);
	lastClickTime = 0;
	lastClickSys = -1;
	FlushEvents(everyEvent - diskMask, 0);
	InvalRect(&mapDialog->portRect);
	MapDialogUpdate();
	CreateMapTriggerRgn();
	do {
		ModalDialog(filter, &itemHit);
		if (itemHit == kOkItem)
			done = true;
		if (itemHit == 7)
			SetCursor(&qd.arrow);
		if (itemHit == kMapZoomOutItem && canZoomOut) {
			mapZoom *= 1.333333;
			GetDialogItem(mapDialog, 2, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapZoomOutItem, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapZoomInItem, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapInfoItem, &itemType, &item, &box);
			InvalRect(&box);
			RedrawNebulaeOffscreen();
		}
		if (itemHit == kMapZoomInItem && canZoomIn) {
			mapZoom *= 0.75;
			GetDialogItem(mapDialog, 2, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapZoomOutItem, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapZoomInItem, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapInfoItem, &itemType, &item, &box);
			InvalRect(&box);
			RedrawNebulaeOffscreen();
		}
		if (itemHit == kMapClearRouteItem) {
			for (i = 0; i < kMaxHyperRoute; i++)
				hyperRoute[i] = -1;
			showClearRouteButton = false;
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapClearRouteItem, &itemType, &item, &box);
			InvalRect(&box);
			MapDialogUpdate();
		}
		if (itemHit == kMapViewItem) {
			GetMouse(&mouse);
			lastMouse = mouse;
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
			localPt.h = mouse.h - box.left;
			localPt.v = mouse.v - box.top;
			w = box.right - box.left;
			h = box.bottom - box.top;
			found = -1;
			for (i = 0; i < MaxSystems; i++) {
				sysPt.h = gSystems[i].xPos;
				sysPt.v = gSystems[i].yPos;
				r.left = box.left + w / 2 + sysPt.h / mapZoom - mapCenter.h / mapZoom;
				r.top = box.top + h / 2 + sysPt.v / mapZoom - mapCenter.v / mapZoom;
				r.right = r.left;
				r.bottom = r.top;
				InsetRect(&r, -10, -10);
				if (gSystems[i].loaded && PtInRect(mouse, &r)) {
					inRoute = selected = false;
					for (k = 0; k < kMissionSystSlots; k++) {
						if (i == missionSyst[k] && missionSyst[k] != -1) {
							inRoute = true;
							break;
						}
					}
					if (greenMissionSyst == i)
						selected = true;
					if (gSystems[i].visited > 0) {
						found = i;
						break;
					}
					if (inRoute || selected) {
						found = i;
						break;
					}
					for (j = 0; j < kMaxSystemLinks; j++) {
						if (gSystems[i].links[j] != -1 && gSystems[gSystems[i].links[j]].visited > 0
								&& gSystems[gSystems[i].links[j]].loaded) {
							found = i;
							break;
						}
					}
				}
			}
			if (found != -1) {
				GetDialogItem(mapDialog, 2, &itemType, &item, &r);
				InvalRect(&r);
				GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &r);
				InvalRect(&r);
				GetDialogItem(mapDialog, kMapInfoItem, &itemType, &item, &r);
				InvalRect(&r);
				if (KeyCheck(kKeyShift)) {
					CompactHyperRoute();
					routeLen = -1;
					for (i = 1; i < kMaxHyperRoute; i++) {
						if (hyperRoute[i] == -1) {
							routeLen = i;
							break;
						}
					}
					linked = false;
					if (routeLen != -1) {
						if (routeLen > 1) {
							for (i = 0; i < kMaxSystemLinks; i++) {
								if (found == gSystems[hyperRoute[routeLen - 1]].links[i]) {
									linked = true;
									if (gSystems[hyperRoute[routeLen - 1]].visited <= 0
											&& gSystems[gSystems[hyperRoute[routeLen - 1]].links[i]].visited <= 0)
										linked = false;
								}
							}
						} else {
							for (i = 0; i < kMaxSystemLinks; i++)
								if (found == gSystems[shipData->curSystem].links[i])
									linked = true;
						}
						if (routeLen > 0 && found == hyperRoute[routeLen - 1]) {
							hyperRoute[routeLen - 1] = -1;
							routeLen = -1;
						}
					}
					if (linked) {
						if (routeLen != -1) {
							if (hyperRoute[0] == -1)
								hyperRoute[0] = shipData->curSystem;
							hyperRoute[routeLen] = found;
							showClearRouteButton = true;
						}
						GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &r);
						InvalRect(&r);
						GetDialogItem(mapDialog, kMapClearRouteItem, &itemType, &item, &r);
						InvalRect(&r);
						DrawMapButtons(-1);
						if (portDialog == nil)
							CueNextHyperRouteDest();
					}
				} else {
					selSystem = found;
					link = -1;
					for (i = 0; i < kMaxSystemLinks; i++) {
						if (found == gSystems[shipData->curSystem].links[i]) {
							link = i;
							break;
						}
					}
					if (link != -1 && !onPlanet) {
						shipData->navTargetKind = kNavHyperspace;
						shipData->targetStellar = link;
						hudRedraw.nav = true;
						DrawStatus();
						SetPort(mapDialog);
					}
				}
			} else if (StillDown()) {
				GetMouse(&startMouse);
				lastMouse = startMouse;
				startCenter = mapCenter;
				while (StillDown()) {
					lastMouse = mouse;
					GetMouse(&mouse);
					mapCenter.h = startCenter.h - mapZoom * (mouse.h - startMouse.h);
					mapCenter.v = startCenter.v - mapZoom * (mouse.v - startMouse.v);
					ScrollMapDisplay(mouse.h - lastMouse.h, mouse.v - lastMouse.v);
				}
			}
		}
	} while (!done);
	for (i = 0; i <= 5; i++) {
		if (mapPics[i] != nil) {
			HPurge((Handle)mapPics[i]);
			ReleaseResource((Handle)mapPics[i]);
		}
	}
	for (i = 0; i < MaxNebulae; i++) {
		for (j = 0; j <= 2; j++) {
			if (nebulaPicts[i][j] != nil) {
				HPurge((Handle)nebulaPicts[i][j]);
				ReleaseResource((Handle)nebulaPicts[i][j]);
			}
		}
	}
	w = (mapDialog->portRect.right - mapDialog->portRect.left) / 2;
	h = (mapDialog->portRect.bottom - mapDialog->portRect.top) / 2;
	if (mapCursorSwitchRgn != nil)
		DisposeRgn(mapCursorSwitchRgn);
	if (msgRgn != nil)
		DisposeRgn(msgRgn);
	DisposeRoutineDescriptor(filter);
	DisposeDialog(mapDialog);
	SATSetPortOffScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.offScreen.bounds);
	SATSetPortBackScreen();
	ForeColor(blackColor);
	PaintRect(&gSAT.backScreen.bounds);
	SATSetPortScreen();
	found = (gSAT.wind.bounds.right - gSAT.wind.bounds.left) / 2;
	centerV = (gSAT.wind.bounds.bottom - gSAT.wind.bounds.top) / 2;
	ForeColor(blackColor);
	SetRect(&r, found - w, centerV - h, found + w, centerV + h);
	box = gSAT.wind.bounds;
	box.left = box.right - kStatusPanelWidth;
	if (SectRect(&r, &box, &box))
		RedrawStatus();
	RefreshSAT();
	SetCursor(&qd.arrow);
	if (portDialog != nil)
		shipData->targetStellar = savedStellar;
	else
		CueNextHyperRouteDest();
}

// FUNCTION: EVO 0x100314CC
void CreateMapTriggerRgn(void)
{
}

// FUNCTION: EVO 0x100314D0
void MapDialogUpdate(void)
{
	short		i;
	short		j;
	short		k;
	short		s;
	long		cx;
	short		cy;
	short		w;
	short		h;
	Boolean		isDest;
	short		off;
	Boolean		flag;
	short		itemType;
	Handle		item;
	short		zoomIdx;
	short		lineW;
	short		textW;
	Rect		unusedRect;
	Rect		box2;
	Rect		box;
	Rect		r2;
	Rect		picRect;
	Rect		iconRect;
	long		unusedLong2;
	Boolean		linkDone[MaxSystems];
	Boolean		isMission;
	FloatPoint	p1;
	FloatPoint	p2;
	Str255		dateStr;
	long		unusedLong;

	for (i = 0; i < MaxSystems; i++) {
		linkDone[i] = false;
		(void)flag;
	}
	(void)off;
	cx = mapCenter.h;
	cy = mapCenter.v;
	SATSetPort(&loadPort);
	PenSize(1, 1);
	ForeColor(blackColor);
	PaintRect(&mapDialog->portRect);
	if (!gMapScrolling) {
		GetDialogItem(mapDialog, 1, &itemType, &item, &box2);
		if (RectInRgn(&box2, mapDialog->visRgn)) {
			DrawPicture(mapPics[0], &box2);
			didUpdateMap = true;
		}
		GetDialogItem(mapDialog, kMapZoomOutItem, &itemType, &item, &box2);
		if (RectInRgn(&box2, mapDialog->visRgn)) {
			if (canZoomOut)
				DrawPicture(mapPics[2], &box2);
			else
				PaintRect(&box2);
			didUpdateMap = true;
		}
		GetDialogItem(mapDialog, kMapZoomInItem, &itemType, &item, &box2);
		if (RectInRgn(&box2, mapDialog->visRgn)) {
			if (canZoomIn)
				DrawPicture(mapPics[4], &box2);
			else
				PaintRect(&box2);
			didUpdateMap = true;
		}
	}
	if (gMapScrolling)
		flag = true;
	else {
		GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
		flag = RectInRgn(&box, mapDialog->visRgn);
	}
	if (flag) {
		if (gMapScrolling) {
			SATSetPort(&loadPort);
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
		} else {
			SATSetPortOffScreen();
			PenSize(1, 1);
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
		}
		w = box.right - box.left;
		h = box.bottom - box.top;
		ForeColor(blackColor);
		PaintRect(&box);
		RGBForeColor(&rgbDarkGrey);
		FrameRect(&box);
		ForeColor(blackColor);
		TextFont(geneva);
		TextSize(7);
		if (mapZoom > 1.0)
			TextSize(7);
		if (mapZoom == 1.0)
			TextSize(kDialogTextSize);
		if (mapZoom < 1.0)
			TextSize(kDialogTextSize);
		for (i = 0; i < MaxNebulae; i++) {
			if (mapZoom > 1.0)
				zoomIdx = 0;
			if (mapZoom == 1.0)
				zoomIdx = 1;
			if (mapZoom < 1.0)
				zoomIdx = 2;
			if (nebulaData[i].explored) {
				picRect.left = box.left + w / 2 + nebulaData[i].xPos / (float)mapZoom - cx / mapZoom;
				picRect.top = box.top + h / 2 + nebulaData[i].yPos / (float)mapZoom - cy / mapZoom;
				picRect.right = picRect.left + nebulaData[i].width / (float)mapZoom;
				picRect.bottom = picRect.top + nebulaData[i].height / (float)mapZoom;
				if (SectRect(&picRect, &box, &r2)) {
					if (mapZoom > 1.0)
						zoomIdx = 0;
					if (mapZoom == 1.0)
						zoomIdx = 1;
					if (mapZoom < 1.0)
						zoomIdx = 2;
					if (gMapScrolling)
						SATSetPort(&loadPort);
					else
						SATSetPort(&gSAT.offScreen);
					DrawPicture(nebulaPicts[i][zoomIdx], &picRect);
				}
			}
		}
		i = shipData->curSystem;
		for (j = 0; j < kMaxSystemLinks; j++) {
			if (gSystems[i].links[j] != -1 && gSystems[gSystems[i].links[j]].loaded) {
				if (shipData->navTargetKind == kNavHyperspace && j == shipData->targetStellar)
					RGBForeColor(&dimGreen);
				else
					RGBForeColor(&rgbDarkGrey);
				p1.x = gSystems[i].xPos;
				p1.y = gSystems[i].yPos;
				p2.x = gSystems[gSystems[i].links[j]].xPos;
				p2.y = gSystems[gSystems[i].links[j]].yPos;
				MoveTo(box.left + w / 2 + p1.x / mapZoom - cx / mapZoom, box.top + h / 2 + p1.y / mapZoom - cy / mapZoom);
				LineTo(box.left + w / 2 + p2.x / mapZoom - cx / mapZoom, box.top + h / 2 + p2.y / mapZoom - cy / mapZoom);
			}
		}
		for (i = 0; i < kMaxHyperRoute - 1; i++) {
			if (hyperRoute[i] != -1 && hyperRoute[i + 1] != -1) {
				p1.x = gSystems[hyperRoute[i]].xPos;
				p1.y = gSystems[hyperRoute[i]].yPos;
				p2.x = gSystems[hyperRoute[i + 1]].xPos;
				p2.y = gSystems[hyperRoute[i + 1]].yPos;
				PenSize(2, 2);
				RGBForeColor(&brightGreen);
				MoveTo(box.left + w / 2 + p1.x / mapZoom - cx / mapZoom, box.top + h / 2 + p1.y / mapZoom - cy / mapZoom);
				LineTo(box.left + w / 2 + p2.x / mapZoom - cx / mapZoom, box.top + h / 2 + p2.y / mapZoom - cy / mapZoom);
				PenSize(1, 1);
			}
		}
		linkDone[shipData->curSystem] = true;
		for (i = 0; i < MaxSystems; i++) {
			if (gSystems[i].loaded && gSystems[i].visited > 0) {
				for (j = 0; j < kMaxSystemLinks; j++) {
					if (gSystems[i].links[j] != -1 && gSystems[gSystems[i].links[j]].loaded
							&& !linkDone[gSystems[i].links[j]]) {
						if (i == shipData->curSystem && shipData->navTargetKind == kNavHyperspace && j == shipData->targetStellar)
							RGBForeColor(&dimGreen);
						else
							RGBForeColor(&rgbDarkGrey);
						p1.x = gSystems[i].xPos;
						p1.y = gSystems[i].yPos;
						p2.x = gSystems[gSystems[i].links[j]].xPos;
						p2.y = gSystems[gSystems[i].links[j]].yPos;
						MoveTo(box.left + w / 2 + p1.x / mapZoom - cx / mapZoom, box.top + h / 2 + p1.y / mapZoom - cy / mapZoom);
						LineTo(box.left + w / 2 + p2.x / mapZoom - cx / mapZoom, box.top + h / 2 + p2.y / mapZoom - cy / mapZoom);
					}
				}
				linkDone[i] = true;
			}
		}
		for (i = 0; i < MaxSystems; i++) {
			flag = false;
			isDest = false;
			isMission = false;
			if (gSystems[i].loaded) {
				if (shipData->curSystem == i)
					flag = true;
				else {
					flag = false;
					for (j = 0; j < kMaxSystemLinks; j++) {
						s = gSystems[i].links[j];
						if (s != -1 && gSystems[s].loaded && gSystems[s].visited > 0) {
							flag = true;
							break;
						}
					}
				}
				for (k = 0; k < kMissionSystSlots; k++) {
					if (i == missionSyst[k] && missionSyst[k] != -1) {
						isDest = true;
						break;
					}
				}
				if (greenMissionSyst == i) {
					isMission = true;
					if (!gMissionFlag)
						isDest = false;
				}
				if (flag || isDest || isMission) {
					p1.x = gSystems[i].xPos;
					p1.y = gSystems[i].yPos;
					box2.left = box.left + w / 2 + p1.x / mapZoom - cx / mapZoom;
					box2.top = box.top + h / 2 + p1.y / mapZoom - cy / mapZoom;
					box2.right = box2.left;
					box2.bottom = box2.top;
					InsetRect(&box2, -4, -4);
					ForeColor(blackColor);
					PaintOval(&box2);
					ColorCodeSystem(i);
					FrameOval(&box2);
					if (i == shipData->curSystem) {
						r2 = box2;
						InsetRect(&r2, 2, 2);
						ForeColor(cyanColor);
						PaintOval(&r2);
					}
					if (isDest) {
						SetRect(&iconRect, box2.left - 16, box2.top - 16, box2.left, box2.top);
						off = 0;
						if (mapZoom > 1.0)
							off = 8;
						if (mapZoom == 1.0)
							off = 8;
						if (mapZoom < 1.0)
							off = 0;
						iconRect.left += off;
						iconRect.top += off;
						if (hereIcon != nil)
							PlotCIcon(&iconRect, hereIcon);
					}
					if (isMission) {
						SetRect(&iconRect, box2.right, box2.top - 16, box2.right + 16, box2.top);
						off = 0;
						if (mapZoom > 1.0)
							off = 8;
						if (mapZoom == 1.0)
							off = 8;
						if (mapZoom < 1.0)
							off = 0;
						iconRect.right -= off;
						iconRect.top += off;
						if (destIcon != nil)
							PlotCIcon(&iconRect, destIcon);
					}
				}
			}
		}
		if (selSystem != -1) {
			p1.x = gSystems[selSystem].xPos;
			p1.y = gSystems[selSystem].yPos;
			box2.left = box.left + w / 2 + p1.x / mapZoom - cx / mapZoom;
			box2.top = box.top + h / 2 + p1.y / mapZoom - cy / mapZoom;
			box2.right = box2.left;
			box2.bottom = box2.top;
			InsetRect(&box2, -6, -6);
			RGBForeColor(&brightGreen);
			PenSize(1, 1);
			MoveTo(box2.left + 1, box2.top);
			LineTo(box2.left + 4, box2.top);
			MoveTo(box2.left + 1, box2.bottom);
			LineTo(box2.left + 4, box2.bottom);
			MoveTo(box2.right - 1, box2.top);
			LineTo(box2.right - 4, box2.top);
			MoveTo(box2.right - 1, box2.bottom);
			LineTo(box2.right - 4, box2.bottom);
			MoveTo(box2.left, box2.top + 1);
			LineTo(box2.left, box2.top + 4);
			MoveTo(box2.left, box2.bottom - 1);
			LineTo(box2.left, box2.bottom - 4);
			MoveTo(box2.right, box2.top + 1);
			LineTo(box2.right, box2.top + 4);
			MoveTo(box2.right, box2.bottom - 1);
			LineTo(box2.right, box2.bottom - 4);
			ForeColor(blackColor);
		}
		ForeColor(whiteColor);
		for (i = 0; i < MaxSystems; i++) {
			if (gSystems[i].loaded && gSystems[i].visited > 0 && mapZoom <= 1.75) {
				MoveTo(7 + (box.left + w / 2 + gSystems[i].xPos / mapZoom - cx / mapZoom),
					4 + (box.top + h / 2 + gSystems[i].yPos / mapZoom - cy / mapZoom));
				DrawString(gSystems[i].name);
			}
		}
		RGBForeColor(&rgbMedGrey);
		FrameRect(&box);
		ForeColor(blackColor);
		didUpdateMap = true;
		if (gMapScrolling) {
			SATSetPortScreen();
			SetPort(mapDialog);
			ForeColor(blackColor);
			InsetRect(&box, 1, 1);
			CopyBits(&loadPort.port->portBits, &mapDialog->portBits, &box, &box, srcCopy, nil);
		} else {
			SATSetPort(&loadPort);
			ForeColor(blackColor);
			CopyBits(&gSAT.offScreen.port->portBits, &loadPort.port->portBits, &box, &box, srcCopy, nil);
		}
	}
	if (!gMapScrolling) {
		GetDialogItem(mapDialog, 2, &itemType, &item, &box);
		if (RectInRgn(&box, mapDialog->visRgn)) {
			ForeColor(blackColor);
			PaintRect(&box);
			RGBForeColor(&rgbDarkGrey);
			FrameRect(&box);
			ForeColor(blackColor);
			GetDialogItem(mapDialog, kMapInfoItem, &itemType, &item, &r2);
			MoveTo(r2.left + 1, box.top);
			LineTo(r2.right - 2, box.top);
			TextFont(geneva);
			TextSize(kDialogTextSize);
			s = selSystem;
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 10, box.top + 12);
			DrawString("\pPorts:");
			ForeColor(whiteColor);
			MoveTo(box.left + 45, box.top + 12);
			if (gSystems[s].visited > 0) {
				cx = 0;
				lineW = 0;
				j = 0;
				for (i = 0; i <= 3; i++) {
					if (gSystems[s].navDefault[i] != -1
							&& !(stellarData[gSystems[s].navDefault[i]].flags & kSpobUninhabited)
							&& (stellarData[gSystems[s].navDefault[i]].flags & kSpobCanLand))
						cx++;
				}
				if (cx > 0) {
					for (i = 0; i <= 3; i++) {
						if (gSystems[s].navDefault[i] != -1
								&& !(stellarData[gSystems[s].navDefault[i]].flags & kSpobUninhabited)
								&& (stellarData[gSystems[s].navDefault[i]].flags & kSpobCanLand)) {
							textW = StringWidth(stellarData[gSystems[s].navDefault[i]].name);
							if (box.left + 45 + textW + lineW > r2.right - 20) {
								MoveTo(box.left + 45, box.top + 24);
								lineW = 0;
							} else
								lineW += textW;
							DrawString(stellarData[gSystems[s].navDefault[i]].name);
							j++;
							if (cx > j) {
								DrawString("\p, ");
								lineW += StringWidth("\p, ");
							}
						}
					}
				} else
					DrawString("\pNone");
			} else
				DrawString("\p<Unknown>");
			RGBForeColor(&rgbLightGrey);
			MoveTo(box.left + 10, box.top + 36);
			DrawString("\pNavigation Hazards:");
			ForeColor(whiteColor);
			MoveTo(box.left + 110, box.top + 36);
			if (gSystems[s].visited > 0) {
				if (gSystems[s].asteroidCount > 0) {
					if (gSystems[s].asteroidCount <= 3)
						DrawString("\pSparse ");
					else if (gSystems[s].asteroidCount <= 6)
						DrawString("\pModerate ");
					else
						DrawString("\pDense ");
					DrawString("\pasteroid field");
					if (gSystems[s].interference > 0)
						DrawString("\p, ");
				}
				if (gSystems[s].interference > 0) {
					if (gSystems[s].interference <= 33)
						DrawString("\pLight ");
					else if (gSystems[s].interference <= 66)
						DrawString("\pModerate ");
					else
						DrawString("\pHeavy ");
					DrawString("\psensor interference");
				}
				if (gSystems[s].asteroidCount <= 0 && gSystems[s].interference <= 0)
					DrawString("\pNone");
			} else
				DrawString("\p<Unknown>");
			RGBForeColor(&rgbDarkGrey);
			DateToShortString(gameTime, (char *)dateStr);
			c2pstr((char *)dateStr);
			MoveTo(box.right - StringWidth(dateStr) - 5, box.top + 36);
			DrawString(dateStr);
			ForeColor(blackColor);
			didUpdateMap = true;
		}
		GetDialogItem(mapDialog, kMapInfoItem, &itemType, &item, &box2);
		/* BUG: s keeps the value of the Ports panel; stale when only this panel is redrawn */
		if (RectInRgn(&box2, mapDialog->visRgn)) {
			ForeColor(blackColor);
			PaintRect(&box2);
			RGBForeColor(&rgbDarkGrey);
			FrameRect(&box2);
			ForeColor(blackColor);
			MoveTo(box2.left + 1, box2.bottom - 1);
			LineTo(box2.right - 2, box2.bottom - 1);
			TextFont(geneva);
			TextSize(kDialogTextSize);
			MoveTo(box2.left + 10, box2.top + 12);
			RGBForeColor(&rgbLightGrey);
			if (shipData->navTargetKind == kNavHyperspace) {
				if (shipData->targetStellar == -1) {
					if (selSystem == shipData->curSystem)
						DrawString("\pCurrent System:");
					else
						DrawString("\pSelected System:");
				} else
					DrawString("\pDestination System:");
			} else if (selSystem == shipData->curSystem)
				DrawString("\pCurrent System:");
			else
				DrawString("\pSelected System:");
			MoveTo(box2.left + 15, box2.top + 26);
			ForeColor(whiteColor);
			if (gSystems[s].visited <= 0)
				DrawString("\p<Unknown>");
			else {
				DrawString(gSystems[s].name);
				if (SystemIsInhabited(s)) {
					RGBForeColor(&rgbLightGrey);
					MoveTo(box2.left + 10, box2.top + 75);
					DrawString("\pGovernment:");
					ForeColor(whiteColor);
					MoveTo(box2.left + 15, box2.top + 88);
					if (gSystems[s].govt == -1)
						DrawString("\pIndependent");
					else
						DrawString(govtData[gSystems[s].govt].name);
					MoveTo(box2.left + 10, box2.top + 119);
					RGBForeColor(&rgbLightGrey);
					DrawString("\pLegal Status:");
					ForeColor(whiteColor);
					MoveTo(box2.left + 15, box2.top + 132);
					DrawLegalStatusString(s);
					RGBForeColor(&rgbLightGrey);
					MoveTo(box2.left + 10, box2.top + 155);
					DrawString("\pGoods Traded:");
					ForeColor(whiteColor);
					if (gSystems[s].visited < 2) {
						MoveTo(box2.left + 15, box2.top + 168);
						DrawString("\p<Unknown>");
					} else {
						j = 0;
						for (i = 0; i <= 5; i++) {
							if (SystemTradesGoods(s, i)) {
								MoveTo(box2.left + 15, box2.top + 168 + j);
								DrawString(cargoNames[i]);
								j += 12;
							}
						}
						if (j == 0) {
							MoveTo(box2.left + 15, box2.top + 168);
							DrawString("\pNone");
						}
					}
					RGBForeColor(&rgbLightGrey);
					MoveTo(box2.left + 10, box2.top + 250);
					DrawString("\pServices:");
					cx = 0;
					for (i = 0; i <= 2; i++) {
						if (SystemHasService(s, i))
							cx++;
					}
					if (cx > 0) {
						ForeColor(whiteColor);
						if (gSystems[s].visited < 2) {
							MoveTo(box2.left + 15, box2.top + 263);
							DrawString("\p<Unknown>");
						} else {
							j = 0;
							for (i = 0; i <= 2; i++) {
								if (SystemHasService(s, i)) {
									MoveTo(box2.left + 15, box2.top + 263 + j);
									if (i == 0)
										DrawString("\pTrading");
									if (i == 1)
										DrawString("\pOutfitting");
									if (i == 2)
										DrawString("\pShipyard");
									j += 12;
								}
							}
						}
					} else if (gSystems[s].visited >= 2) {
						MoveTo(box2.left + 15, box2.top + 262);
						ForeColor(whiteColor);
						DrawString("\pNone Available");
					} else if (gSystems[s].visited >= 1) {
						MoveTo(box2.left + 15, box2.top + 262);
						ForeColor(whiteColor);
						DrawString("\p<Unknown>");
					}
				} else {
					MoveTo(box2.left + 10, box2.top + 75);
					ForeColor(whiteColor);
					DrawString("\pUninhabited System");
				}
			}
			didUpdateMap = true;
		}
		DrawMapButtons(-1);
		RGBForeColor(&rgbMedGrey);
		FrameRect(&mapDialog->portRect);
		ForeColor(blackColor);
		SATSetPortScreen();
		SetPort(mapDialog);
		ForeColor(blackColor);
		CopyBits(&loadPort.port->portBits, &mapDialog->portBits, &mapDialog->portRect, &mapDialog->portRect, srcCopy, nil);
	}
}

// FUNCTION: EVO 0x10033FAC
void ScrollMapDisplay(short dh, short dv)
{
	short	itemType;
	Handle	item;
	Rect	box;

	SetPort(mapDialog);
	GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
	box.left++;
	box.top++;
	box.right--;
	box.bottom--;
	BackPat(&qd.black);
	ScrollRect(&box, dh, dv, msgRgn);
	BackPat(&qd.white);
	gMapScrolling = true;
	MapDialogUpdate();
	gMapScrolling = false;
}

// FUNCTION: EVO 0x10034088
void RedrawNebulaeOffscreen(void)
{
	short	i;
	short	top;
	short	zoom;

	SATSetPortBackScreen();
	ForeColor(blackColor);
	top = 0;
	for (i = 0; i < MaxNebulae; i++) {
		SetRect(&nebulaRect[i], 0, 0, 0, 0);
		if (nebulaData[i].explored) {
			if (mapZoom > 1.0)
				zoom = 0;
			if (mapZoom == 1.0)
				zoom = 1;
			if (mapZoom < 1.0)
				zoom = 2;
			if (nebulaPicts[i][zoom] != nil) {
				nebulaRect[i].top = gSAT.backScreen.bounds.top + top;
				nebulaRect[i].left = gSAT.backScreen.bounds.left;
				if (mapZoom >= 0.5) {
					nebulaRect[i].bottom = nebulaRect[i].top + nebulaData[i].height / (float)mapZoom;
					nebulaRect[i].right = nebulaRect[i].left + nebulaData[i].width / (float)mapZoom;
				} else {
					nebulaRect[i].bottom = nebulaRect[i].top + nebulaData[i].height / 0.5;
					nebulaRect[i].right = nebulaRect[i].left + nebulaData[i].width / 0.5;
				}
				top = nebulaRect[i].bottom;
				DrawPicture(nebulaPicts[i][zoom], &nebulaRect[i]);
			}
		}
	}
	SATSetPortScreen();
}

// FUNCTION: EVO 0x10034420
pascal Boolean MapFilter(DialogPtr dlg, EventRecord *event, short *itemHit)
{
	Rect	box;
	Rect	r2;
	short	itemType;
	Rect	r3;
	Handle	item;
	short	s1;
	Point	p1;
	Point	where;
	short	s2;
	Boolean	valid[kMaxSystemLinks];
	long	l1;
	short	i;
	short	count;
	char	key;

	if (mapZoom > 0.5)
		canZoomIn = true;
	else
		canZoomIn = false;
	if (mapZoom < 2.0)
		canZoomOut = true;
	else
		canZoomOut = false;
	didUpdateMap = false;
	if (event->what == keyDown || event->what == autoKey) {
		key = event->message & charCodeMask;
		key = tolower(key);
		if (key == kReturnCharCode || key == kEnterCharCode) {
			*itemHit = kMapDoneItem;
			return true;
		}
		if ((key == kTabCharCode || key == kBackslashCharCode) && !onPlanet) {
			shipData->navTargetKind = kNavHyperspace;
			count = 0;
			for (i = 0; i < kMaxSystemLinks; i++) {
				valid[i] = false;
				if (gSystems[shipData->curSystem].links[i] != -1
						&& gSystems[gSystems[shipData->curSystem].links[i]].loaded) {
					valid[i] = true;
					count++;
				}
			}
			if (count > 0) {
				do {
					shipData->targetStellar++;
					if (shipData->targetStellar >= 5)	/* BUG: only links 0-4 are cycled; hangs when every valid link is past 4 */
						shipData->targetStellar = 0;
					if (shipData->targetStellar < 0)
						shipData->targetStellar = 4;
				} while (!valid[shipData->targetStellar]);
			} else
				shipData->targetStellar = -1;
			hudRedraw.nav = true;
			GetDialogItem(mapDialog, 2, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapInfoItem, &itemType, &item, &box);
			InvalRect(&box);
			DrawStatus();
			SetPort(mapDialog);
			if (shipData->navTargetKind == kNavHyperspace && shipData->targetStellar != -1)
				selSystem = gSystems[shipData->curSystem].links[shipData->targetStellar];
			else
				selSystem = shipData->curSystem;
		}
		if (key == 'c') {
			PlaySound(beepSnd[kBeepMessage], 1, kMidVolume, kMidVolume);
			mapCenter.h = gSystems[shipData->curSystem].xPos;
			mapCenter.v = gSystems[shipData->curSystem].yPos;
			GetDialogItem(mapDialog, 2, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
			InvalRect(&box);
			GetDialogItem(mapDialog, kMapInfoItem, &itemType, &item, &box);
			InvalRect(&box);
		}
		if (key == '-') {
			*itemHit = kMapZoomOutItem;
			return true;
		}
		if (key == '+' || key == '=') {
			*itemHit = kMapZoomInItem;
			return true;
		}
		if (key == kLeftArrowCharCode) {
			mapCenter.h -= 10;
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
			InvalRect(&box);
		}
		if (key == kRightArrowCharCode) {
			mapCenter.h += 10;
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
			InvalRect(&box);
		}
		if (key == kUpArrowCharCode) {
			mapCenter.v -= 10;
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
			InvalRect(&box);
		}
		if (key == kDownArrowCharCode) {
			mapCenter.v += 10;
			GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
			InvalRect(&box);
		}
	}
	if (event->what == mouseDown) {
		where = event->where;
		GlobalToLocal(&where);
		GetDialogItem(mapDialog, kMapViewItem, &itemType, &item, &box);
		if (!PtInRect(where, &box)) {
			count = TrackMapButtons(where);
			if (count == 0) {
				*itemHit = kMapDoneItem;
				return true;
			}
			if (count == 1) {
				*itemHit = kMapZoomOutItem;
				return true;
			}
			if (count == 2) {
				*itemHit = kMapZoomInItem;
				return true;
			}
			if (count == 3) {
				*itemHit = kMapClearRouteItem;
				return true;
			}
			*itemHit = -1;
			return true;
		}
		*itemHit = kMapViewItem;
		return true;
	}
	if (event->what == updateEvt) {
		BeginUpdate(mapDialog);
		MapDialogUpdate();
		EndUpdate(mapDialog);
	}
	return false;
}

// FUNCTION: EVO 0x10034AA4
void CompactHyperRoute(void)
{
	short	i;

	if (hyperRoute[0] == -1) {
		for (i = 0; i < kMaxHyperRoute - 1; i++)
			hyperRoute[i] = hyperRoute[i + 1];
		hyperRoute[kMaxHyperRoute - 1] = -1;
	}
}

// FUNCTION: EVO 0x10034B04
void SpoolNextHyperRouteDest(void)
{
	short	i;

	if (hyperRoute[1] == shipData->curSystem) {
		hyperRoute[0] = -1;
		CompactHyperRoute();
	} else if (hyperRoute[1] == -1) {
		for (i = 0; i < kMaxHyperRoute; i++)
			hyperRoute[i] = -1;
	}
}

// FUNCTION: EVO 0x10034B90
void CueNextHyperRouteDest(void)
{
	short	i;

	if (hyperRoute[1] != -1) {
		for (i = 0; i < kMaxSystemLinks; i++) {
			if (hyperRoute[1] == gSystems[shipData->curSystem].links[i]) {
				shipData->navTargetKind = kNavHyperspace;
				shipData->targetStellar = i;
				hudRedraw.nav = true;
				break;
			}
		}
	}
}
