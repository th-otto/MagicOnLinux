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

const char *_libnls_internal_dngettext(const libnls_domain *domain, libnls_msgid_type msgid1, libnls_msgid_type msgid2, unsigned long n)
{
	nls_key_offset offset;
	
	/* check for empty string - often used - must return original address */
	if (msgid1 == 0 || msgid2 == 0)
		return domain->keys;
	if (domain->current_translation.translations != NULL && (offset = domain->current_translation.offsets[msgid1 - 1]) != 0)
	{
		const char *key = domain->current_translation.translations + offset;
		int nplurals = 0;
		int id = 0;

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
				while (*key++ != '\0')
					;
			}
		} else
		{
			/*
		 	 * something went wrong. We evaluated an id that does not match the formula
		 	 */
		}
		return key;
	}
	/* no translation, return original string, using germanic plural */
	if (n != 1)
		msgid1 = msgid2;
	return domain->keys + domain->languages[0].offsets[msgid1 - 1];
}

/* ------------------------------------------------------------------------- */

/* Similar to 'gettext' but select the plural form corresponding to the
   number N.  */
const char *libnls_ngettext(libnls_msgid_type msgid1, libnls_msgid_type msgid2, unsigned long int n)
{
	if (_libnls_current_domain == NULL)
#if 0
		return (dcngettext)(NULL, msgid1, msgid2, n, LC_MESSAGES);
#else
		return NULL;
#endif
	return _libnls_internal_dngettext(_libnls_current_domain, msgid1, msgid2, n);
}
