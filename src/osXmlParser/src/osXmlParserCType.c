/********************************************************
 * Copyright (C) 2020 Sean Dai
 *
 * @file osXmlParserCType.c
 * xsd and xml parser for ComplexType
 ********************************************************/

#include <string.h>
#include <stdlib.h>

#include "osMBuf.h"
#include "osPL.h"
#include "osList.h"
#include "osDebug.h"
#include "osMemory.h"
#include "osMisc.h"

#include "osXmlParser.h"
#include "osXmlParserCType.h"


static void osXmlComplexType_cleanup(void* data);


osStatus_e osXsdComplexType_getAttrInfo(osList_t* pAttrList, osXmlComplexType_t* pCtInfo)
{
	osStatus_e status = OS_STATUS_OK;
    if(!pCtInfo || !pAttrList)
    {
        logError("null pointer, pCtInfo=%p, pAttrList=%p.", pCtInfo, pAttrList);
        status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

	osListElement_t* pLE = pAttrList->head;
	while(pLE)
	{
        osXmlNameValue_t* pNV = pLE->data;
        if(!pNV)
        {
            logError("pNV is NULL, this shall never happen.");
            goto EXIT;
        }

		bool isIgnored = true;
        switch(pNV->name.p[0])
        {
            case 'n':   //for "name"
                if(pNV->name.l == 4 && strncmp("name", pNV->name.p, pNV->name.l) == 0)
                {
                    pCtInfo->typeName = pNV->value;
					isIgnored = false;
                }
                break;
			case 'm':	//for mixed"
                if(pNV->name.l == 5 && strncmp("mixed", pNV->name.p, pNV->name.l) == 0)
                {
					if(pNV->value.l == 4 && strncmp("true", pNV->value.p, pNV->value.l) == 0)
					{
						pCtInfo->isMixed = true;
						isIgnored = false;
					}
					else if(pNV->value.l == 5 && strncmp("false", pNV->value.p, pNV->value.l) == 0)
                    {
                        pCtInfo->isMixed = false;
						isIgnored = false;
                    }
					else
					{
						mlogInfo(LM_XMLP, "xml mixed value is not correct(%r), ignore.", &pNV->value);
					}
				}
				break;
			default:
				break;
		}
		
		if(isIgnored)
		{
			mlogInfo(LM_XMLP, "attribute(%r) is ignored.", &pNV->name);
		}

		pLE = pLE->next;
	}

EXIT:
	return status;
}	
	
	
osStatus_e osXsdComplexType_getSubTagInfo(osXmlComplexType_t* pCtInfo, osXmlTagInfo_t* pTagInfo)
{
	osStatus_e status = OS_STATUS_OK;

	if(!pCtInfo || !pTagInfo)
	{
		logError("null pointer, pCtInfo=%p, pTagInfo=%p.", pCtInfo, pTagInfo);
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

	//add to the complexTypeInfo data structure
	bool isIgnored = true;
	switch(pTagInfo->tag.l)
	{
		case 6:			//len=6, "xs:all"
			if(strncmp("xs:all", pTagInfo->tag.p, pTagInfo->tag.l) == 0)
			{
				pCtInfo->elemDispType = OS_XML_ELEMENT_DISP_TYPE_ALL;
				isIgnored = false;
			}
			break;
		case 9:			//len=9, "xs:choice"
			if(strncmp("xs:choice", pTagInfo->tag.p, pTagInfo->tag.l) == 0)
			{
				pCtInfo->elemDispType = OS_XML_ELEMENT_DISP_TYPE_SEQUENCE;
				isIgnored = false;
			}
			break;
		case 10:		//len=10, "xs:element"
			if(strncmp("xs:element", pTagInfo->tag.p, pTagInfo->tag.l) == 0)
    		{
				osXsdElement_t* pElement;
				if(pTagInfo->isPElement)
				{
					pElement = pTagInfo->pElement;
				}
				else
				{
        			pElement = oszalloc(sizeof(osXsdElement_t), osXsdElement_cleanup);
        			osXmlElement_getAttrInfo(&pTagInfo->attrNVList, pElement);
				}

        		osList_append(&pCtInfo->elemList, pElement);
                isIgnored = false;
    		}
			break;
		case 11:		//len=11, "xs:sequence"
			if(strncmp("xs:sequence", pTagInfo->tag.p, pTagInfo->tag.l) == 0)
			{
				pCtInfo->elemDispType = OS_XML_ELEMENT_DISP_TYPE_SEQUENCE;
                isIgnored = false;
			}
			break;
		default:
			break;
	}

	if(isIgnored)
	{
		mlogInfo(LM_XMLP, "xml tag(%r) is ignored.", &pTagInfo->tag);
	}

EXIT:
	return status;
}



osXmlComplexType_t* osXsdComplexType_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pCtTagInfo, osXsdElement_t* pParentElem)
{
	osStatus_e status = OS_STATUS_OK;

    osList_t tagList = {};
    osXmlComplexType_t* pCtInfo = NULL;
	osXmlTagInfo_t* pTagInfo = NULL;

    if(!pXmlBuf || !pCtTagInfo)
    {
        logError("null pointer, pXmlBuf=%p, pCtTagInfo=%p.", pXmlBuf, pCtTagInfo);
		status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

    pCtInfo = osmalloc(sizeof(osXmlComplexType_t), osXmlComplexType_cleanup);
    osXsdComplexType_getAttrInfo(&pCtTagInfo->attrNVList, pCtInfo);

    while(pXmlBuf->pos < pXmlBuf->end)
    {
        //get tag info for each tags inside xs:complexType
		status = osXml_parseTag(pXmlBuf, false, false, &pTagInfo, NULL);
        if(status != OS_STATUS_OK || !pTagInfo)
        {
         	logError("fails to osXml_parseTag.");
			status = OS_ERROR_INVALID_VALUE;
        	break;
        }

        if(pTagInfo->isEndTag)
        {
        	osListElement_t* pLE = osList_popTail(&tagList);
            if(!pLE)
            {
            	//"xs:complexType" was not pushed into tagList, so it is possible the end tag is "xs:complexType"
                if(strncmp("xs:complexType", pTagInfo->tag.p, pTagInfo->tag.l))
                {
                	logError("expect the end tag for xs:complexType, but %r is found.", &((osXmlTagInfo_t*)pLE->data)->tag);
            		status = OS_ERROR_INVALID_VALUE;
                }
                else
                {
                	//the parsing for a complexType is done
                    if(!pCtInfo->typeName.l && !pParentElem)
                    {
                    	mlogInfo(LM_XMLP, "parsed a complexType without name, and there is no parent element, pos=%ld.", pXmlBuf->pos);
                    }

                    if(pParentElem)
                    {
                    	//if the complexType is embedded inside a element, directly assign the complexType to the parent element
                        if(osPL_cmp(&pParentElem->elemTypeName, &pCtInfo->typeName) == 0 || pParentElem->elemTypeName.l == 0)
                        {
                        	pParentElem->dataType = OS_XML_DATA_TYPE_COMPLEX;
                            pParentElem->pComplex = pCtInfo;
                        }
                    }
                }
                goto EXIT;
            }

            //compare the end tag name (newly gotten) and the beginning tag name (from the LE)
			mdebug(LM_XMLP, "pTagInfo->tag=%r, pLE->data)->tag=%r", &pTagInfo->tag, &((osXmlTagInfo_t*)pLE->data)->tag);
            if(osPL_cmp(&((osXmlTagInfo_t*)pLE->data)->tag, &pTagInfo->tag) == 0)
            {
            	osXsdComplexType_getSubTagInfo(pCtInfo, (osXmlTagInfo_t*)pLE->data);
				osListElement_delete(pLE);
				pTagInfo = osfree(pTagInfo);
                continue;
            }
            else
            {
            	logError("input xml is invalid, error in (%r).", &pTagInfo->tag);
				osListElement_delete(pLE);
				status = OS_ERROR_INVALID_VALUE;
                goto EXIT;
            }
        }

        if(pTagInfo->isTagDone)
        {
        	//no need to push to the tagList as the end tag is part of the line
            osXsdComplexType_getSubTagInfo(pCtInfo, pTagInfo);
			pTagInfo = osfree(pTagInfo);
        }
        else
        {
        	//special treatment for <xs:element xxx> ... </xs:element>, as xs:element can contain own tags.
            if(pTagInfo->tag.l == 10 && strncmp("xs:element", pTagInfo->tag.p, pTagInfo->tag.l) == 0)
            {
            	osXsdElement_t* pElem = osXsd_parseElement(pXmlBuf, pTagInfo);
                if(!pElem)
                {
                	logError("fails to osXsd_parseElement, pos=%ld.", pXmlBuf->pos);
					status = OS_ERROR_INVALID_VALUE;
                    goto EXIT;
                }

				//reuse the pTagInfo allocated during <xs:element xxx> parsing
                osList_delete(&pTagInfo->attrNVList);
                pTagInfo->isPElement = true;
                pTagInfo->pElement = pElem;

                osXsdComplexType_getSubTagInfo(pCtInfo, pTagInfo);
				osfree(pTagInfo);
            }
            else
            {
               	//add the beginning tag to the tagList, the info in the beginning tag will be processed when the end tag is met
                osList_append(&tagList, pTagInfo);
            }
        }
	}

EXIT:
	if(status != OS_STATUS_OK)
	{
		osfree(pCtInfo);
		pCtInfo = NULL;
	}

	if(pTagInfo)
	{
    	osfree(pTagInfo);
	}

	return pCtInfo;
}


/* recursively going through a XSD complex type and call back all its leaf node value
 *
 * pCTPointer:   IN, a complex type pointer, contains the complex type node
 * callbackInfo: INOUT, the XSD value will be set as one of the parameters in the data structure.  Inside this function, it is passed as an
 *               input to the xsdCallback, which in turn as an input to xmlCallback function
 */
void osXsd_transverseCT(osXsd_ctPointer_t* pCTPointer, osXmlDataCallbackInfo_t* callbackInfo)
{
    if(!pCTPointer)
    {
        logError("null pointer, pCTPointer=%p.", pCTPointer);
        return;
    }

    osXmlComplexType_t* pCT = pCTPointer->pCT;
    int elemCount = osList_getCount(&pCT->elemList);
    osListElement_t* pLE = pCT->elemList.head;
    while(pLE)
    {
        if(pCTPointer->doneListIdx < elemCount)
        {
            pCTPointer->doneListIdx++;
            osXsdElement_t* pChildElem = pLE->data;

			osXsd_elemCallback(pChildElem, callbackInfo);

			if(!osXml_isXsdElemSimpleType(pChildElem))
            {
                osXsd_ctPointer_t* pNextCTPointer = oszalloc(sizeof(osXsd_ctPointer_t), NULL);
                pNextCTPointer->pCT = pChildElem->pComplex;
                osXsd_transverseCT(pNextCTPointer, callbackInfo);
                osfree(pNextCTPointer);
            }
        }
        pLE = pLE->next;
    }

    return;
}


static void osXmlComplexType_cleanup(void* data)
{
    if(!data)
    {
        return;
    }

	osXmlComplexType_t* pCT = data;
	osList_delete(&pCT->elemList);
}
