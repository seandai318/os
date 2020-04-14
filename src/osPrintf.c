/**
 * @file osPrint.c Formatted printing
 *
 * Copyright (C) 2019 InterLogic
 */
#include <string.h>
#include <math.h>
#include "osTypes.h"
#include "osString.h"
//#include <re_sa.h>
#include "osPrintf.h"
#include "osMemory.h"
#include "osMBuf.h"
#include "osPL.h"
#include "osSocketAddr.h"


enum length_modifier {
	LENMOD_NONE      = 0,
	LENMOD_LONG      = 1,
	LENMOD_LONG_LONG = 2,
	LENMOD_SIZE      = 42,
};

enum {
	DEC_SIZE = 42,
	NUM_SIZE = 64
};

static const char prfx_neg[]  = "-";
static const char prfx_hex[]  = "0x";
static const char str_nil[]  = "(nil)";

/**
 * p: a string to be printed
 * sz: the size of the print string
 * pad: space (number of characters) a print object will occupy, like if pad=5 for a print integer "4", the printout will be: "    4"
 *      this is for print format like %5d
 * pch: the filling character for pad, like in above example, if pch='0', the printout will be: "00004".  This is for print format like %05d
 * prfx: add '-' for a negative value, "0x" for heximal.  the format %05d for "-4" will be "-0004"
 * plr: align the printed object on the left, in this case, the pch is useless.  the format %-05d for -4 will be "-4   ", here '-' can be
 *      in any position between % and d. 
 */
static int write_padded(const char *p, size_t sz, size_t pad, char pch, bool plr, const char *prfx, osPrintf_h pHandler, void *arg)
{
	const size_t prfx_len = osStrLen(prfx);
	int err = 0;

	pad -= MIN(pad, prfx_len);

	if (prfx && pch == '0')
	{
		err |= pHandler(prfx, prfx_len, arg);
	}

	while (!plr && (pad-- > sz))
	{
		err |= pHandler(&pch, 1, arg);
	}

	if (prfx && pch != '0')
	{
		err |= pHandler(prfx, prfx_len, arg);
	}

	if (p && sz)
	{
		err |= pHandler(p, sz, arg);
	}

	while (plr && pad-- > sz)
	{
		err |= pHandler(&pch, 1, arg);
	}

	return err;
}


/* integer to string */
static uint32_t local_itoa(char *buf, uint64_t n, uint8_t base, bool uc)
{
	char c, *p = buf + NUM_SIZE;
	uint32_t len = 1;
	const char a = uc ? 'A' : 'a';

	*--p = '\0';
	do {
		const uint64_t dv  = n / base;
		const uint64_t mul = dv * base;

		c = (char)(n - mul);

		if (c < 10)
			*--p = '0' + c;
		else
			*--p = a + (c - 10);

		n = dv;
		++len;

	} while (n != 0);

	memmove(buf, p, len);

	return len - 1;
}


/* float value to string */
static size_t local_ftoa(char *buf, double n, size_t dp)
{
	char *p = buf;
	long long a = (long long)n;
	double b = n - (double)a;

	b = (b < 0) ? -b : b;

	/* integral part */
	p += local_itoa(p, (a < 0) ? -a : a, 10, false);

	*p++ = '.';

	/* decimal digits */
	while (dp--) {
		char v;

		b *= 10;
		v  = (char)b;
		b -= v;

		*p++ = '0' + (char)v;
	}

	*p = '\0';

	return p - buf;
}

int osPrintfHandler_debug(const char *p, size_t size, void* arg)
{
    FILE* f =(FILE*) arg;
	if(!f || !p ||!size)
	{
		return 1;
	}

    if (1 != fwrite(p, size, 1, f))
	{
        return ENOMEM;
	}

    return 0;
}


/**
 * Print a formatted string based on fmt and ap using print handler pHandler.  pHandler can be a print handler to print out in stdout, on a buffer, on a file, etc.
 *
 * @param fmt Formatted string
 * @param ap  Variable argument
 * @param pHandler Print handler
 * @param arg Handler argument, shall contain the output destination
 *
 * @return 0 if success, otherwise errorcode
 *
 * Extensions:
 *
 * <pre>
 *   %b  (char *, size_t)        Buffer string with pointer and length
 *   %r  (osPointerLen_t)             Pointer-length object
 *   %w  (uint8_t *, size_t)     Binary buffer to hexadecimal format
 *   %j  (osSocketAddr_t *)           Socket address - address part only
 *   %J  (osSocketAddr_t *)           Socket address and port - like 1.2.3.4:1234
 *   %H  (osPrintfHandlerName_t, void *) Print handler with argument
 *   %v  (char *fmt, va_list *)  Variable argument list
 *   %m  (int)                   Describe an error code
 * </pre>
 *
 * Reserved for the future:
 *
 *   %k
 *   %y
 *
 */
int osPrintf_onHandler(const char *fmt, va_list ap, osPrintf_h pHandler, void *arg)
{
	uint8_t base, *bptr;
	char pch, ch, num[NUM_SIZE], addr[64], msg[256];
	enum length_modifier lenmod = LENMOD_NONE;
	osPrintf_t pf;
	bool fm = false, plr = false;
	const osPointerLen_t *pl;
	const osMBuf_t* pMbuf;
	size_t pad = 0, fpad = -1, len, i;
	const char *str, *p = fmt, *p0 = fmt;
	const osSocketAddr_t *sa;
	osPrintfHandlerName_t ph;
	void *ph_arg;
	va_list *apl;
	int err = 0;
	void *ptr;
	uint64_t n;
	int64_t sn;
	bool uc = false;
	double dbl;

	if (!fmt || !pHandler)
	{
		return EINVAL;
	}

	pf.pHandler = pHandler;
	pf.arg = arg;

	for (;*p && !err; p++)
	{
		// if % has not been met
		if (!fm) 
		{
			if (*p != '%')
			{
				continue;
			}
			pch = ' ';
			plr = false;
			pad = 0;
			fpad = -1;
			lenmod = LENMOD_NONE;
			uc = false;

			if (p > p0)
			{
				err |= pHandler(p0, p - p0, arg);
			}
			//set fm=1 when seeing %
			fm = true;
			continue;
		}

		fm = false;
		base = 10;

		switch (*p) 
		{
			case '-':
				plr = true;
				fm  = true;
				break;

			case '.':
				fpad = pad;
				pad = 0;
				fm = true;
				break;

			case '%':
				ch = '%';

				err |= pHandler(&ch, 1, arg);
				break;

			case 'b':
				str = va_arg(ap, const char *);
				len = va_arg(ap, size_t);

				err |= write_padded(str, str ? len : 0, pad, ' ', plr, NULL, pHandler, arg);
				break;

			case 'c':
				ch = va_arg(ap, int);

				err |= write_padded(&ch, 1, pad, ' ', plr, NULL, pHandler, arg);
				break;

			case 'd':
			case 'i':
				switch (lenmod)
				{
					case LENMOD_SIZE:
						sn = va_arg(ap, ssize_t);
						break;

					default:
					case LENMOD_LONG_LONG:
						sn = va_arg(ap, signed long long);
						break;

					case LENMOD_LONG:
						sn = va_arg(ap, signed long);
						break;

					case LENMOD_NONE:
						sn = va_arg(ap, signed);
						break;
				}

				len = local_itoa(num, (sn < 0) ? -sn : sn, base, false);

				err |= write_padded(num, len, pad, plr ? ' ' : pch, plr, (sn < 0) ? prfx_neg : NULL, pHandler, arg);
				break;

			case 'f':
			case 'F':
				dbl = va_arg(ap, double);

				if (fpad == (size_t)-1) 
				{
					fpad = pad;
					pad  = 0;
				}

				if (isinf(dbl)) 
				{
					err |= write_padded("inf", 3, fpad, ' ', plr, NULL, pHandler, arg);
				}
				else if (isnan(dbl)) 
				{
					err |= write_padded("nan", 3, fpad, ' ', plr, NULL, pHandler, arg);
				}
				else 
				{
					len = local_ftoa(num, dbl, pad ? min(pad, DEC_SIZE) : 6);

					err |= write_padded(num, len, fpad, plr ? ' ' : pch, plr, (dbl<0) ? prfx_neg : NULL, pHandler, arg);
				}
				break;

			case 'H':
				ph = va_arg(ap, osPrintfHandlerName_t);
				ph_arg = va_arg(ap, void *);

				if (ph)
				{
					err |= ph(&pf, ph_arg);
				}
				break;

			case 'l':
				++lenmod;
				fm = true;
				break;

			case 'm':
				str = osStrError(va_arg(ap, int), msg, sizeof(msg));
				err |= write_padded(str, osStrLen(str), pad, ' ', plr, NULL, pHandler, arg);
				break;

			case 'p':
				ptr = va_arg(ap, void *);

				if (ptr) 
				{
					len = local_itoa(num, (unsigned long int)ptr, 16, false);
					err |= write_padded(num, len, pad, plr ? ' ' : pch, plr, prfx_hex, pHandler, arg);
				}
				else 
				{
					err |= write_padded(str_nil, sizeof(str_nil) - 1, pad, ' ', plr, NULL, pHandler, arg);
				}
				break;

			case 'r':
				pl = va_arg(ap, const osPointerLen_t *);

				err |= write_padded(pl ? pl->p : NULL, (pl && pl->p) ? pl->l : 0, pad, ' ', plr, NULL, pHandler, arg);
				break;

			case 'M':
				pMbuf = va_arg(ap, const osMBuf_t *);

                err |= write_padded(pMbuf ? pMbuf->buf : NULL, (pMbuf && pMbuf->buf) ? pMbuf->end : 0, pad, ' ', plr, NULL, pHandler, arg);
				break;
			case 's':
				str = va_arg(ap, const char *);
				err |= write_padded(str, osStrLen(str), pad, ' ', plr, NULL, pHandler, arg);
				break;

			case 'X':
				uc = true;
				/*@fallthrough@*/
			case 'x':
				base = 16;
				/*@fallthrough@*/
			case 'u':
				switch (lenmod) 
				{
					case LENMOD_SIZE:
						n = va_arg(ap, size_t);
						break;

					default:
					case LENMOD_LONG_LONG:
						n = va_arg(ap, unsigned long long);
						break;

					case LENMOD_LONG:
						n = va_arg(ap, unsigned long);
						break;

					case LENMOD_NONE:
						n = va_arg(ap, unsigned);
						break;
				}

				len = local_itoa(num, n, base, uc);

				err |= write_padded(num, len, pad, plr ? ' ' : pch, plr, NULL, pHandler, arg);
				break;

			case 'v':
				str = va_arg(ap, char *);
				apl = va_arg(ap, va_list *);

				if (!str || !apl)
				{
					break;
				}
				err |= osPrintf_onHandler(str, *apl, pHandler, arg);
				break;

			case 'W':
				uc = true;
				/*@fallthrough@*/
			case 'w':
				bptr = va_arg(ap, uint8_t *);
				len = va_arg(ap, size_t);

				len = bptr ? len : 0;
				pch = plr ? ' ' : pch;

				while (!plr && pad-- > (len * 2))
				{
					err |= pHandler(&pch, 1, arg);
				}
				for (i=0; i<len; i++) 
				{
					const uint8_t v = *bptr++;
					uint32_t l = local_itoa(num, v, 16, uc);
					err |= write_padded(num, l, 2, '0', false, NULL, pHandler, arg);
				}

				while (plr && pad-- > (len * 2))
				{
					err |= pHandler(&pch, 1, arg);
				}
				break;

			case 'z':
				lenmod = LENMOD_SIZE;
				fm = true;
				break;
/*
			case 'j':
				sa = va_arg(ap, osSocketAddr_t *);
				if (!sa)
				{
					break;
				}
				if (osSA_ntop(sa, addr, sizeof(addr))) 
				{
					err |= write_padded("?", 1, pad, ' ', plr, NULL, pHandler, arg);
					break;
				}
				err |= write_padded(addr, strlen(addr), pad, ' ', plr, NULL, pHandler, arg);
				break;

			case 'J':
				sa = va_arg(ap, osSocketAddr_t *);
				if (!sa)
				{
					break;
				}
				if (osSA_ntop(sa, addr, sizeof(addr)))
				{
					err |= write_padded("?", 1, pad, ' ', plr, NULL, pHandler, arg);
					break;
				}
*/
#ifdef HAVE_INET6
				if (AF_INET6 == sa_af(sa))
				{
					ch = '[';
					err |= pHandler(&ch, 1, arg);
				}
#endif
				err |= write_padded(addr, strlen(addr), pad, ' ', plr, NULL, pHandler, arg);
#ifdef HAVE_INET6
				if (AF_INET6 == sa_af(sa)) 
				{
					ch = ']';
					err |= pHandler(&ch, 1, arg);
				}
#endif

				ch = ':';
				err |= pHandler(&ch, 1, arg);
				len = local_itoa(num, osSA_port(sa), 10, false);
				err |= write_padded(num, len, pad, plr ? ' ' : pch, plr, NULL, pHandler, arg);

				break;

			default:
				if (('0' <= *p) && (*p <= '9')) 
				{
					if (!pad && ('0' == *p)) 
					{
						pch = '0';
					}
					else 
					{
						pad *= 10;
						pad += *p - '0';
					}
					fm = true;
					break;
				}

				ch = '?';

				err |= pHandler(&ch, 1, arg);
				break;
		}

		if (!fm)
		{
			p0 = p + 1;
		}
	}

	if (!fm && p > p0)
	{
		err |= pHandler(p0, p - p0, arg);
	}
	return err;
}


static int osPrintfHandler_pl(const char *p, size_t size, void *arg)
{
	osPointerLen_t *pl = arg;

	if (size > pl->l)
	{
		return ENOMEM;
	}

	memcpy((void *)pl->p, p, size);

	osPL_advance(pl, size);

	return 0;
}


struct dyn_print {
	char *str;
	char *p;
	size_t l;
	size_t size;
};


static int osPrintfHandler_dyn(const char *p, size_t size, void *arg)
{
	struct dyn_print *dp = arg;

	if (size > dp->l - 1) 
	{
		const size_t new_size = MAX(dp->size + size, dp->size * 2);
		char *str = osMem_realloc(dp->str, new_size);
		if (!str)
		{
			return ENOMEM;
		}

		dp->str = str;
		dp->l += new_size - dp->size;
		dp->p = dp->str + new_size - dp->l;
		dp->size = new_size;
	}

	memcpy(dp->p, p, size);

	dp->p += size;
	dp->l -= size;

	return 0;
}


struct stream_print {
	FILE *f;
	size_t n;
};

static int osPrintfHandler_stream(const char *p, size_t size, void *arg)
{
	struct stream_print *sp = arg;

	if (1 != fwrite(p, size, 1, sp->f))
		return ENOMEM;

	sp->n += size;

	return 0;
}


static int osPrintfHandler_mbuf(const char *p, size_t size, void *arg)
{
    osMBuf_t *mb = arg;

    return osMBuf_writeBuf(mb, (const uint8_t *)p, size, true);
}


/**
 * Print a formatted string to a file stream like stdout, stderr, file, etc., using va_list
 *
 * @param stream File stream for the output
 * @param fmt    Formatted string
 * @param ap     Variable-arguments list
 *
 * @return The number of characters printed, or -1 if error
 */
int osPrintf_onFile(FILE *stream, const char *fmt, va_list ap)
{
	struct stream_print sp;

	if (!stream)
		return -1;

	sp.f = stream;
	sp.n = 0;

	if (0 != osPrintf_onHandler(fmt, ap, osPrintfHandler_stream, &sp))
		return -1;

	return (int)sp.n;
}


/**
 * Print a formatted string to stdout, using va_list
 *
 * @param fmt Formatted string
 * @param ap  Variable-arguments list
 *
 * @return The number of characters printed, or -1 if error
 */
int osPrintf_onStdout(const char *fmt, va_list ap)
{
	return osPrintf_onFile(stdout, fmt, ap);
}


/**
 * Print a formatted string to a buffer pointed by str, using va_list
 *
 * @param str  Buffer for output string
 * @param size Size of buffer
 * @param fmt  Formatted string
 * @param ap   Variable-arguments list
 *
 * @return The number of characters printed, or -1 if error
 */
int osPrintf_onBuffer(char *str, size_t size, const char *fmt, va_list ap)
{
	osPointerLen_t pl;
	int err;

	if (!str || !size)
		return -1;

	pl.p = str;
	pl.l = size - 1;

	err = osPrintf_onHandler(fmt, ap, osPrintfHandler_pl, &pl);

	str[size - pl.l - 1] = '\0';

	return err ? -1 : (int)(size - pl.l - 1);
}


/**
 * Print a formatted string to a dynamically allocated buffer, using va_list
 *
 * @param strp Pointer for output string
 * @param fmt  Formatted string
 * @param ap   Variable-arguments list
 *
 * @return 0 if success, otherwise errorcode
 */
int osPrintf_onDynBuffer(char **strp, const char *fmt, va_list ap)
{
	struct dyn_print dp;
	int err;

	if (!strp)
	{
		return EINVAL;
	}

	dp.size = 16;
	dp.str  = osmalloc_r(dp.size, NULL);
	if (!dp.str)
	{
		return ENOMEM;
	}

	dp.p = dp.str;
	dp.l = dp.size;

	err = osPrintf_onHandler(fmt, ap, osPrintfHandler_dyn, &dp);
	if (err)
	{
		goto out;
	}

	*dp.p = '\0';

 out:
	if (err)
	{
		osfree(dp.str);
	}
	else
	{
		*strp = dp.str;
	}

	return err;
}


/**
 * Print a formatted variable argument list to a memory buffer
 *
 * @param mb  Memory buffer
 * @param fmt Formatted string
 * @param ap  Variable argument list
 *
 * @return 0 if success, otherwise errorcode
 */
int osPrintf_onMBuf(osMBuf_t *mb, const char *fmt, va_list ap)
{
    return osPrintf_onHandler(fmt, ap, osPrintfHandler_mbuf, mb);
}


/**
 * Print a formatted string via a print handler
 *
 * @param pf  Print backend
 * @param fmt Formatted string
 *
 * @return 0 if success, otherwise errorcode
 */
int osPrintf_handler(osPrintf_t *pf, const char *fmt, ...)
{
	va_list ap;
	int err;

	if (!pf)
	{
		return EINVAL;
	}

	va_start(ap, fmt);
	err = osPrintf_onHandler(fmt, ap, pf->pHandler, pf->arg);
	va_end(ap);

	return err;
}


/**
 * Print a formatted string to a file stream
 *
 * @param stream File stream for output
 * @param fmt    Formatted string
 *
 * @return The number of characters printed, or -1 if error
 */
int osPrintf(FILE *stream, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = osPrintf_onFile(stream, fmt, ap);
	va_end(ap);

	return n;
}


/**
 * Print a formatted string to stdout
 *
 * @param fmt    Formatted string
 *
 * @return The number of characters printed, or -1 if error
 */
int osPrintf_stdout(const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = osPrintf_onStdout(fmt, ap);
	va_end(ap);

	return n;
}


/**
 * Print a formatted string to a buffer
 *
 * @param str  Buffer for output string
 * @param size Size of buffer
 * @param fmt  Formatted string
 *
 * @return The number of characters printed, or -1 if error
 */
int osPrintf_buffer(char *str, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = osPrintf_onBuffer(str, size, fmt, ap);
	va_end(ap);

	return n;
}


/**
 * Print a formatted string to a buffer
 *
 * @param strp Buffer pointer for output string
 * @param fmt  Formatted string
 *
 * @return 0 if success, otherwise errorcode
 */
int osPrintf_dynBuffer(char **strp, const char *fmt, ...)
{
	va_list ap;
	int err;

	va_start(ap, fmt);
	err = osPrintf_onDynBuffer(strp, fmt, ap);
	va_end(ap);

	return err;
}


/**
 * Print a formatted string to a memory buffer
 *
 * @param mb  Memory buffer
 * @param fmt Formatted string
 *
 * @return 0 if success, otherwise errorcode
 */
int osPrintf_mbuf(osMBuf_t *mb, const char *fmt, ...)
{
    int err = 0;
    va_list ap;

    va_start(ap, fmt);
    err = osPrintf_onHandler(fmt, ap, osPrintfHandler_mbuf, mb);
    va_end(ap);

    return err;
}

