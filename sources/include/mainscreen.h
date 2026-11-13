/* mainscreen.h -- main screen and prefs dialogs. Notes: Research/notes/headers.md */
#ifndef MAINSCREEN_H
#define MAINSCREEN_H

#include "evglobals.h"

void DoAboutBox(void);
void PlayVictoryAnimation(void);
short TrackMainScreenButtons(Point where);
void InitMainScreen(void);
void DoMainScreen(void);
void InitMenuBar(void);
void DoMenuClick(long menuResult);
void SetupInterfaceBackground(void);
void DoPrefsDialog(void);
void RunPrefsDialog(void);
pascal Boolean KeyFilter(DialogPtr theDialog, EventRecord *event, short *itemHit);
void KeyDialogUpdate(void);
void DoGameSpeedDialog(void);
void DrawGameSpeedSlider(void);
void EnterGameFromMainScreen(void);
void DoNewPilot(void);
Boolean DoNewPilotNameDialog(StringPtr name, short maxLen, Boolean *strictPlay);
void PlayCreditsChime(void);
void ShowIntroScreen(void);
void InitTitleRects(void);
void MainScreenRolloverDraw(void);
void DrawClosedButtons(void);
void AnimateRowReveal(void);

#endif
