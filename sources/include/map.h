/* map.h -- galaxy map. Notes: Research/notes/headers.md */
#ifndef MAP_H
#define MAP_H

#include "evglobals.h"

#define kNebulaZooms		3

extern PicHandle mapPics[8];
extern Boolean canZoomIn;
extern Boolean canZoomOut;
extern Boolean showClearRouteButton;
extern Boolean gMissionFlag;
extern short greenMissionSyst;
extern DialogPtr mapDialog;

void DoSystemMap(void);
void CreateMapTriggerRgn(void);
void MapDialogUpdate(void);
void ScrollMapDisplay(short dh, short dv);
void RedrawNebulaeOffscreen(void);
pascal Boolean MapFilter(DialogPtr dlg, EventRecord *event, short *itemHit);
void CompactHyperRoute(void);
void SpoolNextHyperRouteDest(void);
void CueNextHyperRouteDest(void);

#endif
