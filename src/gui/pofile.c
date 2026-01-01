#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>
#include "libnls/libnls.h"
#undef textdomain
#undef bindtextdomain
#undef bind_textdomain_codeset
#include "nls-enable.h"
#include "libnls/country.h"
#include "pofile.h"

/*
 * memory
 */

#define g_malloc(n) xmalloc(n)
#define g_free(ptr) free(ptr)
#define g_calloc(n, s) xcalloc((size_t)(n), (size_t)(s))
#define g_malloc0(n) xcalloc((size_t)(n), 1)
#define g_realloc(ptr, s) xrealloc(ptr, s)

#define g_new(t, n) ((t *)g_malloc((size_t)(n) * sizeof(t)))
#define g_new0(t, n) ((t *)g_malloc0((size_t)(n) * sizeof(t)))
#define g_renew(t, p, n) ((t *)g_realloc(p, (size_t)(n) * sizeof(t)))
#define g_strdup(s) xstrdup(s)

#include "libnls/expreval.h"
#include "libnls/expreval-internal.h"
#include "libnls/libnlsI.h"

#define KINFO(x) errout x

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif


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

/* this is also used by gettext() */
#define CONTEXT_GLUE '\004'

static _BOOL pass_comments = TRUE;
static FILE *echo_nl_file;

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

static char *xstrdup(const char *s)
{
	size_t len;
	char *a;

	if (s == NULL)
		return NULL;
	len = strlen(s);
	a = g_new(typeof(*a), len + 1);
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
		d->buf = g_new(typeof(*d->buf), d->size);
	} else
	{
		d->size *= 4;
		d->buf = g_renew(typeof(*d->buf), d->buf, d->size);
	}
}

/* ------------------------------------------------------------------------- */

static da *da_new(void)
{
	da *d = g_new(typeof(*d), 1);

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

/* ------------------------------------------------------------------------- */

static str *s_new(void)
{
	str *s = g_new(typeof(*s), 1);

	s->size = 0;
	s->len = 0;
	s->buf = NULL;
	return s;
}

/* ------------------------------------------------------------------------- */

static size_t s_length(str *s)
{
	return s->len;
}

/* ------------------------------------------------------------------------- */

static char *s_text(str *s)
{
	return s->buf;
}

/* ------------------------------------------------------------------------- */

static void s_grow(str *s)
{
	if (s->size == 0)
	{
		s->size = STR_SIZE;
		s->buf = g_new(typeof(*s->buf), s->size);
	} else
	{
		s->size *= 4;
		s->buf = g_renew(typeof(*s->buf), s->buf, s->size);
	}
}

/* ------------------------------------------------------------------------- */

static void s_free(str *s)
{
	if (s)
	{
		g_free(s_text(s));
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

/* add a trailing 0 if needed and release excess mem */
static char *s_close(str *s)
{
	if (s->size == 0)
	{
		if (s->buf == NULL)
		{
			s->buf = g_new(typeof(*s->buf), 1);
			s->buf[0] = 0;
		}
		return s->buf;
	}
	s->buf = g_renew(typeof(*s->buf), s->buf, s->len + 1);
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
	const char *key;
	size_t keylen;
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

/* ------------------------------------------------------------------------- */

static hash *h_new(void)
{
	hash *h = g_new0(typeof(*h), 1);

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
	return m % HASH_SIZ;
}

/* ------------------------------------------------------------------------- */

static void *h_find(hash *h, const char *key, size_t keylen)
{
	unsigned int m = compute_hash(key, keylen);
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

static void h_insert(hash *h, hi *k)
{
	unsigned int m = compute_hash(k->key, k->keylen);
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
#define KIND_OLD  2

typedef struct poe
{
	hi msgid;							/* the key (super-type) */
	int kind;							/* kind of entry */
	char *comment;						/* free user comments */
	da *refs;							/* the references to locations in code */
	char *refstr;						/* a char * representation of the references */
	str *msgkey;						/* the lookup key */
	str *msgstr;						/* the translation */
	int nplurals;						/* number of plural strings */
	size_t msgctxt_len;					/* length of the context string, including CONTEXT_GLUE marker */
	int msgnum;
	int lineno;
	int keyoffset;
} poe;

/* ------------------------------------------------------------------------- */

static poe *poe_new(str *msgid, size_t msgctxt_len)
{
	poe *e = g_new0(typeof(*e), 1);

	e->msgid.key = s_text(msgid);
	e->msgid.keylen = s_length(msgid);
	e->msgkey = msgid;
	e->kind = KIND_NORM;
	e->comment = NULL;
	e->refs = NULL;
	e->msgstr = NULL;
	e->refstr = NULL;
	e->nplurals = 0;
	e->msgctxt_len = msgctxt_len;
	e->msgnum = 0;
	e->lineno = 0;
	e->keyoffset = 0;
	return e;
}

/* ------------------------------------------------------------------------- */

static void poe_free(poe *e)
{
	if (e)
	{
		s_free(e->msgkey);
		g_free(e->comment);
		g_free(e->refstr);
		s_free(e->msgstr);
		da_free(e->refs);
		g_free(e);
	}
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

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
	oh *o = g_new(typeof(*o), 1);

	o->h = h_new();
	o->d = da_new();
	return o;
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
	h_insert(o->h, &k->msgid);
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

#if 0
/*
 * ref - reference to locations in source files
 */

typedef struct ref {
	const char *fname;
	int lineno;
} ref;

static ref *ref_new(const char *fname, int lineno)
{
	ref *r = g_new(typeof(*r), 1);
	r->fname = fname;
	r->lineno = lineno;
	return r;
}
#endif

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

/* ------------------------------------------------------------------------- */

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
		warn(_("%s: empty administrative entry"), fname);
		ret = FALSE;
	} else
	{
#define AE_CHECK(s, f) \
		else if (strncmp(c, s, sizeof(s) - 1) == 0) \
		{ \
			char *val = c + sizeof(s) - 1; \
			while (*val == ' ') val++; \
			m = t - val; \
			a->f = g_new(typeof(*a->f), m + 1); \
			memcpy(a->f, val, m); \
			a->f[m] = '\0'; \
		}
		
		for (;;)
		{
			t = strchr(c, '\n');
			if (t == NULL)
			{
				error(_("%s: fields in administrative entry must end with \\n"), fname);
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
			error(_("%s: Expecting \"%s\" in administrative entry"), fname, "Last-Translator");
			ret = FALSE;
		}
		if (a->langteam == NULL)
		{
			error(_("%s: Expecting \"%s\" in administrative entry"), fname, "Language-Team");
			ret = FALSE;
		}
		if (a->language == NULL || *a->language == '\0')
		{
			error(_("%s: Expecting \"%s\" in administrative entry"), fname, "Language");
			ret = FALSE;
		}
		if (a->mimeversion == NULL || strcmp(a->mimeversion, "1.0") != 0)
		{
			error(_("%s: MIME version must be 1.0"), fname);
			ret = FALSE;
		}
		if (a->transfer_encoding == NULL || strcmp(a->transfer_encoding, "8bit") != 0)
		{
			error(_("%s: Content-Transfer-Encoding must be 8bit"), fname);
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
		ExprEvalNode *pluralp;

		if (libnls_extract_plural_expression(a->plural_form, &pluralp, &nplurals))
		{
			char buf[200];
			const struct _nls_plural *plural;

			libnls_expreval_print_expression(pluralp, buf, sizeof(buf), FALSE);
			domain->nplurals = nplurals;
			for (plural = libnls_plurals; plural->exp != NULL; plural++)
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
		libnls_free_expression(pluralp);

		if (domain->plural_form == NULL)
		{
			error(_("%s: unknown Plural-Forms entry"), fname);
			ret = FALSE;
		}
	} else
	{
		/*
		 * xgettext does not generate an entry for it in the messages.pot
		 */
		if (!domain->is_pot)
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

/* ------------------------------------------------------------------------- */

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
		f->index--;
		if (f->buf[f->index] == 012)
		{
			f->lineno--;
		}
	}
}

/* ------------------------------------------------------------------------- */

static void ibackn(IFILE *f, int n)
{
	while (--n >= 0)
	{
		iback(f);
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
	IFILE *f = g_new0(typeof(*f), 1);

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
				if (echo_nl_file)
				{
					putc('\\', echo_nl_file);
					putc('\n', echo_nl_file);
				}
				goto again;
			} else
			{
				ibackn(f, 2);
				return '\\';
			}
		} else if (ret == 012)
		{
			f->lineno++;
			if (echo_nl_file)
			{
				putc('\\', echo_nl_file);
				putc('\n', echo_nl_file);
			}
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

#define is_white(c)  (((c) == ' ') || ((c) == '\t') || ((c) == '\f'))
#define is_letter(c) ((((c) >= 'a') && ((c) <= 'z')) || (((c) >= 'A') && ((c) <= 'Z')))
#define is_digit(c)  (((c) >= '0') && ((c) <= '9'))
#define is_octal(c)  (((c) >= '0') && ((c) <= '7'))
#define is_hexdig(c) ((((c) >= 'a') && ((c) <= 'f')) || (((c) >= 'A') && ((c) <= 'F')))
#define is_hex(c)    (is_digit(c) || is_hexdig(c))

typedef struct parse_c_action {
	void (*gstring)(void *self, str *s, const char *fname, int lineno);
	void (*string)(void *self, str *s);
	void (*other)(void *self, int c);
} parse_c_action;

/*
 * functions swallowing lexical tokens. return TRUE if
 * the token was the one tested for, return FALSE otherwise.
 */

static _BOOL try_eof(IFILE *f)
{
	int c = inextc(f);
	if (c == EOF)
		return TRUE;

	iback(f);
	return FALSE;
}

/* ------------------------------------------------------------------------- */

static _BOOL try_c_comment(IFILE *f, const parse_c_action *pca, void *self)
{
	int c;

	c = inextc(f);
	if (c == '/')
	{
		c = inextc(f);
		if (c == '/')
		{
			if (pass_comments)
			{
				pca->other(self, '/');
				pca->other(self, '/');
			}
			do
			{
				c = inextc(f);
				if (pass_comments && c != EOF)
				{
					pca->other(self, c);
				}
			} while (c != EOF && c != '\n');
			return TRUE;
		}

		if (c == '*')
		{
			int state = 0;

			if (pass_comments)
			{
				pca->other(self, '/');
				pca->other(self, '*');
			}
			do
			{
				c = inextc(f);
				if (c == '*')
				{
					if (pass_comments)
						pca->other(self, c);
					state = 1;
				} else if (c == '/')
				{
					if (state == 1)
					{
						if (pass_comments)
							pca->other(self, c);
						return TRUE;
					}
					if (pass_comments)
						pca->other(self, c);
					state = 0;
				} else
				{
					if (pass_comments && c != EOF)
						pca->other(self, c);
					state = 0;
				}
			} while (c != EOF);
			if (c == EOF)
			{
				warn(_("EOF reached inside comment"));
				return TRUE;
			}
		}
	}
	iback(f);
	return FALSE;
}

/* ------------------------------------------------------------------------- */

static _BOOL try_white(IFILE *f)
{
	int c;

	c = inextc(f);
	if (is_white(c) || c == '\n')
	{
		do
		{
			c = inextc(f);
		} while (is_white(c) || c == '\n');
		if (c != EOF)
			iback(f);
		return TRUE;
	}

	iback(f);
	return FALSE;
}

/* ------------------------------------------------------------------------- */

static _BOOL try_c_white(IFILE *f, const parse_c_action *pca, void *self)
{
	if (try_eof(f))
		return FALSE;

	if (try_c_comment(f, pca, self) || try_white(f))
	{
		while (!try_eof(f) && (try_c_comment(f, pca, self) || try_white(f)))
			;
		return TRUE;
	}

	return FALSE;
}

/* ------------------------------------------------------------------------- */

/* only one "..." string will be appended to string s */
static _BOOL get_c_string(IFILE *f, str *s, _BOOL translate)
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
					if (translate)
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
 * parse c files
 * put strings surrounded by _("...") or N_("...") into the ordered-hash
 *
 * when anything meaningful has been parsed, the corresponding structure of
 * the action structure is called.
 */

#if 0
static void pca_xgettext_gstring(void *self, str *s, const char *fname, int lineno)
{
	oh *o = (oh *) self;
	poe *e;
	ref *r;
	
	/* add the string into the hash */
	e = o_find(o, s_text(s), s_length(s));
	if (e)
	{
		/* the string already exists */
		s_free(s);
	} else
	{
		s_close(s);
		e = poe_new(s, 0);
		e->msgnum = o_len(o);
		o_insert(o, e);
	}
	r = ref_new(fname, lineno);
	if (e->refs == 0)
		e->refs = da_new();
	da_add(e->refs, r);
}

/* ------------------------------------------------------------------------- */

static void pca_xgettext_string(void *self, str *s)
{
	UNUSED(self);
	s_free(s);
}

/* ------------------------------------------------------------------------- */

static void pca_xgettext_other(void *self, int c)
{
	UNUSED(self);
	UNUSED(c);
}

static parse_c_action const pca_xgettext = { pca_xgettext_gstring, pca_xgettext_string, pca_xgettext_other };
#endif

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

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
			fputs("\"\n\t                  \"", out);
		break;
	case '"':
		fputs("\\\"", out);
		break;
	case '\\':
		fputs("\\\\", out);
		break;
	case '\0':
		/* yes, this can happen: plurals are stored as several strings */
		fputs("\\0", out);
		if (cont)
			fputs("\"\n\t                  \"", out);
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

static char *escaped_string(const char *text, size_t len)
{
	size_t i;
	char buf[10];
	str *s;

	s = s_new();
	s_addch(s, '"');
	i = 0;
	while (i < len)
	{
		unsigned char c = text[i++];

		switch (c)
		{
		case '\a':
			s_addstr(s, "\\a");
			break;
		case '\b':
			s_addstr(s, "\\b");
			break;
		case '\v':
			s_addstr(s, "\\v");
			break;
		case '\e':
			s_addstr(s, "\\e");
			break;
		case '\f':
			s_addstr(s, "\\f");
			break;
		case '\r':
			s_addstr(s, "\\r");
			break;
		case '\t':
			s_addstr(s, "\\t");
			break;
		case '\n':
			s_addstr(s, "\\n");
			break;
		case '"':
			s_addstr(s, "\\\"");
			break;
		case '\\':
			s_addstr(s, "\\\\");
			break;
		case '\0':
			/* yes, this can happen: plurals are stored as several strings */
			s_addstr(s, "\\0");
			break;
		default:
			if (c < ' ' || c >= 127u)
			{
				sprintf(buf, "\\%03o", c);
				s_addstr(s, buf);
			} else
			{
				s_addch(s, c);
			}
			break;
		}
	}
	s_addch(s, '"');
	return s_detach(s);
}

/* ------------------------------------------------------------------------- */

static void print_escaped(FILE *out, const char *str, size_t len, _BOOL eos)
{
	size_t i;
	
	fputc('"', out);
	i = 0;
	while (i < len)
	{
		unsigned char c = str[i++];
		put_escaped(c, out, eos && i < len && str[i] != '\0');
	}
	fputc('"', out);
	if (eos)
		fputs(" \"\\0\"\n", out);
}

/* ------------------------------------------------------------------------- */

static void print_escaped_str(FILE *out, str *s, _BOOL eos)
{
	print_escaped(out, s_text(s), s_length(s), eos);
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

/* pcati - Parse C Action Translate Info */
typedef struct pcati {
	FILE *f;
	oh *o;
	int multilang;
	const char *msgid_prefix;
	const char *messages_file;
} pcati;

static void pca_translate_gstring(void *self, str *s, const char *fname, int lineno)
{
	pcati *p = (pcati *) self;
	poe *e;

	UNUSED(fname);
	UNUSED(lineno);

	if (p->multilang)
	{
		e = o_find(p->o, s_text(s), s_length(s));
		if (e)
		{
			/* use message index, even if there is no translation */
			assert(e->msgnum > 0);
			fprintf(p->f, "%s%d", p->msgid_prefix, e->msgnum);
		} else
		{
			char *t;

			/* something went wrong. messages.pot not up-to-date? */
			s_close(s);
			t = escaped_string(s_text(s), s_length(s));
			fatal(_("msgid '%s' not found in %s\n(run 'xgettext' to generate it)"), t, p->messages_file);
			g_free(t);
		}
	} else
	{
		str *trans;

		e = o_find(p->o, s_text(s), s_length(s));
		if (e && e->msgstr && s_length(e->msgstr) != 0) /* if the translation isn't empty, use it instead */
		{
			trans = e->msgstr;
		} else
		{
			trans = s;
		}
		print_escaped_str(p->f, trans, FALSE);
	}
	s_free(s);
}

/* ------------------------------------------------------------------------- */

static void pca_translate_string(void *self, str *s)
{
	pcati *p = (pcati *) self;

	s_close(s);
	print_escaped_str(p->f, s, FALSE);
	s_free(s);
}

/* ------------------------------------------------------------------------- */

static void pca_translate_other(void *self, int c)
{
	pcati *p = (pcati *) self;

	fputc(c, p->f);
}

static parse_c_action const pca_c_translate = { pca_translate_gstring, pca_translate_string, pca_translate_other };

/* ------------------------------------------------------------------------- */

/*
 * parse C code
 *
 * the state machine appears to have the following states
 * (info supplied by Eero Tamminen):
 *  state   meaning
 *    0     valid place to start token
 *    1     within token starting with 'N'
 *    2     within '_' or  'N_' token
 *    3     valid '_(' or  'N_(', can now parse string
 *    4     parsing some other identifier
 *    5     inside single quote
 */
static _BOOL parse_c_file(const char *fname, const parse_c_action *pca, void *self)
{
	int c;
	int state;
	int gettext_type;
	str *s;
	int lineno;
	IFILE *f;

	f = ifopen(fname);
	if (f == NULL)
	{
		error(_("could not open %s: %s"), fname, strerror(errno));
		return FALSE;
	}

	state = 0;
	gettext_type = 0;
	for (;;)
	{
		c = inextc(f);
		if (c == EOF)
			break;

		if (c == '/')
		{
			c = inextc(f);
			if (c == '/')
			{
				if (!pass_comments)
					pca->other(self, '\n');
			}
			ibackn(f, 2);
			c = '/';
			state = 0;
			gettext_type = 0;
			if (!try_c_comment(f, pca, self))
			{
				pca->other(self, c);
			} else
			{
				if (!pass_comments)
					pca->other(self, ' ');
			}
		} else if (c == '\"')
		{
			if (state == 3)
			{
				/* this is a new gettext string */
				s = s_new();
				lineno = f->lineno;
				/* accumulate all consecutive strings (separated by spaces) */
				do
				{
					iback(f);
					get_c_string(f, s, TRUE);
					try_c_white(f, pca, self);
					c = inextc(f);
				} while (c == '\"');
				if (c != ')')
				{
					char *t = s_detach(s);
					warn(_("_(\"...\" with no closing )\nthe string is %s"), t);
					g_free(t);
					state = 0;
					gettext_type = 0;
					continue;
				}
				/* handle the string */
				pca->gstring(self, s, fname, lineno);
				pca->other(self, ')');
				state = 0;
				gettext_type = 0;
			} else if (state == 5)
			{
				pca->other(self, c);
			} else
			{
				iback(f);
				s = s_new();
				get_c_string(f, s, FALSE);
				pca->string(self, s);
			}
		} else
		{
			if (c == '(')
			{
				if (state == 2)
				{
					state = 3;
				} else
				{
					state = 0;
					gettext_type = 0;
				}
			} else if (c == '_')
			{
				if (state < 2)
				{
					if (gettext_type == 0)
						gettext_type = '_';
					state = 2;
				} else
				{
					state = 4;
				}
			} else if (c == 'N')
			{
				if (state == 0)
				{
					state = 1;
					gettext_type = 'N';
				} else
				{
					state = 4;
				}
			} else if (is_white(c))
			{
				if (state == 1 || state == 4)
				{
					state = 0;
					gettext_type = 0;
				}
			} else if (is_letter(c) || is_digit(c))
			{
				state = 4;
			} else if (c == '\'')
			{
				if (state == 5)
					state = 0;
				else
					state = 5;
			} else
			{
				if (state < 5)
				{
					state = 0;
					gettext_type = 0;
				}
			}
			pca->other(self, c);
		}
	}
	ifclose(f);
	return TRUE;
}

/* ------------------------------------------------------------------------- */

static void pca_translate_xml_gstring(void *self, str *s, const char *fname, int lineno)
{
	pcati *p = (pcati *) self;
	poe *e;

	UNUSED(fname);
	UNUSED(lineno);

	if (p->multilang)
	{
		char *t;

		s_close(s);
		t = escaped_string(s_text(s), s_length(s));
		e = o_find(p->o, s_text(s), s_length(s));
		if (e)
		{
			/* use message index, even if there is no translation */
			assert(e->msgnum > 0);
			fprintf(p->f, "\t{ %s, %d },\n", t, e->msgnum);
		} else
		{
			/* something went wrong. messages.pot not up-to-date? */
			fatal(_("msgid '%s' not found in %s\n(run 'xgettext' to generate it)"), t, p->messages_file);
		}
		g_free(t);
	} else
	{
		str *trans;

		e = o_find(p->o, s_text(s), s_length(s));
		if (e && e->msgstr && s_length(e->msgstr) != 0) /* if the translation isn't empty, use it instead */
		{
			trans = e->msgstr;
		} else
		{
			trans = s;
		}
		print_escaped_str(p->f, trans, FALSE);
	}
	s_free(s);
}

/* ------------------------------------------------------------------------- */

static void pca_translate_xml_string(void *self, str *s)
{
	pcati *p = (pcati *) self;

	s_close(s);
	if (!p->multilang)
		print_escaped_str(p->f, s, FALSE);
	s_free(s);
}

/* ------------------------------------------------------------------------- */

static void pca_xml_other(void *self, int c)
{
	pcati *p = (pcati *) self;

	if (!p->multilang)
		fputc(c, p->f);
}

static parse_c_action const pca_xml_translate = { pca_translate_xml_gstring, pca_translate_xml_string, pca_xml_other };

/* ------------------------------------------------------------------------- */

static _BOOL parse_xml_file(const char *fname, const parse_c_action *pca, void *self)
{
	int c;
	int state;
	int gettext_type;
	str *s;
	int lineno;
	IFILE *f;

	f = ifopen(fname);
	if (f == NULL)
	{
		error(_("could not open %s: %s"), fname, strerror(errno));
		return FALSE;
	}

	state = 0;
	gettext_type = 0;
	for (;;)
	{
		c = inextc(f);
		if (c == EOF)
			break;

		if (c == '<')
		{
			c = inextc(f);
			if (c == '!')
			{
				/* not quite right, but should do for the moment */
				if (pass_comments)
				{
					pca->other(self, '<');
					pca->other(self, c);
				}
				for (;;)
				{
					c = inextc(f);
					if (c == EOF)
						break;
					if (pass_comments)
						pca->other(self, c);
					if (c == '-')
					{
						while (c == '-')
						{
							c = inextc(f);
							if (c == EOF)
								break;
							if (pass_comments)
								pca->other(self, c);
							if (c == '>')
								break;
						}
						if (c == '>')
							break;
					}
				}
			} else
			{
				pca->other(self, '<');
				pca->other(self, c);
			}
		} else if (c == '\"')
		{
			if (state == 3)
			{
				/* this is a new gettext string */
				s = s_new();
				lineno = f->lineno;
				/* accumulate all consecutive strings (separated by spaces) */
				iback(f);
				get_c_string(f, s, TRUE);
				/* handle the string */
				pca->gstring(self, s, fname, lineno);
				state = 0;
				gettext_type = 0;
			} else if (state == 5)
			{
				pca->other(self, c);
			} else
			{
				iback(f);
				s = s_new();
				get_c_string(f, s, FALSE);
				pca->string(self, s);
			}
		} else
		{
			if (c == '=')
			{
				if (state == 2)
				{
					state = 3;
				} else
				{
					state = 0;
					gettext_type = 0;
				}
			} else if (c == '_')
			{
				if (state < 2)
				{
					if (gettext_type == 0)
						gettext_type = '_';
					state = 2;
				} else
				{
					state = 4;
				}
			} else if (is_white(c))
			{
				state = 0;
				gettext_type = 0;
			} else if (is_letter(c) || is_digit(c))
			{
				if (state != 2)
					state = 4;
			} else if (c == '\'')
			{
				if (state == 5)
					state = 0;
				else
					state = 5;
			} else
			{
				if (state < 5)
				{
					state = 0;
					gettext_type = 0;
				}
			}
			pca->other(self, c);
		}
	}
	ifclose(f);
	return TRUE;
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
		info = g_new(typeof(*info), 1);
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
		{
			int i;

			for (i = 0; i < MAX_PLURALS; i++)
				msgstr[i] = NULL;
		}
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
			e = poe_new(s, 0);
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
				get_c_string(f, s, TRUE);
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
			if (strcmp(s_text(entrytype), "msgid") == 0)
			{
				msgid = s;
			} else if (strcmp(s_text(entrytype), "msgid[0]") == 0)
			{
				msgid = s;
			} else if (strcmp(s_text(entrytype), "msgid[1]") == 0)
			{
				msgid_plural = s;
			} else if (strcmp(s_text(entrytype), "msgid_plural") == 0)
			{
				msgid_plural = s;
			} else if (strcmp(s_text(entrytype), "msgstr") == 0)
			{
				msgstr[0] = s;
			} else if (strcmp(s_text(entrytype), "msgstr[0]") == 0)
			{
				msgstr[0] = s;
			} else if (strcmp(s_text(entrytype), "msgstr[1]") == 0)
			{
				msgstr[1] = s;
			} else if (strcmp(s_text(entrytype), "msgstr[2]") == 0)
			{
				msgstr[2] = s;
#if MAX_PLURALS >= 4
			} else if (strcmp(s_text(entrytype), "msgstr[3]") == 0)
			{
				msgstr[3] = s;
#endif
#if MAX_PLURALS >= 5
			} else if (strcmp(s_text(entrytype), "msgstr[4]") == 0)
			{
				msgstr[4] = s;
#endif
#if MAX_PLURALS >= 6
			} else if (strcmp(s_text(entrytype), "msgstr[5]") == 0)
			{
				msgstr[5] = s;
#endif
			} else if (strcmp(s_text(entrytype), "msgctxt") == 0)
			{
				msgctxt = s;
			} else
			{
				error(_("%s:%d: unsupported entry %s"), fname, f->lineno, s_text(entrytype));
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
		{
			int i;

			for (i = 0; i < MAX_PLURALS; i++)
				if (msgstr[i] != NULL)
					nplurals++;
		}
		if (msgid == NULL || msgstr[0] == NULL)
		{
			error(_("%s:%d: missing msgid"), fname, f->lineno);
			s_free(msgid);
			s_free(msgstr[0]);
			s_free(msgctxt);
			retval = FALSE;
		} else if ((msgid_plural != NULL && nplurals != domain->nplurals) ||
			(msgid_plural == NULL && nplurals > 1))
		{
			error(_("%s:%d: wrong number of plural forms"), fname, f->lineno);
			s_free(msgid);
			s_free(msgstr[0]);
			s_free(msgctxt);
			retval = FALSE;
		} else if (s_length(msgid) == 0 && (msgid_plural != NULL || msgctxt != NULL))
		{
			error(_("%s:%d: administrative entry must not have plural or context"), fname, f->lineno);
			s_free(msgid);
			s_free(msgstr[0]);
			s_free(msgctxt);
			retval = FALSE;
		} else
		{
			size_t msgctxt_len = 0;

			/* now we have the complete entry */
			if (msgid_plural != NULL)
			{
				/* use "key1\0key2" as real key */
				s_addch(msgid, '\0');
				s_close(msgid_plural);
				s_addstr(msgid, s_text(msgid_plural));
			} else
			{
				nplurals = 0;
			}
			s_close(msgid);
			if (msgctxt != NULL)
			{
				s_addch(msgctxt, CONTEXT_GLUE);
				msgctxt_len = s_length(msgctxt);
				s_addstrn(msgctxt, s_text(msgid), s_length(msgid));
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
					s_addstr(msgstr[0], s_text(msgstr[1]));
					s_free(msgstr[1]);
					msgstr[1] = NULL;
					s_addch(msgstr[0], '\0');
					s_close(msgstr[2]);
					s_addstr(msgstr[0], s_text(msgstr[2]));
					s_free(msgstr[2]);
					msgstr[2] = NULL;
				} else if (msgstr[1] != NULL)
				{
					s_addch(msgstr[0], '\0');
					s_close(msgstr[1]);
					s_addstr(msgstr[0], s_text(msgstr[1]));
					s_free(msgstr[1]);
					msgstr[1] = NULL;
				}
			}
			e = o_find(o, s_text(msgid), s_length(msgid));
			if (e)
			{
				char *t = escaped_string(s_text(msgid), s_length(msgid));
				warn(_("%s:%d: duplicate entry %s"), f->fname, f->lineno, t);
				g_free(t);
				s_free(msgid);
				s_free(msgstr[0]);
				retval = FALSE;
			} else
			{
				e = poe_new(msgid, msgctxt_len);
				if (msgstr[0] != NULL)
					s_close(msgstr[0]);
				e->msgstr = msgstr[0];
				e->nplurals = nplurals;
				if (e->msgid.key && *e->msgid.key != '\0' && e->msgstr != NULL && s_length(e->msgstr) != 0)
				{
					/* FIXME: have to check each plural string, not only the last one */
					size_t lp = s_length(e->msgkey);
					size_t ln = s_length(e->msgstr);
					if ((e->msgid.key[lp - 1] == '\n' && s_text(e->msgstr)[ln - 1] != '\n') ||
						(e->msgid.key[lp - 1] != '\n' && s_text(e->msgstr)[ln - 1] == '\n'))
					{
						char *t1 = escaped_string(s_text(e->msgkey), s_length(e->msgkey));
						char *t2 = escaped_string(s_text(e->msgstr), s_length(e->msgstr));
						warn(_("%s:%d: entries do not both end with '\\n' in translation of '%s' to '%s'"),
							f->fname, f->lineno, t1, t2);
						g_free(t2);
						g_free(t1);
					}
					if ((e->msgid.key[0] == '\n' && s_text(e->msgstr)[0] != '\n') ||
						(e->msgid.key[0] != '\n' && s_text(e->msgstr)[0] == '\n'))
					{
						char *t1 = escaped_string(s_text(e->msgkey), s_length(e->msgkey));
						char *t2 = escaped_string(s_text(e->msgstr), s_length(e->msgstr));
						warn(_("%s:%d: entries do not both begin with '\\n' in translation of '%s' to '%s'"),
							f->fname, f->lineno, t1, t2);
						g_free(t2);
						g_free(t1);
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
					if (parse_ae(domain, fname, s_text(e->msgstr), &a) == FALSE)
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
		{
			int i;

			for (i = 1; i < MAX_PLURALS; i++)
				s_free(msgstr[i]);
		}
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
		languages = g_new(typeof(*languages), n + 1);
		for (i = 0; i < n; i++)
		{
			struct lang_info *info = (struct lang_info *)da_nth(d, i);
			languages[i] = g_new0(typeof(**languages), 1);
			languages[i]->lang_id = info->name;
			g_free(info);
		}
		languages[n] = NULL;
	}
	da_free(d);
	return languages;
}

/* ------------------------------------------------------------------------- */

void po_free(po_domain *domain)
{
	if (domain != NULL)
	{
		po_delete_translations(domain);
		g_free(domain->lang_id);
		g_free(domain->filename);
		g_free(domain);
	}
}

/* ------------------------------------------------------------------------- */

void po_exit(po_domain **languages)
{
	size_t i;
	
	if (languages != NULL)
	{
		for (i = 0; languages[i] != NULL; i++)
		{
			po_free(languages[i]);
		}
		g_free(languages);
	}
}

/* ------------------------------------------------------------------------- */

/*
 * load po file
 */
static oh *po_load(po_domain *domain, const char *po_dir, _BOOL report_translations, const char *messages_filename)
{
	oh *o;
	poe *e;
	size_t i, n;
	unsigned long numtransl = 0;				/* number of translated entries */
	unsigned long numuntransl = 0;				/* number of untranslated entries */
	_BOOL retval = FALSE;
	str *s;

	if (po_dir == NULL)
		po_dir = PO_DIR;
	
	domain->tos_country_code = language_from_name(domain->lang_id);
	if (domain->tos_country_code == LANG_SYSTEM)
	{
		warn(_("unknown language %s"), domain->lang_id);
	}

	s = s_new();
	s_addstr(s, po_dir);
	i = s_length(s);
	if (i > 0 && po_dir[i - 1] != '/')
		s_addstr(s, "/");
	if (messages_filename)
	{
		s_addstr(s, messages_filename);
		s_addstr(s, ".pot");
		report_translations = FALSE;
		domain->is_pot = TRUE;
	} else
	{
		s_addstr(s, domain->lang_id);
		s_addstr(s, ".po");
		domain->is_pot = FALSE;
	}
	domain->filename = s_detach(s);
	
	o = o_new();
	if (parse_po_file(domain, domain->filename, o))
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
			} else if (e->kind == KIND_NORM)
			{
				if (e->msgstr && s_length(e->msgstr) != 0)
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
	
	return o;
}

/* ------------------------------------------------------------------------- */

static _BOOL key_equal(const char *key, size_t keylen, str *msgstr)
{
	if (keylen != s_length(msgstr))
		return FALSE;
	return memcmp(key, s_text(msgstr), keylen) == 0;
}

/* ------------------------------------------------------------------------- */

static _BOOL po_build_keys(po_domain *domain, oh *o, _BOOL verbose)
{
	str *keys;
	size_t i, n;
	_BOOL ret = TRUE;

	keys = s_new();
	/*
	 * must reserve first byte of keys
	 * since an offset of 0 is used for an empty string
	 */
	s_addch(keys, '\0');
	
	n = o_len(o);
	domain->key_offsets = g_new0(typeof(*domain->key_offsets), n);
	domain->num_keys = 0;
	for (i = 0; i < n; i++)
	{
		poe *e = o_nth(o, i);
		
		if (e->kind == KIND_NORM && e->msgid.key[0] != '\0')
		{
			const char *key;
			size_t keylen;

			/* store offset to key */
			e->keyoffset = s_length(keys);
			domain->key_offsets[domain->num_keys] = s_length(keys);
			++domain->num_keys;
			e->msgnum = domain->num_keys;
			/*
			 * the msgctxt is used here for hashing, but not written out
			 */
			key = e->msgid.key + e->msgctxt_len;
			keylen = s_length(e->msgkey) - e->msgctxt_len;
			s_addstrn(keys, key, keylen);
			s_addch(keys, '\0');
		}
	}
	if (verbose)
		printf("%s: %lu bytes\n", domain->lang_id, (unsigned long)s_length(keys));

	/*
	 * Note: when cross-compiling, this tool must still be compiled by the host.
	 * But in that case, we don't know sizeof(nls_key_offset) of the target machine.
	 */
	if (sizeof(nls_key_offset) < sizeof(size_t))
	{
		if (s_length(keys) >= (size_t)1 << (8 * sizeof(nls_key_offset)))
		{
			error(_("%s: length of keys %lu does not fit into nls_key_offset"), domain->lang_id, (unsigned long)s_length(keys));
			ret = FALSE;
		}
	}

	domain->keys_len = s_length(keys);
	domain->keys = s_detach(keys);
	return ret;
}

/* ------------------------------------------------------------------------- */

_BOOL po_create_keys(po_domain *domain, const char *po_dir, _BOOL verbose)
{
	oh *o;
	_BOOL ret;

	o = po_load(domain, po_dir, FALSE, NULL);
	if (o == NULL)
		return FALSE;
	ret = po_build_keys(domain, o, verbose);
	o_free(o, TRUE);
	return ret;
}

/* ------------------------------------------------------------------------- */

_BOOL po_create_translations(po_domain *domain, const char *po_dir, _BOOL report_translations)
{
	oh *o;
	size_t i, n;
	str *translations;
	_BOOL ret = TRUE;
	unsigned long num_keys;
	size_t key_offset;

	o = po_load(domain, po_dir, report_translations, NULL);
	if (o == NULL)
		return FALSE;
	if (po_build_keys(domain, o, report_translations) == FALSE)
	{
		ret = FALSE;
	} else
	{
		translations = s_new();
		/*
		 * must reserve first byte of translations,
		 * since an offset of 0 means no translation available
		 */
		s_addch(translations, '\0');
		
		n = o_len(o);
		/* clear target offsets */
		domain->translation_offsets = g_new0(typeof(*domain->translation_offsets), n);
		domain->msgnums = g_new0(typeof(*domain->msgnums), n);
		domain->have_translation = g_new0(typeof(*domain->have_translation), n);
		domain->translation_lens = g_new0(typeof(*domain->translation_lens), n);
		domain->num_translations = 0;
	
		/* create the offsets table */
		num_keys = 0;
		key_offset = 1;
		for (i = 0; i < n; i++)
		{
			poe *e = o_nth(o, i);
			
			if (e->kind == KIND_NORM && e->msgid.key[0] != '\0')
			{
				const char *key;
				size_t keylen;
	
				/*
				 * the msgctxt is used here for hashing, but not written out
				 */
				key = e->msgid.key + e->msgctxt_len;
				keylen = s_length(e->msgkey) - e->msgctxt_len;
				assert(domain->key_offsets[num_keys] == key_offset);
				key_offset += keylen + 1;
				++num_keys;
				e->msgnum = num_keys;
				if (e->msgstr && s_length(e->msgstr) != 0)
				{
					domain->have_translation[e->msgnum - 1] = TRUE;
					if (!key_equal(key, keylen, e->msgstr))
					{
						/*
						 * TODO: hash also translated strings, and eliminate duplicates
						 */
						domain->msgnums[domain->num_translations] = e->msgnum;
						domain->translation_offsets[e->msgnum - 1] = s_length(translations);
						domain->translation_lens[domain->num_translations] = s_length(e->msgstr) + 1;
						domain->num_translations++;
						s_addstrn(translations, s_text(e->msgstr), s_length(e->msgstr));
						s_addch(translations, '\0');
					} else
					{
						/* no translation needed */
					}
				} else
				{
					/* translation missing */
				}
			}
		}
		assert(num_keys == domain->num_keys);
	
		if (report_translations)
		{
			KINFO((_("%s.po: keys %lu (%lu bytes), translations %lu bytes\n"), domain->lang_id, num_keys, (unsigned long)domain->keys_len, (unsigned long)s_length(translations)));
		}
		/*
		 * Note: when cross-compiling, this tool must still be compiled by host.
		 * But in that case, we don't known sizeof(nls_key_offset) of the target machine
		 */
		if (sizeof(nls_key_offset) < sizeof(size_t))
		{
			if (s_length(translations) >= (size_t)1 << (8 * sizeof(nls_key_offset)))
			{
				error(_("%s: length of translations %lu does not fit into nls_key_offset"), domain->lang_id, (unsigned long)s_length(translations));
				ret = FALSE;
			}
		}
		domain->translations_len = s_length(translations);
		domain->translations = s_detach(translations);
	}
	
	o_free(o, TRUE);
	
	return ret;
}

/* ------------------------------------------------------------------------- */

void po_delete_translations(po_domain *domain)
{
	g_free(domain->key_offsets);
	domain->key_offsets = NULL;
	g_free(domain->translation_offsets);
	domain->translation_offsets = NULL;
	g_free(domain->translations);
	domain->translations = NULL;
	g_free(domain->keys);
	domain->keys = NULL;
	g_free(domain->msgnums);
	domain->msgnums = NULL;
	g_free(domain->have_translation);
	domain->have_translation = NULL;
	g_free(domain->translation_lens);
	domain->translation_lens = NULL;
}

/* ------------------------------------------------------------------------- */

_BOOL po_verify_keys(po_domain *ref, po_domain *trans)
{
	(void)ref;
	(void)trans;
	return TRUE;
}

/* ------------------------------------------------------------------------- */

void po_dump_keys(po_domain *domain, FILE *out)
{
	const char *s;
	size_t i, len;
	size_t k;

	fprintf(out,
		"/*\n"
		" * generated by nlstool -- DO NOT EDIT\n"
		" */\n"
		"\n"
		"#include <stddef.h>\n"
		"#include \"libnls/libnls.h\"\n"
		"#include \"libnls/country.h\"\n"
		"\n");
	if (domain == NULL || domain->keys == NULL)
		return;

	/*
	 * dump the actual key strings
	 */
	fprintf(out,
		"/*\n"
		" * The keys for offset tables below.\n"
		" */\n"
		"static char const nls_key_strings[] =\n"
		"{\n");
	s = domain->keys;
	len = domain->keys_len;
	for (i = 0, k = 0; i < len; )
	{
		size_t keylen;
		
		fprintf(out, "\t/* %5u %5u */ ", (unsigned int)k, (unsigned int)i);
		if (k == 0)
			keylen = domain->key_offsets[0];
		else if (k == domain->num_keys)
			keylen = len - domain->key_offsets[k - 1];
		else
			keylen = domain->key_offsets[k] - domain->key_offsets[k - 1];
		assert(keylen > 0);
		print_escaped(out, s + i, keylen - 1, TRUE);
		i += keylen;
		k++;
	}
	assert(k == domain->num_keys + 1);
	fprintf(out, "\t/* %5u %5u */\n", (unsigned int)k, (unsigned int)len);
	fprintf(out, "};\n\n");

	/* dump the offsets table for "en" */
	fprintf(out, "/*\n");
	fprintf(out, " * offset table for lang %s.\n", "en");
	fprintf(out, " */\n");
	fprintf(out, "const nls_key_offset msg_%s_offsets[] = {\n", "en");
	for (k = 0; k < domain->num_keys; k++)
	{
		fprintf(out, "\t/* %5u */ %u,\n", (unsigned int)k + 1, domain->key_offsets[k]);
	}
	fprintf(out, "};\n\n");
}

/* ------------------------------------------------------------------------- */

void po_dump_translation(po_domain *domain, FILE *out)
{
	const char *s;
	size_t i, len;
	size_t k;
	unsigned long num_translated, num_untranslated;

	if (domain == NULL || domain->translations == NULL)
		return;

	num_translated = 0;
	num_untranslated = 0;
	for (i = 0; i < domain->num_keys; i++)
	{
		if (domain->have_translation[i])
			num_translated++;
		else
			num_untranslated++;
	}
	
	/*
	 * dump the actual translation strings
	 */
	fprintf(out,
		"/*\n"
		" * translations for lang %s\n"
		" * %s: translated: %lu, untranslated: %lu\n"
		" */\n"
		"static char const msg_%s_translations[] = {\n",
		domain->lang_id,
		domain->lang_id,
		num_translated, num_untranslated,
		domain->lang_id);
	s = domain->translations;
	len = domain->translations_len;
	k = 0;
	for (i = 0; i < len; )
	{
		size_t msglen;

		msglen = k == 0 ? 1 : domain->translation_lens[k - 1];
		fprintf(out, "\t/* %5u %5u */ ", k == 0 ? 0 : (unsigned int)domain->msgnums[k - 1], (unsigned int)i);
		assert(msglen > 0);
		print_escaped(out, s + i, msglen - 1, TRUE);
		i += msglen;
		k++;
	}
	fprintf(out, "\t/*       %5u */\n", (unsigned int)len);
	fprintf(out, "};\n\n");
	assert(k == domain->num_translations + 1);

	/* dump the offsets table */
	fprintf(out, "/*\n");
	fprintf(out, " * offset table for lang %s.\n", domain->lang_id);
	fprintf(out, " */\n");
	fprintf(out, "static const nls_key_offset msg_%s_offsets[] = {\n", domain->lang_id);
	for (i = 0; i < domain->num_keys; i++)
	{
		fprintf(out, "\t/* %5u */ %u,%s\n", (unsigned int)i + 1, domain->translation_offsets[i],
			domain->translation_offsets[i] == 0 ? (domain->have_translation[i] ? " /* original string */" : " /* missing translation */") : "");
	}
	fprintf(out, "};\n\n");
}

/* ------------------------------------------------------------------------- */

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
	fprintf(out, "\t{ COUNTRY_US, \"en\", PLURAL_NOT_ONE, nls_key_strings, msg_en_offsets },\n");
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
		fprintf(out, "\t{ %s, \"%s\", %s, msg_%s_translations, msg_%s_offsets },\n",
			tos_country_str,
			domain->lang_id,
			domain->plural_form,
			domain->lang_id,
			domain->lang_id);
	}
	fprintf(out, "\t{ 0, \"\", 0, NULL, NULL }\n};\n\n");
	fprintf(out, "libnls_domain %s_domain = { \"%s\", nls_key_strings, %lu, nls_languages, { 0, \"\", 0, 0, 0 } };\n", domain_name, domain_name, languages[0]->num_keys);
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*****************************************************************************/

int po_translate(const char *po_dir, const char *default_domain, const char *lang, int count, char **filenames, int verbose)
{
	FILE *g;
	pcati p;
	char *to;
	int filenum;
	int ret = TRUE;
	po_domain *domain;

	p.o = NULL;
	p.msgid_prefix = "";

	if (strcmp(lang, "all") == 0)
	{
		poe *eref;
		int i, n, num_keys;

		p.multilang = TRUE;

		/* read original messages */
		domain = g_new0(typeof(*domain), 1);
		domain->lang_id = g_strdup("en_US");
		p.multilang = TRUE;
		p.o = po_load(domain, po_dir, verbose, default_domain);
		p.messages_file = domain->filename;
		if (p.o == NULL)
		{
			ret = FALSE;
		} else
		{
			n = o_len(p.o);
			num_keys = 0;
			for (i = 0; i < n; i++)
			{
				eref = o_nth(p.o, i);
				if (eref->kind == KIND_NORM && *eref->msgid.key != '\0')
				{
					num_keys++;
					eref->msgnum = num_keys;
				}
			}
		}
	} else
	{
		p.multilang = FALSE;
		/* read all translations */
		domain = g_new0(typeof(*domain), 1);
		domain->lang_id = g_strdup(lang);
		p.o = po_load(domain, po_dir, verbose, NULL);
		if (p.o == NULL)
		{
			ret = FALSE;
		}
	}

	for (filenum = 0; ret && filenum < count; filenum++)
	{
		const char *from = filenames[filenum];
		gboolean xml = FALSE;

		/* build destination filename */
		size_t len = strlen(from);
		if (len >= 2 && from[len - 2] == '.' && from[len - 1] == 'c')
		{
			to = g_new(typeof(*to), len + 3);
			strcpy(to, from);
			strcpy(to + len - 2, ".tr.c");
		} else if (len >= 3 && from[len - 3] == '.' && from[len - 2] == 'c' && from[len - 1] == 'c')
		{
			to = g_new(typeof(*to), len + 3);
			strcpy(to, from);
			strcpy(to + len - 3, ".tr.cc");
		} else if (len >= 4 && from[len - 4] == '.' && from[len - 3] == 'x' && from[len - 2] == 'm' && from[len - 1] == 'l')
		{
			to = g_new(typeof(*to), len + 6);
			strcpy(to, from);
			strcpy(to + len, ".tr.c");
			xml = TRUE;
		} else
		{
			warn(_("I only translate .c files"));
			ret = FALSE;
		}
		if (ret)
		{
			g = fopen(to, "w");
			if (g == NULL)
			{
				error(_("cannot create %s: %s"), to, strerror(errno));
				ret = FALSE;
			} else
			{
				p.f = g;

				setvbuf(g, NULL, _IONBF, 0);
				echo_nl_file = g;
				if (xml)
					ret = parse_xml_file(from, &pca_xml_translate, &p);
				else
					ret = parse_c_file(from, &pca_c_translate, &p);
				echo_nl_file = NULL;

				fclose(g);
			}
			g_free(to);
		}
	}
	o_free(p.o, TRUE);
	po_free(domain);
	return ret;
}
