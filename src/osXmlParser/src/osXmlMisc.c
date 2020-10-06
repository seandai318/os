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

        if(pXmlBuf->buf[pXmlBuf->pos] != pPattern->p[i]);
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
    if(!pNsAlias ||!str||!pTag)
    {
        logError("null pointer, pNsAlias=%p, str=%p, pTag=%p.", pNsAlias, str, pTag);
        return -1;
    }

    if(pTag->l - pNsAlias->l != strLen)
    {
        return -1;
    }

    if(strncmp(pTag->p, pNsAlias->p, pNsAlias->l) != 0)
    {
        return -1;
    }

    return strncmp(str, &pTag->p[pNsAlias->l], pTag->l - pNsAlias->l);
}


int osXml_xsTagCmp(char* str, int strlen, osPointerLen_t* pTag)
{
    osPointerLen_t* pXsAlias = osXsd_getXSAlias();
    return osXml_tagCmp(pXsAlias, str, strlen, pTag);
}


bool osXml_singleDelimitMatch(const char* str, int strlen, char delimit, osPointerLen_t* tag, osPointerLen_t* pFirstSection)
{
    if(!str || !tag || !pFirstSection)
    {
        logError("null pointer, str=%p, tag=%p, pFirstSection=%p", str, tag, pFirstSection);
        return false;
    }

    //case when there is no delimit
    if(strlen == tag->l && memcmp(str, tag->p, strlen) == 0)
    {
        pFirstSection->l = 0;
        return true;
    }

    for(int i=0; i<tag->l; i++)
    {
        if(tag->p[i] == delimit)
        {
            if(strlen +i +1 == tag->l && memcmp(str, &tag->p[i+1], strlen) == 0)
            {
                pFirstSection->p = tag->p;
                pFirstSection->l = i+1;
                return true;
            }
        }
    }

    return false;
}
