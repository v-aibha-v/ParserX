#ifndef SLR_H
#define SLR_H

#include "first_follow.h"
#include "table.h"

LRMachine buildSLRMachine(const Grammar& g);

#endif
