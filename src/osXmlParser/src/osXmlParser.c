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



static osXsdElement_t* osXml_getChildXsdElemByTag(osPointerLen_t* pTag, osXsd_elemPointer_t* pXsdPointer, osXmlElemDispType_e* pParentXsdDispType, int* listIdx);
static osXmlComplexType_t* osXsdPointer_getCT(osXsd_elemPointer_t* pXsdPointer);


	
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

			//only perform no NULL value osXml_xmlCallback() when the element is a leaf
            if(osXml_isXsdElemSimpleType(pCurXsdElem) || (pCurXsdElem->isElementAny && pCurXsdElem->anyElem.xmlAnyElem.isLeaf))
            {
                value.l = tagStartPos - value.l;

				//The data sanity check is performed by callback()
            	status = osXml_xmlCallback(pCurXsdElem, &value, callbackInfo);
			}

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

				//pCurXSAlias always points to the current root element's XS namespace
				osXsd_setXSAlias(&pXsdRootElem->pSchema->xsAlias);
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

            //only perform NULL value osXml_xmlCallback() when the element is not a leaf or is <xs:any> element, be note at that time we do not know if a <xs:any> element is a leaf
            if(callbackInfo->isAllowNoLeaf && (!osXml_isXsdElemSimpleType(pXsdPointer->pCurElem) || pXsdPointer->pCurElem->isElementAny))
            {
                //The data sanity check is performed by callback()
                status = osXml_xmlCallback(pXsdPointer->pCurElem, NULL, callbackInfo);
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



bool osXml_isXmlValid(osMBuf_t* pXmlBuf, osMBuf_t* pXsdBuf, osXmlDataCallbackInfo_t* callbackInfo)
{
    if(!pXsdBuf || !pXmlBuf)
    {
        logError("null pointer, pXmlBuf=%p, pXsdBuf=%p.", pXmlBuf, pXsdBuf);
        return false;
    }

    osXsdNamespace_t* pNS = osXsd_parse(pXsdBuf);
    if(!pNS)
    {
        logError("fails to osXsd_parse for xsdMBuf, pos=%ld.", pXsdBuf->pos);
        return false;
    }

	//assume one schema
	osXsdSchema_t* pSchema = pNS->schemaList.head->data;
    if(osXml_parse(pXmlBuf, ((osXsdElement_t*)pSchema->gElementList.head->data), callbackInfo) != OS_STATUS_OK)
	{
		logError("fails to osXml_parse(), xmlBuf pos=%ld.", pXmlBuf->pos);
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
    osXsdNamespace_t* pNS = osXsd_parse(xsdMBuf);
    if(!pNS)
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
    osXsdSchema_t* pSchema = pNS->schemaList.head->data;
    status = osXml_parse(xmlBuf, ((osXsdElement_t*)pSchema->gElementList.head->data), callbackInfo);
//    status = osXml_parse(xmlBuf, pXsdRoot, callbackInfo);
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


/* this function can be called for leaf or no leaf, for no leaf, it is expected to be called in the xml element start, like <xs:xxx>
 * a xml element stop is like </xs:xxx>.  For called in xml start, the value shall be set to NULL
 */
osStatus_e osXml_xmlCallback(osXsdElement_t* pElement, osPointerLen_t* value, osXmlDataCallbackInfo_t* callbackInfo)
{
	osStatus_e status = OS_STATUS_OK;
	bool isLeaf = true;

	//as mentioned, value can be NULL
	if(!pElement || !callbackInfo)
	{
		logError("null pointer, pElement=%p, callbackInfo=%p.", pElement, callbackInfo);
		return OS_ERROR_NULL_POINTER;
	}

	//leaf related check
	if(value)
	{
		if(!osXml_isXsdElemSimpleType(pElement) && !(pElement->isElementAny && pElement->anyElem.xmlAnyElem.isLeaf))
		{
			logError("a no leaf element(%r) has a value(%r)", &pElement->elemName, value);
			return OS_ERROR_INVALID_VALUE;
		}
	}
	else
	{
		//in value==0 stage, a <xs:any> element has not determined if it is a leaf, but a xs type element already know from the previous parsed xsd
        if(osXml_isXsdElemSimpleType(pElement))
        {
            logError("the element(%r) is a leaf, but there is no value.", &pElement->elemName);
            return OS_ERROR_INVALID_VALUE;
        }

		if(!callbackInfo->isAllowNoLeaf)
		{
			return status;
		}

		isLeaf = false;
#if 0
		if(pElement->isElementAny)
		{
			pElement->dataType = OS_XML_DATA_TYPE_COMPLEX;
		}
#endif
    }


    for(int i=0; i<callbackInfo->maxXmlDataSize; i++)
    {
        //that requires within xmlData, the dataName has to be sorted from shortest to longest
        if(pElement->elemName.l < callbackInfo->xmlData[i].dataName.l)
        {
            mdebug(LM_XMLP, "the app does not need element(%r), value=%r, ignore.", &pElement->elemName, isLeaf? value : NULL);
            return status;
        }

        //find the matching data name
        if(osPL_cmp(&pElement->elemName, &callbackInfo->xmlData[i].dataName) == 0)
        {
            osXmlDataType_e origElemDataType = pElement->dataType;

			if(isLeaf && value)
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
				callbackInfo->xmlData[i].dataType = origElemDataType;
			}
            break;
        }
    } //for(int i=0; i<callbackInfo->maxXmlDataSize; i++)

	return status;
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
