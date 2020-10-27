/******************************************************************************************************
 * Copyright (C) 2019, 2020, Sean Dai
 * 
 * @file osPreMemory.c
 * This function pre-allocates a chunk of memory blocks for various memory sizes.  Each memory
 * block can be referenced by more than one users/threads. The reference add and removal can be
 * protected by achquiring a mutex from a shared mutex poll.  A user may also chose not to acquire
 * a mutex if a memory block is only used inside a thread.  
 *
 * Be noted the user data within a memory block is not protected by the above mentioned mutex.  So 
 * a user has to be careful to prevent multiple threads modifying/reading user data simultaneously.  
 * If simultaneously modifying/reading ever to happen, a user has to allocate a seperate memory block 
 * to copy the user data instead of referencing the same memory block.
 *
 * If a user requires to acquire a mutex from the mutex poll when allocating a new memory block, and
 * the mutex acquisition fails due to say the mutex poll is empty, the memory block allocation will 
 * fail, and a null pointer will be returned.
 *
 * Other than the shared memory poll, each chunk of memory blocks that has the same memory size share
 * a memory block allocation/deallocation mutex.  In order to allocate or deallocate a memory block, the 
 * associated mutex has to be acquired first.
 ********************************************************************************************************/  


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "osTypes.h"
#include "osDebug.h"
#include "osPL.h"
#include "osPreMemory.h"


#define OS_PREMEM_MAX_IDX       	13		//this number does not include Mutex
#define OS_PREMEM_MAX_CHUNK_SIZE    11000
#define OS_PREMEM_MAX_MUTEX_POLL	30010
#define OS_PREMEM_MAX_DEBUG_SIZE	80
#define OS_PREMEM_MAX_DEBUG_FILE	20
#define OS_PREMEM_MAX_DEBUG_FUNC	20


typedef struct osPreMemHdr {
	uint8_t preMemIdx;
    uint32_t nrefs;             //number of references
	osPreMemFree_h dHandler;	//memory free handler
	pthread_mutex_t* pMutex;
    struct osPreMemHdr* nextBlock;
#ifdef PREMEM_DEBUG
	struct osPreMemHdr* usedPrev;
	struct osPreMemHdr* usedNext;
	char dbgInfo[OS_PREMEM_MAX_DEBUG_SIZE];
#endif
} osPreMemBlockHdr_t;


typedef struct preMemIdx {
	uint32_t size;
    uint32_t count;             //memory block count in the chunk, for used chunk, it is used under PREMEM_DEBUG
#ifdef PREMEM_DEBUG
	uint32_t peakCount;
	uint32_t relCount;
	osPreMemBlockHdr_t* usedHead;
	osPreMemBlockHdr_t* usedTail;
#endif
	pthread_mutex_t	mutex;
	osPreMemBlockHdr_t* blockStart;	//point to the first block of sequentially allocated memory blocks
	osPreMemBlockHdr_t* next;	//point to the currently first block of the memory chunk
	osPreMemBlockHdr_t* end;	//point to the last block of the memory chunk
} osPreMemIdx_t;



static osPreMemBlockHdr_t* osPreMem_allocBlocks(uint8_t idx, uint32_t memSize, uint32_t memNum, osPreMemBlockHdr_t** ppEnd);
static char* osPreMem_usedInfo1(uint8_t idx, int* n, uint32_t* count);
static void* osPreMem_get(uint32_t size, bool isPrintDebug);
static void* osPreMem_alloc_internal(size_t size, osPreMemFree_h dh, bool isNeedMutex, bool isPrintDebug);
static void* osPreMem_realloc_internal(void* pData, size_t size, bool isPrintDebug);
static void osPreMem_release(void* ptr, bool isPrintDebug);
static void* osPreMem_free_internal(void *pData, bool isPrintDebug);
#ifdef PREMEM_DEBUG
static void* osPreMem_allocDebug_internal(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line, bool isPrintDebug);
static void* osPreMem_dallocDebug_internal(const void* src, size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line, bool isPrintDebug);
static void* osPreMem_zallocDebug_internal(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line, bool isPrintDebug);
static void* osPreMem_reallocDebug_internal(void* pData, size_t size, char* file, const char* func, int line, bool isPrintDebug);
#endif

//65536 for trHash and tpServerLB hash, 262144 for proxy hash, 1048576 for reg hash
static uint32_t osPreMemSize[OS_PREMEM_MAX_IDX+1] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 65536, 262144, 1048576, sizeof(pthread_mutex_t)};
static uint32_t osPreMemNum[OS_PREMEM_MAX_IDX+1] = {10010, 10010, 10010, 10010, 10010,10010, 10010,10010, 10010, 10010, 10, 1, 1, OS_PREMEM_MAX_MUTEX_POLL};
static osPreMemIdx_t osPreMemUnused[OS_PREMEM_MAX_IDX+1];
#ifdef PREMEM_DEBUG
static osPreMemIdx_t osPreMemUsed[OS_PREMEM_MAX_IDX+1];
static pthread_mutex_t iallocMutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t ialloc = 0;
static uint32_t totalUsedCount = 0;
#endif


void osPreMem_init()
{
	//sanity check
	for(int i=0; i<OS_PREMEM_MAX_IDX; i++)
	{
		if(osPreMemNum[i] > OS_PREMEM_MAX_CHUNK_SIZE)
		{
			logError("pre-allocated memory size for idx=%d is larger than allowed(%d).", i, OS_PREMEM_MAX_CHUNK_SIZE);
			exit(EXIT_FAILURE);
		}
	}

	for(int i=0; i<=OS_PREMEM_MAX_IDX; i++)
	{
		osPreMemUnused[i].size = osPreMemSize[i];
		osPreMemUnused[i].count = osPreMemNum[i];
#ifdef PREMEM_DEBUG
		osPreMemUnused[i].peakCount = 0;
        osPreMemUnused[i].relCount = 0;
#endif
		osPreMemUnused[i].blockStart = osPreMem_allocBlocks(i, osPreMemSize[i], osPreMemNum[i], &osPreMemUnused[i].end);
		osPreMemUnused[i].next = osPreMemUnused[i].blockStart;
		if(!osPreMemUnused[i].blockStart || !osPreMemUnused[i].end)
		{
			logError("allocate memory for osPreMem[%d] fails, memsize=%d.", i, osPreMemSize[i]);
            exit(EXIT_FAILURE);
		}
		
        if(pthread_mutex_init(&osPreMemUnused[i].mutex, NULL) !=0)
		{
			logError("osPreMemUnused[%d] mutex init failed.", i);
            exit(EXIT_FAILURE);
		}

       	logInfo("osPreMemUnused[%d].blockStart=%p, end=%p, size=%d, count=%d\n", i, osPreMemUnused[i].blockStart, osPreMemUnused[i].end, osPreMemUnused[i].size, osPreMemUnused[i].count);

#ifdef PREMEM_DEBUG
		osPreMemUsed[i].size = osPreMemSize[i];
		osPreMemUsed[i].count = 0;
		osPreMemUsed[i].usedHead = NULL;
		osPreMemUsed[i].usedTail = NULL;

		if(pthread_mutex_init(&osPreMemUsed[i].mutex, NULL) !=0)
        {
            logError("osPreMemUsed[%d] mutex init failed.", i);
            exit(EXIT_FAILURE);
        }
#endif
	}

	pthread_mutex_t* pMutex;
	osPreMemBlockHdr_t* pBlock;
	for(int i=0; i<osPreMemNum[OS_PREMEM_MAX_IDX]; i++)
	{
		pBlock = ((void*)osPreMemUnused[OS_PREMEM_MAX_IDX].blockStart) + (sizeof(osPreMemBlockHdr_t)+osPreMemSize[OS_PREMEM_MAX_IDX])*i;
		pMutex = (pthread_mutex_t*) (pBlock + 1);

		if(pthread_mutex_init(pMutex, NULL) != 0)
		{
            logError("osPreMem mutex poll init failed.");
            exit(EXIT_FAILURE);
		}
	}
}
		

static osPreMemBlockHdr_t* osPreMem_allocBlocks(uint8_t idx, uint32_t memSize, uint32_t memNum, osPreMemBlockHdr_t** ppEnd)
{
	if(memNum == 0 || memSize == 0)
	{
		logError("at least one of the values is set to 0, memNum=%d, memSize=%d.", memNum, memSize);
		return NULL;
	}

	osPreMemBlockHdr_t* preMemBlocks = malloc((sizeof(osPreMemBlockHdr_t)+memSize)*memNum);

	if(!preMemBlocks)
	{
		logError("prealloc memory fails, memSize=%d, memNum=%d.", memSize, memNum);
		return NULL;
	}

	osPreMemBlockHdr_t* ptr = preMemBlocks;
	for(int i=0; i<memNum-1; i++)
	{
		ptr->preMemIdx = idx;
		ptr->nextBlock = ((void*)preMemBlocks) + (sizeof(osPreMemBlockHdr_t)+memSize)*(i+1);
		ptr = ptr->nextBlock;
	}

	ptr->preMemIdx = idx;
	ptr->nextBlock = NULL;
	*ppEnd = ptr;

	return preMemBlocks;
}


static void* osPreMem_get(uint32_t size, bool isPrintDebug)
{
	void* ptr = NULL;

	for(int i=0; i<OS_PREMEM_MAX_IDX; i++)
	{
		if(size <= osPreMemUnused[i].size)
		{
			pthread_mutex_lock(&osPreMemUnused[i].mutex);

			if(!osPreMemUnused[i].next)
			{
				logError("panic! osPreMem(%d] is empty, osPreMem_get for size (%d) fails.", i, size);

	            pthread_mutex_unlock(&osPreMemUnused[i].mutex);

				return NULL;
			}

			osPreMemBlockHdr_t* pBlock = osPreMemUnused[i].next;
			osPreMemUnused[i].next = pBlock->nextBlock;
			pBlock->nextBlock = NULL;
#ifdef PREMEM_DEBUG
			if(osPreMemUnused[i].relCount == 0)
			{
				++osPreMemUnused[i].peakCount;
			}
			else
			{
				--osPreMemUnused[i].relCount;
			}
#endif

			++osPreMemUnused[i].count;

			pthread_mutex_unlock(&osPreMemUnused[i].mutex);

#ifdef PREMEM_DEBUG
            pthread_mutex_lock(&osPreMemUsed[i].mutex);

			pBlock->usedPrev = osPreMemUsed[i].usedTail;
			pBlock->usedNext = NULL;

			if(!osPreMemUsed[i].usedHead)
			{
				osPreMemUsed[i].usedHead = pBlock;
			}

			if(osPreMemUsed[i].usedTail)
			{
				osPreMemUsed[i].usedTail->usedNext = pBlock;
			}
			osPreMemUsed[i].usedTail = pBlock;

			++osPreMemUsed[i].count;

            pthread_mutex_unlock(&osPreMemUsed[i].mutex);
#endif

			if(isPrintDebug)
			{
				mdebug(LM_MEM, "preMemory(%p, size=%u) is allocated.", (void*)(pBlock+1), size);  
			}
			return (void*)(pBlock+1);
		}
	}

	logError("the requested memory size(%d) is larger than any pre-allocated block.", size);
		
	return NULL;
}


static void osPreMem_release(void* ptr, bool isPrintDebug)
{
	if(!ptr)
	{
		return;
	}

	osPreMemBlockHdr_t* pBlock = ((osPreMemBlockHdr_t*)ptr) -1;

	if(pBlock->preMemIdx >= OS_PREMEM_MAX_IDX)
	{
		logError("preMem block has invalid preMemIdx (%d).", pBlock->preMemIdx);
		return;
	}	

    pthread_mutex_lock(&osPreMemUnused[pBlock->preMemIdx].mutex);

	//insert the newly available block to the end of block chain
	if(!osPreMemUnused[pBlock->preMemIdx].next)
	{
		osPreMemUnused[pBlock->preMemIdx].next = pBlock;
		osPreMemUnused[pBlock->preMemIdx].end = pBlock;
	}
	else
	{
		osPreMemUnused[pBlock->preMemIdx].end->nextBlock = pBlock;
		osPreMemUnused[pBlock->preMemIdx].end = pBlock;
	}

#ifdef PREMEM_DEBUG
	++osPreMemUnused[pBlock->preMemIdx].relCount;
#endif

    ++osPreMemUnused[pBlock->preMemIdx].count;

    pthread_mutex_unlock(&osPreMemUnused[pBlock->preMemIdx].mutex);

#ifdef PREMEM_DEBUG
    pthread_mutex_lock(&osPreMemUsed[pBlock->preMemIdx].mutex);

	if(pBlock->usedPrev)
	{
        pBlock->usedPrev->usedNext = pBlock->usedNext;
	}
	else
	{
		osPreMemUsed[pBlock->preMemIdx].usedHead = pBlock->usedNext;
	}

    if(pBlock->usedNext)
    {
        pBlock->usedNext->usedPrev = pBlock->usedPrev;
    }
    else
    {
        osPreMemUsed[pBlock->preMemIdx].usedTail = pBlock->usedPrev;
    }

	pBlock->usedPrev = NULL;
	pBlock->usedNext = NULL;

    --osPreMemUsed[pBlock->preMemIdx].count;

    pthread_mutex_unlock(&osPreMemUsed[pBlock->preMemIdx].mutex);

    pthread_mutex_lock(&iallocMutex);
    --totalUsedCount;
    pthread_mutex_unlock(&iallocMutex);
#endif

	if(isPrintDebug)
	{	
		mdebug(LM_MEM, "preMemory(%p) is deallocated.", ptr);
	}
}


pthread_mutex_t* osPreMem_getMutex()
{
    void* ptr = NULL;

    pthread_mutex_lock(&osPreMemUnused[OS_PREMEM_MAX_IDX].mutex);

    if(!osPreMemUnused[OS_PREMEM_MAX_IDX].next)
    {
        logError("panic! osPreMem mutex poll is empty.");

        pthread_mutex_unlock(&osPreMemUnused[OS_PREMEM_MAX_IDX].mutex);

        return NULL;
    }

    osPreMemBlockHdr_t* pBlock = osPreMemUnused[OS_PREMEM_MAX_IDX].next;
    osPreMemUnused[OS_PREMEM_MAX_IDX].next = pBlock->nextBlock;
    pBlock->nextBlock = NULL;

#ifdef PREMEM_DEBUG
    if(osPreMemUnused[OS_PREMEM_MAX_IDX].relCount == 0)
    {
        ++osPreMemUnused[OS_PREMEM_MAX_IDX].peakCount;
    }
    else
    {
        --osPreMemUnused[OS_PREMEM_MAX_IDX].relCount;
    }
#endif

    pthread_mutex_unlock(&osPreMemUnused[OS_PREMEM_MAX_IDX].mutex);

    return (pthread_mutex_t*)(pBlock+1);
}


void osPreMem_mutexRelease(pthread_mutex_t* ptr)
{
    if(!ptr)
    {
        return;
    }

    osPreMemBlockHdr_t* pBlock = ((osPreMemBlockHdr_t*)ptr) -1;

    if(pBlock->preMemIdx != OS_PREMEM_MAX_IDX)
    {
        logError("preMem mutex has invalid preMemIdx (%d).", pBlock->preMemIdx);
        return;
    }

    pthread_mutex_lock(&osPreMemUnused[OS_PREMEM_MAX_IDX].mutex);

    //insert the newly available mutex to the end of block chain
    if(!osPreMemUnused[OS_PREMEM_MAX_IDX].next)
    {
        osPreMemUnused[OS_PREMEM_MAX_IDX].next = pBlock;
        osPreMemUnused[OS_PREMEM_MAX_IDX].end = pBlock;
    }
    else
    {
        osPreMemUnused[OS_PREMEM_MAX_IDX].end->nextBlock = pBlock;
        osPreMemUnused[OS_PREMEM_MAX_IDX].end = pBlock;
    }

#ifdef PREMEM_DEBUG
	++osPreMemUnused[OS_PREMEM_MAX_IDX].relCount;
#endif

    pthread_mutex_unlock(&osPreMemUnused[OS_PREMEM_MAX_IDX].mutex);
}


int osPreMem_getCount(uint8_t idx, bool isUnusedCount)
{
	int count = -1;

	if(idx > OS_PREMEM_MAX_IDX)
	{
		logError("idx(%d) is larger than the maximum premem idx(%d).", idx, OS_PREMEM_MAX_IDX);
		return count;
	}
	
	count = 0;

    pthread_mutex_lock(&osPreMemUnused[idx].mutex);

	osPreMemBlockHdr_t* pBlock = osPreMemUnused[idx].next;
	while(pBlock != NULL)
	{
		count++;
		pBlock = pBlock->nextBlock;
	}

    pthread_mutex_unlock(&osPreMemUnused[idx].mutex);

	if(!isUnusedCount)
	{
		count = osPreMemNum[idx] - count;
	}

	return count;
}


void osPreMem_stat()
{
    int count, n=0;
    char printBuf[(OS_PREMEM_MAX_IDX+2)*48]={};

	n += sprintf(&printBuf[n], "%s", "  i     size  available  unavailable    peak\n");

    for(int i=0; i<OS_PREMEM_MAX_IDX; i++)
    {
        count = osPreMem_getCount(i, true);
#ifdef PREMEM_DEBUG
        n += sprintf(&printBuf[n], "%3d  %7d  %9d  %11d%8d\n", i, osPreMemSize[i], count,  osPreMemNum[i]-count, osPreMemUnused[i].peakCount);
#else
        n += sprintf(&printBuf[n], "%3d  %7d  %9d  %11d\n", i, osPreMemSize[i], count,  osPreMemNum[i]-count);
#endif
    }
	
	count = osPreMem_getCount(OS_PREMEM_MAX_IDX, true);
#ifdef PREMEM_DEBUG
    sprintf(&printBuf[n], "Mutex Pool:  %10d  %11d%8d\n", count,  osPreMemNum[OS_PREMEM_MAX_IDX]-count, osPreMemUnused[OS_PREMEM_MAX_IDX].peakCount);
#else
	sprintf(&printBuf[n], "Mutex Pool:  %10d  %11d\n", count,  osPreMemNum[OS_PREMEM_MAX_IDX]-count);
#endif
    logInfo("osPreMem statistics:\n%s\n", printBuf);
}


static char* osPreMem_usedInfo1(uint8_t idx, int* n, uint32_t* count)
{
    if(idx >= OS_PREMEM_MAX_IDX)
    {
        logError("idx(%d) is larger than the maximum premem idx(%d).", idx, OS_PREMEM_MAX_IDX-1);
        return NULL;
    }

#ifndef PREMEM_DEBUG
    logInfo("need to turn on PREMEM_DEBUG first.");
	return NULL;
#else
	char* dbgPrint = NULL;
	uint32_t ic = 0;
    pthread_mutex_lock(&osPreMemUsed[idx].mutex);
	*count = osPreMemUsed[idx].count;
    ic = *count;

	*n=0;
	if(*count == 0)
	{
		goto EXIT;
	}

    dbgPrint = malloc(*count*OS_PREMEM_MAX_DEBUG_SIZE);
	osPreMemBlockHdr_t* ple = osPreMemUsed[idx].usedHead;
	while(ple)
	{
		*n += sprintf(&dbgPrint[*n], "%s", ple->dbgInfo);
		ple = ple->usedNext;
		--ic;
	}

EXIT:
    pthread_mutex_unlock(&osPreMemUsed[idx].mutex);

	if(ic)
	{
		logError("panic! used memory(size=%d) in PMM does not match with count, remaining count=0x%lx.", osPreMemSize[idx], ic);
	}

	return dbgPrint;
#endif
}


//if idx == -1, print all userInfo, otherwise, print individual chunk's userInfo
void osPreMem_usedInfo(int idx)
{
    if(idx >= OS_PREMEM_MAX_IDX || idx < -1)
    {
        logError("idx(%d) is not in the range (-1 ~ %d).", idx, OS_PREMEM_MAX_IDX-1);
        return;
    }

#ifndef PREMEM_DEBUG
    logInfo("need to turn on PREMEM_DEBUG first.");
#else
	int n;
	uint32_t count;
	char* dbgPrint = NULL;
	if(idx >= 0)
	{
		dbgPrint = osPreMem_usedInfo1(idx, &n, &count);
		if(!dbgPrint || !n)
		{
			logInfo("used pre-memory(size=%d): None.", osPreMemSize[idx]);
			return;
		}	
		
		logInfo("used pre-memory (size=%d), total Count=%d:\n%s", osPreMemSize[idx], count, dbgPrint);
		free(dbgPrint);
		return;
	}
	else
	{
		uint32_t totalCount = 0;

		//add extra 1 besides OS_PREMEM_MAX_CHUNK_SIZE in malloc to account for extra bytes like "size=%d:\n"
		char* dbgPrintTotal = malloc((OS_PREMEM_MAX_CHUNK_SIZE+1)*OS_PREMEM_MAX_DEBUG_SIZE);
		int prtIndex = 0;
		for(int i=0; i<OS_PREMEM_MAX_IDX; i++)
		{
			dbgPrint = osPreMem_usedInfo1(i, &n, &count);
			if(dbgPrint && n)
			{
				//print ouf the existing statistics if the remaining bytes are not big enough
				if((prtIndex+ n) >= OS_PREMEM_MAX_CHUNK_SIZE * OS_PREMEM_MAX_DEBUG_SIZE)
				{
					dbgPrintTotal[prtIndex ? --prtIndex : 0] = 0;
					logInfo("used pre-memory, total count=%d:\n%s", totalCount, dbgPrintTotal);
					prtIndex = 0;
				}
					
				prtIndex += sprintf(&dbgPrintTotal[prtIndex], "size=%d, count=%d:\n", osPreMemSize[i], count);
				memcpy(&dbgPrintTotal[prtIndex], dbgPrint, n);
				prtIndex += n;
				dbgPrintTotal[prtIndex++] = '\n';
			
				totalCount += count;
				
				free(dbgPrint);
			}
		}
	
		dbgPrintTotal[prtIndex ? --prtIndex : 0] = 0;
		logInfo("used pre-memory, total count=%d:\n%s", totalCount, dbgPrintTotal);

		free(dbgPrintTotal);
	}
#endif
}


static void* osPreMem_alloc_internal(size_t size, osPreMemFree_h dh, bool isNeedMutex, bool isPrintDebug)
{
	void* pMem;

    pMem = osPreMem_get(size, isPrintDebug);
    if (!pMem)
    {
        return NULL;
    }

	osPreMemBlockHdr_t* ptr = ((osPreMemBlockHdr_t*)pMem) -1;

    ptr->nrefs = 1;
    ptr->dHandler = dh;
	if(isNeedMutex)
	{
		ptr->pMutex = osPreMem_getMutex();
		if(!ptr->pMutex)
		{
			logError("fails to osPreMem_getMutex.");
			osPreMem_release(pMem, isPrintDebug);
			pMem = NULL;
		}
	}
	else
	{
		ptr->pMutex = NULL;
	}

    return pMem;
}


void* osPreMem_alloc(size_t size, osPreMemFree_h dh, bool isNeedMutex)
{
	return osPreMem_alloc_internal(size, dh, isNeedMutex, true);
}


void* osPreMem_alloc1(size_t size, osPreMemFree_h dh, bool isNeedMutex)
{
    return osPreMem_alloc_internal(size, dh, isNeedMutex, false);
}


/*allocate a osMemory block, initiates the block with the src content, the new object's nrefs is initiated to 1. */
void* osPreMem_dalloc(const void* src, size_t size, osPreMemFree_h dh, bool isNeedMutex)
{
    void* pMem = osPreMem_alloc(size, dh, isNeedMutex);
    if (!pMem)
    {
        return NULL;
    }

    memcpy(pMem, src, size);

    return pMem;
}


void* osPreMem_dalloc1(const void* src, size_t size, osPreMemFree_h dh, bool isNeedMutex)
{
    void* pMem = osPreMem_alloc1(size, dh, isNeedMutex);
    if (!pMem)
    {
        return NULL;
    }

    memcpy(pMem, src, size);

    return pMem;
}


// Allocate a new reference-counted memory object. Memory is zeroed.
void* osPreMem_zalloc(size_t size, osPreMemFree_h dh, bool isNeedMutex)
{
    void* ptr = osPreMem_alloc(size, dh, isNeedMutex);
    if (!ptr)
    {
        return NULL;
    }

    memset(ptr, 0, size);

    return ptr;
}


void* osPreMem_zalloc1(size_t size, osPreMemFree_h dh, bool isNeedMutex)
{
    void* ptr = osPreMem_alloc1(size, dh, isNeedMutex);
    if (!ptr)
    {
        return NULL;
    }

    memset(ptr, 0, size);

    return ptr;
}



//pData is the data part of osPreMem data structure
//dHandler and whether require mutex inherent from pData
static void* osPreMem_realloc_internal(void* pData, size_t size, bool isPrintDebug)
{
	void* pMem = NULL;

    if (!pData)
    {
        return NULL;
    }

	if(size > 0)
	{	
    	osPreMemBlockHdr_t* ptr = ((osPreMemBlockHdr_t *)pData) - 1;
    	osPreMemFree_h dh = ptr->dHandler;
		bool isNeedMutex = ptr->pMutex ? true : false;

		if(isPrintDebug)
		{
    		pMem = osPreMem_dalloc(pData, size, dh, isNeedMutex);
		}
		else
		{
            pMem = osPreMem_dalloc1(pData, size, dh, isNeedMutex);
		}
	}

	if(isPrintDebug)
	{
		osPreMem_free(pData);
	}
	else
	{
        osPreMem_free1(pData);
    }

    return pMem;
}


void* osPreMem_realloc(void* pData, size_t size)
{
	return osPreMem_realloc_internal(pData, size, true);
}


void* osPreMem_realloc1(void* pData, size_t size)
{
    return osPreMem_realloc_internal(pData, size, false);
}


//pData is the data part of osPreMem data structure
void* osPreMem_ref(void *pData)
{
    osPreMemBlockHdr_t *ptr;

    if (!pData)
    {
        return NULL;
    }

    ptr = ((osPreMemBlockHdr_t *)pData) - 1;

	ptr->pMutex ? pthread_mutex_lock(ptr->pMutex) : (void)0;

	if(!ptr->nrefs)
	{
		ptr->pMutex ? pthread_mutex_unlock(ptr->pMutex) : (void)0;
		logError("ptr->nrefs is NULL.");
		return NULL;
	}

    ++ptr->nrefs;

	ptr->pMutex ? pthread_mutex_unlock(ptr->pMutex) : (void)0;

    return pData;
}


static void* osPreMem_free_internal(void *pData, bool isPrintDebug)
{
    if (!pData)
    {
        return NULL;
    }

    osPreMemBlockHdr_t* ptr = ((osPreMemBlockHdr_t *)pData) - 1;

    ptr->pMutex ? pthread_mutex_lock(ptr->pMutex) : (void)0;

	if(ptr->nrefs == 0)
	{
        ptr->pMutex ? pthread_mutex_unlock(ptr->pMutex) : (void)0;
		logError("try to free a memory(%p) that has nrefs=0.", pData);
		return NULL;
	}

    if (--ptr->nrefs == 0)
    {
        if (ptr->dHandler)
        {
            ptr->dHandler(pData);
        }

        // do not free own if dhandler adds reference to the ptr
        if (ptr->nrefs == 0)
        {
    		ptr->pMutex ? pthread_mutex_unlock(ptr->pMutex) : (void)0;
			osPreMem_mutexRelease(ptr->pMutex);

            osPreMem_release(pData, isPrintDebug);

            return NULL;
        }
    }

    ptr->pMutex ? pthread_mutex_unlock(ptr->pMutex) : (void)0;

    return pData;
}


void* osPreMem_free(void *pData)
{
	return osPreMem_free_internal(pData, true);
}


void* osPreMem_free1(void *pData)
{
    return osPreMem_free_internal(pData, false);
}


uint32_t osPreMem_getnrefs(void* pData)
{
    if (!pData)
    {
        return 0;
    }

	uint32_t nrefs = 0;
    osPreMemBlockHdr_t* ptr = ((osPreMemBlockHdr_t *)pData) - 1;

    ptr->pMutex ? pthread_mutex_lock(ptr->pMutex) : (void)0;

	nrefs = ptr->nrefs;
	
    ptr->pMutex ? pthread_mutex_unlock(ptr->pMutex) : (void)0;

	return nrefs;
}


bool osPreMem_isNeedMutex(void* pData)
{
	if(!pData)
	{
		return false;
	}

    osPreMemBlockHdr_t* ptr = ((osPreMemBlockHdr_t *)pData) - 1;
	return ptr->pMutex ? true : false;
}


#ifdef PREMEM_DEBUG
static void* osPreMem_allocDebug_internal(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line, bool isPrintDebug)
{
	void* pMem = NULL;
	if(isPrintDebug)
	{
		pMem = osPreMem_alloc(size, dh, isNeedMutex);
	}
	else
	{
		pMem = osPreMem_alloc1(size, dh, isNeedMutex);
    }
	if(!pMem)
	{
		return NULL;
	}

    uint32_t allocTime = 0;
    pthread_mutex_lock(&iallocMutex);
    allocTime = ialloc++;
	++totalUsedCount;
    pthread_mutex_unlock(&iallocMutex);

	osPreMemBlockHdr_t* ptr = ((osPreMemBlockHdr_t*)pMem) -1;

	int n = 0;
    n +=sprintf(&ptr->dbgInfo[n], "%p ", pMem);

	int m = snprintf(&ptr->dbgInfo[n], OS_PREMEM_MAX_DEBUG_FILE, "%s", file);
	if(m >= OS_PREMEM_MAX_DEBUG_FILE)
	{
		m = OS_PREMEM_MAX_DEBUG_FILE-1;
	}
	n += m;

	m = snprintf(&ptr->dbgInfo[n], OS_PREMEM_MAX_DEBUG_FUNC+1, ":%s", func);
    if(m > OS_PREMEM_MAX_DEBUG_FUNC)
    {
        m = OS_PREMEM_MAX_DEBUG_FUNC;
    }
	n += m;

    line = line > 99999 ? 99999 : line;
	m = sprintf(&ptr->dbgInfo[n], ":%d", line);
	n += m;

	n += sprintf(&ptr->dbgInfo[n], ":0x%x", allocTime);
	
	ptr->dbgInfo[n] = '\n';
	return pMem;
}


void* osPreMem_allocDebug(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line)
{
	return osPreMem_allocDebug_internal(size, dh, isNeedMutex, file, func, line, true);
}


void* osPreMem_allocDebug1(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line)
{
    return osPreMem_allocDebug_internal(size, dh, isNeedMutex, file, func, line, false);
}


static void* osPreMem_dallocDebug_internal(const void* src, size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line, bool isPrintDebug)
{
    void* pMem = osPreMem_allocDebug(size, dh, isNeedMutex, file, func, line);
    if (!pMem)
    {
        return NULL;
    }

    memcpy(pMem, src, size);

    return pMem;
}


void* osPreMem_dallocDebug(const void* src, size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line)
{
	return osPreMem_dallocDebug_internal(src, size, dh, isNeedMutex, file, func, line, true);
}


void* osPreMem_dallocDebug1(const void* src, size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line)
{
    return osPreMem_dallocDebug_internal(src, size, dh, isNeedMutex, file, func, line, false);
}


static void* osPreMem_zallocDebug_internal(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line, bool isPrintDebug)
{
    void* ptr = osPreMem_allocDebug(size, dh, isNeedMutex, file, func, line);
    if (!ptr)
    {
        return NULL;
    }

    memset(ptr, 0, size);

    return ptr;
}


void* osPreMem_zallocDebug(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line)
{
	return osPreMem_zallocDebug_internal(size, dh, isNeedMutex, file, func, line, true);
}


void* osPreMem_zallocDebug1(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line)
{
    return osPreMem_zallocDebug_internal(size, dh, isNeedMutex, file, func, line, false);
}


static void* osPreMem_reallocDebug_internal(void* pData, size_t size, char* file, const char* func, int line, bool isPrintDebug)
{
    void* pMem = NULL;

    if (!pData)
    {
        return NULL;
    }

    if(size > 0)
    {
        osPreMemBlockHdr_t* ptr = ((osPreMemBlockHdr_t *)pData) - 1;
        osPreMemFree_h dh = ptr->dHandler;
        bool isNeedMutex = ptr->pMutex ? true : false;

        pMem = osPreMem_dallocDebug(pData, size, dh, isNeedMutex, file, func, line);
    }

    osPreMem_free(pData);

    return pMem;
}


void* osPreMem_reallocDebug(void* pData, size_t size, char* file, const char* func, int line)
{
	return osPreMem_reallocDebug_internal(pData, size, file, func, line, true);
}


void* osPreMem_reallocDebug1(void* pData, size_t size, char* file, const char* func, int line)
{
    return osPreMem_reallocDebug_internal(pData, size, file, func, line, false);
}


#endif
