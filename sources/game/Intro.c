/* Intro.c -- startup splash screens, the loading bar, the credits scroller and the intro music. Notes: Research/notes/Intro.md */
#define OLDROUTINENAMES 1
#include <Quickdraw.h>
#include <Resources.h>
#include <Memory.h>
#include <TextUtils.h>
#include <Sound.h>
#include <Events.h>
#include <string.h>
#include <stdlib.h>
#include "evo.h"


extern SoundRequest		musicRequest;


#define kSplashPictID		131
#define kAmbrosiaLogoPictID	8100
#define kSplashClutID		1000
#define kProgressType		'sp\x95t'
#define kProgressID			128
#define kFadeSpeed			16
#define kMaxCreditLines		128
#define kCreditLineHeight	30
#define kFadeZone			32
#define kCreditFontSize		24
#define kBaseScreenHeight	480
#define kIntroMusicID		30000
#define kDefaultLoopID		30001
#define kLoopTailID			30002

Boolean		screenFaded;
Boolean		musicPairStarted;
Rect		progressRect;
double		progressCount;
double		progressMax;
SndChannelPtr	musicChannel;
Ptr			loopTail;
Ptr			loopExtra;
Ptr			loopHead;
CTabHandle	splashClut;
RGBColor	progressFillColor;
RGBColor	progressEdgeColor;
RGBColor	progressFrameColor;
Handle		progressRes;

// FUNCTION: EVO 0x100414A0
void ShowSplashScreen(void)
{
	PicHandle	pic;
	Rect		r;
	Rect		picRect;
	Rect		srcRect;
	Rect		dstRect;

	if (!screenFaded)
		FadeScreenOut(kFadeSpeed, &rgbBlack);
	pic = GetPicture(kSplashPictID);
	splashClut = GetCTable(kSplashClutID);
	SATSetPortScreen();
	r = gSAT.wind.bounds;
	CenterPict(pic, &r);
	SetMonitorClut(splashClut, 0);
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
	DrawPicture(pic, &r);
	HPurge((Handle)pic);
	ReleaseResource((Handle)pic);
	SATSetPortScreen();
	SetClutToColor(&rgbBlack, 0);
	FadeToClut(kFadeSpeed, splashClut);
	SetMonitorClut(gameClut, 0);
}

// FUNCTION: EVO 0x100415CC
void HideSplashScreen(void)
{
	SATSetPortScreen();
	ForeColor(blackColor);
	SetMonitorClut(splashClut, 0);
	ForeColor(blackColor);
	FadeToColor(24, &rgbBlack);
	SetMonitorClut(gameClut, 0);
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
}

// FUNCTION: EVO 0x1004165C
void ShowAmbrosiaLogo(void)
{
	PicHandle	pic;
	Rect		r;

	pic = GetPicture(kAmbrosiaLogoPictID);
	SATSetPortScreen();
	r = gSAT.wind.bounds;
	CenterPict(pic, &r);
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
	DrawPicture(pic, &r);
	FadeScreenIn(kFadeSpeed);
}

// FUNCTION: EVO 0x100416F4
void HideAmbrosiaLogo(void)
{
	FadeScreenOut(kFadeSpeed, &rgbBlack);
	ForeColor(blackColor);
	PaintRect(&gSAT.wind.bounds);
}

// FUNCTION: EVO 0x1004173C
void InitIntroProgressBar(void)
{
	Rect	bar;
	Rect	line;
	Rect	fill;
	short	i;

	progressRes = GetResource(kProgressType, kProgressID);
	if (progressRes != nil) {
		i = **(short **)progressRes;
		HPurge(progressRes);
		ReleaseResource(progressRes);
	}
	progressMax = i;	/* BUG: i is never set when the resource is missing */
	progressCount = 0.0;

	progressFillColor.red = progressFillColor.blue = 0;
	progressFillColor.green = 65535;
	progressEdgeColor.red = progressEdgeColor.blue = 0;
	progressEdgeColor.green = 40000;
	progressFrameColor.red = progressFrameColor.green = progressFrameColor.blue = 25000;

	i = (gSAT.wind.bounds.left + gSAT.wind.bounds.right) / 2;
	if (gSAT.wind.bounds.bottom - gSAT.wind.bounds.top > kBaseScreenHeight)
		SetRect(&progressRect, i - 100, gSAT.wind.bounds.bottom - 23, i + 100, gSAT.wind.bounds.bottom - 13);
	else
		SetRect(&progressRect, i - 100, gSAT.wind.bounds.bottom - 13, i + 100, gSAT.wind.bounds.bottom - 3);

	SetMonitorClut(splashClut, 0);
	line = progressRect;
	InsetRect(&line, 0, 4);
	while (line.bottom <= progressRect.bottom) {
		RGBForeColor(&progressFrameColor);
		FrameRect(&line);
		ForeColor(blackColor);
		fill = line;
		InsetRect(&fill, 1, 1);
		PaintRect(&fill);
		InsetRect(&line, 0, -1);
		Delay(2, &tix);
	}
	SetMonitorClut(gameClut, 0);
	DrawIntroProgressBar();
}

// FUNCTION: EVO 0x1004198C
void DeltaIntroProgressBar(double amount)
{
	progressCount += amount;
	DrawIntroProgressBar();
}

// FUNCTION: EVO 0x100419CC
void DrawIntroProgressBar(void)
{
	Rect	box;

	SetMonitorClut(splashClut, 0);
	SATSetPort(&loadPort);
	ForeColor(blackColor);
	RGBForeColor(&progressFrameColor);
	FrameRect(&progressRect);
	box = progressRect;
	InsetRect(&box, 1, 1);
	box.right = 198.0 * (progressCount / progressMax) + box.left;
	if (box.right >= progressRect.right)
		box.right = progressRect.right - 1;
	RGBForeColor(&progressFillColor);
	InsetRect(&box, 1, 1);
	PaintRect(&box);
	RGBForeColor(&progressEdgeColor);
	InsetRect(&box, -1, -1);
	FrameRect(&box);
	ForeColor(blackColor);
	box.left = box.right;
	box.right = progressRect.right - 1;
	PaintRect(&box);
	SATSetPortScreen();
	ForeColor(blackColor);
	CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits,
			&progressRect, &progressRect, srcCopy, nil);
	SetMonitorClut(gameClut, 0);
}

// FUNCTION: EVO 0x10041BA0
Boolean ShowScrollingText(short strListID)
{
	Rect		view;
	Rect		box;
	Rect		screen;
	Str255		text;
	Str255		name;
	short		count;
	RGBColor	shade;
	Ptr			line[kMaxCreditLines];
	Handle		strs;
	Boolean		finished;
	short		maxWidth;
	short		width;
	unsigned long	startTicks;
	short		offset;
	short		delay;
	Boolean		shown;
	short		i;
	short		y;

	finished = true;
	strs = GetResource('STR#', strListID);
	if (strs != nil) {
		HLock(strs);
		BlockMoveData(*strs, &count, 2);
		HUnlock(strs);
		HPurge(strs);
		ReleaseResource(strs);
		if (count <= 0)
			return;
	} else
		return;

	FadeToColor(kFadeSpeed, &rgbBlack);
	SATSetPortScreen();
	SetMonitorClut(gameClut, 0);
	ForeColor(blackColor);
	screen = gSAT.wind.bounds;
	screen.top -= 20;
	PaintRect(&screen);
	SetMonitorClut(gameClut, 1);
	PaintRect(&screen);
	maxWidth = 0;
	TextFont(times);
	TextSize(kCreditFontSize);

	if (count > kMaxCreditLines)
		count = kMaxCreditLines;
	for (i = 0; i < count; i++) {
		GetIndString(text, strListID, i + 1);
		width = StringWidth(text);
		if (width > maxWidth)
			maxWidth = width;
		line[i] = nil;
		line[i] = NewPtr(256);
		if (line[i] != nil) {
			if (text[1] == '<' && text[2] == 'R' && text[3] == 'E' && text[4] == 'G' && text[5] == '>') {
				strcpy((char *)text, "   ");
				GetRegisteredOwnerName(name);
				p2cstr(name);
				strcat((char *)text, (char *)name);
				c2pstr((char *)text);
			}
			strncpy(line[i], (char *)text, 250);
		}
	}
	if (maxWidth < 32)
		maxWidth = 32;

	box.left = box.right = (gSAT.wind.bounds.left + gSAT.wind.bounds.right) / 2;
	box.top = box.bottom = (gSAT.wind.bounds.top + gSAT.wind.bounds.bottom) / 2;
	InsetRect(&box, -(maxWidth / 2 + 8), -160);
	OffsetRect(&box, -10, 0);
	view = box;

	SATSetPort(&loadPort);
	TextFont(times);
	TextSize(kCreditFontSize);
	ForeColor(blackColor);
	PaintRect(&loadPort.bounds);

	for (offset = 0; offset <= count * kCreditLineHeight + (view.bottom - view.top); offset++) {
		startTicks = TickCount();
		for (i = 0; i < count; i++) {
			y = view.bottom - offset + i * kCreditLineHeight;
			if (y >= box.top && y <= box.bottom + 24) {
				ForeColor(whiteColor);
				shown = true;
				if (abs(y - box.bottom) <= kFadeZone) {
					if (y >= box.bottom)
						shown = false;
					else {
						shade.red = 65535.0 * (abs(y - box.bottom) / 32.0);
						shade.green = shade.blue = shade.red;
						RGBForeColor(&shade);
					}
				}
				if (abs(y - (box.top + 24)) <= kFadeZone) {
					if (y <= box.top + 24)
						shown = false;
					else {
						shade.red = 65535.0 * (abs(y - (box.top + 24)) / 32.0);
						shade.green = shade.blue = shade.red;
						RGBForeColor(&shade);
					}
				}
				if (shown) {
					MoveTo(view.left + 10, y);
					DrawString((StringPtr)line[i]);
				}
			}
		}
		SATSetPortScreen();
		ForeColor(blackColor);
		CopyBits(&loadPort.port->portBits, &gSAT.wind.port->portBits, &view, &box, srcCopy, nil);
		SATSetPort(&loadPort);
		ForeColor(blackColor);
		PaintRect(&view);
		QuickKeyCheckHandler();
		if (Button() || QuickKeyCheck(kKeyReturn) || QuickKeyCheck(kKeySpace)) {
			finished = false;
			break;
		}
		if (QuickKeyCheck(kKeyDownArrow))
			delay = 4;
		else if (QuickKeyCheck(kKeyUpArrow))
			delay = 0;
		else
			delay = 2;
		while (TickCount() - startTicks < delay)
			;
	}

	for (i = 0; i < count; i++)
		if (line[i] != nil)
			DisposePtr(line[i]);
	SATSetPortScreen();
	return finished;
}

// FUNCTION: EVO 0x1004227C
void StartIntroMusic(void)
{
	OSErr	err;

	if (settings.introMusic) {
		musicChannel = nil;
		err = SndNewChannel(&musicChannel, sampledSynth, initMono, nil);
		if (musicChannel != nil && err == noErr)
			err = SndStartFilePlay(musicChannel, 0, kIntroMusicID, 65534, nil, nil, nil, true);
	}
}

// FUNCTION: EVO 0x10042320
void StopIntroMusic(Boolean beep)
{
	OSErr	err;
	long	level;

	if (musicChannel != nil) {
		err = SndPauseFilePlay(musicChannel);
		if (err != channelNotBusy) {
			SndStopFilePlay(musicChannel, true);
			if (beep && settings.introMusic) {
				GetSysBeepVolume(&level);
				if (level > 0)
					PlaySound(explodeSnd[0], 10, kMidVolume, kMidVolume);
			}
		}
		SndDisposeChannel(musicChannel, true);
		musicChannel = nil;
	}
}

// FUNCTION: EVO 0x100423F4
void LoadAndStartSoundPair(short id)
{
	if (!settings.introMusic)
		return;
	musicPairStarted = true;
	loopHead = nil;
	loopHead = LoadSndResource(id);
	if (loopHead == nil)
		loopHead = LoadSndResource(kDefaultLoopID);
	if (loopHead == nil)
		return;
	loopTail = LoadSndResource(kLoopTailID);
	if (loopTail == nil) {
		DisposePtr(loopHead);
		return;
	}
	musicRequest.snd = loopHead;
	musicRequest.pitch = kFixedOne;
	musicRequest.callback = (SoundCallback)SoundPairCallback;
	musicRequest.priority = 50;
	musicRequest.volLeft = kMidVolume;
	musicRequest.volRight = kMidVolume;
	EnqueueSoundVoice(&musicRequest);
}

// FUNCTION: EVO 0x100424F8
void StopAndDisposeSoundPair(void)
{
	if (settings.introMusic) {
		FlushMixQueueEntries((long)loopHead);
		FlushMixQueueEntries((long)loopTail);
		FlushMixQueueEntries((long)loopTail);	/* BUG: flushes loopTail twice; loopExtra is never flushed */
		if (loopHead != nil)
			DisposePtr(loopHead);
		if (loopTail != nil)
			DisposePtr(loopTail);
		if (loopExtra != nil)
			DisposePtr(loopExtra);
	}
}

// FUNCTION: EVO 0x100425B0
void SoundPairCallback(long message)
{
	if (message != 1)
		return;
	musicRequest.snd = loopTail;
	musicRequest.pitch = kFixedOne;
	musicRequest.callback = (SoundCallback)SoundPairCallback;
	musicRequest.priority = 50;
	musicRequest.volLeft = kMidVolume;
	musicRequest.volRight = kMidVolume;
	EnqueueSoundVoice(&musicRequest);
}
