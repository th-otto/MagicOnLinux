/* no translations currently */
#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext(String)
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif
#else
#define _(x) x
#define N_(x) x
#define textdomain(domain)
#define bindtextdomain(domain, directory)
#define bind_textdomain_codeset(domain, codeset)
#endif

enum {
	TYPE_NONE,
	TYPE_PATH,
	TYPE_FOLDER,
	TYPE_STRING,
	TYPE_INT,
	TYPE_UINT,
	TYPE_BOOL,
	TYPE_CHOICE
};

struct pref_val {
	int type;
	union {
		char *s;
		struct {
			char *p;
			int flags;
#define NO_FLAGS -1
#define DRV_FLAG_RDONLY         1   /* read-only */
#define DRV_FLAG_8p3            2   /* filenames in 8+3 format, uppercase */
#define DRV_FLAG_CASE_INSENS    4   /* case insensitive, e.g. (V)FAT or HFS(+) */
		} p;
		struct {
			long v;
			long minval;
			long maxval;
		} i;
		double d;
		gboolean b;
		struct {
			int c;
			int minval;
			int maxval;
		} c;
	} u;
};

