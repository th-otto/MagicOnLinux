/* Expression parsing and evaluation for plural form selection.
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

#ifndef _PLURAL_EXP_H
#define _PLURAL_EXP_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FALSE
#  define FALSE 0
#  define TRUE 1
#endif

enum expression_operator
{
	/* Without arguments:  */
	var,							/* The variable "n".  */
	num,							/* Decimal number.  */
	/* Unary operators:  */
	lnot,							/* Logical NOT.  */
	/* Binary operators:  */
	mult,							/* Multiplication.  */
	divide,							/* Division.  */
	modulo,							/* Modulo operation.  */
	plus,							/* Addition.  */
	minus,							/* Subtraction.  */
	less_than,						/* Comparison.  */
	greater_than,					/* Comparison.  */
	less_or_equal,					/* Comparison.  */
	greater_or_equal,				/* Comparison.  */
	equal,							/* Comparison for equality.  */
	not_equal,						/* Comparison for inequality.  */
	land,							/* Logical AND.  */
	lor,							/* Logical OR.  */
	/* Ternary operators:  */
	qmop							/* Question mark operator.  */
};

/* This is the representation of the expressions to determine the
   plural form.  */
struct expression
{
	int nargs;						/* Number of arguments.  */
	enum expression_operator operation;
	struct
	{
		struct expression *args[3];	/* Up to three arguments.  */
		unsigned long int num;		/* Number value for `num'.  */
	} val;
};

/* This is the data structure to pass information to the parser and get
   the result in a thread-safe way.  */
struct parse_args
{
	const char *cp;
	struct expression *res;
};


extern struct expression _libnls_germanic_plural;

void libnls_free_plural_expression(struct expression *exp);
int libnls_parse_plural_expression(struct parse_args *arg);
int libnls_extract_plural_expression(const char *nullentry, struct expression **pluralp, int *npluralsp);

/* Evaluating a parsed plural expression.  */

enum eval_status
{
	PE_INTDIV = -1,    /* Integer division by zero */
	PE_INTOVF = -2,    /* Integer overflow */
	PE_STACKOVF = -3,  /* Stack overflow */
	PE_ASSERT = -4,    /* Assertion failure */
	PE_STROVF = -5,    /* String overflow */
	PE_OK = 0          /* Evaluation succeeded, produced a value */
};

int libnls_plural_print(const struct expression *pexp, char *buf, size_t size, int utf8);


#ifdef __cplusplus
}
#endif

#endif /* _PLURAL_EXP_H */
