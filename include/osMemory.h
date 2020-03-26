/**
 * @file osMemory.h  Interface to Memory management.  Depending on compile flag PREMEM, PMM or NMM will be used
 *
 * Copyright (C) 2019-2020 Sean Dai
 */


#ifndef _OS_MEMORY_H
#define _OS_MEMORY_H

#include "osNativeMemory.h"
#include "osPreMemory.h"

#ifdef PREMEM

#ifndef PREMEM_DEBUG
#define osmalloc(size, dh)          osPreMem_alloc(size, dh, false)
#define osmalloc_r(size, dh)        osPreMem_alloc(size, dh, true)
#define osdalloc(src, size, dh)     osPreMem_dalloc(src, size, dh, false)
#define osdalloc_r(src, size, dh)   osPreMem_dalloc(src, size, dh, true)
#define oszalloc(size, dh)          osPreMem_zalloc(size, dh, false)
#define oszalloc_r(size, dh)        osPreMem_zalloc(size, dh, true)
#else
#define osmalloc(size, dh)          osPreMem_allocDebug(size, dh, false, __FILE__, __func__, __LINE__)
#define osmalloc_r(size, dh)        osPreMem_allocDebug(size, dh, true, __FILE__, __func__, __LINE__)
#define osdalloc(src, size, dh)     osPreMem_dallocDebug(src, size, dh, false, __FILE__, __func__, __LINE__)
#define osdalloc_r(src, size, dh)   osPreMem_dallocDebug(src, size, dh, true, __FILE__, __func__, __LINE__)
#define oszalloc(size, dh)          osPreMem_zallocDebug(size, dh, false, __FILE__, __func__, __LINE__)
#define oszalloc_r(size, dh)        osPreMem_zallocDebug(size, dh, true, __FILE__, __func__, __LINE__)
#endif
#define osfree              osPreMem_free
#define osmemref            osPreMem_ref
#define osmem_stat          osPreMem_stat
#define osmem_usedinfo      osPreMem_usedInfo
#define osmem_allusedinfo() osPreMem_usedInfo(-1)

#else

#define osmalloc(size, dh)          osMem_alloc(size, dh)
#define osmalloc_r(size, dh)        osMem_alloc(size, dh)
#define osdalloc(src, size, dh)     osMem_dalloc(src, size, dh)
#define osdalloc_r(src, size, dh)   osMem_dalloc(src, size, dh)
#define oszalloc(size, dh)          osMem_zalloc(size, dh)
#define oszalloc_r(size, dh)        osMem_zalloc(size, dh)
#define osfree              		osMem_deref
#define osmemref            		osMem_ref
#define osmem_stat()          		(void)0
#define osmem_usedinfo(idx)      	(void)0
#define osmem_allusedinfo() 		(void)0

#endif


#endif
