/* Search.c -- catalog search with a software fallback, probably Jim Luther's MoreFiles Search.c (provisional name). Notes: Research/notes/Search.md */
// COMPILER: CWPro1
// COMPILE: -opt l4,noschedule
// COMPILE-WHY: separately built library part, the later (UI 2.1.4) build: CW11 Gold MWCPPC 1.7: Research/notes/library-compiler.md (LIB9)
#include <Types.h>
#include <Memory.h>
#include <Files.h>
#include <Errors.h>
#include <Gestalt.h>
#include <Timer.h>
#include <TextUtils.h>
#include <MixedMode.h>

enum {
	kAdditionalLevelRecs = 20
};

#pragma options align=mac68k

typedef struct LevelRec {
	long		dirModDate;
	long		dirID;
	short		index;
} LevelRec, *LevelRecPtr, **LevelRecHandle;

typedef struct SearchPositionRec {
	long			initialize;
	unsigned short	stackDepth;
	short			priv[5];
} SearchPositionRec;

typedef struct ExtendedTMTask {
	TMTask		theTask;
	Boolean		stopSearch;
} ExtendedTMTask;

pascal OSErr GetVolumeInfoNoName(ConstStr255Param pathname, short vRefNum, HParmBlkPtr pb);
pascal OSErr DetermineVRefNum(ConstStr255Param pathname, short vRefNum, short *realVRefNum);
pascal OSErr HGetVolParms(ConstStr255Param volName, short vRefNum, GetVolParmsInfoBuffer *volParmsInfo, long *infoSize);
pascal Ptr GetTempBuffer(long buffReqSize, long *buffActSize);

static LevelRecHandle	gStack;
static Size				gStackSize;
static Boolean			gChecked;
static Boolean			gHasExtFS;
static CatPositionRec	gCatPosition;
static short			gLastVRefNum;

// FUNCTION: EVO 0x10073CE4
OSErr CheckVol(ConstStr255Param pathname, short vRefNum, short *realVRefNum, long *volID)
{
	HParamBlockRec pb;
	OSErr error;

	error = GetVolumeInfoNoName(pathname, vRefNum, &pb);
	if (error == noErr) {
		*realVRefNum = pb.volumeParam.ioVRefNum;
		*volID = pb.volumeParam.ioVCrDate + pb.volumeParam.ioVRefNum + pb.volumeParam.ioVNmAlBlks + pb.volumeParam.ioVAlBlkSiz + pb.volumeParam.ioVFSID;
	}
	return error;
}

// FUNCTION: EVO 0x10073D5C
OSErr CheckStack(unsigned short stackDepth, LevelRecHandle searchStack, Size *searchStackSize)
{
	OSErr result;

	if (*searchStackSize / sizeof(LevelRec) == stackDepth + 1) {
		SetHandleSize((Handle)searchStack, *searchStackSize + kAdditionalLevelRecs * sizeof(LevelRec));
		result = MemError();
		*searchStackSize = GetHandleSize((Handle)searchStack);
	} else
		result = noErr;
	return result;
}

// FUNCTION: EVO 0x10073DEC
OSErr VerifyUserPB(CSParamPtr userPB, Boolean *includeFiles, Boolean *includeDirs, Boolean *includeNames)
{
	CInfoPBPtr searchInfo1, searchInfo2;

	searchInfo1 = userPB->ioSearchInfo1;
	searchInfo2 = userPB->ioSearchInfo2;
	if (userPB->ioMatchPtr == nil || searchInfo1 == nil)
		goto ParamErrExit;
	if ((userPB->ioSearchBits & ~(fsSBPartialName | fsSBFullName | fsSBNegate)) && searchInfo2 == nil)
		goto ParamErrExit;
	*includeFiles = false;
	*includeDirs = false;
	*includeNames = false;
	if (userPB->ioSearchBits & (fsSBPartialName | fsSBFullName)) {
		if (searchInfo1->hFileInfo.ioNamePtr == nil || searchInfo1->hFileInfo.ioNamePtr[0] == 0 || searchInfo1->hFileInfo.ioNamePtr[0] > sizeof(Str63) - 1)
			goto ParamErrExit;
		*includeNames = true;
	}
	if (userPB->ioSearchBits & fsSBFlAttrib) {
		if (searchInfo2->hFileInfo.ioFlAttrib & ~(ioDirMask | 0x01))
			goto ParamErrExit;
		if (searchInfo2->hFileInfo.ioFlAttrib & ioDirMask) {
			if (searchInfo1->hFileInfo.ioFlAttrib & ioDirMask)
				*includeDirs = true;
			else
				*includeFiles = true;
		} else {
			*includeDirs = true;
			*includeFiles = true;
		}
	} else {
		*includeDirs = true;
		*includeFiles = true;
	}
	if (*includeDirs && (userPB->ioSearchBits & fsSBFlAttrib) && (searchInfo2->hFileInfo.ioFlAttrib & 0x01))
		goto ParamErrExit;
	if (*includeFiles && (userPB->ioSearchBits & fsSBDrNmFls))
		goto ParamErrExit;
	if (*includeDirs && (userPB->ioSearchBits & (fsSBFlLgLen | fsSBFlPyLen | fsSBFlRLgLen | fsSBFlRPyLen)))
		goto ParamErrExit;
	return noErr;
ParamErrExit:
	return paramErr;
}

// FUNCTION: EVO 0x10073F30
Boolean IsSubString(ConstStr255Param aStringPtr, ConstStr255Param subStringPtr)
{
	short strLength;
	short subStrLength;
	Boolean found;
	short index;

	found = false;
	strLength = aStringPtr[0];
	subStrLength = subStringPtr[0];
	if (subStrLength <= strLength) {
		register short count;
		register short strIndex;
		register short subStrIndex;

		index = 1;
		count = strLength - subStrLength + 1;
		do {
			strIndex = index;
			subStrIndex = 1;
			while (!found && aStringPtr[strIndex] == subStringPtr[subStrIndex]) {
				if (subStrIndex == subStrLength)
					found = true;
				else {
					++subStrIndex;
					++strIndex;
				}
			}
			if (!found) {
				++index;
				--count;
			}
		} while (count != 0 && !found);
	}
	return found;
}

// FUNCTION: EVO 0x10073FCC
Boolean CompareMasked(const long *data1, const long *data2, const long *mask, short longsToCompare)
{
	Boolean result;

	result = true;
	while (longsToCompare != 0 && result == true) {
		if (*mask & (*data1 ^ *data2))
			result = false;
		data1++;
		data2++;
		mask++;
		longsToCompare--;
	}
	return result;
}

// FUNCTION: EVO 0x1007401C
void CheckForMatches(CInfoPBPtr cPB, CSParamPtr userPB, ConstStr255Param matchName, Boolean includeFiles, Boolean includeDirs)
{
	long searchBits;
	CInfoPBPtr searchInfo1, searchInfo2;
	Boolean foundMatch;
	Str63 itemName;

	foundMatch = false;
	searchBits = userPB->ioSearchBits;
	searchInfo1 = userPB->ioSearchInfo1;
	searchInfo2 = userPB->ioSearchInfo2;
	if (!(cPB->hFileInfo.ioFlAttrib & ioDirMask)) {
		if (!includeFiles)
			goto Failed;
	} else {
		if (!includeDirs)
			goto Failed;
	}
	if (searchBits & fsSBPartialName) {
		if (cPB->hFileInfo.ioNamePtr[0] == 0 || cPB->hFileInfo.ioNamePtr[0] > sizeof(Str63) - 1)
			goto Failed;
		BlockMoveData(cPB->hFileInfo.ioNamePtr, itemName, cPB->hFileInfo.ioNamePtr[0] + 1);
		UpperString(itemName, true);
		if (!IsSubString(itemName, matchName))
			goto Failed;
		if (searchBits == fsSBPartialName)
			goto Hit;
	}
	if (searchBits & fsSBFullName) {
		if (!EqualString(cPB->hFileInfo.ioNamePtr, matchName, false, true))
			goto Failed;
		if (searchBits == fsSBFullName)
			goto Hit;
	}
	if (searchBits & fsSBFlParID) {
		if ((unsigned long)cPB->hFileInfo.ioFlParID < (unsigned long)searchInfo1->hFileInfo.ioFlParID || (unsigned long)cPB->hFileInfo.ioFlParID > (unsigned long)searchInfo2->hFileInfo.ioFlParID)
			goto Failed;
	}
	if (searchBits & fsSBFlAttrib) {
		if ((searchInfo2->hFileInfo.ioFlAttrib) & (cPB->hFileInfo.ioFlAttrib ^ searchInfo1->hFileInfo.ioFlAttrib))
			goto Failed;
	}
	if (searchBits & fsSBDrNmFls) {
		if (cPB->dirInfo.ioDrNmFls < searchInfo1->dirInfo.ioDrNmFls || cPB->dirInfo.ioDrNmFls > searchInfo2->dirInfo.ioDrNmFls)
			goto Failed;
	}
	if (searchBits & fsSBFlFndrInfo) {
		if (!CompareMasked((long *)&cPB->hFileInfo.ioFlFndrInfo, (long *)&searchInfo1->hFileInfo.ioFlFndrInfo, (long *)&searchInfo2->hFileInfo.ioFlFndrInfo, 4))
			goto Failed;
	}
	if (searchBits & fsSBFlXFndrInfo) {
		if (!CompareMasked((long *)&cPB->hFileInfo.ioFlXFndrInfo, (long *)&searchInfo1->hFileInfo.ioFlXFndrInfo, (long *)&searchInfo2->hFileInfo.ioFlXFndrInfo, 4))
			goto Failed;
	}
	if (searchBits & fsSBFlLgLen) {
		if ((unsigned long)cPB->hFileInfo.ioFlLgLen < (unsigned long)searchInfo1->hFileInfo.ioFlLgLen || (unsigned long)cPB->hFileInfo.ioFlLgLen > (unsigned long)searchInfo2->hFileInfo.ioFlLgLen)
			goto Failed;
	}
	if (searchBits & fsSBFlPyLen) {
		if ((unsigned long)cPB->hFileInfo.ioFlPyLen < (unsigned long)searchInfo1->hFileInfo.ioFlPyLen || (unsigned long)cPB->hFileInfo.ioFlPyLen > (unsigned long)searchInfo2->hFileInfo.ioFlPyLen)
			goto Failed;
	}
	if (searchBits & fsSBFlRLgLen) {
		if ((unsigned long)cPB->hFileInfo.ioFlRLgLen < (unsigned long)searchInfo1->hFileInfo.ioFlRLgLen || (unsigned long)cPB->hFileInfo.ioFlRLgLen > (unsigned long)searchInfo2->hFileInfo.ioFlRLgLen)
			goto Failed;
	}
	if (searchBits & fsSBFlRPyLen) {
		if ((unsigned long)cPB->hFileInfo.ioFlRPyLen < (unsigned long)searchInfo1->hFileInfo.ioFlRPyLen || (unsigned long)cPB->hFileInfo.ioFlRPyLen > (unsigned long)searchInfo2->hFileInfo.ioFlRPyLen)
			goto Failed;
	}
	if (searchBits & fsSBFlCrDat) {
		if (cPB->hFileInfo.ioFlCrDat < searchInfo1->hFileInfo.ioFlCrDat || cPB->hFileInfo.ioFlCrDat > searchInfo2->hFileInfo.ioFlCrDat)
			goto Failed;
	}
	if (searchBits & fsSBFlMdDat) {
		if (cPB->hFileInfo.ioFlMdDat < searchInfo1->hFileInfo.ioFlMdDat || cPB->hFileInfo.ioFlMdDat > searchInfo2->hFileInfo.ioFlMdDat)
			goto Failed;
	}
	if (searchBits & fsSBFlBkDat) {
		if (cPB->hFileInfo.ioFlBkDat < searchInfo1->hFileInfo.ioFlBkDat || cPB->hFileInfo.ioFlBkDat > searchInfo2->hFileInfo.ioFlBkDat)
			goto Failed;
	}
Hit:
	foundMatch = true;
Failed:
	if (searchBits & fsSBNegate)
		foundMatch = !foundMatch;
	if (foundMatch) {
		userPB->ioMatchPtr[userPB->ioActMatchCount].vRefNum = cPB->hFileInfo.ioVRefNum;
		userPB->ioMatchPtr[userPB->ioActMatchCount].parID = cPB->hFileInfo.ioFlParID;
		if (cPB->hFileInfo.ioNamePtr[0] > 63)
			cPB->hFileInfo.ioNamePtr[0] = 63;
		BlockMoveData(cPB->hFileInfo.ioNamePtr, userPB->ioMatchPtr[userPB->ioActMatchCount].name, cPB->hFileInfo.ioNamePtr[0] + 1);
		userPB->ioActMatchCount++;
	}
}

#if GENERATINGCFM
// FUNCTION: EVO 0x10074360
static void SearchTimeout(TMTaskPtr tmTaskPtr)
{
	((ExtendedTMTask *)tmTaskPtr)->stopSearch = true;
}
#else
static pascal TMTaskPtr GetTMTaskPtr(void) ONEWORDINLINE(0x2E89);

static void SearchTimeout(void)
{
	((ExtendedTMTask *)GetTMTaskPtr())->stopSearch = true;
}
#endif

// FUNCTION: EVO 0x1007436C
long GetDirModDate(short vRefNum, long dirID)
{
	CInfoPBRec pb;
	Str31 tempName;
	long modDate;

	tempName[0] = 0;
	pb.dirInfo.ioNamePtr = tempName;
	pb.dirInfo.ioVRefNum = vRefNum;
	pb.dirInfo.ioDrDirID = dirID;
	pb.dirInfo.ioFDirIndex = -1;
	if (PBGetCatInfoSync(&pb) == noErr)
		modDate = pb.dirInfo.ioDrMdDat;
	else
		modDate = -1;
	return modDate;
}

// FUNCTION: EVO 0x100743D0
pascal OSErr IndexedSearch(CSParamPtr pb, long dirID)
{
	SearchPositionRec *catPosition;
	short index;
	OSErr result;
	ExtendedTMTask timerTask;
	short realVRefNum;
	Str63 itemName;
	CInfoPBRec cPB;
	long tempLong;
	Boolean includeFiles, includeDirs, includeNames;
	Str63 upperName;
	long modDate;

	timerTask.stopSearch = false;
	if (pb->ioSearchTime != 0) {
		timerTask.theTask.tmAddr = NewTimerProc(SearchTimeout);
		InsTime((QElemPtr)&timerTask.theTask);
		PrimeTime((QElemPtr)&timerTask.theTask, pb->ioSearchTime);
	}
	result = VerifyUserPB(pb, &includeFiles, &includeDirs, &includeNames);
	if (result == noErr) {
		pb->ioActMatchCount = 0;
		if (includeNames) {
			BlockMoveData(pb->ioSearchInfo1->hFileInfo.ioNamePtr, upperName, pb->ioSearchInfo1->hFileInfo.ioNamePtr[0] + 1);
			UpperString(upperName, true);
		}
		catPosition = (SearchPositionRec *)&pb->ioCatPosition;
		if (gStack == nil)
			gStack = (LevelRecHandle)NewHandle(kAdditionalLevelRecs * sizeof(LevelRec));
		if (gStack != nil) {
			gStackSize = GetHandleSize((Handle)gStack);
			if (catPosition->initialize == 0) {
				result = CheckVol(pb->ioNamePtr, pb->ioVRefNum, &realVRefNum, &catPosition->initialize);
				if (result == noErr) {
					catPosition->stackDepth = 0;
					index = -1;
				}
			} else {
				result = CheckVol(pb->ioNamePtr, pb->ioVRefNum, &realVRefNum, &tempLong);
				if (result == noErr) {
					if (catPosition->initialize == tempLong) {
						if (catPosition->stackDepth != 0) {
							catPosition->stackDepth--;
							dirID = (*gStack)[catPosition->stackDepth].dirID;
							index = (*gStack)[catPosition->stackDepth].index;
							modDate = GetDirModDate(realVRefNum, dirID);
							if (modDate != (*gStack)[catPosition->stackDepth].dirModDate)
								result = catChangedErr;
						} else
							result = paramErr;
					} else
						result = catChangedErr;
				}
			}
			if (result == noErr) {
				cPB.hFileInfo.ioNamePtr = itemName;
				cPB.hFileInfo.ioVRefNum = realVRefNum;
				do {
					cPB.hFileInfo.ioFDirIndex = index;
					cPB.hFileInfo.ioDirID = dirID;
					result = PBGetCatInfoSync(&cPB);
					if (index != -1) {
						if (result == noErr) {
							CheckForMatches(&cPB, pb, upperName, includeFiles, includeDirs);
							index++;
							if (cPB.hFileInfo.ioFlAttrib & ioDirMask) {
								result = CheckStack(catPosition->stackDepth, gStack, &gStackSize);
								if (result == noErr) {
									(*gStack)[catPosition->stackDepth].dirID = dirID;
									(*gStack)[catPosition->stackDepth].index = index;
									(*gStack)[catPosition->stackDepth].dirModDate = GetDirModDate(realVRefNum, dirID);
									catPosition->stackDepth++;
									dirID = cPB.hFileInfo.ioDirID;
									index = 1;
								}
							}
						} else {
							if (catPosition->stackDepth != 0) {
								catPosition->stackDepth--;
								dirID = (*gStack)[catPosition->stackDepth].dirID;
								index = (*gStack)[catPosition->stackDepth].index;
								modDate = GetDirModDate(realVRefNum, dirID);
								if (modDate != (*gStack)[catPosition->stackDepth].dirModDate)
									result = catChangedErr;
								else
									result = noErr;
							} else
								result = eofErr;
						}
					} else if (result == noErr) {
						CheckForMatches(&cPB, pb, upperName, includeFiles, includeDirs);
						index = 1;
					}
				} while (!timerTask.stopSearch && result == noErr && pb->ioReqMatchCount > pb->ioActMatchCount);
				if (result == noErr) {
					result = CheckStack(catPosition->stackDepth, gStack, &gStackSize);
					if (result == noErr) {
						(*gStack)[catPosition->stackDepth].dirID = dirID;
						(*gStack)[catPosition->stackDepth].index = index;
						(*gStack)[catPosition->stackDepth].dirModDate = GetDirModDate(realVRefNum, dirID);
						catPosition->stackDepth++;
					}
				}
			}
		} else
			result = memFullErr;
	}
	if (pb->ioSearchTime != 0) {
		RmvTime((QElemPtr)&timerTask.theTask);
		DisposeRoutineDescriptor(timerTask.theTask.tmAddr);
	}
	return result;
}

// FUNCTION: EVO 0x10074818
pascal OSErr PBCatSearchSyncCompat(CSParamPtr paramBlock)
{
	OSErr result;
	Boolean supportsCatSearch;
	long response;
	GetVolParmsInfoBuffer volParmsInfo;
	long infoSize;

	result = noErr;
	if (!gChecked) {
		gChecked = true;
		if (Gestalt(gestaltFSAttr, &response) == noErr)
			gHasExtFS = (response & (1L << gestaltFullExtFSDispatching)) != 0;
	}
	supportsCatSearch = false;
	if (gHasExtFS) {
		infoSize = sizeof(GetVolParmsInfoBuffer);
		result = HGetVolParms(paramBlock->ioNamePtr, paramBlock->ioVRefNum, &volParmsInfo, &infoSize);
		if (result == noErr)
			supportsCatSearch = (volParmsInfo.vMAttrib & (1L << bHasCatSearch)) != 0;
	}
	if (result == noErr || result == paramErr) {
		if (supportsCatSearch)
			result = PBCatSearchSync(paramBlock);
		else
			result = IndexedSearch(paramBlock, fsRtDirID);
	}
	return result;
}

// FUNCTION: EVO 0x10074930
pascal OSErr NameFileSearch(ConstStr255Param volName, short vRefNum, ConstStr255Param fileName, FSSpecPtr matches, long reqMatchCount, long *actMatchCount, Boolean newSearch, Boolean partial)
{
	CInfoPBRec searchInfo1;
	CInfoPBRec searchInfo2;
	HParamBlockRec pb;
	OSErr error;

	error = DetermineVRefNum(volName, vRefNum, &vRefNum);
	if (error != noErr)
		return error;
	pb.csParam.ioNamePtr = nil;
	pb.csParam.ioVRefNum = vRefNum;
	pb.csParam.ioMatchPtr = matches;
	pb.csParam.ioReqMatchCount = reqMatchCount;
	pb.csParam.ioSearchBits = partial ? (fsSBPartialName | fsSBFlAttrib) : (fsSBFullName | fsSBFlAttrib);
	pb.csParam.ioSearchInfo1 = &searchInfo1;
	pb.csParam.ioSearchInfo2 = &searchInfo2;
	pb.csParam.ioSearchTime = 0;
	if (newSearch || gLastVRefNum != vRefNum)
		gCatPosition.initialize = 0;
	pb.csParam.ioCatPosition = gCatPosition;
	pb.csParam.ioOptBuffer = GetTempBuffer(0x4000, &pb.csParam.ioOptBufSize);
	searchInfo1.hFileInfo.ioNamePtr = (StringPtr)fileName;
	searchInfo2.hFileInfo.ioNamePtr = nil;
	searchInfo1.hFileInfo.ioFlAttrib = 0;
	searchInfo2.hFileInfo.ioFlAttrib = ioDirMask;
	error = PBCatSearchSyncCompat((CSParamPtr)&pb);
	if (error == noErr || error == eofErr)
		*actMatchCount = pb.csParam.ioActMatchCount;
	else
		*actMatchCount = 0;
	if (error == noErr || error == catChangedErr) {
		gCatPosition = pb.csParam.ioCatPosition;
		gLastVRefNum = vRefNum;
	} else
		gCatPosition.initialize = 0;
	if (pb.csParam.ioOptBuffer != nil)
		DisposePtr(pb.csParam.ioOptBuffer);
	return error;
}

// FUNCTION: EVO 0x10074ADC
OSErr InstallCallbackPtr(Ptr p, long proc)
{
	Ptr q = nil;
	OSErr err = noErr;

	if (p != nil) {
		q = p;
		*(long *)(q + 6) = proc;
	}
	return err;
}
