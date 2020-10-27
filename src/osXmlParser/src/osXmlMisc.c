/********************************************************
 * Copyright (C) 2020 Sean Dai
 *
 * @file osXmlMisc.h
 * Miscellaneous helper functions for both osXml and osXsd 
 ********************************************************/

#include <string.h>

#include "osXmlMisc.h"
#include "osXsdParser.h"


//the pattern can not contain char '"'
bool osXml_findPattern(osMBuf_t* pXmlBuf, osPointerLen_t* pPattern, bool isAdvancePos)
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

        if(pXmlBuf->buf[pXmlBuf->pos] != pPattern->p[i])
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


//the check starting pos must not within a double quote.
bool osXml_findWhiteSpace(osMBuf_t* pBuf, bool isAdvancePos)
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


int osXml_tagCmp(osPointerLen_t* pNsAlias, char* str, int strLen, osPointerLen_t* pTag)
{
	int returnValue = -1;
	
    if(!pNsAlias ||!str||!pTag)
    {
        logError("null pointer, pNsAlias=%p, str=%p, pTag=%p.", pNsAlias, str, pTag);
        goto EXIT;
    }

	//need to consider the delimit ':'
	if(pNsAlias->l)
	{
		if(pTag->l - pNsAlias->l -1 != strLen)
		{
        	goto EXIT;
		}

		if(strncmp(pTag->p, pNsAlias->p, pNsAlias->l) != 0)
	    {
    		goto EXIT;
		}

		if(pTag->p[pNsAlias->l] != ':')
		{
            goto EXIT;
		}

		returnValue = strncmp(str, &pTag->p[pNsAlias->l+1], pTag->l - pNsAlias->l-1);
	}
	else
	{
		if(pTag->l != strLen)
		{
			goto EXIT;
		}

    	returnValue = strncmp(str, pTag->p, pTag->l);
	}

EXIT:
	return returnValue;
}


//compare a ns alias preceeded tag with a specified tag name without ns alias, like osXml_xsTagCmp("element", 7, &pTagInfo->tag)
int osXml_xsTagCmp(char* str, int strlen, osPointerLen_t* pTag)
{
    osPointerLen_t* pXsAlias = osXsd_getXSAlias();
    return osXml_tagCmp(pXsAlias, str, strlen, pTag);
}


/* match a known string before or after the delimit. The string is specified with str.
 * return true either a delimit is found (the first delimit), or no delimit, but src and dest string match
 * str:          IN, the string to be checked if exist in tag
 * strlen:       IN, the string length
 * delimit:      IN, the delimit char, like ':' in "xmlns:xs"
 * tag:          IN, the string to be checked whether it ontains str
 * isStrAfterDelimit: IN, true, the delimit preceeds str, false, the delimit after str
 * pSection:     OUT, the other part of tag after excluding str and ':'
 */
bool osXml_singleDelimitMatch(const char* str, int strlen, char delimit, osPointerLen_t* tag, bool isStrAfterDelimit, osPointerLen_t* pSection)
{
    if(!str || !tag || !pSection)
    {
        logError("null pointer, str=%p, tag=%p, pSection=%p", str, tag, pSection);
        return false;
    }

    //case when there is no delimit
    if(strlen == tag->l && memcmp(str, tag->p, strlen) == 0)
    {
        pSection->l = 0;
        return true;
    }

    for(int i=0; i<tag->l; i++)
    {
        if(tag->p[i] == delimit)
        {
			if(isStrAfterDelimit)
			{
            	if(strlen +i +1 == tag->l && memcmp(str, &tag->p[i+1], strlen) == 0)
            	{
                	pSection->p = tag->p;
                	pSection->l = i;
                	return true;
            	}
			}
			else
			{
				if(strlen == i && memcmp(str, tag->p, strlen) == 0)
				{
					pSection->p = &tag->p[strlen+1];
					pSection->l = tag->l - strlen -1;
					return true;
				}
			}
			break;
        }
    }

    return false;
}


void osXml_singleDelimitParse(osPointerLen_t* src, char delimit, osPointerLen_t* pLeftStr, osPointerLen_t* pRightStr)
{
	//this is a internal function, save time to check null pointer
	for(int i=0; i<src->l; i++)
    {
        if(src->p[i] == delimit)
        {
			pLeftStr->p = src->p;
			pLeftStr->l = i;

			pRightStr->p = &src->p[pLeftStr->l + 1];
			pRightStr->l = src->l - pLeftStr->l - 1;
			
			return;
		}
	}

	pLeftStr->l = 0;
	*pRightStr = *src;
}

