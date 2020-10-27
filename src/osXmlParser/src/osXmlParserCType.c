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
#include "osXsdParser.h"
#include "osXmlParserCType.h"
#include "osXmlParserCommon.h"
#include "osXmlMisc.h"



static osStatus_e osXsdComplexType_getAttrInfo(osList_t* pAttrList, osXmlComplexType_t* pCtInfo);


static osStatus_e osXsdComplexType_getAttrInfo(osList_t* pAttrList, osXmlComplexType_t* pCtInfo)
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
	osPointerLen_t* pXsAlias = osXsd_getXSAlias();	//if the alias is not empty, pXsAlias contains ':', otherwise, pXsAlias->l=0.   
	if(strncmp(pTagInfo->tag.p, pXsAlias->p, pXsAlias->l) != 0)
	{
		logError("tag(%r) does not contain XS alias(%r).", &pTagInfo->tag, pXsAlias);
		status = OS_ERROR_INVALID_VALUE;
		goto EXIT;
	}

    size_t realTagStart = pXsAlias->l ? (pXsAlias->l+1) : 0;
    size_t realTagLen = pTagInfo->tag.l - realTagStart;
	switch(realTagLen)
	{
		case 3:			//len=4, "all", "any"
			if(strncmp("all", &pTagInfo->tag.p[realTagStart], 3) == 0)
			{
				pCtInfo->elemDispType = OS_XML_ELEMENT_DISP_TYPE_ALL;
				isIgnored = false;
			}
			else if(strncmp("any", &pTagInfo->tag.p[realTagStart], 3) == 0)
			{
                osXsdElement_t* pElement;
				//for case <xs:any> ... </xs:any>.  In that case, isPElement will be set
                if(pTagInfo->isPElement)
                {
                    pElement = pTagInfo->pElement;
                }
                else
                {
					//for case <xs:any xxx />
					pElement = oszalloc(sizeof(osXsdElement_t), osXsdElement_cleanup);
					pElement->isElementAny = true;
					pElement->anyElem.elemAnyTag.processContent = OS_XSD_PROCESS_CONTENTS_STRICT;

					status = osXmlElement_getAttrInfo(&pTagInfo->attrNVList, pElement);
					if(status != OS_STATUS_OK || pElement->anyElem.elemAnyTag.processContent == OS_XSD_PROCESS_CONTENTS_STRICT || !(pElement->anyElem.elemAnyTag.elemNamespace == OS_XSD_NAME_SPACE_ANY || pElement->anyElem.elemAnyTag.elemNamespace == OS_XSD_NAME_SPACE_OTHER))
					{
						osfree(pElement);
						logError("fails in one of the following: osXmlElement_getAttrInfo() status=%d, processContent=%d, elemNamespace=%d.", status, pElement->anyElem.elemAnyTag.processContent, pElement->anyElem.elemAnyTag.elemNamespace);
						if(status == OS_STATUS_OK)
						{
							status = OS_ERROR_INVALID_VALUE;
						}
						goto EXIT;
					}
				}

                mdebug(LM_XMLP, "A any element is parsed, minOccurs=%d, maxOccurs=%d, namespace=%d, processContents=%d", pElement->minOccurs, pElement->maxOccurs, pElement->anyElem.elemAnyTag.elemNamespace, pElement->anyElem.elemAnyTag.processContent);
                osList_append(&pCtInfo->elemList, pElement);
                isIgnored = false;
            }
			break;
		case 6:			//len=6, "choice"
			if(strncmp("choice", &pTagInfo->tag.p[realTagStart], 6) == 0)
			{
				pCtInfo->elemDispType = OS_XML_ELEMENT_DISP_TYPE_SEQUENCE;
				isIgnored = false;
			}
			break;
		case 7:		//len=7, "element"
			if(strncmp("element", &pTagInfo->tag.p[realTagStart], 7) == 0)
    		{
				osXsdElement_t* pElement;
                //for case <xs:element> ... </xs:element>.  In that case, isPElement will be set
				if(pTagInfo->isPElement)
				{
					pElement = pTagInfo->pElement;
				}
				else
				{
					//for case <xs:element xxx />
        			pElement = oszalloc(sizeof(osXsdElement_t), osXsdElement_cleanup);
        			osXmlElement_getAttrInfo(&pTagInfo->attrNVList, pElement);
				}

				mdebug(LM_XMLP, "element(%r) is parsed, minOccurs=%d, maxOccurs=%d, dataType=%d", &pElement->elemName, pElement->minOccurs, pElement->maxOccurs, pElement->dataType);
        		osList_append(&pCtInfo->elemList, pElement);
                isIgnored = false;
    		}
			break;
		case 8:		//len=8, "sequence"
			if(strncmp("sequence", &pTagInfo->tag.p[realTagStart], 8) == 0)
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
            	//"xs:complexType" was not pushed into tagList, this happens for global complexType.
				if(osXml_xsTagCmp("complexType", 11, &pTagInfo->tag))
                {
                	logError("expect the end tag for xs:complexType, but %r is found.", &pTagInfo->tag);
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
        	//for case <xs:element xxx> ... </xs:element>.  For case <xs:element xxx />, it is handled in pTagInfo->isTagDone case.
			if(osXml_xsTagCmp("element", 7, &pTagInfo->tag) == 0)
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
			//for case <xs:any> (annotation?) </xs:any>.  for case <xs:any xxx />, it is handled in pTagInfo->isTagDone case.
            else if(osXml_xsTagCmp("any", 3, &pTagInfo->tag) == 0)
            {
				osXsdElement_t* pElem = osXsd_parseElementAny(pXmlBuf, pTagInfo);
				if(!pElem)
                {
                    logError("fails to osXsd_parseElementAny, pos=%ld.", pXmlBuf->pos);
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

	mdebug(LM_XMLP, "complexType(%r) is parsed, elemDispType=%d", &pCtInfo->typeName, pCtInfo->elemDispType);

	return pCtInfo;
}


/* recursively going through a XSD complex type and call back all its leaf node value
 *
 * pCTPointer:   IN, a complex type pointer, contains the complex type node
 * callbackInfo: INOUT, the XSD value will be set as one of the parameters in the data structure.  Inside this function, it is passed as an
 *               input to the xsdCallback, which in turn as an input to xmlCallback function
 */
osStatus_e osXsd_transverseCT(osXsd_ctPointer_t* pCTPointer, osXmlDataCallbackInfo_t* callbackInfo)
{
	osStatus_e status = OS_STATUS_OK;

    if(!pCTPointer)
    {
        logError("null pointer, pCTPointer=%p.", pCTPointer);
        status = OS_ERROR_NULL_POINTER;
		goto EXIT;
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

			status = osXsd_elemCallback(pChildElem, callbackInfo);
			if(status != OS_STATUS_OK)
			{
				logError("fails to osXsd_elemCallback.");
				goto EXIT;
			}

			if(!osXml_isXsdElemSimpleType(pChildElem))
            {
#if 0
                osXsd_ctPointer_t* pNextCTPointer = oszalloc(sizeof(osXsd_ctPointer_t), NULL);
                pNextCTPointer->pCT = pChildElem->pComplex;
                status = osXsd_transverseCT(pNextCTPointer, callbackInfo);
                osfree(pNextCTPointer);
#else
				osXsd_ctPointer_t nextCtPointer = {};
				nextCtPointer.pCT = pChildElem->pComplex;
				status = osXsd_transverseCT(&nextCtPointer, callbackInfo);
#endif
				if(status != OS_STATUS_OK)
				{
					goto EXIT;
				}
            }
        }
        pLE = pLE->next;
    }

EXIT:
    return status;
}


void osXmlComplexType_cleanup(void* data)
{
    if(!data)
    {
        return;
    }

	osXmlComplexType_t* pCT = data;
	osList_delete(&pCT->elemList);
}
