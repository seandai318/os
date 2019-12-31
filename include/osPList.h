#ifndef _OS_P_LIST_H
#define _OS_P_LIST_H

#include "osTypes.h"
#include "osList.h"


typedef struct osPList {
	size_t count;
	void* pFirstData;
	osList_t list;
	osListElement_t* pCurLE;//for retrieval, store the current retrieval LE, initiated to be 0;
	size_t retCount;		//for retrieval, if retCount = 0, osPList_getNext() shall return pFirstData, if retCount == count, return NULL. 
} osPList_t;


size_t osPList_getCount(osPList_t* ple);
void* osPLIst_getTopData(osPList_t* ple);
void* osPList_getBottomData(osPList_t* ple);
//each time when retrieving a whole PLE, the first call must set isRetInit=true, then must set isRetInit=false, otherwise, thi function may not return correct data
void* osPList_getNext(osPList_t* ple, bool isRetInit);
void* osPList_add(osPList_t* ple, void* data);
void* osPList_deleteTop(osPList_t* ple);



#endif
