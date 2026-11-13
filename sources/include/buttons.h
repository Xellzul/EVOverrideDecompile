/* buttons.h -- button rows. Notes: Research/notes/headers.md */
#ifndef BUTTONS_H
#define BUTTONS_H

#include "evglobals.h"

short TrackMapButtons(Point where);
void DrawMapButtons(short hilite);
short TrackShipyardButtons(Point where);
void DrawShipyardButtons(short hilite);
short TrackOutfitButtons(Point where);
void DrawOutfitButtons(short hilite);
short TrackPlanetButtons(Point where);
void DrawPlanetButtons(short hilite);
short TrackTradeButtons(Point where);
void DrawTradeButtons(short hilite);
short TrackCapAssignButtons(Point where);
void DrawCapAssignButtons(short hilite);
short TrackCommDialogButtons(Point where);
void DrawCommDialogButtons(short hilite);
short TrackPlanetCommDialogButtons(Point where);
void DrawPlanetCommDialogButtons(short hilite);
short TrackMissionButtons(Point where);
void DrawMissionButtons(short hilite);
short TrackMissionInfoButtons(Point where);
void DrawMissionInfoButtons(short hilite);
short TrackSingleMissionButtons(Point where);
void DrawSingleMissionButtons(short hilite);
short TrackPlayerInfoButtons(short page, Point where);
void DrawPlayerInfoButtons(short page, short hilite);
short TrackHaggleButtons(Point where);
void DrawHaggleButtons(short hilite);
short TrackPlunderButtons(Point where);
void DrawPlunderButtons(short hilite);
short TrackBarButtons(Point where);
void DrawBarButtons(short hilite);
short TrackSlotButtons(Point where);
void DrawSlotButtons(short hilite);
short TrackOkButton(Rect box);
void DrawOkButton(Rect box, Boolean hilite);

#endif
