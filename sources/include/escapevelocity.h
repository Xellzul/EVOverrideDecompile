/* escapevelocity.h -- main and the game loop. Notes: Research/notes/headers.md */
#ifndef ESCAPEVELOCITY_H
#define ESCAPEVELOCITY_H

#include "evglobals.h"

void main(void);
void SpawnWorldSpriteNodes(void);
void PlayGame(void);
void DoPlayGameWork(Boolean full);
pascal void ShotHitTask(SpritePtr a, SpritePtr b);
pascal void RunWeaponHitDispatcher(SpritePtr a, SpritePtr b);
void DamageShip(ShipObj *ship, FloatPoint where, short impact, short massDmg, short energyDmg, short owner, Boolean flagA, Boolean flagB, Boolean fromEscort, Boolean flagD);
void DeleteAllComputerShips(Boolean disabledToo, Boolean b);
void SetupShipsInSystem(short syst);
short RandomShipSpawn(short syst, short mode);
short HyperShipSpawn(short syst);
short SpecificDudeSpawn(short dude, short syst);
short MissionDudeSpawn(short dude, short syst, short mission);
void EnterMoreShips(short system);
void RespawnEscort(ShipObj *esc);
#define kCommShowTicks 240
#define kCommTextSize 12
void DisplayComm(StringPtr msg, short count, short font, short size, RGBColor *color, Boolean italicOn, Boolean noBlit);
void RedrawComm(Boolean noBlit);
Boolean LaunchFighter(ShipObj *launcher, short weapon);
void JettisonCargo(Boolean missionCargo);
void SpawnBox(short ship);
void SpawnShot(short owner, short target, short weapType);
void SpawnExplod(FloatPoint where, short type, short size, short extra);
void SpawnSmoke(FloatPoint where, short mode, short layer);
void CreateAsteroids();
void SpawnAsteroid(Boolean edge);
pascal void AsteroidTask(SpritePtr sp);
pascal void AsteroidHitTask(SpritePtr a, SpritePtr b);
short SpawnStellarDefenseShip(short stellar);
Boolean StellarHasDefenders(short stellar);
Boolean SufficentTargetedDamage(ShipObj *ship);
void CalculateTargetedDamage(void);
void MakeEscortsHyperWithParent(ShipObj *leader);
short SpawnEscort(short type, short stellar);
void DoEscortLand(ShipObj *leader);
void DoEscortPayment(short ticks);
void DoStellarTimePassage(void);
short SpawnPerson(short syst, Boolean flag, short pers);
void DoShipCapture(ShipObj *old, Boolean keepAsEscort);
void DisasterHandler(void);
void DoAutoRefuel(void);
void SpawnFleet(short syst);
short GenericRandomShipSpawn(short syst, short reserve);
void HyperSpawnFleet(short syst, short fleet);
void SpeakVoiceMessage(short id);
void ShowPersonHailQuote(short quote);
void SpawnBountyHunter(void);
void BankRobberyNewsEvent(void);

#endif
