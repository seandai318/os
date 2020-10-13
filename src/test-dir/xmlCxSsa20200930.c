#include <stdio.h>
#include <stdlib.h>

#include "osMemory.h"
#include "osPreMemory.h"
#include "osDebug.h"
#include "osMBuf.h"
#include "osList.h"
#include "osPL.h"
#include "osXmlParserIntf.h"


#if 0
typedef enum {
	TEST_XML_IDENTITY,
    TEST_XML_PRIVATEID,                    
    TEST_XML_PUBLIC_ID,                    
    TEST_SHARED_IFCSET_ID,                 
    TEST_XML_BARRING_INDICATION,           
	TEST_HSS_SUBSCRIPTION_ID,
    TEST_HSS_SUBSCRIPTION_ID_TYPE,         
    TEST_HSS_SUBSCRIPTION_ID_DATA,         
    TEST_HSS_MAX_NO_SIMULTANEOUS_SESSIONS, 
    TEST_XML_MAX_DATA_NAME_NUM,
} testConfig_xmlDataName_e;


osXmlData_t testConfig_xmlData[TEST_XML_MAX_DATA_NAME_NUM] = {
	{TEST_XML_IDENTITY,						{"Identity", sizeof("Identity")-1}, OS_XML_DATA_TYPE_SIMPLE, 0},
    {TEST_XML_PRIVATEID,                    {"PrivateID", sizeof("PrivateID")-1}, OS_XML_DATA_TYPE_SIMPLE, 0},
    {TEST_XML_PUBLIC_ID,					{"PublicIdentity", sizeof("PublicIdentity")-1}, OS_XML_DATA_TYPE_COMPLEX, 0},
    {TEST_SHARED_IFCSET_ID,					{"SharedIFCSetID", sizeof("SharedIFCSetID")-1}, OS_XML_DATA_TYPE_SIMPLE, 0},
    {TEST_XML_BARRING_INDICATION,      		{"BarringIndication", sizeof("BarringIndication")-1}, OS_XML_DATA_TYPE_SIMPLE, 0},
	{TEST_HSS_SUBSCRIPTION_ID, 				{"HSS:SubscriptionId", sizeof("HSS:SubscriptionId")-1}, OS_XML_DATA_TYPE_ANY, 0},
    {TEST_HSS_SUBSCRIPTION_ID_TYPE,			{"HSS:SubscriptionIdType", sizeof("HSS:SubscriptionIdType")-1}, OS_XML_DATA_TYPE_ANY, 0},
    {TEST_HSS_SUBSCRIPTION_ID_DATA,			{"HSS:SubscriptionIdData", sizeof("HSS:SubscriptionIdData")-1}, OS_XML_DATA_TYPE_ANY, 0},
    {TEST_HSS_MAX_NO_SIMULTANEOUS_SESSIONS,	{"HSS:MaxNoSimultaneousSessions", sizeof("HSS:MaxNoSimultaneousSessions")-1}, OS_XML_DATA_TYPE_ANY, 0}
};
#else
typedef enum {
    TEST_XML_IDENTITY,
    TEST_XML_PRIVATEID,
    TEST_XML_PUBLIC_ID,
    TEST_SHARED_IFCSET_ID,
    TEST_HSS_SUBSCRIPTION_ID,
    TEST_XML_BARRING_INDICATION,
    TEST_HSS_SUBSCRIPTION_ID_TYPE,
    TEST_HSS_SUBSCRIPTION_ID_DATA,
    TEST_HSS_MAX_NO_SIMULTANEOUS_SESSIONS,
    TEST_XML_MAX_DATA_NAME_NUM,
} testConfig_xmlDataName_e;


osXmlData_t testConfig_xmlData[TEST_XML_MAX_DATA_NAME_NUM] = {
    {TEST_XML_IDENTITY,                     {"Identity", sizeof("Identity")-1}, OS_XML_DATA_TYPE_SIMPLE, 0},
    {TEST_XML_PRIVATEID,                    {"PrivateID", sizeof("PrivateID")-1}, OS_XML_DATA_TYPE_SIMPLE, 0},
    {TEST_XML_PUBLIC_ID,                    {"PublicIdentity", sizeof("PublicIdentity")-1}, OS_XML_DATA_TYPE_COMPLEX, 0},
    {TEST_SHARED_IFCSET_ID,                 {"SharedIFCSetID", sizeof("SharedIFCSetID")-1}, OS_XML_DATA_TYPE_SIMPLE, 0},
    {TEST_HSS_SUBSCRIPTION_ID,              {"SubscriptionId", sizeof("SubscriptionId")-1}, OS_XML_DATA_TYPE_ANY, 0},
    {TEST_XML_BARRING_INDICATION,           {"BarringIndication", sizeof("BarringIndication")-1}, OS_XML_DATA_TYPE_SIMPLE, 0},
    {TEST_HSS_SUBSCRIPTION_ID_TYPE,         {"SubscriptionIdType", sizeof("SubscriptionIdType")-1}, OS_XML_DATA_TYPE_ANY, 0},
    {TEST_HSS_SUBSCRIPTION_ID_DATA,         {"SubscriptionIdData", sizeof("SubscriptionIdData")-1}, OS_XML_DATA_TYPE_ANY, 0},
    {TEST_HSS_MAX_NO_SIMULTANEOUS_SESSIONS, {"MaxNoSimultaneousSessions", sizeof("MaxNoSimultaneousSessions")-1}, OS_XML_DATA_TYPE_ANY, 0}
};
#endif

void callback(osXmlData_t* pXmlValue, void* nsInfo)
{
	if(!pXmlValue)
	{
		logError("null pointer, pXmlValue.");
		return;
	}

	switch(pXmlValue->eDataName)
	{
		case TEST_XML_IDENTITY:
			debug("TEST_XML_IDENTITY,  nsalias=%r, dataType=%d, value=%r", &pXmlValue->nsAlias, pXmlValue->dataType, &pXmlValue->xmlStr);
			break;
		case TEST_XML_PRIVATEID:
			debug("TEST_XML_PRIVATEID, nsalias=%r, dataType=%d, value=%r", &pXmlValue->nsAlias, pXmlValue->dataType, &pXmlValue->xmlStr);
			break;
		case TEST_XML_PUBLIC_ID:
            debug("TEST_XML_PUBLIC_ID, nsalias=%r, dataType=%d, value=%r", &pXmlValue->nsAlias, pXmlValue->dataType, &pXmlValue->xmlStr);
			break;
    	case TEST_SHARED_IFCSET_ID:
			debug("TEST_SHARED_IFCSET_ID, nsalias=%r, dataType=%d, value=%d", &pXmlValue->nsAlias, pXmlValue->dataType, pXmlValue->xmlInt);
			break;
		case TEST_HSS_SUBSCRIPTION_ID:
            debug("TEST_HSS_SUBSCRIPTION_ID, nsalias=%r, dataType=%d, value=%r", &pXmlValue->nsAlias, pXmlValue->dataType, pXmlValue->xmlStr);
            break;
    	case TEST_HSS_SUBSCRIPTION_ID_TYPE:
            debug("TEST_HSS_SUBSCRIPTION_ID_TYPE, nsalias=%r, dataType=%d, value=%r", &pXmlValue->nsAlias, pXmlValue->dataType, &pXmlValue->xmlStr);
            break;
    	case TEST_HSS_SUBSCRIPTION_ID_DATA:
            debug("TEST_HSS_SUBSCRIPTION_ID_DATA, nsalias=%r, dataType=%d, value=%r", &pXmlValue->nsAlias, pXmlValue->dataType, &pXmlValue->xmlStr);
            break;
    	case TEST_XML_BARRING_INDICATION:
            debug("TEST_XML_BARRING_INDICATION, nsalias=%r, dataType=%d, value=%d", &pXmlValue->nsAlias, pXmlValue->dataType, pXmlValue->xmlIsTrue);
            break;
    	case TEST_HSS_MAX_NO_SIMULTANEOUS_SESSIONS:
            debug("TEST_HSS_MAX_NO_SIMULTANEOUS_SESSIONS, nsalias=%r, dataType=%d, value=%r", &pXmlValue->nsAlias, pXmlValue->dataType, &pXmlValue->xmlStr);
            break;
		default:
			debug("dataName=%r is not handled, dataType=%d, nsalias=%r.", &pXmlValue->dataName, pXmlValue->dataType, &pXmlValue->nsAlias);
			switch(pXmlValue->dataType)
			{
				case OS_XML_DATA_TYPE_XS_BOOLEAN:
					debug("	value = %s", pXmlValue->xmlIsTrue ? "true" :"false");
					break;
    			case OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE:
    			case OS_XML_DATA_TYPE_XS_SHORT:
    			case OS_XML_DATA_TYPE_XS_INTEGER:
    			case OS_XML_DATA_TYPE_XS_LONG:
					debug("	value = %d", pXmlValue->xmlInt);
					break;
				case OS_XML_DATA_TYPE_XS_STRING:
					debug("	value = %r", &pXmlValue->xmlStr);
					break;
				defult:
					break;
			}
			break;
	}
}

int main(int argc, char* argv[])
{
    if(argc !=3)
    {
        printf("need to input the xsd and xml file names. ./exec xsdFile xmlFile");
        return 1;
    }

    osPreMem_init();
    osDbg_init(DBG_DEBUG, DBG_ALL);
    osDbg_mInit(LM_XMLP, DBG_DEBUG);

    //osDbg_mInit(LM_MEM, DBG_DEBUG);

	osmem_stat();
	osmem_allusedinfo();

#if 0
	debug("i am ok here.");
	osMBuf_t* xsdMBuf = osMBuf_readFile(argv[1], 16000);
	debug("xsdMsg=\n%M", xsdMBuf);
	osMBuf_t* xmlMBuf = osMBuf_readFile(argv[2], 6000);
#endif

	osXmlDataCallbackInfo_t cbInfo={true, false, true, callback, testConfig_xmlData, TEST_XML_MAX_DATA_NAME_NUM};
	osXml_getLeafValue(".", argv[1], argv[2], &cbInfo);
#if 0
	bool isValid = osXml_isXmlValid(xmlMBuf, xsdMBuf, false, &cbInfo);	
	debug("xml is parsed %s.", isValid ? "OK" : "NOT OK");
#endif

#if 0    
	osmem_stat();
    osmem_allusedinfo();

	osMBuf_dealloc(xsdMBuf);
	osMBuf_dealloc(xmlMBuf);

   	osmem_stat();
    osmem_allusedinfo();
#endif
	return 0;
}
