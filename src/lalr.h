#ifndef LALR_H
#define LALR_H

#include "table.h"

LRMachine buildLALRMachine(const Grammar& g, const LRMachine& clrMachine);

#endif
