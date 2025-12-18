#ifndef __POFILE_H__
#define __POFILE_H__ 1

#ifndef FALSE
# define FALSE 0
# define TRUE 1
typedef int _BOOL;
#endif

typedef struct _po_domain {
	language_t tos_country_code;
	const char *lang_id;
	char *keys;
	size_t keys_len;
	char *translations;
	size_t translations_len;
	nls_key_offset **hash;
	const char *plural_form;
	const char *key_string_name;
} po_domain;

void errout(const char *format, ...) __attribute__((format(printf, 1, 2)));
void erroutv(const char *format, va_list args) __attribute__((format(printf, 1, 0)));

char **po_init(const char *po_dir);
void po_exit(char **languages);

_BOOL po_create_hash(po_domain *domain, const char *po_dir, const char *lang, _BOOL report_translations);
void po_delete_hash(po_domain *domain);
void po_dump_keys(po_domain *domain, FILE *out);
void po_dump_hash(po_domain *domain, FILE *out);
void po_dump_languages(char **languages, FILE *out);

#endif /* __POFILE_H__ */
