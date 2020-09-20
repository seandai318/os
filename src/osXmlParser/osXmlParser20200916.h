/* Copyright <c) 2020, Sean Dai
 * xml parser interface
 */

#ifndef _OS_XML_PARSER_INTF_H
#define _OS_XML_PARSER_INTF_H

#include "osList.h"
#include "osTypes.h"
#include "osMBuf.h"


#define OS_XSD_COMPLEX_TYPE_MAX_ALLOWED_CHILD_ELEM	50
#define OSXML_IS_LWS(a) (!(a^0x20) || !(a^0x9) || !(a^0xa))
#define OSXML_IS_XS_TYPE(a) (a != NULL && a->l >=3 && !(a->p[0]^'x') && !(a->p[1]^'s') && !(a->p[2]^':'))
#define OSXML_IS_COMMENT_START(p) (*p=='<' && *(p+1)=='!' && *(p+2)=='-' && *(p+3)=='-')
#define OSXML_IS_COMMENT_STOP(p) (*p=='>' && *(p-1)=='-' && *(p-2)=='-')

#define OS_XML_MAX_FILE_NAME_SIZE	160		//the maximum xml and xsd file name length

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
	OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE,
	OS_XML_DATA_TYPE_XS_SHORT,
	OS_XML_DATA_TYPE_XS_INTEGER,
	OS_XML_DATA_TYPE_XS_LONG,
	OS_XML_DATA_TYPE_XS_STRING,	//xs:anyURI falls in this enum
	OS_XML_DATA_TYPE_NO_XS,		//a collective type that represents  all no XS defined type, like simple and complex
	OS_XML_DATA_TYPE_SIMPLE,
	OS_XML_DATA_TYPE_COMPLEX,
} osXmlDataType_e;


typedef enum {
	OS_XML_RESTRICTION_FACET_UNKNOWN,
	OS_XML_RESTRICTION_FACET_LENGTH,
	OS_XML_RESTRICTION_FACET_MIN_LENGTH,
	OS_XML_RESTRICTION_FACET_MAX_LENGTH,
	OS_XML_RESTRICTION_FACET_PATTERN,		//regular expression pattern
	OS_XML_RESTRICTION_FACET_ENUM,			//can be value or string, depending on the restriction base type
	OS_XML_RESTRICTION_FACET_MIN_INCLUSIVE,
	OS_XML_RESTRICTION_FACET_MAX_INCLUSIVE,
	OS_XML_RESTRICTION_FACET_MIN_EXCLUSIVE,
	OS_XML_RESTRICTION_FACET_MAX_EXCLUSIVE,
	OS_XML_RESTRICTION_FACET_WHITE_SPACE,	//"preserve", "replace", "collapse"
	OS_XML_RESTRICTION_FACET_TOTAL_DIGITS,
	OS_XML_RESTRICTION_FACET_FRACTION_DIGITS,
} osXmlRestrictionFacet_e;


typedef enum {
	OS_XML_RES_FACET_WHITE_SPACE_PRESERVE,	//"preserve", no touch to the white space
	OS_XML_RES_FACET_WHITE_SPACE_REPLACE,	//"replace", REPLACE all white space characters (line feeds, tabs, spaces, and carriage returns) with spaces
 	OS_XML_RES_FACET_WHITE_SPACE_COLLAPSE,	//""collapse", line feeds, tabs, spaces, carriage returns are replaced with spaces, leading and trailing spaces are removed, and multiple spaces are reduced to a single space
} osXmlRestrictionFacetWSAction_e;


typedef struct {
    osXmlRestrictionFacet_e facet;
    union {
        uint64_t value;
        osPointerLen_t string;  //used when facet = OS_XML_RESTRICTION_FACET_PATTERN, or OS_XML_RESTRICTION_FACET_ENUM (when base=xs:string)
    };
} osXmlRestrictionFacet_t;

typedef struct {
    osPointerLen_t typeName;	//must be in the beginning of this data structure, for the use in osXsd_getTypeByname()
	osXmlDataType_e baseType;
	osList_t facetList;			//each element is a osXmlRestrictionFacet_t
} osSimpleType_t;


typedef struct {
    int eDataName;          //different app use different enum, like sipConfig_xmlDataName_e, diaConfig_xmlDataName_e, etc.
    osPointerLen_t dataName;
    osXmlDataType_e dataType;	 //for simpleType, this is set to OS_XML_DATA_TYPE_SIMPLE by user, and when callback, set to the XS type (like int, etc.)by the xmlparser
    union {
        bool xmlIsTrue;
        uint64_t xmlInt;
        osPointerLen_t xmlStr;
    };
} osXmlData_t;


typedef enum {
	OS_XML_ELEMENT_DISP_TYPE_ALL,
	OS_XML_ELEMENT_DISP_TYPE_SEQUENCE,
	OS_XML_ELEMENT_DISP_TYPE_CHOICE,
} osXmlElemDispType_e;


typedef struct osXml_complexTypeInfo {
    osPointerLen_t typeName;	//must be in the beginning of this data structure, for the use in osXsd_getTypeByname()
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
	uinon {
		osXmlComplexType_t* pComplex;
		osSimpleType_t* pSimple;
	};
	osList_t* pSimpleTypeList;	//each element contains osSimpleType_t, all xsdElement points to the same osList
} osXsdElement_t;



//app provided info for osXmlDataCallback_h callback
typedef struct {
	osXmlData_t* xmlData;
	int maxXmlDataSize;
} osXmlDataCallbackInfo_t;


//callback function to provide xml element value to the application
typedef osStatus_e (*osXmlDataCallback_h)(osPointerLen_t* elemName, osPointerLen_t* value, osXmlDataType_e dataType, osXmlDataCallbackInfo_t* callbackInfo);

typedef void (*osXsdElemCallback_h)(osXsdElement_t* pXsdElem, osXmlDataCallback_h callback, osXmlDataCallbackInfo_t* callbackInfo);


osXsdElement_t* osXsd_parse(osMBuf_t* pXmlBuf);
osStatus_e osXml_parse(osMBuf_t* pBuf, osXsdElement_t* pXsdRootElem, osXmlDataCallback_h callback, osXmlDataCallbackInfo_t* callbackInfo);
//get xml leaf node value based on the xsd and xml files
osStatus_e osXml_getLeafValue(char* fileFolder, char* xsdFileName, char* xmlFileName, osXmlDataCallback_h callback, osXmlDataCallbackInfo_t* callbackInfo);
//generic xml callback implementation.  app can chose to use this one or provide its own callback function, depending on the needs
osStatus_e osXml_xmlCallback(osPointerLen_t* elemName, osPointerLen_t* value, osXmlDataType_e dataType, osXmlDataCallbackInfo_t* callbackInfo);
void osXsd_browseNode(osXsdElement_t* pXsdElem, osXsdElemCallback_h xsdElemCallback, osXmlDataCallback_h callback, osXmlDataCallbackInfo_t* callbackInfo);


#endif
