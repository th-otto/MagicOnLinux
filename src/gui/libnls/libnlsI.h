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

extern libnls_domain *_libnls_current_domain;
extern struct libnls_domain_list *_libnls_all_domains;
extern struct _nls_plural const nls_plurals[];

const char *_libnls_internal_dgettext(const libnls_domain *domain, const char *key) __attribute__((__format_arg__(2)));
const char *_libnls_internal_dngettext(const libnls_domain *domain, const char *msgid1, const char *msgid2, unsigned long int n) __attribute__((__format_arg__(2))) __attribute__((__format_arg__(3)));
