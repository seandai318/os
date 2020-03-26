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
    void* first;
    osList_t more;
} osListPlus_t;


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
//delete the osList_t overhead plus deref the data
void osList_delete(osList_t *list);
//only delete the osList_t, data is not touched
void osList_clear(osList_t *list);
osListElement_t* osList_append(osList_t *list, void *data);
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
void osList_sort(osList_t *list, osListSortHandler sortHandler, void *arg);
osListElement_t *osList_lookup(const osList_t *list, bool fwd, osListApply_h applyHandler, void *arg);
osListElement_t* osList_getHead(const osList_t *list);
osListElement_t* osList_getTail(const osList_t *list);
uint32_t osList_getCount(const osList_t *list);
osStatus_e osList_addString(osList_t *pList, char* nameParam, size_t nameLen);

osStatus_e osListPlus_append(osListPlus_t* pList, void* pData);
void osListPlus_clear(osListPlus_t* pList);



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


static inline bool list_isempty(const osList_t *list)
{
	return list ? list->head == NULL : true;
}


#define LIST_FOREACH(list, le)					\
	for ((le) = list_head((list)); (le); (le) = (le)->next)


#endif
