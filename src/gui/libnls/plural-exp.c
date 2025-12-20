/* Expression parsing for plural form selection.
   Copyright (C) 2000-2013 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@cygnus.com>, 2000.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "eval-plural.h"

/* These structs are the constant expression for the germanic plural
   form determination.  It represents the expression  "n != 1".  */
static struct expression plvar = {
	0,
	var,
	{ { NULL, NULL, NULL }, 0 }
};

static struct expression plone = {
	0,
	num,
	{ { NULL, NULL, NULL }, 1 }
};

struct expression GERMANIC_PLURAL = {
	2,									/* nargs */
	not_equal,							/* operation */
	{ { &plvar, &plone, NULL }, 0 }
};

#define ISSPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\n')

int EXTRACT_PLURAL_EXPRESSION(const char *nullentry, struct expression **pluralp, int *npluralsp)
{
	/* By default we are using the Germanic form: singular form only
	   for `one', the plural form otherwise.  Yes, this is also what
	   English is using since English is a Germanic language.  */
	*pluralp = &GERMANIC_PLURAL;
	*npluralsp = 2;

	if (nullentry != NULL)
	{
		const char *plural;
		const char *nplurals;

		if (strncmp(nullentry, "nplurals=", 9) == 0)
		{
			const char *endp;
			unsigned long int n;

			/* First get the number.  */
			nplurals = nullentry + 9;
			while (ISSPACE((unsigned char) *nplurals))
				++nplurals;
			for (endp = nplurals, n = 0; *endp >= '0' && *endp <= '9'; endp++)
				n = n * 10 + (*endp - '0');
			if (nplurals != endp)
			{
				*npluralsp = n;
				
				plural = endp;
				if (*plural == ';')
					plural++;
				while (ISSPACE((unsigned char) *plural))
					++plural;
				if (strncmp(plural, "plural=", 7) == 0)
				{
					struct parse_args args;

					/* Due to the restrictions bison imposes onto the interface of the
					   scanner function we have to put the input string and the result
					   passed up from the parser into the same structure which address
					   is passed down to the parser.  */
					plural += 7;
					while (ISSPACE((unsigned char) *plural))
						++plural;
					args.cp = plural;
					args.res = NULL;
					if (PLURAL_PARSE(&args) == PE_OK)
					{
						*pluralp = args.res;
						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}


#ifdef MAIN

/* gcc libnls/plural-exp.c -Wall -DMAIN libnls/plural.c libnls/eval-plural-str.c libnls/eval-plural-exp.c libnls/plurals.c */

#include "libnls.h"
#include "libnlsI.h"

static void check_n(struct expression *pluralp, const char *eval_str, int nplurals, unsigned long n)
{
	int id1;
	int id2;

	id1 = PLURAL_EVAL(pluralp, n);
	id2 = PLURAL_EVAL_STRING(eval_str, n);
	/* printf("%lu: %d %d\n", n, id1, id2); */
	fflush(stdout);
	if (id1 < 0 || id1 >= nplurals)
		fprintf(stderr, "error plural_eval: %d\n", id1);
	if (id2 < 0 || id2 >= nplurals)
		fprintf(stderr, "error plural_eval_string: %d\n", id2);
	if (id1 != id2)
		fprintf(stderr, "error: %d != %d\n", id1, id2);
}


static void check(const struct _nls_plural *plural)
{
	int nplurals;
	struct expression *pluralp;
	char buf[200];
	unsigned long n;
	const char *exp = plural->exp;

	if (EXTRACT_PLURAL_EXPRESSION(exp, &pluralp, &nplurals) == FALSE)
	{
		fprintf(stderr, "%s: syntax error\n", plural->id_str);
	} else
	{
		printf("%s: %s: %d ", plural->id_str, exp, nplurals);
		if (PLURAL_PRINT(pluralp, buf, sizeof(buf), FALSE) == FALSE)
		{
			printf("\n");
			fflush(stdout);
			fprintf(stderr, "%s: error plural_print\n", exp);
		} else
		{
			fputs(buf, stdout);
			printf("\n");
			if (strcmp(buf, plural->str) != 0)
				fprintf(stderr, "%s: eval strings differ\n", plural->id_str);
				
			for (n = 0; n < 20; n++)
			{
				check_n(pluralp, buf, nplurals, n);
			}
		}
	}
	FREE_EXPRESSION(pluralp);
}


int main(void)
{
	const struct _nls_plural *plural;
	
	for (plural = nls_plurals; plural->exp != NULL; plural++)
		check(plural);

	return 0;
}
#endif

#ifdef BENCH

#include <sys/time.h>

#define BENCH_TIME (5 * 1000000L)

int main(void)
{
	int nplurals;
	struct expression *pluralp;
	char buf[200];
	struct timeval start, end;
	time_t end_time;
	unsigned long n = 1;
	unsigned long loops;
	const char *str = "nplurals=2; plural=n != 1;";

	if (EXTRACT_PLURAL_EXPRESSION(str, &pluralp, &nplurals) == FALSE)
	{
		fprintf(stderr, "%s: syntax error\n", str);
	} else
	{
		PLURAL_PRINT(pluralp, buf, sizeof(buf), FALSE);
		
		gettimeofday(&start, NULL);
		end_time = start.tv_sec * 1000000L + start.tv_usec + BENCH_TIME;
		loops = 0;
		do
		{
			PLURAL_EVAL(pluralp, n);
			loops++;
			gettimeofday(&end, NULL);
		} while (end.tv_sec * 1000000L + end.tv_usec < end_time);
		printf("plural_eval       : %lu loops in %ld microsecs\n", loops, end.tv_sec * 1000000L + end.tv_usec - (start.tv_sec * 1000000L + start.tv_usec));

		gettimeofday(&start, NULL);
		end_time = start.tv_sec * 1000000L + start.tv_usec + BENCH_TIME;
		loops = 0;
		do
		{
			PLURAL_EVAL_STRING(buf, n);
			loops++;
			gettimeofday(&end, NULL);
		} while (end.tv_sec * 1000000L + end.tv_usec < end_time);
		printf("plural_eval_string: %lu loops in %ld microsecs\n", loops, end.tv_sec * 1000000L + end.tv_usec - (start.tv_sec * 1000000L + start.tv_usec));
	}
	return 0;
}
#endif
