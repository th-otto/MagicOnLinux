#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <assert.h>
#include "expreval.h"
#include "expreval-internal.h"

struct plural_print_args
{
	char *buf;
	size_t size;
	int utf8;
	expreval_error result;
	char numbuf[30];
	int need_space;
	int depth;
	int max_depth;
};


static void expreval_print_recursive(const ExprEvalNode *expr, struct plural_print_args *args)
{
	args->depth++;
	if (args->depth > args->max_depth)
		args->max_depth = args->depth;
	if (args->depth >= EVAL_MAXDEPTH)
	{
		/* The allowed recursion depth is exhausted.  */
		args->result = EXPREVAL_ERROR_STACKOVF;
	} else if (args->result == EXPREVAL_ERROR_NONE)
	{
		switch (expr->type)
		{
		case NODETYPE_VARIABLE:
			if (args->size == 0)
			{
				args->result = EXPREVAL_ERROR_STROVF;
			} else
			{
				args->size--;
				*args->buf++ = PLURAL_C_VAR;
			}
			args->need_space = FALSE;
			break;

		case NODETYPE_VALUE:
			{
				size_t numlen;

				/*
				 * must add a space to avoid pasting two numbers together
				 */
				if (args->need_space)
				{
					if (args->size == 0)
					{
						args->result = EXPREVAL_ERROR_STROVF;
					} else
					{
						args->size--;
						*args->buf++ = ' ';
					}
				}
				numlen = sprintf(args->numbuf, "%lu", expr->u.value.m_val);
				if (args->size < numlen)
				{
					args->result = EXPREVAL_ERROR_STROVF;
				} else
				{
					strcpy(args->buf, args->numbuf);
					args->buf += numlen;
					args->size -= numlen;
				}
				args->need_space = TRUE;
			}
			break;

		case NODETYPE_NOT:
			expreval_print_recursive(expr->u.unary.m_rhs, args);
			args->need_space = FALSE;
			if (args->size == 0)
			{
				args->result = EXPREVAL_ERROR_STROVF;
			} else
			{
				args->size--;
				*args->buf++ = PLURAL_C_NOT;
			}
			break;

		case NODETYPE_NEGATE:
			/* unary minus */
			expreval_print_recursive(expr->u.unary.m_rhs, args);
			args->need_space = FALSE;
			if (args->size == 0)
			{
				args->result = EXPREVAL_ERROR_STROVF;
			} else
			{
				args->size--;
				*args->buf++ = PLURAL_C_UMINUS;
			}
			break;

		case NODETYPE_PLUS:
			/* unary plus: nothing to do */
			break;

		case NODETYPE_COMPLEMENT:
			/* unary minus */
			expreval_print_recursive(expr->u.unary.m_rhs, args);
			args->need_space = FALSE;
			if (args->size == 0)
			{
				args->result = EXPREVAL_ERROR_STROVF;
			} else
			{
				args->size--;
				*args->buf++ = PLURAL_C_CMPL;
			}
			break;

		case NODETYPE_LOR:
		case NODETYPE_LAND:
		case NODETYPE_MULTIPLY:
		case NODETYPE_DIVIDE:
		case NODETYPE_MODULO:
		case NODETYPE_ADD:
		case NODETYPE_SUBTRACT:
		case NODETYPE_LESS:
		case NODETYPE_GREATER:
		case NODETYPE_LESS_EQUAL:
		case NODETYPE_GREATER_EQUAL:
		case NODETYPE_EQUAL:
		case NODETYPE_NOT_EQUAL:
		case NODETYPE_BOR:
		case NODETYPE_BAND:
		case NODETYPE_XOR:
		case NODETYPE_LSHIFT:
		case NODETYPE_RSHIFT:
			expreval_print_recursive(expr->u.binary.m_lhs, args);
			expreval_print_recursive(expr->u.binary.m_rhs, args);
			args->need_space = FALSE;
			if (args->size == 0)
			{
				args->result = EXPREVAL_ERROR_STROVF;
			} else
			{
				args->size--;
				switch (expr->type)
				{
				case NODETYPE_LOR:
					*args->buf++ = PLURAL_C_LOR;
					break;
				case NODETYPE_LAND:
					*args->buf++ = PLURAL_C_LAND;
					break;
				case NODETYPE_MULTIPLY:
					*args->buf++ = PLURAL_C_MULT;
					break;
				case NODETYPE_DIVIDE:
					*args->buf++ = PLURAL_C_DIV;
					break;
				case NODETYPE_MODULO:
					*args->buf++ = PLURAL_C_MOD;
					break;
				case NODETYPE_ADD:
					*args->buf++ = PLURAL_C_ADD;
					break;
				case NODETYPE_SUBTRACT:
					*args->buf++ = PLURAL_C_SUB;
					break;
				case NODETYPE_LESS:
					*args->buf++ = PLURAL_C_LT;
					break;
				case NODETYPE_GREATER:
					*args->buf++ = PLURAL_C_GT;
					break;
				case NODETYPE_LESS_EQUAL:
					if (args->utf8)
					{
						/* already decremented once above */
						if (args->size < 2)
						{
							args->result = EXPREVAL_ERROR_STROVF;
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
				case NODETYPE_GREATER_EQUAL:
					if (args->utf8)
					{
						/* already decremented once above */
						if (args->size < 2)
						{
							args->result = EXPREVAL_ERROR_STROVF;
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
				case NODETYPE_EQUAL:
					*args->buf++ = PLURAL_C_EQ;
					break;
				case NODETYPE_NOT_EQUAL:
					if (args->utf8)
					{
						/* already decremented once above */
						if (args->size < 2)
						{
							args->result = EXPREVAL_ERROR_STROVF;
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
				case NODETYPE_XOR:
					*args->buf++ = PLURAL_C_XOR;
					break;
				default:
					args->result = EXPREVAL_ERROR_ASSERT;
					assert(0);
					break;
				}
			}
			break;

		case NODETYPE_TERNARY:
			/* lhs is the condition */
			expreval_print_recursive(expr->u.binary.m_lhs, args);
			/* rhs is the colon expr */
			expreval_print_recursive(expr->u.binary.m_rhs, args);
			args->need_space = FALSE;
			if (args->size == 0)
			{
				args->result = EXPREVAL_ERROR_STROVF;
			} else
			{
				args->size--;
				*args->buf++ = PLURAL_C_QUEST;
			}
			break;

		case NODETYPE_COLON:
			expreval_print_recursive(expr->u.binary.m_lhs, args);
			expreval_print_recursive(expr->u.binary.m_rhs, args);
			/* nothing to print here */
			break;

		case NODETYPE_FUNCTION:
		case NODETYPE_ASSIGN:
		case NODETYPE_ASSIGN_ADD:
		case NODETYPE_ASSIGN_SUBTRACT:
		case NODETYPE_ASSIGN_MULTIPLY:
		case NODETYPE_ASSIGN_DIVIDE:
		case NODETYPE_ASSIGN_MODULO:
		case NODETYPE_ASSIGN_LSHIFT:
		case NODETYPE_ASSIGN_RSHIFT:
		case NODETYPE_ASSIGN_XOR:
		case NODETYPE_ASSIGN_BOR:
		case NODETYPE_ASSIGN_BAND:
		case NODETYPE_EXPONENT:
		case NODETYPE_COMMA:
		case NODETYPE_REFERENCE:
		default:
			args->result = EXPREVAL_ERROR_ASSERT;
			assert(0);
			break;
		}
	}
	args->depth--;
}


/*
 * convert the expression tree into a simple string
 * that can be iteratively scanned at runtime by
 * expreval_evaluate_string
 */
int libnls_expreval_print_expression(ExprEvalNode *expr, char *buf, size_t size, int utf8)
{
	struct plural_print_args args;
	
	assert(expr != NULL);
	assert(buf != NULL);
	assert(size > 0);
	if (expr == NULL || buf == NULL || size <= 0)
		return FALSE;
	args.buf = buf;
	args.size = size - 1;
	args.utf8 = utf8;
	args.result = EXPREVAL_ERROR_NONE;
	args.need_space = FALSE;
	args.depth = 0;
	args.max_depth = 0;
	expreval_print_recursive(expr, &args);
	*args.buf = '\0';
	return args.result == EXPREVAL_ERROR_NONE;
}
