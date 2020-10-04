/********************************************************
 * Copyright (C) 2020 Sean Dai
 *
 * @file osXmlParserLb.c
 * provide common functions for xml/xsd parser
 ********************************************************/

#include"string.h"

#include "osMemory.h"
#include "osXmlParserData.h"
#include "osXmlParserLib.h"
#include "osXmlParser.h"


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


int osXml_tagCmp(osPointerLen_t* pNsAlias, char* str, int strLen, osPointerLen_t* pTag)
{
	if(!pNsAlias ||!str||!pTag)
	{
		logError("null pointer, pNsAlias=%p, str=%p, pTag=%p.", pNsAlias, str, pTag);
		return -1;
	}

	if(pTag->l - pNsAlias->l != strLen)
	{
		return -1;
	}

	if(strncmp(pTag->p, pNsAlias->p, pNsAlias->l) != 0)
	{
		return -1;
	}

	return strncmp(str, &pTag->p[pNsAlias->l], pTag->l - pNsAlias->l);
}


int osXml_xsTagCmp(char* str, int strlen, osPointerLen_t* pTag)
{
    osPointerLen_t* pXsAlias = osXsd_getXSAlias();
	return osXml_tagCmp(pXsAlias, str, strlen, pTag);
}


bool osXml_singleDelimitMatch(const char* str, int strlen, char delimit, osPointerLen_t* tag, osPointerLen_t* pFirstSection)
{
	if(!str || !tag || !pFirstSection)
	{
		logError("null pointer, str=%p, tag=%p, pFirstSection=%p", str, tag, pFirstSection);
		return false;
	}

	//case when there is no delimit
	if(strlen == tag->l && memcmp(str, tag->p, strlen) == 0)
	{
		pFirstSection->l = 0;
		return true;
	}

	for(int i=0; i<tag->l; i++)
	{
		if(tag->p[i] == delimit)
		{
			if(strlen +i +1 == tag->l && memcmp(str, &tag->p[i+1], strlen) == 0)
			{
				pFirstSection->p = tag->p;
				pFirstSection->l = i+1;
				return true;
			}
		}
	}

	return false;
}

