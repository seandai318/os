/********************************************************
 * Copyright (C) 2020 Sean Dai
 *
 * @file osXmlParserCommon.c
 * provide common functions for xml/xsd parser
 ********************************************************/

#include"string.h"

#include "osMemory.h"
#include "osXmlParserData.h"
#include "osXmlParserCommon.h"
#include "osXmlParser.h"
#include "osXsdParser.h"
#include "osXmlMisc.h"


#define OSXML_IS_COMMENT_START(p) (*p=='<' && *(p+1)=='!' && *(p+2)=='-' && *(p+3)=='-')
#define OSXML_IS_COMMENT_STOP(p) (*p=='>' && *(p-1)=='-' && *(p-2)=='-')


static void osXmlTagInfo_cleanup(void* data);


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
                //  continue;
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
                        if(OSXML_IS_COMMENT_START(&pBuf->buf[pBuf->pos]) && pBuf->pos+7 <= pBuf->end)   //comment is at least 7 char <!---->
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
                        tagPos = pBuf->pos+1;   //+1 means starts after '<'
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
                    tagPos++;       //move back one space for tagPos
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
                    pnvPair->value.p = &pBuf->buf[pBuf->pos+1];     //+1 to start after the current char "
                    nvStartPos = pBuf->pos + 1;                     //+1 to start after the current char "

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


//parse <?xml version="1.0" encoding="UTF-8"?>
osStatus_e osXml_parseFirstTag(osMBuf_t* pXmlBuf)
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

    pXmlData->dataType = dataType;

    return status;
}


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
//      osfree(pTagInfo->pElement);
    }
    else
    {
        osList_delete(&pTagInfo->attrNVList);
    }
}

