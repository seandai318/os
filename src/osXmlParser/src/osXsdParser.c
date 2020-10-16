/********************************************************
 * Copyright (C) 2020 Sean Dai
 *
 * @file osXsdParser.c
 * xsd parser, support the following xml format:
 * xs:boolean, xs:short, xs:integer, xs:long, xs:string, xs:complex
 * validate the xml against xsd.  If a call back is provided, the leaf
 * information of all xml leave elements will be provided via the callback
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
#include "osXmlParserCommon.h"
#include "osXmlParserCType.h"
#include "osXmlParserSType.h"
#include "osXmlMisc.h"


static osXsdSchema_t* osXsd_parseSchema(osMBuf_t* pXmlBuf);
static osStatus_e osXsd_elemLinkChild(osXsdElement_t* pParentElem, osList_t* pCTypeList, osList_t* pSTypeList);
static osStatus_e osXsd_parseGlobalTag(osMBuf_t* pXmlBuf, osList_t* pTypeList, osList_t* pSTypeList, osXmlTagInfo_t** pGlobalElemTagInfo, bool* isEndSchemaTag);
static void* osXsd_getTypeByname(osList_t* pTypeList, osPointerLen_t* pElemTypeName);
osStatus_e osXsd_parseSchemaTag(osMBuf_t* pXmlBuf, osXsd_schemaInfo_t* pSchemaInfo, bool* isSchemaTagDone);
static osXsdNamespace_t* osXsd_getNS(osList_t* pXsdNSList, osPointerLen_t* pTargetNS, bool isCreateNS);
static void osXsdSchema_cleanup(void* data);
static void osXsdNS_cleanup(void* data);
static osXsdElement_t* osXsd_getElementFromList(osList_t* pList, osPointerLen_t* pTag);
static osStatus_e osXmlElement_getSubTagInfo(osXsdElement_t* pElement, osXmlTagInfo_t* pTagInfo);


//the current xs namespace alias (it does not have to be "xs"), always point to the one stored in the current processing root element
static __thread osPointerLen_t* pCurXSAlias;
static __thread osList_t gXsdNSList;	//each entry contains a osXsdNamespace_t


static void tempPrint(osList_t* pList, int i)
{
    osListElement_t* pLE1=pList->head;
    while(pLE1)
    {
        osXmlComplexType_t* pCT=pLE1->data;
        mdebug(LM_XMLP, "to-remove-%d, pCT=%p, list count=%d, typeName=%r", i, pCT, osList_getCount(&pCT->elemList), &pCT->typeName);
        pLE1 = pLE1->next;
    }
}


osMBuf_t* osXsd_initNS(char* fileFolder, char* xsdFileName)
{
    osStatus_e status = OS_STATUS_OK;
    osMBuf_t* xsdMBuf = NULL;

    if(!fileFolder || !xsdFileName)
    {
        logError("null pointer, fileFolder=%p, xsdFileName=%p.", fileFolder, xsdFileName);
        status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

    char xsdFile[OS_XML_MAX_FILE_NAME_SIZE];

    if(snprintf(xsdFile, OS_XML_MAX_FILE_NAME_SIZE, "%s/%s", fileFolder ? fileFolder : ".", xsdFileName) >= OS_XML_MAX_FILE_NAME_SIZE)
    {
        logError("xsdFile name is truncated.");
        status = OS_ERROR_INVALID_VALUE;
    }

    //8000 is the initial mBuf size.  If the reading needs more than 8000, the function will realloc new memory
    xsdMBuf = osMBuf_readFile(xsdFile, 8000);
    if(!xsdMBuf)
    {
        logError("read xsdMBuf fails.");
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

	osPointerLen_t xsdName = {xsdFileName, strlen(xsdFileName)};
    if(!osXsd_isExistSchema(&xsdName))
    {
        logInfo("start xsd parse for %r.", &xsdName);
        osXsdNamespace_t* pNS = osXsd_parse(xsdMBuf, &xsdName);
        if(!pNS)
        {
            logError("fails to osXsd_parse for xsdMBuf.");
            status = OS_ERROR_INVALID_VALUE;
            goto EXIT;
        }
        logInfo("xsd parse for %r is done.", &xsdName);
    }

EXIT:
	if(status != OS_STATUS_OK)
	{
		osMBuf_dealloc(xsdMBuf);
		xsdMBuf = NULL;
	}
	return xsdMBuf;
}


osXsdNamespace_t* osXsd_parse(osMBuf_t* pXmlBuf, osPointerLen_t* pXsdName)
{
	osStatus_e status = OS_STATUS_OK;
	osXsdNamespace_t* pNS = NULL;
	osXsdSchema_t* pSchema = NULL;
    if(!pXmlBuf || !pXsdName)
    {
        logError("null pointer, pXmlBuf=%p, pXsdName=%p.", pXmlBuf, pXsdName);
        status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

	pSchema = osXsd_parseSchema(pXmlBuf);
	if(!pSchema)
	{
		logError("fails to osXsd_parseSchema.");
		status = OS_ERROR_INVALID_VALUE;
		goto EXIT;
	}

	//for now assume the same schema is only inputed/parsed one time.  In the future, for safety, add a xsd name check to prevent the same schema been parsed multiple times
	pNS = osXsd_getNS(&gXsdNSList, &pSchema->schemaInfo.targetNS, true);
	if(!pNS)
	{
        logError("fails to osXsd_getNS for a pSchema(%r).", &pSchema->schemaInfo.targetNS);
        osfree(pSchema);
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

    pNS->pTargetNS = &pSchema->schemaInfo.targetNS;
    if(pSchema->schemaInfo.targetNS.l == 0)
    {
        pNS->pTargetNS = NULL;

		//fill the schemaInfo.targetNS with the xsd name
		pSchema->schemaInfo.targetNS = *pXsdName;
    }

    if(!osList_append(&pNS->schemaList, pSchema))
	{
		logError("fails to osList_append a pSchema(%r).", &pSchema->schemaInfo.targetNS);
		osfree(pSchema);
		status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

	osList_append(&gXsdNSList, pNS);

EXIT:
	if(status != OS_STATUS_OK)
	{
		pNS = osfree(pNS);
	}

	return pNS;
} //osXsd_parse()
	

osXsdSchema_t* osXsd_parseSchema(osMBuf_t* pXmlBuf)	
{
	osStatus_e status = OS_STATUS_OK;	
	osXsdSchema_t* pSchema = NULL;
	
	//parse <?xml version="1.0" encoding="UTF-8"?>
	if(osXml_parseFirstTag(pXmlBuf) != OS_STATUS_OK)
	{
		logError("fails to parse the xsd first line.");
		status = OS_ERROR_INVALID_VALUE;
		goto EXIT;
	}

	bool isSchemaTagDone = false;
    bool isEndSchemaTag = false;
	pSchema = oszalloc(sizeof(pSchema), osXsdSchema_cleanup);
	//parse <xs:schema xxxx>
	if(osXsd_parseSchemaTag(pXmlBuf, &pSchema->schemaInfo, &isSchemaTagDone) != OS_STATUS_OK)
    {
        logError("fails to parse the xsd schema.");
		status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

	//for this XSD, xsAlias would not change.  In xml_parser, xsAlias will point to what is stored in the current rootElement (global element)
	pCurXSAlias = &pSchema->schemaInfo.xsAlias;

	osXsdElement_t* pRootElem = NULL;
	while(pXmlBuf->pos < pXmlBuf->end)
	{
		osXmlTagInfo_t* pGlobalElemTagInfo = NULL;
		status = osXsd_parseGlobalTag(pXmlBuf, &pSchema->gComplexList, &pSchema->gSimpleList, &pGlobalElemTagInfo, &isEndSchemaTag);

		if(status != OS_STATUS_OK)
		{
			logError("fails to osXsd_parseGlobalTag, pos=%ld", pXmlBuf->pos);
	        status = OS_ERROR_INVALID_VALUE;
			goto EXIT;
		}

		//find a global element
		if(pGlobalElemTagInfo)
		{
			if(pGlobalElemTagInfo->isTagDone)
            {
                //for <xs:element name="A" type="B" />
				pRootElem = oszalloc(sizeof(osXsdElement_t), osXsdElement_cleanup);
				pRootElem->isRootElement = true;
                osXmlElement_getAttrInfo(&pGlobalElemTagInfo->attrNVList, pRootElem);
            }
            else if(!pGlobalElemTagInfo->isEndTag)
            {
                //for <xs:element name="A" type="B" ><xs:complexType name="C">...</xs:complexType></xs:element>
                pRootElem = osXsd_parseElement(pXmlBuf, pGlobalElemTagInfo);
                if(!pRootElem)
                {
                    logError("fails to osXsd_parseElement for root element, pos=%ld.", pXmlBuf->pos);
	                status = OS_ERROR_INVALID_VALUE;
                    goto EXIT;
                }
                pRootElem->isRootElement = true;
				pRootElem->pSchema = &pSchema->schemaInfo;
			}

			osList_append(&pSchema->gElementList, pRootElem);
			mdebug(LM_XMLP, "Global element(%r : %p) is added into schema(%p)'s gElementList.", &pGlobalElemTagInfo->tag,  pRootElem, pSchema);
		}

		pGlobalElemTagInfo = osfree(pGlobalElemTagInfo);
		
		if(isEndSchemaTag)
		{
			mlogInfo(LM_XMLP, "xsd parse is completed.");
			break;
		}
	}

	// link the root element with the child complex type.
	osListElement_t* pLE = pSchema->gElementList.head;
	while(pLE)
	{
//tempPrint(&ctypeList, 1);
		pRootElem = pLE->data;
		status = osXsd_elemLinkChild(pRootElem, &pSchema->gComplexList, &pSchema->gSimpleList);
//tempPrint(&ctypeList, 2);
		pLE = pLE->next;
	}

EXIT:
	if(status != OS_STATUS_OK)
	{
		logError("status != OS_STATUS_OK, delete pRootElem.");
		pSchema = osfree(pSchema);
	}

	return pSchema;
}


/* finds the xsdElem node and notify the value to the user via callback.  This is useful when XML does not configure a element, the same
 * element in the XSD file shall be used as the default value and notify user
 *
 * pXsdElem: IN, an XSD element
 * callbackInfo: INOUT, the XSD value will be set as one of the parameters in the data structure.  Inside this function, it is passed as an
 *               input to the osXsd_elemCallback, which in turn as an input to xmlCallback function
 */
osStatus_e osXsd_browseNode(osXsdElement_t* pXsdElem, osXmlDataCallbackInfo_t* callbackInfo)
{
	osStatus_e status = OS_STATUS_OK;

    if(!pXsdElem)
    {
        logError("null pointer, pXsdElem=%p.", pXsdElem);
        status = OS_ERROR_NULL_POINTER;
		goto EXIT;
    }

	//callback to provide info for the specified xsdElement.
	status = osXsd_elemCallback(pXsdElem, callbackInfo);
	if(status != OS_STATUS_OK)
	{
		logError("fails to osXsd_elemCallback for element(%r).", &pXsdElem->elemName);
		goto EXIT;
	}

	if(pXsdElem->dataType == OS_XML_DATA_TYPE_COMPLEX)
	{
#if 0	//somehow making pCTPointer a dynamic variable caused program crash.  Nevertheless, make it a local variable is a better choice
    	osXsd_ctPointer_t* pCTPointer = oszalloc(sizeof(osXsd_ctPointer_t), NULL);
    	pCTPointer->pCT = pXsdElem->pComplex;
    	pCTPointer->doneListIdx = 0;
    	status = osXsd_transverseCT(pCTPointer, callbackInfo);
    	osfree(pCTPointer);
#else
        osXsd_ctPointer_t ctPointer = {};
        ctPointer.pCT = pXsdElem->pComplex;
        ctPointer.doneListIdx = 0;
        status = osXsd_transverseCT(&ctPointer, callbackInfo);
#endif
	}

EXIT:
	return status;
}


bool osXsd_isValid(osMBuf_t* pXsdBuf, osPointerLen_t* xsdName, bool isKeepNsList)
{
    if(!pXsdBuf || !xsdName)
    {
        logError("null pointer, pXsdBuf=%p, xsdName=%p.", pXsdBuf, xsdName);
        return false;
    }

    osXsdNamespace_t* pNS = osXsd_parse(pXsdBuf, xsdName);
    if(!pNS)
    {
        logError("fails to osXsd_parse for xsdMBuf, pos=%ld.", pXsdBuf->pos);
        return false;
    }

	if(!isKeepNsList)
	{
		osList_delete(&gXsdNSList);
	}

    return true;
}


/* link the parent and child complex type to make a tree.  When this function is called, all global complexType and simpleType shall have been resolved.
 * The first call of this function shall have pParentElem as the xsd root element.
 * pParentElem: IN, the parent element
 * pCTypeList:  IN, a list of osXmlComplexType_t entries
 * pSTypeList:  IN, a list of osXmlSimpleType_t entries
 */
static osStatus_e osXsd_elemLinkChild(osXsdElement_t* pParentElem, osList_t* pCTypeList, osList_t* pSTypeList)
{
	osStatus_e status = OS_STATUS_OK;

	if(!pParentElem || !pCTypeList)
	{
		logError("null pointyer, pParentElem=%p, pCTypeList=%p.", pParentElem, pCTypeList);
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

	mdebug(LM_XMLP, "pParentElem->elemTypeName=%r, isElementAny=%d, dataType=%d", &pParentElem->elemTypeName, pParentElem->isElementAny, pParentElem->dataType);
	//check if it is a xs:any element
	if(pParentElem->isElementAny)
	{
		goto EXIT;
	}

	//if not xs:any element
	switch(pParentElem->dataType)
	{
		case OS_XML_DATA_TYPE_NO_XS:
			pParentElem->pComplex = osXsd_getTypeByname(pCTypeList, &pParentElem->elemTypeName);
			//first check if the element is a complex type
			if(pParentElem->pComplex)
			{
				pParentElem->dataType = OS_XML_DATA_TYPE_COMPLEX;
	            osListElement_t* pLE = pParentElem->pComplex->elemList.head;
    	        while(pLE)
        	    {
            	    status = osXsd_elemLinkChild((osXsdElement_t*)pLE->data, pCTypeList, pSTypeList);
                	if(status != OS_STATUS_OK)
                	{
                    	logError("fails to osXsd_elemLinkChild for (%r).", pLE->data ? &((osXsdElement_t*)pLE->data)->elemName : NULL);
                    	goto EXIT;
                	}
                	pLE = pLE->next;
            	}
            	break;
			}

			//if not a complex type, check if it is a simple type
			pParentElem->pSimple = osXsd_getTypeByname(pSTypeList, &pParentElem->elemTypeName);
			if(pParentElem->pSimple)
			{
				pParentElem->dataType = OS_XML_DATA_TYPE_SIMPLE;
				break;
			}

			//the element type does not exist in both cTypeList and sTypeList
			logError("pParentElem->elemTypeName(%r) does not exist in both cTypeList and sTypeList.", &pParentElem->elemTypeName);
			status = OS_ERROR_INVALID_VALUE;
			goto EXIT;
			break;
		case OS_XML_DATA_TYPE_COMPLEX:
			//this is the case for a complex type embedded inside a <xs:element></xs:element>, the pParentElem->pComplex shall have been resolved
			if(!pParentElem->pComplex)
			{
				logError("xsd element=%r, an embedded complex type, but pParentElem->pComplex is null.", &pParentElem->elemName);
				status = OS_ERROR_INVALID_VALUE;
				goto EXIT;
			}
			break;
		case OS_XML_DATA_TYPE_SIMPLE:
            //this is the case for a simple type embedded inside a <xs:element></xs:element>, the pParentElem->pSimple shall have been resolved
            if(!pParentElem->pSimple)
            {
                logError("a embedded simple type, but pParentElem->pSimple is null.");
                status = OS_ERROR_INVALID_VALUE;
                goto EXIT;
            }
			break;
		default:
			mdebug(LM_XMLP, "xsd element(%r), dataType=%d, ignore.", &pParentElem->elemName, pParentElem->dataType);
			//already stored in each element's data structure
//tempPrint(pTypeList, 4);
			break;
	}

EXIT:
	return status;
}


/* The following is an XSD examlpe.
 *
 * <?xml version="1.0" encoding="UTF-8"?>
 * <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" attributeFormDefault="unqualified">
 *   <xs:element name="SeanDai.config" type="SeanDai.configType" />
 *   <xs:complexType name="SeanDai.configType">
 *       <xs:all>
 *       ....
 *       </xs:all>
 *   </xs:complexType>
 *   ....
 * </xs:schema>
 *
 * This function parses the root element (<xs:element name="SeanDai.config" type="SeanDai.configType" / as in the example> and 
 * and complexType (<xs:complexType name="SeanDai.configType">, etc., as in the example>
 *
 * pXmlBuf:            The XSD buffer, pos must points to the beginning of a line
 * pCTypeList:         INOUT, a osXmlComplexType_t data is added into the list when the Global tag is a complex type
 * pSTypeList:         INOUT, a osXmlSimpleType_t data is added into the list when the Global tag is a simple type
 * pGlobalElemTagInfo: OUT, element info.  Since a XSD has only one element, this must be the root element
 * isEndSchemaTag:     OUT, if the line is a scheme end tag </xs:schema>
 */
static osStatus_e osXsd_parseGlobalTag(osMBuf_t* pXmlBuf, osList_t* pCTypeList, osList_t* pSTypeList, osXmlTagInfo_t** pGlobalElemTagInfo, bool* isEndSchemaTag)
{
	osStatus_e status = OS_STATUS_OK;
    osList_t tagList = {};
    osXsdElement_t* pRootElement = NULL;
    osXmlTagInfo_t* pTagInfo = NULL;

    if(!pXmlBuf || !pCTypeList || !pSTypeList || !pGlobalElemTagInfo || !isEndSchemaTag)
    {
        logError("null pointer, pXmlBuf=%p, pCTypeList=%p, pSTypeList=%p, pGlobalElemTagInfo=%p, isEndSchemaTag=%p.", pXmlBuf, pCTypeList, pSTypeList, pGlobalElemTagInfo, isEndSchemaTag);
		status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

    *pGlobalElemTagInfo = NULL;
	*isEndSchemaTag = false;

    //get tag info for the immediate next tag
	status = osXml_parseTag(pXmlBuf, false, false, &pTagInfo, NULL);
    if(status != OS_STATUS_OK)
    {
		logError("fails to osXml_parseTag.");
        goto EXIT;
    }

	if(!pTagInfo)
	{
		mlogInfo(LM_XMLP, "all tags are parsed.");
		status = OS_ERROR_INVALID_VALUE;
		goto EXIT;
	}

	mdebug(LM_XMLP, "tag=%r", &pTagInfo->tag);

	osPointerLen_t* pXsAlias = osXsd_getXSAlias();
	size_t realTagStart = pXsAlias->l ? (pXsAlias->l+1) : 0;
	size_t realTagLen = pTagInfo->tag.l - realTagStart;
	switch(realTagLen)
	{
		case 11:        //len = 11, "complexType"
        {
			if(pTagInfo->tag.p[realTagStart] != 'c' || osXml_tagCmp(pXsAlias, "complexType", 11, &pTagInfo->tag) != 0)
			{
				mlogInfo(LM_XMLP, "top tag(%r) len=11, but is not complexType, ignore.", &pTagInfo->tag);
				break;
			}

			mdebug(LM_XMLP, "decode: xs:complexType, pXmlBuf->buf[pXmlBuf->pos]=%c, pXmlBuf->pos=%ld", pXmlBuf->buf[pXmlBuf->pos], pXmlBuf->pos);
			osXmlComplexType_t* pCtInfo = osXsdComplexType_parse(pXmlBuf, pTagInfo, NULL);
			if(!pCtInfo || !pCtInfo->typeName.l)
			{
				logError("complex global type is NULL or has no typeName, pCtInfo=%p, typeName.l=%ld.", pCtInfo, pCtInfo ? pCtInfo->typeName.l : 0);
				if(pCtInfo)
				{
					osfree(pCtInfo);
				}
				status = OS_ERROR_INVALID_VALUE;
				break;
			}

            //insert the complex type into typeList so that the type can be linked in by elements
            osList_append(pCTypeList, pCtInfo);
			break;
		}
		case 10:			//10, "simpleType"
		{
			if(pTagInfo->tag.p[realTagStart] != 's' || osXml_tagCmp(pXsAlias, "simpleType", 10, &pTagInfo->tag) != 0)
			{
				mlogInfo(LM_XMLP, "top tag(%r) len=10, but is not simpleType, ignore.", &pTagInfo->tag);
                break;
            }

            mdebug(LM_XMLP, "decode: xs:simpleType, pXmlBuf->buf[pXmlBuf->pos]=0x%x, pXmlBuf->pos=%ld", pXmlBuf->buf[pXmlBuf->pos], pXmlBuf->pos);
            osXmlSimpleType_t* pSimpleInfo = osXsdSimpleType_parse(pXmlBuf, pTagInfo, NULL);
            if(!pSimpleInfo || !pSimpleInfo->typeName.l)
            {
                logError("simple global type is NULL or has no typeName, pSimpleInfo=%p, typeName.l=%ld.", pSimpleInfo, pSimpleInfo ? pSimpleInfo->typeName.l : 0);

                if(pSimpleInfo)
                {
                    osfree(pSimpleInfo);
                }
                status = OS_ERROR_INVALID_VALUE;
                break;
            }

			//insert the simple type into sTypeList
			osList_append(pSTypeList, pSimpleInfo);
            break;
        }
		case 7:            //7, "element"
			if(osXml_tagCmp(pXsAlias, "element", 7, &pTagInfo->tag) != 0)
			{
				mlogInfo(LM_XMLP, "top tag(%r) len=7, but is not element, ignore.", &pTagInfo->tag);
                goto EXIT;
            }

			mdebug(LM_XMLP, "decode: xs:element");

			*pGlobalElemTagInfo = pTagInfo;
        	break;
		case 6:				//6, "schema"
			if(osXml_tagCmp(pXsAlias, "schema", 6, &pTagInfo->tag) != 0)
            {
                mlogInfo(LM_XMLP, "top tag(%r) len=6, but is not xs:schema, ignore.", &pTagInfo->tag);
                break;
            }

			if(pTagInfo->isEndTag)
			{
				mlogInfo(LM_XMLP, "isEndTag=true for the schema tag, xsd parse is completed.");
				*isEndSchemaTag = true;
			}
			break;
		default:
			mlogInfo(LM_XMLP, "top tag(%r) is ignored.", &pTagInfo->tag);
			break;
    }

EXIT:
	//only release pTagInfo if not used by pGlobalElemTagInfo
	if(!*pGlobalElemTagInfo)
	{
		osfree(pTagInfo);
	}

    return status;
}



//parse <xs:schema xxxx>
osStatus_e osXsd_parseSchemaTag(osMBuf_t* pXmlBuf, osXsd_schemaInfo_t* pSchemaInfo, bool* isSchemaTagDone)
{
	osStatus_e status = OS_STATUS_OK;
    osXmlTagInfo_t* pTagInfo = NULL;

	if(!pXmlBuf || !pSchemaInfo)
	{
		logError("null pointer, pXmlBuf=%p, pSchemaInfo=%p.", pXmlBuf, pSchemaInfo);
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

	bool isEndTag = false;
	status = osXml_parseTag(pXmlBuf, false, false, &pTagInfo, NULL);

    if(status != OS_STATUS_OK)
    {
        logError("fails to osXml_parseTag for schema tag.");
        goto EXIT;
    }

    if(pTagInfo->isEndTag)
    {
        logError("isEndTag = true for the schema tag.");
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

    if(!pTagInfo)
    {
        mlogInfo(LM_XMLP, "pTagInfo = NULL.");
        goto EXIT;
    }

    *isSchemaTagDone = pTagInfo->isTagDone;

	osPointerLen_t schemaXSAlias;
	if(!osXml_singleDelimitMatch("schema", 6, ':', &pTagInfo->tag, true, &schemaXSAlias))
    {
        logError("expect schema tag, but instead, it is (%r).", &pTagInfo->tag);
		status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

	if(pTagInfo->isPElement)
	{
		logError("a schema tag, expect attrNVList.");
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

	bool isXSAliasFound = false;
	osListElement_t* pLE = pTagInfo->attrNVList.head;
	osXsd_nsAliasInfo_t* pnsAlias;
	while(pLE)
	{
		status = osXml_getNSAlias((osXmlNameValue_t*)pLE->data, &pSchemaInfo->defaultNS, &pnsAlias, &isXSAliasFound, &pSchemaInfo->xsAlias);
		if(status != OS_STATUS_OK)
		{
			logError("fails to osXml_getNSAlias().");
			goto EXIT;
		}

		//find a nsalias
		if(pnsAlias)
		{
			osList_append(&pSchemaInfo->nsAliasList, pnsAlias);
		}
		else if(osPL_strplcmp("targetNamespace", 15, &((osXmlNameValue_t*)pLE->data)->name, true) == 0)
		{
			pSchemaInfo->targetNS = ((osXmlNameValue_t*)pLE->data)->value;
		}

		pLE = pLE->next;
	}

	if(!isXSAliasFound)
	{
		logError("the xsd does not have namespace \"http://www.w3.org/2001/XMLSchema\".");
		status = OS_ERROR_INVALID_VALUE;
		goto EXIT;
	}	

	if(osPL_cmp(&pSchemaInfo->xsAlias, &schemaXSAlias))
	{
		logError("schema(%r) namespace(%r) does not match with XS namespace(%r)", &pTagInfo->tag, &pSchemaInfo->xsAlias, &schemaXSAlias); 
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

EXIT:
	osfree(pTagInfo);

	if(status != OS_STATUS_OK && pSchemaInfo)
	{
		osList_delete(&pSchemaInfo->nsAliasList);
	}

	return status;
}


/* parse information wrapped by <xs:element> xxxx </xs:element>
 * using the following XSD snapshot as an example
 *
 * ......
 *   <xs:complexType name="SeanDai.configType">
 *       <xs:all>
 *     -->   <xs:element name="configFileType" type="xs:integer" fixed="1"
 *               minOccurs="0" maxOccurs="0">
 *               <xs:annotation>
 *                   <xs:documentation>
 *                       sip configuration
 *                   </xs:documentation>
 *               </xs:annotation>
 *           </xs:element>
 *           <xs:element name="SeanDai.component-specific" type="ComponentSpecificType" />
 *       </xs:all>
 *   </xs:complexType>
 *   ......
 *
 * This function can parse info wrapped by <xs:element name="configFileType" type="xs:integer" fixed="1"  minOccurs="0" maxOccurs="0">.
 * The parse starts from the first tag after the <xs:element xxxx>
 * pXmlBuf:      IN, XML/XSD buffer, the pos points to the first tag after the <xs:element xxxx>
 * pElemTagInfo: IN, the <xs:element xxxx> tag info, like name, type, minOccurrs, maxOccurs, etc.
 * return value: OUT, osXsdElement_t, 
 */
osXsdElement_t* osXsd_parseElement(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pElemTagInfo)
{
	DEBUG_BEGIN

	osStatus_e status = OS_STATUS_OK;
    osList_t tagList = {};
	osXsdElement_t* pElement = NULL;
    osXmlTagInfo_t* pTagInfo = NULL;

	if(!pXmlBuf || !pElemTagInfo)
	{
		logError("null pointer, pXmlBuf=%p, pElemTagInfo=%p.", pXmlBuf, pElemTagInfo);
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

    pElement = oszalloc(sizeof(osXsdElement_t), osXsdElement_cleanup);
    osXmlElement_getAttrInfo(&pElemTagInfo->attrNVList, pElement);

	//starts to parse the sub tags of xs:element
    while(pXmlBuf->pos < pXmlBuf->end)
    {
        //get tag info for each tags inside xs:element
		status = osXml_parseTag(pXmlBuf, false, false, &pTagInfo, NULL);
        if(status != OS_STATUS_OK || !pTagInfo)
        {
        	logError("fails to osXml_parseTag.");
            goto EXIT;
        }

        if(pTagInfo->isEndTag)
        {
            osListElement_t* pLE = osList_popTail(&tagList);
            if(!pLE)
            {
            	//"xs:element" was not pushed into tagList, so it is possible the end tag is "xs:element"
				if(osXml_xsTagCmp("element", 7, &pTagInfo->tag))
                {
                	logError("expect the end tag for xs:element, but %r is found.", &((osXmlTagInfo_t*)pLE->data)->tag);
                }

				goto EXIT;
			}

            //compare the end tag name (newly gotten) and the beginning tag name (from the LE)
            if(osPL_cmp(&((osXmlTagInfo_t*)pLE->data)->tag, &pTagInfo->tag) == 0)
            {
            	osXmlElement_getSubTagInfo(pElement, (osXmlTagInfo_t*)pLE->data);
				osListElement_delete(pLE);
				pTagInfo = osfree(pTagInfo);
                continue;
            }
            else
            {
                logError("input xml is invalid, error in (%r).", &pTagInfo->tag);
				osListElement_delete(pLE);
                goto EXIT;
            }
		}

        if(pTagInfo->isTagDone)
        {
		//case for <xs:xxxx xxx\>
	        //no need to push to the tagList as the end tag is part of the line
            osXmlElement_getSubTagInfo(pElement, pTagInfo);
			pTagInfo = osfree(pTagInfo);
        }
        else
       	{
		//case for <xs:xxx xxx>
        	//special treatment for xs:complexType, as xs:complex can contain own tags.  No need to add pTagInfo to tagList as 
        	//osXsdComplexType_parse() will process the whole <xs:complexType xxx> xxxx </xs:complexType>
            if(osXml_xsTagCmp("complexType", 11, &pTagInfo->tag) == 0)
            {
				//we do not need pTagInfo any more
				pTagInfo = osfree(pTagInfo);

				//the link between pElement and child type is done inside osXsdComplexType_parse()
	            osXmlComplexType_t* pCtInfo = osXsdComplexType_parse(pXmlBuf, pTagInfo, pElement);
	            if(!pCtInfo || pCtInfo->typeName.l)
    	        {
					//complex type within a element does not have type name
                	logError("complexType is NULL or has typeName, pCtInfo=%p, typeName=%r.", pCtInfo, pCtInfo ? &pCtInfo->typeName : 0);
					if(pCtInfo)
					{
						osfree(pCtInfo);
					}
                	goto EXIT;
				}	
            }
            else
            {
                //add the beginning tag to the tagList, the info in the beginning tag will be processed when the end tag is met
               	osList_append(&tagList, pTagInfo);
            }
        }
	}

EXIT:
	if(pTagInfo)
	{
		osfree(pTagInfo);
	}

	osList_delete(&tagList);

	DEBUG_END
	return pElement;
}


/* parse <xs:any> xxx </xs:any>
 */
osXsdElement_t* osXsd_parseElementAny(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pElemTagInfo)
{
    osStatus_e status = OS_STATUS_OK;
    osList_t tagList = {};
    osXsdElement_t* pElement = NULL;
    osXmlTagInfo_t* pTagInfo = NULL;

    if(!pXmlBuf || !pElemTagInfo)
    {
        logError("null pointer, pXmlBuf=%p, pElemTagInfo=%p.", pXmlBuf, pElemTagInfo);
        status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

    pElement = oszalloc(sizeof(osXsdElement_t), osXsdElement_cleanup);
    pElement->isElementAny = true;
    pElement->anyElem.elemAnyTag.processContent = OS_XSD_PROCESS_CONTENTS_STRICT;

    status = osXmlElement_getAttrInfo(&pElemTagInfo->attrNVList, pElement);
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

    //starts to parse the sub tags of xs:any, ignore all sub tags
    while(pXmlBuf->pos < pXmlBuf->end)
    {
        //get tag info for each tags inside xs:element
        status = osXml_parseTag(pXmlBuf, false, false, &pTagInfo, NULL);
        if(status != OS_STATUS_OK || !pTagInfo)
        {
            logError("fails to osXml_parseTag.");
            goto EXIT;
        }

        if(pTagInfo->isEndTag)
        {
            osListElement_t* pLE = osList_popTail(&tagList);
            if(!pLE)
            {
                //"xs:element" was not pushed into tagList, so it is possible the end tag is "xs:element"
                if(osXml_xsTagCmp("element", 7, &pTagInfo->tag))
                {
                    logError("expect the end tag for xs:element, but %r is found.", &((osXmlTagInfo_t*)pLE->data)->tag);
                }

                goto EXIT;
            }

            //compare the end tag name (newly gotten) and the beginning tag name (from the LE)
            if(osPL_cmp(&((osXmlTagInfo_t*)pLE->data)->tag, &pTagInfo->tag) == 0)
            {
                osXmlElement_getSubTagInfo(pElement, (osXmlTagInfo_t*)pLE->data);
                osListElement_delete(pLE);
                pTagInfo = osfree(pTagInfo);
                continue;
            }
            else
            {
                logError("input xml is invalid, error in (%r).", &pTagInfo->tag);
                osListElement_delete(pLE);
                goto EXIT;
            }
        }

        if(pTagInfo->isTagDone)
        {
        //case for <xs:xxxx xxx\>
            //no need to push to the tagList as the end tag is part of the line
            osXmlElement_getSubTagInfo(pElement, pTagInfo);
            pTagInfo = osfree(pTagInfo);
        }
        else
        {
        //case for <xs:xxx xxx>
            //special treatment for xs:complexType, as xs:complex can contain own tags.  No need to add pTagInfo to tagList as
            //osXsdComplexType_parse() will process the whole <xs:complexType xxx> xxxx </xs:complexType>
            if(osXml_xsTagCmp("complexType", 11, &pTagInfo->tag) == 0)
            {
                //we do not need pTagInfo any more
                pTagInfo = osfree(pTagInfo);

                //the link between pElement and child type is done inside osXsdComplexType_parse()
                osXmlComplexType_t* pCtInfo = osXsdComplexType_parse(pXmlBuf, pTagInfo, pElement);
                if(!pCtInfo || pCtInfo->typeName.l)
                {
                    //complex type within a element does not have type name
                    logError("complexType is NULL or has typeName, pCtInfo=%p, typeName=%r.", pCtInfo, pCtInfo ? &pCtInfo->typeName : 0);
                    if(pCtInfo)
                    {
                        osfree(pCtInfo);
                    }
                    goto EXIT;
                }
            }
            else
            {
                //add the beginning tag to the tagList, the info in the beginning tag will be processed when the end tag is met
                osList_append(&tagList, pTagInfo);
            }
        }
    }

EXIT:
    if(pTagInfo)
    {
        osfree(pTagInfo);
    }

    osList_delete(&tagList);

    DEBUG_END
    return pElement;
}


osStatus_e osXmlElement_getAttrInfo(osList_t* pAttrList, osXsdElement_t* pElement)
{
	osStatus_e status = OS_STATUS_OK;
	if(!pElement || !pAttrList)
	{
		logError("null pointer, pElement=%p, pAttrList=%p.", pElement, pAttrList);
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

	//set up the default value
    pElement->minOccurs = 1;
    pElement->maxOccurs = 1;

	osListElement_t* pLE = pAttrList->head;
	while(pLE)
	{
		osXmlNameValue_t* pNV = pLE->data;
		if(!pNV)
		{
			logError("pNV is NULL, this shall never happen.");
			status = OS_ERROR_INVALID_VALUE;
			goto EXIT;
		}

		switch(pNV->name.p[0])
		{
			case 'n':	//for "name", "namespace"
				if(pNV->name.l == 4 && strncmp("name", pNV->name.p, pNV->name.l) == 0)		
				{
					pElement->elemName = pNV->value;
				}
				else if(pElement->isElementAny && pNV->name.l == 9 && strncmp("namespace", pNV->name.p, pNV->name.l) == 0)
				{
					if(pNV->value.l == 5 && osPL_strcmp(&pNV->value, "##any") ==0)
					{
						 pElement->anyElem.elemAnyTag.elemNamespace = OS_XSD_NAME_SPACE_ANY;
					}
					else if(pNV->value.l == 7 && osPL_strcmp(&pNV->value, "##other") == 0)
					{
						pElement->anyElem.elemAnyTag.elemNamespace = OS_XSD_NAME_SPACE_OTHER; 
					}
					else
					{
						//for namespace, only support "##any" and "##other"
						logError("is a any element, but namespace value(%r) is not supported(##any, ##other).", &pNV->value);
						status = OS_ERROR_INVALID_VALUE;
						goto EXIT;
					}
				}
				break;
			case 't':	//for type
				if(pNV->name.l == 4 && strncmp("type", pNV->name.p, pNV->name.l) == 0)
                {
					pElement->elemTypeName = pNV->value;
				}
				pElement->dataType = osXsd_getElemDataType(&pElement->elemTypeName);
				break;
			case 'd':	//default
                if(pNV->name.l == 7 && strncmp("default", pNV->name.p, pNV->name.l) == 0)
                {
                    pElement->elemDefault = pNV->value;
                }
                break;
			case 'm':	//minOccurs, maxOccurs
				if(pNV->name.l == 9)
				{
					int isMatchValue = -1;		
					if(strncmp("minOccurs", pNV->name.p, pNV->name.l) == 0)
                	{
						isMatchValue = 0;
					}
					else if(strncmp("maxOccurs", pNV->name.p, pNV->name.l) == 0)
                    {
                        isMatchValue = 1;
                    }

					if(isMatchValue != -1)
					{
						uint32_t value;
						if(pNV->value.l ==9  && strncmp("unbounded", pNV->value.p, 9) == 0)
						{
							value = -1;
						}
						else
						{ 
							status = osPL_convertStr2u32(&pNV->value, &value);
						}
						if(status != OS_STATUS_OK)
						{
							logError("minOccurs or maxOccurs(%r) is not numerical.", &pNV->value);
							goto EXIT;
						}
						else
						{
							if(isMatchValue == 0)
							{
								pElement->minOccurs = value;
								if(value == -1)
								{
									logError("minCoours=unbounded, it is not allowed.");
									status = OS_ERROR_INVALID_VALUE;
									goto EXIT;
								}
							}
							else
							{
								pElement->maxOccurs = value;
							}
						}
					}
                }
				break;
			case 'f':	//fixed, form
                if(pNV->name.l == 5 && strncmp("fixed", pNV->name.p, pNV->name.l) == 0)
                {
                    pElement->fixed = pNV->value;
                }
				else if(pNV->name.l == 4 && strncmp("form", pNV->name.p, pNV->name.l) == 0)
                {
					if(pNV->value.l == 9 && strncmp("qualified", pNV->value.p, pNV->value.l) == 0)
					{
						pElement->isQualified = true;
					}
					else if(pNV->value.l == 11 && strncmp("unqualified", pNV->value.p, pNV->value.l) == 0)
                   	{
                       	pElement->isQualified = false;
                    }
					else
					{
						logError("form attribute has wrong value (%r).", &pNV->value);
					}
				}
				break;
			case 'p':	//processContents
				if(pElement->isElementAny && pNV->name.l == 15 && osPL_strcmp(&pNV->name, "processContents") == 0)
				{
					if(osPL_strcmp(&pNV->value, "lax") == 0)
					{
						pElement->anyElem.elemAnyTag.processContent = OS_XSD_PROCESS_CONTENTS_LAX;
					}
					else if(osPL_strcmp(&pNV->value, "skip") == 0)
					{
						pElement->anyElem.elemAnyTag.processContent = OS_XSD_PROCESS_CONTENTS_SKIP;
					}
					else
					{
	                    logError("is a any element, but processContents value(%r) is not supported (lax, skip).", &pNV->value);
    	                status = OS_ERROR_INVALID_VALUE;
        	            goto EXIT;
					}
				}
				break;
			default:
				logError("unexpected element attribute, ignore.");
				break;
		}

		pLE = pLE->next;
	}

EXIT:
	return status;	
}


static osStatus_e osXmlElement_getSubTagInfo(osXsdElement_t* pElement, osXmlTagInfo_t* pTagInfo)
{
	//the parent element and child type link is already done as part of element parse, for now, this function does noting.

	return OS_STATUS_OK;
}




/* return a matching osComplexType_t or osXmlSimpleType_t.
 * This function applies to both cTypeList and sTypeList.  Since for both osComplexType_t and osXmlSimpleType_t, the typeName is always the first parameter.
 * it does not matter which type to cast in osPL_cmp().  In the implementation, osXmlComplexType_t* is casted
 *
 * pTypeList:     IN, either pCTypeList or pSTypeList.
 * pElemTypeName: IN, the type name of an element that is trying to get the type and type object
 * return value:  void*, can be either osXmlComplexType_t or osXmlSimpleType_t.  The caller shall know which it is based on the pTypeList it used
 */
static void* osXsd_getTypeByname(osList_t* pTypeList, osPointerLen_t* pElemTypeName)
{
	if(!pTypeList || !pElemTypeName)
	{
		logError("null pointer, pTypeList=%p, pElemTypeName=%p.", pTypeList, pElemTypeName);
		return NULL;
	}

	osListElement_t* pLE = pTypeList->head;
	while(pLE)
	{
		if(osPL_cmp(&((osXmlComplexType_t*)pLE->data)->typeName, pElemTypeName) == 0)
		{
			return pLE->data;
		}

		pLE = pLE->next;
	}

	return NULL;
} 

		

static bool osXml_isXsdElemXSType(osXsdElement_t* pXsdElem)
{
    if(!pXsdElem)
    {
        logError("null pointer, pXsdElem.");
        return false;
    }

    switch(pXsdElem->dataType)
    {
		case OS_XML_DATA_TYPE_INVALID:
		case OS_XML_DATA_TYPE_NO_XS:
		case OS_XML_DATA_TYPE_SIMPLE:
		case OS_XML_DATA_TYPE_COMPLEX:
		case OS_XML_DATA_TYPE_ANY:
			return false;
            break;
        default:
            break;
    }

    return true;
}


osXmlDataType_e osXsd_getElemDataType(osPointerLen_t* typeValue)
{
	osXmlDataType_e dataType = OS_XML_DATA_TYPE_INVALID;

	if(!typeValue)
	{
		logError("null pointer, typeValue.");
		goto EXIT;
	}

    osPointerLen_t* pXsAlias = osXsd_getXSAlias();

	//check if the typeValue starts with "xs"
	if(strncmp(pXsAlias->p, typeValue->p, pXsAlias->l) != 0)
	{
		dataType = OS_XML_DATA_TYPE_NO_XS;
		goto EXIT;
	}

    size_t realTagStart = pXsAlias->l ? (pXsAlias->l+1) : 0;
    size_t realTagLen = typeValue->l - realTagStart;
    switch(realTagLen)
	{
        case 3:	//xs:int
            if(strncmp("int", &typeValue->p[realTagStart], 3) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_INTEGER;
                goto EXIT;
            }
            break;
        case 4:
            if(strncmp("long", &typeValue->p[realTagStart], 4) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_LONG;
				goto EXIT;
            }
            break;
		case 5:
            if(strncmp("short", &typeValue->p[realTagStart], 5) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_SHORT;
				goto EXIT;
            }
			break;
		case 6:		//xs:string, xs:anyURI
            if(typeValue->p[realTagStart] == 's' && strncmp("string", &typeValue->p[realTagStart], 6) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_STRING;
				goto EXIT;
            }
            else if (typeValue->p[realTagStart] == 'a' && strncmp("anyURI", &typeValue->p[realTagStart], 6) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_STRING;
				goto EXIT;
            }
			break;
		case 7:	//xs:integer, xs:boolean
			if(typeValue->p[realTagStart] == 'i' && strncmp("integer", &typeValue->p[realTagStart], 7) == 0)
			{
				dataType = OS_XML_DATA_TYPE_XS_INTEGER;
				goto EXIT;
			}
			else if(typeValue->p[realTagStart] == 'b' && strncmp("boolean", &typeValue->p[realTagStart], 7) == 0)
			{
				dataType = OS_XML_DATA_TYPE_XS_BOOLEAN;
				goto EXIT;
			}
			break;
        case 8:    //xs:dateTime
            if(typeValue->p[realTagStart] == 'd' && strncmp("dateTime", &typeValue->p[realTagStart], 8) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_STRING;
				goto EXIT;
            }
            break;

		case 12:	//xs:unsignedByte, xs:base64Binary
			if(typeValue->p[realTagStart] == 'u' && strncmp("unsignedByte", &typeValue->p[realTagStart], 12) == 0)
			{
				dataType = OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE;
				goto EXIT;
			}
            else if(typeValue->p[realTagStart] == 'b' && strncmp("xs:base64Binary", &typeValue->p[realTagStart], 12) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_STRING;
                goto EXIT;
            }
            break;
		default:
			break;		
	}

	mlogInfo(LM_XMLP, "xml data type(%r) is not supported, ignore.", typeValue);

EXIT:
	return dataType;
}



osPointerLen_t* osXsd_getXSAlias()
{
	return pCurXSAlias;
}


void osXsd_setXSAlias(osPointerLen_t* pXsAlias)
{
	pCurXSAlias = pXsAlias;
} 


/* for callback of default or fixed values defined in XSD when the specified xsdElem is a leaf node (i.e., xs simpleType or custome simpleType)
 * pXsdElem:     IN, a xsd element
 * callbackInfo: OUT, to store callback info
 */
osStatus_e osXsd_elemCallback(osXsdElement_t* pXsdElem, osXmlDataCallbackInfo_t* callbackInfo)
{
	osStatus_e status = OS_STATUS_OK;

	if(!pXsdElem)
	{
		logError("null pointer, pXsdElem.");
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

	if(osXml_isXsdElemSimpleType(pXsdElem))
	{
       	if(pXsdElem->fixed.p && pXsdElem->fixed.l > 0)
        {
        	status = osXml_xmlCallback(pXsdElem, &pXsdElem->fixed, NULL, callbackInfo, NULL);
        }
        else if(pXsdElem->elemDefault.p && pXsdElem->elemDefault.l > 0)
        {
        	status = osXml_xmlCallback(pXsdElem, &pXsdElem->elemDefault, NULL, callbackInfo, NULL);
		}

		if(status != OS_STATUS_OK)
		{
			logError("fails to validate element(%r).", &pXsdElem->elemName);
			goto EXIT;
		}
	}

EXIT:
	return status;
}


/* first assume pTargetNS is a real NS target name.  if does not ind match, assume pTargetNS is a xsd name, search schema within a NULL NS
 */
bool osXsd_isExistSchema(osPointerLen_t* pTargetNS)
{
    osXsdNamespace_t* pNS = NULL;
	osXsdNamespace_t* pNullNS = NULL;
	bool isExistNS = false;

    if(!pTargetNS)
    {
        logError("NULL pointer, pTargetNS=%p.", pTargetNS);
        goto EXIT;
    }

    osListElement_t* pLE = gXsdNSList.head;
    while(pLE)
    {
		if(((osXsdNamespace_t*)pLE->data)->pTargetNS == NULL)
		{
			pNullNS = pLE->data;
		}
		else
		{
        	if(osPL_cmp(pTargetNS, ((osXsdNamespace_t*)pLE->data)->pTargetNS) == 0)
        	{
				isExistNS = true;
            	goto EXIT;
        	}
		}
        pLE = pLE->next;
    }

	//check if there is a schema in the null NS that matches the target name
	if(pNullNS)
	{
        osListElement_t* pLE1 = pNS->schemaList.head;
        while(pLE1)
        {
			if(osPL_cmp(&((osXsdSchema_t*)pLE1->data)->schemaInfo.targetNS, pTargetNS) == 0)
			{
				isExistNS = true;
				break;
			}

			pLE1 = pLE1->next;
		}
	}

EXIT:
	return isExistNS;
}
	

static osXsdNamespace_t* osXsd_getNS(osList_t* pXsdNSList, osPointerLen_t* pTargetNS, bool isCreateNS)
{
	osXsdNamespace_t* pNS = NULL;

	if(!pXsdNSList || !pTargetNS)
	{
		logError("NULL pointer, pXsdNSList=%p, pTargetNS=%p.", pXsdNSList, pTargetNS);
		goto EXIT;
	}

	osListElement_t* pLE = pXsdNSList->head;
	while(pLE)
	{
		if(osPL_cmp(pTargetNS, ((osXsdNamespace_t*)pLE->data)->pTargetNS) == 0)
        {
        	pNS = pLE->data;
            goto EXIT;
        }

		pLE = pLE->next;
	}

	if(isCreateNS)
	{
		pNS = oszalloc(sizeof(osXsdNamespace_t), osXsdNS_cleanup);
	}
EXIT:
	return pNS;
}
	

/* get a global element from a NS based on element tag.  The NS is identified from global gXsdNSList based on NS name
 * 
 * pTargetNS:       IN, the NS name or a xsd name
 * isEmptyTargetNS: IN, if the NS a empty NS.  If empty NS, the pTargetNS contains a xsd name
 * pElemTag:        IN, the tag name of the global element
 */
osXsdElement_t* osXsd_getNSRootElem(osPointerLen_t* pTargetNS, bool isEmptyTargetNS, osPointerLen_t* pElemTag)
{
	osXsdElement_t* pElem = NULL;
	osXsdNamespace_t* pNS = NULL;

	if(!pTargetNS || !pElemTag)
	{
		logError("null pointer, pTargetNS=%p, pElemTag=%p.", pTargetNS, pElemTag);
		return NULL;
	}

	//find the right NS from gXsdNSList based on pTargetNS and isEmptyTargetNS
	osListElement_t* pLE = gXsdNSList.head;
	while(pLE)
	{
		if(isEmptyTargetNS)
		{
			if(!((osXsdNamespace_t*)pLE->data)->pTargetNS)
			{
				pNS = pLE->data;
				break;
			}
		}
		else
		{
			if(osPL_cmp(pTargetNS, ((osXsdNamespace_t*)pLE->data)->pTargetNS) == 0)
			{
				pNS = pLE->data;
				break;
			}
		}

		pLE = pLE->next;
	}

	if(pNS)
	{
		//from the NS, compare the element based on the pElemTag.  if the NS is a empty NS, find the right schema first
		osListElement_t* pLE1 = pNS->schemaList.head;
		while(pLE1)
		{
			//for xsd name, find the schema that matching the xsd name
			if(isEmptyTargetNS)
			{
				if(osPL_cmp(&((osXsdSchema_t*)pLE1->data)->schemaInfo.targetNS, pTargetNS) == 0)
				{
					pElem = osXsd_getElementFromList(&((osXsdSchema_t*)pLE1->data)->gElementList, pElemTag);
					break;
				}
			}
			else
			{
				//for targetNS, going through the schema one after another (a ns may contains multiple schema)
				pElem = osXsd_getElementFromList(&((osXsdSchema_t*)pLE1->data)->gElementList, pElemTag);
				if(pElem)
				{
					break;
				}
			}

			pLE1 = pLE1->next;
		}
	}

EXIT:
	mdebug(LM_XMLP, "root element for tag(%r) in pTargetNS(%r), isEmptyTargetNS(%d) is %s", pElemTag, pTargetNS, isEmptyTargetNS, pElem ? "found" : "not found");
	return pElem;
}


static osXsdElement_t* osXsd_getElementFromList(osList_t* pList, osPointerLen_t* pTag)
{
    osXsdElement_t* pElem = NULL;
    if(!pList || !pTag)
    {
        return NULL;
    }

    osListElement_t* pLE = pList->head;
    while(pLE)
    {
        if(osPL_cmp(&((osXsdElement_t*)pLE->data)->elemName, pTag) == 0)
        {
            pElem = pLE->data;
            break;
        }

        pLE = pLE->next;
    }

EXIT:
    return pElem;
}


void osXsdElement_cleanup(void* data)
{
    if(!data)
    {
        return;
    }

	//only cleanup the complexType object, do not free the object, as a complexType may be used by multiple elements, if each element
	//deletes the same complexType object, over free a memory will happen. As a matter of fact, since the xsd tree shall be kept for the 
	//whole program life.  if something wrong during the middle of building the xsd tree, the osList_delete(&ctypeList) during xsd_parse 
	//will take care of freeing the complexType object memory, otherwise, all complexType objects will be kept forever.  
	//The same for the simpleType object memory.  If something wrong during the xsd tree building, osList_free(pSTypeList) will
	//release the simpleType object memory, otherwise, they will be kept forever
	//osXmlComplexType_cleanup is needed to recursively delete all elements starting from the root element 
    osXsdElement_t* pElement = data;
    if(pElement->dataType == OS_XML_DATA_TYPE_COMPLEX)
    {
		osXmlComplexType_cleanup(pElement->pComplex);
    }
}


void osXsdSchema_cleanup(void* data)
{
	if(!data)
	{
		return;
	}

	osXsdSchema_t* pSchema = data;
	osList_delete(&pSchema->gElementList);
	osList_delete(&pSchema->gComplexList);
	osList_delete(&pSchema->gSimpleList);
	osList_delete(&pSchema->schemaInfo.nsAliasList);
}


void osXsdNS_cleanup(void* data)
{
    if(!data)
    {
        return;
    }

	osList_delete(&((osXsdNamespace_t*)data)->schemaList); 
}


void osXsd_freeNsList()
{
	osList_delete(&gXsdNSList);
}
