/* handlers.h -- sprite handlers, shots, beams, explosions. Notes: Research/notes/headers.md */
#ifndef HANDLERS_H
#define HANDLERS_H

#include "evglobals.h"

pascal void HandleShipDisplay(SpritePtr sprite);
pascal void HandleBoxes(SpritePtr sprite);
pascal void HandleExplods(SpritePtr sprite);
pascal void HandleSmoke(SpritePtr sprite);
pascal void HandleStars(SpritePtr sprite);
pascal void HandleDoubleTimeSprite(SpritePtr sprite);
pascal void HandleAlertSprite(SpritePtr sprite);
pascal void HandleStellarSprites(SpritePtr sprite);
pascal void HandleNavTargetSprites(SpritePtr sprite);
pascal void HandleWeapTargetSprites(SpritePtr sprite);
void HandleComm(void);
void HandleBeams(void);
void EraseBeams(void);
void DrawBeams(void);
void SpawnBeam(short owner, short target, short weapType, short angle, Boolean sourceShip, short fixedRange);
void NoOp(void);
void HandleVoiceSound(void);
void KillVoiceSound(void);
void WarpUpCallback(long which, long a, long b);
void HandleGlobalPriceFlux(void);
void HandleLocalPriceFlux(void);
void HandleShotGuidance(short s);
void HandleStellarSystemVisibility(void);
void HandleTimeAdjustment(void);
void HandleShip(ShipObj *ship);
pascal void HandleShot(SpritePtr sprite);

#endif
