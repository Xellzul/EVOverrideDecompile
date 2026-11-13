/* SoundMixer.c -- software sound mixer on a Sound Manager double-buffered channel (provisional name). Notes: Research/notes/SoundMixer.md */
// COMPILER: CWPro2
// COMPILE: -opt l4,schedule604
// COMPILE-WHY: separately built library part: CW Pro 2 MWCPPC 2.0, the CD of its 68k half (TOC-pointer access to tentative statics): Research/notes/SoundMixer.md (FABLE-MIX)
#define OLDROUTINENAMES 1
#include <Types.h>
#include <Memory.h>
#include <Resources.h>
#include <OSUtils.h>
#include <Traps.h>
#include <Gestalt.h>
#include <Sound.h>
#include <SoundInput.h>
#include <FixMath.h>
#include <ToolUtils.h>
#include <MixedMode.h>

#include "evo.h"

#define kMaxVoices	16
#define kMixFrames	1024
#define kSampleBias	128
#define kSampleMax	127
#define kMaxSysVolume	7
#define kSoundMemErr	(-1000)
#define kSoundBusyErr	(-1001)
#define kSoundNoMgrErr	(-1300)

typedef struct MixVoice {
	long			id;
	Ptr				snd;
	short			*data;
	long			ticks;
	unsigned long	step;
	long			ratio;
	unsigned long	pos;
	SoundCallback	callback;
	short			*cur;
	short			*prev;
	long			extra;
	unsigned short	volLeft;
	unsigned short	volRight;
	unsigned short	priority;
} MixVoice;

typedef struct MixSound {
	OSType			tag;
	long			blocks;
	unsigned long	rate;
	short			samples[1];
} MixSound;

static unsigned short	gMasterVolume;
static short		gChannels;
static long		gMixVoiceLimit;
static long		gVoiceCount;
static MixVoice	gVoices[kMaxVoices];
static Fixed		gSampleRate;
static long		gSavedVolume;
static SndDoubleBufferHeader	gDoubleHeader;
static SndChannelPtr	gChannel;
static Ptr			gMixOut;
static Boolean		gStereo;
static Boolean		gHasSoundMgr3;
static DeferredTask	gMixerState;
static long		gNextVoiceId;
static SoundRequest	gCallbackRequest;
static Boolean	gSoundActive = false;

static void MixSoftwareSoundsStereo(long unused);
static void MixSoftwareSounds(long unused);
static void InitSoundMixerState(void);
static Ptr AllocSoundBuffer(void);
static SndChannelPtr AllocSoundChannelControlBlock(void);
static Boolean IsSoundManagerV3Plus(void);
static Boolean IsSoundManagerV3_1Plus(void);
static long FindSoundCommandData(Handle h);
static OSErr ProbeSoundChannelSampleRate(Fixed *rate);
static pascal void TickSoundCallback(SndChannelPtr chan, SndDoubleBufferPtr buf);
static long DisableInterrupts(void);
static void RestoreInterrupts(long state);

// FUNCTION: EVO 0x10074AF0
OSErr BootSoundSubsystem(unsigned short maxVoices, Boolean stereo, unsigned long sampleRate)
{
	OSErr err = 0;
	long response;
	short vol;
	short init;

	if (gSoundActive)
		return kSoundBusyErr;
	if (Gestalt(gestaltSoundAttr, &response) != noErr) {
		err = kSoundNoMgrErr;
	} else {
		gSampleRate = sampleRate;
		if (IsSoundManagerV3_1Plus() && sampleRate == 0) {
			gSampleRate = rate22khz;
			ProbeSoundChannelSampleRate(&gSampleRate);
		}
		if ((response & 1) == 0)
			stereo = false;
		gStereo = stereo;
		if (stereo) {
			init = initStereo;
			gChannels = 2;
		} else {
			init = initMono;
			gChannels = 1;
		}
		gMixVoiceLimit = maxVoices;
		if (gMixVoiceLimit > kMaxVoices)
			gMixVoiceLimit = kMaxVoices;
		InitSoundMixerState();
		gDoubleHeader.dbhNumChannels = gChannels;
		gDoubleHeader.dbhSampleSize = 8;
		gDoubleHeader.dbhCompressionID = 0;
		gDoubleHeader.dbhPacketSize = 0;
		gDoubleHeader.dbhSampleRate = gSampleRate;
		gDoubleHeader.dbhBufferPtr[0] = (SndDoubleBufferPtr)AllocSoundBuffer();
		if (gDoubleHeader.dbhBufferPtr[0] == 0) {
			err = kSoundMemErr;
		} else {
			gDoubleHeader.dbhBufferPtr[1] = (SndDoubleBufferPtr)AllocSoundBuffer();
			if (gDoubleHeader.dbhBufferPtr[0] == 0) {	/* BUG: tests the first buffer again, the second is never checked */
				DisposePtr((Ptr)gDoubleHeader.dbhBufferPtr[0]);
				err = kSoundMemErr;
			} else {
				gDoubleHeader.dbhDoubleBack = NewSndDoubleBackProc(TickSoundCallback);
				gChannel = AllocSoundChannelControlBlock();
				if (SndNewChannel(&gChannel, sampledSynth, init, 0) != noErr) {
					DisposePtr((Ptr)gDoubleHeader.dbhBufferPtr[0]);
					DisposePtr((Ptr)gDoubleHeader.dbhBufferPtr[1]);
					err = kSoundMemErr;
				} else if (SndPlayDoubleBuffer(gChannel, &gDoubleHeader) != noErr) {
					DisposePtr((Ptr)gDoubleHeader.dbhBufferPtr[0]);
					DisposePtr((Ptr)gDoubleHeader.dbhBufferPtr[1]);
					SndDisposeChannel(gChannel, true);
					err = kSoundMemErr;
				} else {
					gSoundActive = true;
					FlushMixQueueEntries(0);
					if (gHasSoundMgr3)
						GetDefaultOutputVolume(&gSavedVolume);
#if !GENERATINGPOWERPC
					else {
						GetSoundVol(&vol);
						gSavedVolume = vol;
					}
#endif
					SetMasterVolume(GetMasterVolume());
				}
			}
		}
	}
	return err;
}

// FUNCTION: EVO 0x10074D48
void TeardownSoundSubsystem(void)
{
	if (gSoundActive) {
		FlushMixQueueEntries(0);
		if (gHasSoundMgr3)
			SetDefaultOutputVolume(gSavedVolume);
#if !GENERATINGPOWERPC
		else
			SetSoundVol(gSavedVolume);
#endif
		SndDisposeChannel(gChannel, true);
		DisposePtr((Ptr)gDoubleHeader.dbhBufferPtr[0]);
		DisposePtr((Ptr)gDoubleHeader.dbhBufferPtr[1]);
		gSoundActive = false;
	}
}

// FUNCTION: EVO 0x10074DDC
void SetMasterVolume(unsigned short volume)
{
	long both;
	short vol;

	if (gSoundActive) {
		if (volume > kMaxMasterVolume)
			volume = kMaxMasterVolume;
		if (volume < 0)
			volume = 0;
		gMasterVolume = volume;
		both = volume + ((long)volume << 16);
		if (gHasSoundMgr3)
			SetDefaultOutputVolume(both);
#if !GENERATINGPOWERPC
		else
		{
			vol = volume / kMaxSysVolume;
			SetSoundVol(vol);
		}
#endif
	}
}

// FUNCTION: EVO 0x10074E44
unsigned short GetMasterVolume(void)
{
	unsigned short v = 0;
	long t;

	if (gSoundActive) {
		if (gHasSoundMgr3) {
			t = ((gSavedVolume >> 16) + (gSavedVolume & 0xFFFF)) / 2;
			v = t;
			if (v > kMaxMasterVolume)
				v = kMaxMasterVolume;
			if (v < 0)
				v = 0;
		} else {
			v = gSavedVolume;
			if (v > kMaxSysVolume)
				v = kMaxSysVolume;
			if (v < 0)
				v = 0;
		}
	}
	return v;
}

// FUNCTION: EVO 0x10074EC0
long TriggerSoundPlay(Ptr snd, unsigned short priority, short volume)
{
	SoundRequest req;
	OSErr err = noErr;
	long result;

	req.snd = snd;
	req.priority = priority;
	req.volLeft = volume;
	req.volRight = volume;
	req.pitch = fixed1;
	req.callback = 0;
	req.extra = 0;
	req.id = 0;
	result = EnqueueSoundVoice(&req);
	return result;
}

// FUNCTION: EVO 0x10074F10
long EnqueueSoundVoice(SoundRequest *req)
{
	Ptr snd;
	Boolean found;
	short i;
	long state;
	long ratio;
	long result = 0;
	Ptr src, dst;
	long size;

	found = false;
	snd = req->snd;
	if (*(Ptr *)req != 0 && gSoundActive) {
		if (req->priority < 1)
			req->priority = 1;
		if (req->volRight > kMaxVoiceVolume)
			req->volRight = kMaxVoiceVolume;
		if (req->volRight < 0)
			req->volRight = 0;
		if (req->volLeft > kMaxVoiceVolume)
			req->volLeft = kMaxVoiceVolume;
		if (req->volLeft < 0)
			req->volLeft = 0;
		state = DisableInterrupts();
		i = 0;
		while (gVoices[i].snd != 0 && i < kMaxVoices && !found) {
			if (req->priority >= gVoices[i].priority
					&& req->volRight + req->volLeft >= gVoices[i].volLeft + gVoices[i].volRight)
				found = true;
			else
				i++;
		}
		if (i >= kMaxVoices) {
			RestoreInterrupts(state);
			return 0;
		}
		if (gVoiceCount >= kMaxVoices)
			gVoiceCount = kMaxVoices - 1;
		src = (Ptr)&gVoices[i];
		dst = (Ptr)&gVoices[i + 1];
		size = (gVoiceCount - i) * sizeof(MixVoice);
		BlockMoveData(src, dst, size);
		ratio = FixDiv(((MixSound *)snd)->rate >> 8, (unsigned long)gSampleRate >> 8);
		result = ((MixSound *)req->snd)->blocks;
		snd = (Ptr)FixDiv(result << 16, FixMul(ratio, req->pitch));
		result = (unsigned long)snd >> 16;
		if (result < 1)
			result = 1;
		gVoices[i].snd = req->snd;
		gVoices[i].data = ((MixSound *)req->snd)->samples;
		gVoices[i].cur = ((MixSound *)req->snd)->samples;
		gVoices[i].prev = 0;
		gVoices[i].ticks = result;
		gVoices[i].ratio = ratio;
		gVoices[i].step = FixMul(gVoices[i].ratio, req->pitch);
		gVoices[i].pos = 0;
		gVoices[i].callback = req->callback;
		gVoices[i].priority = req->priority;
		gVoices[i].volLeft = req->volLeft;
		gVoices[i].volRight = req->volRight;
		gVoices[i].extra = req->extra;
		if (req->id != 0) {
			gVoices[i].id = req->id;
		} else {
			gVoices[i].id = gNextVoiceId;
			gNextVoiceId += 2;
		}
		result = gVoices[i].id;
		if (!gStereo && req->volRight > req->volLeft)
			gVoices[i].volLeft = req->volRight;
		if (found)
			;
		gVoiceCount++;
		RestoreInterrupts(state);
	}
	return result;
}

// FUNCTION: EVO 0x1007520C
void FlushMixQueueEntries(long snd)
{
	Boolean done = false;
	short i;
	SoundCallback cb;
	long state;
	Ptr src, dst;
	long size;

	if (gSoundActive) {
		i = 0;
		while (i < gVoiceCount && !done) {
			if (snd == gVoices[i].id || gVoices[i].snd == (Ptr)snd || snd == 0) {
				cb = gVoices[i].callback;
				state = DisableInterrupts();
				gVoiceCount--;
				src = (Ptr)&gVoices[i] + sizeof(MixVoice);
				dst = (Ptr)&gVoices[i];
				size = (gVoiceCount - i) * sizeof(MixVoice);
				BlockMoveData(src, dst, size);
				gVoices[gVoiceCount].id = 0;
				gVoices[gVoiceCount].snd = 0;
				gVoices[gVoiceCount].data = 0;
				gVoices[gVoiceCount].ticks = 0;
				gVoices[gVoiceCount].step = 0;
				gVoices[gVoiceCount].ratio = 0;
				gVoices[gVoiceCount].pos = 0;
				gVoices[gVoiceCount].cur = 0;
				gVoices[gVoiceCount].callback = 0;
				gVoices[gVoiceCount].priority = 0;
				gVoices[gVoiceCount].volLeft = 0;
				gVoices[gVoiceCount].volRight = 0;
				gVoices[gVoiceCount].extra = 0;
				RestoreInterrupts(state);
				if (cb != 0)
					cb(2, snd, &gCallbackRequest);
			} else {
				i++;
			}
		}
	}
}

// FUNCTION: EVO 0x100753E4
unsigned short CountMatchingSoundVoices(long snd)
{
	short count = 0;
	short i;

	if (gSoundActive) {
		for (i = 0; i < gVoiceCount; i++) {
			if (snd == gVoices[i].id || gVoices[i].snd == (Ptr)snd || snd == 0)
				count++;
		}
	}
	return count;
}

// FUNCTION: EVO 0x10075450
Ptr LoadSndResource(short id)
{
	long units;
	long frames;
	Ptr result = 0;
	Handle h;
	long offset;
	unsigned char *src;
	short *dst;
	SoundHeaderPtr hdr;
	long size;
	Fixed ratio;
	long bytes;
	unsigned char c;
	short s;

	h = GetResource('snd ', id);
	if (h != 0) {
		HNoPurge(h);
		HLockHi(h);
		offset = FindSoundCommandData(h);
		if (offset > 0) {
			hdr = (SoundHeaderPtr)(*h + offset);
			ratio = FixDiv(hdr->sampleRate >> 8, (unsigned long)gSampleRate >> 8);
			offset += 23;
			size = GetHandleSize(h);
			size -= offset;
			bytes = (size + kMixFrames - 1) / kMixFrames * kMixFrames * sizeof(short) + 12;
			result = NewPtrClear(bytes + kMixFrames);
			if (result != 0) {
				units = (size + kMixFrames - 1) / kMixFrames;
				if (units < 1)
					units = 1;
				((MixSound *)result)->tag = 'asnd';
				((MixSound *)result)->blocks = units;
				((MixSound *)result)->rate = hdr->sampleRate;
				src = (unsigned char *)(*h + offset);
				dst = ((MixSound *)result)->samples;
				while (size-- != 0) {
					c = *src++;
					s = c - kSampleBias;
					*dst++ = s;
				}
			}
		}
		HUnlock(h);
		HPurge(h);
	}
	return result;
}

// FUNCTION: EVO 0x10075598
void RemoveMixerVoiceAt(short index)
{
	OSErr err = noErr;
	Ptr src, dst;
	long size;

	if (gSoundActive) {
		gVoiceCount--;
		src = (Ptr)&gVoices[index + 1];
		dst = (Ptr)&gVoices[index];
		size = (gVoiceCount - index) * sizeof(MixVoice);
		BlockMoveData(src, dst, size);
		gVoices[gVoiceCount].id = 0;
		gVoices[gVoiceCount].snd = 0;
		gVoices[gVoiceCount].data = 0;
		gVoices[gVoiceCount].ticks = 0;
		gVoices[gVoiceCount].step = 0;
		gVoices[gVoiceCount].ratio = 0;
		gVoices[gVoiceCount].pos = 0;
		gVoices[gVoiceCount].cur = 0;
		gVoices[gVoiceCount].callback = 0;
		gVoices[gVoiceCount].priority = 0;
		gVoices[gVoiceCount].volLeft = 0;
		gVoices[gVoiceCount].volRight = 0;
		gVoices[gVoiceCount].extra = 0;
	}
}

// FUNCTION: EVO 0x100756E8
static pascal void TickSoundCallback(SndChannelPtr chan, SndDoubleBufferPtr buf)
{
	long i;
	SoundCallback cb;
	long id;
#if !GENERATINGPOWERPC
	long oldA5 = SetA5(buf->dbUserInfo[0]);
#endif

	gMixOut = (Ptr)buf->dbSoundData;
	if (gStereo)
		MixSoftwareSoundsStereo(0);
	else
		MixSoftwareSounds(0);
	for (i = 0; i < gVoiceCount; i++) {
		cb = gVoices[i].callback;
		id = gVoices[i].id;
		if (cb != 0) {
			gCallbackRequest.snd = gVoices[i].snd;
			gCallbackRequest.priority = gVoices[i].priority;
			gCallbackRequest.volLeft = gVoices[i].volLeft;
			gCallbackRequest.volRight = gVoices[i].volRight;
			gCallbackRequest.pitch = fixed1;
			gCallbackRequest.callback = gVoices[i].callback;
			gCallbackRequest.extra = gVoices[i].extra;
			gCallbackRequest.id = id;
			cb(3, id, &gCallbackRequest);
		}
		if (--gVoices[i].ticks == 0) {
			RemoveMixerVoiceAt(i);
			i--;
			if (cb != 0)
				cb(1, id, &gCallbackRequest);
		}
	}
	buf->dbFlags = dbBufferReady;
#if !GENERATINGPOWERPC
	SetA5(oldA5);
#endif
}

// FUNCTION: EVO 0x10075830
static void MixSoftwareSoundsStereo(long unused)
{
	long n, i;
	long left, right, sample, next;
	short *out = (short *)gMixOut;
	MixVoice *v;

	for (n = 0; n < kMixFrames; n++) {
		left = 0;
		right = 0;
		for (i = 0; i < gVoiceCount; i++) {
			v = &gVoices[i];
			if (i < gMixVoiceLimit) {
				sample = v->cur[0];
				if (v->cur == v->prev) {
					next = v->cur[1];
					sample = (sample + next) >> 1;
				}
				v->prev = v->cur;
				left += (sample * v->volLeft) >> 7;
				right += (sample * v->volRight) >> 7;
			}
			v->pos += v->step >> 8;
			v->cur = v->data + (v->pos >> 8);
		}
		if (left > kSampleMax)
			left = kSampleMax;
		if (left < -kSampleMax)
			left = -kSampleMax;
		left += kSampleBias;
		if (right > kSampleMax)
			right = kSampleMax;
		if (right < -kSampleMax)
			right = -kSampleMax;
		right += kSampleBias;
		right |= left << 8;
		*out++ = right;
	}
}

// FUNCTION: EVO 0x1007594C
static void MixSoftwareSounds(long unused)
{
	long n, i;
	long mix, sample, next;
	unsigned char *out = (unsigned char *)gMixOut;
	MixVoice *v;

	for (n = 0; n < kMixFrames; n++) {
		mix = 0;
		for (i = 0; i < gVoiceCount; i++) {
			v = &gVoices[i];
			if (i < gMixVoiceLimit) {
				sample = v->cur[0];
				if (v->cur == v->prev) {
					next = v->cur[1];
					sample = (sample + next) >> 1;
				}
				v->prev = v->cur;
				mix += (sample * v->volLeft) >> 7;
			}
			v->pos += v->step >> 8;
			v->cur = v->data + (v->pos >> 8);
		}
		if (mix > kSampleMax)
			mix = kSampleMax;
		if (mix < -kSampleMax)
			mix = -kSampleMax;
		mix += kSampleBias;
		*out++ = mix;
	}
}

// FUNCTION: EVO 0x10075A28
static void InitSoundMixerState(void)
{
	short i;

	gMixerState.qLink = 0;
	gMixerState.qType = dtQType;
	gMixerState.dtFlags = 0;
	if (gStereo)
		gMixerState.dtAddr = NewDeferredTaskProc(MixSoftwareSoundsStereo);
	else
		gMixerState.dtAddr = NewDeferredTaskProc(MixSoftwareSounds);
	gMixerState.dtParam = 0;
#if !GENERATINGPOWERPC
	gMixerState.dtParam = SetCurrentA5();
#endif
	gMixerState.dtReserved = 0;
	gNextVoiceId = 1;
	gVoiceCount = 0;
	for (i = 0; i < kMaxVoices; i++) {
		gVoices[i].id = 0;
		gVoices[i].snd = 0;
		gVoices[i].data = 0;
		gVoices[i].ticks = 0;
		gVoices[i].step = 0;
		gVoices[i].ratio = 0;
		gVoices[i].pos = 0;
		gVoices[i].cur = 0;
		gVoices[i].callback = 0;
		gVoices[i].priority = 0;
		gVoices[i].volLeft = 0;
		gVoices[i].volRight = 0;
		gVoices[i].extra = 0;
	}
	gHasSoundMgr3 = IsSoundManagerV3Plus();
}

// FUNCTION: EVO 0x10075B80
static Ptr AllocSoundBuffer(void)
{
	SndDoubleBufferPtr buf;
	unsigned short *p;
	short i;
	short count;

	buf = (SndDoubleBufferPtr)NewPtr(gChannels * kMixFrames + sizeof(SndDoubleBuffer));
	if (buf != 0) {
		buf->dbNumFrames = kMixFrames;
		buf->dbUserInfo[0] = 0;
#if !GENERATINGPOWERPC
		buf->dbUserInfo[0] = SetCurrentA5();
#endif
		p = (unsigned short *)buf->dbSoundData;
		count = gChannels << 9;
		for (i = 0; i < count; i++)
			*p++ = kSampleBias << 8 | kSampleBias;
		buf->dbFlags = dbBufferReady;
	}
	return (Ptr)buf;
}

// FUNCTION: EVO 0x10075C14
static SndChannelPtr AllocSoundChannelControlBlock(void)
{
	SndChannelPtr chan;

	chan = (SndChannelPtr)NewPtrClear(sizeof(SndChannel));
	if (chan != 0)
		chan->qLength = stdQLength;
	return chan;
}

// FUNCTION: EVO 0x10075C4C
static Boolean IsMacTrapAvailable(int trap)
{
	Boolean available = false;
	char trapType;

	if ((trap & 0x0800) == 0)
		trapType = OSTrap;
	else
		trapType = ToolTrap;
	if (trapType == ToolTrap && (trap & 0x03FF) >= 0x200
			&& GetToolboxTrapAddress(_InitGraf) == GetToolboxTrapAddress(0xAA6E))
		available = false;
	else
		available = NGetTrapAddress(trap, trapType) != GetToolboxTrapAddress(_Unimplemented);
	return available;
}

// FUNCTION: EVO 0x10075D1C
static long FindSoundCommandData(Handle h)
{
	short numSynths;
	char *p;
	short numCmds;
	long offset = 0;
	short bufCmd = bufferCmd | dataOffsetFlag;
	Boolean done = false;
	short sndCmd = soundCmd | dataOffsetFlag;

	p = *h;
	switch (*(short *)p) {
		case firstSoundFormat:
			numSynths = ((short *)p)[1];
			p += 4;
			p += numSynths * sizeof(ModRef);
			break;
		case secondSoundFormat:
			p += 4;
			break;
		default:
			done = true;
	}
	numCmds = *(short *)p;
	p += 2;
	while (numCmds > 0 && !done) {
		if (*(short *)p == bufCmd || *(short *)p == sndCmd) {
			offset = ((SndCommand *)p)->param2;
			done = true;
		} else {
			p += sizeof(SndCommand);
			numCmds--;
		}
	}
	return offset;
}

// FUNCTION: EVO 0x10075DBC
static OSErr ProbeSoundChannelSampleRate(Fixed *rate)
{
	SndChannelPtr chan = 0;
	OSErr err;

	err = SndNewChannel(&chan, sampledSynth, 0, 0);
	if (err == noErr) {
		err = SndGetInfo(chan, siSampleRate, rate);
		SndDisposeChannel(chan, true);
	}
	return err;
}

// FUNCTION: EVO 0x10075E44
static Boolean IsSoundManagerV3Plus(void)
{
	Boolean ok = false;
	NumVersion v;

	if (IsMacTrapAvailable((short)_SoundDispatch)) {
		v = SndSoundManagerVersion();
		ok = v.majorRev >= 3;
	} else
		ok = false;
	return ok;
}

// FUNCTION: EVO 0x10075EA0
static Boolean IsSoundManagerV3_1Plus(void)
{
	Boolean ok = false;
	NumVersion v;

	if (IsMacTrapAvailable((short)_SoundDispatch)) {
		v = SndSoundManagerVersion();
		if (v.majorRev > 3 || (v.majorRev == 3 && v.minorAndBugRev >= 0x10))
			ok = true;
	}
	return ok;
}

#if !GENERATINGPOWERPC
#pragma parameter __D0 RaiseInterruptMask
static unsigned short RaiseInterruptMask(void) = { 0x40C0, 0x007C, 0x0700 };
#pragma parameter SetStatusRegister(__D0)
static void SetStatusRegister(unsigned short sr) = 0x46C0;
#endif

// FUNCTION: EVO 0x10075F08
static long DisableInterrupts(void)
{
#if GENERATINGPOWERPC
	unsigned short code[6] = { 0x40C0, 0x007C, 0x0700, 0x4E75, 0, 0 };

	return CallUniversalProc((UniversalProcPtr)code, 0x32);
#else
	long state;

	state = RaiseInterruptMask();
	return state;
#endif
}

// FUNCTION: EVO 0x10075F50
static void RestoreInterrupts(long state)
{
#if GENERATINGPOWERPC
	unsigned short code[3] = { 0x46C0, 0x4E75, 0 };

	CallUniversalProc((UniversalProcPtr)code, 0x1802, state);
#else
	SetStatusRegister(state);
#endif
}
