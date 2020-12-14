/**
 * @file oslist.h  Interface to Double Linked List
 *
 * Copyright (C) 2019, InterLogic
 */

#ifndef _OS_LIST_H
#define _OS_LIST_H

#include "osTypes.h"
#include "osDebug.h"


/** Linked-list element */
typedef struct osListElement {
	struct osListElement* prev;    /**< Previous element                    */
	struct osListElement* next;    /**< Next element                        */
	struct osList *list;  /**< Parent list (NULL if not linked-in) */
	void *data;         /**< User-data                           */
} osListElement_t;

/** List Element Initializer */
#define LE_INIT {NULL, NULL, NULL, NULL}


/** Defines a linked list */
typedef struct osList {
	osListElement_t *head;  /**< First list element */
	osListElement_t *tail;  /**< Last list element  */
} osList_t;


typedef struct {
    size_t num;
	bool isDataStatic;		//if true, the list element data is static or owned by other module, no need to dealloc after the osList is used, otherwise, the data needs to be freed after the list use
    void* first;
    osList_t more;
} osListPlus_t;


typedef struct {
	bool isFirst;			//if the current element uses "void* first" of osListPlus_t
	bool isRetrieved;		//the osPlistPlus has done retrieval.  This is needed otherwise the retrieval will start from beginning automatically
	osListElement_t* pLE;	//if the current element uses "osList_t more" of osListPlus_t
} osListPlusElement_t;


/** Linked list Initializer */
#define LIST_INIT {NULL, NULL}


/**
 * Defines the list apply handler
 *
 * @param le  List element
 * @param arg Handler argument
 *
 * @return true to stop traversing, false to continue
 */
typedef bool (*osListApply_h)(osListElement_t *le, void *arg);

/**
 * Defines the list sort handler
 *
 * @param le1  Current list element
 * @param le2  Next list element
 * @param arg  Handler argument
 *
 * @return true if sorted, otherwise false
 */
typedef bool (*osListSortHandler)(osListElement_t *le1, osListElement_t *le2, void *arg);


void osList_init(osList_t *list);
//besides cleanup pList data structure, delete pList itself
void osList_free(osList_t* pList);
//delete the osList_t overhead plus deref the data
void osList_delete(osList_t *list);
//to to used when a list object is created via dynamic memory allocation, as a cleanup function for osfree.
void osList_cleanup(void* pData);
//only delete the osList_t, data is not touched
void osList_clear(osList_t *list);
osListElement_t* osList_append(osList_t *list, void *data);
//if data==NULL, assume data has already appached to the element
void osList_appendLE(osList_t *list, osListElement_t *le, void *data);
osListElement_t* osList_prepend(osList_t *list, void *data);
void osList_prependLE(osList_t *list, osListElement_t *le, void *data);
void osList_insertBefore(osList_t *list, osListElement_t *le, osListElement_t *ile,
			void *data);
void osList_insertAfter(osList_t *list, osListElement_t *le, osListElement_t *ile, void *data);
void osList_orderAppend(osList_t *list, osListSortHandler sortHandler, void* data, void* sortArg); 
void osList_unlinkElement(osListElement_t *le);
//delete a element based on the stored data.
void* osList_deleteElement(osList_t* pList, osListApply_h applyHandler, void *arg);
//each element contains a data structure pointer, the input arg is the address of the data structure, i.e., pointer
void* osList_deletePtrElement(osList_t* pList, void*arg);
//delete a element by element address, the data pointed by the element is also deleted if isFreeData=true.
void osList_deleteElementAll(osListElement_t* pLE, bool isFreeData);
void osList_sort(osList_t *list, osListSortHandler sortHandler, void *arg);
osListElement_t *osList_lookup(const osList_t *list, bool fwd, osListApply_h applyHandler, void *arg);
osListElement_t* osList_getHead(const osList_t *list);
osListElement_t* osList_getTail(const osList_t *list);
osListElement_t* osList_popHead(const osList_t *list);
osListElement_t* osList_popTail(const osList_t *list);
//combine list1 and list2, after the combination, list1 is the head.  pList2 is not freed, up for user to free it if necessary
osList_t* osList_combine(osList_t* pList1, osList_t* pList2);
void* osList_getDataByIdx(osList_t* list, int idx);
uint32_t osList_getCount(const osList_t *list);
osStatus_e osList_addString(osList_t *pList, char* nameParam, size_t nameLen);
osListElement_t* osList_getNextElement(osListElement_t* pLE);
void osListElement_delete(osListElement_t* pLE);

void osListPlus_init(osListPlus_t* pList, bool isDataStatic);
osStatus_e osListPlus_append(osListPlus_t* pList, void* pData);
void* osListPlus_getNextData(osListPlus_t* pList, osListPlusElement_t* pPlusLE);
void osListPlus_clear(osListPlus_t* pList);
void osListPlus_delete(osListPlus_t* pList);
void osListPlus_free(osListPlus_t* pList);


/**
 * Get the user-data from a list element
 *
 * @param le List element
 *
 * @return Pointer to user-data
 */
static inline void *list_ledata(const osListElement_t *le)
{
	return le ? le->data : NULL;
}


static inline bool list_contains(const osList_t *list, const osListElement_t *le)
{
	return le ? le->list == list : false;
}


static inline bool osList_isEmpty(const osList_t *list)
{
	return list ? list->head == NULL : true;
}


#define LIST_FOREACH(list, le)					\
	for ((le) = list_head((list)); (le); (le) = (le)->next)


#endif
