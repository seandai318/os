/**
 * @file osHashKey.h  Hash key generator
 *
 * Copyright (C) 2019, InterLogic
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


#endif
