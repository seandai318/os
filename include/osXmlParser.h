#ifndef _OS_XML_PARSER_H
#define _OS_XML_PARSER_H


#define OSXML_IS_LWS(a) (!(a^0x20) || !(a^0x9) || !(a^0xa))
#define OSXML_IS_XS_TYPE(a) (a != NULL && a->l >=3 && !(a->p[0]^'x') && !(a->p[1]^'s') && !(a->p[2]^':'))


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
	OS_XML_DATA_TYPE_XS_INTEGER,
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
	osList_t elemList;
} osXmlComplexType_t;


typedef struct osXsdElement {
	bool isRootElement;
	osPointerLen_t elemName;
	osPointerLen_t elemTypeName;
	int minOccurs;
	int maxOccurs;
    bool isQualified;
    osPointerLen_t elemDefault;
    osPointerLen_t fixed;
	osXmlDataType_e dataType;
	union {
		bool isTrue;
		int intValue;
		osPointerLen_t string;
		osXmlComplexType_t* pComplex;
	};
} osXsdElement_t;


osXsdElement_t* osXml_parseXsd(osMBuf_t* pXmlBuf);


#endif
