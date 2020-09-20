/* Copyright <c) 2020, Sean Dai
 * xml parser data structures
 */

#ifndef _OS_XML_PARSER_DATA_H
#define _OS_XML_PARSER_DATA_H

#include "osList.h"
#include "osTypes.h"
#include "osMBuf.h"

#include "osXmlParserIntf.h"



typedef enum {
	OS_XML_TAG_XS_SCHEMA,
	OS_XML_TAG_XS_ELEMENT,
	OS_XML_TAG_XS_COMPLEX_TYPE,
	OS_XML_XS_ALL,
	OS_XML_XS_SEQUENCE,
	OS_XML_XS_CHOICE,
} osXmlTag_e;


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


typedef struct {
    osPointerLen_t typeName;    //must be in the beginning of this data structure, for the use in osXsd_getTypeByname()
    osXmlDataType_e baseType;
    osList_t facetList;         //each element is a osXmlRestrictionFacet_t
} osXmlSimpleType_t;


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
	union {
		osXmlComplexType_t* pComplex;
		osXmlSimpleType_t* pSimple;
	};
	osList_t* pSimpleTypeList;	//each element contains osXmlSimpleType_t, all xsdElement points to the same osList
} osXsdElement_t;


//help data structure for tag and namve-value pair list
typedef struct {
    osPointerLen_t tag;
    bool isTagDone;                 //the tag is wrapped in one line, i.e., <tag, tag-content />
    bool isEndTag;                  //the line is the end of tag, i.e., </tag>
    bool isPElement;                //if the union data is pElement, this value is true
    union {
        osList_t attrNVList;        //each list element contains osXmlNameValue_t
        osXsdElement_t* pElement;   //if tag is xs:element, and contains info other than tag attributes, this data structure will be used
    };
} osXmlTagInfo_t;


typedef struct osXmlNameValue {
    osPointerLen_t name;
    osPointerLen_t value;
} osXmlNameValue_t;

#if 0
//callback function to provide xml element value to the application
typedef osStatus_e (*osXmlDataCallback_h)(osPointerLen_t* elemName, osPointerLen_t* value, osXmlDataType_e dataType, osXmlDataCallbackInfo_t* callbackInfo);

typedef void (*osXsdElemCallback_h)(osXsdElement_t* pXsdElem, osXmlDataCallback_h callback, osXmlDataCallbackInfo_t* callbackInfo);
#endif


#endif
