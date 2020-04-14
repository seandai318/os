/**
 * @file osPreMemory.h  Interface to Pre-allocated Memory management with reference counting
 *
 * Copyright (C) 2019-2020 Sean Dai
 */


#ifndef _OS_PRE_MEMORY_H
#define _OS_PRE_MEMORY_H

#include "osTypes.h"


#if 0
#ifndef PREMEM_DEBUG
#define osmalloc(size, dh)  		osPreMem_alloc(size, dh, false)
#define osmalloc_r(size, dh) 		osPreMem_alloc(size, dh, true)
#define osdalloc(src, size, dh)  	osPreMem_dalloc(src, size, dh, false)
#define osdalloc_r(src, size, dh) 	osPreMem_dalloc(src, size, dh, true)
#define oszalloc(size, dh)     		osPreMem_zalloc(size, dh, false)
#define oszalloc_r(size, dh)   		osPreMem_zalloc(size, dh, true)
#else
#define osmalloc(size, dh)  		osPreMem_allocDebug(size, dh, false, __FILE__, __func__, __LINE__)
#define osmalloc_r(size, dh) 		osPreMem_allocDebug(size, dh, true, __FILE__, __func__, __LINE__)
#define osdalloc(src, size, dh)  	osPreMem_dallocDebug(src, size, dh, false, __FILE__, __func__, __LINE__)
#define osdalloc_r(src, size, dh) 	osPreMem_dallocDebug(src, size, dh, true, __FILE__, __func__, __LINE__)
#define oszalloc(size, dh)      	osPreMem_zallocDebug(size, dh, false, __FILE__, __func__, __LINE__)
#define oszalloc_r(size, dh)    	osPreMem_zallocDebug(size, dh, true, __FILE__, __func__, __LINE__)
#endif
#define osfree  			osPreMem_free
#define osmemref			osPreMem_ref
#define osmem_stat			osPreMem_stat
#define osmem_usedinfo		osPreMem_usedInfo
#define osmem_allusedinfo()	osPreMem_usedInfo(-1)
#endif

typedef void (*osPreMemFree_h)(void *data);


//function name ending with 1 does not print memory alloc/dealloc debug info
void* osPreMem_alloc(size_t size, osPreMemFree_h dh, bool isNeedMutex);
void* osPreMem_dalloc(const void* src, size_t size, osPreMemFree_h dh, bool isNeedMutex);
void* osPreMem_zalloc(size_t size, osPreMemFree_h dh, bool isNeedMutex);
void* osPreMem_realloc(void* pData, size_t size);
void* osPreMem_alloc1(size_t size, osPreMemFree_h dh, bool isNeedMutex);
void* osPreMem_dalloc1(const void* src, size_t size, osPreMemFree_h dh, bool isNeedMutex);
void* osPreMem_zalloc1(size_t size, osPreMemFree_h dh, bool isNeedMutex);
void* osPreMem_realloc1(void* pData, size_t size);
void* osPreMem_free(void *pData);
#ifdef PREMEM_DEBUG
void* osPreMem_allocDebug(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line);
void* osPreMem_dallocDebug(const void* src, size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line);
void* osPreMem_zallocDebug(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line);
void* osPreMem_reallocDebug(void* pData, size_t size, char* file, const char* func, int line);
void* osPreMem_allocDebug1(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line);
void* osPreMem_dallocDebug1(const void* src, size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line);
void* osPreMem_zallocDebug1(size_t size, osPreMemFree_h dh, bool isNeedMutex, char* file, const char* func, int line);
void* osPreMem_reallocDebug1(void* pData, size_t size, char* file, const char* func, int line);
void* osPreMem_free1(void *pData);
#endif

void* osPreMem_ref(void *pData);
uint32_t osPreMem_getnrefs(void* pData);
bool osPreMem_isNeedMutex(void* pData);

void osPreMem_init();
//void* osPreMem_get(uint32_t size);
//void osPreMem_release(void* ptr);
//idx: specify which size block to count.  isUnusedCount = true, count for unallocated blocks, =false, count for used blocks
int osPreMem_getCount(uint8_t idx, bool isUnusedCount);
void osPreMem_stat();
//if idx == -1, print all userInfo, otherwise, print individual chunk's userInfo
void osPreMem_usedInfo(int idx);


#endif
