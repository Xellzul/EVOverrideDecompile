/* trigtable.h -- trig tables. Notes: Research/notes/headers.md */
#ifndef TRIGTABLE_H
#define TRIGTABLE_H

#include "evglobals.h"

void BuildTrigTable(void);
void TableSin(short angle);
void TableCos(short angle);
short TableArcTan(float x, float y);

#endif
