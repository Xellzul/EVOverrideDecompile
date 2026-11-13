/* sat.h -- Sprite Animation Toolkit types and calls. Notes: Research/notes/headers.md */
#ifndef SAT_H
#define SAT_H

#include <Quickdraw.h>

#pragma options align=mac68k

typedef struct SATPort {
	GrafPtr		port;
	GDHandle	device;
	Ptr			rows;
	Rect		bounds;
	Ptr			baseAddr;
	short		rowBytes;
} SATPort;

typedef struct SATglobalsRec {
	SATPort		wind;
	short		offSizeH, offSizeV;
	SATPort		offScreen, backScreen;
	long		ox, oy;
	short		pict, bwpict;
	Boolean		fitThePICTs;
	short		sorting, collision, searchWidth;
	PixMapHandle	screen;
	Rect		bounds;
	short		initDepth;
} SATglobalsRec;

typedef struct Face {
	char			*colorData;
	short			resNum;
	BitMap			iconMask;
	short			rowBytes;
	struct Face		*next;
	RgnHandle		maskRgn;
	Ptr				rows, maskRows;
	pascal void		(*redrawProc)(struct Face *, short);
	Ptr				maskData;
} Face, *FacePtr;

typedef struct Sprite {
	short			kind;
	Point			position;
	Rect			hotRect, hotRect2;
	FacePtr			face;
	pascal void		(*task)(struct Sprite *);
	pascal void		(*hitTask)(struct Sprite *, struct Sprite *);
	pascal void		(*destructTask)(struct Sprite *);
	RgnHandle		clip;
	Point			oldpos;
	struct Sprite	*next, *prev;
	Rect			r, oldr;
	Ptr				oldFace;
	Boolean			dirty;
	short			layer;
	Point			speed;
	short			mode;
	Ptr				appPtr;
	long			appLong;
} *SpritePtr;

typedef pascal void (*TaskPtr)(SpritePtr);
typedef pascal void (*EmergencyPtr)(void);

#pragma options align=reset

pascal void SATInstallEmergency(EmergencyPtr emergency);
pascal void CheckNoMem(Ptr p);
pascal OSErr SATMakeOffscreen2(SATPort *port, Rect *r);
pascal void SATMakeOffscreen(SATPort *port, Rect *r);
pascal FacePtr SATNewFace(Rect *faceBounds);
pascal void SATChangedFace(FacePtr theFace);
pascal void SATSetPortFace(FacePtr theFace);
pascal void SATSetPortMask(FacePtr theFace);
pascal void SATSetPort(SATPort *port);
pascal void SATSetPortOffScreen(void);
pascal void SATSetPortBackScreen(void);
pascal void SATSetPortScreen(void);
pascal void SATRedraw(void);
pascal void SATConfigure(Boolean fitPICTs, short sorting, short collision, short searchWidth);
pascal void SATCustomInit(short pict, short bwpict, Rect *windRect, WindowPtr wind, GDHandle device, Boolean useMenuBar, Boolean centerDrawingArea, Boolean fillScreen, Boolean dither4bit, Boolean beSmart);
pascal void SATBackChanged(Rect *r);
pascal SpritePtr SATNewSprite(short kind, short h, short v, TaskPtr setup);
pascal void SATRun(Boolean fast);
pascal void SATSkip(void);
pascal void SATPlotFace(FacePtr face, SATPort *port, Point where, Boolean fast);
pascal void SATPlotFaceToScreen(FacePtr face, Point where, Boolean fast);
pascal void SATSetStrings(Str255 ok, Str255 yes, Str255 no, Str255 quit, Str255 memerr, Str255 noscreen, Str255 nopict, Str255 nowind);
pascal void CreateBWOffscreen(SATPort *port, Rect r);

#endif
