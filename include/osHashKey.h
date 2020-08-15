/* Copyright 2019, 2020, Sean Dai
 * @file osHashKey.h  Hash key generator
 */


#ifndef _OS_HASH_KEY_H
#define _OS_HASH_KEY_H


#include "osPL.h"


/* Hash functions */
uint32_t osHashGetKey(const uint8_t *key, size_t len);
uint32_t osHashGetKey_ci(const char *str, size_t len);
uint32_t osHashGetKey_str(const char *str);
uint32_t osHashGetKey_strCI(const char *str);
uint32_t osHashGetKey_pl(const osPointerLen_t *pl);
uint32_t osHashGetKey_plCI(const osPointerLen_t *pl);
uint32_t hash_fast(const char *k, size_t len);
uint32_t hash_fast_str(const char *str);

uint32_t osHashGetKey_extraKey(const uint8_t *key, size_t len, uint8_t extraKey);
uint32_t osHashGetKey_ci_extraKey(const char *str, size_t len, uint8_t extraKey);
uint32_t osHashGetKey_str_extraKey(const char *str, uint8_t extraKey);
uint32_t osHashGetKey_strCI_extraKey(const char *str, uint8_t extraKey);
uint32_t osHashGetKey_pl_extraKey(const osPointerLen_t *pl, uint8_t extraKey);
uint32_t osHashGetKey_plCI_extraKey(const osPointerLen_t *pl, uint8_t extraKey);


#endif
