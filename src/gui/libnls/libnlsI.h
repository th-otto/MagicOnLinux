struct libnls_domain_list {
	struct libnls_domain_list *next;
	libnls_domain *domain;
};

struct _nls_plural {
	int id;
	const char *id_str;
	int nplurals;
	const char *exp;
	const char *str;
};

#define CONTEXT_GLUE '\004'

#define NLS_NOKEY_ERROR "nls: no key"

extern libnls_domain *_libnls_current_domain;
extern struct libnls_domain_list *_libnls_all_domains;
extern struct _nls_plural const libnls_plurals[];

const char *_libnls_internal_dgettext(const libnls_domain *domain, libnls_msgid_type key);
const char *_libnls_internal_dngettext(const libnls_domain *domain, libnls_msgid_type msgid1, libnls_msgid_type msgid2, unsigned long int n);
void _libnls_set_domain(void);
