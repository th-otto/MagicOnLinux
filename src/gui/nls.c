#include <stdlib.h>
#include <string.h>
#include "nls.h"

/* 1024 entries, means at least 8 KB, plus 8 bytes per string,
 * plus the lengths of strings
 */
nls_domain nls_current_domain;
#ifndef NLS_KEY_STRINGS
#define NLS_KEY_STRINGS nls_key_strings
#endif
extern char const NLS_KEY_STRINGS[];

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/* initialisation */

void nls_init(nls_domain *domain)
{
	domain->hash = NULL;
}

/* ------------------------------------------------------------------------- */

#define TH_BITS 10
#define TH_SIZE (1 << TH_BITS)
#define TH_MASK (TH_SIZE - 1)
#define TH_BMASK ((1 << (16 - TH_BITS)) - 1)

/*
 * if you change this, also change cpute_th_value() in pofile.c
 */
static unsigned int nls_hash(const char *t)
{
	const unsigned char *u = (const unsigned char *) t;
	unsigned short a, b;

	a = 0;
	while (*u)
	{
		a = (a << 1) | ((a >> 15) & 1);
		a += *u++;
	}
	b = (a >> TH_BITS) & TH_BMASK;
	a ^= b;
	a &= TH_MASK;
	return a;
}

/* ------------------------------------------------------------------------- */

const char *nls_dgettext(const nls_domain *domain, const char *key)
{
	unsigned int hash;
	const nls_key_offset *chain;
	nls_key_offset cmp;
	
	/* check for empty string - often used in RSC - must return original address */
	if (domain == NULL || key == NULL || *key == '\0' || domain->hash == NULL)
		return key;
	hash = nls_hash(key);
	if ((chain = domain->hash[hash]) != NULL)
	{
		while ((cmp = *chain++) != 0)
		{
			if (strcmp(&NLS_KEY_STRINGS[cmp], key) == 0)
			{
				/* strings are equal, return next string */
				key = &domain->translations[*chain];
				break;
			}
			/* the strings differ, next */
			chain++;
		}
	}
	/* not in hash, return original string */
	return key;
}
