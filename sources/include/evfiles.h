/* evfiles.h -- data, plugin, pilot and prefs files. Notes: Research/notes/headers.md */
#ifndef EVFILES_H
#define EVFILES_H

#include "evglobals.h"

#define kMaxSheets			256

void InitDataFiles(void);
void OpenDataFiles(void);
void LoadObjectData(void);
void LoadDescription(char *text, short id);
void LoadStrings(void);
Boolean LoadPluginString(short id, Str255 str);
void LoadPrefs(void);
void SavePrefs(void);
short SavePilotFile(short stellar);
short WritePilotData(short stellar);
Boolean OpenPilotFile(void);
void OpenLastPilotFile(void);
OSErr LoadPilotData(short vRefNum, long dirID, StringPtr fileName);
OSErr LoadOldPilotData(short vRefNum, long dirID, StringPtr fileName);
Boolean DoesPilotExist(StringPtr name);
void DeletePilotFile(StringPtr name);
void MakeAlias(FSSpec target);
void LoadSprites(void);
void LoadSpriteSheet(short pictID, short maskID, short width, short height, short xTiles, short yTiles);
void NewSheetFace(FacePtr *face, short width, short height, short x, short y);
Boolean EVBugResourceFieldTest(short field);
OSErr ResolveMacFileAlias(short refNum, StringPtr name, long *dirID, short *vRefNum);
short ReadStoredVersionStamp(void);

#endif
