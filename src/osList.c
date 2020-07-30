 /* @file list.c  Double Linked List implementation, with a header pointer and a tail pointer, 
 *               each element also points to the list head 
 *
 * Copyright (C) 2019, InterLogic 
 */
#include "osTypes.h"
#include "osList.h"
#include "osMemory.h"
#include "osPL.h"
#include "osDebug.h"


/**
 * Initialise a linked list
 *
 * @param list Linked list
 */
void osList_init(osList_t* list)
{
	if (list == NULL)
	{
		return;
	}

	list->head = NULL;
	list->tail = NULL;
}


/**
 * Delete a linked list and free the data of each elements
 *
 * @param list Linked list
 */
void osList_delete(osList_t* pList)
{
	osListElement_t* pLE;

	if (!pList)
	{
		return;
	}

logError("to-remove, delete, addr=%p, total element=%d", pList, osList_getCount(pList));
	pLE = pList->head;
	while (pLE) {
		osListElement_t *pNext = pLE->next;
		void *data = pLE->data;
		pLE->list = NULL;
		pLE->prev = pLE->next = NULL;
		pLE->data = NULL;

    logError("to-remove, delete, addr=%p.", pLE);

		osfree(pLE);

		pLE = pNext;
        osfree(data);
	}

	osList_init(pList);
}


void osListElement_delete(osListElement_t* pLE)
{
	if(!pLE)
	{
		return;
	}

	osfree(pLE->data);
	osfree(pLE);
}


//besides cleanup pList data structure, delete pList itself
void osList_free(osList_t* pList)
{
	if(!pList)
	{
		return;
	}

	osList_delete(pList);
	osfree(pList);
}


//to to used when a list object is created via dynamic memory allocation, as a cleanup function for osfree.
void osList_cleanup(void* pData)
{
	osList_delete(pData);
}


/**
 * Clear a linked list without dereferencing the data of each element
 *
 * @param list Linked list
 */
void osList_clear(osList_t* pList)
{
	osListElement_t *pLE;

	if (!pList)
		return;

	pLE = pList->head;
	while (pLE) {
		osListElement_t *pNext = pLE->next;
		pLE->list = NULL;
		pLE->prev = pLE->next = NULL;
		pLE->data = NULL;

    logError("to-remove, clear, addr=0x%x.", pLE);

		osfree(pLE);
		pLE = pNext;
	}

	osList_init(pList);
}


/**
 * Append a list element to a linked list
 *
 * @param list  Linked list
 * @param le    List element
 * @param data  Element data
 */
osListElement_t* osList_append(osList_t *list, void *data)
{
	if (!list)
	{
		return NULL;
	}

	osListElement_t* pLE = osmalloc_r(sizeof(osListElement_t), NULL);
	if(pLE == NULL)
	{
		logError("fail to allocate osListElement_t");
		return NULL;
	}

	pLE->prev = list->tail;
	pLE->next = NULL;
	pLE->list = list;
	pLE->data = data;

	if (!list->head)
	{
		list->head = pLE;
	}

	if (list->tail)
		list->tail->next = pLE;

	list->tail = pLE;

	logError("to-remove, append, size=%d, addr=%p.", sizeof(osListElement_t), pLE);
	return pLE;
}


/**
 * Append a list element to a linked list
 *
 * @param list  Linked list
 * @param le    List element
 * @param data  Element data, if data==NULL, assume data has already appached to the element 
 */
void osList_appendLE(osList_t* list, osListElement_t *le, void *data)
{
    if (!list || !le)
        return;

    if (le->list) {
        logWarning("append: le linked to %p\n", le->list);
        return;
    }

    le->prev = list->tail;
    le->next = NULL;
    le->list = list;
	if(data)
	{
    	le->data = data;
	}

    if (!list->head)
        list->head = le;

    if (list->tail)
        list->tail->next = le;

    list->tail = le;
}

/**
 * Prepend a list element to a linked list
 *
 * @param list  Linked list
 * @param le    List element
 * @param data  Element data
 */
osListElement_t* osList_prepend(osList_t *list, void *data)
{
	if (!list)
	{
		return NULL;
	}

    osListElement_t* pLE = (osListElement_t*) osmalloc_r(sizeof(osListElement_t), NULL);
    if(pLE == NULL)
    {
        logWarning("fail to allocate osListElement_t");
        return NULL;
    }

	pLE->prev = NULL;
	pLE->next = list->head;
	pLE->list = list;
	pLE->data = data;

	if (list->head)
		list->head->prev = pLE;

	if (!list->tail)
		list->tail = pLE;

	list->head = pLE;

	return pLE;
}


/**
 * Prepend a list element to a linked list
 *
 * @param list  Linked list
 * @param le    List element
 * @param data  Element data
 */
void osList_prependLE(osList_t *list, osListElement_t *le, void *data)
{
    if (!list || !le)
        return;

    if (le->list) {
        logWarning("prepend: le linked to %p\n", le->list);
        return;
    }

    le->prev = NULL;
    le->next = list->head;
    le->list = list;
    le->data = data;

    if (list->head)
        list->head->prev = le;

    if (!list->tail)
        list->tail = le;

    list->head = le;
}


/**
 * Insert a list element before a given list element
 *
 * @param list  Linked list
 * @param le    Given list element
 * @param ile   List element to insert
 * @param data  Element data
 */
void osList_insertBefore(osList_t *list, osListElement_t *le, osListElement_t *ile, void *data)
{
	if (!list || !le || !ile)
		return;

	if (ile->list) {
		logWarning("insert_before: le linked to %p\n", le->list);
		return;
	}

	if (le->prev)
		le->prev->next = ile;
	else if (list->head == le)
		list->head = ile;

	ile->prev = le->prev;
	ile->next = le;
	ile->list = list;
	ile->data = data;

	le->prev = ile;
}


/**
 * Insert a list element after a given list element
 *
 * @param list  Linked list
 * @param le    Given list element
 * @param ile   List element to insert
 * @param data  Element data
 */
void osList_insertAfter(osList_t *list, osListElement_t *le, osListElement_t *ile, void *data)
{
	if (!list || !le || !ile)
		return;

	if (ile->list) {
		logWarning("insert_after: le linked to %p\n", le->list);
		return;
	}

	if (le->next)
		le->next->prev = ile;
	else if (list->tail == le)
		list->tail = ile;

	ile->prev = le;
	ile->next = le->next;
	ile->list = list;
	ile->data = data;

	le->next = ile;
}


/**
 * Remove a list element from a linked list.  It is the caller's responsibility to free the list element
 *
 * @param le    List element to remove
 */
void osList_unlinkElement(osListElement_t *le)
{
	osList_t *list;

	if (!le || !le->list)
		return;

	list = le->list;

	if (le->prev)
		le->prev->next = le->next;
	else
		list->head = le->next;

	if (le->next)
		le->next->prev = le->prev;
	else
		list->tail = le->prev;

	le->next = NULL;
	le->prev = NULL;
	le->list = NULL;
}


//delete a element based on the stored data.
void* osList_deleteElement(osList_t* pList, osListApply_h applyHandler, void *arg)
{
	if(!pList || !applyHandler || !arg)
	{
		return NULL;
	}

	osListElement_t* pLE = osList_lookup(pList, true, applyHandler, arg);
	if(!pLE)
	{
		return NULL;
	}

	void* pData = pLE->data;
	osList_unlinkElement(pLE);
	osfree(pLE);

	return pData;
}


void osList_deleteElementAll(osListElement_t* pLE)
{
	if(!pLE)
	{
		return;
	}

	osList_unlinkElement(pLE);
    osfree(pLE->data);
    osfree(pLE);
}


/**
 * Sort a linked list in an order defined by the sort handler
 *
 * @param list  Linked list
 * @param sh    Sort handler
 * @param arg   Handler argument
 */
void osList_sort(osList_t *list, osListSortHandler sortHandler, void *arg)
{
	osListElement_t *le;
	void* data = NULL;
	bool sort;
	uint32_t itCount=1;

	if (!list || !sortHandler)
		return;

	uint32_t totalLE = osList_getCount(list);

 retry:
	le = list->head;
	sort = false;
	itCount = 1;
	while (le && le->next)
	{
		if (!sortHandler(le, le->next, arg))
		{
			data = le->data;
			le->data = le->next->data;
			le->next->data = data;
		}

		if(totalLE == 2)
		{
			sort = true;
			break;
		}
		if(++itCount  == totalLE)
		{
			break;
		}
		le = le->next;
	}

	if(totalLE-- <= 1 || sort)
	{
		return;
	}

	goto retry;
}

#if 0
void osList_sort(osList_t *list, osListSortHandler sortHandler, void *arg)
{
    osListElement_t *le;
    bool sort;

    if (!list || !sortHandler)
        return;

 retry:
    le = list->head;
    sort = false;

    while (le && le->next) {

        if (sortHandler(le, le->next, arg)) {

            le = le->next;
        }
        else {
            osListElement_t *tle = le->next;

            osList_unlinkElement(le);
            osList_insertAfter(list, tle, le, le->data);
            sort = true;
        }
    }

    if (sort) {
        goto retry;
    }
}
#endif

/**
 * Find the firat matching element in a linked list
 *
 * @param list  Linked list
 * @param fwd   true to traverse from head to tail, false for reverse
 * @param ah    Apply handler
 * @param arg   Handler argument
 *
 * @return Current list element if handler returned true
 */
osListElement_t *osList_lookup(const osList_t *list, bool fwd, osListApply_h applyHandler, void *arg)
{
	osListElement_t *le;

	if (!list || !applyHandler)
	{
		logError("NULL pointer, list=%p, applyHandler=%p.", list, applyHandler);
		return NULL;
	}

	le = fwd ? list->head : list->tail;
	
	while (le) 
	{
		osListElement_t *cur = le;

		le = fwd ? le->next : le->prev;

		if (applyHandler(cur, arg))
		{
			return cur;
		}
	}

	return NULL;
}


/**
 * Get the first element in a linked list
 *
 * @param list  Linked list
 *
 * @return First list element (NULL if empty)
 */
osListElement_t* osList_getHead(const osList_t *list)
{
	return list ? list->head : NULL;
}


osListElement_t* osList_popHead(const osList_t *list)
{
	if(!list)
	{
		return NULL;
	}

	osListElement_t* pLE = list->head;
	osList_unlinkElement(pLE);

	return pLE;
}


/**
 * Get the last element in a linked list
 *
 * @param list  Linked list
 *
 * @return Last list element (NULL if empty)
 */
osListElement_t* osList_getTail(const osList_t *list)
{
	return list ? list->tail : NULL;
}


osListElement_t* osList_popTail(const osList_t *list)
{
    if(!list)
    {
        return NULL;
    }

    osListElement_t* pLE = list->tail;
    osList_unlinkElement(pLE);

    return pLE;
}


void* osList_getDataByIdx(osList_t* list, int idx)
{
	if(!list || idx < 0)
	{
		return NULL;
	}

	osListElement_t* pLE = list->head;
	int i = 0;
	while(pLE)
	{
		if(i == idx)
		{
			return pLE->data;
		}

		i++;
		pLE = pLE->next;
	}

	return NULL;
}
/**
 * Get the number of elements in a linked list
 *
 * @param list  Linked list
 *
 * @return Number of list elements
 */
uint32_t osList_getCount(const osList_t *list)
{
	uint32_t n = 0;
	osListElement_t* pLE;

	if (!list)
		return 0;

	for (pLE = list->head; pLE; pLE = pLE->next)
		++n;

	return n;
}


osStatus_e osList_addString(osList_t *pList, char* nameParam, size_t nameLen)
{
//  DEBUG_BEGIN
    osStatus_e status = OS_STATUS_OK;

    osPointerLen_t* pPL = osmalloc_r(sizeof(osPointerLen_t), NULL);
    if(pPL == NULL)
    {
        logError("could not allocate memory for pPL.");
        status = OS_ERROR_MEMORY_ALLOC_FAILURE;
        goto EXIT;
    }

	if(nameLen == 0)
	{
		logError("string len=0.");
		status = OS_ERROR_INVALID_VALUE;
		goto EXIT;
	}

    pPL->p = nameParam;
    pPL->l = nameLen;

    osListElement_t* pLE = osList_append(pList, pPL);
//      debug("sean, insert pOther.name=%r, pLE=%p", &pOther->name, pLE);
    if(pLE == NULL)
    {
        logError("osList_append failure.");
        osfree(pPL);
        status = OS_ERROR_MEMORY_ALLOC_FAILURE;
        goto EXIT;
    }

EXIT:
//  DEBUG_END
    return status;
}


void osList_orderAppend(osList_t *list, osListSortHandler sortHandler, void* data, void* sortArg)
{
	if(!list || !data)
	{
		logError("null pointer, list=%p, data=%p.", list, data);
		return;
	}

	osListElement_t* pLE2 = osmalloc_r(sizeof(osListElement_t), NULL);
	if(!pLE2)
	{
		logError("osmalloc_r fails.");
		return;
	}

	pLE2->data = data;
	pLE2->list = list;

	osListElement_t* pLE = list->head;
	while(pLE)
	{
		if(sortHandler(pLE, pLE2, sortArg))
		{
			pLE2->prev = pLE->prev;
			pLE2->next = pLE;
			if(pLE->prev == NULL)
			{
				list->head = pLE2;
			}
			else
			{
				pLE->prev->next = pLE2;
			}
			pLE->prev = pLE2;

			return;
		}

		pLE = pLE->next;
	}

	if(!list->head)
	{
		list->head = pLE2;
		list->tail = pLE2;
		pLE2->prev = NULL;
		pLE2->next = NULL;
	}
	else
	{
		pLE2->prev = list->tail;
		pLE2->next = NULL;
		list->tail->next = pLE2;
		list->tail = pLE2;
	}	
}



void osListPlus_init(osListPlus_t* pList, bool isDataStatic)
{
	if(!pList)
	{
		return;
	}

	pList->num = 0;
	pList->isDataStatic = isDataStatic;
	pList->first = NULL;
	osList_init(&pList->more);
}


osStatus_e osListPlus_append(osListPlus_t* pList, void* pData)
{
	osStatus_e status = OS_STATUS_OK;
	if(!pList || !pData)
	{
		logError("null pointer, pList=%p, pData=%p.", pList, pData);
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

logError("to-remove, TCM, num=%d, pData=%p", pList->num, pData);
	if(pList->num == 0)
	{
		pList->first = pData;
	}
	else 
	{
		if(!osList_append(&pList->more, pData))
		{
			status = OS_ERROR_SYSTEM_FAILURE;
			goto EXIT;
		}
	}

	++pList->num;

EXIT:
	return status;
}


void osListPlus_clear(osListPlus_t* pList)
{
	if(!pList)
	{
		return;
	}

	pList->first = NULL;
	if(pList->num > 1)
	{
		osList_clear(&pList->more);
	}

	pList->num = 0;
}

void osListPlus_delete(osListPlus_t* pList)
{
    if(!pList)
    {
        return;
    }

	if(pList->isDataStatic)
	{
        osList_clear(&pList->more);
	}
	else
	{
		osfree(pList->first);
		osList_delete(&pList->more);
	}

	pList->first = NULL;
	pList->isDataStatic = false;
}


void osListPlus_free(osListPlus_t* pList)
{
    if(!pList)
    {
        return;
    }

	osListPlus_delete(pList);
	osfree(pList);
}
	

//get next element from the list.  if already the last one, starts from the head
osListElement_t* osList_getNextElement(osListElement_t* pLE)
{
	osListElement_t* pNextLE = pLE;
	if(!pLE)
	{
		goto EXIT;
	}

	if(pLE->next)
	{
		pNextLE = pLE->next;
	}
	else
	{
		pNextLE = pLE->list->head;
	}

EXIT:
	return pNextLE;
}	
