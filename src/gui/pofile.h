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
	char *keys;
	size_t keys_len;
	char *translations;
	size_t translations_len;
	nls_key_offset **hash;
	const char *plural_form;
	int nplurals;
} po_domain;

void errout(const char *format, ...) __attribute__((format(printf, 1, 2)));
void erroutv(const char *format, va_list args) __attribute__((format(printf, 1, 0)));

po_domain **po_init(const char *po_dir);
void po_exit(po_domain **languages);

_BOOL po_create_hash(po_domain *domain, const char *po_dir, _BOOL report_translations);
void po_delete_hash(po_domain *domain);
void po_dump_keys(po_domain *domain, FILE *out);
void po_dump_hash(po_domain *domain, FILE *out);
void po_dump_languages(const char *domain, po_domain **languages, FILE *out);

#endif /* __POFILE_H__ */
