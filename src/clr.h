#ifndef CLR_H
#define CLR_H

#include "first_follow.h"
#include "table.h"

LRMachine buildCLRMachine(const Grammar& g, const FirstFollow& ff);

#endif
