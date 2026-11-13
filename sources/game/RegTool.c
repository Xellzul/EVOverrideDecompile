/* RegTool.c -- the shareware registration session: prefs record, owner name, nag dialog, styled text (provisional name). Notes: Research/notes/RegTool.md */
// COMPILER: CWPro1
// COMPILE: -opt l4,noschedule
// COMPILE-WHY: separately built library part, the later (UI 2.1.4) build: CW Pro 1 MWCPPC 1.8 (1.7 keeps LoadStyledTextResource's exit codes in err's register): Research/notes/RegTool.md (FABLE-REG)
#include <Types.h>
#include <Memory.h>
#include <Resources.h>
#include <TextUtils.h>
#include <ToolUtils.h>
#include <OSUtils.h>
#include <Dialogs.h>
#include <Windows.h>
#include <TextEdit.h>
#include <Files.h>
#include <Finder.h>
#include <Folders.h>
#include <Processes.h>
#include <SegLoad.h>
#include <Notification.h>
#include <Timer.h>
#include <Sound.h>
#include <MixedMode.h>
#include <Traps.h>

#include "evo.h"

#if GENERATING68K
#define ENTER_CALLBACK()	long oldA5 = SetCurrentA5()
#define EXIT_CALLBACK()		SetA5(oldA5)
#else
#define ENTER_CALLBACK()	long oldA5 = 0
#define EXIT_CALLBACK()
#endif

#define kRegistrationID		900
#define kNagTextID			901
#define kNagAlertID			901
#define kRegStateErr		(-1001)
#define kRegStringsErr		(-1200)
#define kTicksPerSecond		60
#define kSecondsPerHour		3600
#define kSecondsPerDay		86400
#define kTrialDays			90
#define kMaxSearchMatches	20
#ifndef kEnterCharCode
#define kEnterCharCode		0x03
#define kReturnCharCode		0x0D
#endif

#pragma options align=mac68k

typedef struct PilotRec {
	unsigned char	owner[256];
	unsigned long	firstDate;
	unsigned long	seconds;
	unsigned long	launches;
	unsigned long	spare[4];
} PilotRec;

typedef struct LicenseRec {
	Str255			name;
	Str255			code;
	short			copies;
} LicenseRec;

typedef struct NagState {
	long		*windowCount;
	NMRec		nm;
	unsigned char	pad1[261];
	Boolean		shown;
	WindowPtr	window;
	Handle		hStyle;
	Handle		hText;
	unsigned char	pad2[36];
	SndChannelPtr	chan;
	UniversalProcPtr	chanUPP;
	long		chanArg;
	UniversalProcPtr	extraUPP;
	unsigned char	pad3[1024];
} NagState;

typedef struct TextItem {
	struct TextItem	*prev;
	struct TextItem	*next;
	TEHandle	te;
} TextItem;

#pragma options align=reset

typedef void (*TextSubstProc)(Str255 s, short which);

unsigned short	nagLevel = 1;
static TextItem		*textList;
#if GENERATINGPOWERPC
static UniversalProcPtr	callUPPTrap;
#endif

static NagState nag;
Boolean regReady;
unsigned char regStr1[256];
unsigned char regStr4[256];
unsigned long regSessionStart;
PilotRec regRec;
static Boolean		regNagPending;
static Boolean		regTrackTime;

static OSErr ReadPilotPrefs(PilotRec *rec);
static OSErr WritePilotPrefs(PilotRec *rec);
static Ptr GenerateRegistrationCode(StringPtr name, int n, short id);
static void CloseSlideShowWindow(WindowPtr w);
static OSErr OpenPrefsFile(ConstStr255Param name, SInt8 perm, short *refNum);
static Handle DefaultRegistrationRecord(short id);
#if GENERATINGPOWERPC
void CallWindowUPP(WindowPtr w);
#else
pascal short CallWindowUPP(WindowPtr w) THREEWORDINLINE(0x3F3C, 0x005B, 0xA88F);
#endif
Handle LoadRegistrationRecord(short id);
OSErr SaveRegistrationRecord(Handle h, short id);
OSErr GetDaysUsed(long *days);
OSErr GetHoursUsed(long *hours);
OSErr GetRegistrationCode(long *code);
OSErr LaunchApplicationByFSSpec(StringPtr name, Boolean quick);
short EscalationLevel(void);
void ZoomInWindowAnimation(WindowPtr w);
OSErr LoadStyledTextResource(short id, Rect *box, TextSubstProc subst);
void DisposeAllTextEditList(void);
void UpdateAllTextEdits(void);
OSErr RemoveNagNotification(void);
pascal OSErr NameFileSearch(ConstStr255Param volName, short vRefNum, ConstStr255Param name, FSSpec *matches, long reqCount, long *actCount, Boolean newSearch, Boolean partial);
OSErr InstallCallbackPtr(Ptr p, long proc);
static void SubstText(Str255 out, short which);
static pascal Boolean NagFilter(DialogPtr d, EventRecord *e, short *item);
static pascal void UpdateTextItem(DialogPtr d, short item);
static pascal void DrawDefaultRing(DialogPtr d, short item);

// FUNCTION: EVO 0x1007186C
OSErr InitShareWareRegistrationSession(Boolean trackTime, short level)
{
	OSErr err;

	err = 0;
	if (regReady)
		return kRegStateErr;
	nagLevel = level;
	if (nagLevel < 1)
		nagLevel = 1;
	if (nagLevel > 2)
		nagLevel = 2;
	GetIndString(regStr1, kRegistrationID, 1);
	if (regStr1[0] == 0)
		return kRegStringsErr;
	GetIndString(regStr4, kRegistrationID, 4);
	if (regStr4[0] == 0)
		return kRegStringsErr;
	regTrackTime = trackTime;
	if (trackTime)
		GetDateTime(&regSessionStart);
	err = ReadPilotPrefs(&regRec);
	if (err == 0)
		regReady = true;
	return err;
}

// FUNCTION: EVO 0x1007197C
void CloseShareWareRegistrationSession(void)
{
	unsigned long now;

	if (regReady) {
		(void)nag;
		if (regNagPending)
			RemoveNagNotification();
		if (regTrackTime)
			regRec.launches++;
		if (regTrackTime && regSessionStart != 0) {
			GetDateTime(&now);
			regRec.seconds += now - regSessionStart;
		}
		WritePilotPrefs(&regRec);
		regReady = false;
	}
}

// FUNCTION: EVO 0x10071A40
OSErr CheckShareWareRegistrationMatch(Boolean *match)
{
	Handle h;
	Ptr code;
	OSErr err;

	err = 0;
	*match = false;
	if (regReady) {
		h = LoadRegistrationRecord(kRegistrationID);
		SaveRegistrationRecord(h, kRegistrationID);
		HLockHi(h);
		code = GenerateRegistrationCode(((LicenseRec *)*h)->name, ((LicenseRec *)*h)->copies, kRegistrationID);
		*match = EqualString(((LicenseRec *)*h)->code, (StringPtr)code, true, true);
		DisposePtr(code);
		HUnlock(h);
		DisposeHandle(h);
	} else
		err = kRegStateErr;
	return err;
}

// FUNCTION: EVO 0x10071B24
OSErr ShowNagDialog(Boolean first)
{
	OSErr err;
	OSErr result;
	UserItemUPP ringUPP;
	UserItemUPP textUPP;
	DialogPtr d;
	Handle rsrc;
	Boolean done;
	ModalFilterUPP filterUPP;
	short level;
	Boolean match;
	long days;
	short item;
	Handle h;
	short type;
	Rect box;
	GrafPtr savePort;

	err = 0;
	if (regReady) {
		days = 7;
		result = CheckShareWareRegistrationMatch(&match);
		result = GetDaysUsed(&days);
		if ((match || days < 7) && !first)
			goto done;
		done = false;
		level = 0;
		level = EscalationLevel();
		ringUPP = NewUserItemProc(DrawDefaultRing);
		textUPP = NewUserItemProc(UpdateTextItem);
		filterUPP = NewModalFilterProc(NagFilter);
		InitCursor();
		rsrc = GetResource('DLOG', kRegistrationID);
		if (rsrc == 0)
			ExitToShell();
		rsrc = GetResource('DITL', kRegistrationID);
		if (rsrc == 0)
			ExitToShell();
		d = GetNewDialog(kRegistrationID, 0, (WindowPtr)-1);
		GetDialogItem(d, 2, &type, &h, &box);
		HiliteControl((ControlHandle)h, kControlInactivePart);
		GetDialogItem(d, 3, &type, &h, &box);
		SetDialogItem(d, 3, type, (Handle)ringUPP, &box);
		GetDialogItem(d, 5, &type, &h, &box);
		SetDialogItem(d, 5, type, (Handle)textUPP, &box);
		GetPort(&savePort);
		SetPort(d);
		result = LoadStyledTextResource(kRegistrationID, &box, SubstText);
		if (level > 0) {
			GetDialogItem(d, 6, &type, &h, &box);
			result = LoadStyledTextResource(kNagTextID, &box, SubstText);
		}
		SetPort(savePort);
		ZoomInWindowAnimation(d);
		SetWRefCon(d, TickCount() + level * 5L * kTicksPerSecond);
		while (!done) {
			ModalDialog(filterUPP, &item);
			switch (item) {
			case 1:
			case 2:
				done = true;
				break;
			}
		}
		HideWindow(d);
		DisposeAllTextEditList();
		DisposeRoutineDescriptor(filterUPP);
		DisposeDialog(d);
		DisposeRoutineDescriptor(ringUPP);
		DisposeRoutineDescriptor(textUPP);
		if (item == 1) {
			short i = 0;
			Str255 appName = "\pRegister ";
			Str255 msg;
			Str255 msg2;
			long ticks;

			GetIndString(msg, kRegistrationID, 1);
			BlockMove(&msg[1], appName + appName[0] + 1, msg[0]);
			appName[0] += msg[0];
			result = LaunchApplicationByFSSpec(appName, true);
			if (result != 0) {
				for (i = 0; i < 5; i++) {
					SysBeep(1);
					Delay(kTicksPerSecond, &ticks);
				}
				InitCursor();
				GetIndString(msg2, kRegistrationID, 4);
				ParamText(msg2, appName, msg2, msg2);
				result = Alert(kNagAlertID, 0);
				if (result == -1)
					ExitToShell();
			}
		}
	} else
		err = kRegStateErr;
done:
	return err;
}

// FUNCTION: EVO 0x10071F3C
OSErr GetRegisteredOwnerName(StringPtr name)
{
	OSErr err;
	Handle h;
	Boolean match;
	OSErr checkErr;

	err = 0;
	if (regReady) {
		checkErr = CheckShareWareRegistrationMatch(&match);
		if (match) {
			h = LoadRegistrationRecord(kRegistrationID);
			SaveRegistrationRecord(h, kRegistrationID);
			BlockMoveData(*h, name, sizeof(Str255));
			HUnlock(h);
			DisposeHandle(h);
		} else
			GetIndString(name, kRegistrationID, 2);
	} else
		err = kRegStateErr;
	return err;
}

// FUNCTION: EVO 0x10072008
OSErr GetDaysUsed(long *days)
{
	OSErr err;
	unsigned long now;

	err = 0;
	if (regReady) {
		GetDateTime(&now);
		*days = (now - regRec.firstDate) / kSecondsPerDay + 1;
		if (now < regRec.firstDate)
			*days = kTrialDays;
	} else
		err = kRegStateErr;
	return err;
}

// FUNCTION: EVO 0x100720A8
OSErr GetRegistrationCode(long *code)
{
	OSErr err;

	err = 0;
	if (regReady)
		*code = regRec.launches;
	else
		err = kRegStateErr;
	return err;
}

// FUNCTION: EVO 0x100720D8
OSErr GetHoursUsed(long *hours)
{
	OSErr err;
	unsigned long now;

	err = 0;
	if (regReady) {
		GetDateTime(&now);
		*hours = regRec.seconds / kSecondsPerHour;
	} else
		err = kRegStateErr;
	return err;
}

// FUNCTION: EVO 0x10072148
OSErr LaunchApplicationByFSSpec(StringPtr name, Boolean quick)
{
	OSErr err;
	short i;
	short best;
	CursHandle watch;
	LaunchParamBlockRec lp;
	short vRef;
	long dirID;
	FSSpec spec;
	long count;
	FSSpec specs[kMaxSearchMatches];
	FInfo fi;

	err = 0;
	if (quick) {
		err = HGetVol(0, &vRef, &dirID);
		if (err == 0) {
			err = FSMakeFSSpec(vRef, dirID, name, &spec);
			lp.launchBlockID = extendedBlock;
			lp.launchEPBLength = extendedBlockLen;
			lp.launchFileFlags = 0;
			lp.launchControlFlags = launchContinue | launchNoFileFlags;
			lp.launchAppSpec = &spec;
			lp.launchAppParameters = 0;
			err = LaunchApplication(&lp);
		}
	}
	if (!quick || err != 0) {
		count = 0;
		watch = GetCursor(watchCursor);
		if (watch != 0)
			SetCursor(*watch);
		err = NameFileSearch(0, -1, name, specs, kMaxSearchMatches, &count, 1, 0);
		InitCursor();
		if (count > 0) {
			best = -1;
			for (i = 0; i < count; i++) {
				FSpGetFInfo(&specs[i], &fi);
				if (fi.fdType == 'APPL' && fi.fdCreator == 'Areg' && (fi.fdFlags & kIsAlias) == 0)
					best = i;
			}
			if (best != -1) {
				lp.launchBlockID = extendedBlock;
				lp.launchEPBLength = extendedBlockLen;
				lp.launchFileFlags = 0;
				lp.launchControlFlags = launchContinue | launchNoFileFlags;
				lp.launchAppSpec = &specs[best];
				lp.launchAppParameters = 0;
				err = LaunchApplication(&lp);
			}
		}
	}
	return err;
}

// FUNCTION: EVO 0x10072344
static OSErr ReadPilotPrefs(PilotRec *rec)
{
	OSErr err;
	Boolean needInit;
	Boolean found;
	PilotRec buf;
	short vRef;
	long dirID;
	short ref;
	long count;
	FInfo fi;

	err = 0;
	ref = -1;
	needInit = false;
	err = FindFolder(kOnSystemDisk, 'pref', true, &vRef, &dirID);
	if (err == 0) {
		err = HOpen(vRef, dirID, "\pthaumaturgy.log", fsRdWrPerm, &ref);
		if (err == 0) {
			found = false;
			err = HGetFInfo(vRef, dirID, "\pthaumaturgy.log", &fi);
			fi.fdFlags |= kIsInvisible;
			err = HSetFInfo(vRef, dirID, "\pthaumaturgy.log", &fi);
			err = SetFPos(ref, fsFromStart, 0);
			while (!found && err == 0) {
				count = sizeof(PilotRec);
				err = FSRead(ref, &count, &buf);
				if (err == 0) {
					if (EqualString(regStr1, buf.owner, true, true))
						found = true;
				}
			}
			if (found)
				BlockMoveData(&buf, rec, sizeof(PilotRec));
			else
				needInit = true;
			err = FSClose(ref);
		} else
			needInit = true;
	}
	if (needInit) {
		BlockMoveData(regStr1, rec, sizeof(rec->owner));
		GetDateTime(&rec->firstDate);
		rec->seconds = 0;
		rec->launches = 0;
		rec->spare[0] = 0;
		rec->spare[1] = 0;
		rec->spare[2] = 0;
		rec->spare[3] = 0;
		err = WritePilotPrefs(rec);
	}
	return err;
}

// FUNCTION: EVO 0x10072520
static OSErr WritePilotPrefs(PilotRec *rec)
{
	OSErr err;
	Boolean found;
	PilotRec buf;
	short vRef;
	long dirID;
	short ref;
	long count;
	FInfo fi;

	err = 0;
	ref = -1;
	err = FindFolder(kOnSystemDisk, 'pref', true, &vRef, &dirID);
	if (err == 0) {
		err = HOpen(vRef, dirID, "\pthaumaturgy.log", fsRdWrPerm, &ref);
		if (err == fnfErr) {
			err = HCreate(vRef, dirID, "\pthaumaturgy.log", '????', '????');
			err = HOpen(vRef, dirID, "\pthaumaturgy.log", fsRdWrPerm, &ref);
		}
		if (err == 0) {
			found = false;
			err = HGetFInfo(vRef, dirID, "\pthaumaturgy.log", &fi);
			fi.fdFlags |= kIsInvisible;
			err = HSetFInfo(vRef, dirID, "\pthaumaturgy.log", &fi);
			err = SetFPos(ref, fsFromStart, 0);
			while (!found && err == 0) {
				count = sizeof(PilotRec);
				err = FSRead(ref, &count, &buf);
				if (err == 0) {
					if (EqualString(regStr1, buf.owner, true, true))
						found = true;
				}
			}
			if (found)
				err = SetFPos(ref, fsFromMark, -sizeof(PilotRec));
			count = sizeof(PilotRec);
			err = FSWrite(ref, &count, rec);
			err = FSClose(ref);
		}
	}
	return err;
}

// FUNCTION: EVO 0x10072700
static Handle DefaultRegistrationRecord(short id)
{
	OSErr err;
	Handle h;

	err = 0;
	h = 0;
	h = NewHandleClear(sizeof(LicenseRec));
	if (h == 0 || MemError() != 0)
		return h;
	HLockHi(h);
	GetIndString(((LicenseRec *)*h)->name, id, 2);
	((LicenseRec *)*h)->copies = 0;
	GetIndString(((LicenseRec *)*h)->code, id, 3);
	HUnlock(h);
	return h;
}

// FUNCTION: EVO 0x100727BC
Handle LoadRegistrationRecord(short id)
{
	OSErr err = 0;
	short ref = -1;
	Handle h = 0;
	long eof = 0;
	long count = sizeof(LicenseRec);
	Str255 license = "\p License";
	Str255 s;
	Str255 name;
	Ptr src;
	Ptr dst;

	GetIndString(s, id, 1);
	BlockMove(s, name, sizeof(Str255));
	src = (Ptr)&license[1];
	dst = (Ptr)&name[name[0] + 1];
	BlockMove(src, dst, license[0]);
	name[0] += license[0];
	err = OpenPrefsFile(name, 1, &ref);
	if (err != 0)
		return h;
	err = GetEOF(ref, &eof);
	if (err != 0)
		return h;
	if (eof == 0)
		h = DefaultRegistrationRecord(id);
	else {
		h = NewHandleClear(count);
		if (h == 0 || MemError() != 0)
			return h;
		HLock(h);
		err = FSRead(ref, &count, *h);
		HUnlock(h);
		if (err != 0) {
			DisposeHandle(h);
			h = 0;
		}
	}
	err = FSClose(ref);
	return h;
}

// FUNCTION: EVO 0x10072964
OSErr SaveRegistrationRecord(Handle h, short id)
{
	OSErr err = 0;
	short ref = -1;
	long count = GetHandleSize(h);
	Str255 license = "\p License";
	Str255 s;
	Str255 name;
	Ptr src;
	Ptr dst;

	GetIndString(s, id, 1);
	BlockMove(s, name, sizeof(Str255));
	src = (Ptr)&license[1];
	dst = (Ptr)&name[name[0] + 1];
	BlockMove(src, dst, license[0]);
	name[0] += license[0];
	err = OpenPrefsFile(name, 3, &ref);
	if (err != 0)
		return err;
	err = SetFPos(ref, fsFromStart, 0);
	if (err != 0) {
		FSClose(ref);
		return err;
	}
	HLock(h);
	err = FSWrite(ref, &count, *h);
	HUnlock(h);
	if (err != 0) {
		FlushVol(0, 0);
		FSClose(ref);
		return err;
	}
	err = FlushVol(0, 0);
	if (err != 0)
		return err;
	err = FSClose(ref);
	return err;
}

// FUNCTION: EVO 0x10072B0C
static OSErr OpenPrefsFile(ConstStr255Param name, SInt8 perm, short *refNum)
{
	OSErr err;
	long size;
	short vRef;
	long dirID;
	short ref;

	err = 0;
	ref = -1;
	*refNum = ref;
	err = FindFolder(kOnSystemDisk, 'pref', true, &vRef, &dirID);
	if (err != 0)
		return err;
	err = HOpen(vRef, dirID, name, perm, &ref);
	if (err == fnfErr) {
		err = HCreate(vRef, dirID, name, 'Areg', 'Alic');
		if (err != 0)
			return err;
		err = HOpen(vRef, dirID, name, perm, &ref);
		if (err != 0)
			return err;
	} else if (err != 0)
		return err;
	*refNum = ref;
	return err;
}

// FUNCTION: EVO 0x10072C1C
void RotateRight(unsigned long *v, short n)
{
	unsigned long t;

	t = *v << (32 - n);
	*v = *v >> n;
	*v |= t;
}

// FUNCTION: EVO 0x10072C44
void RotateLeft(unsigned long *v, short n)
{
	unsigned long t;

	t = *v >> (32 - n);
	*v = *v << n;
	*v |= t;
}

// FUNCTION: EVO 0x10072C6C
void UpperCasePString(StringPtr s)
{
	short i;

	for (i = s[0] + 1; i > 0; i--)
		if (s[i] >= 'a' && s[i] <= 'z')
			s[i] -= 'a' - 'A';
}

// FUNCTION: EVO 0x10072CA8
static void ScrambleString(unsigned long *v, StringPtr s, int n)
{
	short len;
	short i;
	unsigned char b;
	unsigned short w;

	len = s[0];
	for (i = 1; i <= len; i++) {
		switch (nagLevel) {
		case 1:
			b = *v & 0xFF;
			b += s[i];
			*v = (*v & 0xFFFFFF00) | b;
			RotateLeft(v, 5);
			w = *v & 0xFFFF;
			w += n;
			*v = (*v & 0xFFFF0000) | w;
			break;
		case 2:
			b = *v & 0xFF;
			b += s[i];
			*v = (*v & 0xFFFFFF00) ^ b;
			RotateLeft(v, 6);
			w = *v & 0xFFFF;
			w += n;
			*v = (*v & 0xFFFF0000) | w;
			*v ^= 0xDEADBEEF;
			RotateRight(v, 1);
			break;
		}
	}
}

// FUNCTION: EVO 0x10072DD0
static Ptr GenerateRegistrationCode(StringPtr name, int n, short id)
{
	Str255 key;
	unsigned long st;
	Ptr p;
	short i;
	long c;

	GetIndString(key, id, 1);
	p = NewPtr(sizeof(Str255));
	if (p != 0) {
		UpperCasePString(name);
		st = 0;
		ScrambleString(&st, name, n);
		ScrambleString(&st, key, n);
		p[0] = 8;
		for (i = 1; i <= 8; i++) {
			c = st & 0xF;
			RotateRight(&st, 4);
			c += 'A';
			p[i] = c;
		}
	}
	return p;
}

// FUNCTION: EVO 0x10072EAC
static void SubstText(Str255 out, short which)
{
	Str255 s;

	switch (which) {
	case 1:
		GetIndString(s, kRegistrationID, 4);
		break;
	case 2: {
		long days = 0;
		GetDaysUsed(&days);
		NumToString(days, s);
		break;
	}
	case 3: {
		long hours = 0;
		GetHoursUsed(&hours);
		NumToString(hours, s);
		break;
	}
	case 4: {
		long days = 0;
		s[0] = 0;
		GetDaysUsed(&days);
		if (days != 1) {
			s[0]++;
			s[1] = 's';
		}
		break;
	}
	case 5: {
		long hours = 0;
		s[0] = 0;
		GetHoursUsed(&hours);
		if (hours != 1) {
			s[0]++;
			s[1] = 's';
		}
		break;
	}
	case 6: {
		long code = 0;
		GetRegistrationCode(&code);
		NumToString(code, s);
		break;
	}
	case 7: {
		long code = 0;
		s[0] = 0;
		GetRegistrationCode(&code);
		if (code != 1) {
			s[0]++;
			s[1] = 's';
		}
		break;
	}
	}
	BlockMoveData(s, out, sizeof(Str255));
}

// FUNCTION: EVO 0x10073030
void ZoomInWindowAnimation(WindowPtr w)
{
	GrafPtr savePort;
	Rect r;
	Rect z;
	Point p;
	CGrafPtr wmPort;
	int dv;
	int dh;
	short i;
	long ticks;

	GetPort(&savePort);
	r = w->portRect;
	SetPort(w);
	p.h = r.left;
	p.v = r.top;
	LocalToGlobal(&p);
	r.left = p.h;
	r.top = p.v;
	p.h = r.right;
	p.v = r.bottom;
	LocalToGlobal(&p);
	r.right = p.h;
	r.bottom = p.v;
	GetCWMgrPort(&wmPort);
	SetPort((GrafPtr)wmPort);
	PenNormal();
	ClipRect(&(*GetGrayRgn())->rgnBBox);
	dv = (r.right - r.left) / 16 / 2;
	dh = (r.bottom - r.top) / 16 / 2;
	z = r;
	InsetRect(&z, dv * 16, dh * 16);
	for (i = 0; i <= 16; i++) {
		Delay(1, &ticks);
		FrameRect(&z);
		InsetRect(&z, 1, 1);
		EraseRect(&z);
		InsetRect(&z, -1, -1);
		InsetRect(&z, -dv, -dh);
	}
	ShowWindow(w);
	SetPort(savePort);
}

// FUNCTION: EVO 0x10073218
short EscalationLevel(void)
{
	short level;
	long days;
	long hours;

	level = 0;
	GetDaysUsed(&days);
	GetHoursUsed(&hours);
	if (days > 30 && hours > 6)
		level++;
	if (days > 60 && hours > 12)
		level++;
	return level;
}

// FUNCTION: EVO 0x1007328C
static pascal Boolean NagFilter(DialogPtr d, EventRecord *e, short *item)
{
	ENTER_CALLBACK();
	Boolean result;
	long ref;
	Handle h;
	short type;
	Rect box;
	Boolean cmd;
	char c;
	long ticks;
	ControlHandle ctl;

	result = false;
	ref = GetWRefCon(d);
	if (ref != 0 && TickCount() - ref > 300 && TickCount() > ref) {
		GetDialogItem(d, 2, &type, &h, &box);
		HiliteControl((ControlHandle)h, 0);
		SetWRefCon(d, 0);
	}
	if (BitAnd(e->modifiers, cmdKey) != 0)
		cmd = true;
	else
		cmd = false;
	if ((e->what == keyDown || e->what == autoKey) && !cmd) {
		c = BitAnd(e->message, charCodeMask);
		if (c == kEnterCharCode || c == kReturnCharCode) {
			*item = 1;
			result = true;
			GetDialogItem(d, 1, &type, &h, &box);
			ctl = (ControlHandle)h;
			HiliteControl((ControlHandle)h, 1);
			Delay(8, &ticks);
			HiliteControl((ControlHandle)h, 0);
		}
	}
	EXIT_CALLBACK();
	return result;
}
// FUNCTION: EVO 0x100733F4
static pascal void UpdateTextItem(DialogPtr d, short item)
{
	ENTER_CALLBACK();

	UpdateAllTextEdits();
	EXIT_CALLBACK();
}

// FUNCTION: EVO 0x10073418
static pascal void DrawDefaultRing(DialogPtr d, short item)
{
	Rect box;
	short type;
	Handle h;

	PenNormal();
	GetDialogItem(d, item, &type, &h, &box);
	PenSize(3, 3);
	FrameRoundRect(&box, 16, 16);
	PenSize(1, 1);
}

// FUNCTION: EVO 0x100734A4
OSErr RemoveNagNotification(void)
{
	if (nag.window != 0)
		CloseSlideShowWindow(nag.window);
	else if (nag.nm.qType != 0)
		NMRemove(&nag.nm);
	return 0;
}

// FUNCTION: EVO 0x100734F4
static void CloseSlideShowWindow(WindowPtr w)
{
	if (w == nag.window) {
		nag.shown = false;
		if (nag.window != 0) {
			DisposeAllTextEditList();
			ShowHide(nag.window, false);
			CallWindowUPP(nag.window);
			nag.window = 0;
			(*nag.windowCount)--;
		}
		if (nag.chan != 0) {
			InstallCallbackPtr((Ptr)nag.chan, nag.chanArg);
			nag.chan = 0;
		}
		if (nag.chanUPP != 0) {
			DisposeRoutineDescriptor(nag.chanUPP);
			nag.chanUPP = 0;
		}
		if (nag.extraUPP != 0) {
			DisposeRoutineDescriptor(nag.extraUPP);
			nag.extraUPP = 0;
		}
		if (nag.hText != 0) {
			DisposeHandle(nag.hText);
			nag.hText = 0;
		}
		if (nag.hStyle != 0) {
			DisposeHandle(nag.hStyle);
			nag.hStyle = 0;
		}
	}
}

#if GENERATINGPOWERPC
// FUNCTION: EVO 0x1007360C
static UniversalProcPtr GetCallUPPTrap(void)
{
	if (callUPPTrap == 0)
		callUPPTrap = (UniversalProcPtr)NGetTrapAddress(_OSDispatch, ToolTrap);
	return callUPPTrap;
}

// FUNCTION: EVO 0x10073650
void CallWindowUPP(WindowPtr w)
{
	CallUniversalProc(GetCallUPPTrap(), 0x2E0, w, 0x5B);
}
#endif

// FUNCTION: EVO 0x10073690
OSErr LoadStyledTextResource(short id, Rect *box, TextSubstProc subst)
{
	OSErr err;
	TextItem *item;
	Handle txt;
	long len;
	Handle style;
	short i;
	Handle hText;
	TextItem *p;
	short which;
	Rect r;
	Str255 s;

	err = 0;
	item = (TextItem *)NewPtr(sizeof(TextItem));
	if (item == 0)
		err = memFullErr;
	else {
		txt = GetResource('TEXT', id);
		if (txt == 0) {
			DisposePtr((Ptr)item);
			err = resNotFound;
			return err;
		}
		HNoPurge(txt);
		HLockHi(txt);
		len = GetHandleSize(txt);
		r = *box;
		OffsetRect(&r, 30000, 30000);
		item->te = TEStyleNew(&r, &r);
		if (item == 0) {
			HUnlock(txt);
			HPurge(txt);
			DisposePtr((Ptr)item);
			err = memFullErr;
			return err;
		}
		style = GetResource('styl', id);
		if (style == 0) {
			TEDispose(item->te);
			HUnlock(txt);
			HPurge(txt);
			DisposePtr((Ptr)item);
			err = resNotFound;
			return err;
		}
		HNoPurge(style);
		TEActivate(item->te);
		TEStyleInsert(*txt, len, (StScrpHandle)style, item->te);
		TECalText(item->te);
		HPurge(style);
		HUnlock(txt);
		HPurge(txt);
		i = 0;
		hText = (**item->te).hText;
		while (i < (**item->te).teLength - 1) {
			if ((*hText)[i] == '^' && (*hText)[i + 1] >= '0' && (*hText)[i + 1] <= '9') {
				which = (*hText)[i + 1] - '0' + 1;
				if (subst != 0)
					subst(s, which);
				TESetSelect(i, i + 2, item->te);
				TEDelete(item->te);
				TEInsert(&s[1], s[0], item->te);
			} else
				i++;
		}
		TEDeactivate(item->te);
		(**item->te).destRect = (**item->te).viewRect = *box;
		TECalText(item->te);
		TEUpdate(box, item->te);
		p = textList;
		if (p == 0) {
			textList = item;
			item->prev = 0;
			item->next = 0;
		} else {
			while (p != 0) {
				if (p->next == 0) {
					p->next = item;
					item->prev = p;
					item->next = 0;
					p = 0;
				} else
					p = p->next;
			}
		}
	}
	return err;
}

// FUNCTION: EVO 0x100739CC
void UpdateAllTextEdits(void)
{
	GrafPtr savePort;
	TextItem *p;
	Rect r;
	GrafPtr port;

	GetPort(&savePort);
	for (p = textList; p != 0; p = p->next) {
		r = (**p->te).destRect;
		port = (**p->te).inPort;
		SetPort(port);
		TEUpdate(&r, p->te);
	}
	SetPort(savePort);
}

// FUNCTION: EVO 0x10073A58
void DisposeAllTextEditList(void)
{
	GrafPtr savePort;
	TextItem *p;
	Rect r;
	GrafPtr port;
	TextItem *next;

	GetPort(&savePort);
	for (p = textList; p != 0; p = next) {
		next = p->next;
		r = (**p->te).destRect;
		port = (**p->te).inPort;
		SetPort(port);
		InvalRect(&r);
		TEDispose(p->te);
		DisposePtr((Ptr)p);
	}
	textList = 0;
	SetPort(savePort);
}
