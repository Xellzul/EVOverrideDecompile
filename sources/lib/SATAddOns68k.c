/* SATAddOns68k.c -- CreateBWOffscreen of the SAT Add-ons library (FastLoad.p), 68k, as the Pascal compiler left it */
// COMPILER: Pro30mac (68K)
// LINK-POST: behind the SAT library, last member of CODE 1
// COMPILE: -opt off -enum min -sym on -Cpp_exceptions off
// COMPILE-WHY: a statically linked library part built by CodeWarrior Pascal; no Pascal compiler exists here, so the object is written as assembly
#include <Types.h>
#include <QuickDraw.h>

/* offsets inside SATPort and GrafPort */
#define PORT_PORT		0x00
#define PORT_DEVICE		0x04
#define PORT_BOUNDS		0x0C
#define PORT_BASEADDR	0x14
#define PORT_ROWBYTES	0x18
#define GP_BASEADDR		0x02
#define GP_ROWBYTES		0x06
#define GP_BOUNDS		0x08
#define GP_PORTRECT		0x10
#define GP_VISRGN		0x18

extern void CHECKNOMEM(void);
extern void __lmul__(void);

// FUNCTION: EVO CODE 1 +0x47014
asm pascal void CREATEBWOFFSCREEN(void *portP, const Rect *frame)
{
	link	a6, #-12
	move.l	a2, -(a7)
	movea.l	12(a6), a2
	movea.l	8(a6), a0
	move.l	(a0), -8(a6)
	move.l	4(a0), -4(a6)
	pea		-12(a6)
	dc.w	0xA874
	clr.l	PORT_DEVICE(a2)
	subq.w	#4, a7
	pea		0x6C
	move.l	(a7)+, d0
	dc.w	0xA11E
	move.l	a0, (a7)
	movea.l	(a7)+, a0
	move.l	a0, (a2)
	move.l	(a2), -(a7)
	jsr		CHECKNOMEM
	move.l	(a2), -(a7)
	dc.w	0xA86F
	movea.l	(a2), a0
	move.l	-8(a6), GP_PORTRECT(a0)
	move.l	-4(a6), GP_PORTRECT+4(a0)
	movea.l	(a2), a0
	movea.l	(a2), a1
	move.l	GP_PORTRECT(a0), GP_BOUNDS(a1)
	move.l	GP_PORTRECT+4(a0), GP_BOUNDS+4(a1)
	movea.l	(a2), a0
	move.l	GP_VISRGN(a0), -(a7)
	pea		-8(a6)
	dc.w	0xA8DF
	pea		-8(a6)
	dc.w	0xA87B
	movea.l	(a2), a0
	movea.l	(a2), a1
	move.w	GP_PORTRECT+6(a1), d0
	sub.w	GP_PORTRECT+2(a0), d0
	addi.w	#31, d0
	ext.l	d0
	move.l	d0, d1
	asr.l	#4, d1
	lsr.l	#8, d1
	lsr.l	#8, d1
	lsr.l	#8, d1
	lsr.l	#3, d1
	add.l	d0, d1
	asr.l	#5, d1
	moveq	#4, d0
	jsr		__lmul__
	movea.l	(a2), a0
	move.w	d0, GP_ROWBYTES(a0)
	subq.w	#4, a7
	movea.l	(a2), a0
	movea.l	(a2), a1
	move.w	GP_PORTRECT+4(a1), d0
	sub.w	GP_PORTRECT(a0), d0
	movea.l	(a2), a0
	muls.w	GP_ROWBYTES(a0), d0
	move.l	d0, -(a7)
	move.l	(a7)+, d0
	dc.w	0xA11E
	move.l	a0, (a7)
	movea.l	(a7)+, a0
	movea.l	(a2), a1
	move.l	a0, GP_BASEADDR(a1)
	movea.l	(a2), a0
	move.l	GP_BASEADDR(a0), -(a7)
	jsr		CHECKNOMEM
	move.l	(a2), -(a7)
	dc.w	0xA873
	movea.l	(a2), a0
	pea		GP_PORTRECT(a0)
	dc.w	0xA8A3
	movea.l	(a2), a0
	move.l	GP_BASEADDR(a0), PORT_BASEADDR(a2)
	movea.l	(a2), a0
	move.l	GP_BOUNDS(a0), PORT_BOUNDS(a2)
	move.l	GP_BOUNDS+4(a0), PORT_BOUNDS+4(a2)
	movea.l	(a2), a0
	move.w	GP_ROWBYTES(a0), PORT_ROWBYTES(a2)
	move.l	-12(a6), -(a7)
	dc.w	0xA873
	movea.l	(a7)+, a2
	unlk	a6
	movea.l	(a7)+, a0
	addq.w	#8, a7
	jmp		(a0)
	dc.b	0x91, 'C','R','E','A','T','E','B','W','O','F','F','S','C','R','E','E','N'
	dc.w	0
}
