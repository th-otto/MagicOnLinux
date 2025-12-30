/* Plural expression evaluation.
   Copyright (C) 2000-2013 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "eval-plural.h"

/* Evaluate the plural expression and return an index value.  */
static int plural_eval_recurse(const struct expression *pexp, unsigned long int *n, unsigned int allowed_depth)
{
	int result;

	if (allowed_depth == 0)
    	/* The allowed recursion depth is exhausted.  */
		return PE_STACKOVF;
	allowed_depth--;

	switch (pexp->nargs)
	{
	case 0:
		switch ((int) pexp->operation)
		{
		case var:
			return PE_OK;
		case num:
			*n = pexp->val.num;
			return PE_OK;
		default:
			assert(0);
			break;
		}
		break;
	case 1:
		/* pexp->operation must be lnot.  */
		assert(pexp->operation == lnot);
		if (pexp->operation == lnot)
		{
			unsigned long int arg;

			arg = *n;
			result = plural_eval_recurse(pexp->val.args[0], &arg, allowed_depth);
			if (result < 0)
				return result;
			*n = arg == 0;
			return PE_OK;
		}
		break;
	case 2:
		{
			unsigned long int left_arg;
			unsigned long int right_arg;
			
			left_arg = *n;
			result = plural_eval_recurse(pexp->val.args[0], &left_arg, allowed_depth);

			if (result < 0)
				return result;
			if (pexp->operation == lor)
			{
				if (left_arg != 0)
				{
					*n = TRUE;
					return PE_OK;
				}
				right_arg = *n;
				result = plural_eval_recurse(pexp->val.args[1], &right_arg, allowed_depth);
				if (result < 0)
					return result;
				*n = right_arg != 0;
				return PE_OK;
			} else if (pexp->operation == land)
			{
				if (left_arg == 0)
				{
					*n = FALSE;
					return PE_OK;
				}
				right_arg = *n;
				result = plural_eval_recurse(pexp->val.args[1], &right_arg, allowed_depth);
				if (result < 0)
					return result;
				*n = right_arg != 0;
				return PE_OK;
			} else
			{
				right_arg = *n;
				result = plural_eval_recurse(pexp->val.args[1], &right_arg, allowed_depth);
				if (result < 0)
					return result;
				switch (pexp->operation)
				{
				case mult:
					*n = left_arg * right_arg;
					return PE_OK;
				case divide:
					if (right_arg == 0)
						return PE_INTDIV;
					*n = left_arg / right_arg;
					return PE_OK;
				case modulo:
					if (right_arg == 0)
						return PE_INTDIV;
					*n = left_arg % right_arg;
					return PE_OK;
				case plus:
					*n = left_arg + right_arg;
					return PE_OK;
				case minus:
					*n = left_arg - right_arg;
					return PE_OK;
				case less_than:
					*n = left_arg < right_arg;
					return PE_OK;
				case greater_than:
					*n = left_arg > right_arg;
					return PE_OK;
				case less_or_equal:
					*n = left_arg <= right_arg;
					return PE_OK;
				case greater_or_equal:
					*n = left_arg >= right_arg;
					return PE_OK;
				case equal:
					*n = left_arg == right_arg;
					return PE_OK;
				case not_equal:
					*n = left_arg != right_arg;
					return PE_OK;
				case num:
				case var:
				case lnot:
				case land:
				case lor:
				case qmop:
				default:
					assert(0);
					break;
				}
			}
		}
		break;
	case 3:
		/* pexp->operation must be qmop.  */
		assert(pexp->operation == qmop);
		if (pexp->operation == qmop)
		{
			unsigned long int boolarg;
			
			boolarg = *n;
			result = plural_eval_recurse(pexp->val.args[0], &boolarg, allowed_depth);
			if (result < 0)
				return result;

			return plural_eval_recurse(pexp->val.args[boolarg != 0 ? 1 : 2], n, allowed_depth);
		}
		break;
	}
	/* NOTREACHED */
	return PE_ASSERT;
}


/* Evaluates a plural expression PEXP for n=N.  */
int libnls_plural_eval(const struct expression *pexp, unsigned long int n)
{
	int result;

	result = plural_eval_recurse(pexp, &n, EVAL_MAXDEPTH);
	if (result < 0)
		return result;
	return (int)n;
}


struct plural_print_args
{
	char *buf;
	size_t size;
	int utf8;
	enum eval_status result;
	char numbuf[30];
	int need_space;
	int depth;
	int max_depth;
};

static void plural_print_recursive(const struct expression *pexp, struct plural_print_args *args)
{
	args->depth++;
	if (args->depth > args->max_depth)
		args->max_depth = args->depth;
	if (args->depth >= EVAL_MAXDEPTH)
	{
		/* The allowed recursion depth is exhausted.  */
		args->result = PE_STACKOVF;
	} else if (args->result == PE_OK)
	{
		switch (pexp->nargs)
		{
		case 0:
			switch ((int) pexp->operation)
			{
			case var:
				if (args->size == 0)
				{
					args->result = PE_STROVF;
				} else
				{
					args->size--;
					*args->buf++ = PLURAL_C_VAR;
				}
				args->need_space = FALSE;
				break;
			case num:
				{
					size_t numlen;

					/*
					 * must add a space to avoid pasting two numbers together
					 */
					if (args->need_space)
					{
						if (args->size == 0)
						{
							args->result = PE_STROVF;
						} else
						{
							args->size--;
							*args->buf++ = ' ';
						}
					}
					numlen = sprintf(args->numbuf, "%lu", pexp->val.num);
					if (args->size < numlen)
					{
						args->result = PE_STROVF;
					} else
					{
						strcpy(args->buf, args->numbuf);
						args->buf += numlen;
						args->size -= numlen;
					}
					args->need_space = TRUE;
				}
				break;
			default:
				args->result = PE_ASSERT;
				assert(0);
				break;
			}
			break;
		case 1:
			/* pexp->operation must be lnot.  */
			assert(pexp->operation == lnot);
			if (pexp->operation == lnot)
			{
				plural_print_recursive(pexp->val.args[0], args);
				args->need_space = FALSE;
				if (args->size == 0)
				{
					args->result = PE_STROVF;
				} else
				{
					args->size--;
					*args->buf++ = PLURAL_C_NOT;
				}
			} else
			{
				args->result = PE_ASSERT;
			}
			break;
		case 2:
			{
				plural_print_recursive(pexp->val.args[0], args);
				plural_print_recursive(pexp->val.args[1], args);
				args->need_space = FALSE;
				if (args->size == 0)
				{
					args->result = PE_STROVF;
				} else
				{
					args->size--;
					switch (pexp->operation)
					{
					case lor:
						*args->buf++ = PLURAL_C_LOR;
						break;
					case land:
						*args->buf++ = PLURAL_C_LAND;
						break;
					case mult:
						*args->buf++ = PLURAL_C_MULT;
						break;
					case divide:
						*args->buf++ = PLURAL_C_DIV;
						break;
					case modulo:
						*args->buf++ = PLURAL_C_MOD;
						break;
					case plus:
						*args->buf++ = PLURAL_C_ADD;
						break;
					case minus:
						*args->buf++ = PLURAL_C_SUB;
						break;
					case less_than:
						*args->buf++ = PLURAL_C_LT;
						break;
					case greater_than:
						*args->buf++ = PLURAL_C_GT;
						break;
					case less_or_equal:
						if (args->utf8)
						{
							/* already decremented once above */
							if (args->size < 2)
							{
								args->result = PE_STROVF;
							} else
							{
								args->size -= 2;
								*args->buf++ = '\342';
								*args->buf++ = '\211';
								*args->buf++ = '\244';
							}
						} else
						{
							*args->buf++ = PLURAL_C_LE;
						}
						break;
					case greater_or_equal:
						if (args->utf8)
						{
							/* already decremented once above */
							if (args->size < 2)
							{
								args->result = PE_STROVF;
							} else
							{
								args->size -= 2;
								*args->buf++ = '\342';
								*args->buf++ = '\211';
								*args->buf++ = '\245';
							}
						} else
						{
							*args->buf++ = PLURAL_C_GE;
						}
						break;
					case equal:
						*args->buf++ = PLURAL_C_EQ;
						break;
					case not_equal:
						if (args->utf8)
						{
							/* already decremented once above */
							if (args->size < 2)
							{
								args->result = PE_STROVF;
							} else
							{
								args->size -= 2;
								*args->buf++ = '\342';
								*args->buf++ = '\211';
								*args->buf++ = '\240';
							}
						} else
						{
							*args->buf++ = PLURAL_C_NE;
						}
						break;
					case lnot:
					case num:
					case var:
					case qmop:
					default:
						args->result = PE_ASSERT;
						assert(0);
						break;
					}
					break;
				}
			}
			break;
		case 3:
			/* pexp->operation must be qmop.  */
			assert(pexp->operation == qmop);
			if (pexp->operation == qmop)
			{
				plural_print_recursive(pexp->val.args[0], args);
				plural_print_recursive(pexp->val.args[1], args);
				plural_print_recursive(pexp->val.args[2], args);
				args->need_space = FALSE;
				if (args->size == 0)
				{
					args->result = PE_STROVF;
				} else
				{
					args->size--;
					*args->buf++ = PLURAL_C_QUEST;
				}
			} else
			{
				args->result = PE_ASSERT;
			}
			break;
		default:
			args->result = PE_ASSERT;
			assert(0);
			break;
		}
	}
	args->depth--;
}


/*
 * convert the expression tree into a simple string
 * that can be iteratively scanned at runtime by
 * libnls_plural_eval_string
 */
int libnls_plural_print(const struct expression *pexp, char *buf, size_t size, int utf8)
{
	struct plural_print_args args;
	
	assert(pexp != NULL);
	assert(buf != NULL);
	assert(size > 0);
	if (pexp == NULL || buf == NULL || size <= 0)
		return FALSE;
	args.buf = buf;
	args.size = size - 1;
	args.utf8 = utf8;
	args.result = PE_OK;
	args.need_space = FALSE;
	args.depth = 0;
	args.max_depth = 0;
	plural_print_recursive(pexp, &args);
	*args.buf = '\0';
	return args.result == PE_OK;
}
