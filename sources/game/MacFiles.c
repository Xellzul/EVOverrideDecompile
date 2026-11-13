/* MacFiles.c -- pilot-file scrambling and resource-fork scanning (provisional name). Notes: Research/notes/MacFiles.md */
#include <Files.h>
#include <Aliases.h>
#include <Resources.h>
#include <Memory.h>
#include <Errors.h>
#include <MixedMode.h>
#include "evo.h"


#define kPilotKeyStep	0xDEADBEEF

// FUNCTION: EVO 0x100617C4
OSErr DoEncryption(Handle h, long key)
{
	unsigned long w;
	long k;
	unsigned long *p;
	long words;
	short rest;
	unsigned char *b;
	OSErr err;
	long size;

	err = 0;
	if (h == nil)
		err = nilHandleErr;
	else if (*h == nil)
		err = nilHandleErr;
	else {
		k = key;
		p = (unsigned long *)*h;
		size = GetHandleSize(h);
		words = size >> 2;
		rest = size - (words << 2);
		while (words--) {
			(void)words;
			w = *p ^ k;
			*p++ = w;
			k = (k + kPilotKeyStep) ^ kPilotKeyStep;
		}
		w = *p ^ k;
		b = (unsigned char *)p;
		while (rest--) {
			unsigned char c;

			c = (w & 0xFF000000) >> 24;
			*b++ = c;
			w <<= 8;
		}
	}
	return err;
}

// FUNCTION: EVO 0x1006189C
OSErr OpenResourceForksByType(short refNum, StringPtr fileName, OSType type)
{
	Str255 name;
	long dirID;
	short vRef;
	FSSpec spec;
	FInfo fndrInfo;
	Boolean isFolder, wasAliased;
	CInfoPBPtr pb;
	OSErr err;
	short resRef;
	OSErr ret;

	ret = 0;
	err = GetCatalogStartDir(refNum, fileName, &dirID, &vRef);
	if (err == 0) {
		name[0] = 0;
		pb = (CInfoPBPtr)NewPtrClear(sizeof(CInfoPBRec));
		if (pb != nil) {
			pb->dirInfo.ioFDirIndex = 0;
			pb->dirInfo.ioCompletion = nil;
			pb->dirInfo.ioNamePtr = name;
			pb->dirInfo.ioVRefNum = vRef;
			pb->dirInfo.ioDrDirID = dirID;
			while (err == 0) {
				pb->dirInfo.ioDrDirID = dirID;
				pb->dirInfo.ioCompletion = nil;
				pb->dirInfo.ioNamePtr = name;
				pb->dirInfo.ioFDirIndex++;
				err = PBGetCatInfoSync(pb);
				if (err == 0) {
					spec.vRefNum = pb->dirInfo.ioVRefNum;
					spec.parID = dirID;
					BlockMove(name, spec.name, name[0] + 1);
					err = ResolveAliasFile(&spec, true, &isFolder, &wasAliased);
					err = FSpGetFInfo(&spec, &fndrInfo);
					if (isFolder)
						err = 0;
					else if (fndrInfo.fdType == type)
						resRef = FSpOpenResFile(&spec, fsRdWrPerm);
				}
			}
			DisposePtr((Ptr)pb);
		} else
			ret = memFullErr;
	} else
		ret = err;
	return ret;
}

// FUNCTION: EVO 0x10061A28
OSErr GetCatalogStartDir(short refNum, StringPtr fileName, long *dirID, short *vRefNum)
{
	FCBPBRec fcb;
	Str255 name;
	Boolean wasAliased, isFolder;
	FSSpec spec;
	CInfoPBPtr pb;
	OSErr err;

	fcb.ioRefNum = refNum;
	fcb.ioNamePtr = name;
	fcb.ioFCBIndx = 0;
	err = PBGetFCBInfoSync(&fcb);
	if (err == 0) {
		name[0] = 0;
		pb = (CInfoPBPtr)NewPtrClear(sizeof(CInfoPBRec));
		if (pb != nil) {
			spec.vRefNum = fcb.ioFCBVRefNum;
			spec.parID = fcb.ioFCBParID;
			BlockMove(fileName, spec.name, fileName[0] + 1);
			BlockMove(fileName, name, fileName[0] + 1);
			err = ResolveAliasFile(&spec, true, &isFolder, &wasAliased);
			if (err == 0 && wasAliased) {
				pb->dirInfo.ioNamePtr = spec.name;
				pb->dirInfo.ioVRefNum = spec.vRefNum;
				pb->dirInfo.ioDrDirID = spec.parID;
			} else {
				pb->dirInfo.ioNamePtr = name;
				pb->dirInfo.ioVRefNum = fcb.ioFCBVRefNum;
				pb->dirInfo.ioDrDirID = fcb.ioFCBParID;
			}
			pb->dirInfo.ioFDirIndex = 0;
			pb->dirInfo.ioCompletion = nil;
			err = PBGetCatInfoSync(pb);
			if (err == 0) {
				*dirID = pb->dirInfo.ioDrDirID;
				*vRefNum = pb->dirInfo.ioVRefNum;
			}
			DisposePtr((Ptr)pb);
		}
	}
	return err;
}
