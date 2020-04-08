/**
 * @file osMemory.c  Memory management with reference counting
 *
 * Copyright (C) 2019 InterLogic
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "osTypes.h"
#include "osList.h"
#include "osPL.h"
#include "osMBuf.h"
#include "osMemory.h"
#include "osPrintf.h"
#include "osDebug.h"		//to-remove

/** Defines a reference-counting memory object */
typedef struct osMemHeader {
	uint32_t nrefs;     		/**< Number of references  */
	osMemDestroy_h dHandler;  	/**< Destroy handler       */
} osMemHeader_t;



/**
 * Allocate a new reference-counted memory object
 *
 * @param size Size of memory object
 * @param dh   Optional destructor, called when destroyed
 *
 * @return Pointer to allocated object
 */
void* osMem_alloc(size_t size, osMemDestroy_h dh)
{
	osMemHeader_t *m;

	m = malloc(sizeof(osMemHeader_t) + size);
	if (!m)
	{
		return NULL;
	}

	m->nrefs = 1;
	m->dHandler = dh;

	mdebug(LM_MEM, "alloc-addr=%p, req size=%ld, nrefs=1", (void*)(m + 1), size);
	return (void*)(m + 1);
}


//the same as osMem_alloc, except ref=n
void* osMem_nalloc(size_t size, osMemDestroy_h dh, uint32_t n)
{
    osMemHeader_t *m;

    m = malloc(sizeof(osMemHeader_t) + size);
    if (!m)
    {
        return NULL;
    }

    m->nrefs = n;
    m->dHandler = dh;

	mdebug(LM_MEM, "alloc-addr=%p, req size=%ld, nrefs=%d", (void*)(m + 1), size, n);
    return (void*)(m + 1);
}


/*allocate a osMemory block, initiates the block with the src content, the new object's nrefs is initiated to 1. */
void* osMem_dalloc(const void* src, size_t size, osMemDestroy_h dh)
{
    osMemHeader_t *m;

    m = malloc(sizeof(osMemHeader_t) + size);
    if (!m)
    {
        return NULL;
    }

    m->nrefs = 1;
    m->dHandler = dh;
	memcpy((void*)(m + 1), src, size);

	mdebug(LM_MEM, "alloc-addr=%p, req size=%ld, nrefs=1", (void*)(m + 1), size);
    return (void*)(m + 1);
}


/**
 * Allocate a new reference-counted memory object. Memory is zeroed.
 *
 * @param size Size of memory object
 * @param dh   Optional destructor, called when destroyed
 *
 * @return Pointer to allocated object
 */
void* osMem_zalloc(size_t size, osMemDestroy_h dh)
{
	void *p;

	p = osMem_alloc(size, dh);
	if (!p)
	{
		return NULL;
	}

	memset(p, 0, size);

	mdebug(LM_MEM, "alloc-addr=%p, req size=%ld, nrefs=1", p, size);
	return p;
}


/**
 * Re-allocate a reference-counted memory object
 *
 * @param pData Previous allocated memory object, not include the memory header
 * @param size New size of memory object
 *
 * @return New pointer to allocated object
 *
 * @note Realloc NULL pointer is not supported
 */
void* osMem_realloc(void* pData, size_t size)
{
	osMemHeader_t *m, *m2;

	if (!pData)
	{
		return NULL;
	}

	m = ((osMemHeader_t *)pData) - 1;

	m2 = realloc(m, sizeof(osMemHeader_t) + size);

	if (!m2)
	{
		return NULL;
	}

	return (void *)(m2 + 1);
}


#ifndef SIZE_MAX
#define SIZE_MAX    (~((size_t)0))
#endif


/**
 * Re-allocate a reference-counted array
 *
 * @param ptr      Pointer to existing array, NULL to allocate a new array
 * @param nmemb    Number of members in array
 * @param membsize Number of bytes in each member
 * @param dh       Optional destructor, only used when ptr is NULL
 *
 * @return New pointer to allocated array
 */
void* osMem_reallocArray(void *ptr, size_t nmemb, size_t membsize, osMemDestroy_h dh)
{
	size_t tsize;

	if (membsize && nmemb > SIZE_MAX / membsize) 
	{
		return NULL;
	}

	tsize = nmemb * membsize;

	if (ptr) 
	{
		return osMem_realloc(ptr, tsize);
	}
	else {
		return osMem_alloc(tsize, dh);
	}
}


/**
 * Reference a reference-counted memory object
 *
 * @param data Memory object
 *
 * @return Memory object (same as data)
 */
void* osMem_ref(void *ptr)
{
	osMemHeader_t *m;

	if (!ptr)
	{
		return NULL;
	}

	m = ((osMemHeader_t *)ptr) - 1;

	++m->nrefs;

	mdebug(LM_MEM, "ref-alloc-addr=%p, nrefs=%d", ptr, m->nrefs);

	return ptr;
}


//assign number of ref number
void* osMem_nref(void *ptr, uint32_t n)
{
    osMemHeader_t *m;

    if (!ptr)
    {
        return NULL;
    }

    m = ((osMemHeader_t *)ptr) - 1;

	m->nrefs = n;

    return ptr;
}


/**
 * Dereference a reference-counted memory object. When the reference count
 * is zero, the destroy handler will be called (if present) and the memory
 * will be freed
 *
 * @param data Memory object
 *
 */
void* osMem_deref(void *pData)
{
	osMemHeader_t *m;

	if (!pData)
	{		
		return NULL;
	}

	m = ((osMemHeader_t *)pData) - 1;

	mdebug(LM_MEM, "de-alloc-addr=%p, nrefs=%ld", pData, m->nrefs);
	if(m->nrefs == 0)
	{
		logError("try to free a already deallocated memory(%p).", pData);
		return NULL;
	}
 
	if (--m->nrefs == 0)
	{
		if (m->dHandler)
		{
			m->dHandler(pData);
		}

		/* NOTE: check if the destructor called osMem_ref() */
		if (m->nrefs == 0)
		{
			free(m);

			return NULL;
		}
	}

	return pData;
}


/**
 * Get number of references to a reference-counted memory object
 *
 * @param data Memory object
 *
 * @return Number of references
 */
uint32_t osMem_getRefNum(const void* pData)
{
	osMemHeader_t *m;

	if (!pData)
	{
		return 0;
	}

	m = ((osMemHeader_t *)pData) - 1;

	return m->nrefs;
}



/**
 * Debug all allocated memory objects
 */
void mem_debug(void)
{
#if MEM_DEBUG
	uint32_t n;

	mem_lock();
	n = list_count(&meml);
	mem_unlock();

	if (!n)
		return;

	DEBUG_WARNING("Memory leaks (%u):\n", n);

	mem_lock();
	(void)list_apply(&meml, true, debug_handler, NULL);
	mem_unlock();
#endif
}


/**
 * Set the memory allocation threshold. This is only used for debugging
 * and out-of-memory simulation
 *
 * @param n Threshold value
 */
void osMem_setThreshold(ssize_t n)
{
#if MEM_DEBUG
	mem_lock();
	threshold = n;
	mem_unlock();
#else
	(void)n;
#endif
}


/**
 * Print memory status
 *
 * @param pf     Print handler for debug output
 * @param unused Unused parameter
 *
 * @return 0 if success, otherwise errorcode
 */
int osMem_status(osPrintf_t *pf, void *unused)
{
#if MEM_DEBUG
	osMem_tstat stat;
	uint32_t c;
	int err = 0;

	(void)unused;

	mem_lock();
	memcpy(&stat, &memstat, sizeof(stat));
	c = list_count(&meml);
	mem_unlock();

	err |= re_hprintf(pf, "Memory status: (%u bytes overhead pr block)\n",
			  sizeof(osMemHeader_t));
	err |= re_hprintf(pf, " Cur:  %u blocks, %u bytes (total %u bytes)\n",
			  stat.blocks_cur, stat.bytes_cur,
			  stat.bytes_cur
			  +(stat.blocks_cur*sizeof(osMemHeader_t)));
	err |= re_hprintf(pf, " Peak: %u blocks, %u bytes (total %u bytes)\n",
			  stat.blocks_peak, stat.bytes_peak,
			  stat.bytes_peak
			  +(stat.blocks_peak*sizeof(osMemHeader_t)));
	err |= re_hprintf(pf, " Block size: min=%u, max=%u\n",
			  stat.size_min, stat.size_max);
	err |= re_hprintf(pf, " Total %u blocks allocated\n", c);

	return err;
#else
	(void)pf;
	(void)unused;
	return 0;
#endif
}


/**
 * Get memory statistics
 *
 * @param mstat Returned memory statistics
 *
 * @return 0 if success, otherwise errorcode
 */
int osMem_getStat(osMemStat_t *mstat)
{
	if (!mstat)
	{
		return EINVAL;
	}
#if MEM_DEBUG
	mem_lock();
	memcpy(mstat, &memstat, sizeof(*mstat));
	mem_unlock();
	return 0;
#else
	return ENOSYS;
#endif
}
