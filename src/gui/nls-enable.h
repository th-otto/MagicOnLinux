#ifdef ENABLE_NLS
#ifdef FORCE_LIBINTL
#include <libintl.h>
#else
#include "libnls/libnls.h"
#define DECLARE_DOMAIN(package) extern libnls_domain __libnls_cat(package, _domain)
#endif
/* The separator between msgctxt and msgid in a .mo file.  */
#ifndef GETTEXT_CONTEXT_GLUE
#  define GETTEXT_CONTEXT_GLUE "\004"
#endif
#ifdef QT_VERSION
#define _(String) QString::fromUtf8(gettext(String)).toUtf8().constData()
#define P_(String1, String2, n) QString::fromUtf8(ngettext(String1, String2, n)).toUtf8().constData()
#else
#define _(String) gettext(String)
#define P_(String1, String2, n) ngettext(String1, String2, n)
#endif
#define N_(String) ((char *)((unsigned long)(String)))
#define NC_(Context, String) ((char *)((unsigned long)(String)))
#define C_(Context, String) gettext(NC_(Context, String))
#else
#define _(String) String
#define N_(String) String
#define NC_(Context, String) String
#define C_(Context, String) String
#define P_(String1, String2, n) ((n) != 1 ? String2 : String1)
#define textdomain(domain)
#define bindtextdomain(domain, directory)
#define bind_textdomain_codeset(domain, codeset)
#endif

#ifndef DECLARE_DOMAIN
#define DECLARE_DOMAIN(package)
#endif

#ifndef PACKAGE_LOCALE_DIR
#define PACKAGE_LOCALE_DIR "/usr/share/locale"
#endif
