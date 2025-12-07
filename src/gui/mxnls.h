/* no translations currently */
#ifdef ENABLE_NLS
#include <libintl.h>
/* The separator between msgctxt and msgid in a .mo file.  */
#ifndef GETTEXT_CONTEXT_GLUE
#  define GETTEXT_CONTEXT_GLUE "\004"
#endif
#ifdef QT_VERSION
#define _(String) QString::fromUtf8(gettext(String)).toUtf8().constData()
#else
#define _(String) gettext(String)
#endif
#define N_(String) (String)
#define NC_(Context, String) Context GETTEXT_CONTEXT_GLUE String
#else
#define _(x) x
#define N_(x) x
#define NC_(Context, x) x
#define textdomain(domain)
#define bindtextdomain(domain, directory)
#define bind_textdomain_codeset(domain, codeset)
#endif

#ifndef PACKAGE_LOCALE_DIR
#define PACKAGE_LOCALE_DIR "/usr/share/locale"
#endif
