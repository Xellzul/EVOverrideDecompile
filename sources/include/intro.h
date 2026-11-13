/* intro.h -- intro and boot screens. Notes: Research/notes/headers.md */
#ifndef INTRO_H
#define INTRO_H

#include "evglobals.h"

void ShowSplashScreen(void);
void HideSplashScreen(void);
void ShowAmbrosiaLogo(void);
void HideAmbrosiaLogo(void);
void InitIntroProgressBar(void);
void DeltaIntroProgressBar(double amount);
void DrawIntroProgressBar(void);
Boolean ShowScrollingText(short strListID);
void StartIntroMusic(void);
void StopIntroMusic(Boolean beep);
void LoadAndStartSoundPair(short id);
void StopAndDisposeSoundPair(void);
void SoundPairCallback(long message);

#endif
