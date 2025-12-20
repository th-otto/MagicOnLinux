#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include "libnls/libnls.h"
#undef textdomain
#undef bindtextdomain
#undef bind_textdomain_codeset
#include "nls-enable.h"
#include "libnls/country.h"
#include "pofile.h"
#include "libnls/plural-exp.h"
#include "libnls/libnlsI.h"
#include "libnls/nlshash.h"

#define KINFO(x) errout x


struct lang_info
{
	char *name;
	int in_use;
};

typedef struct da
{
	size_t size;
	void **buf;
	size_t len;
} da;


#define PO_DIR "../po/"

#define INT_TO_PTR(p)    ((void *)(intptr_t)(p))
#define INT_FROM_POINTER(p) ((int)(intptr_t)(p))

/* this is also used by gettext() */
#define CONTEXT_GLUE '\004'

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/*
 * errors
 */

__attribute__((format(printf, 1, 2)))
static void warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	errout(_("Warning: "));
	erroutv(fmt, ap);
	errout("\n");
	va_end(ap);
}

/* ------------------------------------------------------------------------- */

__attribute__((format(printf, 1, 2)))
static void fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	errout(_("Fatal: "));
	erroutv(fmt, ap);
	errout("\n");
	va_end(ap);
	exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------------- */

__attribute__((format(printf, 1, 2)))
static void error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	errout(_("Error: "));
	erroutv(fmt, ap);
	errout("\n");
	va_end(ap);
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/
/*
 * memory
 */

#define g_malloc(n) xmalloc(n)
#define g_calloc(n, s) xcalloc((size_t)(n), (size_t)(s))
#define g_malloc0(n) xcalloc((size_t)(n), 1)
#define g_realloc(ptr, s) xrealloc(ptr, s)

#define g_new(t, n) ((t *)g_malloc((size_t)(n) * sizeof(t)))
#define g_new0(t, n) ((t *)g_malloc0((size_t)(n) * sizeof(t)))
#define g_renew(t, p, n) ((t *)g_realloc(p, (size_t)(n) * sizeof(t)))
#define g_strdup(s) xstrdup(s)

/* ------------------------------------------------------------------------- */

static void *xmalloc(size_t s)
{
	void *a = malloc(s);

	if (a == NULL)
		fatal("%s", strerror(errno));
	return a;
}

/* ------------------------------------------------------------------------- */

static void *xcalloc(size_t n, size_t s)
{
	void *a = calloc(n, s);

	if (a == NULL)
		fatal("%s", strerror(errno));
	return a;
}

/* ------------------------------------------------------------------------- */

static void *xrealloc(void *b, size_t s)
{
	void *a = realloc(b, s);

	if (a == NULL)
		fatal("%s", strerror(errno));
	return a;
}

/* ------------------------------------------------------------------------- */

static void g_free(void *s)
{
	if (s)
	{
		free(s);
	}
}

/* ------------------------------------------------------------------------- */

static char *xstrdup(const char *s)
{
	size_t len;
	char *a;

	if (s == NULL)
		return NULL;
	len = strlen(s);
	a = g_new(char, len + 1);
	strcpy(a, s);
	return a;
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/*
 * da - dynamic array
 */
#define DA_SIZE 1000

static void da_grow(da *d)
{
	if (d->size == 0)
	{
		d->size = DA_SIZE;
		d->buf = g_new(void *, d->size);
	} else
	{
		d->size *= 4;
		d->buf = g_renew(void *, d->buf, d->size);
	}
}

/* ------------------------------------------------------------------------- */

static da *da_new(void)
{
	da *d = g_new(da, 1);

	d->size = 0;
	d->len = 0;
	d->buf = NULL;
	return d;
}

/* ------------------------------------------------------------------------- */

static void da_free(da *d)
{
	if (d)
	{
		g_free(d->buf);
		g_free(d);
	}
}

/* ------------------------------------------------------------------------- */

static size_t da_len(da *d)
{
	return d->len;
}

/* ------------------------------------------------------------------------- */

static void *da_nth(da *d, size_t n)
{
	return d->buf[n];
}

/* ------------------------------------------------------------------------- */

static void da_add(da *d, void *elem)
{
	if (d->len >= d->size)
	{
		da_grow(d);
	}
	d->buf[d->len++] = elem;
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/*
 * str - string
 */

typedef struct str
{
	size_t size;
	size_t len;
	char *buf;
} str;

#define STR_SIZE 100

static str *s_new(void)
{
	str *s = g_new(str, 1);

	s->size = 0;
	s->len = 0;
	s->buf = NULL;
	return s;
}

/* ------------------------------------------------------------------------- */

static void s_grow(str *s)
{
	if (s->size == 0)
	{
		s->size = STR_SIZE;
		s->buf = g_new(char, s->size);
	} else
	{
		s->size *= 4;
		s->buf = g_renew(char, s->buf, s->size);
	}
}

/* ------------------------------------------------------------------------- */

static void s_free(str *s)
{
	if (s)
	{
		g_free(s->buf);
		g_free(s);
	}
}

/* ------------------------------------------------------------------------- */

static void s_addch(str *s, char c)
{
	if (s->len >= s->size)
	{
		s_grow(s);
	}
	s->buf[s->len++] = c;
}

/* ------------------------------------------------------------------------- */

static void s_addstr(str *s, const char *t)
{
	while (*t)
	{
		s_addch(s, *t++);
	}
}

/* ------------------------------------------------------------------------- */

static void s_addstrn(str *s, const char *t, size_t len)
{
	while (len)
	{
		s_addch(s, *t++);
		len--;
	}
}

/* ------------------------------------------------------------------------- */

static size_t s_length(str *s)
{
	return s->len;
}

/* ------------------------------------------------------------------------- */

/* add a trailing 0 if needed and release excess mem */
static char *s_close(str *s)
{
	if (s->size == 0)
	{
		if (s->buf == NULL)
		{
			s->buf = g_new(char, 1);
			s->buf[0] = 0;
		}
		return s->buf;
	}
	s->buf = g_renew(char, s->buf, s->len + 1);
	s->size = s->len + 1;
	s->buf[s->len] = '\0';
	return s->buf;
}

/* ------------------------------------------------------------------------- */

static char *s_detach(str *s)
{
	char *t = s_close(s);

	g_free(s);
	return t;
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/*
 * hi - hash item. This is intended to be aggregated by effective
 * hash item structures (a way to implement inheritance in C)
 */

typedef struct hash_item
{
	char *key;
} hi;

/*
 * hash - a hash will contain hash-items sorted by their hash
 * value.
 */

#define HASH_SIZ 10000

typedef struct hash
{
	da *d[HASH_SIZ];
} hash;

static hash *h_new(void)
{
	hash *h = g_new0(hash, 1);

	return h;
}

/* ------------------------------------------------------------------------- */

/* a dumb one */
static unsigned int compute_hash(const char *t, size_t keylen)
{
	unsigned int m = 0;

	while (keylen)
	{
		m += *t++;
		m <<= 1;
		keylen--;
	}
	return m;
}

/* ------------------------------------------------------------------------- */

static void *h_find(hash *h, const char *key, size_t keylen)
{
	unsigned int m = compute_hash(key, keylen) % HASH_SIZ;
	da *d;
	size_t i, n;
	hi *k;

	d = h->d[m];
	if (d != NULL)
	{
		n = da_len(d);
		for (i = 0; i < n; i++)
		{
			k = (hi *)da_nth(d, i);
			if (memcmp(key, k->key, keylen) == 0)
			{
				return k;
			}
		}
	}
	return NULL;
}

/* ------------------------------------------------------------------------- */

static void h_insert(hash *h, void *k, size_t keylen)
{
	unsigned int m = compute_hash(((hi *) k)->key, keylen) % HASH_SIZ;
	da *d;

	d = h->d[m];
	if (d == NULL)
	{
		d = da_new();
		h->d[m] = d;
	}
	da_add(d, k);
}

/* ------------------------------------------------------------------------- */

static void h_free(hash *h)
{
	size_t i;
	
	if (h)
	{
		for (i = 0; i < HASH_SIZ; i++)
			da_free(h->d[i]);
		g_free(h);
	}
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/*
 * poe - po-entries
 * the po structure is an ordered-hash of po-entries,
 * the po-entry being a sub-type of hash-item.
 */

#define KIND_NORM 0
#define KIND_COMM 1
#define KIND_OLD 2

typedef struct poe
{
	hi msgid;							/* the key (super-type) */
	int kind;							/* kind of entry */
	char *comment;						/* free user comments */
	da *refs;							/* the references to locations in code */
	char *refstr;						/* a char * representation of the references */
	str *msgkey;
	str *msgstr;						/* the translation */
	int nplurals;
} poe;

/*
 * oh - ordered hash
 */

typedef struct oh
{
	hash *h;
	da *d;
} oh;


static oh *o_new(void)
{
	oh *o = g_new(oh, 1);

	o->h = h_new();
	o->d = da_new();
	return o;
}

/* ------------------------------------------------------------------------- */

static void poe_free(poe *e)
{
	if (e)
	{
		g_free(e->msgid.key);
		g_free(e->comment);
		g_free(e->refstr);
		g_free(e->msgstr);
		da_free(e->refs);
		g_free(e);
	}
}

/* ------------------------------------------------------------------------- */

static void o_free(oh *o, _BOOL freeentries)
{
	size_t i, len;
	
	if (o)
	{
		h_free(o->h);
		
		if (o->d)
		{
			if (freeentries)
			{
				len = da_len(o->d);
				for (i = 0; i < len; i++)
				{
					poe_free((poe *)da_nth(o->d, i));
				}
			}
			da_free(o->d);
		}
		g_free(o);
	}
}

/* ------------------------------------------------------------------------- */

static poe *o_find(oh *o, const char *t, size_t keylen)
{
	return (poe *)h_find(o->h, t, keylen);
}

/* ------------------------------------------------------------------------- */

static void o_insert(oh *o, poe *k)
{
	da_add(o->d, k);
	h_insert(o->h, k, k->msgkey->len);
}

/* ------------------------------------------------------------------------- */

static void o_add(oh *o, poe *k)
{
	da_add(o->d, k);
}

/* ------------------------------------------------------------------------- */

static size_t o_len(oh *o)
{
	return da_len(o->d);
}

/* ------------------------------------------------------------------------- */

static poe *o_nth(oh *o, size_t n)
{
	return (poe *)da_nth(o->d, n);
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/*
 * ref - reference to locations in source files
 */

typedef struct ref
{
	const char *fname;
	int lineno;
} ref;

static poe *poe_new(str *msgid)
{
	poe *e = g_new0(poe, 1);

	e->msgid.key = msgid->buf;
	e->msgkey = msgid;
	e->kind = KIND_NORM;
	e->comment = NULL;
	e->refs = NULL;
	e->msgstr = NULL;
	e->refstr = NULL;
	e->nplurals = 0;
	return e;
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/*
 * gettext administrative entry, an entry with msgid empty, and
 * msgstr being specially formatted (example in doc/nls.txt)
 */

typedef struct
{
	char *lasttrans;
	char *langteam;
	char *charset;
	char *mimeversion;
	char *transfer_encoding;
	char *language;
	char *plural_form;
	str *other;
} ae_t;


static void free_pot_ae(ae_t *a)
{
	g_free(a->lasttrans);
	a->lasttrans = NULL;
	g_free(a->langteam);
	a->langteam = NULL;
	g_free(a->charset);
	a->charset = NULL;
	g_free(a->mimeversion);
	a->mimeversion = NULL;
	g_free(a->transfer_encoding);
	a->transfer_encoding = NULL;
	g_free(a->language);
	a->language = NULL;
	g_free(a->plural_form);
	a->plural_form = NULL;
	s_free(a->other);
	a->other = NULL;
}

/* ------------------------------------------------------------------------- */

static _BOOL parse_ae(po_domain *domain, const char *fname, char *msgstr, ae_t *a)
{
	char *c = msgstr;
	char *t;
	int m;
	_BOOL ret = TRUE;
	const char *from_charset = NULL;

	memset(a, 0, sizeof(*a));
	if (msgstr == NULL || *msgstr == '\0')
	{
		warn(_("Empty administrative entry"));
		ret = FALSE;
	} else
	{
#define AE_CHECK(s, f) \
		else if (strncmp(c, s, sizeof(s) - 1) == 0) \
		{ \
			char *val = c + sizeof(s) - 1; \
			while (*val == ' ') val++; \
			m = t - val; \
			a->f = g_new(char, m + 1); \
			memcpy(a->f, val, m); \
			a->f[m] = '\0'; \
		}
		
		for (;;)
		{
			t = strchr(c, '\n');
			if (t == NULL)
			{
				error(_("Fields in administrative entry must end with \\n"));
				return FALSE;
			}
			if (0) { }
			AE_CHECK("Last-Translator:", lasttrans)
			AE_CHECK("Language-Team:", langteam)
			AE_CHECK("MIME-Version:", mimeversion)
			AE_CHECK("Content-Type: text/plain; charset=", charset)
			AE_CHECK("Content-Transfer-Encoding:", transfer_encoding)
			AE_CHECK("Language:", language)
			AE_CHECK("Plural-Forms:", plural_form)
			else
			{
				m = t - c;
				/* warn(_("unsupported administrative entry %.*s"), m, c); */
				if (a->other == NULL)
					a->other = s_new();
				while (c <= t)
				{
					s_addch(a->other, *c);
					c++;
				}
			}
			c = t + 1;
			if (*c == '\0')
				break;
		}
#undef AE_CHECK
		if (a->other)
			s_close(a->other);
	
		if (a->lasttrans == NULL)
		{
			error(_("Expecting \"%s\" in administrative entry"), "Last-Translator");
			ret = FALSE;
		}
		if (a->langteam == NULL)
		{
			error(_("Expecting \"%s\" in administrative entry"), "Language-Team");
			ret = FALSE;
		}
		if (a->language == NULL || *a->language == '\0')
		{
			error(_("Expecting \"%s\" in administrative entry"), "Language");
			ret = FALSE;
		}
		if (a->mimeversion == NULL || strcmp(a->mimeversion, "1.0") != 0)
		{
			error(_("MIME version must be 1.0"));
			ret = FALSE;
		}
		if (a->transfer_encoding == NULL || strcmp(a->transfer_encoding, "8bit") != 0)
		{
			error(_("Content-Transfer-Encoding must be 8bit"));
			ret = FALSE;
		}
		from_charset = a->charset;
	}
	
	if (from_charset == NULL)
	{
		warn(_("%s: missing charset"), fname);
		from_charset = "UTF-8";
	}
	/*
	 * the messages.pot has an entry "charset=CHARSET" (literally),
	 * unless some of the keys are non-ascii
	 */
	if (strcmp(from_charset, "UTF-8") != 0 && strcmp(from_charset, "CHARSET") != 0)
	{
		error(_("%s: charset unsupported: %s"), fname, from_charset);
		ret = FALSE;
	}

	if (a->language != NULL)
	{
		if (strlen(domain->lang_id) > 2 ? strcmp(a->language, domain->lang_id) != 0 : strncmp(a->language, domain->lang_id, 2) != 0)
		{
			warn(_("%s: language '%s' does not match '%s' from LINGUAS"), fname, a->language, domain->lang_id);
		}
	}

	domain->plural_form = NULL;
	if (a->plural_form != NULL)
	{
		int nplurals;
		struct expression *pluralp;

		if (EXTRACT_PLURAL_EXPRESSION(a->plural_form, &pluralp, &nplurals))
		{
			char buf[200];
			const struct _nls_plural *plural;

			PLURAL_PRINT(pluralp, buf, sizeof(buf), FALSE);
			domain->nplurals = nplurals;
			for (plural = nls_plurals; plural->exp != NULL; plural++)
			{
				if (strcmp(plural->str, buf) == 0)
				{
					domain->plural_form = plural->id_str;
					break;
				}
			}
		} else
		{
			error(_("%s: syntax error in Plural-Forms entry"), fname);
			ret = FALSE;
		}
		FREE_EXPRESSION(pluralp);

		if (domain->plural_form == NULL)
		{
			error(_("%s: unknown Plural-Forms entry"), fname);
			ret = FALSE;
		}
	} else
	{
		warn(_("%s: missing Plural-Forms entry"), fname);
	}
	if (domain->plural_form == NULL)
	{
		domain->nplurals = 2;
		domain->plural_form = "PLURAL_NOT_ONE"; /* most common one */
	}

	if (ret == FALSE)
	{
		error(_("%s: bad administrative entry"), fname);
	}

	return ret;
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/*
 * input files
 */

#define BACKSIZ 10
#define READSIZ 512

typedef struct ifile
{
	int lineno;
	char *fname;
	FILE *fh;
	unsigned char buf[BACKSIZ + READSIZ];
	int size;
	int index;
	int ateof;
} IFILE;


static void irefill(IFILE *f)
{
	if (f->size > BACKSIZ)
	{
		memmove(f->buf, f->buf + f->size - BACKSIZ, BACKSIZ);
		f->size = BACKSIZ;
		f->index = f->size;
	}
	f->size += fread(f->buf + f->size, 1, READSIZ, f->fh);
}

/* ------------------------------------------------------------------------- */

static void iback(IFILE *f)
{
	if (f->index == 0)
	{
		fatal(_("too far backward"));
	} else
	{
		if (f->buf[f->index] == 012)
		{
			f->lineno--;
		}
		f->index--;
	}
}

/* ------------------------------------------------------------------------- */

static void ibackn(IFILE *f, int n)
{
	f->index -= n;
	if (f->index < 0)
	{
		fatal(_("too far backward"));
	}
}

/* ------------------------------------------------------------------------- */

static void ifclose(IFILE *f)
{
	if (f)
	{
		if (f->fh)
			fclose(f->fh);
		g_free(f->fname);
		g_free(f);
	}
}

/* ------------------------------------------------------------------------- */

static IFILE *ifopen(const char *fname)
{
	IFILE *f = g_new0(IFILE, 1);

	f->fname = g_strdup(fname);
	f->fh = fopen(fname, "rb");
	if (f->fh == NULL)
	{
		ifclose(f);
		return NULL;
	}
	f->size = 0;
	f->index = 0;
	f->ateof = 0;
	f->lineno = 1;
	return f;
}

/* ------------------------------------------------------------------------- */

static int igetc(IFILE *f)
{
	if (f->index >= f->size)
	{
		irefill(f);
		if (f->index >= f->size)
		{
			f->ateof = 1;
			return EOF;
		}
	}
	return f->buf[f->index++];
}

/* ------------------------------------------------------------------------- */

/* returns the next logical char, in sh syntax */
static int inextsh(IFILE *f)
{
	int ret;

	ret = igetc(f);
	if (ret == 015)
	{
		ret = igetc(f);
		if (ret == 012)
		{
			/* found CR/LF */
			f->lineno++;
			return '\n';
		} else
		{
			/* found sole CR */
			iback(f);
			f->lineno++;
			return '\n';
		}
	} else if (ret == 012)
	{
		/* found sole LF */
		f->lineno++;
		return '\n';
	} else
	{
		return ret;
	}
}

/* ------------------------------------------------------------------------- */

/* returns the next logical char, in C syntax */
static int inextc(IFILE *f)
{
	int ret;

  again:
	ret = igetc(f);
	/* look ahead if backslash new-line */
	if (ret == '\\')
	{
		ret = igetc(f);
		if (ret == 015)
		{
			ret = igetc(f);
			if (ret == 012)
			{
				f->lineno++;
				goto again;
			} else
			{
				ibackn(f, 2);
				return '\\';
			}
		} else if (ret == 012)
		{
			f->lineno++;
			goto again;
		} else
		{
			iback(f);
			return '\\';
		}
	} else if (ret == 015)
	{
		ret = igetc(f);
		if (ret == 012)
		{
			f->lineno++;
			return '\n';
		} else
		{
			iback(f);
			return 015;
		}
	} else if (ret == 012)
	{
		f->lineno++;
		return '\n';
	} else
	{
		return ret;
	}
}

/* ------------------------------------------------------------------------- */

#define is_white(c)  (((c)== ' ') || ((c) == '\t') || ((c) == '\f'))
#define is_letter(c) ((((c) >= 'a') && ((c) <= 'z')) || (((c) >= 'A') && ((c) <= 'Z')))
#define is_digit(c)  (((c) >= '0') && ((c) <= '9'))
#define is_octal(c)  (((c) >= '0') && ((c) <= '7'))
#define is_hexdig(c) ((((c) >= 'a') && ((c) <= 'f')) || (((c) >= 'A') && ((c) <= 'F')))
#define is_hex(c)    (is_digit(c) || is_hexdig(c))

/* only one "..." string will be appended to string s */
static int get_c_string(IFILE *f, str *s)
{
	int c;

	c = inextc(f);
	if (c != '"')
	{
		iback(f);
		return FALSE;
	}
	for (;;)
	{
		c = inextc(f);
		if (c == EOF)
		{
			warn(_("%s:%d: EOF reached inside string"), f->fname, f->lineno);
			return FALSE;
		} else if (c == '\\')
		{
			c = inextc(f);
			if (c == EOF)
			{
				warn(_("%s:%d: EOF reached inside string"), f->fname, f->lineno);
				return FALSE;
			} else if (is_octal(c))
			{
				int i;
				int a = c - '0';

				c = inextc(f);
				for (i = 0; i < 3 && is_octal(c); i++)
				{
					a <<= 3;
					a += (c - '0');
					c = inextc(f);
				}
				if (a == 0)
				{
					error(_("%s:%d: EOS inside string"), f->fname, f->lineno);
					return FALSE;
				}
				s_addch(s, a);
				iback(f);
			} else if (c == 'x')
			{
				int a = 0;

				c = inextc(f);
				while (is_hex(c))
				{
					a <<= 4;
					if (c <= '9')
					{
						a += (c - '0');
					} else if (c <= 'F')
					{
						a += (c - 'A' + 10);
					} else
					{
						a += (c - 'a' + 10);
					}
					c = inextc(f);
				}
				if (a == 0)
				{
					error(_("%s:%d: EOS inside string"), f->fname, f->lineno);
					return FALSE;
				}
				s_addch(s, a);
				iback(f);
			} else
			{
				switch (c)
				{
				case 'a':
					c = '\a';
					break;
				case 'b':
					c = '\b';
					break;
				case 'v':
					warn(_("%s:%d: internationalized messages should not contain the '\\v' escape sequence"), f->fname, f->lineno);
					c = '\v';
					break;
				case 'e':
					c = 033;
					break;				/* GNU C extension: \e for escape */
				case 'f':
					c = '\f';
					break;
				case 'r':
					c = '\r';
					break;
				case 't':
					c = '\t';
					break;
				case 'n':
					c = '\n';
					break;
				case '\\':
				case '"':
				case '\'':
					break;
				default:
					error(_("%s:%d: invalid control sequence '\\%c'"), f->fname, f->lineno, c);
					return FALSE;
				}
				s_addch(s, c);
			}
		} else if (c == '\"')
		{
			return TRUE;
		} else
		{
			s_addch(s, c);
		}
	}
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/*
 * read a simple textfile,
 * ignoring lines that start with a #,
 * and empty lines
 */

/* decomposes a string <lang>, adding
 * a lang_info item from it to the array
 */
static void parse_linguas_item(da *d, char *s, int in_use)
{
	struct lang_info *info;
	char *p = s;
	
	while (is_letter(*p) || *p == '_')
		p++; 
	if (p > s && (p - s) <= ISO639_CODE_LEN && (is_white(*p) || *p == '\0'))
	{
		*p = '\0';
		info = g_new(struct lang_info, 1);
		info->name = s;
		info->in_use = in_use;
		da_add(d, info);
		return;
	}
	warn(_("LINGUAS: bad lang specification \"%s\""), s);
	g_free(s);
}

/* ------------------------------------------------------------------------- */

static _BOOL parse_oipl_file(const char *fname, da *d)
{
	int c;
	IFILE *f;

	f = ifopen(fname);
	if (f == NULL)
	{
		error(_("could not open %s: %s"), fname, strerror(errno));
		return FALSE;
	}
	for (;;)
	{
		c = inextsh(f);
		if (c == EOF)
		{
			break;
		} else if (c == '#')
		{
			while (c != EOF && c != '\n')
			{
				c = inextsh(f);
			}
		} else if (c == ' ' || c == '\t')
		{
			while (c == ' ' || c == '\t')
			{
				c = inextsh(f);
			}
			if (c != EOF && c != '\n')
			{
				warn(_("%s:%d: syntax error"), fname, f->lineno);
				while (c != EOF && c != '\n')
				{
					c = inextsh(f);
				}
			}
		} else if (c == '\n')
		{
			continue;
		} else
		{
			str *s = s_new();

			while (c != EOF && c != '\n')
			{
				if (c != '\r')
					s_addch(s, c);
				c = inextsh(f);
			}
			parse_linguas_item(d, s_detach(s), TRUE);
		}
	}
	ifclose(f);
	return TRUE;
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/*
 * parse po files
 */
static _BOOL parse_po_file(po_domain *domain, const char *fname, oh *o)
{
	int c;
	IFILE *f;
	poe *e;
	str *s;
	str *userstr;
	str *refstr;
	str *otherstr;
	str *msgid;
	str *msgid_plural;
#define MAX_PLURALS 3
	str *msgstr[MAX_PLURALS];
	str *msgctxt;
	str *entrytype;
	int nplurals;
	_BOOL retval = TRUE;
	
	f = ifopen(fname);
	if (f == NULL)
	{
		error(_("could not open %s: %s"), fname, strerror(errno));
		return FALSE;
	}
	entrytype = s_new();
	for (;;)
	{
		c = inextsh(f);
		/* skip any blank line before next entry */
		while (c == ' ' || c == '\t')
		{
			while (c == ' ' || c == '\t')
			{
				c = inextsh(f);
			}
			if (c != EOF && c != '\n')
			{
				warn(_("%s:%d: syntax error"), fname, f->lineno);
				while (c != EOF && c != '\n')
				{
					c = inextsh(f);
				}
			}
			c = inextsh(f);
		}
		if (c == EOF)
		{
			break;
		}

		/* start an entry */
		userstr = NULL;
		refstr = NULL;
		otherstr = NULL;
		msgid = NULL;
		msgid_plural = NULL;
		msgstr[0] = NULL;
		msgstr[1] = NULL;
		msgstr[2] = NULL;
		msgctxt = NULL;
		while (c == '#')
		{
			c = inextsh(f);
			switch (c)
			{
			case '\n':
			case ' ':					/* user comment */
				if (!userstr)
					userstr = s_new();
				s = userstr;
				break;
			case ':':					/* ref comment */
				if (!refstr)
					refstr = s_new();
				s = refstr;
				break;
			case '.':					/* translator comment */
			case ',':					/* gettext flags (c-format etc.)  */
			default:					/* other comment */
				if (!otherstr)
					otherstr = s_new();
				s = otherstr;
				break;
			}
			/* accumulate this comment line to the string */
			s_addch(s, '#');
			if (c == EOF)
			{
				s_addch(s, '\n');
				break;
			}
			s_addch(s, c);
			if (c != '\n')
			{
				while (c != EOF && c != '\n')
				{
					c = inextsh(f);
					s_addch(s, c);
				}
				if (c == EOF)
				{
					s_addch(s, '\n');
				}
			}
			c = inextsh(f);
		}
		if (c == ' ' || c == '\t' || c == '\n' || c == EOF)
		{
			/* the previous entry is a pure comment */
			if (userstr)
			{
				if (otherstr)
				{
					s_addstr(userstr, s_close(otherstr));
					s_free(otherstr);
				}
			} else if (otherstr)
			{
				userstr = otherstr;
			} else
			{
				if (refstr)
				{
					s_free(refstr);
					warn(_("%s:%d: stray ref ignored"), fname, f->lineno);
				}
				/* we will reach here when an entry is followed by more than one
				 * empty line, at each additional empty line.
				 */
				continue;
			}
			s = s_new();
			s_close(s);
			e = poe_new(s);
			e->comment = s_detach(userstr);
			e->kind = KIND_COMM;
			o_add(o, e);
			continue;
		}
		do
		{
			entrytype->len = 0;
			while (c != EOF && c != '\n' && c != ' ' && c != '\t')
			{
				s_addch(entrytype, c);
				c = inextsh(f);
			}
			s_addch(entrytype, '\0');
			while (c == ' ' || c == '\t')
			{
				c = inextsh(f);
			}
			if (c != '\"')
				goto err;
			s = s_new();
			/* accumulate all consecutive strings (separated by spaces) */
			do
			{
				iback(f);
				get_c_string(f, s);
				c = inextsh(f);
				while (c == ' ' || c == '\t')
				{
					c = inextsh(f);
				}
				if (c == EOF)
					break;
				if (c != '\n')
					goto err;
				c = inextsh(f);
			} while (c == '\"');
			if (strcmp(entrytype->buf, "msgid") == 0)
			{
				msgid = s;
			} else if (strcmp(entrytype->buf, "msgid[0]") == 0)
			{
				msgid = s;
			} else if (strcmp(entrytype->buf, "msgid[1]") == 0)
			{
				msgid_plural = s;
			} else if (strcmp(entrytype->buf, "msgid_plural") == 0)
			{
				msgid_plural = s;
			} else if (strcmp(entrytype->buf, "msgstr") == 0)
			{
				msgstr[0] = s;
			} else if (strcmp(entrytype->buf, "msgstr[0]") == 0)
			{
				msgstr[0] = s;
			} else if (strcmp(entrytype->buf, "msgstr[1]") == 0)
			{
				msgstr[1] = s;
			} else if (strcmp(entrytype->buf, "msgstr[2]") == 0)
			{
				msgstr[2] = s;
			} else if (strcmp(entrytype->buf, "msgctxt") == 0)
			{
				msgctxt = s;
			} else
			{
				error(_("%s:%d: unsupported entry %s"), fname, f->lineno, entrytype->buf);
				s_free(s);
				retval = FALSE;
			}
		} while (c != '\n' && c != EOF);
		
		if (c != '\n' && c != EOF)
			goto err;
		/* put the comment in userstr */
		if (userstr)
		{
			if (otherstr)
			{
				s_addstr(userstr, s_close(otherstr));
				s_free(otherstr);
				otherstr = 0;
			}
		} else if (otherstr)
		{
			userstr = otherstr;
			otherstr = 0;
		}
		nplurals = 0;
		if (msgid_plural != NULL)
		{
			if (msgstr[2] != NULL && msgstr[1] != NULL)
				nplurals = 3;
			else if (msgstr[1] != NULL)
				nplurals = 2;
			else if (msgstr[0] != NULL)
				nplurals = 1;
		}
		if (msgid == NULL || msgstr[0] == NULL)
		{
			error(_("%s:%d: missing msgid"), fname, f->lineno);
			s_free(msgid);
			s_free(msgstr[0]);
			s_free(msgctxt);
			retval = FALSE;
		} else if ((msgid_plural != NULL && msgstr[1] == NULL) ||
			(msgid_plural == NULL && msgstr[1] != NULL) ||
			(msgid_plural != NULL && nplurals != domain->nplurals))
		{
			error(_("%s:%d: wrong number of plural forms"), fname, f->lineno);
			s_free(msgid);
			s_free(msgstr[0]);
			s_free(msgctxt);
			retval = FALSE;
		} else
		{
			/* now we have the complete entry */
			if (msgid_plural != NULL)
			{
				s_addch(msgid, '\0');
				s_close(msgid_plural);
				s_addstr(msgid, msgid_plural->buf);
			}
			s_close(msgid);
			if (msgctxt != NULL)
			{
				s_addch(msgctxt, CONTEXT_GLUE);
				s_addstrn(msgctxt, msgid->buf, msgid->len);
				s_free(msgid);
				s_close(msgctxt);
				msgid = msgctxt;
			}
			if (msgid_plural != NULL)
			{
				/*
				 * Note: there is only 1 plural key, but up to 6 translations
				 * (only 3 are supported here)
				 */
				/* add plural strings to msgstr */
				if (msgstr[2] != NULL && msgstr[1] != NULL)
				{
					s_addch(msgstr[0], '\0');
					s_close(msgstr[1]);
					s_addstr(msgstr[0], msgstr[1]->buf);
					s_free(msgstr[1]);
					msgstr[1] = NULL;
					s_addch(msgstr[0], '\0');
					s_close(msgstr[2]);
					s_addstr(msgstr[0], msgstr[2]->buf);
					s_free(msgstr[2]);
					msgstr[2] = NULL;
				} else if (msgstr[1] != NULL)
				{
					s_addch(msgstr[0], '\0');
					s_close(msgstr[1]);
					s_addstr(msgstr[0], msgstr[1]->buf);
					s_free(msgstr[1]);
					msgstr[1] = NULL;
				}
			}
			e = o_find(o, msgid->buf, msgid->len);
			if (e)
			{
				warn(_("%s:%d: duplicate entry %s"), f->fname, f->lineno, msgid->buf); /* FIXME: may print CONTEXT_GLUE in msgid */
				s_free(msgid);
				s_free(msgstr[0]);
				retval = FALSE;
			} else
			{
				e = poe_new(msgid);
				if (msgstr[0] != NULL)
					s_close(msgstr[0]);
				e->msgstr = msgstr[0];
				e->nplurals = nplurals;
				if (e->msgid.key && *e->msgid.key != '\0' && e->msgstr != NULL && e->msgstr->len != 0)
				{
					size_t lp = e->msgkey->len;
					size_t ln = e->msgstr->len;
					if ((e->msgid.key[lp - 1] == '\n' && e->msgstr->buf[ln - 1] != '\n') ||
						(e->msgid.key[lp - 1] != '\n' && e->msgstr->buf[ln - 1] == '\n'))
					{
						warn(_("%s:%d: entries do not both end with '\\n' in translation of '%s' to '%s'"),
							f->fname, f->lineno, e->msgid.key, e->msgstr->buf);
					}
					if ((e->msgid.key[0] == '\n' && e->msgstr->buf[0] != '\n') ||
						(e->msgid.key[0] != '\n' && e->msgstr->buf[0] == '\n'))
					{
						warn(_("%s:%d: entries do not both begin with '\\n' in translation of '%s' to '%s'"),
							f->fname, f->lineno, e->msgid.key, e->msgstr->buf);
					}
				}
				if (refstr)
				{
					e->refstr = s_detach(refstr);
					refstr = NULL;
				}
				if (userstr)
				{
					e->comment = s_detach(userstr);
					userstr = NULL;
				}
				o_insert(o, e);
				if (e->msgid.key[0] == '\0')
				{
					ae_t a;

					/* get the source charset from the po file */
					memset(&a, 0, sizeof(a));
					if (parse_ae(domain, fname, e->msgstr->buf, &a) == FALSE)
						retval = FALSE;
					free_pot_ae(&a);
				}
			}
		}
		/* free temp strings */
		s_free(refstr);
		s_free(otherstr);
		s_free(userstr);
		s_free(msgid_plural);
		s_free(msgstr[1]);
		s_free(msgstr[2]);
		continue;
	  err:
		warn(_("%s:%d: syntax error (c = '%c')"), fname, f->lineno, c);
		while (c != '\n' && c != EOF)
		{
			c = inextsh(f);
		}
		retval = FALSE;
	}
	s_free(entrytype);

	ifclose(f);
	return retval;
}

/* ------------------------------------------------------------------------- */

po_domain **po_init(const char *po_dir)
{
	str *s;
	da *d;
	size_t i;
	size_t n;
	char *fname;
	po_domain **languages;

	if (po_dir == NULL)
		po_dir = PO_DIR;
	s = s_new();
	s_addstr(s, po_dir);
	i = strlen(po_dir);
	if (i > 0 && po_dir[i - 1] != '/')
		s_addstr(s, "/");
	s_addstr(s, "LINGUAS");
	fname = s_detach(s);
	d = da_new();
	if (!parse_oipl_file(fname, d))
	{
		da_free(d);
		g_free(fname);
		return NULL;
	}
	g_free(fname);
	n = da_len(d);
	if (n == 0)
	{
		warn(_("LINGUAS: no languages found"));
		languages = NULL;
	} else
	{
		languages = g_new(po_domain *, n + 1);
		for (i = 0; i < n; i++)
		{
			struct lang_info *info = (struct lang_info *)da_nth(d, i);
			languages[i] = g_new0(po_domain, 1);
			languages[i]->lang_id = info->name;
			g_free(info);
		}
		languages[n] = NULL;
	}
	da_free(d);
	return languages;
}

/* ------------------------------------------------------------------------- */

void po_exit(po_domain **languages)
{
	size_t i;
	
	for (i = 0; languages[i] != NULL; i++)
	{
		po_domain *domain = languages[i];
		po_delete_hash(domain);
		g_free(domain->lang_id);
		g_free(domain);
	}
	g_free(languages);
}

/* ------------------------------------------------------------------------- */

/*
 * load po file
 */
static oh *po_load(po_domain *domain, const char *po_dir, _BOOL report_translations)
{
	oh *o;
	poe *e;
	size_t i, n;
	unsigned long numtransl = 0;				/* number of translated entries */
	unsigned long numuntransl = 0;				/* number of untranslated entries */
	_BOOL retval = FALSE;
	char *fname;
	str *s;

	if (po_dir == NULL)
		po_dir = PO_DIR;
	
	s = s_new();
	s_addstr(s, po_dir);
	i = strlen(po_dir);
	if (i > 0 && po_dir[i - 1] != '/')
		s_addstr(s, "/");
	s_addstr(s, domain->lang_id);
	s_addstr(s, ".po");
	fname = s_detach(s);
	
	o = o_new();
	if (parse_po_file(domain, fname, o))
	{
		retval = TRUE;
		n = o_len(o);
		for (i = 0; i < n; i++)
		{
			e = o_nth(o, i);
			if (e->kind == KIND_COMM)
			{
				/* comment, ignore */
			} else if (e->msgid.key[0] == 0)
			{
				/* the old admin entry - do nothing */
			} else
			{
				if (e->msgstr && e->msgstr->len != 0)
				{
					numtransl++;
				} else
				{
					if (report_translations)
					{
						KINFO((_("%s.po: untranslated: '%s'\n"), domain->lang_id, e->msgid.key));
					}
					numuntransl++;
				}
			}
		}
	
		/* print stats */
		if (report_translations)
		{
			KINFO((_("%s.po: translated %lu, untranslated %lu\n"), domain->lang_id, numtransl, numuntransl));
		}
	}
	
	if (!retval)
	{
		o_free(o, TRUE);
		o = NULL;
	}
	
	g_free(fname);
	
	return o;
}

static _BOOL key_equal(str *msgkey, str *msgstr)
{
	size_t keylen = msgkey->len;
	const char *key = msgkey->buf;
	return memcmp(key, msgstr->buf, keylen) == 0;
}


_BOOL po_create_hash(po_domain *domain, const char *po_dir, _BOOL report_translations)
{
	oh *o;
	da *th[TH_SIZE];
	size_t i, n;
	str *keys;
	str *translations;
	nls_key_offset **hash;
	_BOOL ret = TRUE;
	unsigned long num_collisions;
	unsigned long num_keys;

	domain->tos_country_code = language_from_name(domain->lang_id);
	if (domain->tos_country_code == LANG_SYSTEM)
	{
		warn(_("unknown language %s"), domain->lang_id);
	}
	o = po_load(domain, po_dir, report_translations);
	if (o == NULL)
		return FALSE;
	
	/* clear target hash */
	for (i = 0; i < TH_SIZE; i++)
	{
		th[i] = NULL;
	}
	
	keys = s_new();
	translations = s_new();
	/*
	 * must reserve first byte of keys/translations,
	 * since an offset of 0 terminates the hash table
	 */
	s_addch(keys, '\0');
	s_addch(translations, '\0');
	
	n = o_len(o);
	num_collisions = 0;
	num_keys = 0;
	for (i = 0; i < n; i++)
	{
		poe *e = o_nth(o, i);
		
		if (e->kind == KIND_NORM && e->msgid.key[0] != '\0')
		{
			unsigned int a = nls_hash(e->msgid.key);

			num_keys++;
			if (th[a] == NULL)
				th[a] = da_new();
			else
				num_collisions++;
			da_add(th[a], INT_TO_PTR(s_length(keys)));
			s_addstrn(keys, e->msgid.key, e->msgkey->len);
			s_addch(keys, '\0');
			if (e->msgstr && e->msgstr->len != 0 && !key_equal(e->msgkey, e->msgstr))
			{
				da_add(th[a], INT_TO_PTR(s_length(translations)));
				s_addstrn(translations, e->msgstr->buf, e->msgstr->len);
				s_addch(translations, '\0');
			} else
			{
				da_add(th[a], NULL);
			}
		}
	}

	/* create the nls hash table */
	hash = g_new0(nls_key_offset *, TH_SIZE);
	for (i = 0; i < TH_SIZE; i++)
	{
		if (th[i])
		{
			size_t ii, nn;
			nls_key_offset *t;
			nls_key_offset num_trans;
			
			nn = da_len(th[i]);
			t = g_new(nls_key_offset, nn + 2);
			/*
			 * length of chain in first slot
			 */
			t[0] = nn;
			num_trans = 0;
			hash[i] = t;
			for (ii = 0; ii < nn; ii += 2)
			{
				t[ii + 2] = INT_FROM_POINTER(da_nth(th[i], ii + 0));
				t[ii + 3] = INT_FROM_POINTER(da_nth(th[i], ii + 1));
				if (da_nth(th[i], ii + 1) != NULL)
					num_trans++;
			}			
			/*
			 * number of non-empty translations for this chain in 2nd slot
			 */
			t[1] = num_trans;
			da_free(th[i]);
		}
	}
	
	if (report_translations)
	{
		KINFO((_("%s.po: keys %lu (%lu bytes), translations %lu bytes\n"), domain->lang_id, num_keys, (unsigned long)s_length(keys), (unsigned long)s_length(translations)));
	}
	if (sizeof(nls_key_offset) < sizeof(size_t))
	{
		if (s_length(keys) >= (size_t)1 << (8 * sizeof(nls_key_offset)))
		{
			error(_("%s: length of keys %lu does not fit into nls_key_offset"), domain->lang_id, (unsigned long)s_length(keys));
			ret = FALSE;
		}
		if (s_length(translations) >= (size_t)1 << (8 * sizeof(nls_key_offset)))
		{
			error(_("%s: length of translations %lu does not fit into nls_key_offset"), domain->lang_id, (unsigned long)s_length(translations));
			ret = FALSE;
		}
	}
	domain->hash = hash;
	domain->keys_len = s_length(keys);
	domain->keys = s_detach(keys);
	domain->translations_len = s_length(translations);
	domain->translations = s_detach(translations);

	o_free(o, TRUE);
	
	return ret;
}

/* ------------------------------------------------------------------------- */

void po_delete_hash(po_domain *domain)
{
	nls_key_offset **hash;
	nls_key_offset *t;
	int i;
	
	hash = domain->hash;
	if (hash != NULL)
	{
		for (i = 0; i < TH_SIZE; i++)
		{
			t = hash[i];
			if (t != NULL)
			{
				g_free(t);
			}
		}
		g_free(hash);
		domain->hash = NULL;
	}
	g_free(domain->translations);
	domain->translations = NULL;
	g_free(domain->keys);
	domain->keys = NULL;
}

/* ------------------------------------------------------------------------- */

static void put_escaped(unsigned char c, FILE *out, int cont)
{
	switch (c)
	{
	case '\a':
		fputs("\\a", out);
		break;
	case '\b':
		fputs("\\b", out);
		break;
	case '\v':
		fputs("\\v", out);
		break;
	case '\e':
		fputs("\\e", out);
		break;
	case '\f':
		fputs("\\f", out);
		break;
	case '\r':
		fputs("\\r", out);
		break;
	case '\t':
		fputs("\\t", out);
		break;
	case '\n':
		fputs("\\n", out);
		if (cont)
			fputs("\"\n\t            \"", out);
		break;
	case '"':
		fputs("\\\"", out);
		break;
	case '\\':
		fputs("\\\\", out);
		break;
	default:
		if (c < ' ' || c >= 127u)
		{
			fprintf(out, "\\%03o", c);
		} else
		{
			putc(c, out);
		}
		break;
	}
}

/* ------------------------------------------------------------------------- */

void po_dump_keys(po_domain *domain, FILE *out)
{
	const char *s;
	unsigned char c;
	size_t i, len;

	fprintf(out,
		"/*\n"
		" * generated by nlstool -- DO NOT EDIT\n"
		" */\n"
		"\n"
		"#include <stddef.h>\n"
		"#include \"libnls/libnls.h\"\n"
		"#include \"libnls/country.h\"\n"
		"\n");
	if (domain == NULL || domain->hash == NULL)
		return;

	/*
	 * dump the actual key strings
	 */
	fprintf(out,
		"/*\n"
		" * The keys for hash tables below.\n"
		" */\n"
		"static char const nls_key_strings[] =\n"
		"{\n");
	s = domain->keys;
	len = domain->keys_len;
	for (i = 0; i < len; )
	{
		fprintf(out, "\t/* %5u */ \"", (unsigned int)i);
		while (i < len && s[i] != '\0')
		{
			c = s[i++];
			put_escaped(c, out, i < len && s[i] != '\0');
		}
		i++;
		fputs("\\0\"\n", out);
	}
	fprintf(out, "\t/* %5u */\n", (unsigned int)len);
	fprintf(out, "};\n\n");
}

/* ------------------------------------------------------------------------- */

void po_dump_hash(po_domain *domain, FILE *out)
{
	unsigned int hash;
	const nls_key_offset *chain;
	const char *s;
	unsigned char c;
	size_t i, len;

	if (domain == NULL || domain->hash == NULL)
		return;

	/*
	 * dump the actual translation strings
	 */
	fprintf(out,
		"/*\n"
		" * translations for lang %s\n"
		" */\n"
		"static char const msg_%s_translations[] = {\n",
		domain->lang_id, domain->lang_id);
	s = domain->translations;
	len = domain->translations_len;
	for (i = 0; i < len; )
	{
		fprintf(out, "\t/* %5u */ \"", (unsigned int)i);
		while (i < len && s[i] != '\0')
		{
			c = s[i++];
			put_escaped(c, out, i < len && s[i] != '\0');
		}
		i++;
		fputs("\\0\"\n", out);
	}
	fprintf(out, "\t/* %5u */\n", (unsigned int)len);
	fprintf(out, "};\n\n");

	/*
	 * dump the hash table contents
	 */
	fprintf(out,
		"/*\n"
		" * hash table for lang %s\n"
		" */\n", domain->lang_id);
	for (hash = 0; hash < TH_SIZE; hash++)
	{
		if ((chain = domain->hash[hash]) != NULL)
		{
			/* get count & translations from first slot2 */
			nls_key_offset count = *chain++;
			nls_key_offset num_trans = *chain++;
			
			if (num_trans != 0)
			{
				fprintf(out, "static nls_key_offset const msg_%s_hash_%u[] = {\n", domain->lang_id, hash);
				while (count > 0)
				{
					nls_key_offset key = *chain++;
					nls_key_offset trans = *chain++;
					if (trans != 0)
						fprintf(out, "\t%u, %u,\n", key, trans);
					count -= 2;
				}
				fprintf(out, "\t0\n};\n\n");
			}
		}
	}

	/*
	 * dump the hash table pointers
	 */
	fprintf(out, "static const nls_key_offset *const msg_%s[] = {\n", domain->lang_id);
	for (hash = 0; hash < TH_SIZE; hash++)
	{
		nls_key_offset num_trans = 0;

		if ((chain = domain->hash[hash]) != NULL)
		{
			chain++;
			num_trans = *chain;
		}
		if (num_trans != 0)
			fprintf(out, "\tmsg_%s_hash_%u", domain->lang_id, hash);
		else
			fputs("\tNULL", out);
		if (hash + 1 < TH_SIZE)
			putc(',', out);
		putc('\n', out);
	}
	fprintf(out, "};\n\n");

#if 0
	{
		const char *tos_country_str;
		char buf[20];

		/*
		 * dump the domain
		 */
		tos_country_str = NULL;
		for (i = 0; i < sizeof(tos_country_strings) / sizeof(tos_country_strings[0]); i++)
		{
			if (tos_country_strings[i].code == domain->tos_country_code)
			{
				tos_country_str = tos_country_strings[i].str;
				break;
			}
		}

		if (tos_country_str == NULL)
		{
			sprintf(buf, "%d", domain->tos_country_code);
			tos_country_str = buf;
		}
		fprintf(out, "static const libnls_translation lang_%s = { %s, \"%s\", %s, msg_%s_translations, msg_%s };\n\n\n",
			domain->lang_id,
			tos_country_str,
			domain->lang_id,
			domain->plural_form,
			domain->lang_id,
			domain->lang_id);
	}
#endif
}


void po_dump_languages(const char *domain_name, po_domain **languages, FILE *out)
{
	size_t i, j;
	
#define C(c) { c, #c }
	static struct {
		language_t code;
		const char *str;
	} const tos_country_strings[] = {
		C(COUNTRY_US),
		C(COUNTRY_DE),
		C(COUNTRY_FR),
		C(COUNTRY_UK),
		C(COUNTRY_ES),
		C(COUNTRY_IT),
		C(COUNTRY_SE),
		C(COUNTRY_SF),
		C(COUNTRY_SG),
		C(COUNTRY_TR),
		C(COUNTRY_FI),
		C(COUNTRY_NO),
		C(COUNTRY_DK),
		C(COUNTRY_SA),
		C(COUNTRY_NL),
		C(COUNTRY_CZ),
		C(COUNTRY_HU),
		C(COUNTRY_PL),
		C(COUNTRY_LT),
		C(COUNTRY_RU),
		C(COUNTRY_EE),
		C(COUNTRY_BY),
		C(COUNTRY_UA),
		C(COUNTRY_SK),
		C(COUNTRY_RO),
		C(COUNTRY_BG),
		C(COUNTRY_SI),
		C(COUNTRY_HR),
		C(COUNTRY_RS),
		C(COUNTRY_ME),
		C(COUNTRY_MK),
		C(COUNTRY_GR),
		C(COUNTRY_LV),
		C(COUNTRY_IL),
		C(COUNTRY_ZA),
		C(COUNTRY_PT),
		C(COUNTRY_BE),
		C(COUNTRY_JP),
		C(COUNTRY_CN),
		C(COUNTRY_KR),
		C(COUNTRY_VN),
		C(COUNTRY_IN),
		C(COUNTRY_IR),
		C(COUNTRY_MN),
		C(COUNTRY_NP),
		C(COUNTRY_LA),
		C(COUNTRY_KH),
		C(COUNTRY_ID),
		C(COUNTRY_BD),
		C(COUNTRY_CA),
		C(COUNTRY_MX),
	};
#undef C

	fprintf(out,
		"/*\n"
		" * the table of available languages.\n"
		" */\n"
		"static libnls_translation const nls_languages[] = {\n");
	for (i = 0; languages[i] != NULL; i++)
	{
		po_domain *domain = languages[i];
		const char *tos_country_str;
		char buf[20];

		/*
		 * dump the domain
		 */
		tos_country_str = NULL;
		for (j = 0; j < sizeof(tos_country_strings) / sizeof(tos_country_strings[0]); j++)
		{
			if (tos_country_strings[j].code == domain->tos_country_code)
			{
				tos_country_str = tos_country_strings[j].str;
				break;
			}
		}

		if (tos_country_str == NULL)
		{
			sprintf(buf, "%d", domain->tos_country_code);
			tos_country_str = buf;
		}
		fprintf(out, "\t{ %s, \"%s\", %s, msg_%s_translations, msg_%s },\n",
			tos_country_str,
			domain->lang_id,
			domain->plural_form,
			domain->lang_id,
			domain->lang_id);
	}
	fprintf(out, "\t{ 0, \"\", 0, NULL, NULL }\n};\n\n");
	fprintf(out, "libnls_domain %s_domain = { \"%s\", nls_key_strings, nls_languages, { 0, \"\", 0, 0, 0 } };\n", domain_name, domain_name);
}
