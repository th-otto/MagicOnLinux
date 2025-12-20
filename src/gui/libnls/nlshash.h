#define TH_BITS 10
#define TH_SIZE (1 << TH_BITS)
#define TH_MASK (TH_SIZE - 1)
#define TH_BMASK ((1 << (16 - TH_BITS)) - 1)

/*
 * Hash function for message ids.
 * Used by the tools that create such a hash,
 * and at runtime to look them up.
 */
static __inline unsigned int nls_hash(const char *t)
{
	const unsigned char *u = (const unsigned char *) t;
	unsigned short a, b;

	a = 0;
	while (*u)
	{
		a = (a << 1) | ((a >> 15) & 1);
		a += *u++;
	}
	b = (a >> TH_BITS) & TH_BMASK;
	a ^= b;
	a &= TH_MASK;
	return a;
}

/*
 * Same as above, but hash the two keys for plural lookups
 */
static __inline unsigned int nls_hash2(const char *key1, const char *key2)
{
	const unsigned char *u = (const unsigned char *) key1;
	unsigned short a, b;

	a = 0;
	while (*u)
	{
		a = (a << 1) | ((a >> 15) & 1);
		a += *u++;
	}
	/* hash a EOS */
	a = (a << 1) | ((a >> 15) & 1);
	/* hash 2nd key */
	u = (const unsigned char *) key2;
	while (*u)
	{
		a = (a << 1) | ((a >> 15) & 1);
		a += *u++;
	}
	b = (a >> TH_BITS) & TH_BMASK;
	a ^= b;
	a &= TH_MASK;
	return a;
}
