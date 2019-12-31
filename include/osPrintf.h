/**
 * @file osPrintf.h  Interface to formatted printf
 *
 * Copyright (C) 2019 InterLogic
 */


#ifndef _OS_PRINTF_H
#define _OS_PRINTF_H

#include <stdarg.h>
#include <stdio.h>


struct osMBuf;

/**
 * Defines the osPrintf_h print handler
 *
 * @param p    String to print
 * @param size Size of string to print
 * @param arg  Handler argument, shall contain the output destination, like stdout, etc.
 *
 * @return 0 for success, otherwise errorcode
 */
typedef int (*osPrintf_h)(const char *p, size_t size, void *arg);

/** Defines a print backend */
typedef struct osPrintf {
	osPrintf_h pHandler; /**< Print handler   */
	void *arg;         /**< Handler agument */
} osPrintf_t;

/**
 * Defines the %H print handler
 *
 * @param pf  Print backend
 * @param arg Handler argument
 *
 * @return 0 for success, otherwise errorcode
 */
typedef int(*osPrintfHandlerName_t)(osPrintf_t* pf, void *arg);

int osPrintf_onHandler(const char *fmt, va_list ap, osPrintf_h pHandler, void *arg);
int osPrintf_onFile(FILE *stream, const char *fmt, va_list ap);
int osPrintf_onStdout(const char *fmt, va_list ap);
int osPrintf_onBuffer(char *str, size_t size, const char *fmt, va_list ap);
int osPrintf_onDynBuffer(char **strp, const char *fmt, va_list ap);
int osPrintf_onMBuf(struct osMBuf *mb, const char *fmt, va_list ap);

int osPrintf_handler(osPrintf_t *pf, const char *fmt, ...);
int osPrintf(FILE *stream, const char *fmt, ...);
int osPrintf_stdout(const char *fmt, ...);
int osPrintf_buffer(char *str, size_t size, const char *fmt, ...);
int osPrintf_dynBuffer(char **strp, const char *fmt, ...);
int osPrintf_mbuf(struct osMBuf *mb, const char *fmt, ...);

int osPrintfHandler_debug(const char *p, size_t size, void* arg);

#endif
