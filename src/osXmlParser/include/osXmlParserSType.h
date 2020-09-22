/* Copyright <c) 2020, Sean Dai
 * xml parser for SimpleType
 */

#ifndef _OS_XML_PARSER_STYPE_H
#define _OS_XML_PARSER_STYPE_H

#include "osList.h"
#include "osTypes.h"
#include "osMBuf.h"

#include "osXmlParserData.h"


osXmlSimpleType_t* osXsdSimpleType_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pSimpleTagInfo, osXsdElement_t* pParentElem);
//osStatus_e osXsdSimpleType_getSubTagInfo(osXmlSimpleType_t* pSimpleInfo, osXmlTagInfo_t* pTagInfo);
osStatus_e osXmlSimpleType_convertData(osXmlSimpleType_t* pSimple, osPointerLen_t* pValue, osXmlData_t* pXmlData);



#endif
