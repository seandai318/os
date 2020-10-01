/********************************************************
 * Copyright (C) 2020 Sean Dai
 *
 * @file osXmlParserLb.c
 * provide common functions for xml/xsd parser
 ********************************************************/

#include "osMemory.h"
#include "osXmlParserData.h"
#include "osXmlParserLib.h"



osXsdElement_t* osXsd_createAnyElem(osPointerLen_t* pTag, bool isRootAnyElem)
{
	if(!pTag)
	{
		return NULL;
	}

	osXsdElement_t* pXsdElem = oszalloc(sizeof(osXsdElement_t), NULL);
	if(!pXsdElem)
	{
		logError("fails to oszalloc() for pXsdElem.");
		return NULL;
	}

	pXsdElem->isElementAny = true;
	pXsdElem->elemName = *pTag;
	pXsdElem->dataType = OS_XML_DATA_TYPE_ANY;	//for a <xs:any> element, ALWAYS treat it as a OS_XML_DATA_TYPE_ANY
	pXsdElem->anyElem.isXmlAnyElem = true;
	pXsdElem->anyElem.xmlAnyElem.isLeaf = true;
	pXsdElem->anyElem.xmlAnyElem.isRootAnyElem = isRootAnyElem ? true : false;

	return pXsdElem;
}


bool isExistXsdAnyElem(osXsd_elemPointer_t* pXsdPointer)
{
	bool isExistAnyElem = false;

    if(pXsdPointer->pCurElem->dataType != OS_XML_DATA_TYPE_COMPLEX)
    {
        mlogInfo(LM_XMLP, "the element(%r) is not complexType.", &pXsdPointer->pCurElem->elemName);
        goto EXIT;
    }

    osXmlComplexType_t* pCT = pXsdPointer->pCurElem->pComplex;
    osListElement_t* pLE = pCT->elemList.head;
    while(pLE)
    {
		if(((osXsdElement_t*)pLE->data)->isElementAny)
		{
			isExistAnyElem = true;
			break;
		}

        pLE = pLE->next;
    }

EXIT:
    return isExistAnyElem;
}

