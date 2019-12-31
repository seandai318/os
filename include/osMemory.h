/**
 * @file osMemory.h  Interface to Memory management with reference counting
 *
 * Copyright (C) 2019 InterLogic
 */


#ifndef _OS_MEMORY_H
#define _OS_MEMORY_H

#include <stdint.h>
#include <stdlib.h>


struct osPrintf;

/**
 * Defines the memory destructor handler, which is called when the reference
 * of a memory object goes down to zero
 *
 * @param data Pointer to memory object
 */
typedef void (*osMemDestroy_h)(void *data);

/** Memory Statistics */
typedef struct osMemStat {
	size_t bytes_cur;    /**< Current bytes allocated      */
	size_t bytes_peak;   /**< Peak bytes allocated         */
	size_t blocks_cur;   /**< Current blocks allocated     */
	size_t blocks_peak;  /**< Peak blocks allocated        */
	size_t size_min;     /**< Lowest block size allocated  */
	size_t size_max;     /**< Largest block size allocated */
} osMemStat_t;


void    *osMem_alloc(size_t size, osMemDestroy_h dh);
void    *osMem_dalloc(const void* src, size_t size, osMemDestroy_h dh);
void    *osMem_zalloc(size_t size, osMemDestroy_h dh);
void    *osMem_realloc(void *data, size_t size);
void    *osMem_reallocArray(void *ptr, size_t nmemb, size_t membsize, osMemDestroy_h dh);
void    *osMem_ref(void *data);
void    *osMem_deref(void *data);
uint32_t osMem_getRefNum(const void *data);

void     osMem_debug(void);
void     osMem_setThreshold(ssize_t n);
int      osMem_status(struct osPrintf *pf, void *unused);
int      osMem_getStat(osMemStat_t *mstat);


#endif
