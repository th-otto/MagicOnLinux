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

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

/* Similar to 'dcgettext' but select the plural form corresponding to the
   number N.  */
const char *libnls_dcngettext(const char *domainname, libnls_msgid_type msgid1, libnls_msgid_type msgid2, unsigned long int n, int category)
{
	struct libnls_domain_list *d;
	
	if (category == LC_MESSAGES)
	{
		for (d = _libnls_all_domains; d != NULL; d = d->next)
		{
			if (strcmp(d->domain->package, domainname) == 0)
			{
				return _libnls_internal_dngettext(d->domain, msgid1, msgid2, n);
			}
		}
	}
	/*
	 * not our package, forward to libintl
	 */
#if 0
	return (dcngettext)(domainname, msgid1, msgid2, n, LC_MESSAGES);
#else
	return NLS_NOKEY_ERROR;
#endif
}
