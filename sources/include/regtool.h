/* regtool.h -- registration. Notes: Research/notes/headers.md */
#ifndef REGTOOL_H
#define REGTOOL_H

#include "evglobals.h"

#pragma options align=mac68k

typedef struct RegRecord {
	unsigned char	owner[256];
	unsigned long	firstDate;
	unsigned long	seconds;
	unsigned long	launches;
	unsigned long	spare[4];
} RegRecord;

#pragma options align=reset

OSErr InitShareWareRegistrationSession(Boolean trackTime, short level);
void CloseShareWareRegistrationSession(void);
OSErr CheckShareWareRegistrationMatch(Boolean *match);
OSErr ShowNagDialog(Boolean first);
OSErr GetRegisteredOwnerName(StringPtr name);
OSErr GetDaysUsed(long *days);
OSErr GetRegistrationCode(long *code);
OSErr GetHoursUsed(long *hours);
short LoadOrInitPilotPrefsRecord(RegRecord *rec);
void WritePilotRecordToPrefsFile(RegRecord *rec);
Handle LoadRegistrationRecord(short id);
void DecodeRegistrationRecord(Handle h, short id);
void RotateRight(unsigned long *v, short n);
void RotateLeft(unsigned long *v, short n);
void UpperCasePString(StringPtr s);

#endif
