/* MoreFilesExtras.c -- volume info helpers, probably Jim Luther's MoreFiles (provisional name). Notes: Research/notes/MoreFilesExtras.md */
// COMPILER: CWPro1
// COMPILE: -opt l4,noschedule
// COMPILE-WHY: separately built library part, the later (UI 2.1.4) build: CW11 Gold MWCPPC 1.7: Research/notes/library-compiler.md (LIB9)
#include <Types.h>
#include <Memory.h>
#include <Files.h>
#include <Errors.h>

#pragma options align=mac68k

pascal OSErr HGetVolParms(ConstStr255Param volName, short vRefNum, GetVolParmsInfoBuffer *volParmsInfo, long *infoSize);
pascal Ptr GetTempBuffer(long buffReqSize, long *buffActSize);
pascal OSErr GetVolumeInfoNoName(ConstStr255Param pathname, short vRefNum, HParmBlkPtr pb);
pascal OSErr DetermineVRefNum(ConstStr255Param pathname, short vRefNum, short *realVRefNum);

// FUNCTION: EVO 0x10073B0C
pascal OSErr HGetVolParms(ConstStr255Param volName, short vRefNum, GetVolParmsInfoBuffer *volParmsInfo, long *infoSize)
{
	HParamBlockRec pb;
	OSErr error;

	pb.ioParam.ioNamePtr = (StringPtr)volName;
	pb.ioParam.ioVRefNum = vRefNum;
	pb.ioParam.ioBuffer = (Ptr)volParmsInfo;
	pb.ioParam.ioReqCount = *infoSize;
	error = PBHGetVolParmsSync(&pb);
	*infoSize = pb.ioParam.ioActCount;
	return error;
}

// FUNCTION: EVO 0x10073B5C
pascal Ptr GetTempBuffer(long buffReqSize, long *buffActSize)
{
	enum {
		kSlopMemory = 0x00008000
	};
	long tempBuffActSize;
	Ptr tempBuffPtr;

	buffReqSize &= 0xfffffc00;
	if (buffReqSize < 0x00000400)
		buffReqSize = 0x00000400;
	tempBuffPtr = NewPtr(buffReqSize);
	if (tempBuffPtr == NULL && buffReqSize > 0x00000400) {
		tempBuffActSize = (FreeMem() - kSlopMemory) & 0xfffffc00;
		buffReqSize = MaxBlock() & 0xfffffc00;
		if (buffReqSize > tempBuffActSize)
			buffReqSize = tempBuffActSize;
		if (buffReqSize == 0)
			buffReqSize = 0x00000400;
		tempBuffPtr = NewPtr(buffReqSize);
	}
	if (tempBuffPtr != NULL)
		*buffActSize = buffReqSize;
	else
		*buffActSize = 0;
	return tempBuffPtr;
}

// FUNCTION: EVO 0x10073C18
pascal OSErr GetVolumeInfoNoName(ConstStr255Param pathname, short vRefNum, HParmBlkPtr pb)
{
	Str255 tempPathname;
	OSErr error;

	if (pb != nil) {
		pb->volumeParam.ioVRefNum = vRefNum;
		if (pathname == nil) {
			pb->volumeParam.ioNamePtr = nil;
			pb->volumeParam.ioVolIndex = 0;
		} else {
			BlockMoveData(pathname, tempPathname, pathname[0] + 1);
			pb->volumeParam.ioNamePtr = tempPathname;
			pb->volumeParam.ioVolIndex = -1;
		}
		error = PBHGetVInfoSync(pb);
		pb->volumeParam.ioNamePtr = nil;
	} else
		error = paramErr;
	return error;
}

// FUNCTION: EVO 0x10073CAC
pascal OSErr DetermineVRefNum(ConstStr255Param pathname, short vRefNum, short *realVRefNum)
{
	HParamBlockRec pb;
	OSErr error;

	error = GetVolumeInfoNoName(pathname, vRefNum, &pb);
	*realVRefNum = pb.volumeParam.ioVRefNum;
	return error;
}
