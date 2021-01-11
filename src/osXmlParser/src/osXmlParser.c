/********************************************************
 * Copyright (C) 2020 Sean Dai
 *
 * @file osXmlParser.c
 * xml instance parser, support the following xml format:
 * xs:boolean, xs:short, xs:integer, xs:long, xs:string, xs:complex
 * validate the xml against xsd.  If a call back is provided, the leaf
 * information of all xml leave elements will be provided via the callback
 ********************************************************/

#include <string.h>
#include <stdlib.h>
#include <time.h>

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



//this data structure used to help xml parsing against its xsd
typedef struct {
	uint16_t choiceElemCount;   //the number of elements present in a choice block
    uint16_t choiceTag;         //the choice tag that identifies a choice group
} osXml_choiceInfo_t;

typedef struct {
	uint8_t childCount;			//how many times a child element appear in a CT
//	osXml_choiceInfo_t* pXmlChoiceInfo;	//only applicable if a element is within a choice block, all elements in the same choice block share the same choiceInfo
//	bool choiceElementUsed;		//only applicable if the child element is part of a choice, true if the child choice element shown in xml, false if not
//	uint16_t choiceTag;			//the choice tag that identifies a choice group
} osXml_assignedChildInfo_t;


typedef struct osXsd_elemPointer {
    osXsdElement_t* pCurElem;       //the current working xsd element
    struct osXsd_elemPointer* pParentXsdPointer;    //the parent xsd pointer
    int curIdx;                     //which idx in assignedChildIdx[] that the xsd element is current processing, used in for the ordering presence of sequence deposition
    osXml_assignedChildInfo_t  assignedChildIdx[OS_XSD_COMPLEX_TYPE_MAX_ALLOWED_CHILD_ELEM]; //if true, the list idx corresponding child element value has been assigned
	osList_t xmlChoiceList;			//each entry contains osXml_choiceInfo_t, repesents a choice block within a complex element
	osXml_nsInfo_t* pXmlnsInfo;	//would not use osXsd_schemaInfo_t.targetNS here
} osXsd_elemPointer_t;


typedef struct {
	osXsdElement_t* pXsdRootElem;	//the first element getting into <xs:any>.  used to determine the <xs:any> parsing is done when </rootElem> is met
	osXsdElement_t* pPrevAnyElem;	//used to determine xmlAnyElem.isLeaf. set to true when <element>, unset when </element>.  If processing a <element> and pPrevAnyElem=true, meaning <element><element>, the new element is not a leaf
} osXmlAnyElemStateInfo_t;


typedef struct {
    size_t tagStartPos;     //the pos where a tag like <element> or </element> starts, pointing to char '<', used to get the value between <element>value</element>
    size_t openTagEndPos;   //the pos where a <element> tag ends, pointing to char after '>', used to get the value between <element>value</element>
} osXmlTagPosInfo_t;

 	
typedef struct {
    bool isXmlParseDone;			//indicate if xml parsing is done
	osList_t gNSList;				//each entry contains osXml_nsInfo_t(pXmlnsInfo), or osXsd_nsAliasInfo_t(pnsAlias) that are allocated during a xml parsing for the purpose of freeing them when parsng is done
	osXml_nsInfo_t* pCurXmlInfo;	//the current pXmlnsInfo for a xml under parse
	osList_t xsdElemPointerList;	//xsdPointer stack.  Each <element>'s xsdPointer will be pushed into the stack, and pop out in </element> handling
    osXsd_elemPointer_t* pParentXsdPointer;
	osXmlTagPosInfo_t tagPosInfo;	// used to get the value between <element>value</element>
//    osPointerLen_t value;   		//element value <element>value</element>
    bool isProcessAnyElem;  		//for anyElement handling, is set when start to processing a <xs:any> root element, unset when the root element is processed
	osXmlAnyElemStateInfo_t anyElemStateInfo;	//used when isProcessAnyElem=true
	osPointerLen_t* xsdName;
    osXmlDataCallbackInfo_t* callbackInfo;
} osXml_parseStateInfo_t;


static bool isExistXsdAnyElem(osXsd_elemPointer_t* pXsdPointer);
static osXsdElement_t* osXml_getChildXsdElemByTag(osPointerLen_t* pTag, osXsd_elemPointer_t* pXsdPointer, osXmlElemDispType_e* pParentXsdDispType, int* listIdx);
static osXml_choiceInfo_t* osXml_getChoiceInfo(osXsd_elemPointer_t* pParentXsdPointer, uint32_t choiceTag);
static osXmlComplexType_t* osXsdPointer_getCT(osXsd_elemPointer_t* pXsdPointer);
static osStatus_e osXml_getNsInfo(osList_t* pAttrNVList, osXml_nsInfo_t** ppNsInfo, osList_t* pNoXmlnsAttrList, osList_t* pgNSList);
static bool osXml_isAliasExist(osPointerLen_t* pRootAlias, osList_t* pAliasList, osPointerLen_t** pRootNS);
static void osXml_updateNsInfo(osXml_nsInfo_t* pNewNsInfo, osXml_nsInfo_t* pXsdPointerXmlnsInfo);
static osListElement_t* osXml_isAliasMatch(osList_t* nsAliasList, osPointerLen_t* pnsAlias);
static void osXmlNsInfo_cleanup(void* data);
static void osXsd_elemPointer_cleanup(void* data);

static osStatus_e osXml_parseInternal(osMBuf_t* pBuf, osPointerLen_t* xsdName, osXmlDataCallbackInfo_t* callbackInfo);
static osStatus_e osXml_parseRootElem(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo);
static osStatus_e osXml_parseSOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo);	//Start Of Tag	<xs:aaa>
static osStatus_e osXml_parseEOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo);	//End Of Tag	</xs:aaa>
static osStatus_e osXml_parseDOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo);	//Done Of Tag <xs:aaa />
static osStatus_e osXml_parseAnyRootElem(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo);
static osStatus_e osXml_parseAnyElemSOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo);
static osStatus_e osXml_parseAnyElemDOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo);
static osStatus_e osXml_parseAnyElemEOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo);



/* pXmlBuf: IN, contains xml contents
 * pXsdBuf: IN, contains xsd contents, if NULL, thexsd has been parsed before this function call
 * xsdName: IN, the name of xsd file, like 3gppSsa.xsd, etc.  If the associated xsd has been parsed before, must matches with the xsd name used in the earlier xsd parse input
 * callbackInfo: IN, instruction to perform xml aprse
 */
osStatus_e osXml_parse(osMBuf_t* pXmlBuf, osMBuf_t* pXsdBuf, osPointerLen_t* xsdName, osXmlDataCallbackInfo_t* callbackInfo)
{
	if(!pXmlBuf || !xsdName)
	{
		logError("null pointer, pXmlBuf=%p, xsdName=%p.", pXmlBuf, xsdName);
		return OS_ERROR_NULL_POINTER;
	}

	if(pXsdBuf)
	{
		osXsdNamespace_t* pNS = osXsd_parse(pXsdBuf, xsdName);
    	if(!pNS)
    	{
        	logError("fails to osXsd_parse for xsdMBuf, pos=%ld.", pXsdBuf->pos);
        	return OS_ERROR_INVALID_VALUE;
    	}
	}
	
	return osXml_parseInternal(pXmlBuf, xsdName, callbackInfo);
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
static osStatus_e osXml_parseInternal(osMBuf_t* pBuf, osPointerLen_t* xsdName, osXmlDataCallbackInfo_t* callbackInfo)
{
    osStatus_e status = OS_STATUS_OK;
    osXmlTagInfo_t* pElemInfo = NULL;
	osXml_parseStateInfo_t stateInfo = {};
	stateInfo.xsdName = xsdName;
	stateInfo.callbackInfo = callbackInfo;

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
    if(!pBuf || !xsdName)
    {
        logError("null pointer, pBuf=%p, xsdName=%p.", pBuf, xsdName);
        status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

    //parse <?xml version="1.0" encoding="UTF-8"?>
    if((status = osXml_parseFirstTag(pBuf)) != OS_STATUS_OK)
    {
        logError("xml parse the first line failure.");
        goto EXIT;
    }

    status = osXml_parseTag(pBuf, false, false, &pElemInfo, &stateInfo.tagPosInfo.tagStartPos);
    if(status != OS_STATUS_OK || !pElemInfo)
    {
        logError("fails to osXml_parseTag for xml, pos=%ld.", pBuf->pos);
        goto EXIT;
    }

	status = osXml_parseRootElem(pBuf, pElemInfo, &stateInfo);
	if(status != OS_STATUS_OK)
	{
		logError("fails to osXml_parseRootElem.");
		goto EXIT;
	}
	
	//done with pElemInfo for the rootElem, free it.
	pElemInfo = osfree(pElemInfo);

    while(pBuf->pos < pBuf->end)
    {
        //handling the trailing section to make sure there is no LWS content
        if(stateInfo.isXmlParseDone)
        {
            if(!OSXML_IS_LWS(pBuf->buf[pBuf->pos]))
            {
                logError("content in the trailing section, it shall not be allowed, we just ignore it. pos=%ld", pBuf->pos);
            }
            pBuf->pos++;
            continue;
        }

        status = osXml_parseTag(pBuf, false, false, &pElemInfo, &stateInfo.tagPosInfo.tagStartPos);
        if(status != OS_STATUS_OK || !pElemInfo)
        {
            logError("fails to osXml_parseTag for xml, pos=%ld.", pBuf->pos);
            goto EXIT;
        }

		if(stateInfo.isProcessAnyElem)
		{
        	if(pElemInfo->isEndTag)
			{
				status =osXml_parseAnyElemEOT(pBuf, pElemInfo, &stateInfo);
			}
			else if(pElemInfo->isTagDone)
			{
				status = osXml_parseAnyElemDOT(pBuf, pElemInfo, &stateInfo);
			}
			else
			{
				status = osXml_parseAnyElemSOT(pBuf, pElemInfo, &stateInfo);
			}
		}
		else
		{
            if(pElemInfo->isEndTag)
            {
                status = osXml_parseEOT(pBuf, pElemInfo, &stateInfo);
            }
            else if(pElemInfo->isTagDone)
            {
                status = osXml_parseDOT(pBuf, pElemInfo, &stateInfo);
            }
            else
            {
                status = osXml_parseSOT(pBuf, pElemInfo, &stateInfo);
            }
        }

	    if(status != OS_STATUS_OK)
    	{
        	logError("fails to osXml_parse element(%r), isProcessAnyElem=%d, isEndTag=%d, isTagDone=%d.", &pElemInfo->tag, stateInfo.isProcessAnyElem, pElemInfo->isEndTag, pElemInfo->isTagDone);
        	goto EXIT;
    	}

		//done with pElemInfo for this tag, free it.
		pElemInfo = osfree(pElemInfo);
	}

EXIT:
    osList_delete(&stateInfo.xsdElemPointerList);
	osList_delete(&stateInfo.gNSList);
    if(pElemInfo)
    {
        osfree(pElemInfo);
    }
    return status;
} //osXml_parse()


/* process the root element. find the xmlns alias list, and the corresponding xsd rootElem */
static osStatus_e osXml_parseRootElem(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo)
{
	osStatus_e status = OS_STATUS_OK;
    osList_t noXmlnsAttrList={};

	osXsd_elemPointer_t* pXsdPointer = oszalloc(sizeof(osXsd_elemPointer_t), osXsd_elemPointer_cleanup);
    pXsdPointer->pParentXsdPointer = NULL;
    mdebug(LM_XMLP, "case <%r>, pParentXsdPointer=NULL", &pElemInfo->tag);

    //to find the rootElem from the right xsd
    osPointerLen_t rootAlias, rootElemName, *pRootNS = NULL;
    osXml_singleDelimitParse(&pElemInfo->tag, ':', &rootAlias, &rootElemName);

    //get the xmlns alias and attribute info based the parsed element tag (pElemInfo)
    status = osXml_getNsInfo(&pElemInfo->attrNVList, &pXsdPointer->pXmlnsInfo, &noXmlnsAttrList, &pStateInfo->gNSList);
    if(status != OS_STATUS_OK)
    {
    	logError("fails to osXml_getNsInfo() for element(%r).", &pElemInfo->tag);
        goto EXIT;
    }

    //sanity check the rootAlias and pXsdPointer->pXmlnsInfo
    if(osList_getCount(&pXsdPointer->pXmlnsInfo->nsAliasList))
    {
    	//if rootAlias exists, needs to match one in the pXsdPointer->pXmlnsInfo->nsAliasList
        if(rootAlias.l)
        {
        	if(!osXml_isAliasExist(&rootAlias, &pXsdPointer->pXmlnsInfo->nsAliasList, &pRootNS))
            {
            	logError("rootAlias(%r) is not defined.", &rootAlias);
                status = OS_ERROR_INVALID_VALUE;
                goto EXIT;
            }
        }
        else
        {
        	//the default NS does not exist, use the input xsd as the default one
            if(!pXsdPointer->pXmlnsInfo->defaultNS.l)
            {
            	pXsdPointer->pXmlnsInfo->defaultNS = *pStateInfo->xsdName;  //take the input xsd as the default xsd
                pXsdPointer->pXmlnsInfo->isDefaultNSXsdName = true;
            }
        }
    }
    else
    {
    	if(rootAlias.l)
        {
        	logError("rootAlias(%r) is not defined.", &rootAlias);
            status = OS_ERROR_INVALID_VALUE;
            goto EXIT;
        }
        else
        {
        	//if there is no xmlns info, create a default one using the input xsdName
            pXsdPointer->pXmlnsInfo->defaultNS = *pStateInfo->xsdName;  //take the input xsd as the default xsd
            pXsdPointer->pXmlnsInfo->isDefaultNSXsdName = true;
        }
    }

    if(!pRootNS)
    {
    	pRootNS = &pXsdPointer->pXmlnsInfo->defaultNS;
    }

	mdebug(LM_XMLP, "rootNS=%r", pRootNS);
//    //save the nsInfoList in the global list for the purpose of memory free when the parse is done
//    osList_append(&pStateInfo->gNSList, pXsdPointer->pXmlnsInfo);
    pStateInfo->pCurXmlInfo = pXsdPointer->pXmlnsInfo;

    //now get the rootElem
    pXsdPointer->pCurElem = osXsd_getNSRootElem(pRootNS, pXsdPointer->pXmlnsInfo->isDefaultNSXsdName, &pElemInfo->tag);
    pStateInfo->anyElemStateInfo.pXsdRootElem = pXsdPointer->pCurElem;
    if(!pXsdPointer->pCurElem)
    {
    	logError("fails to find rootElem for tag(%r).", &pElemInfo->tag);
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

    //for root element, it is unlikely a simpleType.  Just here in case
    if(osXml_isXsdElemSimpleType(pXsdPointer->pCurElem))
    {
    	//set the openTagEndPos to memorize the beginning of value, to be used when parsing </xxx>.
        pStateInfo->tagPosInfo.openTagEndPos = pBuf->pos;
    }
    else
    {
    	pStateInfo->pParentXsdPointer = pXsdPointer;
        mdebug(LM_XMLP, "case <%r>, new pParentXsdPointer=%p, new pParentXsdPointer.elem=%r", &pElemInfo->tag, pStateInfo->pParentXsdPointer, &pStateInfo->pParentXsdPointer->pCurElem->elemName);
    }

    //only perform NULL value osXml_xmlCallback() when the element is not a leaf or is <xs:any> element, be note at that time we do not know if a <xs:any> element is a leaf
    if(pStateInfo->callbackInfo->isAllowNoLeaf && (!osXml_isXsdElemSimpleType(pXsdPointer->pCurElem) || pXsdPointer->pCurElem->dataType == OS_XML_DATA_TYPE_ANY))
    {
    	//The data sanity check is performed by callback()
        status = osXml_xmlCallback(pXsdPointer->pCurElem, NULL, &noXmlnsAttrList, false, pStateInfo->callbackInfo, pStateInfo->pCurXmlInfo);
    }

    osList_append(&pStateInfo->xsdElemPointerList, pXsdPointer);

EXIT:
	osList_clear(&noXmlnsAttrList);
    return status;
} //osXml_parseRootElem


/* process start of element, <element_name> */
static osStatus_e osXml_parseSOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo)
{
	osStatus_e status = OS_STATUS_OK;
	osXsd_elemPointer_t* pParentXsdPointer = pStateInfo->pParentXsdPointer;
	if(!pParentXsdPointer)
	{
		logError("pParentXsdPointer is null.", pParentXsdPointer);
		status = OS_ERROR_INVALID_VALUE;
		goto EXIT;
	}

    mdebug(LM_XMLP, "case <%r>, pParentXsdPointer=%p, pParentXsdPointer.elem=%r", &pElemInfo->tag, pParentXsdPointer, &pParentXsdPointer->pCurElem->elemName);

	osXmlElemDispType_e parentXsdDispType = OS_XML_ELEMENT_DISP_TYPE_ALL;
	int listIdx = -1;   //the sequence order of the current pElemInfo in the belonged complex type
	osXsdElement_t* pCurElem = osXml_getChildXsdElemByTag(&pElemInfo->tag, pParentXsdPointer, &parentXsdDispType, &listIdx);
	if(!pCurElem)
	{
		//could not find a element in the current ns, check if it is part of <xs:any>
		status = osXml_parseAnyRootElem(pBuf, pElemInfo, pStateInfo);
		if(status != OS_STATUS_OK)
		{
			logError("fails to osXml_parseAnyRootElem for tag(%r).", &pElemInfo->tag);
		}
            
		goto EXIT;
	}

    osXsd_elemPointer_t* pXsdPointer = oszalloc(sizeof(osXsd_elemPointer_t), NULL);
    pXsdPointer->pParentXsdPointer = pParentXsdPointer;
    //each child xsdPointer inherent from parent.
    pXsdPointer->pXmlnsInfo = pXsdPointer->pParentXsdPointer->pXmlnsInfo;
	pXsdPointer->pCurElem = pCurElem;

	//check OS_XML_ELEMENT_DISP_TYPE_SEQUENCE case to make sure the element is ordered.  the maxOccurs will be checked later in this function.  the minOccurs will be checked in EOT function
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

            for(int i=pParentXsdPointer->curIdx+1; i<listIdx; i++)
            {
                osXsdElement_t* pXsdElem = osList_getDataByIdx(&pParentCT->elemList, i);
                if(!pXsdElem)
                {
                    logError("elemList idx(%d) does not have xsd element data, this shall never happen.", pParentXsdPointer->curIdx);
                    osfree(pXsdPointer);
                    goto EXIT;
                }

				if(pXsdElem->pChoiceInfo)
				{
					//the gap Elem and the current elem belongs to the same choice, move on 
					if(pCurElem->pChoiceInfo && pCurElem->pChoiceInfo->tag == pXsdElem->pChoiceInfo->tag)
					{
						continue;
					}				

					//the gap elem is a choice element, and the choice has minOccurs = 0
					if(pXsdElem->pChoiceInfo->minOccurs == 0)
					{
						continue;
					}
				}

                if(pXsdElem->minOccurs > 0)
                {
                    logError("curidx=%d, element(%r) minOccurs=%d, and the parent CT is sequence disposition type, but the element does not show up before the current idx(%d).", i, &pXsdElem->elemName, pXsdElem->minOccurs, listIdx);
                    osfree(pXsdPointer);
                    status = OS_ERROR_INVALID_VALUE;
                    goto EXIT;
				}
				else
				{
	                //browse the info for xsd that is not configured in the xml file.  Inside the function, callback for the default value will be called
                    if(pStateInfo->callbackInfo->isUseDefault)
                    {
                    	status = osXsd_browseNode(pXsdElem, pStateInfo->callbackInfo);
                        if(status != OS_STATUS_OK)
                        {
        	                osfree(pXsdPointer);
                            goto EXIT;
                        }
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

    ++pParentXsdPointer->assignedChildIdx[listIdx].childCount;

	//if a element is within a choice, setup xmlChoiceInfo
    osXml_choiceInfo_t* pXmlChoiceInfo = NULL;
    if(pCurElem->pChoiceInfo)
    {
		pXmlChoiceInfo = osXml_getChoiceInfo(pParentXsdPointer, pCurElem->pChoiceInfo->tag);
		if(pXmlChoiceInfo)
		{
			pXmlChoiceInfo->choiceElemCount++;
		}
		else
		{
			pXmlChoiceInfo = oszalloc(sizeof(osXml_choiceInfo_t), NULL);
            pXmlChoiceInfo->choiceElemCount = 1;
            pXmlChoiceInfo->choiceTag = pCurElem->pChoiceInfo->tag;
			osList_append(&pParentXsdPointer->xmlChoiceList, pXmlChoiceInfo);
		}
	}

    mdebug(LM_XMLP, "pParentXsdPointer->assignedChildIdx[%d].childCount=%d for element(%r), choice element=%s", listIdx, pParentXsdPointer->assignedChildIdx[listIdx].childCount, &pCurElem->elemName, pCurElem->pChoiceInfo ? "yes" : "no");

    //check the maximum element occurrence is not exceeded
	if(pCurElem->pChoiceInfo)
	{
		if(pCurElem->pChoiceInfo->maxOccurs != -1 && pXmlChoiceInfo->choiceElemCount > pCurElem->pChoiceInfo->maxOccurs)
		{
            logError("the choice block that contains element(%r) occurance(%d) exceeds the maxOccurs(%d).", &pXsdPointer->pCurElem->elemName, pXmlChoiceInfo->choiceElemCount, pCurElem->pChoiceInfo->maxOccurs);
            status = OS_ERROR_INVALID_VALUE;
            osfree(pXsdPointer);
            goto EXIT;
		}
	}
	else
	{
		if(pXsdPointer->pCurElem->maxOccurs != -1 && pParentXsdPointer->assignedChildIdx[listIdx].childCount > pXsdPointer->pCurElem->maxOccurs)
    	{
        	logError("the element(%r) occurance(%d) exceeds the maxOccurs(%d).", &pXsdPointer->pCurElem->elemName, pParentXsdPointer->assignedChildIdx[listIdx].childCount, pXsdPointer->pCurElem->maxOccurs);
        	status = OS_ERROR_INVALID_VALUE;
        	osfree(pXsdPointer);
        	goto EXIT;
    	}
	}

    if(osXml_isXsdElemSimpleType(pXsdPointer->pCurElem))
    {
        //set the openTagEndPos to memorize the beginning of value, to be used when parsing </xxx>.
        pStateInfo->tagPosInfo.openTagEndPos = pBuf->pos;
    }
    else
    {
        pStateInfo->pParentXsdPointer = pXsdPointer;
        mdebug(LM_XMLP, "case <%r>, new pParentXsdPointer=%p, new pParentXsdPointer.elem=%r", &pElemInfo->tag, pXsdPointer, pXsdPointer ? (&pXsdPointer->pCurElem->elemName) : NULL);

	    //only perform NULL value osXml_xmlCallback() when the element is not a leaf
    	if(pStateInfo->callbackInfo->isAllowNoLeaf)
    	{
        	//The data sanity check is performed by callback()
        	status = osXml_xmlCallback(pXsdPointer->pCurElem, NULL, &pElemInfo->attrNVList, false, pStateInfo->callbackInfo, pStateInfo->pCurXmlInfo);
    	}
	}

    osList_append(&pStateInfo->xsdElemPointerList, pXsdPointer);

EXIT:
    return status;
}



/* process </element_name> */
static osStatus_e osXml_parseEOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo)
{
	osStatus_e status = OS_STATUS_OK;
	osListElement_t* pLE = osList_popTail(&pStateInfo->xsdElemPointerList);
    if(!pLE)
    {
        logError("xsdPointerList popTail is null, element(%r) close does not have matching open, pos=%ld.", &pElemInfo->tag, pBuf->pos);
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

	osXsd_elemPointer_t* pParentXsdPointer = pStateInfo->pParentXsdPointer;
    osXsdElement_t* pCurXsdElem = ((osXsd_elemPointer_t*)pLE->data)->pCurElem;

    mdebug(LM_XMLP, "case </%r>, old pParentXsdPointer=%p, old xsdElem=%r, new xsdElem=%r, new pParentXsdPointer=%p", &pElemInfo->tag, pParentXsdPointer, pParentXsdPointer ? (&pParentXsdPointer->pCurElem->elemName) : NULL, pCurXsdElem ? &pCurXsdElem->elemName : NULL, ((osXsd_elemPointer_t*)pLE->data)->pParentXsdPointer);

    //parentXsd changes, check the previous parentXsd's elemList to make sure all elements in the list are processed
    //it is possible pLE->data->pParentXsdPointer==NULL when the elem is a root element
    if(((osXsd_elemPointer_t*)pLE->data)->pParentXsdPointer != pParentXsdPointer)
    {
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
		//check for minOccurs.  The check of maxOccurs happens in parseSOT
        for(int i=0; i<osList_getCount(&pParentCT->elemList); i++)
        {
          	osXsdElement_t* pXsdElem = osList_getDataByIdx(&pParentCT->elemList, i);
            if(!pXsdElem)
            {
              	logError("elemList idx(%d) does not have xsd element data, this shall never happen.", i);
                osListElement_delete(pLE);
                goto EXIT;
            }

		    if(pXsdElem->pChoiceInfo)
    		{
				osXml_choiceInfo_t* pXmlChoiceInfo = osXml_getChoiceInfo(pParentXsdPointer, pXsdElem->pChoiceInfo->tag);
				if(pXsdElem->pChoiceInfo->minOccurs > 0 && (!pXmlChoiceInfo || pXmlChoiceInfo->choiceElemCount < pXsdElem->pChoiceInfo->minOccurs))
        		{
            		logError("the choice block that contains element(%r) occurance(%d) is less than the minOccurs(%d).", &pXsdElem->elemName, pXmlChoiceInfo ? pXmlChoiceInfo->choiceElemCount : -888, pXsdElem->pChoiceInfo->minOccurs);
					osListElement_delete(pLE);
            		status = OS_ERROR_INVALID_VALUE;
            		goto EXIT;
				}
    		}
			else
    		{
				if(pParentXsdPointer->assignedChildIdx[i].childCount < pXsdElem->minOccurs)
				{
                    logError("the element(%r) occurance(%d) is less than the minOccurs(%d).", &pXsdElem->elemName, pParentXsdPointer->assignedChildIdx[i].childCount, pXsdElem->minOccurs);
                    osListElement_delete(pLE);
                    status = OS_ERROR_INVALID_VALUE;
                    goto EXIT;
                }
			}

			//default callback for OS_XML_ELEMENT_DISP_TYPE_SEQUENCE happens in parseSOT.  Here only for OS_XML_ELEMENT_DISP_TYPE_ALL
			if(elemDispType == OS_XML_ELEMENT_DISP_TYPE_ALL && pXsdElem->minOccurs == 0 && pParentXsdPointer->assignedChildIdx[i].childCount == 0)
            {
               	//browse the info for xsd that is not configured in the xml file, inside the function, callback for the default value will be called
                if(pStateInfo->callbackInfo->isUseDefault)
                {
                  	status = osXsd_browseNode(pXsdElem, pStateInfo->callbackInfo);
                    if(status != OS_STATUS_OK)
                    {
                      	osListElement_delete(pLE);
                        goto EXIT;
                    }
                }
            }
        }

        pStateInfo->pParentXsdPointer = ((osXsd_elemPointer_t*)pLE->data)->pParentXsdPointer;
        if(pStateInfo->pParentXsdPointer)
        {
            //pCurXmlInfo is always updated after processing </element> (in case there is pParentXsdPointer change)
            //pCurXmlInfo will also be updated when <element> is processed and nsAlias may change
            //note pCurXmlInfo is introduced to simplify the setting of nsalias in osXml_xmlCallback().  it is also the reason it is
            //reassigned at the end of processing </element>
            pStateInfo->pCurXmlInfo = pStateInfo->pParentXsdPointer->pXmlnsInfo;

            //if !pParentXsdPointer, meaning the end of schema, that will be processed a little bit later
     	}
    }   //if(((osXsd_elemPointer_t*)pLE->data)->pParentXsdPointer != pParentXsdPointer)

    if(osPL_cmp(&pElemInfo->tag, &pCurXsdElem->elemName) != 0)
    {
        logError("element(%r) close does not match open(%r), pos=%ld.", &pElemInfo->tag, &pCurXsdElem->elemName, pBuf->pos);
        osListElement_delete(pLE);
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

    //only perform no NULL value osXml_xmlCallback() when the element is a leaf, the data sanity check is performed by callback()
    if(osXml_isXsdElemSimpleType(pCurXsdElem))
    {
		osPointerLen_t value = {&pBuf->buf[pStateInfo->tagPosInfo.openTagEndPos], pStateInfo->tagPosInfo.tagStartPos - pStateInfo->tagPosInfo.openTagEndPos}; 
        status = osXml_xmlCallback(pCurXsdElem, &value, NULL, true, pStateInfo->callbackInfo, pStateInfo->pCurXmlInfo);
    }
	else if(pStateInfo->callbackInfo->isAllowNoLeaf)
	{
		//The data sanity check is performed by callback()
		status = osXml_xmlCallback(pCurXsdElem, NULL, NULL, true, pStateInfo->callbackInfo, pStateInfo->pCurXmlInfo);
	}

    if(status != OS_STATUS_OK)
    {
        logError("fails to validate xml for element(%r).", &pCurXsdElem->elemName);
        osListElement_delete(pLE);
        goto EXIT;
    }

    //end of file check
    if(osPL_cmp(&pElemInfo->tag, &pStateInfo->anyElemStateInfo.pXsdRootElem->elemName) == 0)
    {
        mlogInfo(LM_XMLP, "xml parse is completed.");
        pStateInfo->isXmlParseDone = true;
    }

    osListElement_delete(pLE);

EXIT:
    return status;
}	//osXml_parseEOT


/* process <element_name /> */
static osStatus_e osXml_parseDOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo)
{
	osStatus_e status = OS_STATUS_OK;
	int listIdx = -1;   //the sequence order of the current pElemInfo in the belonged complex type

	//case <element_name xxx />, for now, we do not interpret/parse attribute against xsd
	//to-do, get the default value from XSD
    mdebug(LM_XMLP, "case <%r ... />, to-do, get the default value from XSD.", &pElemInfo->tag);
    osXsdElement_t* pCurElem = osXml_getChildXsdElemByTag(&pElemInfo->tag, pStateInfo->pParentXsdPointer, NULL, &listIdx);
	if(!pCurElem)
    {
     	logError("the xsd does not have matching tag(%r) does not match with xsd.", &pElemInfo->tag);
        goto EXIT;
    }

	//to-do, this may need to be modified to follow the same as in SOT, to chek if there is gap for OS_XML_ELEMENT_DISP_TYPE_SEQUENCE case 
    ++pStateInfo->pParentXsdPointer->assignedChildIdx[listIdx].childCount;

    status = osXml_xmlCallback(pCurElem, NULL, &pElemInfo->attrNVList, false, pStateInfo->callbackInfo, pStateInfo->pCurXmlInfo);

EXIT:
    return status;
}



static osStatus_e osXml_parseAnyRootElem(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo)
{
	osStatus_e status = OS_STATUS_OK;
    osList_t noXmlnsAttrList={};

    mdebug(LM_XMLP, "case <%r>, pParentXsdPointer=%p, pParentXsdPointer.elem=%r", &pElemInfo->tag, pStateInfo->pParentXsdPointer, &pStateInfo->pParentXsdPointer->pCurElem->elemName);

	//if a matching child element does not found, check if the parent xsd element contains any element
	//this happens for the first element going into the anyElement processing
	if(!isExistXsdAnyElem(pStateInfo->pParentXsdPointer))
	{
    	logError("the xsd does not have matching tag(%r), does not match with xsd.", &pElemInfo->tag);
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
	}

    osXsd_elemPointer_t* pXsdPointer = oszalloc(sizeof(osXsd_elemPointer_t), NULL);
    pXsdPointer->pParentXsdPointer = pStateInfo->pParentXsdPointer;
    //each child xsdPointer inherent from parent.
    pXsdPointer->pXmlnsInfo = pXsdPointer->pParentXsdPointer->pXmlnsInfo;

	//for the root <xs:any> element, needs to rebuild xmlns alias list
	osPointerLen_t rootAlias, rootElemName;
	osXml_singleDelimitParse(&pElemInfo->tag, ':', &rootAlias, &rootElemName);

	//get the xmlns alias
	osXml_nsInfo_t* pnsInfo = NULL;
	status = osXml_getNsInfo(&pElemInfo->attrNVList, &pnsInfo, &noXmlnsAttrList, &pStateInfo->gNSList);
	if(status != OS_STATUS_OK)
	{
		logError("fails to osXml_getNsInfo() for element(%r).", &pElemInfo->tag);
		goto EXIT;
	}

	//combine the parent nsInfo with new one
	if(pnsInfo)
	{
		osXml_updateNsInfo(pnsInfo, pXsdPointer->pXmlnsInfo);
	}

	/* sanity check the rootAlias and pXsdPointer->pXmlnsInfo.  2 cases: rootAlias(Exist, no exist), 
	pXsdPointer->pXmlnsInfo always exists (in the root elem processing, if xmlns does not exists, xsd name 
	will be used to create a pXsdPointer->pXmlnsInfo).
	if rootAlias exists, needs to match one in the pXsdPointer->pXmlnsInfo->nsAliasList
	*/
	if(rootAlias.l)
	{
		if(!osXml_isAliasExist(&rootAlias, &pXsdPointer->pXmlnsInfo->nsAliasList, NULL))
		{
			logError("rootAlias(%r) is not defined.", &rootAlias);
			status = OS_ERROR_INVALID_VALUE;
			goto EXIT;
		}
	}
	else
	{
		//the default NS does not exist in the new element.
		if(!pXsdPointer->pXmlnsInfo->defaultNS.l)
		{
			logError("defaultNS does not exist in the new element(%r), neither does the element has alias.", &pElemInfo->tag);
			status = OS_ERROR_INVALID_VALUE;
			goto EXIT;
		}
	}

//	//save the nsInfoList in the global list for the purpose of memory free when the parse is done
	if(pnsInfo)
	{
//		osList_append(&pStateInfo->gNSList, pXsdPointer->pXmlnsInfo);
		pStateInfo->pCurXmlInfo = pXsdPointer->pXmlnsInfo;
	}

	//no need to get rootElem as we do not check xml elem against a xsd for <xs:any>.  In the future, we may check if processContents="strict" or "lax"
	pXsdPointer->pCurElem = osXsd_createAnyElem(&pElemInfo->tag, true); //true here indicate it is the root anyElem, isRootAnyElem = true
	pStateInfo->isProcessAnyElem = true;
	mdebug(LM_XMLP, "created a xml element for a processContents=\"lax\" tag(%r), set isProcessAnyElem=true.", &pElemInfo->tag);

	pStateInfo->anyElemStateInfo.pPrevAnyElem = pXsdPointer->pCurElem;

    //for a <xs:any> element, do not move pParentXsdPointer since we do not have xsd, so treate it as a leaf node.

    //set the openTagEndPos to memorize the beginning of value, to be used when parsing </xxx>.
    //whether it is a true leaf element will be determined when processing </xxx>
    pStateInfo->tagPosInfo.openTagEndPos = pBuf->pos;

    //only perform NULL value osXml_xmlCallback() when the element is not a leaf or is <xs:any> element, be note at that time we do not know if a <xs:any> element is a leaf
    if(pStateInfo->callbackInfo->isAllowNoLeaf)
    {
        //The data sanity check is performed by callback()
        status = osXml_xmlCallback(pXsdPointer->pCurElem, NULL, &noXmlnsAttrList, false, pStateInfo->callbackInfo, pStateInfo->pCurXmlInfo);
    }

    osList_append(&pStateInfo->xsdElemPointerList, pXsdPointer);

EXIT:
	osList_clear(&noXmlnsAttrList);
    return status;
}


/* process <element_name> */
static osStatus_e osXml_parseAnyElemSOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo)
{
	osStatus_e status = OS_STATUS_OK;

	if(!pStateInfo->pParentXsdPointer)
	{
		logError("pParentXsdPointer is null.", pStateInfo->pParentXsdPointer);
		status = OS_ERROR_INVALID_VALUE;
		goto EXIT;
	}

	osXsd_elemPointer_t* pXsdPointer = oszalloc(sizeof(osXsd_elemPointer_t), NULL);
    pXsdPointer->pParentXsdPointer = pStateInfo->pParentXsdPointer;
    mdebug(LM_XMLP, "case <%r>, pParentXsdPointer=%p, pParentXsdPointer.elem=%r", &pElemInfo->tag, pXsdPointer->pParentXsdPointer, &pXsdPointer->pParentXsdPointer->pCurElem->elemName);

	//each child xsdPointer inherent from parent.
	pXsdPointer->pXmlnsInfo = pXsdPointer->pParentXsdPointer->pXmlnsInfo;

	pXsdPointer->pCurElem = osXsd_createAnyElem(&pElemInfo->tag, false);

    if(pStateInfo->anyElemStateInfo.pPrevAnyElem)
    {
    	pStateInfo->anyElemStateInfo.pPrevAnyElem->anyElem.xmlAnyElem.isLeaf = false;
    }
    pStateInfo->anyElemStateInfo.pPrevAnyElem = pXsdPointer->pCurElem;

    //for a <xs:any> element, do not move pParentXsdPointer since we do not have xsd, so treate it as a leaf node.

    //set the openTagEndPos to memorize the beginning of value, to be used when parsing </xxx>.
    //whether it is a true leaf element will be determined when processing </xxx>
    pStateInfo->tagPosInfo.openTagEndPos = pBuf->pos;

    //only perform NULL value osXml_xmlCallback() when the element is not a leaf or is <xs:any> element, be note at that time we do not know if a <xs:any> element is a leaf
    if(pStateInfo->callbackInfo->isAllowNoLeaf)
    {
        //The data sanity check is performed by callback()
        status = osXml_xmlCallback(pXsdPointer->pCurElem, NULL, &pElemInfo->attrNVList, false, pStateInfo->callbackInfo, pStateInfo->pCurXmlInfo);
    }

    osList_append(&pStateInfo->xsdElemPointerList, pXsdPointer);

EXIT:
    return status;
}


/* process <element_name /> */
static osStatus_e osXml_parseAnyElemDOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo)
{
	osStatus_e status = OS_STATUS_OK;
    //case <element_name xxx />
    mdebug(LM_XMLP, "case <%r ... />.", &pElemInfo->tag);
    if(pStateInfo->anyElemStateInfo.pPrevAnyElem)
    {
        pStateInfo->anyElemStateInfo.pPrevAnyElem->anyElem.xmlAnyElem.isLeaf = false;
	    pStateInfo->anyElemStateInfo.pPrevAnyElem = NULL;
	}
	
	osXsdElement_t* pCurElem = osXsd_createAnyElem(&pElemInfo->tag, false);
	pCurElem->anyElem.xmlAnyElem.isLeaf = false;

    status = osXml_xmlCallback(pCurElem, NULL, &pElemInfo->attrNVList, false, pStateInfo->callbackInfo, pStateInfo->pCurXmlInfo);

EXIT:
    return status;
}


/* process </element_name> */
static osStatus_e osXml_parseAnyElemEOT(osMBuf_t* pBuf, osXmlTagInfo_t* pElemInfo, osXml_parseStateInfo_t* pStateInfo)
{
    osStatus_e status = OS_STATUS_OK;

	osListElement_t* pLE = osList_popTail(&pStateInfo->xsdElemPointerList);
    if(!pLE)
    {
        logError("xsdPointerList popTail is null, element(%r) close does not have matching open, pos=%ld.", &pElemInfo->tag, pBuf->pos);
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

    //no need to check parentPointer change, since for <xs:any>, the parentXsdPointer does not change

    osXsdElement_t* pCurXsdElem = ((osXsd_elemPointer_t*)pLE->data)->pCurElem;
    if(osPL_cmp(&pElemInfo->tag, &pCurXsdElem->elemName) != 0)
    {
        logError("element(%r) close does not match open(%r), pos=%ld.", &pElemInfo->tag, &pCurXsdElem->elemName, pBuf->pos);
        osListElement_delete(pLE);
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

    //only perform no NULL value osXml_xmlCallback() when the element is a leaf, the data sanity check is performed by callback()
    if(pCurXsdElem->anyElem.xmlAnyElem.isLeaf)
    {
        osPointerLen_t value = {&pBuf->buf[pStateInfo->tagPosInfo.openTagEndPos], pStateInfo->tagPosInfo.tagStartPos - pStateInfo->tagPosInfo.openTagEndPos};
        status = osXml_xmlCallback(pCurXsdElem, &value, NULL, true, pStateInfo->callbackInfo, pStateInfo->pCurXmlInfo);
    }
    else if(pStateInfo->callbackInfo->isAllowNoLeaf)
    {
        //The data sanity check is performed by callback()
        status = osXml_xmlCallback(pCurXsdElem, NULL, NULL, true, pStateInfo->callbackInfo, pStateInfo->pCurXmlInfo);
    }

    //for any element, it is not part of xsd, and was dynamically allocated, need to free when it is not needed any more
    pStateInfo->anyElemStateInfo.pPrevAnyElem = NULL;
    //check if the anyElement processing is done. pCurXsdElem->isElementAny=true, pCurXsdElem->anyElem.isXmlAnyElem=true must have been set
    if(pCurXsdElem->anyElem.xmlAnyElem.isRootAnyElem)
    {
        pStateInfo->isProcessAnyElem = false;
    }
    osfree(pCurXsdElem);

    if(status != OS_STATUS_OK)
    {
        logError("fails to validate xml for element(%r).", &pCurXsdElem->elemName);
        osListElement_delete(pLE);
        goto EXIT;
    }

    //end of file check.  This shall never happen, here for the completeness
    if(osPL_cmp(&pElemInfo->tag, &pStateInfo->anyElemStateInfo.pXsdRootElem->elemName) == 0)
    {
        mlogInfo(LM_XMLP, "xml parse is completed.");
        pStateInfo->isXmlParseDone = true;
    }

    osListElement_delete(pLE);

EXIT:
    return status;
}  //osXml_parseAnyElemEOT() 


bool osXml_isValid(osMBuf_t* pXmlBuf, osMBuf_t* pXsdBuf, osPointerLen_t* xsdName, osXmlDataCallbackInfo_t* callbackInfo)
{
    if(!pXsdBuf || !pXmlBuf)
    {
        logError("null pointer, pXmlBuf=%p, pXsdBuf=%p.", pXmlBuf, pXsdBuf);
        return false;
    }

    osXsdNamespace_t* pNS = osXsd_parse(pXsdBuf, xsdName);
    if(!pNS)
    {
        logError("fails to osXsd_parse for xsdMBuf, pos=%ld.", pXsdBuf->pos);
        return false;
    }

	//assume one schema
	osXsdSchema_t* pSchema = pNS->schemaList.head->data;
    if(osXml_parseInternal(pXmlBuf, xsdName, callbackInfo) != OS_STATUS_OK)
	{
		logError("fails to osXml_parseInternal(), xmlBuf pos=%ld.", pXmlBuf->pos);
		return false;
	}

	osfree(pNS);
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
        logError("read xsdMBuf fails, xsdFile=%s", xsdFile);
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

    osPointerLen_t xsdName = {xsdFileName, strlen(xsdFileName)};
	status = osXml_getElemValue(&xsdName, xsdMBuf, xmlBuf, false, callbackInfo);

EXIT:
    if(status != OS_STATUS_OK)
    {
        osMBuf_dealloc(xsdMBuf);
        osMBuf_dealloc(xmlBuf);
    }

    return status;
}


osStatus_e osXml_getElemValue(osPointerLen_t* xsdName, osMBuf_t* xsdMBuf, osMBuf_t* xmlBuf, bool isKeepXsdNsList, osXmlDataCallbackInfo_t* callbackInfo)
{
    osStatus_e status = OS_STATUS_OK;

    if(!xsdName || !xmlBuf)
    {
        logError("null pointer, xsdName=%p, xmlBuf=%p.", xsdName, xmlBuf);
        status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

	osXsdNamespace_t* pNS = NULL;
	//for now, assume a xsd is only asked to parse one time. need to add xsdname in the schema level to prevent multiple parse for the same xsd, to do
	if(xsdMBuf)
	{
    	logInfo("start xsd parse for %r.", xsdName);
    	pNS = osXsd_parse(xsdMBuf, xsdName);
    	if(!pNS)
    	{
        	logError("fails to osXsd_parse for xsdName(%r).", xsdName);
        	status = OS_ERROR_INVALID_VALUE;
        	goto EXIT;
    	}
    	logInfo("xsd parse for %r is done.", xsdName);
	}

    logInfo("start xml parse, xml size=%ld.", xmlBuf->end);
//    osXsdSchema_t* pSchema = pNS->schemaList.head->data;
    status = osXml_parseInternal(xmlBuf, xsdName, callbackInfo);
    if(status != OS_STATUS_OK)
    {
        logError("xml parse failed.");
        goto EXIT;
    }

    logInfo("xml parse using xsd name(%r) is done.", xsdName);

EXIT:
	if(!isKeepXsdNsList)
	{
    	osXsd_freeNsList(xsdName);
	}

    return status;
}


/* this function can be called for leaf or no leaf, for no leaf, it is expected to be called in the xml element start, like <xs:xxx>
 * a xml element stop is like </xs:xxx>.  For called in xml start, the value shall be set to NULL
 *
 * if isUseDefault=true, then do not parse ns alias in this function, assume user will have ns alias as part of the element name in
 * xmlData.dataName.  The reason is that when isUseDefault=true, the xml parse may go into xsd for the default value, the xsd may use
 * different ns alias for the same ns in xml.  The isUseDefault=true is a poperity implementation, xml spec allows a element to use
 * a default value in xsd only when in xml instance, <element /> is used.  So it shall not be a problem to restrict the ns alias use
 * for isUseDefault=true.  Just need to be careful what xsd is used and to include ns alias in osXmlData_t.dataname for isUseDefault=true.
 */
osStatus_e osXml_xmlCallback(osXsdElement_t* pElement, osPointerLen_t* value, const osList_t* pNoXmlnsAttrList, bool isEOT, osXmlDataCallbackInfo_t* callbackInfo, void* pCurXmlInfo)
{
	osStatus_e status = OS_STATUS_OK;
	bool isLeaf = true;

	//as mentioned, value can be NULL
	if(!pElement || !callbackInfo)
	{
		logError("null pointer, pElement=%p, callbackInfo=%p.", pElement, callbackInfo);
		return OS_ERROR_NULL_POINTER;
	}

	mdebug(LM_XMLP, "for element(%r), value=%r, isEOT=%d.", &pElement->elemName, value, isEOT);
 
	//leaf related check
	if(value)
	{
		if(!osXml_isXsdElemSimpleType(pElement) && !(pElement->dataType == OS_XML_DATA_TYPE_ANY && pElement->anyElem.xmlAnyElem.isLeaf))
		{
			logError("a no leaf element(%r) has a value(%r)", &pElement->elemName, value);
			return OS_ERROR_INVALID_VALUE;
		}
	}
	else
	{
		//in value==NULL stage, a <xs:any> element has not determined if it is a leaf, but a xs type element already know from the previous parsed xsd
        if(osXml_isXsdElemSimpleType(pElement))
        {
            logError("the element(%r) is a leaf, but there is no value.", &pElement->elemName);
            return OS_ERROR_INVALID_VALUE;
        }

		if(!callbackInfo->isAllowNoLeaf)
		{
			mdebug(LM_XMLP, "the element(%r) is not a leaf, but callback isAllowNoLeaf is false.", &pElement->elemName);
			return status;
		}

		isLeaf = false;
    }

	//for all element callback case
	if(callbackInfo->xmlCallback && callbackInfo->isAllElement)
	{
		osXmlData_t xmlData;
		xmlData.eDataName = OS_XML_INVALID_EDATA_NAME;
		xmlData.pNoXmlnsAttrList = pNoXmlnsAttrList;

	    if(isLeaf && value)
        {
			if(pElement->dataType == OS_XML_DATA_TYPE_ANY)
            {
                pElement->dataType = OS_XML_DATA_TYPE_XS_STRING;
            }
                
			switch(pElement->dataType)
            {
                case OS_XML_DATA_TYPE_XS_BOOLEAN:
                case OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE:
                case OS_XML_DATA_TYPE_XS_SHORT:
                case OS_XML_DATA_TYPE_XS_INTEGER:
                case OS_XML_DATA_TYPE_XS_LONG:
                case OS_XML_DATA_TYPE_XS_STRING:
                    status = osXmlXSType_convertData(&pElement->elemName, value, pElement->dataType, &xmlData);
                    break;
                case OS_XML_DATA_TYPE_SIMPLE:
                    status = osXmlSimpleType_convertData(pElement->pSimple, value, &xmlData);
                    break;
                default:
                    logError("unexpected data type(%d) for element(%r).", pElement->dataType, &pElement->elemName);
                    return OS_ERROR_INVALID_VALUE;
                    break;
            }

			//leaf is always EOT
			xmlData.isEOT = true;
		}
		else
		{
			xmlData.dataType = pElement->dataType;
			xmlData.isEOT = isEOT;
		}

		if(callbackInfo->isUseDefault)
		{
			xmlData.dataName = pElement->elemName;
			xmlData.nsAlias.l = 0;
		}
		else
		{
        	osXml_singleDelimitParse(&pElement->elemName, ':', &xmlData.nsAlias, &xmlData.dataName);
		}

		callbackInfo->xmlCallback(&xmlData, pCurXmlInfo, callbackInfo->appData);

		return status;
	}

	//for selective element callback case
    for(int i=0; i<callbackInfo->maxXmlDataSize; i++)
    {
        //that requires: within xmlData, the dataName has to be sorted from shortest to longest
        if(pElement->elemName.l < callbackInfo->xmlData[i].dataName.l)
        {
            mdebug(LM_XMLP, "the app does not need element(%r), value=%r, ignore.", &pElement->elemName, isLeaf? value : NULL);
            return status;
        }

        //find the matching data name
		bool isNameMatch = false; 
		osPointerLen_t nsAlias = {};
		if(callbackInfo->isUseDefault)
		{
			isNameMatch = !osPL_cmp(&pElement->elemName, &callbackInfo->xmlData[i].dataName);
		}
		else
		{
			osPointerLen_t elemRealName;
         	osXml_singleDelimitParse(&pElement->elemName, ':', &nsAlias, &elemRealName);
			isNameMatch = !osPL_cmp(&elemRealName, &callbackInfo->xmlData[i].dataName);
		}

        if(isNameMatch)
        {
			bool inputXmlDataEOT = callbackInfo->xmlData[i].isEOT;
            osXmlDataType_e origElemDataType = pElement->dataType;

			if(isLeaf && value)
			{
				if(pElement->dataType == OS_XML_DATA_TYPE_ANY)
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

	            //leaf is always EOT
    	        callbackInfo->xmlData[i].isEOT = true;
			}
			else
			{
				if(!callbackInfo->xmlData[i].isEOT && isEOT)
				{
					mdebug(LM_XMLP, "the element(%r) is EOT, but app does not require EOT for this element.", &pElement->elemName);
					return OS_STATUS_OK;
				}

				callbackInfo->xmlData[i].isEOT = isEOT;
			}

			callbackInfo->xmlData[i].nsAlias = nsAlias;

			if(callbackInfo->xmlCallback)
			{
				callbackInfo->xmlData[i].pNoXmlnsAttrList = pNoXmlnsAttrList;
				callbackInfo->xmlCallback(&callbackInfo->xmlData[i], (void*)pCurXmlInfo, callbackInfo->appData);
				//assign back to the original user expected data type, since callbackInfo->xmlData[i] may be reused if the xml use the same data type multiple times
				callbackInfo->xmlData[i].dataType = origElemDataType;
			}

			//set back to the original EOT in the input xmlData
			callbackInfo->xmlData[i].isEOT = inputXmlDataEOT;
            break;
        }
    } //for(int i=0; i<callbackInfo->maxXmlDataSize; i++)

	return status;
}



/* get a xsd element(pXsdPointer)'s complex type info, include the order of the current xml element(pTag) in the complex type
 * pTag:               IN, the current element name in xml that is under parsing
 * pXsdPointer:        IN, the pTag's parent element in xsd.  Take the xml and xsd example in function osXml_parseInternal() as an example, when parsing pTag="tLayer2-2", 
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
            goto EXIT;
        }

        pLE = pLE->next;
    }

    //no match found, set back to -1
    *listIdx = -1;

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


static bool isExistXsdAnyElem(osXsd_elemPointer_t* pXsdPointer)
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
		if(((osXsdElement_t*)pLE->data)->dataType == OS_XML_DATA_TYPE_ANY)
        {
            isExistAnyElem = true;
            break;
        }

        pLE = pLE->next;
    }

EXIT:
    return isExistAnyElem;
}


static osStatus_e osXml_getNsInfo(osList_t* pAttrNVList, osXml_nsInfo_t** ppNsInfo, osList_t* pNoXmlnsAttrList, osList_t* pgNSList)
{
	osStatus_e status = OS_STATUS_OK;
	
	if(!pAttrNVList || !ppNsInfo || !pgNSList)
	{
		logError("null pointer, pAttrNVList=%p, ppNsInfo=%p, pgNSList=%p.", pAttrNVList, ppNsInfo, pgNSList);
		return OS_ERROR_NULL_POINTER;
	}

	*ppNsInfo = oszalloc(sizeof(osXml_nsInfo_t), osXmlNsInfo_cleanup);

	osPointerLen_t nsAlias;
	osXsd_nsAliasInfo_t* pnsAlias;
	osListElement_t* pLE = pAttrNVList->head;
	while(pLE)
	{
        if(osXml_singleDelimitMatch("xmlns", 5, ':', &((osXmlNameValue_t*)pLE->data)->name, false, &nsAlias))
		{
			pnsAlias = oszalloc(sizeof(osXsd_nsAliasInfo_t), NULL);
			pnsAlias->nsAlias = nsAlias;
			pnsAlias->ns = ((osXmlNameValue_t*)pLE->data)->value;
			if(!pnsAlias->nsAlias.l)
			{
				if((*ppNsInfo)->defaultNS.l)
				{
					logError("more than one default namespaces. The existing one(%r), the new one(%r).", &(*ppNsInfo)->defaultNS, &((osXmlNameValue_t*)pLE->data)->value);
					osfree(pnsAlias);
					status = OS_ERROR_INVALID_VALUE;
					goto EXIT;
				}

				pnsAlias->nsUseLabel = -1;
				(*ppNsInfo)->defaultNS = pnsAlias->ns;
			}

			osList_append(&(*ppNsInfo)->nsAliasList, pnsAlias);
			//store each nsalias in pnsAlias so that it can be freed when the aprser is done.  Do nor want to store ppNsInfo, because in the course of xml parsing, multiple ppNsInfo may use the same pnsAlias.
			osList_append(pgNSList, pnsAlias);
		}
		else if(pNoXmlnsAttrList)
		{
			//pNoXmlnsAttrList shall not outlive pAttrNVList, so no need to copy pLE->data, just use it
			osList_append(pNoXmlnsAttrList, pLE->data);
		}

		pLE = pLE->next;
	}
		
	//it is possible a ns is default, at the same time is assigned a alias. pnsAlias->nsUseLabel=1 is for this case.  Here we do not check this case and do not combine 2 entries into 1, to save the check time.  i.e., pnsAlias->nsUseLabel will never be 1 when this function is used.	
EXIT:
	if(status != OS_STATUS_OK)
	{
		*ppNsInfo = osfree(*ppNsInfo);	
	}

	//also store ppNsInfo in pgNSList, as ppNsInfo will be pointed by each child element.
	osList_append(pgNSList, *ppNsInfo);
	return status;
}


/* pNewNsInfo: 			 IN,  the new xmlns info from the element been parsing
 * pXsdPointerXmlnsInfo: INOUT, in, the existing parent xsdPointer nsInfo, out, combined/updated nsInfo for the element
 * 
 * The combined nsInfo will be a combination of the aprent nsInfo and the new gotten nsInfo.  If there is alias name overlap, 
 * the new one will override the parent one.  
 */ 
static void osXml_updateNsInfo(osXml_nsInfo_t* pNewNsInfo, osXml_nsInfo_t* pXsdPointerXmlnsInfo)
{
	if(!pNewNsInfo)
	{
		return;
	}

	if(!pXsdPointerXmlnsInfo)
	{
		logError("null pointer, pXsdPointerXmlnsInfo.");
		return;
	}

	//check if there is nsAlias overlap.  Bypass if pXsdPointerXmlnsInfo->nsAliasList has no entry
	osListElement_t* pLE = osList_getCount(&pXsdPointerXmlnsInfo->nsAliasList) ? pNewNsInfo->nsAliasList.head : NULL;
	while(pLE)
	{
		osListElement_t* pMatchLE = osXml_isAliasMatch(&pXsdPointerXmlnsInfo->nsAliasList, &((osXsd_nsAliasInfo_t*)pLE->data)->nsAlias);
		if(pMatchLE)
		{
			osList_deleteElementAll(pMatchLE, true);
		}

		pLE = pLE->next;
	}

	if(pNewNsInfo->defaultNS.l)
	{
		pXsdPointerXmlnsInfo->isDefaultNSXsdName = false;
		pXsdPointerXmlnsInfo->defaultNS = pNewNsInfo->defaultNS;
	}

	osList_combine(&pXsdPointerXmlnsInfo->nsAliasList, &pNewNsInfo->nsAliasList);
}
		

static bool osXml_isAliasExist(osPointerLen_t* pRootAlias, osList_t* pAliasList, osPointerLen_t** ppRootNS)
{
	if(!pRootAlias || !pAliasList)
	{
		logError("null pointer, pRootAlias=%p, pAliasList=%p.", pRootAlias, pAliasList);
		return false;
	}

	osListElement_t* pLE = pAliasList->head;
	while(pLE)
	{
		if(osPL_cmp(&((osXsd_nsAliasInfo_t*)pLE->data)->nsAlias, pRootAlias) == 0)
		{
			if(ppRootNS)
			{
				*ppRootNS = &((osXsd_nsAliasInfo_t*)pLE->data)->ns;
			}
			return true;
		}
	
		pLE = pLE->next;
	}

	if(ppRootNS)
	{
		*ppRootNS = NULL;
	}

	return false;
}


static osListElement_t* osXml_isAliasMatch(osList_t* nsAliasList, osPointerLen_t* pnsAlias)
{
	if(!nsAliasList || !pnsAlias)
	{
		logError("null pointer, nsAliasList=%p, pnsAlias=%p.", nsAliasList, pnsAlias);
		return NULL;
	}

	osListElement_t* pLE = nsAliasList->head;
	while(pLE)
	{
		if(osPL_cmp(&((osXsd_nsAliasInfo_t*)pLE->data)->nsAlias, pnsAlias) == 0)	
		{
			return pLE;
		}

		pLE = pLE->next;
	}

	return NULL;
}


static osXml_choiceInfo_t* osXml_getChoiceInfo(osXsd_elemPointer_t* pParentXsdPointer, uint32_t choiceTag)
{
	if(!pParentXsdPointer)
	{
		logError("null pointer, pParentXsdPointer.");
		return NULL;
	}

	osListElement_t* pLE = pParentXsdPointer->xmlChoiceList.head;
    while(pLE)
    {
        if(((osXml_choiceInfo_t*)pLE->data)->choiceTag == choiceTag)
        {
			return pLE->data;
        }

        pLE = pLE->next;
    }

	return NULL;
}


static void osXmlNsInfo_cleanup(void* data)
{
	if(!data)
	{
		return;
	}

	osXml_nsInfo_t* pnsInfo = data;
	//do not use osList_delete(), as nsAlias is seperately stored in gNSList
	osList_clear(&pnsInfo->nsAliasList);
}


static void osXsd_elemPointer_cleanup(void* data)
{
    if(!data)
    {
        return;
    }

	osXsd_elemPointer_t* pElemPointer = data;
	osList_delete(&pElemPointer->xmlChoiceList);

	//this data structure only refers pXmlnsInfo, no need to free it.  Instead, the object that pXmlnsInfo points to is appended in a gNSList and will be freed over there 
}
