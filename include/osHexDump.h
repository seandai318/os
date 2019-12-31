/**
 * @file osHexDump.h  Interfaces for hexdump
 *
 * Copyright (C) 2019 InterLogic
 */


#ifndef _OS_HEX_DUMP_H
#define _OS_HEX_DUMP_H

#include <stdarg.h>



void hexdump(FILE *f, const void *p, size_t len);



#endif
