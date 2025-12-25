#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "libnls.h"
#include "libnlsI.h"

static const char *_libnls_current_locale = "C";

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

char *libnls_setlocale(int category, const char *locale)
{
	char *ret;
	struct libnls_domain_list *d;
	
	ret = (setlocale)(category, locale);
	if (category == LC_ALL || category == LC_MESSAGES)
	{
		if (locale != NULL && *locale != '\0')
		{
			for (d = _libnls_all_domains; d != NULL; d = d->next)
			{
				libnls_domain *domain = d->domain;
				const libnls_translation *translations;
				
				domain->current_translation.translations = NULL;
				for (translations = domain->languages; /* translations->lang_id != NULL && */ translations->lang_id[0] != '\0'; translations++)
				{
					if (strncmp(locale, translations->lang_id, ISO639_CODE_LEN) == 0)
					{
						domain->current_translation = *translations;
						break;
					}
				}
				if (domain->current_translation.translations == NULL)
				{
					for (translations = domain->languages; /* translations->lang_id != NULL && */ translations->lang_id[0] != '\0'; translations++)
					{
						if (strncmp(locale, translations->lang_id, 2) == 0)
						{
							domain->current_translation = *translations;
							break;
						}
					}
				}
			}
			
			_libnls_current_locale = ret;
		}
	}
	return ret;
}
