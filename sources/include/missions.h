/* missions.h -- missions. Notes: Research/notes/headers.md */
#ifndef MISSIONS_H
#define MISSIONS_H

#include "evglobals.h"

extern Boolean missionBit[NumMissionBits];

void InitMissions(void);
void DoMissionDialog(Boolean fromBar);
void SetupMissionList(void);
void SetupMissionInfoList(void);
void RandomizeMissionData(void);
void RandomizeOneMission(short mission);
void SelectMissionCargoQty(short *value);
short SelectMissionStellar(short type, short home);
short SelectMissionSystem(short type, short home);
void SetMissionDeadline(DateTimeRec *date, short days);
Boolean DoMissionAccept(short mission);
void LoadCurrentMissionData(short mission, short slot);
Boolean DoMissionCargoPickup(short cargoType, short quantity);
void DoMissionSuccess(short mission);
void DoMissionFailure(short mission);
void AbortMission(short mission);
void QuickMissionFailure(short mission);
pascal Boolean MissionFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void MissionDialogUpdate(void);
Boolean IsMissionAvailable(short mission);
void MissionHandler(void);
void MissionLandCheck(void);
void MissionLandCargoCheck(short mission);
void MissionObjectivesCheck(short mission);
void MungeBriefing(Boolean available, short mission);
void MungeForSymbol(char *find, char *replace);
void DoMissionInfoDialog(void);
pascal Boolean MissionInfoFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void MissionInfoDialogUpdate(void);
Boolean RunSingleMissionDialog(short mission);
pascal Boolean SingleMissionFilter(DialogPtr dlg, EventRecord *ev, short *itemHit);
void SingleMissionDialogUpdate(void);
Boolean ValidAuxShipSystem(short syst, short mission);
void AutoAbortMission(short mission);
Boolean SystemVisible(short syst);

#endif
