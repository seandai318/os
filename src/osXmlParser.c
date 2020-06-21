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



//help data structure for tag and namve-value pair list
typedef struct {
	osPointerLen_t tag;
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
    OS_XML_TAG_INFO_START,
    OS_XML_TAG_INFO_BEFORE_TAG_INSIDE_QUOTE,
    OS_XML_TAG_INFO_TAG_START,
    OS_XML_TAG_INFO_TAG,
	OS_XML_TAG_INFO_CONTENT_NAME_START,
	OS_XML_TAG_INFO_CONTENT_NAME,
	OS_XML_TAG_INFO_CONTENT_NAME_STOP,
	OS_XML_TAG_INFO_CONTENT_VALUE_START,
	OS_XML_TAG_INFO_CONTENT_VALUE,
    OS_XML_TAG_INFO_END_TAG_SLASH,
} osXmlCheckTagInfoState_e;


static osXsdElement_t* osXml_getRootElemInfo(osMBuf_t* pXmlBuf);
static osStatus_e osXml_elemLinkChild(osXsdElement_t* pParentElem, osList_t* pTypeList);
static osXsdElement_t* osXml_parseGlobalTags(osMBuf_t* pXmlBuf, osList_t* pTypeList);
static osStatus_e osXml_parseTag(osMBuf_t* pBuf, bool isTagNameChecked, bool* isTagDone, bool* isEndTag, osXmlTagInfo_t** ppTagInfo);
static osXsdElement_t* osXmlElement_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pTagInfo);
static osStatus_e osXmlElement_getAttrInfo(osList_t* pAttrList, osXsdElement_t* pElement);
static osStatus_e osXmlElement_getSubTagInfo(osXsdElement_t* pElement, osXmlTagInfo_t* pTagInfo);
static osXmlComplexType_t* osXmlComplexType_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pCtTagInfo, osXsdElement_t* pParentElem);
static osStatus_e osXmlComplexType_getAttrInfo(osList_t* pAttrList, osXmlComplexType_t* pCtInfo);
static osStatus_e osXmlComplexType_getSubTagInfo(osXmlComplexType_t* pCtInfo, osXmlTagInfo_t* pTagInfo);
static bool osXml_findWhiteSpace(osMBuf_t* pBuf, bool isAdvancePos);
static osXmlDataType_e osXml_getElementType(osPointerLen_t* pTypeValue);
static bool osXml_findPattern(osMBuf_t* pXmlBuf, osPointerLen_t* pPattern, bool isAdvancePos);

static bool osXml_listFoundMatchingType(osListElement_t* pLE, void* typeName);




osXsdElement_t* osXml_parseXsd(osMBuf_t* pXmlBuf)	
{
	
	osXsdElement_t* pRootElem = NULL;
	osList_t typeList = {};
	
	if(!pXmlBuf)
	{
		logError("null pointer, pXmlBuf.");
		goto EXIT;
	}

	while(pXmlBuf->pos < pXmlBuf->end)
	{
		osXsdElement_t* pElem = osXml_parseGlobalTags(pXmlBuf, &typeList);
		if(pElem)
		{
			if(pRootElem)
			{
				logError("there are more than one root element, pos=%ld.", pXmlBuf->pos);
				osfree(pRootElem);
				pRootElem = NULL;
				goto EXIT;
			}
			else
			{
				pRootElem = pElem;
			}
		}
	}

	if(!pRootElem)
	{
		logError("fails to osXml_parseGlobalTags, pRootElem=NULL.");
		goto EXIT;
	}

	osXml_elemLinkChild(pRootElem, &typeList);

EXIT:
	osList_clear(&typeList);
	return pRootElem;
}


static osStatus_e osXml_elemLinkChild(osXsdElement_t* pParentElem, osList_t* pTypeList)
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
			osListElement_t* pLE = osList_lookup(pTypeList, true, osXml_listFoundMatchingType, &pParentElem->elemTypeName);
			if(!pLE)
			{
				logError("does not find complex type definition for(%r).", &pParentElem->elemTypeName);
				status = OS_ERROR_INVALID_VALUE;
				goto EXIT;
			}
			
			pParentElem->pComplex = pLE->data; 
			
			while((pLE = osList_popHead(&pParentElem->pComplex->elemList)) != NULL)
			{
				status = osXml_elemLinkChild((osXsdElement_t*)pLE->data, pTypeList);
				if(status != OS_STATUS_OK)
				{
					logError("fails to osXml_elemLinkChild for (%r).", pLE->data ? &((osXsdElement_t*)pLE->data)->elemName : NULL);
					goto EXIT;
				}
			}
			break;
		}
		case OS_XML_DATA_TYPE_SIMPLE:
			//to do
			break;
		default:
			//already stored in each element's data structure
			break;
	}

EXIT:
	return status;
}


//this function parse the root element and complexType
static osXsdElement_t* osXml_parseGlobalTags(osMBuf_t* pXmlBuf, osList_t* pTypeList)
{
	osStatus_e status = OS_STATUS_OK;
    osList_t tagList = {};
    osXsdElement_t* pRootElement = NULL;

    if(!pXmlBuf)
    {
        logError("pXmlBuf=%p.", pXmlBuf);
        goto EXIT;
    }

    bool isTagDone = false; //the tag is wrapped in one line, i.e., <tag, tag-content />
    bool isEndTag = false;  //the line is the end of tag, i.e., </tag>

    //get tag info for the immediate next tag
    osXmlTagInfo_t* pTagInfo = NULL;
	status = osXml_parseTag(pXmlBuf, false, &isTagDone, &isEndTag, &pTagInfo);
    if(status != OS_STATUS_OK)
    {
		logError("fails to osXml_parseTag.");
        goto EXIT;
    }

	if(!pTagInfo)
	{
		logInfo("all tags are parsed.");
		goto EXIT;
	}

	switch(pTagInfo->tag.l)
	{
		case OS_XML_COMPLEXTYPE_LEN:        //len = 14, "xs:complexType"
        {
			if(pTagInfo->tag.p[3] != 'c' || strncmp("xs:complexType", pTagInfo->tag.p, pTagInfo->tag.l) != 0)
			{
				logInfo("top tag(%r) len=14, but is not xs:complexType, ignore.", &pTagInfo->tag);
				break;
			}

			osXmlComplexType_t* pCtInfo = osXmlComplexType_parse(pXmlBuf, pTagInfo, NULL);
			if(!pCtInfo || !pCtInfo->typeName.l)
			{
				logError("global type is NULL or has no typeName, pCtInfo=%p, typeName.l=%ld.", pCtInfo, pCtInfo->typeName.l);
				status = OS_ERROR_INVALID_VALUE;
				break;
			}

            //insert the complex type into typeList so that the type can be linked in by elements
            osList_append(pTypeList, pCtInfo);
			break;
		}
		case OS_XML_ELEMENT_LEN:            //10, "xs:element"
        {
            pRootElement = osXml_getRootElemInfo(pXmlBuf);
            if(!pRootElement)
            {
                logError("fails to osXml_getRootElemInfo, pos=%ld.", pXmlBuf->pos);
                goto EXIT;
            }
            break;
        }
    }

EXIT:
    return pRootElement;
}


static osXsdElement_t* osXml_getRootElemInfo(osMBuf_t* pXmlBuf)
{
	osStatus_e status = OS_STATUS_OK;
	osXsdElement_t* pRootElement = NULL;
	osXmlTagInfo_t* pTagInfo = NULL;

    if(!pXmlBuf)
    {
        logError("null pointer, pCtInfo.");
        goto EXIT;
    }

    bool isTagDone = false; //the tag is wrapped in one line, i.e., <tag, tag-content />
    bool isEndTag = false;  //the line is the end of tag, i.e., </tag>
    status = osXml_parseTag(pXmlBuf, true, &isTagDone, &isEndTag, &pTagInfo);
    if(status != OS_STATUS_OK || !pTagInfo)
    {
        logError("fails to osXml_parseTag.");
        goto EXIT;
    }

	if(isTagDone)
	{
		//for <xs:element name="A" type="B" />
		pRootElement = osmalloc(sizeof(osXsdElement_t), NULL);

		osXmlElement_getAttrInfo(&pTagInfo->attrNVList, pRootElement);
	}
	else
	{
		//for <xs:element name="A" type="B" ><xs:complexType name="C">...</xs:complexType></xs:element>, TO-DO
		osXsdElement_t* pElem = osXmlElement_parse(pXmlBuf, pTagInfo);
        if(!pElem)
        {
        	logError("fails to osXmlElement_parse, pos=%ld.", pXmlBuf->pos);
            goto EXIT;
        }
	}

EXIT:
	osfree(pTagInfo);
	return pRootElement;
}


static osStatus_e osXmlComplexType_getAttrInfo(osList_t* pAttrList, osXmlComplexType_t* pCtInfo)
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
						logInfo("xml mixed value is not correct(%r), ignore.", &pNV->value);
					}
				}
				break;
			default:
				break;
		}
		
		if(isIgnored)
		{
			logInfo("attribute(%r) is ignored.", &pNV->name);
		}

		pLE = pLE->next;
	}

EXIT:
	return status;
}	
	
	
static osStatus_e osXmlComplexType_getSubTagInfo(osXmlComplexType_t* pCtInfo, osXmlTagInfo_t* pTagInfo)
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
        			pElement = osmalloc(sizeof(osXsdElement_t), NULL);
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
		logInfo("xml tag(%r) is ignored.", &pTagInfo->tag);
	}

EXIT:
	return status;
}


static osXmlComplexType_t* osXmlComplexType_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pCtTagInfo, osXsdElement_t* pParentElem)
{
	osStatus_e status = OS_STATUS_OK;

    osList_t tagList = {};
    osXmlComplexType_t* pCtInfo = NULL;

    if(!pXmlBuf || !pCtTagInfo)
    {
        logError("null pointer, pXmlBuf=%p, pCtTagInfo=%p.", pXmlBuf, pCtTagInfo);
		status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

    pCtInfo = osmalloc(sizeof(osXmlComplexType_t), NULL);
    osXmlComplexType_getAttrInfo(&pCtTagInfo->attrNVList, pCtInfo);

    while(pXmlBuf->pos < pXmlBuf->end)
    {
		bool isTagDone = false; //the tag is wrapped in one line, i.e., <tag, tag-content />
        bool isEndTag = false;  //the line is the end of tag, i.e., </tag>

        //get tag info for each tags inside xs:complexType
        osXmlTagInfo_t* pTagInfo = NULL;
		status = osXml_parseTag(pXmlBuf, false, &isTagDone, &isEndTag, &pTagInfo);
        if(status != OS_STATUS_OK || !pTagInfo)
        {
         	logError("fails to osXml_parseTag.");
        	break;
        }

        if(isEndTag)
        {
        	osListElement_t* pLE = osList_popTail(&tagList);
            if(!pLE)
            {
            	//"xs:complexType" was not pushed into tagList, so it is possible the end tag is "xs:complexType"
                if(strncmp("xs:complexType", ((osXmlTagInfo_t*)pLE->data)->tag.p, ((osXmlTagInfo_t*)pLE->data)->tag.l))
                {
                	logError("expect the end tag for xs:complexType, but %r is found.", &((osXmlTagInfo_t*)pLE->data)->tag);
                }
                else
                {
                	//the parsing for a complexType is done
                    if(!pCtInfo->typeName.l && !pParentElem)
                    {
                    	logInfo("parsed a complexType without name, and there is no parent element, pos=%ld.", pXmlBuf->pos);
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
            if(osPL_cmp(&((osXmlTagInfo_t*)pLE->data)->tag, &pTagInfo->tag) == 0)
            {
            	osXmlComplexType_getSubTagInfo(pCtInfo, (osXmlTagInfo_t*)pLE->data);
                continue;
            }
            else
            {
            	logError("input xml is invalid, error in (%r).", &pTagInfo->tag);
                goto EXIT;
            }
        }

        if(isTagDone)
        {
        	//no need to push to the tagList as the end tag is part of the line
            osXmlComplexType_getSubTagInfo(pCtInfo, pTagInfo);
        }
        else
        {
        	//special treatment for xs:element, as xs:element can contain own tags
            if(pTagInfo->tag.l == 10 && strncmp("xs:element", pTagInfo->tag.p, pTagInfo->tag.l) == 0)
            {
            	osXsdElement_t* pElem = osXmlElement_parse(pXmlBuf, pTagInfo);
                if(!pElem)
                {
                	logError("fails to osXmlElement_parse, pos=%ld.", pXmlBuf->pos);
                    goto EXIT;
                }

                osList_clear(&pTagInfo->attrNVList);
                pTagInfo->isPElement = true;
                pTagInfo->pElement = pElem;

                osXmlComplexType_getSubTagInfo(pCtInfo, pTagInfo);
            }
            else
            {
               	//add the beginning tag to the tagList, the info in the beginning tag will be processed when the end tag is met
                osList_append(&tagList, pTagInfo);
            }
        }
	}

EXIT:
	return pCtInfo;
}


//parse starts from the first tag after the <xs:element xxxx>
static osXsdElement_t* osXmlElement_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pElemTagInfo)
{
	osStatus_e status = OS_STATUS_OK;
    osList_t tagList = {};
	osXsdElement_t* pElement = NULL;

	if(!pXmlBuf || !pElemTagInfo)
	{
		logError("null pointer, pXmlBuf=%p, pTagInfo=%p.", pXmlBuf, pElemTagInfo);
		status = OS_ERROR_NULL_POINTER;
		goto EXIT;
	}

    pElement = osmalloc(sizeof(osXsdElement_t), NULL);
    osXmlElement_getAttrInfo(&pElemTagInfo->attrNVList, pElement);

	//starts to parse the sub tags of xs:element
    while(pXmlBuf->pos < pXmlBuf->end)
    {
    	bool isTagDone = false; //the tag is wrapped in one line, i.e., <tag, tag-content />
        bool isEndTag = false;  //the line is the end of tag, i.e., </tag>

        //get tag info for each tags inside xs:element
        osXmlTagInfo_t* pTagInfo = NULL;
		status = osXml_parseTag(pXmlBuf, false, &isTagDone, &isEndTag, &pTagInfo);
        if(status != OS_STATUS_OK || !pTagInfo)
        {
        	logError("fails to osXml_parseTag.");
            break;
        }

        if(isEndTag)
        {
            osListElement_t* pLE = osList_popTail(&tagList);
            if(!pLE)
            {
            	//"xs:element" was not pushed into tagList, so it is possible the end tag is "xs:element"
                if(((osXmlTagInfo_t*)pLE->data)->tag.l != 10 || strncmp("xs:element", ((osXmlTagInfo_t*)pLE->data)->tag.p, ((osXmlTagInfo_t*)pLE->data)->tag.l))
                {
                	logError("expect the end tag for xs:element, but %r is found.", &((osXmlTagInfo_t*)pLE->data)->tag);
                }

				goto EXIT;
			}

            //compare the end tag name (newly gotten) and the beginning tag name (from the LE)
            if(osPL_cmp(&((osXmlTagInfo_t*)pLE->data)->tag, &pTagInfo->tag) == 0)
            {
            	osXmlElement_getSubTagInfo(pElement, (osXmlTagInfo_t*)pLE->data);
                continue;
            }
            else
            {
                logError("input xml is invalid, error in (%r).", &pTagInfo->tag);
                goto EXIT;
            }
		}

        if(isTagDone)
        {
	        //no need to push to the tagList as the end tag is part of the line
            osXmlElement_getSubTagInfo(pElement, pTagInfo);
        }
        else
       	{
        	//special treatment for xs:complexType, as xs:complex can contain own tags
            if(pTagInfo->tag.l == 14 && strncmp("xs:complexType", pTagInfo->tag.p, pTagInfo->tag.l) == 0)
            {
				//the link between pElement and child type is done inside osXmlComplexType_parse()
	            osXmlComplexType_t* pCtInfo = osXmlComplexType_parse(pXmlBuf, pTagInfo, pElement);
	            if(!pCtInfo || pCtInfo->typeName.l)
    	        {
					//complex type within a element does not have type name
                	logError("complexType is NULL or has typeName, pCtInfo=%p, typeName=%r.", pCtInfo, &pCtInfo->typeName);
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
	                    char* ptr = NULL;
    	                int value = strtol(pNV->value.p, &ptr, 10);
        	            if(ptr)
            	        {
                	        logError("minOccurs(%r) is not numerical.", &pNV->value);
                    	}
						else
						{
							if(isMatchValue == 0)
							{
								pElement->minOccurs = value;
							}
							else
							{
								pElement->maxOccurs = value;
							}
						}
					}
                    pElement->elemName = pNV->value;
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


//isTagNameChecked = true, parse starts after tag name, = false, parse starts before <
//isTagDone == true, the tag is wrapped in one line, i.e., <tag, tag-content />
//isEndTag == true, the line is the end of tag, i.e., </tag>
static osStatus_e osXml_parseTag(osMBuf_t* pBuf, bool isTagNameChecked, bool* isTagDone, bool* isEndTag, osXmlTagInfo_t** ppTagInfo) 
{
	osStatus_e status = OS_STATUS_OK;

	if(!pBuf || !isTagDone || !isEndTag || ppTagInfo)
	{
		logError("null pointer, pBuf=%p, isTagDone=%p, isEndTag=%p, ppTagInfo=%p.", pBuf, isTagDone, isEndTag, ppTagInfo);
		status = OS_ERROR_NULL_POINTER; 
		goto EXIT;
	}

	osXmlTagInfo_t* pTagInfo = NULL;
	*isTagDone = false;
	*isEndTag = false;
	bool isGetTagInfoDone = false;
	osXmlNameValue_t* pnvPair = NULL;
	size_t tagPos = 0, nvStartPos = 0;
	osXmlCheckTagInfoState_e state = OS_XML_TAG_INFO_START;
	if(isTagNameChecked)
	{
		//check tagInfo starts after the tag
		state = OS_XML_TAG_INFO_CONTENT_NAME_START;
	}
	size_t origPos = pBuf->pos;
	while(pBuf->pos < pBuf->end)
	{
		switch(state)
		{
			case OS_XML_TAG_INFO_START:
				//if(pBuf->buf[pBuf->pos] == 0x20 || pBuf->buf[pBuf->pos] == '\n' || pBuf->buf[pBuf->pos] == '\t')
				//{
				//	continue;
				//}
				if(pBuf->buf[pBuf->pos] == '<')
				{
					state = OS_XML_TAG_INFO_TAG_START;
					tagPos = pBuf->pos+1;	//+1 means starts after '<'
				}
				else if(pBuf->buf[pBuf->pos] == '"')
				{
					state = OS_XML_TAG_INFO_BEFORE_TAG_INSIDE_QUOTE;
				}
				break;
			case OS_XML_TAG_INFO_BEFORE_TAG_INSIDE_QUOTE:
				if(pBuf->buf[pBuf->pos] == '"')
                {
                    state = OS_XML_TAG_INFO_START;
                }
                break;
			case OS_XML_TAG_INFO_TAG_START:
				if(pBuf->buf[pBuf->pos] == '/')
				{
					*isEndTag = true;
					state = OS_XML_TAG_INFO_TAG;
				}
				else if(pBuf->buf[pBuf->pos] == 0x20 || pBuf->buf[pBuf->pos] == '\n' || pBuf->buf[pBuf->pos] == '\t')
				{
					logError("white space follows <, pos = %ld.", pBuf->pos);
					goto EXIT;
				}
				break;
			case OS_XML_TAG_INFO_TAG:
				if(pBuf->buf[pBuf->pos] == 0x20 || pBuf->buf[pBuf->pos] == '\n' || pBuf->buf[pBuf->pos] == '\t')
				{
					pTagInfo = oszalloc(sizeof(osXmlTagInfo_t), NULL);
					pTagInfo->tag.p = &pBuf->buf[tagPos];
					pTagInfo->tag.l = pBuf->pos - tagPos;
					
					state = OS_XML_TAG_INFO_CONTENT_NAME_START;
				}
				else if(pBuf->buf[pBuf->pos] == '"')
				{
					logError("tag contains double quote, pos=%ld.", pBuf->pos);
					goto EXIT;
				}
				break;
			case OS_XML_TAG_INFO_CONTENT_NAME_START:
				if(pBuf->buf[pBuf->pos] == '"')
				{
					logError("attribute starts with double quote, pos=%ld.", pBuf->pos);
					goto EXIT;
				}
				else if(!OSXML_IS_LWS(pBuf->buf[pBuf->pos]))
                {
					if(isTagNameChecked)
					{
						//allocate a tagInfo for case when tag has already been checked before this funciton is called
	                    pTagInfo = oszalloc(sizeof(osXmlTagInfo_t), NULL);
					}
					pnvPair = oszalloc(sizeof(osPointerLen_t), NULL);
					pnvPair->name.p = &pBuf->buf[pBuf->pos];
					nvStartPos = pBuf->pos;

					//insert into pTagInfo->attrNVList
					osList_append(&pTagInfo->attrNVList, pnvPair);
					state = OS_XML_TAG_INFO_CONTENT_NAME;
				}
                else if(pBuf->buf[pBuf->pos] == '/')
                {
                    state = OS_XML_TAG_INFO_END_TAG_SLASH;
                }
				else if(pBuf->buf[pBuf->pos] == '>')
				{
					isGetTagInfoDone = true;
				}
				break;
			case OS_XML_TAG_INFO_CONTENT_NAME:
				if(pBuf->buf[pBuf->pos] == '"')
                {
                    logError("xml tag attribute name contains double quote, pos=%ld.", pBuf->pos);
                    goto EXIT;
                }
				else if(OSXML_IS_LWS(pBuf->buf[pBuf->pos]) || pBuf->buf[pBuf->pos] == '=')
				{
					pnvPair->name.l = pBuf->pos - nvStartPos;
					if(pBuf->buf[pBuf->pos] == '=')
					{
						 state = OS_XML_TAG_INFO_CONTENT_VALUE_START;
					}
					else
					{
						state = OS_XML_TAG_INFO_CONTENT_NAME_STOP;
					}
				}
				break;
			case OS_XML_TAG_INFO_CONTENT_NAME_STOP:
				if(pBuf->buf[pBuf->pos] == '=')
				{
					state = OS_XML_TAG_INFO_CONTENT_VALUE_START;
				}
				else if(!OSXML_IS_LWS(pBuf->buf[pBuf->pos]))
				{
					logError("expect =, but got char(%c).", pBuf->buf[pBuf->pos]);
					goto EXIT;
				}
				break;
			case OS_XML_TAG_INFO_CONTENT_VALUE_START:
				if(pBuf->buf[pBuf->pos] == '"')
				{
                    pnvPair->value.p = &pBuf->buf[pBuf->pos];
                    nvStartPos = pBuf->pos + 1;		//+1 to start after the current char "

					state = OS_XML_TAG_INFO_CONTENT_VALUE;
				}
                else if(!OSXML_IS_LWS(pBuf->buf[pBuf->pos]))
                {
                    logError("expect \", but got char(%c).", pBuf->buf[pBuf->pos]);
                    goto EXIT;
                }
                break;
			case OS_XML_TAG_INFO_CONTENT_VALUE:
				if(pBuf->buf[pBuf->pos] == '"')
                {
					pnvPair->value.l = pBuf->pos - nvStartPos;
					state = OS_XML_TAG_INFO_CONTENT_NAME_START;
				}
				break;
            case OS_XML_TAG_INFO_END_TAG_SLASH:
                if(pBuf->buf[pBuf->pos] == '>')
                {
                    *isTagDone = true;
                    isGetTagInfoDone = true;
                }
                else
                {
                    logError("expect > after /, but instead, char=%c.", pBuf->buf[pBuf->pos]);
                    goto EXIT;
                }
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
	//to-do, cleanup memory if error case
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


		
static bool osXml_listFoundMatchingType(osListElement_t* pLE, void* typeName)
{
	if(!pLE || !typeName)
	{
		logError("null pointer, pLE=%p, typeName=%p.", pLE, typeName);
		return false;
	}

	if(osPL_cmp((osPointerLen_t*)pLE->data, (osPointerLen_t*)typeName) == 0)
	{
		return true;
	}

	return false;
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

		logInfo("attribute type(%r) is ignored.", pTypeValue);
	}
	else
	{
		dataType = OS_XML_DATA_TYPE_NO_XS;
	}

EXIT:
	return dataType;
}
