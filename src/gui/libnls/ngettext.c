#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libintl.h>
#undef gettext
#undef dgettext
#undef ngettext
#undef dngettext
#include "libnls.h"
#include "libnlsI.h"
#include "nlshash.h"

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

#if 0 /* not used */
static int n_plurals(int plural_form)
{
	switch (plural_form)
	{
	case PLURAL_NONE:
		return 1;
	case PLURAL_NOT_ONE:
	case PLURAL_GREATER_ONE:
	case PLURAL_RULE_3:
	case PLURAL_RULE_4:
	case PLURAL_RULE_5:
	case PLURAL_RULE_6:
	case PLURAL_RULE_7:
		return 2;
	case PLURAL_RULE_8:
	case PLURAL_RULE_9:
	case PLURAL_RULE_10:
	case PLURAL_RULE_11:
	case PLURAL_RULE_12:
	case PLURAL_RULE_13:
	case PLURAL_RULE_14:
	case PLURAL_RULE_15:
	case PLURAL_RULE_16:
	case PLURAL_RULE_17:
	case PLURAL_RULE_18:
		return 3;
	case PLURAL_RULE_19:
	case PLURAL_RULE_20:
	case PLURAL_RULE_21:
	case PLURAL_RULE_22:
	case PLURAL_RULE_23:
	case PLURAL_RULE_24:
	case PLURAL_RULE_25:
		return 4;
	case PLURAL_RULE_26:
	case PLURAL_RULE_27:
		return 5;
	case PLURAL_RULE_28:
	case PLURAL_RULE_29:
		return 6;
	}
	return 0;
}
#endif

/* ------------------------------------------------------------------------- */

const char *_libnls_internal_dngettext(const libnls_domain *domain, const char *key1, const char *key2, unsigned long n)
{
	unsigned int hash;
	const nls_key_offset *chain;
	nls_key_offset cmp;
	
	/* check for empty string - often used - must return original address */
	if (key1 == NULL || *key1 == '\0' || key2 == NULL || *key2 == '\0' || domain->current_translation.hash == NULL)
		return key1;
	hash = nls_hash2(key1, key2);
	if ((chain = domain->current_translation.hash[hash]) != NULL)
	{
		while ((cmp = *chain++) != 0)
		{
			if (strcmp(&domain->keys[cmp], key1) == 0)
			{
				int nplurals = 0;
				int id = 0;

				/* strings are equal, return next string */
				key1 = &domain->current_translation.translations[*chain++];
				switch (domain->current_translation.plural_form)
				{
				case PLURAL_NONE:
					/* should not happen */
					nplurals = 1;
					break;
				case PLURAL_NOT_ONE:
					nplurals = 2;
					if (n != 1)
						id = 1;
					break;
				case PLURAL_GREATER_ONE:
					nplurals = 2;
					if (n > 1)
						id = 1;
					break;
				case PLURAL_RULE_8:
					nplurals = 3;
					if (n == 1)
						;
					else if (n >= 2 && n <= 4)
						id = 1;
					else
						id = 2;
					break;
				case PLURAL_RULE_9:
					nplurals = 3;
					if ((n % 10) == 1 && (n % 100) != 11)
						;
					else if ((n % 10) >= 2 && (n % 10) <= 4 && ((n % 100) < 10 || (n % 100) >= 20))
						id = 1;
					else
						id = 2;
					break;
				case PLURAL_RULE_10:
					nplurals = 3;
					if ((n % 10) == 1 && (n % 100) != 11)
						;
					else if (n != 0)
						id = 1;
					else
						id = 2;
					break;
				case PLURAL_RULE_11:
					nplurals = 3;
					if (n == 1)
						;
					else if ((n % 10) >= 2 && (n % 10) <= 4 && ((n %100) < 10 || (n % 100) >= 20))
						id = 1;
					else
						id = 2;
					break;
				case PLURAL_RULE_12:
					nplurals = 3;
					if (n == 1)
						;
					else if (n == 0 || ((n % 100) > 0 && (n % 100) < 20))
						id = 1;
					else
						id = 2;
					break;
				case PLURAL_RULE_16:
					nplurals = 3;
					if ((n % 10) == 1 && (n % 100) != 11)
						;
					else if ((n % 10) >= 2 && (n % 10) <= 4 && ((n % 100) < 12 || (n % 100) >= 14))
						id = 1;
					else
						id = 2;
					break;
				}
				if (id < nplurals)
				{
					/*
					 * find the plural translation
					 */
					while (--id >= 0)
					{
						while (*key1++ != '\0')
							;
					}
				} else
				{
					/*
				 	 * something went wrong. We evaluated an id that does not match the formula
				 	 */
				}
				return key1;
			}
			/* the strings differ, next */
			chain++;
		}
	}
	/* not in hash, return original string, using germanic plural */
	if (n != 1)
		key1 = key2;
	return key1;

}

/* ------------------------------------------------------------------------- */

/* Similar to 'gettext' but select the plural form corresponding to the
   number N.  */
const char *libnls_ngettext(const char *msgid1, const char *msgid2, unsigned long int n)
{
	if (_libnls_current_domain == NULL)
		return (dcngettext)(NULL, msgid1, msgid2, n, LC_MESSAGES);
	return _libnls_internal_dngettext(_libnls_current_domain, msgid1, msgid2, n);
}
