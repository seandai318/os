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

#include "osXmlParser.h"


#define OS_XML_COMPLEXTYPE_LEN	14
#define OS_XML_ELEMENT_LEN 		10
#define OS_XML_SCHEMA_LEN		9



//help data structure for tag and namve-value pair list
typedef struct {
	osPointerLen_t tag;
    bool isTagDone; 				//the tag is wrapped in one line, i.e., <tag, tag-content />
    bool isEndTag;  				//the line is the end of tag, i.e., </tag>
	bool isPElement;				//if the union data is pElement, this value is true
	union {
		osList_t attrNVList;		//each list element contains osXmlNameValue_t
		osXsdElement_t* pElement;	//if tag is xs:element, and contains info other than tag attributes, this data structure will be used
	};
} osXmlTagInfo_t;


typedef struct osXmlNameValue {
    osPointerLen_t name;
    osPointerLen_t value;
} osXmlNameValue_t;


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


typedef struct ctPointer {
    osXmlComplexType_t* pCT;
    int doneListIdx;        //listIdx that has already transversed
} osXsd_ctPointer_t;



//static osXsdElement_t* osXsd_getRootElemInfo(osMBuf_t* pXmlBuf);
static osStatus_e osXsd_elemLinkChild(osXsdElement_t* pParentElem, osList_t* pTypeList);
static osStatus_e osXsd_parseGlobalTag(osMBuf_t* pXmlBuf, osList_t* pTypeList, osXmlTagInfo_t** pGlobalElemTagInfo, bool* isEndSchemaTag);
static osXmlComplexType_t* osXsdComplexType_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pCtTagInfo, osXsdElement_t* pParentElem);
static osStatus_e osXsdComplexType_getAttrInfo(osList_t* pAttrList, osXmlComplexType_t* pCtInfo);
static osStatus_e osXsdComplexType_getSubTagInfo(osXmlComplexType_t* pCtInfo, osXmlTagInfo_t* pTagInfo);
static osXmlComplexType_t* osXsd_getCTByname(osList_t* pTypeList, osPointerLen_t* pElemTypeName);
static osStatus_e osXml_parseFirstTag(osMBuf_t* pBuf);
static osStatus_e osXsd_parseSchemaTag(osMBuf_t* pXmlBuf, bool* isSchemaTagDone);
static osXmlDataType_e osXsd_getElemDataType(osPointerLen_t* typeValue);
static void osXsdElement_cleanup(void* data);
static void osXsd_transverseCT(osXsd_ctPointer_t* pCTPointer, osXsdElemCallback_h xsdCallback, osXmlDataCallback_h xmlCallback, osXmlDataCallbackInfo_t* callbackInfo);
static void osXsdElemCallback(osXsdElement_t* pXsdElem, osXmlDataCallback_h xmlCallback, osXmlDataCallbackInfo_t* callbackInfo);

static osStatus_e osXml_parseTag(osMBuf_t* pBuf, bool isTagNameChecked, bool isXsdFirstTag, osXmlTagInfo_t** ppTagInfo, size_t* tagStartPos);
static osXmlDataType_e osXml_getElementType(osPointerLen_t* pTypeValue);
static osXsdElement_t* osXmlElement_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pTagInfo);
static osStatus_e osXmlElement_getAttrInfo(osList_t* pAttrList, osXsdElement_t* pElement);
static osStatus_e osXmlElement_getSubTagInfo(osXsdElement_t* pElement, osXmlTagInfo_t* pTagInfo);
static bool osXml_findWhiteSpace(osMBuf_t* pBuf, bool isAdvancePos);
static bool osXml_findPattern(osMBuf_t* pXmlBuf, osPointerLen_t* pPattern, bool isAdvancePos);
static bool osXml_isXsdElemXSType(osXsdElement_t* pXsdElem);
static osXsdElement_t* osXml_getChildXsdElemByTag(osPointerLen_t* pTag, osXsd_elemPointer_t* pXsdPointer, osXmlElemDispType_e* pParentXsdDispType, int* listIdx);
static osXmlComplexType_t* osXsdPointer_getCT(osXsd_elemPointer_t* pXsdPointer);
static void osXmlTagInfo_cleanup(void* data);
static void osXmlComplexType_cleanup(void* data);



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
	osList_t typeList = {};
	
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
		status = osXsd_parseGlobalTag(pXmlBuf, &typeList, &pGlobalElemTagInfo, &isEndSchemaTag);

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
                pRootElem = osXmlElement_parse(pXmlBuf, pXsdGlobalElemTagInfo);
				pRootElem->isRootElement = true;
                if(!pRootElem)
                {
                    logError("fails to osXmlElement_parse for root element, pos=%ld.", pXmlBuf->pos);
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

//tempPrint(&typeList, 1);
	/* link the root element with the child complex type.  Since the root element can only have one type=xxx, There can 
     * only have one child complex type.  The child complex type may have multiple its own children though 
     * */
	osXsd_elemLinkChild(pRootElem, &typeList);
//tempPrint(&typeList, 2);

EXIT:
	osList_clear(&typeList);
	osfree(pXsdGlobalElemTagInfo);

	if(status != OS_STATUS_OK)
	{
		logError("status != OS_STATUS_OK, delete pRootElem.");
		osfree(pRootElem);
		pRootElem = NULL;
	}

	return pRootElem;
}


/* finds the xsdElem node and notify the value to the user via callback.  This is useful when XML does not configure a element, the same
 * element in the XSD file shall be used as the default value and notify user
 *
 * pXsdElem: IN, an XSD element
 * xsdCallback:  IN, XSD call back for a XSD element to provide XSD values.  Normally, osXsdElemCallback() will be used as the callback function
 * xmlCallback:  IN, XML call back function.  It is used as an input to xsdCallback() to provide the XSD values as the default values to user
 * callbackInfo: INOUT, the XSD value will be set as one of the parameters in the data structure.  Inside this function, it is passed as an
 *               input to the xsdCallback, which in turn as an input to xmlCallback function
 */
void osXsd_browseNode(osXsdElement_t* pXsdElem, osXsdElemCallback_h xsdCallback, osXmlDataCallback_h xmlCallback, osXmlDataCallbackInfo_t* callbackInfo)
{
	osStatus_e status = OS_STATUS_OK;

    if(!pXsdElem || !xsdCallback)
    {
        logError("null pointer, pXsdElem=%p, xsdCallback=%p.", pXsdElem, xsdCallback);
        status = OS_ERROR_NULL_POINTER;
		return;
    }

	//callback to provide info for the specified xsdElement.
	xsdCallback(pXsdElem, xmlCallback, callbackInfo);

	if(pXsdElem->dataType == OS_XML_DATA_TYPE_COMPLEX)
	{
    	osXsd_ctPointer_t* pCTPointer = oszalloc(sizeof(osXsd_ctPointer_t), NULL);
    	pCTPointer->pCT = pXsdElem->pComplex;
    	pCTPointer->doneListIdx = 0;
    	osXsd_transverseCT(pCTPointer, xsdCallback, xmlCallback, callbackInfo);
    	osfree(pCTPointer);
	}
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
 * callback:     IN, callback function for xml leaf node
 * callbackInfo: OUT, callback data, the xml leaf value will be set there
 */
osStatus_e osXml_parse(osMBuf_t* pBuf, osXsdElement_t* pXsdRootElem, osXmlDataCallback_h callback, osXmlDataCallbackInfo_t* callbackInfo)
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
									//browse the info for xsd that is not configured in the xml file, and do callback if required  
									osXsd_browseNode(pXsdElem, osXsdElemCallback, callback, callbackInfo);
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

            if(osXml_isXsdElemXSType(pCurXsdElem))
            {
                if(callback)
                {
                    value.l = tagStartPos - value.l;
                    callback(&pElemInfo->tag, &value, pCurXsdElem->dataType, callbackInfo);
                }
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
                if((pXsdPointer->pCurElem = osXml_getChildXsdElemByTag(&pElemInfo->tag, pParentXsdPointer, &parentXsdDispType, &listIdx)) == NULL)
                {
                    logError("the xsd does not have matching tag(%r) does not match with xsd.", &pElemInfo->tag);
                    osfree(pXsdPointer);
                    status = OS_ERROR_INVALID_VALUE;
                    goto EXIT;
                }

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

						//check the element in front of the curElem
						for(int i=pParentXsdPointer->curIdx; i<listIdx; i++)
						{
							if(pParentXsdPointer->assignedChildIdx[i] == 0)
							{
								osXsdElement_t* pXsdElem = osList_getDataByIdx(&pParentCT->elemList, pParentXsdPointer->curIdx);
		                        if(!pXsdElem)
        		                {
                		            logError("elemList idx(%d) does not have xsd element data, this shall never happen.", pParentXsdPointer->curIdx);
									osfree(pXsdPointer);
                        		    goto EXIT;
                        		}

                        		if(pXsdElem->minOccurs == 0)
								{
                                    //browse the info for xsd that is not configured in the xml file, and do callback if required
									osXsd_browseNode(pXsdElem, osXsdElemCallback, callback, callbackInfo);
								}
								else
								{
		                            logError("element(%r) minOccurs=%d, and the parent CT is sequence disposition type, but it does not show up before idx(%d).", &pXsdElem->elemName, pXsdElem->minOccurs, listIdx);
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
				}
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
            }

            if(osXml_isXsdElemXSType(pXsdPointer->pCurElem))
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
        }
    }

EXIT:
	osList_delete(&xsdElemPointerList);
	if(pElemInfo)
	{
        osfree(pElemInfo);
	}
    return status;
}	//osXml_parse



//get xml leaf node value based on the xsd and xml files
osStatus_e osXml_getLeafValue(char* fileFolder, char* xsdFileName, char* xmlFileName, osXmlDataCallback_h callback, osXmlDataCallbackInfo_t* callbackInfo)
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

    pXsdRoot = osXsd_parse(xsdMBuf);
    if(!pXsdRoot)
    {
        logError("fails to osXsd_parse for xsdMBuf.");
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

	//8000 is the initial mBuf size.  If the reading needs more than 8000, the function will realloc new memory
    xmlBuf = osMBuf_readFile(xmlFile, 8000);
    if(!xmlBuf)
    {
        logError("read xmlBuf fails.");
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

    osXml_parse(xmlBuf, pXsdRoot, callback == NULL ? osXml_xmlCallback : callback, callbackInfo);

EXIT:
    osfree(pXsdRoot);

    if(status != OS_STATUS_OK)
    {
        osMBuf_dealloc(xsdMBuf);
        osMBuf_dealloc(xmlBuf);
    }

    return status;
}


osStatus_e osXml_xmlCallback(osPointerLen_t* elemName, osPointerLen_t* value, osXmlDataType_e dataType, osXmlDataCallbackInfo_t* callbackInfo)
{
    osStatus_e status = OS_STATUS_OK;

    if(!elemName || !value || !callbackInfo)
    {
        logError("null pointer, elemName=%p, value=%p, callback=%p", elemName, value, callbackInfo);
        return OS_ERROR_NULL_POINTER;;
    }

    for(int i=0; i<callbackInfo->maxXmlDataSize; i++)
    {
        if(elemName->l < callbackInfo->xmlData[i].dataName.l)
        {
            mdebug(LM_XMLP, "the app does not need element(%r), ignore.", elemName);
            return status;
        }

        if(osPL_cmp(elemName, &callbackInfo->xmlData[i].dataName) == 0)
        {
            if(dataType != callbackInfo->xmlData[i].dataType)
            {
                logError("the element(%r) data type=%d, but the diaConfig expects data type=%d", elemName, dataType, callbackInfo->xmlData[i].dataType);
                return OS_ERROR_INVALID_VALUE;
            }

            switch (dataType)
            {
                case OS_XML_DATA_TYPE_XS_BOOLEAN:
                    callbackInfo->xmlData[i].xmlIsTrue = strncmp("true", value->p, value->l) == 0 ? true : false;
                    mlogInfo(LM_XMLP, "xmlData[%d].dataName = %r, value=%s", i, elemName, callbackInfo->xmlData[i].xmlIsTrue ? "true" : "false");
                    break;
                case OS_XML_DATA_TYPE_XS_SHORT:
                case OS_XML_DATA_TYPE_XS_INTEGER:
                case OS_XML_DATA_TYPE_XS_LONG:
                    status = osPL_convertStr2u64(value, &callbackInfo->xmlData[i].xmlInt);
                    if(status != OS_STATUS_OK)
                    {
                        logError("falis to convert element(%r) value(%r).", elemName, value);
                        return OS_ERROR_INVALID_VALUE;
                    }
                    mlogInfo(LM_XMLP, "xmlData[%d].dataName = %r, value=%ld", i, elemName, callbackInfo->xmlData[i].xmlInt);
                    break;
                case OS_XML_DATA_TYPE_XS_STRING:
                    callbackInfo->xmlData[i].xmlStr = *value;
                    mlogInfo(LM_XMLP, "xmlData[%d].dataName =%r, value= %r", i, elemName, &callbackInfo->xmlData[i].xmlStr);
                    break;
                default:
                    logError("unexpected data type(%d) for element(%r).", dataType, elemName);
                    return OS_ERROR_INVALID_VALUE;
                    break;
            }
            return status;
        }
    }

    return status;
}


/* link the parent and child complex type to make a tree
 * pParentElem: IN, the parent element
 * pTypeList:   IN, a list of osXmlComplexType_t entries
 */
static osStatus_e osXsd_elemLinkChild(osXsdElement_t* pParentElem, osList_t* pTypeList)
{
	osStatus_e status = OS_STATUS_OK;

	if(!pParentElem || !pTypeList)
	{
		logError("null pointyer, pParentElem=%p, pTypeList=%p.", pParentElem, pTypeList);
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

	switch(pParentElem->dataType)
	{
		case OS_XML_DATA_TYPE_COMPLEX:
		{
			pParentElem->pComplex = osXsd_getCTByname(pTypeList, &pParentElem->elemTypeName);
			if(!pParentElem->pComplex)
			{
				logError("does not find complex type definition for(%r).", &pParentElem->elemTypeName);
				status = OS_ERROR_INVALID_VALUE;
				goto EXIT;
			}
//tempPrint(pTypeList, 3);
		
			osListElement_t* pLE = pParentElem->pComplex->elemList.head;	
			while(pLE)
			//while((pLE = osList_popHead(&pParentElem->pComplex->elemList)) != NULL)
			{
				status = osXsd_elemLinkChild((osXsdElement_t*)pLE->data, pTypeList);
				if(status != OS_STATUS_OK)
				{
					logError("fails to osXsd_elemLinkChild for (%r).", pLE->data ? &((osXsdElement_t*)pLE->data)->elemName : NULL);
					goto EXIT;
				}
				pLE = pLE->next;
			}
			break;
		}
		case OS_XML_DATA_TYPE_SIMPLE:
			//to do
			break;
		default:
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
 * pTypeList:          INOUT, a osXmlComplexType_t data is added into the list when the Global tag is a complex type
 * pGlobalElemTagInfo: OUT, element info.  Since a XSD has only one element, this must be the root element
 * isEndSchemaTag:     OUT, if the line is a scheme end tag </xs:schema>
 */
static osStatus_e osXsd_parseGlobalTag(osMBuf_t* pXmlBuf, osList_t* pTypeList, osXmlTagInfo_t** pGlobalElemTagInfo, bool* isEndSchemaTag)
{
	osStatus_e status = OS_STATUS_OK;
    osList_t tagList = {};
    osXsdElement_t* pRootElement = NULL;
    osXmlTagInfo_t* pTagInfo = NULL;

    if(!pXmlBuf || !pGlobalElemTagInfo || !isEndSchemaTag)
    {
        logError("null pointer, pXmlBuf=%p, pGlobalElemTagInfo=%p, isEndSchemaTag=%p.", pXmlBuf, pGlobalElemTagInfo, isEndSchemaTag);
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
				logError("global type is NULL or has no typeName, pCtInfo=%p, typeName.l=%ld.", pCtInfo, pCtInfo ? pCtInfo->typeName.l : 0);
				if(pCtInfo)
				{
					osfree(pCtInfo);
				}
				status = OS_ERROR_INVALID_VALUE;
				break;
			}

            //insert the complex type into typeList so that the type can be linked in by elements
            osList_append(pTypeList, pCtInfo);
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
	
	
static osStatus_e osXsdComplexType_getSubTagInfo(osXmlComplexType_t* pCtInfo, osXmlTagInfo_t* pTagInfo)
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
		case 9:			//len=9, "xs:sequence"
			if(strncmp("xs:sequence", pTagInfo->tag.p, pTagInfo->tag.l) == 0)
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


static osXmlComplexType_t* osXsdComplexType_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pCtTagInfo, osXsdElement_t* pParentElem)
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
            	osXsdElement_t* pElem = osXmlElement_parse(pXmlBuf, pTagInfo);
                if(!pElem)
                {
                	logError("fails to osXmlElement_parse, pos=%ld.", pXmlBuf->pos);
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
static osXsdElement_t* osXmlElement_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pElemTagInfo)
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


static osStatus_e osXmlElement_getAttrInfo(osList_t* pAttrList, osXsdElement_t* pElement)
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
			case 'n':	//for "name"
				if(pNV->name.l == 4 && strncmp("name", pNV->name.p, pNV->name.l) == 0)		
				{
					pElement->elemName = pNV->value;
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
static osStatus_e osXml_parseTag(osMBuf_t* pBuf, bool isTagNameChecked, bool isXsdFirstTag, osXmlTagInfo_t** ppTagInfo, size_t* tagStartPos) 
{
	DEBUG_BEGIN
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

	DEBUG_END
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



static osXmlComplexType_t* osXsd_getCTByname(osList_t* pTypeList, osPointerLen_t* pElemTypeName)
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


static osXmlDataType_e osXml_getElementType(osPointerLen_t* pTypeValue)
{
	osXmlDataType_e dataType = OS_XML_DATA_TYPE_INVALID;

	if(!pTypeValue)
	{
		logError("null pointer, pTypeValue.");
		goto EXIT;
	}

	if(OSXML_IS_XS_TYPE(pTypeValue))
	{
		bool isIgnored = true;
		switch (pTypeValue->l)
		{
            case 7:
                if(strncmp("xs:long", pTypeValue->p, pTypeValue->l) == 0)
                {
                    dataType = OS_XML_DATA_TYPE_XS_LONG;
                    isIgnored = false;
                }
                break;
			case 8:
                if(strncmp("xs:short", pTypeValue->p, pTypeValue->l) == 0)
                {
                    dataType = OS_XML_DATA_TYPE_XS_SHORT;
                    isIgnored = false;
                }
                break;
			case 9:		//xs:string
				if(strncmp("xs:string", pTypeValue->p, pTypeValue->l) == 0)
				{
					dataType = OS_XML_DATA_TYPE_XS_STRING;
					isIgnored = false;
				}
				break;
			case 10:	//xs:integer, xs:boolean
				if(pTypeValue->p[3] == 'i' && strncmp("xs:integer", pTypeValue->p, pTypeValue->l) == 0)
				{
					dataType = OS_XML_DATA_TYPE_XS_INTEGER;
					isIgnored = false;
                }
				else if(pTypeValue->p[3] == 'b' && strncmp("xs:boolean", pTypeValue->p, pTypeValue->l) == 0)
                {
                    dataType = OS_XML_DATA_TYPE_XS_BOOLEAN;
                    isIgnored = false;
                }
				break;
			default:
				break;
		}

		mlogInfo(LM_XMLP, "attribute type(%r) is ignored.", pTypeValue);
	}
	else
	{
		dataType = OS_XML_DATA_TYPE_NO_XS;
	}

EXIT:
	return dataType;
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
        case OS_XML_DATA_TYPE_XS_BOOLEAN:
		case OS_XML_DATA_TYPE_XS_SHORT:
        case OS_XML_DATA_TYPE_XS_INTEGER:
		case OS_XML_DATA_TYPE_XS_LONG:
        case OS_XML_DATA_TYPE_XS_STRING:
            return true;
            break;
        default:
            break;
    }

    return false;
}


static osXmlDataType_e osXsd_getElemDataType(osPointerLen_t* typeValue)
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
		dataType = OS_XML_DATA_TYPE_COMPLEX;
		goto EXIT;
	}

	switch(typeValue->l)
	{
        case 7:
            if(strncmp("xs:long", typeValue->p, typeValue->l) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_LONG;
            }
            break;
		case 8:
            if(strncmp("xs:short", typeValue->p, typeValue->l) == 0)
            {
                dataType = OS_XML_DATA_TYPE_XS_SHORT;
            }
			break;
		case 9:		//xs:string
			if(strncmp("xs:string", typeValue->p, typeValue->l) != 0)
			{
				mlogInfo(LM_XMLP, "xml data type(%r) is not supported, ignore.", typeValue);
				goto EXIT;
			}
			dataType = OS_XML_DATA_TYPE_XS_STRING;
			break;
		case 10:	//xs:integer, xs:boolean
			if(typeValue->p[9] == 'r' && strncmp("xs:integer", typeValue->p, typeValue->l) == 0)
			{
				dataType = OS_XML_DATA_TYPE_XS_INTEGER;
			}
			else if(typeValue->p[9] == 'n' && strncmp("xs:boolean", typeValue->p, typeValue->l) == 0)
			{
				dataType = OS_XML_DATA_TYPE_XS_BOOLEAN;
			}
			else
			{
				mlogInfo(LM_XMLP, "xml data type(%r) is not supported, ignore.", typeValue);
            }
			break;
		default:
            mlogInfo(LM_XMLP, "xml data type(%r) is not supported, ignore.", typeValue);
			break;		
	}

EXIT:
	return dataType;
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

    if(osXml_isXsdElemXSType(pXsdPointer->pCurElem))
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


/* recursively going through a XSD complex type and call back all its leaf node value
 *
 * pCTPointer:   IN, a complex type pointer, contains the complex type node
 * xsdCallback:  IN, XSD call back for a XSD element to provide XSD values.  Normally, osXsdElemCallback() will be used as the callback function
 * xmlCallback:  IN, XML call back function.  It is used as an input to xsdCallback() to provide the XSD values as the default values to user
 * callbackInfo: INOUT, the XSD value will be set as one of the parameters in the data structure.  Inside this function, it is passed as an
 *               input to the xsdCallback, which in turn as an input to xmlCallback function
 */
static void osXsd_transverseCT(osXsd_ctPointer_t* pCTPointer, osXsdElemCallback_h xsdCallback, osXmlDataCallback_h xmlCallback, osXmlDataCallbackInfo_t* callbackInfo)
{
    if(!pCTPointer || !xsdCallback)
    {
        logError("null pointer, pCTPointer=%p, xsdCallback=%p.", pCTPointer, xsdCallback);
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

			xsdCallback(pChildElem, xmlCallback, callbackInfo);

			if(!osXml_isXsdElemXSType(pChildElem))
            {
                osXsd_ctPointer_t* pNextCTPointer = oszalloc(sizeof(osXsd_ctPointer_t), NULL);
                pNextCTPointer->pCT = pChildElem->pComplex;
                osXsd_transverseCT(pNextCTPointer, xsdCallback, xmlCallback, callbackInfo);
                osfree(pNextCTPointer);
            }
        }
        pLE = pLE->next;
    }

    return;
}


static osXmlComplexType_t* osXsdPointer_getCT(osXsd_elemPointer_t* pXsdPointer)
{
	osXmlComplexType_t* pCT = NULL;

	if(!pXsdPointer)
	{
		logError("null pointer, pXsdPointer.");
		goto EXIT;
	}

	if(osXml_isXsdElemXSType(pXsdPointer->pCurElem))
    {
        mlogInfo(LM_XMLP, "the element(%r) is a XS type.", &pXsdPointer->pCurElem->elemName);
        goto EXIT;
    }
		
	pCT = pXsdPointer->pCurElem->pComplex;

EXIT:
	return pCT;
}


static void osXsdElemCallback(osXsdElement_t* pXsdElem, osXmlDataCallback_h xmlCallback, osXmlDataCallbackInfo_t* callbackInfo)
{
	if(!pXsdElem)
	{
		logError("null pointer, pXsdElem.");
		return;
	}

	if(xmlCallback)
	{
		if(osXml_isXsdElemXSType(pXsdElem))
		{
        	if(pXsdElem->fixed.p && pXsdElem->fixed.l > 0)
            {
            	if(xmlCallback)
                {
                	xmlCallback(&pXsdElem->elemName, &pXsdElem->fixed, pXsdElem->dataType, callbackInfo);
                }
            }
            else if(pXsdElem->elemDefault.p && pXsdElem->elemDefault.l > 0)
            {
            	if(xmlCallback)
                {
                	xmlCallback(&pXsdElem->elemName, &pXsdElem->elemDefault, pXsdElem->dataType, callbackInfo);
                }
            }
		}
	}
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


static void osXmlComplexType_cleanup(void* data)
{
    if(!data)
    {
        return;
    }

	osXmlComplexType_t* pCT = data;
	osList_delete(&pCT->elemList);
}


static void osXsdElement_cleanup(void* data)
{
    if(!data)
    {
        return;
    }

	osXsdElement_t* pElement = data;
	if(pElement->dataType == OS_XML_DATA_TYPE_COMPLEX)
	{
		osfree(pElement->pComplex);
	}
} 
