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

/* Look up MSGID in the DOMAINNAME message catalog for the current CATEGORY
   locale.  */
const char *libnls_dcgettext(const char *domainname, libnls_msgid_type msgid, int category)
{
	struct libnls_domain_list *d;
	
	if (category == LC_MESSAGES)
	{
		for (d = _libnls_all_domains; d != NULL; d = d->next)
		{
			if (strcmp(d->domain->package, domainname) == 0)
			{
				return _libnls_internal_dgettext(d->domain, msgid);
			}
		}
	}
	/*
	 * not our package, forward to libintl
	 */
#if 0
	return (dcgettext)(domainname, msgid, LC_MESSAGES);
#else
	return NLS_NOKEY_ERROR;
#endif
}
