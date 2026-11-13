/* setup.h -- game setup. Notes: Research/notes/headers.md */
#ifndef SETUP_H
#define SETUP_H

#include "evglobals.h"

void SetupToolbox(void);
void SetupSAT(void);
void SetupWind(void);
void InitArrays(void);
void InitOffscreen(void);
void Bye(void);
void InitColors(void);
void InitSprites(void);
void LoadSounds(void);
void InitObjects(void);
void ResetPlayer(Boolean newPilot);
void ResetPlayerRecord(Boolean newPilot);
void InitStarSprites(void);
void RandomizeStars(void);
pascal void SetupDockSprite(SpritePtr sp);
pascal void SetupEscortSprite(SpritePtr sp);
void CheckSystem(void);
void BailCheck(void);
void DefaultPrefs(void);
void DefaultKeys(void);
void SetupMonitor(void);
void ResetGameData(void);
void MeasureProcessorSpeed(void);
pascal void EmergencyBail(void);

#endif
