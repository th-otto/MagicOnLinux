#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#undef gettext
#undef dgettext
#undef ngettext
#undef dngettext
#include <assert.h>
#include "libnls.h"
#include "libnlsI.h"

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

const char *_libnls_internal_dgettext(const libnls_domain *domain, libnls_msgid_type msgid)
{
	nls_key_offset offset;

	assert(msgid <= domain->num_keys);
	/* check for empty string - often used - must return original address */
	if (msgid == 0)
		return domain->keys;
	if (domain->current_translation.translations != NULL && (offset = domain->current_translation.offsets[msgid - 1]) != 0)
		return domain->current_translation.translations + offset;
	/* no translation, return original string */
	return domain->keys + domain->languages[0].offsets[msgid - 1];
}

/* ------------------------------------------------------------------------- */

/* Look up MSGID in the current default message catalog for the current
   LC_MESSAGES locale. If not found, returns MSGID itself (the default
   text). */
const char *libnls_gettext(libnls_msgid_type msgid)
{
	if (_libnls_current_domain == NULL)
#if 0
		return (dgettext)(NULL, msgid);
#else
		return NLS_NOKEY_ERROR;
#endif
	return _libnls_internal_dgettext(_libnls_current_domain, msgid);
}

/* ------------------------------------------------------------------------- */

/* Look up MSGID in the message catalog for the "C" locale.
   If not found, returns NULL. */
const char *libnls_gettext_clocale(libnls_msgid_type msgid)
{
	if (_libnls_current_domain == NULL)
		return NLS_NOKEY_ERROR;
	if (msgid == 0)
		return _libnls_current_domain->keys;
	return _libnls_current_domain->keys + _libnls_current_domain->languages[0].offsets[msgid - 1];
}
