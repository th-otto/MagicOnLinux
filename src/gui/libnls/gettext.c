#include <stdlib.h>
#include <string.h>
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

const char *_libnls_internal_dgettext(const libnls_domain *domain, const char *key)
{
	unsigned int hash;
	const nls_key_offset *chain;
	nls_key_offset cmp;
	
	/* check for empty string - often used - must return original address */
	if (key == NULL || *key == '\0' || domain->current_translation.hash == NULL)
		return key;
	hash = nls_hash(key);
	if ((chain = domain->current_translation.hash[hash]) != NULL)
	{
		while ((cmp = *chain++) != 0)
		{
			if (strcmp(&domain->keys[cmp], key) == 0)
			{
				/* strings are equal, return next string */
				key = &domain->current_translation.translations[*chain];
				break;
			}
			/* the strings differ, next */
			chain++;
		}
	}
	/* not in hash, return original string */
	return key;
}

/* ------------------------------------------------------------------------- */

/* Look up MSGID in the current default message catalog for the current
   LC_MESSAGES locale.  If not found, returns MSGID itself (the default
   text).  */
const char *libnls_gettext(const char *msgid)
{
	if (_libnls_current_domain == NULL)
		return (dgettext)(NULL, msgid);
	return _libnls_internal_dgettext(_libnls_current_domain, msgid);
}
