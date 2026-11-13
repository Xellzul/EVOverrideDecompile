/* globals.h -- the record tables and the most-used globals. Notes: Research/notes/structs.md */
#ifndef GLOBALS_H
#define GLOBALS_H

#include "gametypes.h"
#include "shipobj.h"
#include "evrecords.h"

extern ShipObj *shipData;
extern ShipRec *gShipTypes;
extern SpobRecord *stellarData;
extern SpobRecord *currentSpob;
extern SystRecord *gSystems;
extern StarSlot *starData;
extern WeapRecord *gWeapons;
extern BeamRecord *beamData;
extern OutfRecord *gOutfits;
extern DudeRecord *dudeData;
extern GovtRecord *govtData;
extern PersRecord *personData;
extern AsteroidObj *asteroidData;
extern OopsRecord *disasterData;
extern NebuRecord *nebulaData;
extern FletRecord *fleetData;
extern PodObj *podData;
extern ShotObj *shotData;
extern MissionRecord *currentMissions;
extern MissionState *currentMissionObjectives;
extern JunkRecord *junkData;
extern DeqtRecord *movieData;

extern ShipObj *dialogShip;
extern BriefMissionRec briefMissionInfo[MaxMissions];
extern MissionAvailRec missionAvailData[MaxMissions];

#endif
