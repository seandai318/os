/********************************************************
 * Copyright (C) 2020 Sean Dai
 *
 * @file osXmlParserSType.c
 * xsd and xml parser for SimpleType
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
#include "osXmlParserData.h"
#include "osXmlParserSType.h"
#include "osXmlParserCommon.h"
#include "osXmlMisc.h"


#define OS_XML_COMPLEXTYPE_LEN	14
#define OS_XML_SIMPLETYPE_LEN	13
#define OS_XML_ELEMENT_LEN 		10
#define OS_XML_SCHEMA_LEN		9



static osStatus_e osXsdSimpleType_getSubTagInfo(osXmlSimpleType_t* pSimpleInfo, osXmlTagInfo_t* pTagInfo);
static osStatus_e osXsdSimpleType_getAttrInfo(osList_t* pAttrList, osXmlSimpleType_t* pSInfo);
static osXmlRestrictionFacet_t* osXsdSimpleType_getFacet(osXmlRestrictionFacet_e facetType, osXmlDataType_e baseType, osXmlTagInfo_t* pTagInfo);
static bool osXml_isXSSimpleType(osXmlDataType_e dataType);
static bool osXml_isDigitType(osXmlDataType_e dataType);
static void osXmlSimpleType_cleanup(void* data);


/* parse <xxx> between <xs:simpleType> and </xs:simpleType>, like <<xs:restriction>
 * only support baseType being a XS simple type 
*/
static osStatus_e osXsdSimpleType_getSubTagInfo(osXmlSimpleType_t* pSimpleInfo, osXmlTagInfo_t* pTagInfo)
{
    osStatus_e status = OS_STATUS_OK;

    if(!pSimpleInfo || !pTagInfo)
    {
        logError("null pointer, pSimpleInfo=%p, pTagInfo=%p.", pSimpleInfo, pTagInfo);
        status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

	osXmlRestrictionFacet_e facetType = OS_XML_RESTRICTION_FACET_UNKNOWN;
	osPointerLen_t* pXsAlias = osXsd_getXSAlias();

    //add to the simpleTypeInfo data structure
    switch(pTagInfo->tag.l - pXsAlias->l)
    {
		case 5:		//len=8, "xs:union"
			//this function deals with simpleType facet except for this one and "xs:restriction".  Special handling.
			if(strncmp("union", &pTagInfo->tag.p[pXsAlias->l], 5) == 0)
            {
				//we do not further parse the union member, just assign the base type to be xs:string
				pSimpleInfo->baseType = OS_XML_DATA_TYPE_XS_STRING;
 			}
			break;
		case 6:		//len=9, "xs:length"
			if(strncmp("length", &pTagInfo->tag.p[pXsAlias->l], 6) == 0)
			{
				facetType = OS_XML_RESTRICTION_FACET_LENGTH;
			}
			break;
		case 7:	//len=10, "xs:pattern"
			if(strncmp("pattern", &pTagInfo->tag.p[pXsAlias->l], 7) == 0)
			{
                facetType = OS_XML_RESTRICTION_FACET_PATTERN;
            }
            break;
		case 9:	//len=12, "xs:minLength", "xs:maxLength"
            if(pTagInfo->tag.p[pXsAlias->l+1] == 'i' && strncmp("minLength", &pTagInfo->tag.p[pXsAlias->l], 9) == 0)
            {
                facetType = OS_XML_RESTRICTION_FACET_MIN_LENGTH;
            }
			else if(pTagInfo->tag.p[pXsAlias->l+1] == 'a' &&strncmp("maxLength", &pTagInfo->tag.p[pXsAlias->l], 9) == 0)
            {
                facetType = OS_XML_RESTRICTION_FACET_MAX_LENGTH;
            }
            break;
		case 10:	//len=13, "xs:whiteSpace"
			if(strncmp("whiteSpace", &pTagInfo->tag.p[pXsAlias->l], 10) == 0)
            {
                facetType = OS_XML_RESTRICTION_FACET_WHITE_SPACE;
            }
            break;
        case 11:         //len=14, "xs:restriction", "xs:totalDigits", "xs:enumeration"
			switch(pTagInfo->tag.p[pXsAlias->l])
			{
				case 'r':
					//this function deals with simpleType facet except for this one and "xs:union".  Special handling.
					if(strncmp("restriction", &pTagInfo->tag.p[pXsAlias->l], 11) == 0)
            		{
					    osListElement_t* pLE = pTagInfo->attrNVList.head;
    					while(pLE)
    					{
        					if(osPL_strcmp(&((osXmlNameValue_t*)pLE->data)->name, "base") == 0)
        					{
								pSimpleInfo->baseType = osXsd_getElemDataType(&((osXmlNameValue_t*)pLE->data)->value);
								break;
							}
								
							pLE = pLE->next;
						}

						if(!osXml_isXSSimpleType(pSimpleInfo->baseType))
						{
							logError("a simpleType's base type(%d) is invalid or not XS simple type.", pSimpleInfo->baseType);
							status = OS_ERROR_INVALID_VALUE;
							goto EXIT;
						}
            		}
					break;
				case 't':
					if(strncmp("totalDigits", &pTagInfo->tag.p[pXsAlias->l], 11) == 0)
					{
						facetType = OS_XML_RESTRICTION_FACET_TOTAL_DIGITS;
					}
					break;
				case 'e':
					if(strncmp("enumeration", &pTagInfo->tag.p[pXsAlias->l], 11) == 0)
                    {
						facetType = OS_XML_RESTRICTION_FACET_ENUM;
					}
                    break;
				default:
                    break;
			}
            break;
		case 12:	//len=15, "xs:minExclusive", "xs:minInclusive", "xs:maxExclusive", "xs:maxInclusive"
			switch(pTagInfo->tag.p[pXsAlias->l+1])
			{
				case 'i':
					if(pTagInfo->tag.p[pXsAlias->l+3] == 'E' && strncmp("minExclusive", &pTagInfo->tag.p[pXsAlias->l], 12) == 0)
					{
						facetType = OS_XML_RESTRICTION_FACET_MIN_EXCLUSIVE;
					}
					else if(pTagInfo->tag.p[pXsAlias->l+3] == 'I' && strncmp("minInclusive", &pTagInfo->tag.p[pXsAlias->l], 12) == 0)
					{
						facetType = OS_XML_RESTRICTION_FACET_MIN_INCLUSIVE;
					}
					break;
				case 'a':
                	if(pTagInfo->tag.p[pXsAlias->l+3] == 'E' && strncmp("maxExclusive", &pTagInfo->tag.p[pXsAlias->l], 12) == 0)
                	{
						facetType = OS_XML_RESTRICTION_FACET_MAX_EXCLUSIVE;
					}
                	else if(pTagInfo->tag.p[pXsAlias->l+3] == 'I' && strncmp("maxInclusive", &pTagInfo->tag.p[pXsAlias->l], 12) == 0)
                	{
						facetType = OS_XML_RESTRICTION_FACET_MAX_INCLUSIVE;
                	}
					break;
				default:
                	break;
            }
			break;
        case 14:         //len=17, "xs:fractionDigits"
            if(strncmp("fractionDigits", &pTagInfo->tag.p[pXsAlias->l], 14) == 0)
            {
				facetType = OS_XML_RESTRICTION_FACET_FRACTION_DIGITS;
            }
            break;
        default:
            break;
    }

	if(facetType != OS_XML_RESTRICTION_FACET_UNKNOWN)
	{
		//add to the facet to simpleTypeInfo data structure
		osXmlRestrictionFacet_t* pFacet = osXsdSimpleType_getFacet(facetType, pSimpleInfo->baseType, pTagInfo);
        if(pFacet)
        {
			osList_append(&pSimpleInfo->facetList, pFacet);
			goto EXIT;
		}
		status = OS_ERROR_INVALID_VALUE;
	}

    mlogInfo(LM_XMLP, "xml tag(%r) is ignored.", &pTagInfo->tag);

EXIT:
    return status;
}



/* for now, only support xs:restriction, no xs:list, no xs:union. do not support derivation, as such, no check for attribute final
 */
osXmlSimpleType_t* osXsdSimpleType_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pSimpleTagInfo, osXsdElement_t* pParentElem)
{
    osStatus_e status = OS_STATUS_OK;

    osList_t tagList = {};
    osXmlSimpleType_t* pSimpleInfo = NULL;
    osXmlTagInfo_t* pTagInfo = NULL;

    if(!pXmlBuf || !pSimpleTagInfo)
    {
        logError("null pointer, pXmlBuf=%p, pSimpleTagInfo=%p.", pXmlBuf, pSimpleTagInfo);
        status = OS_ERROR_NULL_POINTER;
        goto EXIT;
    }

    pSimpleInfo = osmalloc(sizeof(osXmlSimpleType_t), osXmlSimpleType_cleanup);
    osXsdSimpleType_getAttrInfo(&pSimpleTagInfo->attrNVList, pSimpleInfo);

    while(pXmlBuf->pos < pXmlBuf->end)
    {
        //get tag info for each tags inside xs:simpleType
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
                //"xs:simpleType" was not pushed into tagList, so it is possible the end tag is "xs:simpleType"
                if(osXml_xsTagCmp("simpleType", 10, &pTagInfo->tag))
                {
                    logError("expect the end tag for xs:simpleType, but %r is found.", &((osXmlTagInfo_t*)pLE->data)->tag);
                    status = OS_ERROR_INVALID_VALUE;
                }
                else
                {
                    //the parsing for a simpleType is done
                    if(!pSimpleInfo->typeName.l && !pParentElem)
                    {
                        mlogInfo(LM_XMLP, "parsed a simpleType without type name, and there is no parent element, pos=%ld.", pXmlBuf->pos);
                    }

                    if(pParentElem)
                    {
                        //if the simpleType is embedded inside a element, directly assign the simpleType to the parent element
                        if(osPL_cmp(&pParentElem->elemTypeName, &pSimpleInfo->typeName) == 0 || pParentElem->elemTypeName.l == 0)
                        {
                            pParentElem->dataType = OS_XML_DATA_TYPE_SIMPLE;
                            pParentElem->pSimple = pSimpleInfo;
                        }
                    }
                }
                goto EXIT;
            }

            //compare the end tag name (newly gotten) and the beginning tag name (from the LE)
            mdebug(LM_XMLP, "pTagInfo->tag=%r, pLE->data)->tag=%r", &pTagInfo->tag, &((osXmlTagInfo_t*)pLE->data)->tag);
            if(osPL_cmp(&((osXmlTagInfo_t*)pLE->data)->tag, &pTagInfo->tag) == 0)
            {
               // osXsdSimpleType_getSubTagInfo(pSimpleInfo, (osXmlTagInfo_t*)pLE->data);
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
            osXsdSimpleType_getSubTagInfo(pSimpleInfo, pTagInfo);
            pTagInfo = osfree(pTagInfo);
        }
        else
        {
			//perform tag parse as soon as a value is gotten.  This is necessary especially for xs:restriction, as baseType is needed for other tag's parsing
            osXsdSimpleType_getSubTagInfo(pSimpleInfo, pTagInfo);

            //add the beginning tag to the tagList, the info in the beginning tag will be processed when the end tag is met
            osList_append(&tagList, pTagInfo);
        }
    }

EXIT:
    if(status != OS_STATUS_OK)
    {
        osfree(pSimpleInfo);
        pSimpleInfo = NULL;
    }

    if(pTagInfo)
    {
        osfree(pTagInfo);
    }

	mdebug(LM_XMLP, "simpleType(%r) is passed, baseType=%d.", &pSimpleInfo->typeName, pSimpleInfo->baseType);

    return pSimpleInfo;
}
                                                                                                                        

static osStatus_e osXsdSimpleType_getAttrInfo(osList_t* pAttrList, osXmlSimpleType_t* pSInfo)
{
    osStatus_e status = OS_STATUS_OK;
    if(!pSInfo || !pAttrList)
    {
        logError("null pointer, pSInfo=%p, pAttrList=%p.", pSInfo, pAttrList);
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
                    pSInfo->typeName = pNV->value;
                    isIgnored = false;
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


static osXmlRestrictionFacet_t* osXsdSimpleType_getFacet(osXmlRestrictionFacet_e facetType, osXmlDataType_e baseType, osXmlTagInfo_t* pTagInfo)
{
    bool isNumerical = true;
    osXmlRestrictionFacet_t* pFacet = NULL;

    if(pTagInfo->isPElement)
    {
        logError("pTagInfo->isPElement is true, not contain NV list.");
        goto EXIT;
    }

    switch(facetType)
    {
        case OS_XML_RESTRICTION_FACET_LENGTH:
        case OS_XML_RESTRICTION_FACET_MIN_LENGTH:
        case OS_XML_RESTRICTION_FACET_MAX_LENGTH:
            if(osXml_isDigitType(baseType))
            {
                goto EXIT;
            }
            break;
        case OS_XML_RESTRICTION_FACET_MIN_INCLUSIVE:
        case OS_XML_RESTRICTION_FACET_MAX_INCLUSIVE:
        case OS_XML_RESTRICTION_FACET_MIN_EXCLUSIVE:
        case OS_XML_RESTRICTION_FACET_MAX_EXCLUSIVE:
        case OS_XML_RESTRICTION_FACET_TOTAL_DIGITS:
        case OS_XML_RESTRICTION_FACET_FRACTION_DIGITS:
            if(!osXml_isDigitType(baseType))
            {
                goto EXIT;
            }
            break;
        case OS_XML_RESTRICTION_FACET_PATTERN:       //regular expression pattern
            if(osXml_isDigitType(baseType))
            {
                goto EXIT;
            }
            isNumerical = false;
            break;
        case OS_XML_RESTRICTION_FACET_ENUM:          //can be value or string, depending on the restriction base type
            isNumerical = osXml_isDigitType(baseType) ? true : false;
            break;
        case OS_XML_RESTRICTION_FACET_WHITE_SPACE:   //"preserve", "replace", "collapse"
        default:
            mlogInfo(LM_XMLP, "facet type(%d) is ignored", facetType);
            goto EXIT;
    }

    pFacet = osmalloc(sizeof(osXmlRestrictionFacet_t), NULL);
    if(!pFacet)
    {
        logError("fails to osmalloc for pFacet.");
        goto EXIT;
    }

    pFacet->facet = facetType;

    osListElement_t* pLE = pTagInfo->attrNVList.head;
    while(pLE)
    {
        if(osPL_strcmp(&((osXmlNameValue_t*)pLE->data)->name, "value") == 0)
        {
            if(isNumerical)
            {
                if(osPL_convertStr2u64(&((osXmlNameValue_t*)pLE->data)->value, &pFacet->value, NULL) != OS_STATUS_OK)
                {
                    logError("expect a numerical value for a facet(%d), but the real value is(%r).", facetType, &((osXmlNameValue_t*)pLE->data)->value);
                    goto EXIT;
				}
            }
            else
            {
                pFacet->string = ((osXmlNameValue_t*)pLE->data)->value;
            }
            goto EXIT;
        }

        pLE = pLE->next;
    }

EXIT:
    return pFacet;
}



/* perform sanity check of the simpleType data from an xml input against the facets of a pSimple object from the xsd.  
 * A xs:simpleType is converted to the simpleType's baseType, the data value will also be set in pXmlData as an output.
 *
 * pValue: the data value gotten from xml input
 * pSimple: a osXmlSimpleType_t object that was parsed based on xsd
 * pXmlData: the output to xml user after the xml parsing, pXmlData->dataType and pXmlData->xmlInt or pXmlData->xmlStr will be set
 */ 
osStatus_e osXmlSimpleType_convertData(osXmlSimpleType_t* pSimple, osPointerLen_t* pValue, osXmlData_t* pXmlData)
{
	osStatus_e status = OS_STATUS_OK;
	pXmlData->dataType = pSimple->baseType;

	switch(pSimple->baseType)
	{
		case OS_XML_DATA_TYPE_XS_BOOLEAN:
			status = osXmlXSType_convertData(&pXmlData->dataName, pValue, OS_XML_DATA_TYPE_XS_BOOLEAN, pXmlData);
			break;
        case OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE:
    	case OS_XML_DATA_TYPE_XS_SHORT:
    	case OS_XML_DATA_TYPE_XS_INTEGER:
    	case OS_XML_DATA_TYPE_XS_LONG:
		{
			int digitNum=0;
			int isEnumMatch = -1;		//-1: not an enum simpleType, 0: an enum simpleType, but match has not found, 1: an enum simpleType, and match found
			if(osPL_convertStr2u64(pValue, &pXmlData->xmlInt, &digitNum) != OS_STATUS_OK)
            {
				pXmlData->dataType = OS_XML_DATA_TYPE_INVALID;
				status = OS_ERROR_INVALID_VALUE;
            	logError("falis to convert simpleType(%r) value(%r).", &pSimple->typeName, pValue);
                goto EXIT;
			}

			osListElement_t* pLE = pSimple->facetList.head;
			while(pLE)
			{
				switch(((osXmlRestrictionFacet_t*)pLE->data)->facet)
				{
    				case OS_XML_RESTRICTION_FACET_ENUM:          //can be value or string, depending on the restriction base type
						if(isEnumMatch == -1)
						{
							isEnumMatch = 0;
						}
						if(isEnumMatch != 1 && pXmlData->xmlInt == ((osXmlRestrictionFacet_t*)pLE->data)->value)
						{
							isEnumMatch=1;
						}
						break;
    				case OS_XML_RESTRICTION_FACET_MIN_INCLUSIVE:
						if(pXmlData->xmlInt < ((osXmlRestrictionFacet_t*)pLE->data)->value)
						{
							pXmlData->dataType = OS_XML_DATA_TYPE_INVALID;
			                status = OS_ERROR_INVALID_VALUE;
							logError("pXmlData->xmlInt(%ld) < OS_XML_RESTRICTION_FACET_MIN_INCLUSIVE(%ld).", pXmlData->xmlInt, ((osXmlRestrictionFacet_t*)pLE->data)->value);
			                goto EXIT;
            			}
						break;
					case OS_XML_RESTRICTION_FACET_MAX_INCLUSIVE:
                        if(pXmlData->xmlInt > ((osXmlRestrictionFacet_t*)pLE->data)->value)
                        {
                            pXmlData->dataType = OS_XML_DATA_TYPE_INVALID;
			                status = OS_ERROR_INVALID_VALUE;
                            logError("pXmlData->xmlInt(%ld) > OS_XML_RESTRICTION_FACET_MAX_INCLUSIVE(%ld).", pXmlData->xmlInt, ((osXmlRestrictionFacet_t*)pLE->data)->value);
                            goto EXIT;
                        }
                        break;
                    case OS_XML_RESTRICTION_FACET_MIN_EXCLUSIVE:
                        if(pXmlData->xmlInt <= ((osXmlRestrictionFacet_t*)pLE->data)->value)
                        {
                            pXmlData->dataType = OS_XML_DATA_TYPE_INVALID;
                            status = OS_ERROR_INVALID_VALUE;
                            logError("pXmlData->xmlInt(%ld) <= OS_XML_RESTRICTION_FACET_MIN_EXCLUSIVE(%ld).", pXmlData->xmlInt, ((osXmlRestrictionFacet_t*)pLE->data)->value);
                            goto EXIT;
                        }
                        break;
    				case OS_XML_RESTRICTION_FACET_MAX_EXCLUSIVE:
                        if(pXmlData->xmlInt >= ((osXmlRestrictionFacet_t*)pLE->data)->value)
                        {
                            pXmlData->dataType = OS_XML_DATA_TYPE_INVALID;
                            status = OS_ERROR_INVALID_VALUE;
                            logError("pXmlData->xmlInt(%ld) >= OS_XML_RESTRICTION_FACET_MAX_EXCLUSIVE(%ld).", pXmlData->xmlInt, ((osXmlRestrictionFacet_t*)pLE->data)->value);
                            goto EXIT;
                        }
                        break;
    				case OS_XML_RESTRICTION_FACET_TOTAL_DIGITS:
						//only check no decimal value
						if(digitNum > ((osXmlRestrictionFacet_t*)pLE->data)->value)
                        {
                            pXmlData->dataType = OS_XML_DATA_TYPE_INVALID;
                            status = OS_ERROR_INVALID_VALUE;
                            logError("pXmlData->xmlInt(%ld) digit num(%d) > OS_XML_RESTRICTION_FACET_TOTAL_DIGITS(%ld).", pXmlData->xmlInt, digitNum, ((osXmlRestrictionFacet_t*)pLE->data)->value);
                            goto EXIT;
                        }
                        break;
    				case OS_XML_RESTRICTION_FACET_FRACTION_DIGITS:
					default:
						logInfo("simpleType base type=%d, unsupported facet(%d), ignore.", pSimple->baseType, ((osXmlRestrictionFacet_t*)pLE->data)->facet);
						break;
				}
				
				pLE = pLE->next;
			} //while(pLE)

			//an enum simpleType, but the xml data does not match in xsd
			if(isEnumMatch == 0)
			{
				pXmlData->dataType = OS_XML_DATA_TYPE_INVALID;
                status = OS_ERROR_INVALID_VALUE;
                logError("pXmlData->xmlInt(%ld) is an enum, but does not match enum value in xsd.", pXmlData->xmlInt);
                goto EXIT;
            }

            mlogInfo(LM_XMLP, "xmlData.dataName=%r, value=%r", &pXmlData->dataName, pValue);

			break;
		}	//case OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE etc.
    	case OS_XML_DATA_TYPE_XS_STRING: 	//xs:anyURI falls in this enum
		{
            int isEnumMatch = -1;       //-1: not an enum simpleType, 0: an enum simpleType, but match has not found, 1: an enum simpleType, and match found
			pXmlData->xmlStr = *pValue;

            osListElement_t* pLE = pSimple->facetList.head;
            while(pLE)
            {
                switch(((osXmlRestrictionFacet_t*)pLE->data)->facet)
                {
        			case OS_XML_RESTRICTION_FACET_LENGTH:
						if(pValue->l != ((osXmlRestrictionFacet_t*)pLE->data)->value)
						{
							pXmlData->dataType = OS_XML_DATA_TYPE_INVALID;
                            status = OS_ERROR_INVALID_VALUE;
							logError("pXmlData->xmlStr length(%ld) does not equal to XSD length facet(%ld).", pValue->l, ((osXmlRestrictionFacet_t*)pLE->data)->value);
							goto EXIT;
						}
						break;
            		case OS_XML_RESTRICTION_FACET_MIN_LENGTH:
						if(pValue->l < ((osXmlRestrictionFacet_t*)pLE->data)->value)
						{
							pXmlData->dataType = OS_XML_DATA_TYPE_INVALID;
                            status = OS_ERROR_INVALID_VALUE;
                    		logError("pXmlData->xmlStr length(%ld) < OS_XML_RESTRICTION_FACET_MIN_LENGTH(%ld).", pValue->l, ((osXmlRestrictionFacet_t*)pLE->data)->value);
                            goto EXIT;
                        }
                        break;
            		case OS_XML_RESTRICTION_FACET_MAX_LENGTH:
                        if(pValue->l > ((osXmlRestrictionFacet_t*)pLE->data)->value)
                        {
                            pXmlData->dataType = OS_XML_DATA_TYPE_INVALID;
                            status = OS_ERROR_INVALID_VALUE;
                            logError("pXmlData->xmlStr length(%ld) > OS_XML_RESTRICTION_FACET_MAX_LENGTH(%ld).", pValue->l, ((osXmlRestrictionFacet_t*)pLE->data)->value);
                            goto EXIT;
                        }
                        break;
                    case OS_XML_RESTRICTION_FACET_ENUM:          //can be value or string, depending on the restriction base type
                        if(isEnumMatch == -1)
                        {
                            isEnumMatch = 0;
                        }
                        if(isEnumMatch != 1 && osPL_cmp(&pXmlData->xmlStr, &((osXmlRestrictionFacet_t*)pLE->data)->string) == 0)
                        {
                            isEnumMatch=1;
                        }
						break;
            		case OS_XML_RESTRICTION_FACET_PATTERN:       //regular expression pattern
            		case OS_XML_RESTRICTION_FACET_WHITE_SPACE:   //"preserve", "replace", "collapse"
					default:
                        logInfo("simpleType base type=%d, unsupported facet(%d), ignore.", pSimple->baseType, ((osXmlRestrictionFacet_t*)pLE->data)->facet);
                        break;
                }

                pLE = pLE->next;
            }	//while(pLE)

            //an enum simpleType, but the xml data does not match in xsd
            if(isEnumMatch == 0)
            {
                pXmlData->dataType = OS_XML_DATA_TYPE_INVALID;
                status = OS_ERROR_INVALID_VALUE;
                logError("pXmlData->xmlStr(%r) is an enum, but does not match enum value in xsd.", &pXmlData->xmlStr);
                goto EXIT;
            }

            mlogInfo(LM_XMLP, "xmlData.dataName=%r, value=%r", &pXmlData->dataName, pValue);

            break;
        }	//case OS_XML_DATA_TYPE_XS_STRING
	} 	//switch(pSimple->baseType)

EXIT:
	return status;
}


static void osXmlSimpleType_cleanup(void* data)
{
	if(!data)
	{
		return;
	}

	osXmlSimpleType_t* pSimple = data;
	osList_delete(&pSimple->facetList);
}


static bool osXml_isDigitType(osXmlDataType_e dataType)
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


static bool osXml_isXSSimpleType(osXmlDataType_e dataType)
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
