/* Copyright <c) 2020, Sean Dai
 * xml parser main function module
 */

#ifndef _OS_XML_PARSER_H
#define _OS_XML_PARSER_H

#include "osList.h"
#include "osTypes.h"
#include "osMBuf.h"

#include "osXmlParserData.h"


#define OS_XSD_COMPLEX_TYPE_MAX_ALLOWED_CHILD_ELEM	50
#define OSXML_IS_LWS(a) (!(a^0x20) || !(a^0x9) || !(a^0xa))
#define OSXML_IS_XS_TYPE(a) (a != NULL && a->l >=3 && !(a->p[0]^'x') && !(a->p[1]^'s') && !(a->p[2]^':'))
#define OSXML_IS_COMMENT_START(p) (*p=='<' && *(p+1)=='!' && *(p+2)=='-' && *(p+3)=='-')
#define OSXML_IS_COMMENT_STOP(p) (*p=='>' && *(p-1)=='-' && *(p-2)=='-')

#define OS_XML_MAX_FILE_NAME_SIZE	160		//the maximum xml and xsd file name length


void osXsd_elemCallback(osXsdElement_t* pXsdElem, osXmlDataCallbackInfo_t* callbackInfo);
static osStatus_e osXml_parseTag(osMBuf_t* pBuf, bool isTagNameChecked, bool isXsdFirstTag, osXmlTagInfo_t** ppTagInfo, size_t* tagStartPos);
static osXsdElement_t* osXsd_parseElement(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pTagInfo);
static osStatus_e osXmlElement_getAttrInfo(osList_t* pAttrList, osXsdElement_t* pElement);
static bool osXml_findWhiteSpace(osMBuf_t* pBuf, bool isAdvancePos);
static bool osXml_findPattern(osMBuf_t* pXmlBuf, osPointerLen_t* pPattern, bool isAdvancePos);
static bool osXml_isXsdElemSimpleType(osXsdElement_t* pXsdElem);




#endif
