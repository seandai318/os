/********************************************************
 * Copyright (C) 2020 Sean Dai
 *
 * @file osXmlParser.c
 * xsd and xml parser, support the following xml format:
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
#include "osXmlParserLib.h"
#include "osXmlParserCType.h"
#include "osXmlParserSType.h"


#define OS_XML_COMPLEXTYPE_LEN	14
#define OS_XML_SIMPLETYPE_LEN	13
#define OS_XML_ELEMENT_LEN 		10
#define OS_XML_SCHEMA_LEN		9


#if 0
//for function osXml_parseTag()
typedef enum {
    OS_XSD_TAG_INFO_START,
	OS_XSD_TAG_INFO_TAG_COMMENT,
    OS_XSD_TAG_INFO_BEFORE_TAG_INSIDE_QUOTE,
    OS_XSD_TAG_INFO_TAG_START,
    OS_XSD_TAG_INFO_TAG,
	OS_XSD_TAG_INFO_CONTENT_NAME_START,
	OS_XSD_TAG_INFO_CONTENT_NAME,
	OS_XSD_TAG_INFO_CONTENT_NAME_STOP,
	OS_XSD_TAG_INFO_CONTENT_VALUE_START,
	OS_XSD_TAG_INFO_CONTENT_VALUE,
    OS_XSD_TAG_INFO_END_TAG_SLASH,
} osXsdCheckTagInfoState_e;


//this data structure used to help xml parsing against its xsd
typedef struct osXsd_elemPointer {
    osXsdElement_t* pCurElem;       //the current working xsd element
	struct osXsd_elemPointer* pParentXsdPointer;	//the parent xsd pointer
	int curIdx;						//which idx in assignedChildIdx[] that the xsd element is current processing, used in for the ordering presence of sequence deposition
    bool  assignedChildIdx[OS_XSD_COMPLEX_TYPE_MAX_ALLOWED_CHILD_ELEM]; //if true, the list idx corresponding child element value has been assigned
} osXsd_elemPointer_t;
#endif
#if 0
typedef struct ctPointer {
    osXmlComplexType_t* pCT;
    int doneListIdx;        //listIdx that has already transversed
} osXsd_ctPointer_t;
#endif


//static osXsdElement_t* osXsd_getRootElemInfo(osMBuf_t* pXmlBuf);
static osXsdElement_t* osXsd_parse(osMBuf_t* pXmlBuf);
static osStatus_e osXsd_elemLinkChild(osXsdElement_t* pParentElem, osList_t* pCTypeList, osList_t* pSTypeList);
static osStatus_e osXsd_parseGlobalTag(osMBuf_t* pXmlBuf, osList_t* pTypeList, osList_t* pSTypeList, osXmlTagInfo_t** pGlobalElemTagInfo, bool* isEndSchemaTag);
static void* osXsd_getTypeByname(osList_t* pTypeList, osPointerLen_t* pElemTypeName);
static osStatus_e osXml_parseFirstTag(osMBuf_t* pBuf);
static osStatus_e osXsd_parseSchemaTag(osMBuf_t* pXmlBuf, bool* isSchemaTagDone);
static osStatus_e osXsd_browseNode(osXsdElement_t* pXsdElem, osXmlDataCallbackInfo_t* callbackInfo);

static osStatus_e osXml_xmlCallback(osXsdElement_t* pElement, osPointerLen_t* value, osXmlDataCallbackInfo_t* callbackInfo);
static osStatus_e osXml_parse(osMBuf_t* pBuf, osXsdElement_t* pXsdRootElem, osXmlDataCallbackInfo_t* callbackInfo);
static osStatus_e osXmlElement_getSubTagInfo(osXsdElement_t* pElement, osXmlTagInfo_t* pTagInfo);
static bool osXml_findWhiteSpace(osMBuf_t* pBuf, bool isAdvancePos);
static bool osXml_findPattern(osMBuf_t* pXmlBuf, osPointerLen_t* pPattern, bool isAdvancePos);
static osXsdElement_t* osXml_getChildXsdElemByTag(osPointerLen_t* pTag, osXsd_elemPointer_t* pXsdPointer, osXmlElemDispType_e* pParentXsdDispType, int* listIdx);
static osXmlComplexType_t* osXsdPointer_getCT(osXsd_elemPointer_t* pXsdPointer);
//static osStatus_e osXmlXSType_convertData(osPointerLen_t* elemName, osPointerLen_t* value, osXmlDataType_e dataType, osXmlData_t* pXmlData);
static void osXmlTagInfo_cleanup(void* data);



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


osXsdElement_t* osXsd_parse(osMBuf_t* pXmlBuf)	
{
	osStatus_e status = OS_STATUS_OK;	
	osXsdElement_t* pRootElem = NULL;
    osXmlTagInfo_t* pXsdGlobalElemTagInfo = NULL;
	osList_t ctypeList = {};	//for xs:complexType
	osList_t* pSTypeList = oszalloc(sizeof(osList_t), NULL);	//for xs:simpleType
	
	if(!pXmlBuf)
	{
		logError("null pointer, pXmlBuf.");
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

	//parse <?xml version="1.0" encoding="UTF-8"?>
	if(osXml_parseFirstTag(pXmlBuf) != OS_STATUS_OK)
	{
		logError("fails to parse the xsd first line.");
		status = OS_ERROR_INVALID_VALUE;
		goto EXIT;
	}

	bool isSchemaTagDone = false;
    bool isEndSchemaTag = false;
	//parse <xs:schema xxxx>
	if(osXsd_parseSchemaTag(pXmlBuf, &isSchemaTagDone) != OS_STATUS_OK)
    {
        logError("fails to parse the xsd schema.");
		status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

	while(pXmlBuf->pos < pXmlBuf->end)
	{
		osXmlTagInfo_t* pGlobalElemTagInfo = NULL;
		status = osXsd_parseGlobalTag(pXmlBuf, &ctypeList, pSTypeList, &pGlobalElemTagInfo, &isEndSchemaTag);

		if(status != OS_STATUS_OK)
		{
			logError("fails to osXsd_parseGlobalTag, pos=%ld", pXmlBuf->pos);
	        status = OS_ERROR_INVALID_VALUE;
			goto EXIT;
		}

		//find a global element
		if(pGlobalElemTagInfo)
		{
			//only allow one global element, root element
			if(pXsdGlobalElemTagInfo)
			{
				logError("more than one global elements.");
				osfree(pGlobalElemTagInfo);
        		status = OS_ERROR_INVALID_VALUE;
				goto EXIT;
			}

			pXsdGlobalElemTagInfo = pGlobalElemTagInfo;
			if(pXsdGlobalElemTagInfo->isTagDone)
            {
                //for <xs:element name="A" type="B" />
				pRootElem = oszalloc(sizeof(osXsdElement_t), osXsdElement_cleanup);
				pRootElem->isRootElement = true;
                osXmlElement_getAttrInfo(&pXsdGlobalElemTagInfo->attrNVList, pRootElem);
            }
            else if(!pXsdGlobalElemTagInfo->isEndTag)
            {
                //for <xs:element name="A" type="B" ><xs:complexType name="C">...</xs:complexType></xs:element>
                pRootElem = osXsd_parseElement(pXmlBuf, pXsdGlobalElemTagInfo);
				pRootElem->isRootElement = true;
				pRootElem->pSimpleTypeList = pSTypeList;
                if(!pRootElem)
                {
                    logError("fails to osXsd_parseElement for root element, pos=%ld.", pXmlBuf->pos);
	                status = OS_ERROR_INVALID_VALUE;
                    goto EXIT;
                }
			}
		}
		
		if(isEndSchemaTag)
		{
			mlogInfo(LM_XMLP, "xsd parse is completed.");
			break;
		}
	}

	if(!pRootElem)
	{
		logError("fails to osXsd_parseGlobalTag, pRootElem=NULL.");
		goto EXIT;
	}

//tempPrint(&ctypeList, 1);
	/* link the root element with the child complex type.  Since the root element can only have one type=xxx, There can 
     * only have one child complex type.  The child complex type may have multiple its own children though 
     * */
	status = osXsd_elemLinkChild(pRootElem, &ctypeList, pSTypeList);
//tempPrint(&ctypeList, 2);

EXIT:
	osfree(pXsdGlobalElemTagInfo);

	if(status == OS_STATUS_OK)
	{
        //delete the cTypeList data structure, the cType data will be kept permanently as part of the xsd tree
        osList_clear(&ctypeList);
    }
	else
	{
		logError("status != OS_STATUS_OK, delete pRootElem.");
		osList_delete(&ctypeList);
		osfree(pRootElem);
		pRootElem = NULL;
		osList_free(pSTypeList);
	}

	return pRootElem;
}


/* finds the xsdElem node and notify the value to the user via callback.  This is useful when XML does not configure a element, the same
 * element in the XSD file shall be used as the default value and notify user
 *
 * pXsdElem: IN, an XSD element
 * callbackInfo: INOUT, the XSD value will be set as one of the parameters in the data structure.  Inside this function, it is passed as an
 *               input to the osXsd_elemCallback, which in turn as an input to xmlCallback function
 */
static osStatus_e osXsd_browseNode(osXsdElement_t* pXsdElem, osXmlDataCallbackInfo_t* callbackInfo)
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
		goto EXIT;
	}

	if(pXsdElem->dataType == OS_XML_DATA_TYPE_COMPLEX)
	{
    	osXsd_ctPointer_t* pCTPointer = oszalloc(sizeof(osXsd_ctPointer_t), NULL);
    	pCTPointer->pCT = pXsdElem->pComplex;
    	pCTPointer->doneListIdx = 0;
    	status = osXsd_transverseCT(pCTPointer, callbackInfo);
    	osfree(pCTPointer);
	}

EXIT:
	return status;
}


/* parse a xml input based on xsd.  Expect the top element is also the xsd root element, i.e., the xml implements the whole xsd  
 * when parsing a element of a input xml, if the element is a complex type, and the complex type contains multiple other elements, if
 * some of the elements are missed in the input xml, this function will use the default defined values of the missed elements in the 
 * xsd via function osXsd_browseNode() if the complex type's dispisition=sequence/all and minOccur=0.  if isUseDefault=false, this function 
 * would not call osXsd_browseNode(). 
* 
 * Using the following xsd and xml as an example
 * xsd:
 * <xs:element name="root" type="tRoot" />
 * <xs:complexType name="tRoot">
 *     <xs:element name="layer1" type="tLayer1-1"/>
 *     <xs:element name="layer2" type="tLayer1-2"/>
 * </xs:complexType>
 * <xs:complexType name="tLayer1-1">
 *     <xs:element name="ip-addr" type="xs:string">
 *     </xs:element>
 *     <xs:element name="tLayer2-1"/>
 * </xs:complexType>
 * <xs:complexType name="tLayer1-2">
 *     <xs:element type="tLayer2-2">
 * </xs:complexType>
 * <xs:complexType name="tLayer2-1">
 *     <xs:element name="count" default="1" type="xs:int"/>
 *     <xs:element name="pm-name" type="xs:String">
 * </xs:complexType>
 * <xs:complexType name="tLayer2-2">
 *    <xs:element name="server-name" type="xs:string"/>
 * </xs:complexType>
 *
 * xml:
 * <root>
 *     <layer1>
 *         <ip-addr>1.2.3.4</ip-addr>
 *         <tLayer2-1>
 *             <pm-name>ims</pm-name>
 *         </tLayer2-1>
 *     </layer1>
 *     <layer2>
 *         <tLayer2-2>
 *             <server-name>super-server</server-name>
 *         </tLayer2-2>
 *     </layer2>
 * </root>
 *
 * pBuf:         IN, xml input buffer
 * pXsdRootElem: IN, the root element of the associated xsd
 * isUseDefault: IN, when true, the default value from xsd will be used if a complex type's disposition=sequence/all, =false, default value from xsd will not be checked
 * callbackInfo: OUT, callback data, the xml leaf value will be set there
 */
static osStatus_e osXml_parse(osMBuf_t* pBuf, osXsdElement_t* pXsdRootElem, osXmlDataCallbackInfo_t* callbackInfo)
{
    osStatus_e status = OS_STATUS_OK;
    osXmlTagInfo_t* pElemInfo = NULL;
    osList_t xsdElemPointerList = {};
	/* note on xsdElemPointerList using xml example above
	 * when parsing <root>, since it is !pElemInfo->isEndTag && !pElemInfo->isTagDone, "root" is pushed into xsdElemPointerList
	 * when parsing <layer1>, since it is !pElemInfo->isEndTag && !pElemInfo->isTagDone, "layer1" is pushed into xsdElemPointerList
	 * when parsing <ip-addr>, since it is !pElemInfo->isEndTag && !pElemInfo->isTagDone, "ip-addr" is pushed into xsdElemPointerList
	 * when parsing </ip-addr>, since it is pElemInfo->isTagDone, "ip-addr" is popped out of xsdElemPointerList.  since the datat type is xs:string, 
	 *      it is a leaf node, value is taken, and callback is called
	 * so on for <tLayer2-1>...</tLyer2-1>
	 * when parsing </layer1>, since it is pElemInfo->isTagDone, "layer1" is popped out of xsdElemPointerList.
	 * so on for <layer2> ... </layer2>
	 * when parsing </root>, "root is popped out of xsdElemPointerList, the xsdElemPointerList is empty, the parse is done
	 */  

    if(!pBuf || !pXsdRootElem)
    {
        logError("null pointer, pBuf=%p, pXsdRootElem=%p.", pBuf, pXsdRootElem);
        status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

	//parse <?xml version="1.0" encoding="UTF-8"?>
	if((status = osXml_parseFirstTag(pBuf)) != OS_STATUS_OK)
	{
		logError("xml parse the first line failure.");
		goto EXIT;
	}

    bool isProcessAnyElem = false;  //for anyElement handling, whether this function is processing the any element block
	osXsdElement_t* pPrevAnyElem = NULL;	//for anyElem handling
    osXsd_elemPointer_t* pXsdPointer = NULL;
    osXsd_elemPointer_t* pParentXsdPointer = NULL;

    osPointerLen_t value;
    size_t tagStartPos;
	int listIdx = -1;	//the sequence order of the current pElemInfo in the belonged complex type
	bool isXmlParseDone = false;
    while(pBuf->pos < pBuf->end)
    {
		//handling the trailing section to make sure there is no LWS content
		if(isXmlParseDone)
		{
			if(!OSXML_IS_LWS(pBuf->buf[pBuf->pos]))
			{
				logError("content in the trailing section, it shall not be allowed, we just ignore it. pos=%ld", pBuf->pos);
			}
			pBuf->pos++;
			continue;
		}

		//make sure when starting new osXml_parseTag(), old pElemInfo has been freed
		pElemInfo = osfree(pElemInfo);

        status = osXml_parseTag(pBuf, false, false, &pElemInfo, &tagStartPos);
		if(status != OS_STATUS_OK || !pElemInfo)
		{
			logError("fails to osXml_parseTag for xml, pos=%ld.", pBuf->pos);
			goto EXIT;
		}

        if(pElemInfo->isEndTag)
        {
			//case </element_name>
            osListElement_t* pLE = osList_popTail(&xsdElemPointerList);
            if(!pLE)
            {
                logError("xsdPointerList popTail is null, element(%r) close does not have matching open, pos=%ld.", &pElemInfo->tag, pBuf->pos);
                status = OS_ERROR_INVALID_VALUE;
                goto EXIT;
            }

            osXsdElement_t* pCurXsdElem = ((osXsd_elemPointer_t*)pLE->data)->pCurElem;
			//parentXsd changes, check the previous parentXsd's elemList to make sure all elements in the list are processed
			//would not apply to processAnyElem case.  Since in isProcessAnyElem == true case, the parentXsdPointer does not change 
			if(((osXsd_elemPointer_t*)pLE->data)->pParentXsdPointer != pParentXsdPointer)
			{
				mdebug(LM_XMLP, "case </%r>, old pParentXsdPointer=%p, old xsdElem=%r, new xsdElem=%r, new pParentXsdPointer=%p", &pElemInfo->tag, pParentXsdPointer, pParentXsdPointer ? (&pParentXsdPointer->pCurElem->elemName) : NULL, pCurXsdElem ? &pCurXsdElem->elemName : NULL, ((osXsd_elemPointer_t*)pLE->data)->pParentXsdPointer);
				//need to move one layer up.  before doing it, check whether there is any min=0, max=0 element, and whether there is mandatory element not in the xml
				osXmlComplexType_t* pParentCT = osXsdPointer_getCT(pParentXsdPointer);
				if(!pParentCT)
				{
					logError("Parent xsd element is not a Complex type.");
					osListElement_delete(pLE);
                	status = OS_ERROR_INVALID_VALUE;
                	goto EXIT;
            	}
				
				osXmlElemDispType_e elemDispType = pParentCT->elemDispType;
				int choiceElemCount = 0;
				for(int i=0; i<osList_getCount(&pParentCT->elemList); i++)
				{
					switch(elemDispType)
					{
						case OS_XML_ELEMENT_DISP_TYPE_ALL:
						{
							if(pParentXsdPointer->assignedChildIdx[i] == 0)
							{
	                            osXsdElement_t* pXsdElem = osList_getDataByIdx(&pParentCT->elemList, i);
    	                        if(!pXsdElem)
        	                    {
            	                    logError("elemList idx(%d) does not have xsd element data, this shall never happen.", i);
				                    osListElement_delete(pLE);
                	                goto EXIT;
                    	        }

								if(pXsdElem->minOccurs > 0)
								{
									logError("element(%r) minOccurs=%d, but xml does not have the corresponding data.", &pXsdElem->elemName, pXsdElem->minOccurs);
                                    osListElement_delete(pLE);
									status = OS_ERROR_INVALID_VALUE;
									goto EXIT;
								}
								else
								{
									//browse the info for xsd that is not configured in the xml file, inside the function, callback for the default value will be called  
									if(callbackInfo->isUseDefault)
									{
										status = osXsd_browseNode(pXsdElem, callbackInfo);
										if(status != OS_STATUS_OK)
										{
											osListElement_delete(pLE);
											goto EXIT;
										}
									}
								}
							}
							else
							{
								//the check of the maxOccurs is done in <element_name> section, do nothing here.
							}
							break;
						}
						case OS_XML_ELEMENT_DISP_TYPE_CHOICE:
						{
							if(pParentXsdPointer->assignedChildIdx[i] != 0)
							{
								choiceElemCount++;
							}

							if(choiceElemCount > 1)
							{
                                osXsdElement_t* pXsdElem = osList_getDataByIdx(&pParentCT->elemList, i);
                                if(!pXsdElem)
                                {
                                    logError("elemList idx(%d) does not have xsd element data, this shall never happen.", i);
                                    osListElement_delete(pLE);
                                    goto EXIT;
                                }

								logError("element(%r) belongs to a complex type that has choice disposition, but there is already other element present.", &pXsdElem->elemName);
                                osListElement_delete(pLE);
								status = OS_ERROR_INVALID_VALUE;
                                goto EXIT;
                            }
 							break;
						}
						case OS_XML_ELEMENT_DISP_TYPE_SEQUENCE:
							//shall already be taken care of in <element_name> section, no need to consider here.
							break;
					}
				}
						
				pParentXsdPointer = ((osXsd_elemPointer_t*)pLE->data)->pParentXsdPointer;

			}	//if(((osXsd_elemPointer_t*)pLE->data)->pParentXsdPointer != pParentXsdPointer)

            if(osPL_cmp(&pElemInfo->tag, &pCurXsdElem->elemName) != 0)
            {
                logError("element(%r) close does not match open(%r), pos=%ld.", &pElemInfo->tag, &pCurXsdElem->elemName, pBuf->pos);
				osListElement_delete(pLE);
                status = OS_ERROR_INVALID_VALUE;
                goto EXIT;
            }

            if(osXml_isXsdElemSimpleType(pCurXsdElem) || isProcessAnyElem)
            {
                value.l = tagStartPos - value.l;
			}

			//The data sanity check is performed by callback()
            status = osXml_xmlCallback(pCurXsdElem, &value, callbackInfo);

			//for any element, it is not part of xsd, and was dynamically allocated, need to free when it is not needed any more
			if(isProcessAnyElem)
			{
				pPrevAnyElem = NULL;
				//check if the anyElement processing is done. pCurXsdElem->isElementAny=true, pCurXsdElem->anyElem.isXmlAnyElem=true must have been set
				if(pCurXsdElem->anyElem.xmlAnyElem.isRootAnyElem)
				{
					isProcessAnyElem = false;
				}
				osfree(pCurXsdElem);
			}

			if(status != OS_STATUS_OK)
			{
				logError("fails to validate xml for element(%r).", &pCurXsdElem->elemName);
				osListElement_delete(pLE);
				goto EXIT;
            }

			//end of file check
			if(osPL_cmp(&pElemInfo->tag, &pXsdRootElem->elemName) == 0)
			{
				mlogInfo(LM_XMLP, "xml parse is completed.");
				isXmlParseDone = true; 
			}

            osListElement_delete(pLE);
        }
        else if(pElemInfo->isTagDone)
        {
            //case <element_name xxx />, for now we do not process attribute
			mdebug(LM_XMLP, "case <%r ... /%r>, do nothing.", &pElemInfo->tag, &pElemInfo->tag);
            if(pParentXsdPointer)
            {
                if((pXsdPointer->pCurElem = osXml_getChildXsdElemByTag(&pElemInfo->tag, pParentXsdPointer, NULL, &listIdx)) == NULL)
                {
                    logError("the xsd does not have matching tag(%r) does not match with xsd.", &pElemInfo->tag);
                    goto EXIT;
                }

                pParentXsdPointer->assignedChildIdx[listIdx] = true;
            }

            continue;
        }
        else
        {
			//case <element_name>
            pXsdPointer = oszalloc(sizeof(osXsd_elemPointer_t), NULL);
			pXsdPointer->pParentXsdPointer = pParentXsdPointer;
			mdebug(LM_XMLP, "case <%r>, pParentXsdPointer=%p, pParentXsdPointer.elem=%r", &pElemInfo->tag, pParentXsdPointer, pParentXsdPointer ? (&pParentXsdPointer->pCurElem->elemName) : NULL);

            if(!pParentXsdPointer)
            {
                pXsdPointer->pCurElem = pXsdRootElem;
				
                if(osPL_cmp(&pElemInfo->tag, &pXsdPointer->pCurElem->elemName) != 0)
                {
                    logError("the element name in xml (%r) does not match with in xsd (%r).", &pElemInfo->tag, &pXsdPointer->pCurElem->elemName);
					osfree(pXsdPointer);
                    status = OS_ERROR_INVALID_VALUE;
                    goto EXIT;
                }
            }
            else
            {
				osXmlElemDispType_e parentXsdDispType = OS_XML_ELEMENT_DISP_TYPE_ALL;

				//if this function is inside any element processing state, directly allocate the any element object
				if(isProcessAnyElem)
				{
					pXsdPointer->pCurElem = osXsd_createAnyElem(&pElemInfo->tag, false);
				}
				else
                {
					if((pXsdPointer->pCurElem = osXml_getChildXsdElemByTag(&pElemInfo->tag, pParentXsdPointer, &parentXsdDispType, &listIdx)) == NULL)
                	{
						//if a matching child element does not found, check if the parent xsd element contains any element
						//this happens for the first element going into the anyElement processing
						if(isExistXsdAnyElem(pParentXsdPointer))
						{
							pXsdPointer->pCurElem = osXsd_createAnyElem(&pElemInfo->tag, true);	//true here indicate it is the root anyElem, isRootAnyElem = true
							isProcessAnyElem = true;
						
							mdebug(LM_XMLP, "created a xml element for a processContents=\"lax\" tag(%r), set isProcessAnyElem=true.", &pElemInfo->tag);  
						}
						else
						{
                    		logError("the xsd does not have matching tag(%r) does not match with xsd.", &pElemInfo->tag);
                    		osfree(pXsdPointer);
                    		status = OS_ERROR_INVALID_VALUE;
                    		goto EXIT;
						}
					}
                }

				//if this is a <xs:any> element
				if(pXsdPointer->pCurElem->isElementAny)
				{
					mdebug(LM_XMLP, "pCurElem->isElementAny=true, isProcessAnyElem=%d, pPrevAnyElem=%p, pParentXsdPointer=%p.", isProcessAnyElem, pPrevAnyElem, pParentXsdPointer);

					if(pPrevAnyElem)
					{
						pPrevAnyElem->anyElem.xmlAnyElem.isLeaf = false;
					}
					pPrevAnyElem = pXsdPointer->pCurElem;
				}
				else
				{
					//check OS_XML_ELEMENT_DISP_TYPE_SEQUENCE case to make sure the element is ordered
					if(parentXsdDispType == OS_XML_ELEMENT_DISP_TYPE_SEQUENCE)
					{
						if(listIdx == pParentXsdPointer->curIdx)
						{
							//the idx pElemInfo equals to what the current element that pParentXsdPointer expects to process, do nothing
						}
						else if(listIdx > pParentXsdPointer->curIdx)
						{
							//there is gap between the pElemInfo and the current element that pParentXsdPointer expects to process, meaning, some elements
							//specified in the complex type of the parent element in xsd is missed in the xml
                        	osXmlComplexType_t* pParentCT = osXsdPointer_getCT(pParentXsdPointer);
                        	if(!pParentCT)
                        	{
                            	logError("Parent xsd element is not a Complex type.");
                    			osfree(pXsdPointer);
                            	status = OS_ERROR_INVALID_VALUE;
                            	goto EXIT;
                        	}

							//check the element in front of the pXsdPointer->pCurElem
							for(int i=pParentXsdPointer->curIdx+1; i<listIdx; i++)
							{
								if(pParentXsdPointer->assignedChildIdx[i] == 0)
								{
									osXsdElement_t* pXsdElem = osList_getDataByIdx(&pParentCT->elemList, i);
		                        	if(!pXsdElem)
        		                	{
                		            	logError("elemList idx(%d) does not have xsd element data, this shall never happen.", pParentXsdPointer->curIdx);
										osfree(pXsdPointer);
                        		    	goto EXIT;
                        			}

                        			if(pXsdElem->minOccurs == 0)
									{
                                    	//browse the info for xsd that is not configured in the xml file.  Inside the function, callback for the default value will be called
                                    	if(callbackInfo->isUseDefault)
										{
											status = osXsd_browseNode(pXsdElem, callbackInfo);
											if(status != OS_STATUS_OK)
											{
												osfree(pXsdPointer);
												goto EXIT;
											}
										}
									}
									else
									{
		                            	logError("curidx=%d, element(%r) minOccurs=%d, and the parent CT is sequence disposition type, but the element does not show up before the current idx(%d).", i, &pXsdElem->elemName, pXsdElem->minOccurs, listIdx);
										osfree(pXsdPointer);
        		                    	status = OS_ERROR_INVALID_VALUE;
                		            	goto EXIT;
                        			}
								}
							}
							pParentXsdPointer->curIdx = listIdx;
						}
						else
						{
							logError("get a listIdx(%d) that is smaller than the current idx(%d), and the parent CT is sequence ddisposition type.", listIdx, pParentXsdPointer->curIdx);
							osfree(pXsdPointer);
                        	status = OS_ERROR_INVALID_VALUE;
                        	goto EXIT;
						}
					} //if(parentXsdDispType == OS_XML_ELEMENT_DISP_TYPE_SEQUENCE)

					//check the maximum element occurrence is not exceeded
					++pParentXsdPointer->assignedChildIdx[listIdx];
					mdebug(LM_XMLP, "pParentXsdPointer->assignedChildIdx[%d]=%d", listIdx, pParentXsdPointer->assignedChildIdx[listIdx]);
					if(pXsdPointer->pCurElem->maxOccurs != -1 && pParentXsdPointer->assignedChildIdx[listIdx] > pXsdPointer->pCurElem->maxOccurs)
					{
						logError("the element(%r) exceeds the maxOccurs(%d).", &pXsdPointer->pCurElem->elemName, pXsdPointer->pCurElem->maxOccurs);
						status = OS_ERROR_INVALID_VALUE;
						osfree(pXsdPointer);
						goto EXIT;
					}
            	} //if(pXsdPointer->pCurElem->isElementAny)
            } //if(!pParentXsdPointer)

			//for a <xs:any> element, do not move pParentXsdPointer since we do not have xsd, so treate it as a leaf node.  
			//whether it is a true leaf element will be determined when processing </xxx>
	        if(osXml_isXsdElemSimpleType(pXsdPointer->pCurElem) || pXsdPointer->pCurElem->isElementAny)
    	    {
				//set value.l to memorize the beginning of value, to be used when parsing </xxx>. 
           	    value.p = &pBuf->buf[pBuf->pos];
               	value.l = pBuf->pos;
           	}
			else
			{
				pParentXsdPointer = pXsdPointer;
				mdebug(LM_XMLP, "case <%r>, new pParentXsdPointer=%p, new pParentXsdPointer.elem=%r", &pElemInfo->tag, pParentXsdPointer, pParentXsdPointer ? (&pParentXsdPointer->pCurElem->elemName) : NULL);
			}

            osList_append(&xsdElemPointerList, pXsdPointer);
        } //if(pElemInfo->isEndTag)
    } //while(pBuf->pos < pBuf->end)

EXIT:
	osList_delete(&xsdElemPointerList);
	if(pElemInfo)
	{
        osfree(pElemInfo);
	}
    return status;
}	//osXml_parse


bool osXml_isXsdValid(osMBuf_t* pXsdBuf)
{
	if(!pXsdBuf)
	{
		logError("null pointer, pXsdBuf.");
		return false;
	}

    osXsdElement_t* pXsdRoot = osXsd_parse(pXsdBuf);
    if(!pXsdRoot)
    {
        logError("fails to osXsd_parse for xsdMBuf, pos=%ld.", pXsdBuf->pos);
        return false;
	}

    osfree(pXsdRoot);
	return true;
}


bool osXml_isXmlValid(osMBuf_t* pXmlBuf, osMBuf_t* pXsdBuf, osXmlDataCallbackInfo_t* callbackInfo)
{
    if(!pXsdBuf || !pXmlBuf)
    {
        logError("null pointer, pXmlBuf=%p, pXsdBuf=%p.", pXmlBuf, pXsdBuf);
        return false;
    }

    osXsdElement_t* pXsdRoot = osXsd_parse(pXsdBuf);
    if(!pXsdRoot)
    {
        logError("fails to osXsd_parse for xsdMBuf, pos=%ld.", pXsdBuf->pos);
        return false;
    }

    if(osXml_parse(pXmlBuf, pXsdRoot, callbackInfo) != OS_STATUS_OK)
	{
		logError("fails to osXml_parse(), xmlBuf pos=%ld.", pXmlBuf->pos);
		return false;
	}

	osfree(pXsdRoot);
	return true;
}



/* get xml leaf node value based on the xsd and xml files
 * fileFolder:   IN, the folder stores both the xsd and xml files
 * xsdFileName:  IN, the xsd file
 * xmlFileName:  IN, the xml file
 * isUseDefault: IN, when true, the default value from xsd will be used if a complex type's disposition=sequence/all, =false, default value from xsd will not be checked
 * callbackInfo: OUT, callback data, the xml leaf value will be set there
 */
osStatus_e osXml_getLeafValue(char* fileFolder, char* xsdFileName, char* xmlFileName, osXmlDataCallbackInfo_t* callbackInfo)
{
    osStatus_e status = OS_STATUS_OK;
    osMBuf_t* xsdMBuf = NULL;
    osMBuf_t* xmlBuf = NULL;
    osXsdElement_t* pXsdRoot = NULL;

    if(!xsdFileName || !xmlFileName)
    {
        logError("null pointer, xsdFileName=%p, xmlFileName=%p.", xsdFileName, xmlFileName);
        status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

    char xsdFile[OS_XML_MAX_FILE_NAME_SIZE];
    char xmlFile[OS_XML_MAX_FILE_NAME_SIZE];

    if(snprintf(xsdFile, OS_XML_MAX_FILE_NAME_SIZE, "%s/%s", fileFolder ? fileFolder : ".", xsdFileName) >= OS_XML_MAX_FILE_NAME_SIZE)
    {
        logError("xsdFile name is truncated.");
        status = OS_ERROR_INVALID_VALUE;
    }

    if(snprintf(xmlFile, OS_XML_MAX_FILE_NAME_SIZE, "%s/%s", fileFolder ? fileFolder : ".", xmlFileName) >= OS_XML_MAX_FILE_NAME_SIZE)
    {
        logError("xmlFile name is truncated.");
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

    xsdMBuf = osMBuf_readFile(xsdFile, 8000);
    if(!xsdMBuf)
    {
        logError("read xsdMBuf fails.");
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

	logInfo("start xsd parse for %s.", xsdFileName);
    pXsdRoot = osXsd_parse(xsdMBuf);
    if(!pXsdRoot)
    {
        logError("fails to osXsd_parse for xsdMBuf.");
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }
	logInfo("xsd parse for %s is done.", xsdFileName);

	//8000 is the initial mBuf size.  If the reading needs more than 8000, the function will realloc new memory
    xmlBuf = osMBuf_readFile(xmlFile, 8000);
    if(!xmlBuf)
    {
        logError("read xmlBuf fails.");
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

	logInfo("start xml parse for %s.", xmlFileName);
    status = osXml_parse(xmlBuf, pXsdRoot, callbackInfo);
	if(status != OS_STATUS_OK)
	{
		logError("xml parse for %s failed.", xmlFileName);
		goto EXIT;
	}

	logInfo("xml parse for %s is done.", xmlFileName);

EXIT:
    osfree(pXsdRoot);

    if(status != OS_STATUS_OK)
    {
        osMBuf_dealloc(xsdMBuf);
        osMBuf_dealloc(xmlBuf);
    }

    return status;
}


static osStatus_e osXml_xmlCallback(osXsdElement_t* pElement, osPointerLen_t* value, osXmlDataCallbackInfo_t* callbackInfo)
{
	osStatus_e status = OS_STATUS_OK;
	bool isLeaf = true;
	bool isLeafOnly = true;		//this parameter will be promoted as a function argument

	if(!pElement || !value || !callbackInfo)
	{
		logError("null pointer, pElement=%p, value=%p, callbackInfo=%p.", pElement, value, callbackInfo);
		return OS_ERROR_NULL_POINTER;
	}

	//check if is leaf
	if(!osXml_isXsdElemSimpleType(pElement) && !(pElement->isElementAny && pElement->anyElem.xmlAnyElem.isLeaf))
	{
	    //for isLeafOnly, only forward value to user when the element is a leaf
		if(isLeafOnly)
		{
			return status;
		}
		else
		{
			isLeaf = false;
		}
	}

    for(int i=0; i<callbackInfo->maxXmlDataSize; i++)
    {
        //that requires within xmlData, the dataName has to be sorted from shortest to longest
        if(pElement->elemName.l < callbackInfo->xmlData[i].dataName.l)
        {
            mdebug(LM_XMLP, "the app does not need element(%r), value=%r, ignore.", &pElement->elemName, value);
            return status;
        }

        //find the matching data name
        if(osPL_cmp(&pElement->elemName, &callbackInfo->xmlData[i].dataName) == 0)
        {
			if(isLeaf)
			{
				if(pElement->isElementAny)
				{
					pElement->dataType = OS_XML_DATA_TYPE_XS_STRING;
				}
				else
				{
					//check the dataType
            		if(pElement->dataType != callbackInfo->xmlData[i].dataType)
            		{
                		logError("the element(%r) data type=%d, but the user expects data type=%d", &pElement->elemName, pElement->dataType, callbackInfo->xmlData[i].dataType);
                		return OS_ERROR_INVALID_VALUE;
            		}
				}

    			switch(pElement->dataType)
    			{
        			case OS_XML_DATA_TYPE_XS_BOOLEAN:
        			case OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE:
        			case OS_XML_DATA_TYPE_XS_SHORT:
        			case OS_XML_DATA_TYPE_XS_INTEGER:
        			case OS_XML_DATA_TYPE_XS_LONG:
        			case OS_XML_DATA_TYPE_XS_STRING:
                		status = osXmlXSType_convertData(&pElement->elemName, value, pElement->dataType, &callbackInfo->xmlData[i]);
            			break;
        			case OS_XML_DATA_TYPE_SIMPLE: 
						status = osXmlSimpleType_convertData(pElement->pSimple, value, &callbackInfo->xmlData[i]);
						break;
					default: 
                    	logError("unexpected data type(%d) for element(%r).", pElement->dataType, &pElement->elemName);
                    	return OS_ERROR_INVALID_VALUE;
                    	break;
            	}
			}

			if(callbackInfo->xmlCallback)
			{
				callbackInfo->xmlCallback(&callbackInfo->xmlData[i]);
				//assign back to the original user expected data type, since callbackInfo->xmlData[i] may be reused if the xml use the same data type multiple times
				//notice for pElement->isElementAny case, the pElement->dataType is changed, but we do not compare dataType for this case, so do not care
				callbackInfo->xmlData[i].dataType = pElement->dataType;
			}
            break;
        }
    } //for(int i=0; i<callbackInfo->maxXmlDataSize; i++)

	return status;
}


osStatus_e osXmlXSType_convertData(osPointerLen_t* elemName, osPointerLen_t* value, osXmlDataType_e dataType, osXmlData_t* pXmlData)
{
    osStatus_e status = OS_STATUS_OK;

    if(!elemName || !value || !pXmlData)
    {
        logError("null pointer, elemName=%p, value=%p, pXmlData=%p", elemName, value, pXmlData);
        return OS_ERROR_NULL_POINTER;;
    }

    switch (dataType)
    {
		//boolean can take the form of 1, 0, "true", "false"
        case OS_XML_DATA_TYPE_XS_BOOLEAN:
			if(value->l == 1)
			{
				pXmlData->xmlIsTrue = value->p[0] == 0x31 ? true : false;
			}
			else
			{
            	pXmlData->xmlIsTrue = strncmp("true", value->p, value->l) == 0 ? true : false;
			}
            mlogInfo(LM_XMLP, "xmlData.dataName = %r, value=%s", elemName, pXmlData->xmlIsTrue ? "true" : "false");
            break;
		case OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE:
        case OS_XML_DATA_TYPE_XS_SHORT:
        case OS_XML_DATA_TYPE_XS_INTEGER:
        case OS_XML_DATA_TYPE_XS_LONG:
            status = osPL_convertStr2u64(value, &pXmlData->xmlInt, NULL);
            if(status != OS_STATUS_OK)
            {
                logError("falis to convert element(%r) value(%r).", elemName, value);
                return OS_ERROR_INVALID_VALUE;
            }
            mlogInfo(LM_XMLP, "xmlData.dataName = %r, value=%ld", elemName, pXmlData->xmlInt);
            break;
        case OS_XML_DATA_TYPE_XS_STRING:
            pXmlData->xmlStr = *value;
            mlogInfo(LM_XMLP, "xmlData.dataName =%r, value= %r", elemName, &pXmlData->xmlStr);
            break;
        default:
            logError("unexpected data type(%d) for element(%r).", dataType, elemName);
            return OS_ERROR_INVALID_VALUE;
            break;
    }

    return status;
}


/* link the parent and child complex type to make a tree.  When this function is called, all global complexType and simpleType shall have been resolved.
 * The first call of this function shall have pParentElem as the xsd root element.
 * pParentElem: IN, the parent element
 * pCTypeList:  IN, a list of osXmlComplexType_t entries
 * pSTypeList:  IN, a list of osXmlSimpleType_t entries
 */
static osStatus_e osXsd_elemLinkChild(osXsdElement_t* pParentElem, osList_t* pCTypeList, osList_t* pSTypeList)
{
	DEBUG_BEGIN
	osStatus_e status = OS_STATUS_OK;

	if(!pParentElem || !pCTypeList)
	{
		logError("null pointyer, pParentElem=%p, pCTypeList=%p.", pParentElem, pCTypeList);
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

	//check if it is a xs:any element
	if(pParentElem->isElementAny)
	{
        mdebug(LM_XMLP, "a xs:any element. no further process is needed, ignore.");
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
	DEBUG_END
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
	switch(pTagInfo->tag.l)
	{
		case OS_XML_COMPLEXTYPE_LEN:        //len = 14, "xs:complexType"
        {
			if(pTagInfo->tag.p[3] != 'c' || strncmp("xs:complexType", pTagInfo->tag.p, pTagInfo->tag.l) != 0)
			{
				mlogInfo(LM_XMLP, "top tag(%r) len=14, but is not xs:complexType, ignore.", &pTagInfo->tag);
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
		case OS_XML_SIMPLETYPE_LEN:			//13, "xs:simpleType"
		{
			if(pTagInfo->tag.p[3] != 's' || strncmp("xs:simpleType", pTagInfo->tag.p, pTagInfo->tag.l) != 0)
			{
				mlogInfo(LM_XMLP, "top tag(%r) len=13, but is not xs:simpleType, ignore.", &pTagInfo->tag);
                break;
            }

            mdebug(LM_XMLP, "decode: xs:simpleType, pXmlBuf->buf[pXmlBuf->pos]=0x%x, pXmlBuf->pos=%ld", pXmlBuf->buf[pXmlBuf->pos], pXmlBuf->pos);
            osXmlSimpleType_t* pSimpleInfo = osXsdSimpleType_parse(pXmlBuf, pTagInfo, NULL);
            if(!pSimpleInfo || !pSimpleInfo->typeName.l)
            {
                logError("simple global type is NULL or has no typeName, pSimpleInfo=%p, typeName.l=%ld.", pSimpleInfo, pSimpleInfo ? pSimpleInfo->typeName.l : 0);

int i=100/pSimpleInfo->typeName.l;
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
		case OS_XML_ELEMENT_LEN:            //10, "xs:element"
			if(strncmp("xs:element", pTagInfo->tag.p, pTagInfo->tag.l) != 0)
			{
				mlogInfo(LM_XMLP, "top tag(%r) len=10, but is not xs:element, ignore.", &pTagInfo->tag);
                goto EXIT;
            }

			mdebug(LM_XMLP, "decode: xs:element");

			*pGlobalElemTagInfo = pTagInfo;
        	break;
		case OS_XML_SCHEMA_LEN:				//9, "xs:schema"
			if(strncmp("xs:schema", pTagInfo->tag.p, pTagInfo->tag.l) != 0)
            {
                mlogInfo(LM_XMLP, "top tag(%r) len=9, but is not xs:schema, ignore.", &pTagInfo->tag);
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


//parse <?xml version="1.0" encoding="UTF-8"?>
static osStatus_e osXml_parseFirstTag(osMBuf_t* pXmlBuf)
{
	osStatus_e status = OS_STATUS_OK;
    osXmlTagInfo_t* pTagInfo = NULL;

	if(!pXmlBuf)
	{
		logError("null pointer, pXmlBuf.");
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

    //get tag info for the immediate next tag
    status = osXml_parseTag(pXmlBuf, false, true, &pTagInfo, NULL);
    if(status != OS_STATUS_OK)
    {
        logError("fails to osXml_parseTag for the first xsd line.");
        goto EXIT;
    }

	if(!pTagInfo->isTagDone)
	{
		logError("isTagDone = false for the first xsd line parsing.");
		status = OS_ERROR_INVALID_VALUE;
		goto EXIT;
	}

    if(!pTagInfo)
    {
        mlogInfo(LM_XMLP, "all tags are parsed.");
        goto EXIT;
    }

	if(strncmp("xml", pTagInfo->tag.p, pTagInfo->tag.l) != 0)
	{
		logError("the first line of xsd, expect tag xml, but instead, it is (%r).", &pTagInfo->tag);
		goto EXIT;
	}

	char* attrName[2] = {"version", "encoding"};
	for(int i=0; i<2; i++)
	{
		bool isMatch = false;
		osListElement_t* pLE = pTagInfo->attrNVList.head;
		while(pLE)
		{
			if(strncmp(attrName[i], ((osXmlNameValue_t*)pLE->data)->name.p, ((osXmlNameValue_t*)pLE->data)->name.l) == 0)
			{
				if(i==0)
				{
					if(strncmp("1.0", ((osXmlNameValue_t*)pLE->data)->value.p, ((osXmlNameValue_t*)pLE->data)->value.l) == 0)
					{
						isMatch = true;
					}
				}
				else if(i == 1)
                {
                    if(strncmp("UTF-8", ((osXmlNameValue_t*)pLE->data)->value.p, ((osXmlNameValue_t*)pLE->data)->value.l) == 0)
                    {
                        isMatch = true;
                    }
                }
				break;
			}

			pLE = pLE->next;
		}

		if(!isMatch)
		{
			logError("the xsd first line does not have attribute(%s) or proper attribute value(%s).", attrName[i], i==0? "1.0" : "UTF-8");
			status = OS_ERROR_INVALID_VALUE;
			goto EXIT;
		}
	}

EXIT:
	osfree(pTagInfo);

	return status;
}


//parse <xs:schema xxxx>
osStatus_e osXsd_parseSchemaTag(osMBuf_t* pXmlBuf, bool* isSchemaTagDone)
{
	osStatus_e status = OS_STATUS_OK;
    osXmlTagInfo_t* pTagInfo = NULL;

	if(!pXmlBuf)
	{
		logError("null pointer, pXmlBuf.");
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
        logError("isEndTag = false for the schema tag.");
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

    if(!pTagInfo)
    {
        mlogInfo(LM_XMLP, "pTagInfo = NULL.");
        goto EXIT;
    }

    *isSchemaTagDone = pTagInfo->isTagDone;

    if(strncmp("xs:schema", pTagInfo->tag.p, pTagInfo->tag.l) != 0)
    {
        logError("expect schema tag, but instead, it is (%r).", &pTagInfo->tag);
		status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

EXIT:
	osfree(pTagInfo);

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
				if(pTagInfo->tag.l != 10 || strncmp("xs:element", pTagInfo->tag.p, pTagInfo->tag.l))
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
            if(pTagInfo->tag.l == 14 && strncmp("xs:complexType", pTagInfo->tag.p, pTagInfo->tag.l) == 0)
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
                if(pTagInfo->tag.l != 10 || strncmp("xs:element", pTagInfo->tag.p, pTagInfo->tag.l))
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
            if(pTagInfo->tag.l == 14 && strncmp("xs:complexType", pTagInfo->tag.p, pTagInfo->tag.l) == 0)
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


/* this function parse a information inside quote <...> in XSD or XML
 * isTagNameChecked = true, parse starts after tag name, = false, parse starts before <
 * isTagDone == true, the tag is wrapped in one line, i.e., <tag, tag-content />
 * isEndTag == true, the line is the end of tag, i.e., </tag>
 *
 * pBug:             IN, a XML/XSD buffer
 * isTagNameChecked: IN, whether the parse shall start after the tag name, like <xs:element name="xxx...>, whether the chck shall start after "xs:element"
 * isXsdFirstTag:    IN, if this is to check the first XML/XSD line
 * ppTagInfo:        OUT, the parsed tag info for the line
 * tagStartPos:      OUT, the buffer position of '<'
 */
osStatus_e osXml_parseTag(osMBuf_t* pBuf, bool isTagNameChecked, bool isXsdFirstTag, osXmlTagInfo_t** ppTagInfo, size_t* tagStartPos) 
{
	osStatus_e status = OS_STATUS_OK;
    osXmlTagInfo_t* pTagInfo = NULL;

	if(!pBuf || !ppTagInfo)
	{
		logError("null pointer, pBuf=%p, ppTagInfo=%p.", pBuf, ppTagInfo);
		status = OS_ERROR_NULL_POINTER; 
		goto EXIT;
	}

    pTagInfo = oszalloc(sizeof(osXmlTagInfo_t), osXmlTagInfo_cleanup);

	if(isXsdFirstTag && pBuf->pos != 0)
	{
		mlogInfo(LM_XMLP, "isXsdFirstTag=true, but pBuf->pos != 0, force pBuf->pos to 0.");
		pBuf->pos = 0;
	}

	char slashChar = isXsdFirstTag ? '?' : '/';
	bool isTagDone = false;
	bool isEndTag = false;
	bool isGetTagInfoDone = false;
	osXmlNameValue_t* pnvPair = NULL;
	size_t tagPos = 0, nvStartPos = 0;
	osXsdCheckTagInfoState_e state = OS_XSD_TAG_INFO_START;
	if(isTagNameChecked)
	{
		//check tagInfo starts after the tag
		state = OS_XSD_TAG_INFO_CONTENT_NAME_START;
	}
	size_t origPos = pBuf->pos;
	bool commentIsInsideQuote = false;
	int commentCount = 0;
	while(pBuf->pos < pBuf->end)
	{
		switch(state)
		{
			case OS_XSD_TAG_INFO_START:
				//if(pBuf->buf[pBuf->pos] == 0x20 || pBuf->buf[pBuf->pos] == '\n' || pBuf->buf[pBuf->pos] == '\t')
				//{
				//	continue;
				//}
				if(isXsdFirstTag)
				{
					if(pBuf->buf[0] != '<' || pBuf->buf[1] != '?')
					{
						logError("isXsdFirstTag=true, but the xsd does not start with <?");
						status = OS_ERROR_INVALID_VALUE;
						goto EXIT;
					}

					pBuf->pos = 1;
                    state = OS_XSD_TAG_INFO_TAG_START;
                    tagPos = pBuf->pos+1;   //+1 means starts after '<'
				}
				else
				{
					if(pBuf->buf[pBuf->pos] == '<')
					{
						//remove the comment part
						if(OSXML_IS_COMMENT_START(&pBuf->buf[pBuf->pos]) && pBuf->pos+7 <= pBuf->end)	//comment is at least 7 char <!---->
						{
				 			pBuf->pos += 4;
							commentCount = 0;
							state = OS_XSD_TAG_INFO_TAG_COMMENT;
							continue;
						}
						if(tagStartPos)
						{
							*tagStartPos = pBuf->pos;
						}
						state = OS_XSD_TAG_INFO_TAG_START;
						tagPos = pBuf->pos+1;	//+1 means starts after '<'
					}
					else if(pBuf->buf[pBuf->pos] == '"')
					{
						state = OS_XSD_TAG_INFO_BEFORE_TAG_INSIDE_QUOTE;
					}
				}
				break;
			case OS_XSD_TAG_INFO_TAG_COMMENT:
				if(pBuf->buf[pBuf->pos] == '"')
				{
					commentIsInsideQuote = commentIsInsideQuote ? false : true;
				}
				//make sure the number of x >=0. <!--xxx-->, and the end pattern is not inside double quote
				if(commentCount >=3 && !commentIsInsideQuote && pBuf->buf[pBuf->pos] == '>')
				{
					if(OSXML_IS_COMMENT_STOP(&pBuf->buf[pBuf->pos]))
					{
						state = OS_XSD_TAG_INFO_START;
					}
				}
				commentCount++;
				pBuf->pos++;
				break;
			case OS_XSD_TAG_INFO_BEFORE_TAG_INSIDE_QUOTE:
				if(pBuf->buf[pBuf->pos] == '"')
                {
                    state = OS_XSD_TAG_INFO_START;
                }
                break;
			case OS_XSD_TAG_INFO_TAG_START:
				if(pBuf->buf[pBuf->pos] == 0x20 || pBuf->buf[pBuf->pos] == '\n' || pBuf->buf[pBuf->pos] == '\t')
				{
					logError("white space follows <, pos = %ld.", pBuf->pos);
					goto EXIT;
				}

				if(pBuf->buf[pBuf->pos] == '/')
                {
                    isEndTag = true;
					tagPos++;		//move back one space for tagPos
				}

				state = OS_XSD_TAG_INFO_TAG;
				break;
			case OS_XSD_TAG_INFO_TAG:
				if(pBuf->buf[pBuf->pos] == 0x20 || pBuf->buf[pBuf->pos] == '\n' || pBuf->buf[pBuf->pos] == '\t')
				{
					pTagInfo->tag.p = &pBuf->buf[tagPos];
					pTagInfo->tag.l = pBuf->pos - tagPos;
					
					state = OS_XSD_TAG_INFO_CONTENT_NAME_START;
				}
				else if(pBuf->buf[pBuf->pos] == slashChar)
                {
                    pTagInfo->tag.p = &pBuf->buf[tagPos];
                    pTagInfo->tag.l = pBuf->pos - tagPos;

                    state = OS_XSD_TAG_INFO_END_TAG_SLASH;
                }
				else if(pBuf->buf[pBuf->pos] == '>')
				{
				 	pTagInfo->tag.p = &pBuf->buf[tagPos];
                    pTagInfo->tag.l = pBuf->pos - tagPos;

                    isGetTagInfoDone = true;
				}
				else if(pBuf->buf[pBuf->pos] == '"')
				{
					logError("tag contains double quote, pos=%ld.", pBuf->pos);
					goto EXIT;
				}
				break;
			case OS_XSD_TAG_INFO_CONTENT_NAME_START:
				if(pBuf->buf[pBuf->pos] == '"')
				{
					logError("attribute starts with double quote, pos=%ld.", pBuf->pos);
					goto EXIT;
				}
                else if(pBuf->buf[pBuf->pos] == slashChar)
                {
                    state = OS_XSD_TAG_INFO_END_TAG_SLASH;
                }
                else if(pBuf->buf[pBuf->pos] == '>')
                {
                    isGetTagInfoDone = true;
                }
				else if(!OSXML_IS_LWS(pBuf->buf[pBuf->pos]))
                {
					pnvPair = oszalloc(sizeof(osXmlNameValue_t), NULL);
					pnvPair->name.p = &pBuf->buf[pBuf->pos];
					nvStartPos = pBuf->pos;

					//insert into pTagInfo->attrNVList
					osList_append(&pTagInfo->attrNVList, pnvPair);
					state = OS_XSD_TAG_INFO_CONTENT_NAME;
				}
				break;
			case OS_XSD_TAG_INFO_CONTENT_NAME:
				if(pBuf->buf[pBuf->pos] == '"')
                {
                    logError("xml tag attribute name contains double quote, pos=%ld.", pBuf->pos);
                    goto EXIT;
                }
				else if(OSXML_IS_LWS(pBuf->buf[pBuf->pos]) || pBuf->buf[pBuf->pos] == '=')
				{
					pnvPair->name.l = pBuf->pos - nvStartPos;
					mdebug(LM_XMLP, "pnvPair->name=%r, pos=%ld", &pnvPair->name, pBuf->pos);
					if(pBuf->buf[pBuf->pos] == '=')
					{
						 state = OS_XSD_TAG_INFO_CONTENT_VALUE_START;
					}
					else
					{
						state = OS_XSD_TAG_INFO_CONTENT_NAME_STOP;
					}
				}
				break;
			case OS_XSD_TAG_INFO_CONTENT_NAME_STOP:
				if(pBuf->buf[pBuf->pos] == '=')
				{
					state = OS_XSD_TAG_INFO_CONTENT_VALUE_START;
				}
				else if(!OSXML_IS_LWS(pBuf->buf[pBuf->pos]))
				{
					logError("expect =, but got char(%c), pos=%ld.", pBuf->buf[pBuf->pos], pBuf->pos);
					goto EXIT;
				}
				break;
			case OS_XSD_TAG_INFO_CONTENT_VALUE_START:
				if(pBuf->buf[pBuf->pos] == '"')
				{
                    pnvPair->value.p = &pBuf->buf[pBuf->pos+1];		//+1 to start after the current char "
                    nvStartPos = pBuf->pos + 1;						//+1 to start after the current char "

					state = OS_XSD_TAG_INFO_CONTENT_VALUE;
				}
                else if(!OSXML_IS_LWS(pBuf->buf[pBuf->pos]))
                {
                    logError("expect \", but got char(%c).", pBuf->buf[pBuf->pos]);
                    goto EXIT;
                }
                break;
			case OS_XSD_TAG_INFO_CONTENT_VALUE:
				if(pBuf->buf[pBuf->pos] == '"')
                {
					pnvPair->value.l = pBuf->pos - nvStartPos;
					state = OS_XSD_TAG_INFO_CONTENT_NAME_START;
					mdebug(LM_XMLP, "pnvPair->value=%r, pos=%ld", &pnvPair->value, pBuf->pos);
				}
				break;
            case OS_XSD_TAG_INFO_END_TAG_SLASH:
                if(pBuf->buf[pBuf->pos] == '>')
                {
                    isTagDone = true;
                    isGetTagInfoDone = true;
                }
                else
                {
                    logError("expect > after /, but instead, char=%c.", pBuf->buf[pBuf->pos]);
                    goto EXIT;
                }
				break;
			default:
				logError("unexpected tagInfo state(%d), this shall never happen.", state);
				goto EXIT;
				break;
		}

		pBuf->pos++;
		if(isGetTagInfoDone)
		{
			break;
		}
	}

EXIT:
    if(status != OS_STATUS_OK)
    {
    //to-do, cleanup memory if error case
        osfree(pTagInfo);
		pTagInfo = NULL;
    }
	else
	{
		if(pTagInfo)
		{
    		pTagInfo->isTagDone = isTagDone;
    		pTagInfo->isEndTag = isEndTag;
			mdebug(LM_XMLP, "tag=%r is parsed, isTagDone=%d, isEndTag=%d", &pTagInfo->tag, pTagInfo->isTagDone, pTagInfo->isEndTag);
		}
	}

	*ppTagInfo = pTagInfo;

	return status;
}


//the pattern can not contain char '"'
static bool osXml_findPattern(osMBuf_t* pXmlBuf, osPointerLen_t* pPattern, bool isAdvancePos)
{
	bool isPatternFound = false;

	if(!pXmlBuf || !pPattern)
	{
		logError("null pointer, pXmlBuf=%p, pPattern=%p", pXmlBuf, pPattern);
		goto EXIT;
	}

	if(!pPattern->l)
	{
		logError("pattern is empty.");
		goto EXIT;
	}

	int i=0;
	bool isInsideQuote = false;
	size_t origPos = pXmlBuf->pos;
	while(pXmlBuf->pos < pXmlBuf->end && (pXmlBuf->pos + pPattern->l) < pXmlBuf->end)
	{
		if(isInsideQuote)
		{
			if(pXmlBuf->buf[pXmlBuf->pos] == '"')
			{
				isInsideQuote = false;
			}

			continue;
		}
		else
		{
			if(pXmlBuf->buf[pXmlBuf->pos] == '"')
			{
				isInsideQuote = true;
				continue;
			}
		}

		if(pXmlBuf->buf[pXmlBuf->pos] != pPattern->p[i]);
		{
			++pXmlBuf->pos;
			++i;
			continue;
		}

		if(pPattern->l == 1)
		{
			isPatternFound = true;
			++pXmlBuf->pos;
			goto EXIT;
		}

		if(pXmlBuf->buf[pXmlBuf->pos+pPattern->l] == pPattern->p[pPattern->l-1])
		{
			if(pPattern->l == 2)
			{
				isPatternFound = true;
				++pXmlBuf->pos;
				goto EXIT;
			}

			if(strncmp(&pXmlBuf->buf[pXmlBuf->pos+1], &pPattern->p[1], pPattern->l-2) == 0)
			{
				isPatternFound = true;
				pXmlBuf->pos += pPattern->l;
               	goto EXIT;
           	}
		}

		++pXmlBuf->pos;
		++i;
	}

EXIT:
	if(!isAdvancePos)
	{
		pXmlBuf->pos = origPos;
	}

	return isPatternFound;
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

		

//the check starting pos must not within a double quote.
static bool osXml_findWhiteSpace(osMBuf_t* pBuf, bool isAdvancePos)
{
	bool isFound = false;

	if(!pBuf)
	{
		logError("null pointer, pBuf.");
		goto EXIT;
	}

	bool isInsideQuote = false;
	size_t origPos = pBuf->pos;
	for(; pBuf->pos < pBuf->size; pBuf->pos++)
	{
		//only check white space if out of inside double quote ""
		if(isInsideQuote)
		{
			if(pBuf->buf[pBuf->pos] == '"')
			{
				isInsideQuote = false;
			}
			continue;
		}

		switch(pBuf->buf[pBuf->pos])
		{
			case 0x20:
			case '\n':
			case 't':
				isFound = false;
				 pBuf->pos++;
				goto EXIT;
				break;
			case '"':
				isInsideQuote = isInsideQuote ? false : true;
				break;
			default:
				break;
		}
	}

EXIT:
	return isFound;
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


bool osXml_isXsdElemSimpleType(osXsdElement_t* pXsdElem)
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

	//for now, assume the only supported no XS data type is complex data type
	if(strncmp("xs:", typeValue->p, 3) != 0)
	{
		dataType = OS_XML_DATA_TYPE_NO_XS;
		goto EXIT;
	}

	switch(typeValue->l)
	{
        case 6:	//xs:int
            if(strncmp("xs:int", typeValue->p, typeValue->l) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_INTEGER;
                goto EXIT;
            }
            break;
        case 7:
            if(strncmp("xs:long", typeValue->p, typeValue->l) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_LONG;
				goto EXIT;
            }
            break;
		case 8:
            if(strncmp("xs:short", typeValue->p, typeValue->l) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_SHORT;
				goto EXIT;
            }
			break;
		case 9:		//xs:string, xs:anyURI
            if(typeValue->p[3] == 's' && strncmp("xs:string", typeValue->p, typeValue->l) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_STRING;
				goto EXIT;
            }
            else if (typeValue->p[3] == 'a' && strncmp("xs:anyURI", typeValue->p, typeValue->l) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_STRING;
				goto EXIT;
            }
			break;
		case 10:	//xs:integer, xs:boolean
			if(typeValue->p[9] == 'r' && strncmp("xs:integer", typeValue->p, typeValue->l) == 0)
			{
				dataType = OS_XML_DATA_TYPE_XS_INTEGER;
				goto EXIT;
			}
			else if(typeValue->p[9] == 'n' && strncmp("xs:boolean", typeValue->p, typeValue->l) == 0)
			{
				dataType = OS_XML_DATA_TYPE_XS_BOOLEAN;
				goto EXIT;
			}
			break;
        case 11:    //xs:dateTime
            if(typeValue->p[3] == 'd' && strncmp("xs:dateTime", typeValue->p, typeValue->l) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_STRING;
				goto EXIT;
            }
            break;

		case 15:	//xs:unsignedByte, xs:base64Binary
			if(typeValue->p[3] == 'u' && strncmp("xs:unsignedByte", typeValue->p, typeValue->l) == 0)
			{
				dataType = OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE;
				goto EXIT;
			}
            else if(typeValue->p[3] == 'b' && strncmp("xs:base64Binary", typeValue->p, typeValue->l) == 0)
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


bool osXml_isDigitType(osXmlDataType_e dataType)
{
	switch(dataType)
	{
    	case OS_XML_DATA_TYPE_XS_BOOLEAN:
    	case OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE:
    	case OS_XML_DATA_TYPE_XS_SHORT:
    	case OS_XML_DATA_TYPE_XS_INTEGER:
    	case OS_XML_DATA_TYPE_XS_LONG:
			return true;
			break;
		default:
			break;
	}

	return false;
}


bool osXml_isXSSimpleType(osXmlDataType_e dataType)
{
    switch(dataType)
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


/* get a xsd element(pXsdPointer)'s complex type info, include the order of the current xml element(pTag) in the complex type
 * pTag:               IN, the current element name in xml that is under parsing
 * pXsdPointer:        IN, the pTag's parent element in xsd.  Take the xml and xsd example in function osXml_parse() as an example, when parsing pTag="tLayer2-2", 
 *                     the pXsdPointer="layer2"
 * pParentXsdDispType: OUT, the disposition type of pXsdPointer, like all, sequence, etc.
 * listIdx:            OUT, the sequenence order of pTag in the complex type definition of pXsdPointer
 */ 
static osXsdElement_t* osXml_getChildXsdElemByTag(osPointerLen_t* pTag, osXsd_elemPointer_t* pXsdPointer, osXmlElemDispType_e* pParentXsdDispType, int* listIdx)
{
    osXsdElement_t* pChildXsdElem = NULL;

    if(!pTag || !pXsdPointer || !listIdx)
    {
        logError("null pointer, pTag=%p, pXsdPointer=%p, listIdx=%p.", pTag, pXsdPointer, listIdx);
        goto EXIT;
    }

    if(osXml_isXsdElemSimpleType(pXsdPointer->pCurElem))
    {
        mlogInfo(LM_XMLP, "the element(%r) is XS type, does not have child.", &pXsdPointer->pCurElem->elemName);
        goto EXIT;
    }

    osXmlComplexType_t* pCT = pXsdPointer->pCurElem->pComplex;
	if(pParentXsdDispType)
	{
		*pParentXsdDispType = pCT->elemDispType;
	}
    osListElement_t* pLE = pCT->elemList.head;
    *listIdx = -1;
    while(pLE)
    {
		(*listIdx)++;
        if(osPL_cmp(pTag, &((osXsdElement_t*)pLE->data)->elemName) == 0)
        {
            pChildXsdElem = pLE->data;
            break;
        }

        pLE = pLE->next;
    }

EXIT:
    return pChildXsdElem;
}



static osXmlComplexType_t* osXsdPointer_getCT(osXsd_elemPointer_t* pXsdPointer)
{
	osXmlComplexType_t* pCT = NULL;

	if(!pXsdPointer)
	{
		logError("null pointer, pXsdPointer.");
		goto EXIT;
	}

	if(osXml_isXsdElemSimpleType(pXsdPointer->pCurElem))
    {
        mlogInfo(LM_XMLP, "the element(%r) is a XS type.", &pXsdPointer->pCurElem->elemName);
        goto EXIT;
    }
		
	pCT = pXsdPointer->pCurElem->pComplex;

EXIT:
	return pCT;
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
        	status = osXml_xmlCallback(pXsdElem, &pXsdElem->fixed, callbackInfo);
        }
        else if(pXsdElem->elemDefault.p && pXsdElem->elemDefault.l > 0)
        {
        	status = osXml_xmlCallback(pXsdElem, &pXsdElem->elemDefault, callbackInfo);
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



static void osXmlTagInfo_cleanup(void* data)
{
	if(!data)
	{
		return;
	}

	osXmlTagInfo_t* pTagInfo = data;
	if(pTagInfo->isPElement)
	{
		//no need to free pTagInfo->pElement, as the pElement will be part of the rootElement tree
//		osfree(pTagInfo->pElement);
	}
	else
	{
		osList_delete(&pTagInfo->attrNVList);
	}
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
