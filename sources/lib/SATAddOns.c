/* SATAddOns.c -- CreateBWOffscreen of the SAT Add-ons library (FastLoad.p), as the Pascal compiler left it */
// COMPILER: Pro5
// LINK-POST: behind the SAT library, before the glue
// COMPILE: -opt off -enum min -sym on -Cpp_exceptions off
// COMPILE-WHY: a statically linked library part built by CodeWarrior Pascal; no Pascal compiler exists here, so the object is written as assembly
#include <Types.h>
#include <Memory.h>
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

extern void CheckNoMem(Ptr p);

// FUNCTION: EVO 0x1007E73C
asm void CreateBWOffscreen(void *portP, const Rect *frame)
{
	nofralloc
	mflr	r0
	stw		r31, -4(r1)
	stw		r0, 8(r1)
	stwu	r1, -0x50(r1)
	mr		r31, r3
	stw		r4, 0x6C(r1)
	stw		r5, 0x70(r1)
	addi	r3, r1, 0x38
	bl		GetPort
	nop
	li		r3, 0
	stw		r3, PORT_DEVICE(r31)
	li		r3, 0x6C
	bl		NewPtr
	nop
	stw		r3, PORT_PORT(r31)
	lwz		r3, PORT_PORT(r31)
	bl		CheckNoMem
	nop
	lwz		r3, PORT_PORT(r31)
	bl		OpenPort
	nop
	lwz		r4, PORT_PORT(r31)
	lwz		r5, 0x6C(r1)
	lwz		r6, 0x70(r1)
	stw		r5, GP_PORTRECT(r4)
	stw		r6, GP_PORTRECT+4(r4)
	lwz		r7, PORT_PORT(r31)
	lwz		r8, PORT_PORT(r31)
	lwz		r9, GP_PORTRECT(r7)
	lwz		r10, GP_PORTRECT+4(r7)
	stw		r9, GP_BOUNDS(r8)
	stw		r10, GP_BOUNDS+4(r8)
	lwz		r11, PORT_PORT(r31)
	lwz		r3, GP_VISRGN(r11)
	addi	r4, r1, 0x6C
	bl		RectRgn
	nop
	addi	r3, r1, 0x6C
	bl		ClipRect
	nop
	lwz		r12, PORT_PORT(r31)
	lha		r12, GP_PORTRECT+2(r12)
	lwz		r3, PORT_PORT(r31)
	lha		r3, GP_PORTRECT+6(r3)
	subf	r12, r12, r3
	addi	r12, r12, 31
	extsh	r12, r12
	srawi	r12, r12, 5
	addze	r12, r12
	slwi	r12, r12, 2
	lwz		r4, PORT_PORT(r31)
	sth		r12, GP_ROWBYTES(r4)
	lwz		r5, PORT_PORT(r31)
	lha		r5, GP_ROWBYTES(r5)
	lwz		r6, PORT_PORT(r31)
	lha		r6, GP_PORTRECT(r6)
	lwz		r7, PORT_PORT(r31)
	lha		r7, GP_PORTRECT+4(r7)
	subf	r6, r6, r7
	extsh	r6, r6
	mullw	r3, r6, r5
	bl		NewPtr
	nop
	lwz		r8, PORT_PORT(r31)
	stw		r3, GP_BASEADDR(r8)
	lwz		r9, PORT_PORT(r31)
	lwz		r3, GP_BASEADDR(r9)
	bl		CheckNoMem
	nop
	lwz		r3, PORT_PORT(r31)
	bl		SetPort
	nop
	lwz		r10, PORT_PORT(r31)
	addi	r3, r10, GP_PORTRECT
	bl		EraseRect
	nop
	lwz		r11, PORT_PORT(r31)
	lwz		r11, GP_BASEADDR(r11)
	stw		r11, PORT_BASEADDR(r31)
	lwz		r12, PORT_PORT(r31)
	lwz		r3, GP_BOUNDS(r12)
	lwz		r4, GP_BOUNDS+4(r12)
	stw		r3, PORT_BOUNDS(r31)
	stw		r4, PORT_BOUNDS+4(r31)
	lwz		r5, PORT_PORT(r31)
	lha		r5, GP_ROWBYTES(r5)
	sth		r5, PORT_ROWBYTES(r31)
	lwz		r3, 0x38(r1)
	bl		SetPort
	nop
	lwz		r0, 0x58(r1)
	addi	r1, r1, 0x50
	mtlr	r0
	lwz		r31, -4(r1)
	blr
}
