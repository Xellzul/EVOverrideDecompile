/* macfiles.h -- Mac file helpers. Notes: Research/notes/headers.md */
#ifndef MACFILES_H
#define MACFILES_H

#include "evglobals.h"

OSErr DoEncryption(Handle h, long key);
OSErr OpenResourceForksByType(short refNum, StringPtr fileName, OSType type);
OSErr GetCatalogStartDir(short refNum, StringPtr fileName, long *dirID, short *vRefNum);

#endif
