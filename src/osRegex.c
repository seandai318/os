/********************************************************
 * Copyright (C) 2019, Sean Dai
 *
 * @file osRegex.c  Implements basic regular expressions
 * Adapted from regex.c @Creytiv.com
 ********************************************************
 * @file regex.c Implements basic regular expressions
 *
 * Copyright (C) 2010 Creytiv.com
 ********************************************************/

#include <ctype.h>
#include <stdarg.h>
#include "osTypes.h"
#include "osPL.h"


/** Defines a character range */
struct chr {
	uint8_t min;  /**< Minimum value */
	uint8_t max;  /**< Maximum value */
};


static bool expr_match(const struct chr *chrv, uint32_t n, uint8_t c, bool neg)
{
	uint32_t i;
	
	for (i=0; i<n; i++) {
		printf("sean, i=%d, chrvmin=%d, chrvmax=%d, n=%d, c=%c, neg=%d\n", i, chrv[i].min, chrv[i].max, n, c, neg);
		if (c < chrv[i].min)
			continue;

		if (c > chrv[i].max)
			continue;

		break;
	}

	printf("sean, expr_match, neg=%d, i=%d, n=%d, return=%d\n", neg, i, n, neg ? (i == n) : (i != n));
	return neg ? (i == n) : (i != n);
}


/**
 * Parse a string using basic regular expressions. Any number of matching
 * expressions can be given, and each match will be stored in a "osPointerLen_t"
 * pointer-length type.
 *
 * @param ptr  String to parse
 * @param len  Length of string
 * @param expr Regular expressions string
 *
 * @return 0 if success, otherwise errorcode
 *
 * Example:
 *
 *   We parse the buffer for any numerical values, to get a match we must have
 *   1 or more occurences of the digits 0-9. The result is stored in 'num',
 *   which is of pointer-length type and will point to the first location in
 *   the buffer that contains "42".
 *
 * <pre>
 const char buf[] = "foo 42 bar";
 osPointerLen_t num;
 int err = re_regex(buf, strlen(buf), "[0-9]+", &num);

 here num contains a pointer to '42'
 * </pre>
 */
int osRegex(const char *ptr, size_t len, const char *expr, ...)
{
	struct chr chrv[64];
	const char *p, *ep;
	bool fm, range = false, ec = false, neg = false, qesc = false;
	uint32_t n = 0;
	va_list ap;
	bool eesc;
	size_t l;

	if (!ptr || !expr)
		return EINVAL;

 again:
	printf("sean, again\n");
	eesc = false;
	fm = false;
	l  = len--;
	p  = ptr++;
	ep = expr;

	va_start(ap, expr);

	if (!l)
		goto out;

	for (; *ep; ep++) {
		printf("sean, first *ep loop, *ep=%c, n=%d\n", *ep, n);
		if ('\\' == *ep && !eesc) {
			eesc = true;
			continue;
		}

		if (!fm) {

			/* Start of character class */
			if ('[' == *ep && !eesc) {
				n     = 0;
				fm    = true;
				ec    = false;
				neg   = false;
				range = false;
				qesc  = false;
				continue;
			}

			if (!l)
				break;

			if (tolower(*ep) != tolower(*p)) {
				printf("sean, 111, *ep=0x%x, *p=0x%x\n", *ep, *p);
				va_end(ap);
				goto again;
			}

			eesc = false;
			++p;
			--l;
			continue;
		}
		/* End of character class */
		else if (ec) {
			printf("sean, enter ec=true\n");
			uint32_t nm, nmin, nmax;
			osPointerLen_t lpl, *pl = va_arg(ap, osPointerLen_t *);
			printf("sean, get a pl\n");
			bool quote = false, esc = false;

			/* Match 0 or more times */
			if ('*' == *ep) {
				nmin = 0;
				nmax = -1;
			}
			/* Match 1 or more times */
			else if ('+' == *ep) {
				nmin = 1;
				nmax = -1;
			}
			/* Match exactly n times */
			else if ('1' <= *ep && *ep <= '9') {
				nmin = *ep - '0';
				nmax = *ep - '0';
			}
			else
				break;

			fm = false;

			lpl.p = p;
			lpl.l = 0;

			for (nm = 0; l && nm < nmax; nm++, p++, l--, lpl.l++) {
				printf("sean, nm=%d, *p=%c,l=%ld,  lpl.l=%ld\n", nm, *p, l, lpl.l);
				if (qesc) {

					if (esc) {
						esc = false;
						continue;
					}

					switch (*p) {

					case '\\':
						esc = true;
						continue;

					case '"':
						quote = !quote;
						continue;
					}

					if (quote)
						continue;
				}

				if (!expr_match(chrv, n, tolower(*p), neg))
				{
					printf("sean, to break\n");
					break;
				}
			}

			/* Strip quotes */
			if (qesc && lpl.l > 1 &&
			    lpl.p[0] == '"' && lpl.p[lpl.l - 1] == '"') {

				lpl.p += 1;
				lpl.l -= 2;
				nm    -= 2;
			}

			if ((nm < nmin) || (nm > nmax)) {
				va_end(ap);
				goto again;
			}

			if (pl)
				*pl = lpl;

			eesc = false;
			continue;
		}

		if (eesc) {
			eesc = false;
			goto chr;
		}

		printf("sean, *ep=%c\n", *ep);
		switch (*ep) {
			/* End of character class */
		case ']':
			ec = true;
			printf("sean, set ec=true\n");
			continue;

			/* Negate with quote escape */
		case '~':
			if (n)
				break;

			qesc = true;
			neg  = true;
			continue;

			/* Negate */
		case '^':
			if (n)
				break;

			neg = true;
			continue;

			/* Range */
		case '-':
			if (!n || range)
				break;

			range = true;
			--n;
			continue;
		}

	chr:
		printf("sean, inside char, n=%d, max=%d, min=%d, *ep=%c\n", n, chrv[n].max, chrv[n].min, *ep);
		if (n >= ARRAY_SIZE(chrv))
			break;

		chrv[n].max = tolower(*ep);

		if (range)
			range = false;
		else
			chrv[n].min = tolower(*ep);

		++n;

		printf("sean, end of char, n=%d, max=%d, min=%d, *ep=%c\n", n, chrv[n-1].max, chrv[n-1].min, *ep);
	}
 out:
	va_end(ap);

	if (fm)
		return EINVAL;

	return *ep ? ENOENT : 0;
}
