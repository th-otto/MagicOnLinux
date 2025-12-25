#ifndef __POFILE_H__
#define __POFILE_H__ 1

#ifndef FALSE
# define FALSE 0
# define TRUE 1
typedef int _BOOL;
#endif

#define ISO639_CODE_LEN 5

typedef unsigned short nls_key_offset;

typedef struct _po_domain {
	int tos_country_code;
	char *lang_id;
	char *filename;
	_BOOL is_pot;

	char *keys;
	size_t keys_len;
	unsigned long num_keys;
	nls_key_offset *key_offsets;

	char *translations;
	size_t translations_len;
	unsigned long num_translations;
	/* indexed by msgid: */
	nls_key_offset *translation_offsets;
	/* indexed by running translation str number: */
	nls_key_offset *translation_lens;
	/* indexed by running translation str number: */
	int *msgnums;
	/* indexed by msgid: */
	unsigned char *have_translation;

	const char *plural_form;
	int nplurals;
} po_domain;

void errout(const char *format, ...) __attribute__((format(printf, 1, 2)));
void erroutv(const char *format, va_list args) __attribute__((format(printf, 1, 0)));

po_domain **po_init(const char *po_dir);
void po_free(po_domain *domain);
void po_exit(po_domain **languages);

_BOOL po_create_keys(po_domain *domain, const char *po_dir, _BOOL verbose);
_BOOL po_create_translations(po_domain *domain, const char *po_dir, _BOOL report_translations);
void po_delete_translations(po_domain *domain);
void po_dump_keys(po_domain *domain, FILE *out);
void po_dump_translation(po_domain *domain, FILE *out);
void po_dump_languages(const char *domain, po_domain **languages, FILE *out);
int po_translate(const char *po_dir, const char *default_domain, const char *lang, int count, char **filenames, int verbose);
_BOOL po_verify_keys(po_domain *ref, po_domain *trans);

#endif /* __POFILE_H__ */
