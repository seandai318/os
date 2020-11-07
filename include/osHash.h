/* Copyright (C) 2019, 2020, Sean Dai
 * Interface to hashmap table
 */


#ifndef OS_HASH_H
#define OS_HASH_H

#include <pthread.h>
#include "osPL.h"
#include "osList.h"
#include "osConfig.h"
#include "osTypes.h"
#include "osString.h"


typedef struct osHashBucketInfo {
	osList_t bucketList;
	pthread_mutex_t bucketMutex;
} osHashBucketInfo_t;


/** Defines a hashmap table */
typedef struct osHash {
	osHashBucketInfo_t* bucket;
//    osList_t *bucket;  /**< Bucket with linked lists */
    uint32_t bsize;       /**< Bucket size              */
} osHash_t;


typedef enum {
    OSHASHKEY_STR,
    OSHASHKEY_INT,
    OSHASHKEY_PL,
} osHashKeyType_e;


typedef struct osHashData {
    osHashKeyType_e hashKeyType;
    union {
        osStringInfo_t hashKeyStr;
        uint32_t hashKeyInt;	//this will be used directly as a hash key
		osPLinfo_t hashKeyPL;
    };
    void* pData;
} osHashData_t;


//there are three memory allocation for a hash node, list_element as a hash node (osListElement_t), hashData(osHashData_t), and osHashData_t.pData (user data).
typedef enum {
	OS_HASH_DEL_NODE_TYPE_ALL,				//remove node from Hash, and free all memory
	OS_HASH_DEL_NODE_TYPE_KEEP_USER_DATA,	//remove node from Hash, free osListElement_t and osHashData_t, keep the user data
	OS_HASH_DEL_NODE_TYPE_KEEP_HASH_DATA,	//remove node from Hash only, free osListElement_t
	OS_HASH_DEL_NODE_TYPE_NONE,				//remove node from Hash only, do not free any memory
} osHashDelNodeType_e;

osHash_t* osHash_create(uint32_t bsize);

//osPlHash_addUserData and osPlHash_getUserData has to be used in pair
osListElement_t* osPlHash_addUserData(osHash_t *h, osPointerLen_t* plKey, bool isCase, void* userData);
void* osPlHash_getUserData(osHash_t *h, osPointerLen_t* plKey, bool isCase);
osListElement_t* osPlHash_getElement(osHash_t *h, osPointerLen_t* plKey, bool isCase);

osListElement_t* osHash_add(osHash_t *h,  osHashData_t* pHashData);
void* osHash_replaceUserData(osHash_t *h, osListElement_t* pHashLE, void* newData);
osListElement_t* osHash_lookupByKey(const osHash_t *h, void* key, osHashKeyType_e keyType);
osListElement_t* osHash_addStrkey(osHash_t *h, const char* str, size_t len, bool isCase, void *data);
osListElement_t* osHash_addKey(osHash_t *h, uint32_t key, void *data);
osListElement_t* osHash_addPLkey(osHash_t *h, const osPointerLen_t* pPL, bool isCaseSensitive, void *data);
osListElement_t* osHash_addElement(osHash_t *h, uint32_t key, void *data, osListElement_t* pHashElement);
void osHash_deleteNode(osListElement_t* pHashElement, osHashDelNodeType_e delType);
void osHash_deleteNodeByKey1(const osHash_t *h, uint32_t key, osListApply_h ah, void *arg, osHashDelNodeType_e delType);
void osHash_deleteNodeByKey(const osHash_t *h, osHashData_t* pHashData, osHashDelNodeType_e delType);
uint32_t osHash_getKeyPL(const osPointerLen_t* pPL,bool isCase);
//get hash key for two input keys, one is PL, one is unit8_t
uint32_t osHash_getKeyPL_extraKey(const osPointerLen_t* pPL, bool isCase, uint8_t extraKey);
//pStr is null terminated string
uint32_t osHash_getKeyStr_extraKey(const char* pStr, bool isCase, uint8_t extraKey);
uint32_t osHash_getKeyU8(const uint8_t keyInput);
uint32_t osHash_getKeyU16(const uint16_t keyInput);
uint32_t osHash_getKeyU32(const uint32_t keyInput);
uint32_t osHash_getKeyU64(const uint64_t keyInput);
//osListElement_t* osHash_lookup1(const osHash_t *h, uint32_t key, osListApply_h ah, void *arg);
osListElement_t* osHash_lookup(const osHash_t *h, osHashData_t* pHashData);
osListElement_t* osHash_lookup1(const osHash_t *h, uint32_t key, osListApply_h ah, void *arg);
osListElement_t* osHash_lookupGlobal(const osHash_t *h, osListApply_h ah, void *arg);
osList_t* osHash_getBucketList(const osHash_t *h, uint32_t key);
uint32_t osHash_getBucketSize(const osHash_t *h);
uint32_t osHash_getBucketElementsCount(osListElement_t* pLE);
uint32_t osHash_getBucketElementsCountGlobal(osHash_t* pHash);
void* osHash_getData(osListElement_t* pHashLE);
void osHash_delete(osHash_t *h);
void osHash_clear(osHash_t *h);
uint32_t hash_valid_size(uint32_t size);



#endif
