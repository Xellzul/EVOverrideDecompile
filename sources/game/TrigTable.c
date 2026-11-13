/* TrigTable.c -- degree-indexed sine, cosine, tangent and arctangent tables. Notes: Research/notes/TrigTable.md */
#include <Types.h>
#include <math.h>
#include "evo.h"

#define kAtanTableSize	1024

float sinTable[kFullCircle];
float cosTable[kFullCircle];
float tanTable[kFullCircle];
float trigResult;
short atanTable[kAtanTableSize];

// FUNCTION: EVO 0x10058064
void BuildTrigTable(void)
{
	long i;
	double rad;
	double x;
	double deg;
	double dead;

	for (i = 0; i <= kFullCircle - 1; i++) {
		rad = (float)(0.01745329 * (float)i);
		sinTable[i] = sin(rad);
		cosTable[i] = cos(rad);
		tanTable[i] = tan(rad);
	}
	for (i = 0; i <= kAtanTableSize - 1; i++) {
		x = 0.01 * i;
		deg = atan(x);
		deg *= 57.2957795;
		atanTable[i] = deg;
	}
}

// FUNCTION: EVO 0x10058198
void TableSin(short angle)
{
	while (angle < 0)
		angle += kFullCircle;
	while (angle >= kFullCircle)
		angle -= kFullCircle;
	trigResult = sinTable[angle];
}

// FUNCTION: EVO 0x100581d8
void TableCos(short angle)
{
	while (angle < 0)
		angle += kFullCircle;
	while (angle >= kFullCircle)
		angle -= kFullCircle;
	trigResult = cosTable[angle];
}

// FUNCTION: EVO 0x10058218
short TableArcTan(float x, float y)
{
	float ratio;
	short idx;
	short ang;
	long dead;

	if (FloatAbs(x) < FloatAbs(y))
		ratio = FloatAbs(x / y);
	else
		ratio = FloatAbs(y / x);
	idx = 100 * ratio;
	if (idx < 0)
		idx = 0;
	if (idx > kAtanTableSize - 1)
		idx = kAtanTableSize - 1;
	ang = atanTable[idx];
	if (ang < 0)
		ang = -ang;
	if (FloatAbs(x) < FloatAbs(y))
		ang = 90 - ang;
	if (x >= 0.0f && y >= 0.0f)
		ang = ang;
	if (x < 0.0f && y >= 0.0f)
		ang = kHalfCircle - ang;
	if (x < 0.0f && y < 0.0f)
		ang = ang + kHalfCircle;
	if (x >= 0.0f && y < 0.0f)
		ang = -ang;
	ang -= 90;
	if (ang < 0)
		ang += kFullCircle;
	if (ang >= kFullCircle)
		ang -= kFullCircle;
	return ang;
}
