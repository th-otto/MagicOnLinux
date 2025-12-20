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

/* Similar to 'dgettext' but select the plural form corresponding to the
   number N.  */
const char *libnls_dngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long int n)
{
	struct libnls_domain_list *d;
	
	for (d = _libnls_all_domains; d != NULL; d = d->next)
	{
		if (strcmp(d->domain->package, domainname) == 0)
		{
			return _libnls_internal_dngettext(d->domain, msgid1, msgid2, n);
		}
	}
	/*
	 * not our package, forward to libintl
	 */
	return (dcngettext)(domainname, msgid1, msgid2, n, LC_MESSAGES);
}
