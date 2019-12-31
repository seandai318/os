#include "osPList.h"
#include "osDebug.h"

size_t osPList_getCount(osPList_t* ple)
{
	if(!ple)
	{
		return 0;
	}

	return ple->count;
}


void* osPLIst_getTopData(osPList_t* ple)
{
	if(!ple)
    {
        return NULL;
    }

	return ple->pFirstData;
}


void* osPList_getBottomData(osPList_t* ple)
{
    if(!ple)
    {
        return NULL;
    }

	if(ple->count <= 1)
	{
		return ple->pFirstData;
	}
	else
	{
		return ple->list.tail ? ple->list.tail->data : NULL;
	}
}


void* osPList_getNext(osPList_t* ple, bool isRetInit)
{
	void* pData = NULL;

	if(!ple)
    {
        return NULL;
    }

	if(isRetInit)
	{
		ple->count = 0;
		ple->pCurLE = NULL;
	}

	if(ple->retCount++ < ple->count)
	{
		if(ple->retCount == 1)
		{
			pData = ple->pFirstData;
		}
		else
		{
			ple->pCurLE = ple->pCurLE ? ple->pCurLE->next : ple->list.head;
			if(ple->pCurLE)
			{
				pData = ple->pCurLE->data;
			}
			else
			{
				//this shall never happen, just a safeguard
				logError("pCurLE == NULL, this is unexpected."); 
				ple->retCount = ple->count;
				
			}
		}
	}

	return pData;
}


void* osPList_add(osPList_t* ple, void* data)
{
    if(!ple || !data)
    {
        return NULL;
    }

	if(ple->count++ == 0)
	{
		ple->pFirstData = data;
		return ple->pFirstData;
	} 
	else
	{
		if(!osList_append(&ple->list, data))
		{
			logError("osList_append fails.");
			return NULL;
		}
	}

	return data;
}


void* osPList_deleteTop(osPList_t* ple)
{
	void* pData = NULL;

    if(!ple)
    {
        return NULL;
    }

	if(ple->count == 0)
	{
		return NULL;
	}

	pData = ple->pFirstData;
	if(ple->count-- == 1)
	{
		ple->pFirstData = NULL;
	}
	else
	{
        osListElement_t* pLE = ple->list.head;
		if(pLE)
		{
			ple->pFirstData = pLE->data;
			osList_unlinkElement(pLE);
			free(pLE);
		}
		else
		{
			logError("pLE == NULL while count=%d, unexpected.", ple->count+1);
			ple->pFirstData = NULL;
			pData = NULL;
		}
	}

	return pData;
}

	 
			
	

