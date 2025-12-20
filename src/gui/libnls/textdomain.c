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
#include <assert.h>

libnls_domain *_libnls_current_domain;
struct libnls_domain_list *_libnls_all_domains;

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

static void add_domain(libnls_domain *domain)
{
	struct libnls_domain_list *d;
	
	for (d = _libnls_all_domains; d != NULL; d = d->next)
		if (strcmp(d->domain->package, domain->package) == 0)
			return;
	d = malloc(sizeof(*d));
	assert(d);
	d->next = _libnls_all_domains;
	d->domain = domain;
	_libnls_all_domains = d;
}

/*** ---------------------------------------------------------------------- ***/

const char *libnls_bindtextdomain(const char *domainname, const char *dirname, libnls_domain *domain)
{
	if (strcmp(domainname, domain->package) != 0)
	{
		/*
		 * not our package, forward to libintl
		 */
		return (bindtextdomain)(domainname, dirname);
	}
	add_domain(domain);
	return NULL;
}

/*** ---------------------------------------------------------------------- ***/

const char *libnls_bind_textdomain_codeset(const char *domainname, const char *codeset)
{
	struct libnls_domain_list *d;
	
	for (d = _libnls_all_domains; d != NULL; d = d->next)
	{
		if (strcmp(d->domain->package, domainname) == 0)
		{
			if (codeset != NULL && strcmp(codeset, "UTF-8") != 0)
			{
				fprintf(stderr, "libnls: only UTF-8 supported\n");
			}
			return "UTF-8";
		}
	}
	/*
	 * not our package, forward to libintl
	 */
	return (bind_textdomain_codeset)(domainname, codeset);
}

/*** ---------------------------------------------------------------------- ***/

const char *libnls_textdomain(const char *domainname)
{
	struct libnls_domain_list *d;
	
	for (d = _libnls_all_domains; d != NULL; d = d->next)
	{
		if (strcmp(d->domain->package, domainname) == 0)
		{
			_libnls_current_domain = d->domain;
			return d->domain->package;
		}
	}
	/*
	 * not our package, forward to libintl
	 */
	return (textdomain)(domainname);
}
