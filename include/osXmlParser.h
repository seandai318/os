#ifndef _OS_XML_PARSER_H
#define _OS_XML_PARSER_H

#include "osList.h"


#define OS_XSD_COMPLEX_TYPE_MAX_ALLOWED_CHILD_ELEM	50
#define OSXML_IS_LWS(a) (!(a^0x20) || !(a^0x9) || !(a^0xa))
#define OSXML_IS_XS_TYPE(a) (a != NULL && a->l >=3 && !(a->p[0]^'x') && !(a->p[1]^'s') && !(a->p[2]^':'))
#define OSXML_IS_COMMENT_START(p) (*p=='<' && *(p+1)=='!' && *(p+2)=='-' && *(p+3)=='-')
#define OSXML_IS_COMMENT_STOP(p) (*p=='>' && *(p-1)=='-' && *(p-2)=='-')


typedef enum {
	OS_XML_TAG_XS_SCHEMA,
	OS_XML_TAG_XS_ELEMENT,
	OS_XML_TAG_XS_COMPLEX_TYPE,
	OS_XML_XS_ALL,
	OS_XML_XS_SEQUENCE,
	OS_XML_XS_CHOICE,
} osXmlTag_e;


typedef enum {
	OS_XML_DATA_TYPE_INVALID,
	OS_XML_DATA_TYPE_XS_BOOLEAN,
	OS_XML_DATA_TYPE_XS_SHORT,
	OS_XML_DATA_TYPE_XS_INTEGER,
	OS_XML_DATA_TYPE_XS_LONG,
	OS_XML_DATA_TYPE_XS_STRING,
	OS_XML_DATA_TYPE_NO_XS,		//a collective type that represents  all no XS defined type, like simple and complex
	OS_XML_DATA_TYPE_SIMPLE,
	OS_XML_DATA_TYPE_COMPLEX,
} osXmlDataType_e;


typedef enum {
	OS_XML_ELEMENT_DISP_TYPE_ALL,
	OS_XML_ELEMENT_DISP_TYPE_SEQUENCE,
	OS_XML_ELEMENT_DISP_TYPE_CHOICE,
} osXmlElemDispType_e;


typedef struct osXml_complexTypeInfo {
    osPointerLen_t typeName;
    bool isMixed;
	osXmlElemDispType_e elemDispType;
	osList_t elemList;		//each element is comprised of osXsdElement_t
} osXmlComplexType_t;


typedef struct osXsdElement {
	bool isRootElement;
	osPointerLen_t elemName;
	osPointerLen_t elemTypeName;
	int minOccurs;			//>=0
	int maxOccurs;			// -1 means unbounded
    bool isQualified;
    osPointerLen_t elemDefault;
    osPointerLen_t fixed;
	osXmlDataType_e dataType;
	osXmlComplexType_t* pComplex;
} osXsdElement_t;


typedef struct osXmlElement {
	osPointerLen_t name;
	osList_t* pChild;
    osXmlDataType_e dataType;	//if DataType != XS type, pChild=NULL, there is corresponding data inside union, otherwise, pChild != NULL, but there is no data inside union.
    union {
        bool isTrue;			//boolean
        int intValue;			//integer
        osPointerLen_t string;	//string
    };
} osXmlElement_t;


typedef void (*osXmlDataCallback_h)(osPointerLen_t* elemName, osPointerLen_t* value, osXmlDataType_e dataType);
typedef void (*osXsdElemCallback_h)(osXsdElement_t* pXsdElem, osXmlDataCallback_h callback);


osXsdElement_t* osXsd_parse(osMBuf_t* pXmlBuf);
osStatus_e osXml_parse(osMBuf_t* pBuf, osXsdElement_t* pXsdRootElem, osXmlDataCallback_h callback);
void osXsd_browseNode(osXsdElement_t* pXsdElem, osXsdElemCallback_h xsdElemCallback, osXmlDataCallback_h callback);


#endif
