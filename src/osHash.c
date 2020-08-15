/**
 * @file osHash.c  Hashmap table
 * Copyright (C) 2019, 2020, Sean Dai
 */

#include <string.h>
#include <pthread.h>
#include "osTypes.h"
#include "osMemory.h"
#include "osMBuf.h"
#include "osList.h"
#include "osHash.h"
#include "osHashKey.h"
#include "osDebug.h"
#include "osString.h"



static uint32_t osHash_getKeyStr(const char* str, size_t len, bool isCase);
//static uint32_t osHash_getKeyPL(const osPointerLen_t* pPL,bool isCase);
static bool osHashCompare(osListElement_t *le, void *data);


static void osHash_destructor(void *data)
{
	osHash_t *h = data;

	osfree(h->bucket);
}


/**
 * Allocate a new hashmap table
 *
 * @param hp     Address of newly created hashmap pointer
 * @param bsize  Bucket size
 *
 * @return 0 if success, otherwise errorcode
 */
osHash_t* osHash_create(uint32_t bsize)
{
	osHash_t* h;
	int err = 0;

	if (!bsize)
	{
		return NULL;
	}

	//normalize bucket size
    uint32_t x;
    for (x=0; (uint32_t)1<<x < bsize && x < 31; x++)
	{
        continue;
	}
    bsize = 1<<x;

	h = oszalloc(sizeof(osHash_t), osHash_destructor);
	if (!h)
	{
        logError("fails to oszalloc for h");

		return NULL;
	}

	h->bsize = bsize;

	h->bucket = oszalloc(bsize*sizeof(osHashBucketInfo_t), NULL);
	if (!h->bucket) 
	{
		logError("fails to oszalloc for h->bucket, bsize=%u, requiredSize=%u.", bsize, bsize*sizeof(osHashBucketInfo_t));
		err = 1;
		goto EXIT;
	}
	for(int i=0; i<bsize; i++)
	{
		pthread_mutex_init(&h->bucket[i].bucketMutex, NULL);
	} 

 EXIT:
	if (err)
	{
		osfree(h);
		return NULL;
	}
	else
	{
		printf("aaa\n");
		return h;
	}
}


osListElement_t* osHash_add(osHash_t *h, osHashData_t* pHashData)
{
    if(!h || !pHashData)
    {
        logError("invalid NULL passing in parameters, h=%p, pHashData=%p.", h, pHashData);
        return NULL;
    }

    osListElement_t* pLE= NULL;
    switch (pHashData->hashKeyType)
    {
        case OSHASHKEY_STR:
            pLE = osHash_addStrkey(h, pHashData->hashKeyStr.str, pHashData->hashKeyStr.len, pHashData->hashKeyStr.isCase, pHashData);
            break;
        case OSHASHKEY_INT:
            pLE = osHash_addKey(h, pHashData->hashKeyInt, pHashData);
            break;
        case OSHASHKEY_PL:
            pLE = osHash_addPLkey(h, pHashData->hashKeyPL.pPL, pHashData->hashKeyPL.isCase, pHashData);
            break;
        default:
            logError("invalid hashKeyType (%d)", pHashData->hashKeyType);
			goto out;
    }

out:
	return pLE;
}



/*
 * len: the string len. If len=0, then the string is NULL terminated
 */
osListElement_t* osHash_addStrkey(osHash_t *h, const char* str, size_t len, bool isCase, void *data)
{
	if(!h || !str)
	{
		return NULL;
	}

	uint32_t hashKey = osHash_getKeyStr(str, len, isCase);

	return osHash_addElement(h, hashKey, data, NULL);	
}


osListElement_t* osHash_addKey(osHash_t *h, uint32_t key, void *data)
{
	return osHash_addElement(h, key, data, NULL);
}


osListElement_t* osHash_addPLkey(osHash_t *h, const osPointerLen_t* pPL, bool isCase, void *data)
{
    if(!h || !pPL)
    {
        return NULL;
    }

    uint32_t hashKey = osHash_getKeyPL(pPL, isCase);

	return osHash_addElement(h, hashKey, data, NULL);
}



uint32_t osHash_getBucketElementsCount(osListElement_t* pLE)
{
	if(!pLE)
	{
		logError("pLE is empty");
		return 0;
	}

	uint32_t count = 0;
    osList_t* pList = pLE->list;
    if(!pList)
    {
        logError("the hash list element is not linked in");
        return 0;
    }

	pthread_mutex_t* pMutex = &((osHashBucketInfo_t*)pList)->bucketMutex;
    pthread_mutex_lock(pMutex);

	count = osList_getCount(pList);

    pthread_mutex_unlock(pMutex);

	return count;	
}



uint32_t osHash_getBucketElementsCountGlobal(osHash_t* pHash)
{
	if(!pHash)
	{
		logError("pHash is NULL");
		return 0;
	}

	uint32_t count=0;
	for (int i=0; i<pHash->bsize; i++)
	{
		pthread_mutex_lock(&pHash->bucket[i].bucketMutex);
    	osList_t* pList = &pHash->bucket[i].bucketList;
		count += osList_getCount(pList);
		pthread_mutex_unlock(&pHash->bucket[i].bucketMutex);
	}

	return count;	
}


	
/**
 * Add an element to the hashmap table
 *
 * @param h      Hashmap table
 * @param key    Hash key
 * @param data   Element data
 * @param pHashElement	return the allocated hash element if it is not NULL
 */
osListElement_t* osHash_addElement(osHash_t *h, uint32_t key, void *data, osListElement_t* pHashElement)
{
	if (!h)
	{
		return NULL;
	}

	osListElement_t* pLE;
	uint32_t index = key & (h->bsize-1);

	pthread_mutex_lock(&h->bucket[index].bucketMutex);
	if(pHashElement ==  NULL)
	{
		pLE = osList_append(&h->bucket[index].bucketList, data);
	}
	else
	{
		osList_appendLE(&h->bucket[index].bucketList, pHashElement, data);
		pLE = pHashElement;
	}
	pthread_mutex_unlock(&h->bucket[index].bucketMutex);

	return pLE;
}


/**
 * Remove a hash element from the hashmap table, it is caller's responsibility to free the element
 *
 * @param le     List element
 */
void osHash_deleteNode(osListElement_t* pHashElement, osHashDelNodeType_e delType)
{
	if(!pHashElement)
	{
		logError("null pointer, pHashElement.");
		return;
	}

    osList_t* pList = pHashElement->list;
    if(!pList)
    {
        logError("the hash list element is not linked in");
        return;
    }

    pthread_mutex_t* pMutex = &((osHashBucketInfo_t*)pList)->bucketMutex;
    pthread_mutex_lock(pMutex);

    osList_unlinkElement(pHashElement);

    pthread_mutex_unlock(pMutex);

    switch(delType)
    {
        case OS_HASH_DEL_NODE_TYPE_ALL:
			osfree(((osHashData_t*)pHashElement->data)->pData);
            osfree(pHashElement->data);
            osfree(pHashElement);
            break;
        case OS_HASH_DEL_NODE_TYPE_KEEP_USER_DATA:
			osfree(pHashElement->data);
            osfree(pHashElement);
            break;
		case OS_HASH_DEL_NODE_TYPE_KEEP_HASH_DATA:
			osfree(pHashElement);
        default:
            break;
    }
}


void osHash_deleteNodeByKey1(const osHash_t *h, uint32_t key, osListApply_h ah, void *arg, osHashDelNodeType_e delType)
{
	osListElement_t* pHE = osHash_lookup1(h, key, ah, arg);

	osHash_deleteNode(pHE, delType);
}


void osHash_deleteNodeByKey(const osHash_t *h, osHashData_t* pHashData, osHashDelNodeType_e delType)
{
    if(!h || !pHashData)
    {
        logError("invalid NULL passing in parameters");
        return;
    }

    uint32_t hashKey = 0;
    switch (pHashData->hashKeyType)
    {
        case OSHASHKEY_STR:
            hashKey = osHash_getKeyStr(pHashData->hashKeyStr.str, pHashData->hashKeyStr.len, pHashData->hashKeyStr.isCase);
            break;
        case OSHASHKEY_INT:
            hashKey = pHashData->hashKeyInt;
            break;
        case OSHASHKEY_PL:
            hashKey = osHash_getKeyPL(pHashData->hashKeyPL.pPL, pHashData->hashKeyPL.isCase);
            break;
        default:
            logError("invalid hashKeyType (%d)", pHashData->hashKeyType);
            return;
    }

	osListElement_t* pLE = NULL;
    pthread_mutex_lock(&h->bucket[hashKey & (h->bsize-1)].bucketMutex);

    pLE = osList_lookup(&h->bucket[hashKey & (h->bsize-1)].bucketList, true, osHashCompare, pHashData);

    osList_unlinkElement(pLE);

    pthread_mutex_unlock(&h->bucket[hashKey & (h->bsize-1)].bucketMutex);

    switch(delType)
    {
        case OS_HASH_DEL_NODE_TYPE_ALL:
            osfree(((osHashData_t*)pLE->data)->pData);
            osfree(pLE->data);
            osfree(pLE);
            break;
        case OS_HASH_DEL_NODE_TYPE_KEEP_USER_DATA:
            osfree(pLE->data);
            osfree(pLE);
            break;
        case OS_HASH_DEL_NODE_TYPE_KEEP_HASH_DATA:
            osfree(pLE);
        default:
            break;
    }
}


/**
 * Apply a handler function to all elements in the hashmap with a matching key
 *
 * @param h   Hashmap table
 * @param key Hash key
 * @param ah  Apply handler
 * @param arg Handler argument
 *
 * @return List element if traversing stopped, otherwise NULL
 */
osListElement_t* osHash_lookup1(const osHash_t *h, uint32_t key, osListApply_h ah, void *arg)
{
	if (!h || !ah)
	{
		return NULL;
	}

	pthread_mutex_lock(&h->bucket[key & (h->bsize-1)].bucketMutex);
	osListElement_t* pLE = osList_lookup(&h->bucket[key & (h->bsize-1)].bucketList, true, ah, arg);
	pthread_mutex_unlock(&h->bucket[key & (h->bsize-1)].bucketMutex);

	return pLE;
}


osListElement_t* osHash_lookup(const osHash_t *h, osHashData_t* pHashData)
{
	if(!h || !pHashData)
	{
		logError("invalid NULL passing in parameters");
		return NULL;
	}

	uint32_t hashKey = 0;
	switch (pHashData->hashKeyType)
	{
		case OSHASHKEY_STR:
			hashKey = osHash_getKeyStr(pHashData->hashKeyStr.str, pHashData->hashKeyStr.len, pHashData->hashKeyStr.isCase);
			break;
		case OSHASHKEY_INT:
			hashKey = pHashData->hashKeyInt;
			break;
		case OSHASHKEY_PL:
			hashKey = osHash_getKeyPL(pHashData->hashKeyPL.pPL, pHashData->hashKeyPL.isCase);
			break;
		default:
			logError("invalid hashKeyType (%d)", pHashData->hashKeyType);
			return NULL;
	}

	return osHash_lookup1(h, hashKey, osHashCompare, pHashData);
}


//this function assume the key information is set in osHashData_t 
osListElement_t* osHash_lookupByKey(const osHash_t *h, void* key, osHashKeyType_e keyType)
{
    if (!h || !key)
    {
        return NULL;
    }

	osHashData_t hashData;
	hashData.hashKeyType = keyType;

    uint32_t hashKey = 0;
	switch(keyType)
	{
		case OSHASHKEY_STR:
			hashData.hashKeyStr = *(osStringInfo_t*)key;
            hashKey = osHash_getKeyStr(hashData.hashKeyStr.str, hashData.hashKeyStr.len, hashData.hashKeyStr.isCase);
			break;
		case OSHASHKEY_INT:
			hashData.hashKeyInt = *(uint32_t*)key;
			hashKey = hashData.hashKeyInt;
			break;
		case OSHASHKEY_PL:
			hashData.hashKeyPL = *(osPLinfo_t*)key;
            hashKey = osHash_getKeyPL(hashData.hashKeyPL.pPL, hashData.hashKeyPL.isCase);
			break;
		default:
            logError("invalid hashKeyType (%d)", keyType);
			return NULL;
	}

    return osHash_lookup1(h, hashKey, osHashCompare, &hashData);
}


/**
 * Apply a handler function to all elements in the hashmap
 *
 * @param h   Hashmap table
 * @param ah  Apply handler
 * @param arg Handler argument
 *
 * @return List element if traversing stopped, otherwise NULL
 */
osListElement_t* osHash_lookupGlobal(const osHash_t *h, osListApply_h ah, void *arg)
{
	osListElement_t *le = NULL;
	uint32_t i;

	if (!h || !ah)
	{
		return NULL;
	}

	for (i=0; (i<h->bsize) && !le; i++)
	{
		pthread_mutex_lock(&h->bucket[i].bucketMutex);
		le = osList_lookup(&h->bucket[i].bucketList, true, ah, arg);
		pthread_mutex_unlock(&h->bucket[i].bucketMutex);
	}

	return le;
}


/**
 * Return bucket list for a given index
 *
 * @param h   Hashmap table
 * @param key Hash key
 *
 * @return Bucket list if valid input, otherwise NULL
 */
osList_t* osHash_getBucketList(const osHash_t *h, uint32_t key)
{
	return h ? &h->bucket[key & (h->bsize - 1)].bucketList : NULL;
}


/**
 * Get hash bucket size
 *
 * @param h Hashmap table
 *
 * @return hash bucket size
 */
uint32_t osHash_getBucketSize(const osHash_t *h)
{
	return h ? h->bsize : 0;
}


/**
 * Flush a hashmap and free all elements
 *
 * @param h Hashmap table
 */
void osHash_delete(osHash_t *h)
{
	uint32_t i;

	if (!h)
	{
		return;
	}

	for (i=0; i<h->bsize; i++)
	{
		pthread_mutex_lock(&h->bucket[i].bucketMutex);
		osList_delete(&h->bucket[i].bucketList);
		pthread_mutex_unlock(&h->bucket[i].bucketMutex);
	}
}


/**
 * Clear a hashmap without dereferencing the elements
 *
 * @param h Hashmap table
 */
void osHash_clear(osHash_t *h)
{
	uint32_t i;

	if (!h)
	{
		return;
	}

	for (i=0; i<h->bsize; i++)
	{
		pthread_mutex_lock(&h->bucket[i].bucketMutex);
		osList_clear(&h->bucket[i].bucketList);
		pthread_mutex_unlock(&h->bucket[i].bucketMutex);
	}
}

	

uint32_t osHash_getKeyStr(const char* str, size_t len, bool isCase)
{
    uint32_t hashKey = 0;

    if (len == 0)
    {
        if(isCase)
        {
            hashKey = osHashGetKey_str(str);
        }
        else
        {
            hashKey = osHashGetKey_strCI(str);
        }
    }
    else
    {
        if(isCase)
        {
            hashKey = osHashGetKey((const uint8_t*) str, len);
        }
        else
        {
            hashKey = osHashGetKey_ci(str, len);
        }
    }

	return hashKey;
}

uint32_t osHash_getKeyPL(const osPointerLen_t* pPL, bool isCase)
{
    uint32_t hashKey = 0;
    if (isCase)
    {
        hashKey = osHashGetKey_pl(pPL);
    }
    else
    {
        hashKey = osHashGetKey_plCI(pPL);
    }

	return hashKey;
}


uint32_t osHash_getKeyPL_extraKey(const osPointerLen_t* pPL, bool isCase, uint8_t extraKey)
{
    uint32_t hashKey = 0;
    if (isCase)
    {
        hashKey = osHashGetKey_pl_extraKey(pPL, extraKey);
    }
    else
    {
        hashKey = osHashGetKey_plCI_extraKey(pPL, extraKey);
    }

    return hashKey;
}


uint32_t osHash_getKeyStr_extraKey(const char* pStr, bool isCase, uint8_t extraKey)
{
	osPointerLen_t pl = {pStr, strlen(pStr)};
    uint32_t hashKey = 0;
    if (isCase)
    {
        hashKey = osHashGetKey_pl_extraKey(&pl, extraKey);
    }
    else
    {
        hashKey = osHashGetKey_plCI_extraKey(&pl, extraKey);
    }

    return hashKey;
}


void* osHash_getData(osListElement_t* pHashLE)
{
	if(!pHashLE)
	{
		return NULL;
	}

	return ((osHashData_t*)pHashLE->data)->pData;
}


static bool osHashCompare(osListElement_t *le, void *data)
{
    if(!data || ! le)
    {
        return false;
    }

    osHashData_t* pData = (osHashData_t*) data;
    osHashData_t* pHashData = (osHashData_t*) le->data;
    if(pHashData->hashKeyType != pData->hashKeyType)
    {
        return false;
    }

    switch (pData->hashKeyType)
    {
        case OSHASHKEY_STR:
            if(pData->hashKeyStr.len != pHashData->hashKeyStr.len || pData->hashKeyStr.isCase != pHashData->hashKeyStr.isCase)
            {
                return false;
            }
            if(pData->hashKeyStr.isCase)
            {
                return !strncmp(pData->hashKeyStr.str, pHashData->hashKeyStr.str, pData->hashKeyStr.len);
            }
            else
            {
                return !strncasecmp(pData->hashKeyStr.str, pHashData->hashKeyStr.str, pData->hashKeyStr.len);
            }
        break;
        case OSHASHKEY_INT:
            return (pData->hashKeyInt == pHashData->hashKeyInt);
            break;
        case OSHASHKEY_PL:
        default:
            break;
    }

    return false;
}

