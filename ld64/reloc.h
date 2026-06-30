#ifndef LD64_RELOC_H
#define LD64_RELOC_H

#include "obj.h"
#include "resolve.h"

int reloc_patch(ObjFile *objs, int obj_count, SymMap *sm); /* returns error count */

#endif
